/**
 * OBD2 WiFi Monitor per ESP8266 D1 Mini
 *
 * Mostra in tempo reale su display OLED SH1106 128x64:
 * - Pressione boost (bar)
 * - Temperatura olio motore (C)
 * - Coppia motore erogata (Nm)
 *
 * Comunica con un adattatore ELM327 WiFi tramite comandi AT su TCP.
 * All'avvio esegue una scansione dei PID OBD2 supportati dall'ECU.
 *
 * Hardware:
 *   ESP8266 D1 Mini + OLED SH1106 1.3" (I2C)
 *   Adattatore ELM327 WiFi (inserito nella presa OBD2 del veicolo)
 *
 * Cablaggio (solo display OLED):
 *   OLED VCC -> 3.3V
 *   OLED GND -> GND
 *   OLED SCL -> D1 (GPIO5)
 *   OLED SDA -> D2 (GPIO4)
 *
 * Velocità di acquisizione dati OBD (in WiFi) ~2Hz (round-robin PID + aggiornamento parziale display)
 *
 * Veicolo: Audi A5 B8 2.7 TDI (CGKA) Multitronic
 *
 * @since 2026-03-19 mattia.Alesi
 */

#include <ESP8266WiFi.h>
#include <U8g2lib.h>
#include <Wire.h>
/** No relative path with Arduino IDE => decided to not to define it as an ext. lib. so abs. path*/
#include "/Users/alesimattia/Documents/OBD2-car_dashboard-OLED/dtc_descriptions.h"  

// ============================================================
// CONFIGURAZIONE — Modificare qui se necessario
// ============================================================

// Rete WiFi dell'adattatore ELM327 (varia per modello)
#define ELM327_SSID       "WiFi_OBDII"
#define ELM327_PASS       ""              // Vuoto se rete aperta

// Indirizzo IP e porta TCP dell'ELM327
#define ELM327_IP         "192.168.0.10"
#define ELM327_PORT       35000

// Timeout risposta ELM327 (ms)
#define ELM327_TIMEOUT    2000
// Timeout piu' lungo per il primo comando OBD (l'ELM327 cerca il protocollo)
#define ELM327_FIRST_TIMEOUT 10000

// Protocollo OBD2: "ATSP0" = auto-detect, "ATSP6" = CAN 11bit 500kbps (Audi/VW)
#define ELM327_PROTOCOL   "ATSP0"

// Tentativi massimi di connessione
#define MAX_RETRIES       3

// Orientamento display: 0 = landscape (128x64), 1 = portrait 90° (64x128)
#define DISPLAY_ORIENTATION 0

// PID OBD2 per i dati di monitoraggio
#define PID_MAP           0x0B    // Pressione assoluta collettore (kPa)
#define PID_OIL_TEMP      0x5C    // Temperatura olio motore
#define PID_TORQUE_PCT    0x62    // Coppia motore attuale (%)
#define PID_TORQUE_REF    0x63    // Coppia di riferimento (Nm)
#define PID_FUEL_LEVEL    0x2F    // Livello carburante (%)

// Capacita' serbatoio in litri (Audi A5 B8)
#define TANK_CAPACITY     65

// Pressione atmosferica standard per calcolo boost
#define ATMOSPHERIC_KPA   101.325f

// Coppia di riferimento del motore CGKA 2.7 TDI (Nm)
// Usata se PID 0x63 non e' disponibile via OBD2 standard
#define TORQUE_REF_DEFAULT 400

// DTC (Diagnostic Trouble Codes)
#define MAX_DTC            6      // Max DTC gestibili (multi-line ELM327)
#define DTC_PER_PAGE_LAND  2      // DTC per pagina in landscape
#define DTC_PER_PAGE_PORT  3      // DTC per pagina in portrait
#define DTC_CHECK_INTERVAL 30000  // Intervallo controllo MIL (ms)
#define SCREEN_SWITCH_MS   5000   // Alternanza schermate dati/errori (ms)

// ============================================================
// Oggetti globali
// ============================================================

WiFiClient elmClient;

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

enum AppMode { MODE_CONNECT, MODE_SCAN, MODE_MONITOR };
AppMode currentMode = MODE_CONNECT;

// Bitmask PID supportati dall'ECU (256 bit = 32 byte)
uint8_t pidSupported[32];

