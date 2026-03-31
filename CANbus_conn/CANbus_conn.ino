/**
 * OBD2 Monitor per ESP8266 D1 Mini
 *
 * Mostra in tempo reale su display OLED SH1106 128x64:
 * - Pressione boost (bar)
 * - Coppia motore stimata (Nm) — da carico motore (PID 0x04) * 400 Nm
 * - Temperatura liquido raffreddamento (C)
 * - Apertura valvola EGR (%)
 *
 * All'avvio esegue una scansione dei PID OBD2 supportati dall'ECU
 * e mostra i risultati su Serial Monitor e OLED.
 *
 * Hardware:
 *   ESP8266 D1 Mini + MCP2515/TJA1050 CAN (modulo integrato, SPI) + OLED SH1106 1.3" (I2C)
 *
 * Cablaggio:
 *   MCP2515: VCC->5V, GND->GND, CS->D8, SO->D6, SI->D7, SCK->D5, INT->D0
 *   OLED:    VCC->3.3V, GND->GND, SCL->D1, SDA->D2
 *   OBD2:    CANH->Pin6, CANL->Pin14, GND->Pin4/5
 *
 * Velocità di acquisizione dati OBD ~8-10Hz (round-robin PID + aggiornamento parziale display)
 *
 * Veicolo: Audi A5 B8 2.7 TDI (CGKA) Multitronic
 *
 * @since 2026-03-19 mattia.Alesi
 */

#include <SPI.h>
#include <mcp_can.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPUpdateServer.h>
#include <U8g2lib.h>
#include <Wire.h>
#include "dtc_descriptions.h"

// ============================================================
// CONFIGURAZIONE — Modificare qui se necessario
// ============================================================

// Cristallo MCP2515: MCP_8MHZ o MCP_16MHZ (controlla il modulo)
#define MCP2515_CRYSTAL MCP_8MHZ

// Pin SPI per MCP2515
#define CAN_CS_PIN 15  // D8 (GPIO15)

// Velocita' bus CAN OBD2
#define CAN_SPEED CAN_500KBPS

// Protocollo OBD2
#define OBD2_REQUEST_ID 0x7DF   // Broadcast richiesta
#define OBD2_RESPONSE_ID 0x7E8  // Risposta ECU motore
#define OBD2_TIMEOUT_MS 100     // Timeout risposta (ms)
#define MAX_RETRIES 3           // Tentativi connessione iniziale

// PID OBD2 per i dati di monitoraggio
#define PID_MAP 0x0B           // Pressione assoluta collettore (kPa)
#define PID_ENGINE_LOAD 0x04   // Carico motore calcolato (%)
#define PID_COOLANT_TEMP 0x05  // Temperatura liquido raffreddamento (°C)
#define PID_EGR 0x2C           // Apertura valvola EGR comandata (%)
#define PID_EGR_ERROR 0x2D    // Errore EGR (%)

// OTA — Aggiornamento firmware via browser (http://192.168.4.1/update)
#define OTA_SSID    "OBD2_UPDATE"   // SSID del SoftAP per OTA
#define OTA_PASS    "obd2flash"     // Password WPA2 (min 8 chars)
#define OTA_PORT    80              // Porta web server
#define OTA_WINDOW_MS 180000        // Finestra OTA: 3 minuti dal boot

// Orientamento display: 0 = landscape (128x64), 1 = portrait 90° (64x128)
#define DISPLAY_ORIENTATION 0

// Debug: 1 = attiva diagnostica avanzata su Serial, 0 = disattiva
#define DEBUG 0

// Pressione atmosferica standard per calcolo boost
#define ATMOSPHERIC_KPA 101.325f

// Coppia massima dichiarata del motore CGKA 2.7 TDI (Nm)
// Usata per stimare la coppia dal carico motore: loadPct * 400 / 100
#define TORQUE_REF_DEFAULT 400

// DTC (Diagnostic Trouble Codes)
#define MAX_DTC 6                 // Max DTC gestibili (multi-frame CAN)
#define DTC_PER_PAGE_LAND 2       // DTC per pagina in landscape
#define DTC_PER_PAGE_PORT 3       // DTC per pagina in portrait
#define DTC_CHECK_INTERVAL 30000  // Intervallo controllo MIL (ms)
#define SCREEN_SWITCH_MS 5000     // Alternanza schermate dati/errori (ms)

// ============================================================
// Oggetti globali
// ============================================================

MCP_CAN CAN(CAN_CS_PIN);
ESP8266WebServer httpServer(OTA_PORT);
ESP8266HTTPUpdateServer httpUpdater;

// Display OLED SH1106 128x64, I2C hardware, senza pin reset
// Rotazione condizionale: R0=landscape(0°), R1=portrait(90°)
#if DISPLAY_ORIENTATION == 0
U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);
#else
U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R1, U8X8_PIN_NONE);
#endif

// ============================================================
// Stato applicazione
// ============================================================

enum AppMode { MODE_SCAN,
               MODE_MONITOR };
AppMode currentMode = MODE_SCAN;

// Bitmask PID supportati dall'ECU (256 bit = 32 byte)
uint8_t pidSupported[32];

// Flag: PID specifici supportati (determinati dalla scansione)
bool mapSupported     = false;
bool loadSupported    = false;    // PID 0x04
bool coolantSupported = false;    // PID 0x05
bool egrSupported       = false;    // PID 0x2C
bool egrErrorSupported  = false;    // PID 0x2D

// Valori correnti delle letture
float boostBar  = 0.0f;
int   loadPct   = 0;              // Carico motore (%)
int   torqueNm  = 0;              // Coppia stimata (loadPct * 400 / 100)
int   coolantC  = 0;              // Temperatura liquido (°C)
int   egrPct    = 0;              // Apertura EGR (%)
int   egrErrPct = 0;              // Errore EGR (%)

// Flag disponibilita' runtime (false se PID va in timeout)
bool mapAvailable     = false;
bool loadAvailable    = false;
bool coolantAvailable = false;
bool egrAvailable     = false;
bool egrErrAvailable  = false;

// DTC — codici errore e stato MIL
uint8_t dtcCount = 0;
bool milOn = false;
uint16_t dtcCodes[MAX_DTC];
unsigned long lastDtcCheck = 0;
unsigned long lastScreenSwitch = 0;
// Schermata corrente: 0 = dati, 1 = pagina DTC 1, 2 = pagina DTC 2, etc.
uint8_t currentScreen = 0;
uint8_t totalScreens = 1;  // Calcolato in base a dtcCount

// ============================================================
// Round-robin: lettura un PID alla volta per ciclo (massimizza refresh display)
// Indici: 0=boost, 1=coppia(stima), 2=temp, 3=egr, 4=egrError
// Boost e coppia pesati ~4x rispetto a temp, egr e egrError
// EGR e EGR_ERROR separati in slot distinti per cicli uniformi
// ============================================================
const uint8_t PID_SCHEDULE[] PROGMEM = {0, 1, 0, 1, 2, 0, 1, 0, 1, 3, 0, 4};
const uint8_t PID_SCHEDULE_LEN = 12;
uint8_t pidScheduleIdx = 0;

// Dirty tracking: valori precedenti per aggiornamento parziale display
float prevBoostBar = -999.0f;
int prevBoostInt = -99999;   // (int)(boostBar * 100) per confronto senza errori float
int prevTorqueNm = -999;
int prevCoolantC = -999;
int prevEgrPct = -999;
bool labelsDrawn = false;    // Etichette statiche gia' disegnate nel buffer

// OTA: true finche' la finestra di aggiornamento e' attiva
bool otaActive = true;
int cachedRightX = 86;       // Posizione X colonna destra, calcolata alla prima draw

// ============================================================
// SETUP
// ============================================================

void setup() {
  Serial.begin(115200);
  Serial.println(F("\n=== OBD2 Monitor ==="));
  Serial.println(F("Audi A5 B8 2.7 TDI CGKA"));

  // I2C su pin ESP8266: SDA=GPIO4(D2), SCL=GPIO5(D1)
  Wire.begin(4, 5);
  Wire.setClock(400000);

  // SoftAP per aggiornamento OTA via browser
  WiFi.mode(WIFI_AP);
  WiFi.softAP(OTA_SSID, OTA_PASS);
  httpUpdater.setup(&httpServer, "/update");
  httpServer.begin();
  Serial.println(F("OTA pronto: http://192.168.4.1/update"));

  // Inizializza display OLED (400kHz I2C esplicito anche via u8g2)
  u8g2.setBusClock(400000);
  u8g2.begin();
  u8g2.setFontPosTop();

  // Splash screen — centrato verticalmente
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tr);
#if DISPLAY_ORIENTATION == 0
  // Landscape (128x64): 2 righe centrate + OTA in basso
  u8g2.drawStr(28, 11, "OBD2 MONITOR");
  u8g2.drawStr(7, 27, "A5 B8 2.7 TDI CGKA");
  u8g2.drawStr(16, 54, "OTA: 192.168.4.1");
