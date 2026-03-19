/**
 * OBD2 Monitor per ESP8266 D1 Mini
 *
 * Mostra in tempo reale su display OLED SH1106 128x64:
 * - Pressione boost (bar)
 * - Temperatura olio motore (C)
 * - Coppia motore erogata (Nm)
 *
 * All'avvio esegue una scansione dei PID OBD2 supportati dall'ECU
 * e mostra i risultati su Serial Monitor e OLED.
 *
 * Hardware:
 *   ESP8266 D1 Mini + MCP2515 CAN (SPI) + OLED SH1106 1.3" (I2C)
 *
 * Cablaggio:
 *   MCP2515: VCC->5V, GND->GND, CS->D8, SO->D6, SI->D7, SCK->D5, INT->D0
 *   OLED:    VCC->3.3V, GND->GND, SCL->D1, SDA->D2
 *   OBD2:    CANH->Pin6, CANL->Pin14, GND->Pin4/5
 *
 * Velocità di acquisizione dati OBD 3-7Hz
 *
 * Veicolo: Audi A5 B8 2.7 TDI (CGKA) Multitronic
 *
 * @since 2026-03-19 mattia.Alesi
 */

#include <SPI.h>
#include <mcp_can.h>
#include <U8g2lib.h>
#include <Wire.h>

// ============================================================
// CONFIGURAZIONE — Modificare qui se necessario
// ============================================================

// Cristallo MCP2515: MCP_8MHZ o MCP_16MHZ (controlla il modulo)
#define MCP2515_CRYSTAL   MCP_8MHZ

// Pin SPI per MCP2515
#define CAN_CS_PIN        15    // D8 (GPIO15)

// Velocita' bus CAN OBD2
#define CAN_SPEED         CAN_500KBPS

// Protocollo OBD2
#define OBD2_REQUEST_ID   0x7DF   // Broadcast richiesta
#define OBD2_RESPONSE_ID  0x7E8   // Risposta ECU motore
#define OBD2_TIMEOUT_MS   100     // Timeout risposta (ms)
#define MAX_RETRIES       3       // Tentativi connessione iniziale

// PID OBD2 per i dati di monitoraggio
#define PID_MAP           0x0B    // Pressione assoluta collettore (kPa)
#define PID_OIL_TEMP      0x5C    // Temperatura olio motore
#define PID_TORQUE_PCT    0x62    // Coppia motore attuale (%)
#define PID_TORQUE_REF    0x63    // Coppia di riferimento (Nm)
#define PID_FUEL_LEVEL    0x2F    // Livello carburante (%)

// Capacita' serbatoio in litri (Audi A5 B8)
#define TANK_CAPACITY     65

// Orientamento display: 0 = landscape (128x64), 1 = portrait 90° (64x128)
#define DISPLAY_ORIENTATION 0

// Pressione atmosferica standard per calcolo boost
#define ATMOSPHERIC_KPA   101.325f

// Coppia di riferimento del motore CGKA 2.7 TDI (Nm)
// Usata se PID 0x63 non e' disponibile via OBD2 standard
#define TORQUE_REF_DEFAULT 400

// ============================================================
// Oggetti globali
// ============================================================

MCP_CAN CAN(CAN_CS_PIN);

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