// Flag: PID specifici supportati (determinati dalla scansione)
bool mapSupported       = false;
bool oilTempSupported   = false;
bool torquePctSupported = false;
bool torqueRefSupported = false;
bool fuelSupported      = false;

// Valori correnti delle letture
float boostBar    = 0.0f;
int   oilTempC    = 0;
int   torquePct   = 0;
int   torqueRefNm = TORQUE_REF_DEFAULT;
int   torqueNm    = 0;
int   fuelLiters  = 0;

// Flag disponibilita' runtime (false se PID va in timeout)
bool mapAvailable     = false;
bool oilTempAvailable = false;
bool torqueAvailable  = false;
bool fuelAvailable    = false;

// Coppia di riferimento: letta una sola volta all'inizio del monitor
bool torqueRefRead = false;

// DTC — codici errore e stato MIL
uint8_t  dtcCount = 0;
bool     milOn = false;
uint16_t dtcCodes[MAX_DTC];
unsigned long lastDtcCheck = 0;
unsigned long lastScreenSwitch = 0;
// Schermata corrente: 0 = dati, 1 = pagina DTC 1, 2 = pagina DTC 2, etc.
uint8_t  currentScreen = 0;
uint8_t  totalScreens  = 1;

// Flag per il primo comando OBD (timeout piu' lungo per auto-detect protocollo)
bool firstOBDQuery = true;

// Contatore cicli di lettura
unsigned long cycleCount = 0;

// ============================================================
// Round-robin: lettura un PID alla volta per ciclo (massimizza refresh display)
// Indici: 0=boost, 1=coppia, 2=olio, 3=fuel
// Boost e coppia pesati 4x rispetto a olio e fuel (cambiano piu' velocemente)
// ============================================================
const uint8_t PID_SCHEDULE[] PROGMEM = {0, 1, 0, 1, 2, 0, 1, 0, 1, 3};
const uint8_t PID_SCHEDULE_LEN = 10;
uint8_t pidScheduleIdx = 0;

// Dirty tracking: valori precedenti per aggiornamento parziale display
float prevBoostBar = -999.0f;
int prevBoostInt = -99999;   // (int)(boostBar * 100) per confronto senza errori float
int prevOilTempC = -999;
int prevTorqueNm = -999;
int prevFuelLiters = -999;
bool labelsDrawn = false;    // Etichette statiche gia' disegnate nel buffer
int cachedRightX = 86;       // Posizione X colonna destra, calcolata alla prima draw

// ============================================================
// SETUP
// ============================================================

void setup() {
  Serial.begin(115200);
  Serial.println(F("\n=== OBD2 WiFi Monitor ==="));
  Serial.println(F("Audi A5 B8 2.7 TDI CGKA"));

  // I2C su pin ESP01
  Wire.begin(0, 2);
  Wire.setClock(400000);

  // Inizializza display OLED (400kHz I2C esplicito anche via u8g2)
  u8g2.setBusClock(400000);
  u8g2.begin();
  u8g2.setFontPosTop();

  // Splash screen
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tr);
  u8g2.drawStr(10, 10, "OBD2 WIFI MON");
  u8g2.drawStr(14, 26, "A5 B8 2.7 TDI");
  u8g2.drawStr(40, 42, "CGKA");
  u8g2.drawStr(46, 54, "v1.0");
  u8g2.sendBuffer();
  delay(2000);

  memset(pidSupported, 0, sizeof(pidSupported));
  currentMode = MODE_CONNECT;
}

// ============================================================
// LOOP
// ============================================================

void loop() {
  switch (currentMode) {
    case MODE_CONNECT:
      executeConnectMode();
      break;
    case MODE_SCAN:
      executeScanMode();
      break;
    case MODE_MONITOR:
      executeMonitorMode();
      break;
  }
}

// ============================================================
// PARSING HEX
// ============================================================

/** Converte un singolo carattere hex ('0'-'9','A'-'F','a'-'f') in valore numerico */
uint8_t hexCharToVal(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  return 0;
}

/** Parsa 2 caratteri hex a partire dalla posizione pos e restituisce il byte */
uint8_t parseHexByte(const String& s, int pos) {
  if (pos + 1 >= (int)s.length()) return 0;
  return (hexCharToVal(s[pos]) << 4) | hexCharToVal(s[pos + 1]);
}

// ============================================================
// COMUNICAZIONE ELM327
// ============================================================