#else
  // Portrait (64x128): 5 righe centrate + OTA in basso, IP spezzato
  u8g2.drawStr(20, 10, "OBD2");
  u8g2.drawStr(11, 24, "MONITOR");
  u8g2.drawStr(17, 38, "A5 B8");
  u8g2.drawStr(11, 52, "2.7 TDI");
  u8g2.drawStr(20, 66, "CGKA");
  u8g2.drawStr(20, 90, "OTA:");
  u8g2.drawStr(8, 104,  "192.168.");
  u8g2.drawStr(20, 118, ".4.1");
#endif
  u8g2.sendBuffer();
  delay(2000);

  // Inizializza MCP2515 — centrato verticalmente
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tr);
#if DISPLAY_ORIENTATION == 0
  u8g2.drawStr(0, 27, "Init CAN bus...");   // (64-10)/2 = 27
#else
  u8g2.drawStr(0, 59, "Init CAN bus...");   // (128-10)/2 = 59
#endif
  u8g2.sendBuffer();
  Serial.println(F("Inizializzazione MCP2515..."));

  if (CAN.begin(MCP_ANY, CAN_SPEED, MCP2515_CRYSTAL) != CAN_OK) {
    Serial.println(F("ERRORE: MCP2515 init fallito!"));
    showError("CAN init fail", "Controlla SPI");
    while (true) { yield(); }
  }

  CAN.setMode(MCP_NORMAL);
  Serial.println(F("MCP2515 OK - CAN 500kbps"));

  // Prepara scansione PID
  memset(pidSupported, 0, sizeof(pidSupported));
  currentMode = MODE_SCAN;
}

// ============================================================
// LOOP
// ============================================================

void loop() {
  // OTA: attivo solo nei primi 3 minuti dal boot, poi spegne SoftAP e WiFi
  if (otaActive) {
    httpServer.handleClient();
    if (millis() > OTA_WINDOW_MS) {
      httpServer.stop();
      WiFi.softAPdisconnect(true);
      WiFi.mode(WIFI_OFF);
      otaActive = false;
      Serial.println(F("OTA: finestra chiusa, WiFi spento"));
    }
  }
  switch (currentMode) {
    case MODE_SCAN:
      executeScanMode();
      break;
    case MODE_MONITOR:
      executeMonitorMode();
      break;
  }
}

// ============================================================
// COMUNICAZIONE CAN / OBD2
// ============================================================

/** Svuota il buffer di ricezione CAN da messaggi pendenti */
void flushCANBuffer() {
  unsigned long id;
  uint8_t len;
  uint8_t buf[8];
  while (CAN.checkReceive() == CAN_MSGAVAIL) {
    CAN.readMsgBuf(&id, &len, buf);
    yield();
  }
}

/**
 * Invia una richiesta OBD2 Service 01 per il PID specificato.
 * Frame CAN: [0x02, 0x01, pid, 0, 0, 0, 0, 0] su ID 0x7DF
 */
bool sendOBD2Request(uint8_t pid) {
  uint8_t data[8] = { 0x02, 0x01, pid, 0x00, 0x00, 0x00, 0x00, 0x00 };
  return (CAN.sendMsgBuf(OBD2_REQUEST_ID, 0, 8, data) == CAN_OK);
}

/**
 * Attende una risposta OBD2 dall'ECU motore (ID 0x7E8) per il PID atteso.
 * Restituisce true se la risposta arriva entro OBD2_TIMEOUT_MS.
 * I dati grezzi del frame CAN vengono copiati in responseData.
 */
bool readOBD2Response(uint8_t expectedPid, uint8_t* responseData, uint8_t* responseLen) {
  unsigned long start = millis();
  unsigned long rxId;
  uint8_t len;
  uint8_t buf[8];

  while ((millis() - start) < OBD2_TIMEOUT_MS) {
    if (CAN.checkReceive() == CAN_MSGAVAIL) {
      CAN.readMsgBuf(&rxId, &len, buf);

      // Filtra: ID ECU motore, risposta Service 01 (0x41), PID corretto
      if (rxId == OBD2_RESPONSE_ID && buf[1] == 0x41 && buf[2] == expectedPid) {
        memcpy(responseData, buf, 8);
        *responseLen = len;
        return true;
      }
    }
    yield();
  }
  return false;
}

/**
 * Interroga un PID OBD2: svuota il buffer, invia la richiesta e attende la risposta.
 * Restituisce true se la risposta viene ricevuta correttamente.
 */
bool queryPID(uint8_t pid, uint8_t* data, uint8_t* len) {
  flushCANBuffer();
  if (!sendOBD2Request(pid)) {
    return false;
  }
  return readOBD2Response(pid, data, len);
}

// ============================================================
// GESTIONE BITMASK PID SUPPORTATI
// ============================================================

/**
 * Memorizza i 4 byte di risposta alla query di supporto PID
 * nel bitmask globale pidSupported[].
 * basePid: 0x00, 0x20, 0x40, 0x60
 * fourBytes: i 4 byte dati della risposta (data[3..6])
 */
void storeSupportBitmask(uint8_t basePid, uint8_t* fourBytes) {
  uint8_t offset = basePid / 8;
  pidSupported[offset] = fourBytes[0];
  pidSupported[offset + 1] = fourBytes[1];
  pidSupported[offset + 2] = fourBytes[2];
  pidSupported[offset + 3] = fourBytes[3];
}

/**
 * Verifica se un PID specifico e' supportato dall'ECU.
 * Controlla il bit corrispondente nel bitmask pidSupported[].
 */
bool isPIDSupported(uint8_t pid) {
  if (pid == 0) return true;
  uint8_t byteIndex = (pid - 1) / 8;
  uint8_t bitIndex = 7 - ((pid - 1) % 8);
  return (pidSupported[byteIndex] & (1 << bitIndex)) != 0;
}

// ============================================================
// MODALITA' SCANSIONE
// ============================================================

/**
 * Esegue la scansione completa dei PID supportati dall'ECU.
 * Interroga i range 0x00, 0x20, 0x40, 0x60 seguendo la catena
 * di supporto (ogni risposta indica se il range successivo esiste).
 * Mostra il progresso su OLED e i risultati su Serial.
 */