enum AppMode { MODE_SCAN, MODE_MONITOR };
AppMode currentMode = MODE_SCAN;

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

  // Inizializza display OLED
  u8g2.begin();
  u8g2.setFontPosTop();

  // Splash screen
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tr);
  u8g2.drawStr(20, 10, "OBD2 MONITOR");
  u8g2.drawStr(14, 26, "A5 B8 2.7 TDI");
  u8g2.drawStr(40, 42, "CGKA");
  u8g2.drawStr(46, 54, "v1.0");
  u8g2.sendBuffer();
  delay(2000);

  // Inizializza MCP2515
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tr);
  u8g2.drawStr(0, 27, "Init CAN bus...");
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
  uint8_t data[8] = {0x02, 0x01, pid, 0x00, 0x00, 0x00, 0x00, 0x00};
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

  // Scansione range successivi (0x20, 0x40, 0x60)
  uint8_t nextRanges[] = {0x20, 0x40, 0x60};
  bool chainContinues = (data[6] & 0x01);

  for (int i = 0; i < 3 && chainContinues; i++) {
    uint8_t pid = nextRanges[i];

    char progBuf[20];
    snprintf(progBuf, sizeof(progBuf), "Range: 0x%02X", pid);
    drawScanScreen("ECU connessa!", progBuf);

    Serial.print(F("Query PID 0x"));
    Serial.print(pid, HEX);
    Serial.print(F("... "));

    if (queryPID(pid, data, &len)) {
      storeSupportBitmask(pid, &data[3]);
      Serial.print(F("OK ["));
      for (int j = 3; j < 7; j++) {
        if (data[j] < 0x10) Serial.print(F("0"));
        Serial.print(data[j], HEX);
        if (j < 6) Serial.print(F(" "));
      }
      Serial.println(F("]"));
      chainContinues = (data[6] & 0x01);
    } else {
      Serial.println(F("Nessuna risposta"));
      break;
    }
    delay(50);
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
 * Legge la temperatura olio motore.
 * @see PID 0x5C: 1 byte, formula = A - 40, range -40..215 C
 */
bool readOilTemperature(int* temp) {
  uint8_t data[8];
  uint8_t len;
  if (queryPID(PID_OIL_TEMP, data, &len)) {
    *temp = (int)data[3] - 40;
    return true;
  }
  return false;
}

/**
 * Legge la coppia motore attuale in percentuale.
 * @see PID 0x62: 1 byte, formula = A - 125, range -125..130 %
 */
bool readTorquePercent(int* pct) {
  uint8_t data[8];
  uint8_t len;
  if (queryPID(PID_TORQUE_PCT, data, &len)) {
    *pct = (int)data[3] - 125;
    return true;
  }
  return false;
}

/**
 * Legge la coppia motore di riferimento (valore costante del motore).
 * @see PID 0x63: 2 byte, formula = A*256 + B, range 0..65535 Nm
 */
bool readTorqueReference(int* refNm) {
  uint8_t data[8];
  uint8_t len;
  if (queryPID(PID_TORQUE_REF, data, &len)) {
    *refNm = ((int)data[3] * 256) + (int)data[4];
    return true;
  }
  return false;
}

/**
 * Legge il livello carburante e lo converte in litri.
 * @see PID 0x2F: 1 byte, formula = A * 100 / 255 (%), poi * TANK_CAPACITY / 100
 */
bool readFuelLevel(int* liters) {
  uint8_t data[8];
  uint8_t len;
  if (queryPID(PID_FUEL_LEVEL, data, &len)) {
    int pct = ((int)data[3] * 100) / 255;
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
 * Legge in sequenza: boost, olio, coppia (+ riferimento una sola volta).
 * Ogni ciclo impiega ~150-300ms (3 PID x 50-100ms ciascuno).
 */
void executeMonitorMode() {
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
 * Landscape: etichetta piccola sopra, valore grande sotto (4 gruppi in 64px)
 * Portrait: "NOME valore" su una riga con font grande (4 righe in 128px)
 */
void updateDisplay() {
  char buf[20];
  u8g2.clearBuffer();

#if DISPLAY_ORIENTATION == 0
  // --- LANDSCAPE (128x64) --- Due colonne, nome sopra valore sotto
  // Font 7x14B: 14px alto, 7px largo. 2 gruppi di 2 righe + gap 7px
  // Layout: 14+14+7+14+14 = 63px
  // Colonna sinistra (x=0): BOOST, OLIO — Colonna destra (x=66): COPPIA, FUEL
  char val[14];

  u8g2.setFont(u8g2_font_7x14B_tr);

  // Gruppo 1 (y=0..27): BOOST | COPPIA
  u8g2.drawStr(0, 0, "BOOST");
  if (mapSupported && mapAvailable) {
    int bc = (int)(boostBar * 100.0f);
    char s = (bc >= 0) ? '+' : '-';
    int a = abs(bc);
    snprintf(val, sizeof(val), "%c%d.%02d bar", s, a / 100, a % 100);
  } else { snprintf(val, sizeof(val), "N/D"); }
  u8g2.drawStr(0, 14, val);

  u8g2.drawStr(66, 0, "COPPIA");
  if (torquePctSupported && torqueAvailable) {
    snprintf(val, sizeof(val), "%d Nm", torqueNm);
  } else { snprintf(val, sizeof(val), "N/D"); }
  u8g2.drawStr(66, 14, val);

  // Gruppo 2 (y=35..62): OLIO | FUEL
  u8g2.drawStr(0, 35, "OLIO");
  if (oilTempSupported && oilTempAvailable) {
    snprintf(val, sizeof(val), "%d C", oilTempC);
  } else { snprintf(val, sizeof(val), "N/D"); }
  u8g2.drawStr(0, 49, val);

  u8g2.drawStr(66, 35, "FUEL");
  if (fuelSupported && fuelAvailable) {
    snprintf(val, sizeof(val), "%d L", fuelLiters);
  } else { snprintf(val, sizeof(val), "N/D"); }
  u8g2.drawStr(66, 49, val);

#else
  // --- PORTRAIT (64x128) --- Una colonna, "NOME valore" sulla stessa riga
  // Font 6x13B: 13px alto, 6px largo (bold). Max 10 chars in 64px.
  // 4 righe + 3 gap (7px): 4*13+3*7 = 73px, centrato in 128px → offset 27px
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
  } else { snprintf(line, sizeof(line), "BOOST N/D"); }
  u8g2.drawStr(0, 27, line);

  if (torquePctSupported && torqueAvailable) {
    snprintf(line, sizeof(line), "COPPIA %d", torqueNm);
  } else { snprintf(line, sizeof(line), "COPPIA N/D"); }
  u8g2.drawStr(0, 47, line);

  if (oilTempSupported && oilTempAvailable) {
    snprintf(line, sizeof(line), "OLIO %d C", oilTempC);
  } else { snprintf(line, sizeof(line), "OLIO N/D"); }
  u8g2.drawStr(0, 67, line);

  if (fuelSupported && fuelAvailable) {
    snprintf(line, sizeof(line), "FUEL %d L", fuelLiters);
  } else { snprintf(line, sizeof(line), "FUEL N/D"); }
  u8g2.drawStr(0, 87, line);

#endif

  u8g2.sendBuffer();
}

// ============================================================
// UTILITA'
// ============================================================

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
 * Usata per errori fatali (CAN init, ECU non risponde).
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