/**
 * Invia un comando AT/OBD all'ELM327 e attende la risposta.
 * Legge fino al prompt '>' dell'ELM327 o fino al timeout.
 * Restituisce true se una risposta e' stata ricevuta.
 */
bool sendATCommand(const char* cmd, String& response, unsigned long timeout) {
  if (!elmClient.connected()) return false;

  // Svuota eventuale dati residui nel buffer TCP
  while (elmClient.available()) {
    elmClient.read();
    yield();
  }

  // Invia comando + carriage return
  elmClient.print(cmd);
  elmClient.print('\r');

  Serial.print(F("TX: "));
  Serial.println(cmd);

  // Attendi risposta fino a '>' o timeout
  response = "";
  unsigned long start = millis();

  while ((millis() - start) < timeout) {
    if (elmClient.available()) {
      char c = elmClient.read();
      if (c == '>') break;  // Prompt ELM327 = fine risposta
      response += c;
    }
    yield();
  }

  // Rimuovi \r e \n dalla risposta
  response.replace("\r", "");
  response.replace("\n", "");

  Serial.print(F("RX: "));
  Serial.println(response);

  return response.length() > 0;
}

/**
 * Interroga un PID OBD2 via ELM327.
 * Invia il comando "01XX" e cerca nella risposta il pattern "41XX".
 * Estrae fino a maxBytes byte di dati dalla risposta hex.
 * Restituisce true se la risposta e' valida.
 */
bool queryOBDPID(uint8_t pid, uint8_t* dataBytes, uint8_t maxBytes, uint8_t* actualBytes) {
  // Formatta comando OBD: "01XX"
  char cmd[5];
  snprintf(cmd, sizeof(cmd), "01%02X", pid);

  // Timeout piu' lungo per la prima query (auto-detect protocollo)
  unsigned long timeout = firstOBDQuery ? ELM327_FIRST_TIMEOUT : ELM327_TIMEOUT;

  String response;
  if (!sendATCommand(cmd, response, timeout)) {
    return false;
  }

  firstOBDQuery = false;

  // Cerca il pattern di risposta "41XX" nella stringa
  char pattern[5];
  snprintf(pattern, sizeof(pattern), "41%02X", pid);
  // Cerca anche con lettere maiuscole (la risposta potrebbe essere maiuscola o minuscola)
  String upperResp = response;
  upperResp.toUpperCase();
  String upperPattern = String(pattern);
  upperPattern.toUpperCase();

  int pos = upperResp.indexOf(upperPattern);
  if (pos < 0) {
    // Risposta non contiene il pattern atteso (NO DATA, ERROR, etc.)
    return false;
  }

  // I byte dati iniziano subito dopo "41XX" (posizione +4 dal pattern)
  int dataStart = pos + 4;
  *actualBytes = 0;

  for (uint8_t i = 0; i < maxBytes; i++) {
    int bytePos = dataStart + (i * 2);
    if (bytePos + 1 >= (int)upperResp.length()) break;
    // Verifica che i caratteri siano hex validi
    char c1 = upperResp[bytePos];
    char c2 = upperResp[bytePos + 1];
    if (!isHexadecimalDigit(c1) || !isHexadecimalDigit(c2)) break;
    dataBytes[i] = parseHexByte(upperResp, bytePos);
    (*actualBytes)++;
  }

  return (*actualBytes) > 0;
}

// ============================================================
// GESTIONE BITMASK PID SUPPORTATI
// ============================================================

/**
 * Memorizza i 4 byte di risposta alla query di supporto PID
 * nel bitmask globale pidSupported[].
 * basePid: 0x00, 0x20, 0x40, 0x60
 * fourBytes: i 4 byte dati della risposta
 */
void storeSupportBitmask(uint8_t basePid, uint8_t* fourBytes) {
  uint8_t offset = basePid / 8;
  pidSupported[offset]     = fourBytes[0];
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
  uint8_t bitIndex  = 7 - ((pid - 1) % 8);
  return (pidSupported[byteIndex] & (1 << bitIndex)) != 0;
}

// ============================================================
// MODALITA' CONNESSIONE
// ============================================================

/**
 * Connette l'ESP8266 alla rete WiFi dell'ELM327, stabilisce la
 * connessione TCP e inizializza l'ELM327 con i comandi AT.
 */
