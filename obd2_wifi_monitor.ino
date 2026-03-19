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
 * Velocità di acquisizione dati OBD (in WiFi) 1-3Hz
 *
 * Veicolo: Audi A5 B8 2.7 TDI (CGKA) Multitronic
 *
 * @since 2026-03-19 mattia.Alesi
 */

#include <ESP8266WiFi.h>
#include <U8x8lib.h>
#include <Wire.h>

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

// Orientamento display: 0 = normale, 1 = ruotato 180 gradi
#define DISPLAY_FLIP      0

// Font display: u8x8_font_torussansbold8_r (rotondo) o u8x8_font_chroma48medium8_r (spigoloso)
#define DISPLAY_FONT      u8x8_font_torussansbold8_r

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

// ============================================================
// Oggetti globali
// ============================================================

WiFiClient elmClient;

// Display OLED SH1106 128x64, I2C hardware, senza pin reset
U8X8_SH1106_128X64_NONAME_HW_I2C u8x8(U8X8_PIN_NONE);

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

// Flag per il primo comando OBD (timeout piu' lungo per auto-detect protocollo)
bool firstOBDQuery = true;

// Contatore cicli di lettura
unsigned long cycleCount = 0;

// ============================================================
// SETUP
// ============================================================

void setup() {
  Serial.begin(115200);
  Serial.println(F("\n=== OBD2 WiFi Monitor ==="));
  Serial.println(F("Audi A5 B8 2.7 TDI CGKA"));

  // I2C su pin ESP8266: SDA=GPIO4(D2), SCL=GPIO5(D1)
  Wire.begin(4, 5);
  Wire.setClock(400000);

  // Inizializza display OLED
  u8x8.begin();
  u8x8.setFlipMode(DISPLAY_FLIP);
  u8x8.setFont(DISPLAY_FONT);

  // Splash screen
  u8x8.clear();
  u8x8.drawString(1, 1, "OBD2 WIFI MON");
  u8x8.drawString(1, 3, "A5 B8 2.7 TDI");
  u8x8.drawString(5, 5, "CGKA");
  u8x8.drawString(5, 7, "v1.0");
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
  u8x8.clear();
  u8x8.drawString(0, 0, "CONNESSIONE");
  u8x8.drawString(0, 2, "WiFi:");
  u8x8.drawString(0, 3, ELM327_SSID);

  Serial.print(F("Connessione WiFi a: "));
  Serial.println(F(ELM327_SSID));

  WiFi.mode(WIFI_STA);
  WiFi.begin(ELM327_SSID, ELM327_PASS);

  int wifiAttempts = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(F("."));
    u8x8.drawString(wifiAttempts % 16, 5, ".");
    wifiAttempts++;
    if (wifiAttempts > 30) {
      Serial.println(F("\nWiFi non trovato!"));
      showError("WiFi non", "trovato!");
      delay(3000);
      u8x8.clear();
      wifiAttempts = 0;
      u8x8.drawString(0, 0, "CONNESSIONE");
      u8x8.drawString(0, 2, "WiFi:");
      u8x8.drawString(0, 3, ELM327_SSID);
      WiFi.begin(ELM327_SSID, ELM327_PASS);
    }
  }

  Serial.println(F("\nWiFi connesso!"));
  Serial.print(F("IP: "));
  Serial.println(WiFi.localIP());
  u8x8.drawString(0, 5, "WiFi OK!        ");

  // --- Fase 2: Connessione TCP all'ELM327 ---
  u8x8.drawString(0, 7, "TCP...");
  Serial.print(F("Connessione TCP a "));
  Serial.print(F(ELM327_IP));
  Serial.print(F(":"));
  Serial.println(ELM327_PORT);

  IPAddress elmIP;
  elmIP.fromString(ELM327_IP);

  for (int attempt = 1; attempt <= MAX_RETRIES; attempt++) {
    if (elmClient.connect(elmIP, ELM327_PORT)) {
      Serial.println(F("TCP connesso!"));
      u8x8.drawString(0, 7, "TCP OK!         ");
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
  u8x8.clear();
  u8x8.drawString(0, 0, "INIT ELM327");
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
  u8x8.drawString(0, 2, "ELM327 OK!");
  delay(1000);

  // Reset stato per nuova sessione (utile in caso di riconnessione)
  memset(pidSupported, 0, sizeof(pidSupported));
  torqueRefRead = false;
  firstOBDQuery = true;
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
  u8x8.clear();
  u8x8.drawString(0, 1, "SCANSIONE PID");
  u8x8.drawString(0, 3, "Attendi...");

  uint8_t supportRanges[] = {0x00, 0x20, 0x40, 0x60};
  uint8_t dataBytes[4];
  uint8_t numBytes;
  bool chainContinues = true;

  Serial.println(F("\n--- SCANSIONE PID SUPPORTATI ---"));

  for (int i = 0; i < 4 && chainContinues; i++) {
    uint8_t rangePid = supportRanges[i];

    char progBuf[17];
    snprintf(progBuf, sizeof(progBuf), "Range: 0x%02X    ", rangePid);
    u8x8.drawString(0, 5, progBuf);

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
  u8x8.clear();
  char countBuf[17];
  snprintf(countBuf, sizeof(countBuf), "PID trovati: %d", totalFound);
  u8x8.drawString(0, 0, "SCAN COMPLETA");
  u8x8.drawString(0, 2, countBuf);
  u8x8.drawString(0, 4, mapSupported       ? "BOOST:  SI" : "BOOST:  NO");
  u8x8.drawString(0, 5, oilTempSupported   ? "OLIO:   SI" : "OLIO:   NO");
  u8x8.drawString(0, 6, torquePctSupported ? "COPPIA: SI" : "COPPIA: NO");
  u8x8.drawString(0, 7, fuelSupported      ? "FUEL:   SI" : "FUEL:   NO");

  delay(5000);

  u8x8.clear();
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
// MODALITA' MONITOR
// ============================================================

/**
 * Esegue un ciclo completo di lettura OBD2 e aggiornamento display.
 * Verifica la connessione TCP prima di ogni ciclo.
 * Ogni ciclo impiega ~500-1500ms (ELM327 e' piu' lento del CAN diretto).
 */
void executeMonitorMode() {
  // Verifica connessione TCP attiva
  if (!elmClient.connected()) {
    Serial.println(F("Connessione TCP persa! Riconnessione..."));
    u8x8.clear();
    u8x8.drawString(0, 3, "Riconnessione..");
    elmClient.stop();
    delay(1000);
    currentMode = MODE_CONNECT;
    return;
  }

  // Coppia di riferimento: lettura singola (valore costante del motore)
  // Se PID 0x63 non disponibile, usa il default TORQUE_REF_DEFAULT (400 Nm per CGKA)
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

  // Lettura boost (MAP)
  if (mapSupported) {
    mapAvailable = readBoostPressure(&boostBar);
  }

  // Lettura temperatura olio motore
  if (oilTempSupported) {
    oilTempAvailable = readOilTemperature(&oilTempC);
  }

  // Lettura coppia motore attuale — converte sempre in Nm
  // Usa torqueRefNm da ECU se disponibile, altrimenti il default (400 Nm)
  if (torquePctSupported) {
    torqueAvailable = readTorquePercent(&torquePct);
    if (torqueAvailable) {
      torqueNm = (torquePct * torqueRefNm) / 100;
    }
  }

  // Lettura livello carburante
  if (fuelSupported) {
    fuelAvailable = readFuelLevel(&fuelLiters);
  }

  // Aggiorna display
  updateDisplay();
  yield();
}

/**
 * Aggiorna il display OLED con i valori correnti.
 * Layout a due colonne (16x8 griglia U8x8):
 *   Riga 0: BOOST    OLIO      (labels)
 *   Riga 2: +0.85bar  92C      (valori)
 *   Riga 4: COPPIA  280 Nm     (coppia)
 *   Riga 6: ciclo:1234         (status)
 */
void updateDisplay() {
  char buf[9]; // 8 chars + null per ogni colonna

  // Riga 0: Labels — colonna sinistra BOOST, colonna destra OLIO
  u8x8.drawString(0, 0, "BOOST   ");
  u8x8.drawString(8, 0, "OLIO    ");

  // Riga 2: Valori — colonna sinistra boost, colonna destra olio
  if (mapSupported && mapAvailable) {
    int boostCenti = (int)(boostBar * 100.0f);
    char sign = (boostCenti >= 0) ? '+' : '-';
    int absVal = abs(boostCenti);
    snprintf(buf, sizeof(buf), "%c%d.%02dbar", sign, absVal / 100, absVal % 100);
  } else {
    snprintf(buf, sizeof(buf), "  N/D   ");
  }
  u8x8.drawString(0, 2, buf);

  if (oilTempSupported && oilTempAvailable) {
    snprintf(buf, sizeof(buf), " %3d C  ", oilTempC);
  } else {
    snprintf(buf, sizeof(buf), "  N/D   ");
  }
  u8x8.drawString(8, 2, buf);

  // Riga 4: Coppia motore (riga piena, sempre in Nm)
  if (torquePctSupported && torqueAvailable) {
    char line[17];
    snprintf(line, sizeof(line), "COPPIA  %4d Nm ", torqueNm);
    u8x8.drawString(0, 4, line);
  } else {
    u8x8.drawString(0, 4, "COPPIA   N/D    ");
  }

  // Riga 6: Livello carburante in litri
  char line[17];
  if (fuelSupported && fuelAvailable) {
    snprintf(line, sizeof(line), "FUEL    %3d L   ", fuelLiters);
  } else {
    snprintf(line, sizeof(line), "FUEL     N/D    ");
  }
  u8x8.drawString(0, 6, line);
}

// ============================================================
// UTILITA'
// ============================================================

/**
 * Mostra un messaggio di errore su OLED e Serial.
 * Usata per errori fatali (connessione, ECU non risponde).
 */
void showError(const char* line1, const char* line2) {
  u8x8.clear();
  u8x8.drawString(3, 2, "ERRORE:");
  u8x8.drawString(0, 4, line1);
  u8x8.drawString(0, 6, line2);
  Serial.print(F("ERRORE: "));
  Serial.println(line1);
  Serial.println(line2);
}