void executeScanMode() {
  drawScanScreen("Connessione...", "");

  // Tentativo di connessione con retry
  uint8_t data[8];
  uint8_t len;
  bool connected = false;

  for (int attempt = 1; attempt <= MAX_RETRIES; attempt++) {
    Serial.print(F("Tentativo ECU: "));
    Serial.print(attempt);
    Serial.print(F("/"));
    Serial.println(MAX_RETRIES);

    char attBuf[20];
    snprintf(attBuf, sizeof(attBuf), "Tentativo %d/%d", attempt, MAX_RETRIES);
    drawScanScreen("Connessione...", attBuf);

    if (queryPID(0x00, data, &len)) {
      connected = true;
      storeSupportBitmask(0x00, &data[3]);
      break;
    }
    delay(500);
  }

  if (!connected) {
    Serial.println(F("ERRORE: ECU non risponde!"));
    showError("ECU non risp.", "Quadro acceso?");
    while (true) { yield(); }
  }

  Serial.println(F("ECU connessa!"));
  drawScanScreen("ECU connessa!", "");

  // Scansiona dinamicamente tutti i range Mode 01: 0x20, 0x40, ...
  // fino a quando il bit di continuazione non si azzera oppure si arriva a 0xE0.
  bool chainContinues = (data[6] & 0x01) != 0;

  for (uint16_t rangePid = 0x20; rangePid <= 0xE0 && chainContinues; rangePid += 0x20) {
    char progBuf[20];
    snprintf(progBuf, sizeof(progBuf), "Range: 0x%02X", (uint8_t)rangePid);
    drawScanScreen("ECU connessa!", progBuf);

    Serial.print(F("Query PID 0x"));
    if (rangePid < 0x10) Serial.print(F("0"));
    Serial.print((uint8_t)rangePid, HEX);
    Serial.print(F("... "));

    if (queryPID((uint8_t)rangePid, data, &len)) {
      storeSupportBitmask((uint8_t)rangePid, &data[3]);
      Serial.print(F("OK ["));
      for (int j = 3; j < 7; j++) {
        if (data[j] < 0x10) Serial.print(F("0"));
        Serial.print(data[j], HEX);
        if (j < 6) Serial.print(F(" "));
      }
      Serial.println(F("]"));
      chainContinues = (data[6] & 0x01) != 0;
    } else {
      Serial.println(F("Nessuna risposta"));
      break;
    }
    delay(10);  // 10ms sufficiente tra query CAN (era 50ms)
  }

  // Conta e stampa tutti i PID supportati trovati in tutta la bitmap Mode 01
  int totalFound = 0;
  Serial.println(F("\nPID supportati:"));
  for (int p = 1; p <= 0xFF; p++) {
    if (isPIDSupported(p)) {
      Serial.print(F("  0x"));
      if (p < 0x10) Serial.print(F("0"));
      Serial.println(p, HEX);
      totalFound++;
    }
  }
  Serial.print(F("Totale: "));
  Serial.println(totalFound);

  // Imposta flag per i PID di monitoraggio
  mapSupported     = isPIDSupported(PID_MAP);
  loadSupported    = isPIDSupported(PID_ENGINE_LOAD);
  coolantSupported = isPIDSupported(PID_COOLANT_TEMP);
  egrSupported       = isPIDSupported(PID_EGR);
  egrErrorSupported  = isPIDSupported(PID_EGR_ERROR);

  // Log disponibilita'
  Serial.println(F("\nDisponibilita' PID monitor:"));
  Serial.print(F("  MAP (0x0B):        "));
  Serial.println(mapSupported ? "SI" : "NO");
  Serial.print(F("  Load (0x04):       "));
  Serial.println(loadSupported ? "SI" : "NO");
  Serial.print(F("  Coolant (0x05):    "));
  Serial.println(coolantSupported ? "SI" : "NO");
  Serial.print(F("  EGR (0x2C):        "));
  Serial.println(egrSupported ? "SI" : "NO");
  Serial.print(F("  EGR Err (0x2D):    "));
  Serial.println(egrErrorSupported ? "SI" : "NO");

  // Mostra risultato su display
  char countBuf[20];
  snprintf(countBuf, sizeof(countBuf), "PID trovati: %d", totalFound);
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_5x7_tr);
  drawStrCentered(0, "SCAN COMPLETA");
  drawStrCentered(10, countBuf);
  u8g2.drawStr(0, 24, mapSupported     ? "BOOST:  SI" : "BOOST:  NO");
  u8g2.drawStr(0, 34, loadSupported    ? "COPPIA: SI" : "COPPIA: NO");
  u8g2.drawStr(0, 44, coolantSupported ? "TEMP:   SI" : "TEMP:   NO");
  u8g2.drawStr(0, 54, egrSupported     ? "EGR:    SI" : "EGR:    NO");
  u8g2.sendBuffer();

  delay(2000);  // Mostra risultati scan (era 5000ms)
  // Reset stato display e scheduling per la nuova sessione monitor
  labelsDrawn = false;
  pidScheduleIdx = 0;
  currentMode = MODE_MONITOR;
}

// ============================================================
// LETTURA DATI OBD2
// ============================================================

/**
 * Legge la pressione MAP e calcola il boost in bar.
 * Boost = (MAP_kPa - pressione_atmosferica) / 100
 * @see PID 0x0B: 1 byte, range 0-255 kPa
 */
bool readBoostPressure(float* boost) {
  uint8_t data[8];
  uint8_t len;
  if (queryPID(PID_MAP, data, &len)) {
    float mapKpa = (float)data[3];
    *boost = (mapKpa - ATMOSPHERIC_KPA) / 100.0f;
    return true;
  }
  return false;
}

/**
 * Legge il carico motore calcolato.
 * @see PID 0x04: 1 byte, formula = A * 100 / 255, range 0..100 %
 * @since 27/03/26 Mattia Alesi
 */
bool readEngineLoad(int* load) {
  uint8_t data[8];
  uint8_t len;
  if (queryPID(PID_ENGINE_LOAD, data, &len)) {
    *load = ((int)data[3] * 100) / 255;
    return true;
  }
  return false;
}

/**
 * Legge la temperatura del liquido di raffreddamento.
 * @see PID 0x05: 1 byte, formula = A - 40, range -40..215 C
 * @since 27/03/26 Mattia Alesi
 */
bool readCoolantTemp(int* temp) {
  uint8_t data[8];
  uint8_t len;
  if (queryPID(PID_COOLANT_TEMP, data, &len)) {
    *temp = (int)data[3] - 40;
    return true;
  }
  return false;
}

/**
 * Legge l'apertura comandata della valvola EGR.
 * @see PID 0x2C: 1 byte, formula = A * 100 / 255, range 0..100 %
 * @since 27/03/26 Mattia Alesi
 */
bool readEGR(int* egr) {
  uint8_t data[8];
  uint8_t len;
  if (queryPID(PID_EGR, data, &len)) {
    *egr = ((int)data[3] * 100) / 255;
    return true;
  }
  return false;
}

/**
 * Legge l'errore EGR (differenza tra comandato e reale).
 * @see PID 0x2D: 1 byte, formula = (A - 128) * 100 / 128, range -100..+99 %
 * @since 31/03/26 Mattia Alesi
 */
bool readEGRError(int* err) {
  uint8_t data[8];
  uint8_t len;
  if (queryPID(PID_EGR_ERROR, data, &len)) {
    *err = ((int)data[3] - 128) * 100 / 128;
    return true;
  }
  return false;
}

// ============================================================
// LETTURA DTC (CODICI ERRORE)
// ============================================================

/**
 * Controlla lo stato MIL e il numero di DTC attivi.
 * @see PID 0x01: byte A bit 7 = MIL on/off, bit 6-0 = numero DTC
 */
void checkMILStatus() {
  uint8_t data[8];
  uint8_t len;
  if (queryPID(0x01, data, &len)) {
    milOn = (data[3] & 0x80) != 0;
    uint8_t count = data[3] & 0x7F;
    if (milOn && count > 0) {
      readDTCCodes();
    } else {
      dtcCount = 0;
      currentScreen = 0;
      totalScreens = 1;
    }
    Serial.print(F("MIL: "));
    Serial.print(milOn ? "ON" : "OFF");
    Serial.print(F(", DTC: "));
    Serial.println(dtcCount);
  }
}

/**
 * Legge i codici DTC attivi via Mode 03.
 * Gestisce single frame (fino a 3 DTC) e multi-frame ISO-TP (fino a MAX_DTC).
 * Aggiornato per supportare paginazione display con piu' di 2 DTC.
 */