void executeConnectMode() {
  // --- Fase 1: Connessione WiFi ---
  drawConnectScreen("WiFi...", ELM327_SSID);

  Serial.print(F("Connessione WiFi a: "));
  Serial.println(F(ELM327_SSID));

  WiFi.mode(WIFI_STA);
  WiFi.begin(ELM327_SSID, ELM327_PASS);

  int wifiAttempts = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(F("."));
    wifiAttempts++;
    if (wifiAttempts > 30) {
      Serial.println(F("\nWiFi non trovato!"));
      showError("WiFi non", "trovato!");
      delay(3000);
      wifiAttempts = 0;
      drawConnectScreen("WiFi...", ELM327_SSID);
      WiFi.begin(ELM327_SSID, ELM327_PASS);
    }
  }

  Serial.println(F("\nWiFi connesso!"));
  Serial.print(F("IP: "));
  Serial.println(WiFi.localIP());
  drawConnectScreen("WiFi OK!", "TCP...");

  // --- Fase 2: Connessione TCP all'ELM327 ---
  Serial.print(F("Connessione TCP a "));
  Serial.print(F(ELM327_IP));
  Serial.print(F(":"));
  Serial.println(ELM327_PORT);

  IPAddress elmIP;
  elmIP.fromString(ELM327_IP);

  for (int attempt = 1; attempt <= MAX_RETRIES; attempt++) {
    if (elmClient.connect(elmIP, ELM327_PORT)) {
      Serial.println(F("TCP connesso!"));
      drawConnectScreen("WiFi OK!", "TCP OK!");
      break;
    }
    Serial.print(F("TCP tentativo "));
    Serial.print(attempt);
    Serial.println(F(" fallito"));
    if (attempt == MAX_RETRIES) {
      showError("TCP fallito", ELM327_IP);
      while (true) { yield(); }
    }
    delay(1000);
  }

  delay(500);

  // --- Fase 3: Inizializzazione ELM327 ---
  drawConnectScreen("Init ELM327...", "");
  Serial.println(F("Inizializzazione ELM327..."));

  String response;

  // Reset ELM327 (timeout lungo, il reset puo' richiedere qualche secondo)
  sendATCommand("ATZ", response, 3000);
  delay(500);

  // Configurazione: echo off, linefeeds off, spazi off, headers off
  sendATCommand("ATE0", response, ELM327_TIMEOUT);
  sendATCommand("ATL0", response, ELM327_TIMEOUT);
  sendATCommand("ATS0", response, ELM327_TIMEOUT);
  sendATCommand("ATH0", response, ELM327_TIMEOUT);

  // Protocollo CAN (configurabile tramite ELM327_PROTOCOL)
  sendATCommand(ELM327_PROTOCOL, response, ELM327_TIMEOUT);

  Serial.println(F("ELM327 inizializzato"));
  drawConnectScreen("ELM327 OK!", "");
  delay(1000);

  // Reset stato per nuova sessione (utile in caso di riconnessione)
  memset(pidSupported, 0, sizeof(pidSupported));
  torqueRefRead = false;
  firstOBDQuery = true;
  labelsDrawn = false;
  pidScheduleIdx = 0;
  currentMode = MODE_SCAN;
}

// ============================================================
// MODALITA' SCANSIONE
// ============================================================

/**
 * Esegue la scansione dei PID supportati dall'ECU.
 * Interroga i range 0x00, 0x20, 0x40, 0x60 seguendo la catena di supporto.
 * Mostra il progresso su OLED e i risultati dettagliati su Serial.
 */
void executeScanMode() {
  drawScanScreen("Attendi...", "");

  uint8_t supportRanges[] = {0x00, 0x20, 0x40, 0x60};
  uint8_t dataBytes[4];
  uint8_t numBytes;
  bool chainContinues = true;

  Serial.println(F("\n--- SCANSIONE PID SUPPORTATI ---"));

  for (int i = 0; i < 4 && chainContinues; i++) {
    uint8_t rangePid = supportRanges[i];

    char progBuf[20];
    snprintf(progBuf, sizeof(progBuf), "Range: 0x%02X", rangePid);
    drawScanScreen("Scansione...", progBuf);

    Serial.print(F("Query PID 0x"));
    Serial.print(rangePid, HEX);
    Serial.print(F("... "));

    if (queryOBDPID(rangePid, dataBytes, 4, &numBytes) && numBytes == 4) {
      storeSupportBitmask(rangePid, dataBytes);

      Serial.print(F("OK ["));
      for (int j = 0; j < 4; j++) {
        if (dataBytes[j] < 0x10) Serial.print(F("0"));
        Serial.print(dataBytes[j], HEX);
        if (j < 3) Serial.print(F(" "));
      }
      Serial.println(F("]"));

      // L'ultimo bit indica se il range successivo e' supportato
      chainContinues = (dataBytes[3] & 0x01);
    } else {
      Serial.println(F("Nessuna risposta"));
      if (i == 0) {
        // Se nemmeno il primo range risponde, l'ECU non comunica
        showError("ECU non risp.", "Quadro acceso?");
        while (true) { yield(); }
      }
      break;
    }
    delay(100);
  }

  // Conta e stampa tutti i PID supportati
  int totalFound = 0;
  Serial.println(F("\nPID supportati:"));
  for (int p = 1; p <= 0x80; p++) {
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
  mapSupported       = isPIDSupported(PID_MAP);
  oilTempSupported   = isPIDSupported(PID_OIL_TEMP);
  torquePctSupported = isPIDSupported(PID_TORQUE_PCT);
  torqueRefSupported = isPIDSupported(PID_TORQUE_REF);
  fuelSupported      = isPIDSupported(PID_FUEL_LEVEL);

  // Log disponibilita'
  Serial.println(F("\nDisponibilita' PID monitor:"));
  Serial.print(F("  MAP (0x0B):        ")); Serial.println(mapSupported ? "SI" : "NO");
  Serial.print(F("  Oil Temp (0x5C):   ")); Serial.println(oilTempSupported ? "SI" : "NO");
  Serial.print(F("  Torque % (0x62):   ")); Serial.println(torquePctSupported ? "SI" : "NO");
  Serial.print(F("  Torque Ref (0x63): ")); Serial.println(torqueRefSupported ? "SI" : "NO");
  Serial.print(F("  Fuel Level (0x2F): ")); Serial.println(fuelSupported ? "SI" : "NO");

  // Mostra risultato su display
  char countBuf[20];
  snprintf(countBuf, sizeof(countBuf), "PID trovati: %d", totalFound);
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_5x7_tr);
  u8g2.drawStr(0, 0, "SCAN COMPLETA");
  u8g2.drawStr(0, 10, countBuf);
  u8g2.drawStr(0, 24, mapSupported       ? "BOOST:  SI" : "BOOST:  NO");
  u8g2.drawStr(0, 34, torquePctSupported ? "COPPIA: SI" : "COPPIA: NO");
  u8g2.drawStr(0, 44, oilTempSupported   ? "OLIO:   SI" : "OLIO:   NO");
  u8g2.drawStr(0, 54, fuelSupported      ? "FUEL:   SI" : "FUEL:   NO");
  u8g2.sendBuffer();

  delay(5000);
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
  uint8_t dataBytes[1];
  uint8_t numBytes;
  if (queryOBDPID(PID_MAP, dataBytes, 1, &numBytes) && numBytes >= 1) {
    float mapKpa = (float)dataBytes[0];
    *boost = (mapKpa - ATMOSPHERIC_KPA) / 100.0f;
    return true;
  }
  return false;
}

/**
 * Legge la temperatura olio motore.
 * @see PID 0x5C: 1 byte, formula = A - 40, range -40..215 C
 */
bool readOilTemperature(int* temp) {
  uint8_t dataBytes[1];
  uint8_t numBytes;
  if (queryOBDPID(PID_OIL_TEMP, dataBytes, 1, &numBytes) && numBytes >= 1) {
    *temp = (int)dataBytes[0] - 40;
    return true;
  }
  return false;
}

/**
 * Legge la coppia motore attuale in percentuale.
 * @see PID 0x62: 1 byte, formula = A - 125, range -125..130 %
 */
bool readTorquePercent(int* pct) {
  uint8_t dataBytes[1];
  uint8_t numBytes;
  if (queryOBDPID(PID_TORQUE_PCT, dataBytes, 1, &numBytes) && numBytes >= 1) {
    *pct = (int)dataBytes[0] - 125;
    return true;
  }
  return false;
}

/**
 * Legge la coppia motore di riferimento (valore costante del motore).
 * @see PID 0x63: 2 byte, formula = A*256 + B, range 0..65535 Nm
 */