void readDTCCodes() {
  flushCANBuffer();

  // Mode 03: richiesta DTC
  uint8_t reqData[8] = { 0x01, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
  if (CAN.sendMsgBuf(OBD2_REQUEST_ID, 0, 8, reqData) != CAN_OK) {
    return;
  }

  // Buffer per raccogliere tutti i byte DTC (single o multi-frame)
  uint8_t dtcBytes[MAX_DTC * 2];
  uint8_t totalDtcBytes = 0;
  dtcCount = 0;

  unsigned long start = millis();
  unsigned long rxId;
  uint8_t len;
  uint8_t buf[8];

  while ((millis() - start) < OBD2_TIMEOUT_MS) {
    if (CAN.checkReceive() == CAN_MSGAVAIL) {
      CAN.readMsgBuf(&rxId, &len, buf);
      if (rxId != OBD2_RESPONSE_ID) continue;

      uint8_t pci = buf[0] & 0xF0;

      if (pci == 0x00) {
        // --- Single Frame: [len, 0x43, DTC1_H, DTC1_L, ...] ---
        if (buf[1] == 0x43) {
          uint8_t dataLen = buf[0] & 0x0F;
          for (int i = 2; i < 2 + dataLen - 1 && totalDtcBytes < MAX_DTC * 2; i++) {
            dtcBytes[totalDtcBytes++] = buf[i];
          }
        }
        break;

      } else if (pci == 0x10) {
        // --- First Frame (multi-frame): [0x10, totLen, 0x43, DTC1_H, DTC1_L, ...] ---
        if (buf[2] == 0x43) {
          for (int i = 3; i < 8 && totalDtcBytes < MAX_DTC * 2; i++) {
            dtcBytes[totalDtcBytes++] = buf[i];
          }
          // Invia Flow Control per ricevere i Consecutive Frames
          uint8_t fc[8] = { 0x30, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
          CAN.sendMsgBuf(OBD2_REQUEST_ID, 0, 8, fc);
        }

      } else if (pci == 0x20) {
        // --- Consecutive Frame: [0x2N, data...] ---
        for (int i = 1; i < 8 && totalDtcBytes < MAX_DTC * 2; i++) {
          dtcBytes[totalDtcBytes++] = buf[i];
        }
      }
    }
    yield();
  }

  // Parsa i DTC dal buffer raccolto (2 byte per codice)
  for (int i = 0; i + 1 < totalDtcBytes && dtcCount < MAX_DTC; i += 2) {
    uint16_t code = ((uint16_t)dtcBytes[i] << 8) | dtcBytes[i + 1];
    if (code != 0x0000) {
      dtcCodes[dtcCount++] = code;
    }
  }

  // Calcola il numero totale di schermate (1 dati + N pagine DTC)
  if (dtcCount > 0) {
#if DISPLAY_ORIENTATION == 0
    uint8_t perPage = DTC_PER_PAGE_LAND;
#else
    uint8_t perPage = DTC_PER_PAGE_PORT;
#endif
    totalScreens = 1 + (dtcCount + perPage - 1) / perPage;
  } else {
    totalScreens = 1;
  }

  // Log su serial
  for (int i = 0; i < dtcCount; i++) {
    char codeStr[6];
    decodeDTC(dtcCodes[i], codeStr);
    Serial.print(F("  DTC: "));
    Serial.println(codeStr);
  }
}

// ============================================================
// MODALITA' MONITOR
// ============================================================

/**
 * Verifica se lo slot PID round-robin e' supportato dall'ECU.
 * @param slot indice nello scheduling (0=boost, 1=coppia, 2=temp, 3=egr, 4=egrError)
 * @return true se il PID corrispondente e' supportato
 * @see executeMonitorMode()
 * @since 24/03/26 Mattia Alesi
 * @modified 31/03/26 — aggiunto slot 4 per EGR_ERROR separato
 */
bool isPidSlotSupported(uint8_t slot) {
  switch (slot) {
    case 0: return mapSupported;
    case 1: return loadSupported;
    case 2: return coolantSupported;
    case 3: return egrSupported;
    case 4: return egrErrorSupported;
    default: return false;
  }
}

/**
 * Esegue un ciclo di lettura OBD2 e aggiornamento display.
 * Legge UN solo PID per ciclo in round-robin pesato per massimizzare
 * la velocita' di refresh del display (~8-10 Hz invece di 3-7 Hz).
 * Boost e coppia vengono letti 4x piu' spesso di temp e EGR.
 *
 * @modified 24/03/26 — round-robin PID invece di lettura sequenziale
 */
void executeMonitorMode() {
  // Controllo periodico MIL/DTC (ogni DTC_CHECK_INTERVAL ms)
  if (millis() - lastDtcCheck > DTC_CHECK_INTERVAL) {
    checkMILStatus();
    lastDtcCheck = millis();
  }

  // Logica alternanza schermate: dati → DTC pag.1 → DTC pag.2 → dati (ogni 5s)
  if (dtcCount > 0 && totalScreens > 1) {
    if (millis() - lastScreenSwitch > SCREEN_SWITCH_MS) {
      currentScreen = (currentScreen + 1) % totalScreens;
      lastScreenSwitch = millis();
    }
  } else {
    currentScreen = 0;
  }

  if (currentScreen > 0) {
    // Schermata DTC — il buffer viene sovrascritto, forza ridisegno completo al ritorno
    labelsDrawn = false;
    drawDTCScreen(currentScreen - 1);
  } else {
    // Round-robin: trova il prossimo PID supportato nella sequenza
    uint8_t attempts = 0;
    uint8_t pidIdx;
    do {
      pidIdx = pgm_read_byte(&PID_SCHEDULE[pidScheduleIdx]);
      pidScheduleIdx = (pidScheduleIdx + 1) % PID_SCHEDULE_LEN;
      attempts++;
    } while (!isPidSlotSupported(pidIdx) && attempts < PID_SCHEDULE_LEN);

    // Legge UN solo PID per ciclo
    switch (pidIdx) {
      case 0:  // Boost
        if (mapSupported) {
          mapAvailable = readBoostPressure(&boostBar);
        }
        break;
      case 1:  // Coppia (stimata da carico motore)
        if (loadSupported) {
          loadAvailable = readEngineLoad(&loadPct);
          if (loadAvailable) {
            torqueNm = (loadPct * TORQUE_REF_DEFAULT) / 100;
          }
        }
        break;
      case 2:  // Temperatura liquido
        if (coolantSupported) {
          coolantAvailable = readCoolantTemp(&coolantC);
        }
        break;
      case 3:  // EGR
        if (egrSupported) {
          egrAvailable = readEGR(&egrPct);
        }
        break;
      case 4:  // Errore EGR (slot separato per cicli uniformi)
        if (egrErrorSupported) {
          egrErrAvailable = readEGRError(&egrErrPct);
        }
        break;
    }
    updateDisplay();
#if DEBUG
    printAdvancedDiagnostics();
#endif
  }
  yield();
}

// ============================================================
// HELPER DISPLAY: disegno singoli valori e cancellazione aree
// ============================================================

/**
 * Cancella un'area rettangolare nel framebuffer senza toccare il resto.
 * Usa setDrawColor(0) + drawBox per azzerare solo i pixel specificati.
 * @see updateDisplay()
 * @since 24/03/26 Mattia Alesi
 */
void clearValueArea(int x, int y, int w, int h) {
  u8g2.setDrawColor(0);
  u8g2.drawBox(x, y, w, h);
  u8g2.setDrawColor(1);
}

/**
 * Disegna il valore boost con 2 cifre decimali.
 * Colonna sinistra, y=14. Font deve essere gia' impostato.
 * @see updateDisplay()
 * @since 24/03/26 Mattia Alesi
 */
void drawBoostValue() {
  char val[14];
  if (mapSupported && mapAvailable) {
    int bc = (int)(boostBar * 100.0f);
    char s = (bc >= 0) ? '+' : '-';
    int a = abs(bc);
    snprintf(val, sizeof(val), "%c%d.%02d Bar", s, a / 100, a % 100);
  } else {
    snprintf(val, sizeof(val), "N/D");
  }
  u8g2.drawStr(0, 14, val);
}

/**
 * Disegna il valore coppia motore stimata in Nm (da carico motore * 400 Nm).
 * Colonna sinistra, y=49. Font deve essere gia' impostato.
 * @see updateDisplay()
 * @modified 31/03/26 — spostato da alto-DX a basso-SX
 */
void drawTorqueValue() {
  char val[14];
  if (loadSupported && loadAvailable) {
    snprintf(val, sizeof(val), "%d Nm", torqueNm);
  } else {
    snprintf(val, sizeof(val), "N/D");
  }
  u8g2.drawStr(0, 49, val);
}

/**
 * Disegna il valore temperatura liquido raffreddamento.
 * Colonna destra (cachedRightX), y=14. Font deve essere gia' impostato.
 * @see updateDisplay()
 * @modified 31/03/26 — spostato da basso-SX a alto-DX
 */
void drawCoolantValue() {
  char val[14];
  if (coolantSupported && coolantAvailable) {
    snprintf(val, sizeof(val), "%d C", coolantC);
  } else {
    snprintf(val, sizeof(val), "N/D");
  }
  u8g2.drawStr(cachedRightX, 14, val);
}

/**
 * Disegna il valore apertura valvola EGR con errore tra parentesi.
 * Formato: "44%(30)" — colonna destra (cachedRightX), y=49.
 * @see updateDisplay()
 * @modified 31/03/26 — aggiunto errore EGR (PID 0x2D) tra parentesi
 */
void drawEgrValue() {
  char val[14];
  if (egrSupported && egrAvailable) {
    snprintf(val, sizeof(val), "%d%%(%d)", egrPct, egrErrPct);
  } else {
    snprintf(val, sizeof(val), "N/D");
  }
  u8g2.drawStr(cachedRightX, 49, val);
}

/**
 * Aggiorna il display OLED con i valori correnti.
 * In landscape usa aggiornamento parziale: ridisegna solo le tile modificate
 * invece dell'intero framebuffer, riducendo il trasferimento I2C da ~1024 a ~256-384 byte.
 * Le etichette statiche (BOOST, TEMP, COPPIA, EGR) vengono disegnate una sola volta.
 * In portrait usa sendBuffer completo (guadagno parziale minimo su 64px).
 *
 * Tile layout landscape (font 7x14B, 14px alto):
 *   Etichette riga 1 (y=0):  pixel 0-13  -> tile rows 0-1
 *   Valori riga 1 (y=14):    pixel 14-27 -> tile rows 1-3
 *   Etichette riga 2 (y=35): pixel 35-48 -> tile rows 4-6
 *   Valori riga 2 (y=49):    pixel 49-62 -> tile rows 6-7
 *
 * @modified 24/03/26 — aggiornamento parziale con dirty tracking e updateDisplayArea()
 */
void updateDisplay() {
#if DISPLAY_ORIENTATION == 0
  // --- LANDSCAPE (128x64) con aggiornamento parziale ---
  u8g2.setFont(u8g2_font_7x14B_tr);

  // Prepara stringhe colonna destra per calcolo rightX dinamico
  // Colonna destra: TEMP (riga 1) + EGR (riga 2)
  char rVal1[14];
  char rVal2[14];
  if (coolantSupported && coolantAvailable) {
    snprintf(rVal1, sizeof(rVal1), "%d C", coolantC);
  } else {
    snprintf(rVal1, sizeof(rVal1), "N/D");
  }
  if (egrSupported && egrAvailable) {
    snprintf(rVal2, sizeof(rVal2), "%d%%(%d)", egrPct, egrErrPct);
  } else {
    snprintf(rVal2, sizeof(rVal2), "N/D");
  }

  const char* rightStrs[] = { "TEMP", rVal1, "EGR", rVal2 };
  int newRightX = calcRightColumnX(rightStrs, 4, 128);

  // Se rightX e' cambiato (raro: solo con valori molto larghi), forza ridisegno completo
  if (newRightX != cachedRightX) {
    labelsDrawn = false;
    cachedRightX = newRightX;
  }

  if (!labelsDrawn) {
    // --- RIDISEGNO COMPLETO: etichette + valori + sendBuffer ---
    u8g2.clearBuffer();

    // Etichette statiche
    u8g2.drawStr(0, 0, "BOOST");
    u8g2.drawStr(cachedRightX, 0, "TEMP");
    u8g2.drawStr(0, 35, "COPPIA");
    u8g2.drawStr(cachedRightX, 35, "EGR");

    // Valori iniziali
    drawBoostValue();
    drawTorqueValue();
    drawCoolantValue();
    drawEgrValue();

    u8g2.sendBuffer();
    labelsDrawn = true;

    // Salva valori correnti per dirty tracking (sentinel -99999 = non disponibile)
    prevBoostInt = mapAvailable ? (int)(boostBar * 100.0f) : -99999;
    prevTorqueNm = loadAvailable ? torqueNm : -99999;
    prevCoolantC = coolantAvailable ? coolantC : -99999;
    prevEgrPct = egrAvailable ? egrPct : -99999;
    return;
  }

  // --- AGGIORNAMENTO PARZIALE: solo valori cambiati ---
  // Sentinel -99999 codifica "PID non disponibile" per confronto corretto
  bool row1dirty = false;
  bool row2dirty = false;
  int curBoostInt = mapAvailable ? (int)(boostBar * 100.0f) : -99999;
  int curTorqueNm = loadAvailable ? torqueNm : -99999;
  int curCoolantC = coolantAvailable ? coolantC : -99999;
  int curEgrPct = egrAvailable ? egrPct : -99999;

  // Boost (colonna sinistra, riga 1)
  if (curBoostInt != prevBoostInt) {
    clearValueArea(0, 14, cachedRightX, 14);
    drawBoostValue();
    prevBoostInt = curBoostInt;
    row1dirty = true;
  }

  // Temperatura liquido (colonna destra, riga 1)
  if (curCoolantC != prevCoolantC) {
    clearValueArea(cachedRightX, 14, 128 - cachedRightX, 14);
    drawCoolantValue();
    prevCoolantC = curCoolantC;
    row1dirty = true;
  }

  // Coppia stimata (colonna sinistra, riga 2)
  if (curTorqueNm != prevTorqueNm) {
    clearValueArea(0, 49, cachedRightX, 14);
    drawTorqueValue();
    prevTorqueNm = curTorqueNm;
    row2dirty = true;
  }

  // EGR (colonna destra, riga 2)
  if (curEgrPct != prevEgrPct) {
    clearValueArea(cachedRightX, 49, 128 - cachedRightX, 14);
    drawEgrValue();
    prevEgrPct = curEgrPct;
    row2dirty = true;
  }

  // Invia solo le tile rows modificate (8 pixel per tile row)
  if (row1dirty) {
    u8g2.updateDisplayArea(0, 1, 16, 3);  // Tile rows 1-3 (pixel 8-31)
  }
  if (row2dirty) {
    u8g2.updateDisplayArea(0, 6, 16, 2);  // Tile rows 6-7 (pixel 48-63)
  }

#else
  // --- PORTRAIT (64x128) --- sendBuffer completo (mapping tile R1 troppo complesso)
  u8g2.clearBuffer();
  char line[11];

  u8g2.setFont(u8g2_font_6x13B_tr);

  if (mapSupported && mapAvailable) {
    int bc = (int)(boostBar * 100.0f);
    int a = abs(bc);
    if (bc >= 0) {
      snprintf(line, sizeof(line), "BOOST %d.%02d", a / 100, a % 100);
    } else {
      snprintf(line, sizeof(line), "BOOST-%d.%02d", a / 100, a % 100);
    }
  } else {
    snprintf(line, sizeof(line), "BOOST N/D");
  }
  u8g2.drawStr(0, 27, line);

  if (loadSupported && loadAvailable) {
    snprintf(line, sizeof(line), "COPPIA %d", torqueNm);
  } else {
    snprintf(line, sizeof(line), "COPPIA N/D");
  }
  u8g2.drawStr(0, 47, line);

  if (coolantSupported && coolantAvailable) {
    snprintf(line, sizeof(line), "TEMP %d C", coolantC);
  } else {
    snprintf(line, sizeof(line), "TEMP N/D");
  }
  u8g2.drawStr(0, 67, line);

  if (egrSupported && egrAvailable) {
    snprintf(line, sizeof(line), "EGR %d(%d)", egrPct, egrErrPct);
  } else {
    snprintf(line, sizeof(line), "EGR N/D");
  }
  u8g2.drawStr(0, 87, line);

  u8g2.sendBuffer();
#endif
}

/**
 * Disegna una pagina di codici errore DTC.
 * @param page indice pagina (0 = prima pagina DTC, 1 = seconda, etc.)
 */
void drawDTCScreen(uint8_t page) {
  u8g2.clearBuffer();

#if DISPLAY_ORIENTATION == 0
  // --- LANDSCAPE (128x64) ---
  // Titolo con numero pagina se piu' di una pagina DTC
  uint8_t perPage = DTC_PER_PAGE_LAND;
  uint8_t dtcPages = (dtcCount + perPage - 1) / perPage;
  char title[22];
  if (dtcPages > 1) {
    snprintf(title, sizeof(title), "ERRORI (%d) pag.%d/%d", dtcCount, page + 1, dtcPages);
  } else {
    snprintf(title, sizeof(title), "ERRORI ATTIVI (%d)", dtcCount);
  }
  u8g2.setFont(u8g2_font_7x14B_tr);
  u8g2.drawStr(0, 0, title);

  u8g2.setFont(u8g2_font_6x10_tr);
  uint8_t startIdx = page * perPage;
  for (int i = 0; i < perPage && (startIdx + i) < dtcCount; i++) {
    int yBase = 20 + i * 22;
    char codeStr[6];
    decodeDTC(dtcCodes[startIdx + i], codeStr);
    u8g2.drawStr(0, yBase, codeStr);

    const char* desc = getDTCDescription(dtcCodes[startIdx + i]);
    if (desc != NULL) {
      char descBuf[22];
      strncpy_P(descBuf, desc, sizeof(descBuf) - 1);
      descBuf[sizeof(descBuf) - 1] = '\0';
      u8g2.drawStr(0, yBase + 11, descBuf);
    }
  }

#else
  // --- PORTRAIT (64x128) ---
  uint8_t perPage = DTC_PER_PAGE_PORT;
  uint8_t dtcPages = (dtcCount + perPage - 1) / perPage;
  char title[16];
  if (dtcPages > 1) {
    snprintf(title, sizeof(title), "ERR(%d) %d/%d", dtcCount, page + 1, dtcPages);
  } else {
    snprintf(title, sizeof(title), "ERRORI (%d)", dtcCount);
  }
  u8g2.setFont(u8g2_font_6x13B_tr);
  u8g2.drawStr(0, 2, title);

  uint8_t startIdx = page * perPage;
  for (int i = 0; i < perPage && (startIdx + i) < dtcCount; i++) {
    int yBase = 22 + i * 36;
    char codeStr[6];
    decodeDTC(dtcCodes[startIdx + i], codeStr);
    u8g2.setFont(u8g2_font_6x13B_tr);
    u8g2.drawStr(0, yBase, codeStr);

    const char* desc = getDTCDescription(dtcCodes[startIdx + i]);
    if (desc != NULL) {
      char descBuf[22];
      strncpy_P(descBuf, desc, sizeof(descBuf) - 1);
      descBuf[sizeof(descBuf) - 1] = '\0';
      u8g2.setFont(u8g2_font_5x8_tr);
      u8g2.drawStr(0, yBase + 14, descBuf);
    }
  }

#endif

  u8g2.sendBuffer();
}

#if DEBUG
// ============================================================
// DIAGNOSTICA SERIALE AVANZATA
// ============================================================

/**
 * Stampa su Serial tutti i 47 parametri (diretti + calcolati) dai PID disponibili.
 * Completamente autocontenuta: legge tutti i PID internamente via CAN.
 * Attivata da #define DEBUG 1. Con DEBUG 0 non viene compilata.
 *
 * @since 31/03/26 Mattia Alesi
 */
void printAdvancedDiagnostics() {
  uint8_t data[8];
  uint8_t len;

  // Variabili locali per tutti i PID letti (riusate nei calcoli derivati)
  float loadPctL = 0, mapKpa = 0, baroKpa = 0, mafGs = 0, lambdaV = 1.0f;
  float iatC = 0, ambC = 0, coolC = 0, volts = 0;
  int rpm = 0, speedKmh = 0, pedalD = -1, pedalE = -1, throttle = 0;
  int egrCmd = 0, egrErr = 0, railBar = 0;
  unsigned long runtimeS = 0;
  bool hLoad = false, hMap = false, hBaro = false, hMaf = false, hLambda = false;
  bool hIat = false, hAmb = false, hCool = false, hRpm = false, hSpeed = false;
  bool hPedD = false, hPedE = false, hThrottle = false, hEgr = false, hEgrErr = false;
  bool hRail = false, hVolts = false, hRuntime = false;

  static float prevBoostBar = 0;
  static int prevSpeedKmh = 0;
  static unsigned long prevMs = 0;

  Serial.println(F("\n========== DIAGNOSTICA COMPLETA (47 parametri) =========="));

  // ---- LETTURA PID DIRETTI ----
  // NB: CAN restituisce dati in data[3..7], non data[0..3] come ELM327

  // 1. Carico motore (0x04) [0-100%]
  if (queryPID(0x04, data, &len)) {
    loadPctL = ((float)data[3] * 100.0f) / 255.0f; hLoad = true;
    Serial.print(F("  01. Carico motore: ")); Serial.print(loadPctL, 1);
    Serial.println(F(" %  [idle: 15-25%, pieno carico: 80-100%]"));
  } else { Serial.println(F("  01. Carico motore: N/D")); }

  // 2. Temp. liquido refrigerante (0x05) [-40..215 C]
  if (queryPID(0x05, data, &len)) {
    coolC = (float)data[3] - 40.0f; hCool = true;
    Serial.print(F("  02. Temp. liquido: ")); Serial.print((int)coolC);
    Serial.println(F(" C  [regime: 85-95 C]"));
  } else { Serial.println(F("  02. Temp. liquido: N/D")); }

  // 3. MAP (0x0B) [0-255 kPa]
  if (queryPID(0x0B, data, &len)) {
    mapKpa = (float)data[3]; hMap = true;
    Serial.print(F("  03. MAP: ")); Serial.print((int)mapKpa);
    Serial.println(F(" kPa  [aspirato: ~100, boost: 120-250]"));
  } else { Serial.println(F("  03. MAP: N/D")); }

  // 4. RPM (0x0C) [0-16383 rpm]
  if (queryPID(0x0C, data, &len)) {
    rpm = ((data[3] << 8) | data[4]) / 4; hRpm = true;
    Serial.print(F("  04. RPM: ")); Serial.print(rpm);
    Serial.println(F(" rpm  [idle: 750-850, max: ~4500]"));
  } else { Serial.println(F("  04. RPM: N/D")); }

  // 5. Velocita' (0x0D) [0-255 km/h]
  if (queryPID(0x0D, data, &len)) {
    speedKmh = data[3]; hSpeed = true;
    Serial.print(F("  05. Velocita: ")); Serial.print(speedKmh);
    Serial.println(F(" km/h"));
  } else { Serial.println(F("  05. Velocita: N/D")); }

  // 6. Temp. aria aspirazione IAT (0x0F) [-40..215 C]
  if (queryPID(0x0F, data, &len)) {
    iatC = (float)data[3] - 40.0f; hIat = true;
    Serial.print(F("  06. Temp. aria (IAT): ")); Serial.print((int)iatC);
    Serial.println(F(" C  [post-intercooler: ambiente+10-30]"));
  } else { Serial.println(F("  06. Temp. aria (IAT): N/D")); }

  // 7. MAF (0x10) [0-655.35 g/s]
  if (queryPID(0x10, data, &len)) {
    mafGs = ((float)((data[3] << 8) | data[4])) / 100.0f; hMaf = true;
    Serial.print(F("  07. MAF: ")); Serial.print(mafGs, 1);
    Serial.println(F(" g/s  [idle: 3-8, pieno carico: 40-80]"));
  } else { Serial.println(F("  07. MAF: N/D")); }

  // 8. Pressione rail (0x23) [0-655350 kPa]
  if (queryPID(0x23, data, &len)) {
    int railKpa = ((data[3] << 8) | data[4]) * 10;
    railBar = railKpa / 100; hRail = true;
    Serial.print(F("  08. Pressione rail: ")); Serial.print(railBar);
    Serial.println(F(" bar  [idle: 250-400, carico: 800-1800]"));
  } else { Serial.println(F("  08. Pressione rail: N/D")); }

  // 9. Lambda (0x24) [0-2 ratio]
  if (queryPID(0x24, data, &len)) {
    lambdaV = ((float)((data[3] << 8) | data[4])) / 32768.0f; hLambda = true;
    Serial.print(F("  09. Lambda: ")); Serial.print(lambdaV, 3);
    Serial.println(F("  [stechiometrico: 1.0, diesel normale: 1.3-3.5]"));

    // 10. Tensione O2 (0x24 byte C,D)
    float o2v = ((float)((data[5] << 8) | data[6])) / 8192.0f;
    Serial.print(F("  10. Tensione O2: ")); Serial.print(o2v, 3);
    Serial.println(F(" V  [0-8 V]"));
  } else {
    Serial.println(F("  09. Lambda: N/D"));
    Serial.println(F("  10. Tensione O2: N/D"));
  }

  // 11. EGR comandato (0x2C) [0-100%]
  if (queryPID(0x2C, data, &len)) {
    egrCmd = ((int)data[3] * 100) / 255; hEgr = true;
    Serial.print(F("  11. EGR comandato: ")); Serial.print(egrCmd);
    Serial.println(F(" %  [0=chiuso, regime: 10-50%]"));
  } else { Serial.println(F("  11. EGR comandato: N/D")); }

  // 12. Errore EGR (0x2D) [-100..+99%]
  if (queryPID(0x2D, data, &len)) {
    egrErr = ((int)data[3] - 128) * 100 / 128; hEgrErr = true;
    Serial.print(F("  12. Errore EGR: ")); Serial.print(egrErr);
    Serial.println(F(" %  [normale: -5..+5%, allarme: >|10|%]"));
  } else { Serial.println(F("  12. Errore EGR: N/D")); }

  // 13. Pressione barometrica (0x33) [0-255 kPa]
  if (queryPID(0x33, data, &len)) {
    baroKpa = (float)data[3]; hBaro = true;
    Serial.print(F("  13. Pressione baro: ")); Serial.print((int)baroKpa);
    Serial.println(F(" kPa  [livello mare: 101 kPa]"));
  } else { Serial.println(F("  13. Pressione baro: N/D")); }

  // 14. Tensione ECU (0x42) [0-65.5 V]
  if (queryPID(0x42, data, &len)) {
    volts = ((float)((data[3] << 8) | data[4])) / 1000.0f; hVolts = true;
    Serial.print(F("  14. Tensione ECU: ")); Serial.print(volts, 1);
    Serial.println(F(" V  [motore acceso: 13.5-14.5 V]"));
  } else { Serial.println(F("  14. Tensione ECU: N/D")); }

  // 15. Temp. esterna (0x46) [-40..215 C]
  if (queryPID(0x46, data, &len)) {
    ambC = (float)data[3] - 40.0f; hAmb = true;
    Serial.print(F("  15. Temp. esterna: ")); Serial.print((int)ambC);
    Serial.println(F(" C"));
  } else { Serial.println(F("  15. Temp. esterna: N/D")); }

  // 16. Pedale acceleratore D (0x49) [0-100%]
  if (queryPID(0x49, data, &len)) {
    pedalD = ((int)data[3] * 100) / 255; hPedD = true;
    Serial.print(F("  16. Pedale acc. D: ")); Serial.print(pedalD);
    Serial.println(F(" %"));
  } else { Serial.println(F("  16. Pedale acc. D: N/D")); }

  // 17. Pedale acceleratore E (0x4A) [0-100%]
  if (queryPID(0x4A, data, &len)) {
    pedalE = ((int)data[3] * 100) / 255; hPedE = true;
    Serial.print(F("  17. Pedale acc. E: ")); Serial.print(pedalE);
    Serial.println(F(" %"));
  } else { Serial.println(F("  17. Pedale acc. E: N/D")); }

  // 18. Farfalla comandata (0x4C) [0-100%]
  if (queryPID(0x4C, data, &len)) {
    throttle = ((int)data[3] * 100) / 255; hThrottle = true;
    Serial.print(F("  18. Farfalla cmd: ")); Serial.print(throttle);
    Serial.println(F(" %"));
  } else { Serial.println(F("  18. Farfalla cmd: N/D")); }

  // 19. Stato MIL + DTC (0x01)
  if (queryPID(0x01, data, &len)) {
    bool mil = (data[3] & 0x80) != 0;
    int dtcN = data[3] & 0x7F;
    Serial.print(F("  19. MIL: ")); Serial.print(mil ? "ACCESA" : "SPENTA");
    Serial.print(F(", DTC attivi: ")); Serial.println(dtcN);
  } else { Serial.println(F("  19. MIL/DTC: N/D")); }

  // 20. Tempo motore acceso (0x1F) [0-65535 s]
  if (queryPID(0x1F, data, &len)) {
    runtimeS = (data[3] << 8) | data[4]; hRuntime = true;
    int h = runtimeS / 3600;
    int m = (runtimeS % 3600) / 60;
    int s = runtimeS % 60;
    Serial.print(F("  20. Tempo motore: ")); Serial.print(h); Serial.print(F(":"));
    if (m < 10) Serial.print(F("0")); Serial.print(m); Serial.print(F(":"));
    if (s < 10) Serial.print(F("0")); Serial.println(s);
  } else { Serial.println(F("  20. Tempo motore: N/D")); }

  // 21. Km con MIL accesa (0x21)
  if (queryPID(0x21, data, &len)) {
    int kmMil = (data[3] << 8) | data[4];
    Serial.print(F("  21. Km con MIL: ")); Serial.print(kmMil);
    Serial.println(F(" km  [0 = nessun errore attivo]"));
  } else { Serial.println(F("  21. Km con MIL: N/D")); }

  // 22. Avviamenti da reset DTC (0x30)
  if (queryPID(0x30, data, &len)) {
    Serial.print(F("  22. Avviamenti da reset: ")); Serial.println(data[3]);
  } else { Serial.println(F("  22. Avviamenti da reset: N/D")); }

  // 23. Km da reset DTC (0x31)
  if (queryPID(0x31, data, &len)) {
    int kmReset = (data[3] << 8) | data[4];
    Serial.print(F("  23. Km da reset DTC: ")); Serial.print(kmReset);
    Serial.println(F(" km"));
  } else { Serial.println(F("  23. Km da reset DTC: N/D")); }

  // ---- VALORI CALCOLATI ----
  Serial.println(F("  --- CALCOLATI ---"));

  // 24. Boost (bar)
  if (hMap && hBaro) {
    float boostBar = (mapKpa - baroKpa) / 100.0f;
    Serial.print(F("  24. Boost: ")); Serial.print(boostBar, 2);
    Serial.println(F(" bar  [aspira: <0, turbo: 0.3-1.5]"));
  } else { Serial.println(F("  24. Boost: N/D")); }

  // 25-27. Coppia, Potenza kW, Potenza CV, 47. BSFC
  if (hLoad) {
    int torqueNmL = (int)(loadPctL * 400.0f / 100.0f);
    Serial.print(F("  25. Coppia stimata: ")); Serial.print(torqueNmL);
    Serial.println(F(" Nm  [idle: 60-100, max: 400]"));

    if (hRpm && rpm > 0) {
      float powKw = (float)torqueNmL * (float)rpm / 9549.0f;
      Serial.print(F("  26. Potenza stimata: ")); Serial.print(powKw, 1);
      Serial.println(F(" kW  [max: 132]"));
      float powCv = powKw * 1.36f;
      Serial.print(F("  27. Potenza stimata: ")); Serial.print(powCv, 1);
      Serial.println(F(" CV  [max: 180]"));

      if (hMaf && hLambda && powKw > 1.0f) {
        float fGs = mafGs / (14.5f * (lambdaV > 0.5f ? lambdaV : 1.0f));
        float bsfc = (fGs * 3600.0f) / powKw;
        Serial.print(F("  47. BSFC: ")); Serial.print((int)bsfc);
        Serial.println(F(" g/kWh  [ottimo: 200-250, normale: 250-350]"));
      } else { Serial.println(F("  47. BSFC: N/D (potenza insufficiente)")); }
    } else {
      Serial.println(F("  26. Potenza kW: N/D"));
      Serial.println(F("  27. Potenza CV: N/D"));
      Serial.println(F("  47. BSFC: N/D"));
    }
  } else {
    Serial.println(F("  25. Coppia: N/D"));
    Serial.println(F("  26. Potenza kW: N/D"));
    Serial.println(F("  27. Potenza CV: N/D"));
    Serial.println(F("  47. BSFC: N/D"));
  }

  // 28. AFR effettivo
  if (hLambda) {
    float afr = lambdaV * 14.5f;
    Serial.print(F("  28. AFR effettivo: ")); Serial.print(afr, 1);
    Serial.println(F("  [diesel: 18-50, stechiometrico: 14.5]"));
  } else { Serial.println(F("  28. AFR: N/D")); }

  // 29. Consumo L/h
  float fuelLh = 0;
  bool haveFuel = false;
  if (hMaf) {
    float lam = (hLambda && lambdaV > 0.5f) ? lambdaV : 1.0f;
    float fuelGs = mafGs / (14.5f * lam);
    fuelLh = (fuelGs * 3600.0f) / 835.0f;
    haveFuel = true;
    Serial.print(F("  29. Consumo: ")); Serial.print(fuelLh, 1);
    Serial.println(F(" L/h  [idle: 0.8-1.5, crociera: 4-8, max: 15-25]"));
  } else { Serial.println(F("  29. Consumo L/h: N/D")); }

  // 30. Consumo L/100km
  if (haveFuel && hSpeed && speedKmh > 3) {
    float l100 = (fuelLh / (float)speedKmh) * 100.0f;
    Serial.print(F("  30. Consumo: ")); Serial.print(l100, 1);
    Serial.println(F(" L/100km  [citta: 8-12, strada: 5-7, autostrada: 7-10]"));
  } else { Serial.println(F("  30. Consumo L/100km: N/D (fermo o dati mancanti)")); }

  // 31. Altitudine (m)
  if (hBaro && baroKpa > 0) {
    float altM = 44330.0f * (1.0f - pow(baroKpa / 101.325f, 0.1903f));
    Serial.print(F("  31. Altitudine: ")); Serial.print((int)altM);
    Serial.println(F(" m  [rif: 101.3 kPa = 0 m]"));
  } else { Serial.println(F("  31. Altitudine: N/D")); }

  // 32. Densita' aria
  if (hMap && hIat) {
    float rho = (mapKpa * 1000.0f) / (287.058f * (iatC + 273.15f));
    Serial.print(F("  32. Densita aria: ")); Serial.print(rho, 3);
    Serial.println(F(" kg/m3  [livello mare 20C: 1.204]"));
  } else { Serial.println(F("  32. Densita aria: N/D")); }

  // 33. Efficienza intercooler
  if (hMap && hBaro && hIat && hAmb && mapKpa > baroKpa) {
    float ambK = ambC + 273.15f;
    float tTeor = ambK * pow(mapKpa / baroKpa, 0.286f) - 273.15f;
    float denom = tTeor - ambC;
    if (denom > 1.0f) {
      int eff = (int)(((tTeor - iatC) / denom) * 100.0f);
      Serial.print(F("  33. Eff. intercooler: ")); Serial.print(eff);
      Serial.println(F(" %  [buono: 60-85%]"));
    } else { Serial.println(F("  33. Eff. intercooler: N/D (denom troppo piccolo)")); }
  } else { Serial.println(F("  33. Eff. intercooler: N/D (no boost)")); }

  // 34. Rapporto compressione turbo
  if (hMap && hBaro && baroKpa > 0) {
    float pr = mapKpa / baroKpa;
    Serial.print(F("  34. Rapporto compr. turbo: ")); Serial.print(pr, 2);
    Serial.println(F("  [1.0=aspirato, 1.5=0.5bar, 2.0=1bar]"));
  } else { Serial.println(F("  34. Rapporto compr. turbo: N/D")); }

  // 35. Delta temp motore-ambiente
  if (hCool && hAmb) {
    int delta = (int)(coolC - ambC);
    Serial.print(F("  35. Delta temp mot-amb: ")); Serial.print(delta);
    Serial.println(F(" C  [regime: 60-80 C sopra ambiente]"));
  } else { Serial.println(F("  35. Delta temp mot-amb: N/D")); }

  // 36. Efficienza volumetrica
  if (hMaf && hRpm && hMap && hIat && rpm > 0) {
    float rho = (mapKpa * 1000.0f) / (287.058f * (iatC + 273.15f));
    float volEff = (mafGs * 120.0f) / (2.698f * (float)rpm * rho / 1000.0f);
    Serial.print(F("  36. Eff. volumetrica: ")); Serial.print((int)volEff);
    Serial.println(F(" %  [normale: 70-95%]"));
  } else { Serial.println(F("  36. Eff. volumetrica: N/D")); }

  // 37. Drift pedale
  if (hPedD && hPedE) {
    int drift = abs(pedalD - pedalE);
    Serial.print(F("  37. Drift pedale: ")); Serial.print(drift);
    Serial.print(F(" % (D=")); Serial.print(pedalD);
    Serial.print(F(" E=")); Serial.print(pedalE);
    Serial.println(F(")  [normale: <3%, allarme: >5%]"));
  } else { Serial.println(F("  37. Drift pedale: N/D")); }

  // 39. Risposta farfalla
  if (hThrottle && hPedD) {
    int resp = throttle - pedalD;
    Serial.print(F("  39. Delta farfalla-pedale: ")); Serial.print(resp);
    Serial.println(F(" %  [normale: -5..+5%]"));
  } else { Serial.println(F("  39. Delta farfalla-pedale: N/D")); }

  // 40. Stato DFCO
  if (hLoad) {
    bool dfco = (loadPctL < 1.0f);
    Serial.print(F("  40. DFCO: ")); Serial.println(dfco ? "ATTIVO (iniezione tagliata)" : "OFF");
  } else { Serial.println(F("  40. DFCO: N/D")); }

  // 42. Rapporto CVT
  if (hRpm && hSpeed && speedKmh > 5 && rpm > 0) {
    float gearRatio = (float)rpm / ((float)speedKmh * 7.9f);
    Serial.print(F("  42. Rapporto CVT: ")); Serial.print(gearRatio, 2);
    Serial.println(F("  [basso(1a): ~3.5, alto(6a): ~0.6]"));
  } else { Serial.println(F("  42. Rapporto CVT: N/D (fermo)")); }

  // 43. Accelerazione
  unsigned long nowMs = millis();
  if (hSpeed && prevMs > 0) {
    float dtS = (float)(nowMs - prevMs) / 1000.0f;
    if (dtS > 0.05f) {
      float accMs2 = ((float)(speedKmh - prevSpeedKmh) / 3.6f) / dtS;
      Serial.print(F("  43. Accelerazione: ")); Serial.print(accMs2, 2);
      Serial.println(F(" m/s2  [frenata: <-3, acc. forte: >2]"));
    }
  } else { Serial.println(F("  43. Accelerazione: N/D (primo ciclo)")); }

  // 44. Variazione boost
  if (hMap && hBaro && prevMs > 0) {
    float dtS = (float)(nowMs - prevMs) / 1000.0f;
    float curBoost = (mapKpa - baroKpa) / 100.0f;
    if (dtS > 0.05f) {
      float dBoost = (curBoost - prevBoostBar) / dtS;
      Serial.print(F("  44. Variaz. boost: ")); Serial.print(dBoost, 2);
      Serial.println(F(" bar/s  [risposta turbo: >0.5 = reattivo]"));
      prevBoostBar = curBoost;
    }
  } else { Serial.println(F("  44. Variaz. boost: N/D (primo ciclo)")); }

  // 45. Sottoraffredamento motore
  if (hCool && hRuntime) {
    bool undertemp = (coolC < 75.0f && runtimeS > 600);
    Serial.print(F("  45. Sottoraffredamento: "));
    Serial.println(undertemp ? "SI (termostato?)" : "NO");
  } else { Serial.println(F("  45. Sottoraffredamento: N/D")); }

  // 46. Stato batteria/alternatore
  if (hVolts && hLoad) {
    bool motoreAcceso = (loadPctL > 5.0f);
    if (motoreAcceso) {
      const char* stato = "OK";
      if (volts < 12.5f) stato = "ALLARME BASSA";
      else if (volts < 13.5f) stato = "Bassa";
      else if (volts > 15.0f) stato = "ALLARME ALTA";
      else if (volts > 14.5f) stato = "Alta";
      Serial.print(F("  46. Stato batteria: ")); Serial.print(volts, 1);
      Serial.print(F(" V -> ")); Serial.println(stato);
    } else {
      Serial.print(F("  46. Batteria (quadro): ")); Serial.print(volts, 1);
      Serial.println(F(" V  [a motore spento: 12.2-12.8 V]"));
    }
  } else { Serial.println(F("  46. Stato batteria: N/D")); }

  prevSpeedKmh = hSpeed ? speedKmh : 0;
  prevMs = nowMs;

  Serial.println(F("==========================================================\n"));
}
#endif

// ============================================================
// UTILITA'
// ============================================================

/**
 * Calcola la posizione X per allineare un gruppo di stringhe al bordo destro del display.
 * Misura la larghezza di ogni stringa con il font corrente e restituisce
 * la X tale che la stringa piu' larga tocchi il bordo destro.
 *
 * @param strings   array di puntatori a stringa
 * @param count     numero di stringhe nell'array
 * @param displayW  larghezza display in pixel (128 per landscape)
 * @return posizione X da usare per drawStr()
 * @see updateDisplay()
 * @since 2026-03-20 mattia.Alesi
 */
int calcRightColumnX(const char* strings[], int count, int displayW) {
  int maxW = 0;
  for (int i = 0; i < count; i++) {
    int w = u8g2.getStrWidth(strings[i]);
    if (w > maxW) maxW = w;
  }
  return displayW - maxW;
}

/**
 * Disegna una stringa centrata orizzontalmente nel display.
 * Se la stringa e' piu' larga del display, viene allineata a sinistra (x=0).
 * Il font deve essere gia' impostato prima della chiamata.
 * @since 31/03/26 Mattia Alesi
 */
void drawStrCentered(int y, const char* str) {
#if DISPLAY_ORIENTATION == 0
  int x = (128 - (int)u8g2.getStrWidth(str)) / 2;
#else
  int x = (64 - (int)u8g2.getStrWidth(str)) / 2;
#endif
  u8g2.drawStr(x > 0 ? x : 0, y, str);
}

/** Disegna la schermata di scansione PID con stato e dettaglio — centrata H+V */
void drawScanScreen(const char* status, const char* detail) {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tr);
#if DISPLAY_ORIENTATION == 0
  drawStrCentered(11, "SCANSIONE PID");
  drawStrCentered(27, status);
  drawStrCentered(43, detail);
#else
  drawStrCentered(43, "SCANSIONE PID");
  drawStrCentered(59, status);
  drawStrCentered(75, detail);
#endif
  u8g2.sendBuffer();
}

/**
 * Mostra un messaggio di errore su OLED e Serial.
 * Usata per errori fatali (CAN init, ECU non risponde).
 */
void showError(const char* line1, const char* line2) {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tr);
#if DISPLAY_ORIENTATION == 0
  drawStrCentered(11, "ERRORE:");
  drawStrCentered(27, line1);
  drawStrCentered(43, line2);
#else
  drawStrCentered(43, "ERRORE:");
  drawStrCentered(59, line1);
  drawStrCentered(75, line2);
#endif
  u8g2.sendBuffer();
  Serial.print(F("ERRORE: "));
  Serial.println(line1);
  Serial.println(line2);
}