bool readTorqueReference(int* refNm) {
  uint8_t dataBytes[2];
  uint8_t numBytes;
  if (queryOBDPID(PID_TORQUE_REF, dataBytes, 2, &numBytes) && numBytes >= 2) {
    *refNm = ((int)dataBytes[0] * 256) + (int)dataBytes[1];
    return true;
  }
  return false;
}

/**
 * Legge il livello carburante e lo converte in litri.
 * @see PID 0x2F: 1 byte, formula = A * 100 / 255 (%), poi * TANK_CAPACITY / 100
 */
bool readFuelLevel(int* liters) {
  uint8_t dataBytes[1];
  uint8_t numBytes;
  if (queryOBDPID(PID_FUEL_LEVEL, dataBytes, 1, &numBytes) && numBytes >= 1) {
    int pct = ((int)dataBytes[0] * 100) / 255;
    *liters = (pct * TANK_CAPACITY) / 100;
    return true;
  }
  return false;
}

// ============================================================
// LETTURA DTC (CODICI ERRORE)
// ============================================================

/**
 * Controlla lo stato MIL e il numero di DTC attivi via PID 0x01.
 * Se MIL accesa, legge i codici DTC con Mode 03.
 */
void checkMILStatus() {
  uint8_t dataBytes[4];
  uint8_t numBytes;
  if (queryOBDPID(0x01, dataBytes, 4, &numBytes) && numBytes >= 1) {
    milOn = (dataBytes[0] & 0x80) != 0;
    uint8_t count = dataBytes[0] & 0x7F;
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
 * Legge i codici DTC attivi via Mode 03 (comando "03" sull'ELM327).
 * Parsa la risposta hex "43XXYY..." e popola dtcCodes[].
 */
void readDTCCodes() {
  String response;
  if (!sendATCommand("03", response, ELM327_TIMEOUT)) {
    return;
  }

  // Cerca "43" nella risposta (Mode 03 response)
  String upperResp = response;
  upperResp.toUpperCase();
  int pos = upperResp.indexOf("43");
  if (pos < 0) return;

  // I byte DTC iniziano dopo "43": XXYY per ogni DTC
  int dataStart = pos + 2;
  dtcCount = 0;

  for (int i = 0; i < MAX_DTC; i++) {
    int bytePos = dataStart + (i * 4); // 4 hex chars = 2 byte per DTC
    if (bytePos + 3 >= (int)upperResp.length()) break;

    char c1 = upperResp[bytePos];
    char c2 = upperResp[bytePos + 1];
    char c3 = upperResp[bytePos + 2];
    char c4 = upperResp[bytePos + 3];
    if (!isHexadecimalDigit(c1) || !isHexadecimalDigit(c2) ||
        !isHexadecimalDigit(c3) || !isHexadecimalDigit(c4)) break;

    uint8_t hi = parseHexByte(upperResp, bytePos);
    uint8_t lo = parseHexByte(upperResp, bytePos + 2);
    uint16_t code = ((uint16_t)hi << 8) | lo;

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
 * @param slot indice nello scheduling (0=boost, 1=coppia, 2=olio, 3=fuel)
 * @return true se il PID corrispondente e' supportato
 * @see executeMonitorMode()
 * @since 24/03/26 Mattia Alesi
 */
bool isPidSlotSupported(uint8_t slot) {
  switch (slot) {
    case 0: return mapSupported;
    case 1: return torquePctSupported;
    case 2: return oilTempSupported;
    case 3: return fuelSupported;
    default: return false;
  }
}

/**
 * Esegue un ciclo di lettura OBD2 e aggiornamento display.
 * Legge UN solo PID per ciclo in round-robin pesato per massimizzare
 * la velocita' di refresh del display (~2 Hz invece di 1-3 Hz).
 * Boost e coppia vengono letti 4x piu' spesso di olio e fuel.
 *
 * @modified 24/03/26 — round-robin PID invece di lettura sequenziale
 */
void executeMonitorMode() {
  // Verifica connessione TCP attiva
  if (!elmClient.connected()) {
    Serial.println(F("Connessione TCP persa! Riconnessione..."));
    drawConnectScreen("Riconnessione..", "");
    elmClient.stop();
    delay(1000);
    labelsDrawn = false;
    currentMode = MODE_CONNECT;
    return;
  }

  // Controllo periodico MIL/DTC (ogni DTC_CHECK_INTERVAL ms)
  if (millis() - lastDtcCheck > DTC_CHECK_INTERVAL) {
    checkMILStatus();
    lastDtcCheck = millis();
  }

  // Coppia di riferimento: lettura singola (valore costante del motore)
  if (torqueRefSupported && !torqueRefRead) {
    int refFromEcu = 0;
    if (readTorqueReference(&refFromEcu) && refFromEcu > 0) {
      torqueRefNm = refFromEcu;
      torqueRefRead = true;
      Serial.print(F("Coppia riferimento ECU: "));
      Serial.print(torqueRefNm);
      Serial.println(F(" Nm"));
    }
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
      case 1:  // Coppia
        if (torquePctSupported) {
          torqueAvailable = readTorquePercent(&torquePct);
          if (torqueAvailable) {
            torqueNm = (torquePct * torqueRefNm) / 100;
          }
        }
        break;
      case 2:  // Olio
        if (oilTempSupported) {
          oilTempAvailable = readOilTemperature(&oilTempC);
        }
        break;
      case 3:  // Fuel
        if (fuelSupported) {
          fuelAvailable = readFuelLevel(&fuelLiters);
        }
        break;
    }
    updateDisplay();
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
    snprintf(val, sizeof(val), "%c%d.%02d bar", s, a / 100, a % 100);
  } else {
    snprintf(val, sizeof(val), "N/D");
  }
  u8g2.drawStr(0, 14, val);
}

/**
 * Disegna il valore coppia motore in Nm.
 * Colonna destra (cachedRightX), y=14. Font deve essere gia' impostato.
 * @see updateDisplay()
 * @since 24/03/26 Mattia Alesi
 */
void drawTorqueValue() {
  char val[14];
  if (torquePctSupported && torqueAvailable) {
    snprintf(val, sizeof(val), "%d Nm", torqueNm);
  } else {
    snprintf(val, sizeof(val), "N/D");
  }
  u8g2.drawStr(cachedRightX, 14, val);
}

/**
 * Disegna il valore temperatura olio motore.
 * Colonna sinistra, y=49. Font deve essere gia' impostato.
 * @see updateDisplay()
 * @since 24/03/26 Mattia Alesi
 */
void drawOilValue() {
  char val[14];
  if (oilTempSupported && oilTempAvailable) {
    snprintf(val, sizeof(val), "%d C", oilTempC);
  } else {
    snprintf(val, sizeof(val), "N/D");
  }
  u8g2.drawStr(0, 49, val);
}

/**
 * Disegna il valore livello carburante in litri.
 * Colonna destra (cachedRightX), y=49. Font deve essere gia' impostato.
 * @see updateDisplay()
 * @since 24/03/26 Mattia Alesi
 */
void drawFuelValue() {
  char val[14];
  if (fuelSupported && fuelAvailable) {
    snprintf(val, sizeof(val), "%d L", fuelLiters);
  } else {
    snprintf(val, sizeof(val), "N/D");
  }
  u8g2.drawStr(cachedRightX, 49, val);
}

/**
 * Aggiorna il display OLED con i valori correnti.
 * In landscape usa aggiornamento parziale: ridisegna solo le tile modificate
 * invece dell'intero framebuffer, riducendo il trasferimento I2C da ~1024 a ~256-384 byte.
 * Le etichette statiche (BOOST, COPPIA, OLIO, FUEL) vengono disegnate una sola volta.
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
  char rVal1[14];
  char rVal2[14];
  if (torquePctSupported && torqueAvailable) {
    snprintf(rVal1, sizeof(rVal1), "%d Nm", torqueNm);
  } else {
    snprintf(rVal1, sizeof(rVal1), "N/D");
  }
  if (fuelSupported && fuelAvailable) {
    snprintf(rVal2, sizeof(rVal2), "%d L", fuelLiters);
  } else {
    snprintf(rVal2, sizeof(rVal2), "N/D");
  }

  const char* rightStrs[] = { "COPPIA", rVal1, "FUEL", rVal2 };
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
    u8g2.drawStr(cachedRightX, 0, "COPPIA");
    u8g2.drawStr(0, 35, "OLIO");
    u8g2.drawStr(cachedRightX, 35, "FUEL");

    // Valori iniziali
    drawBoostValue();
    drawTorqueValue();
    drawOilValue();
    drawFuelValue();

    u8g2.sendBuffer();
    labelsDrawn = true;

    // Salva valori correnti per dirty tracking (sentinel -99999 = non disponibile)
    prevBoostInt = mapAvailable ? (int)(boostBar * 100.0f) : -99999;
    prevTorqueNm = torqueAvailable ? torqueNm : -99999;
    prevOilTempC = oilTempAvailable ? oilTempC : -99999;
    prevFuelLiters = fuelAvailable ? fuelLiters : -99999;
    return;
  }

  // --- AGGIORNAMENTO PARZIALE: solo valori cambiati ---
  // Sentinel -99999 codifica "PID non disponibile" per confronto corretto
  bool row1dirty = false;
  bool row2dirty = false;
  int curBoostInt = mapAvailable ? (int)(boostBar * 100.0f) : -99999;
  int curTorqueNm = torqueAvailable ? torqueNm : -99999;
  int curOilTempC = oilTempAvailable ? oilTempC : -99999;
  int curFuelLiters = fuelAvailable ? fuelLiters : -99999;

  // Boost (colonna sinistra, riga 1)
  if (curBoostInt != prevBoostInt) {
    clearValueArea(0, 14, cachedRightX, 14);
    drawBoostValue();
    prevBoostInt = curBoostInt;
    row1dirty = true;
  }

  // Coppia (colonna destra, riga 1)
  if (curTorqueNm != prevTorqueNm) {
    clearValueArea(cachedRightX, 14, 128 - cachedRightX, 14);
    drawTorqueValue();
    prevTorqueNm = curTorqueNm;
    row1dirty = true;
  }

  // Olio (colonna sinistra, riga 2)
  if (curOilTempC != prevOilTempC) {
    clearValueArea(0, 49, cachedRightX, 14);
    drawOilValue();
    prevOilTempC = curOilTempC;
    row2dirty = true;
  }

  // Fuel (colonna destra, riga 2)
  if (curFuelLiters != prevFuelLiters) {
    clearValueArea(cachedRightX, 49, 128 - cachedRightX, 14);
    drawFuelValue();
    prevFuelLiters = curFuelLiters;
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

  if (torquePctSupported && torqueAvailable) {
    snprintf(line, sizeof(line), "COPPIA %d", torqueNm);
  } else {
    snprintf(line, sizeof(line), "COPPIA N/D");
  }
  u8g2.drawStr(0, 47, line);

  if (oilTempSupported && oilTempAvailable) {
    snprintf(line, sizeof(line), "OLIO %d C", oilTempC);
  } else {
    snprintf(line, sizeof(line), "OLIO N/D");
  }
  u8g2.drawStr(0, 67, line);

  if (fuelSupported && fuelAvailable) {
    snprintf(line, sizeof(line), "FUEL %d L", fuelLiters);
  } else {
    snprintf(line, sizeof(line), "FUEL N/D");
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
      char descBuf[12];
      strncpy_P(descBuf, desc, sizeof(descBuf) - 1);
      descBuf[sizeof(descBuf) - 1] = '\0';
      u8g2.setFont(u8g2_font_5x8_tr);
      u8g2.drawStr(0, yBase + 14, descBuf);
    }
  }

#endif

  u8g2.sendBuffer();
}

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

/** Disegna la schermata di connessione WiFi/TCP */
void drawConnectScreen(const char* status, const char* detail) {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tr);
  u8g2.drawStr(0, 8, "CONNESSIONE");
  u8g2.drawStr(0, 28, status);
  u8g2.drawStr(0, 44, detail);
  u8g2.sendBuffer();
}

/** Disegna la schermata di scansione PID con stato e dettaglio */
void drawScanScreen(const char* status, const char* detail) {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tr);
  u8g2.drawStr(0, 8, "SCANSIONE PID");
  u8g2.drawStr(0, 28, status);
  u8g2.drawStr(0, 44, detail);
  u8g2.sendBuffer();
}

/**
 * Mostra un messaggio di errore su OLED e Serial.
 * Usata per errori fatali (connessione, ECU non risponde).
 */
void showError(const char* line1, const char* line2) {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tr);
  u8g2.drawStr(20, 16, "ERRORE:");
  u8g2.drawStr(0, 34, line1);
  u8g2.drawStr(0, 50, line2);
  u8g2.sendBuffer();
  Serial.print(F("ERRORE: "));
  Serial.println(line1);
  Serial.println(line2);
}
