/**
 * OBD2 Monitor per ESP8266 D1 Mini
 *
 * Mostra in tempo reale su display OLED SH1106 128x64:
 * - Pressione boost (bar), letta dalla centralina (PID 0x70) o stimata da BoostModel
 * - Coppia motore all'albero (Nm), letta dalla centralina (PID 0x62, 0x8E, 0x63) o stimata da TorqueModel
 * - Temperatura liquido raffreddamento (C)
 * - Carica batteria ibrida (%, PID 0x5B se la vettura lo espone)
 *
 * All'avvio scopre tutte le centraline OBD e i PID Mode 01 e InfoType Mode 09 che dichiarano
 * (report su Serial e pagina /scan); la scansione approfondita si avvia dalla pagina web.
 *
 * Hardware:
 *   ESP8266 D1 Mini + MCP2515/TJA1050 CAN (modulo integrato, SPI) + OLED SH1106 1.3" (I2C)
 *
 * Cablaggio:
 *   MCP2515: VCC->5V, GND->GND, CS->D8, SO->D6, SI->D7, SCK->D5, INT->D0
 *   OLED:    VCC->3.3V, GND->GND, SCL->D1, SDA->D2
 *   OBD2:    CANH->Pin6, CANL->Pin14, GND->Pin4/5
 *   Pulsante: NA tra D3 (GPIO0) e GND. Click breve = scorri schermate;
 *             pressione lunga (>=2.5s) = cancella DTC (OBD2 Mode 04).
 *
 * Acquisizione: una richiesta multi-PID al motore per ciclo (fino a 6 PID), con ripiego automatico
 * a un PID per ciclo se la centralina non accetta le richieste multiple; aggiornamento parziale del display.
 *
 * Veicolo: Audi A5 F5 (B9.5) 40 TFSI 150 kW mild hybrid, motore DMSA, cambio S tronic 0CK
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
#include "TorqueModel.h"
#include "BoostModel.h"
#include "button_handler.h"
#include "light_sensor.h"
#include "pid_descriptions.h"

// ============================================================
// CONFIGURAZIONE — Modificare qui se necessario
// ============================================================

// Cristallo MCP2515: MCP_8MHZ o MCP_16MHZ (controlla il modulo)
#define MCP2515_CRYSTAL MCP_8MHZ

// Pin SPI per MCP2515
#define CAN_CS_PIN 15  // D8 (GPIO15)

// Velocita' bus CAN OBD2
#define CAN_SPEED CAN_500KBPS

// Attesa massima fra due consecutive frame ISO-TP (N_Cr di ISO 15765-4 = 150 ms)
#define ISOTP_CONSECUTIVE_TIMEOUT_MS 150
// Attesa estesa dopo una risposta 7F xx 78 (P2*CAN di ISO 15765-4)
#define RESPONSE_PENDING_TIMEOUT_MS 5000
// Cicli consecutivi senza risposta dal motore prima del ripristino (bus-off o nuovo scan)
#define SILENT_CYCLES_BEFORE_RECOVERY 10

// OTA — Aggiornamento firmware via browser (http://192.168.4.1/update)
#define OTA_SSID    "OBD2_UPDATE"   // SSID del SoftAP per OTA
#define OTA_PASS    "alesimattia"     // WPA2 (min 8 chars)
#define OTA_PORT    80              // web server
#define OTA_WINDOW_MS 180000        // Finestra OTA: 3 minuti dal boot

// Diagnostica avanzata su Serial: attivabile runtime da /dashboard
bool debugMode = false;
#define DIAG_PRINT_INTERVAL_MS 5000  // Intervallo minimo fra due stampe della diagnostica

// Layout display landscape 128x64 (font 7x14B, 7px wide)
// Colonna DX: "100%" = 4 chars × 7px = 28px → 128-28 = 100
// I valori della temperatura più larghi traslano a sinistra dinamicamente.
#define RIGHT_COL_X 100
#define TEMP_EXTENDED_X 93    // Min x TEMP: "-40 C"/"215 C" = 35px → 128-35

// DTC (Diagnostic Trouble Codes)
#define DTC_PER_PAGE      2       // DTC per pagina display
#define SCREEN_SWITCH_MS 5000     // Alternanza schermate dati/errori (ms)

// ============================================================
// Oggetti globali
// ============================================================

MCP_CAN CAN(CAN_CS_PIN);
ESP8266WebServer httpServer(OTA_PORT);
ESP8266HTTPUpdateServer httpUpdater;

// Display OLED SH1106 128x64, I2C hardware, landscape
U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);

// ============================================================
// Stato applicazione
// ============================================================

enum AppMode { MODE_SCAN,
               MODE_MONITOR };
AppMode currentMode = MODE_SCAN;

unsigned long lastScreenSwitch = 0;
// Schermata corrente: 0 = dati, 1 = pagina DTC 1, 2 = pagina DTC 2, etc.
uint8_t currentScreen = 0;
uint8_t totalScreens = 1;  // Calcolato in base a dtcCount
// Ultima pagina DTC disegnata: la pagina si ridisegna solo quando cambia (0xFF = forza ridisegno)
uint8_t lastDrawnDtcScreen = 0xFF;

// Dirty tracking: valori precedenti per aggiornamento parziale display
int prevBoostInt = -99999;   // (int)(boostBar * 100) per confronto senza errori float
int prevTorqueNm = -99999;
int prevCoolantC = -99999;
int prevHybBattPct = -99999;
bool labelsDrawn = false;    // Etichette statiche gia' disegnate nel buffer

// OTA: true finche' la finestra di aggiornamento è attiva
bool otaActive = true;
unsigned long otaDeadline = OTA_WINDOW_MS;
bool otaClientWasConnected = false;

// Invii CAN falliti di fila: con TXREQ bloccato nel MCP2515 serve una reinizializzazione
uint8_t canTxFailures = 0;
unsigned long lastDiagPrint = 0;

// Logger seriale: cattura Serial.* nel buffer circolare leggibile via /serial-data.
// Va incluso DOPO tutti gli header di sistema (Arduino.h ecc.) per non
// rompere "extern HardwareSerial Serial" nelle loro dichiarazioni.
#include "serial_logger.h"

// Discovery e lettura OBD condivise con la build WiFi; il trasporto è obdTransact() più sotto
#include "obd_proto.h"
#include "obd_scan.h"
#include "obd_live.h"

// Dashboard web live (endpoint /dashboard, /data, /scan, /probe, /debug)
#define OBD_CONN_CAN
#include "web_dashboard.h"

// ============================================================
// SETUP
// ============================================================

void setup() {
  Serial.begin(115200);
  Serial.println(F("\n=== OBD2 Monitor ==="));
  Serial.println(F("Audi A5 F5 (B9.5) 40 TFSI DMSA S tronic"));

  // I2C su pin ESP8266: SDA=GPIO4(D2), SCL=GPIO5(D1)
  // (sono i default del core ESP8266, ma li dichiariamo per chiarezza)
  Wire.begin(4, 5);

  // SoftAP per aggiornamento OTA via browser
  WiFi.mode(WIFI_AP);
  WiFi.softAP(OTA_SSID, OTA_PASS);
  httpUpdater.setup(&httpServer, "/update");
  setupWebDashboard(httpServer);
  httpServer.begin();
  Serial.println(F("OTA pronto: http://192.168.4.1/update"));

  // Inizializza display OLED (400kHz I2C esplicito anche via u8g2)
  u8g2.setBusClock(400000);
  u8g2.begin();
  u8g2.setFontPosTop();
  initBrightness();  // legge LDR e applica contrasto iniziale

  // Splash screen — centrato verticalmente
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tr);
  u8g2.drawStr(28, 11, "OBD2 MONITOR");
  u8g2.drawStr(19, 27, "A5 B9.5 40 TFSI");
  u8g2.drawStr(16, 54, "OTA: 192.168.4.1");
  u8g2.sendBuffer();
  delay(2000);

  // Inizializza MCP2515
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tr);
  u8g2.drawStr(0, 27, "Init CAN bus...");
  u8g2.sendBuffer();
  Serial.println(F("Inizializzazione MCP2515..."));

  if (!canInit()) {
    Serial.println(F("ERRORE: MCP2515 init fallito!"));
    showError("CAN init fail", "Controlla SPI");
    while (true) { yield(); }
  }
  Serial.println(F("MCP2515 OK - CAN 500kbps"));

  // Pulsante fisico (clear DTC + cycle schermate)
  buttonInit();

  currentMode = MODE_SCAN;
}

// ============================================================
// LOOP
// ============================================================

void loop() {
  updateBrightness();  // sample LDR + state machine auto-brightness (non bloccante)

  /** OTA attivo finche' otaDeadline non scade E nessun client è connesso.
  * Se un client si connette, il timeout si sospende.
  * Quando il client si disconnette, il timeout riparte da OTA_WINDOW_MS */
  if (otaActive) {
    httpServer.handleClient();
    bool clientNow = (WiFi.softAPgetStationNum() > 0);
    if (clientNow) {
      otaClientWasConnected = true;
    } else if (otaClientWasConnected) {
      otaDeadline = millis() + OTA_WINDOW_MS;
      otaClientWasConnected = false;
      Serial.println(F("OTA: client disconnesso, timeout ripartito"));
    }
    if (!clientNow && millis() > otaDeadline) {
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
      if (obdProbeRunning()) {
        executeProbeStep();
      } else {
        executeMonitorMode();
      }
      break;
  }
}

// ============================================================
// TRASPORTO CAN / OBD2 (ISO 15765-4)
// ============================================================

/**
 * Inizializza il MCP2515 a 500 kbps accettando tutti gli ID: con MCP_ANY maschere e filtri non hanno
 * effetto e begin() lascia il chip in loopback, quindi il modo normale va impostato esplicitamente.
 * @since 23/10/26 Mattia Alesi
 */
bool canInit() {
  if (CAN.begin(MCP_ANY, CAN_SPEED, MCP2515_CRYSTAL) != CAN_OK) return false;
  CAN.setMode(MCP_NORMAL);
  return true;
}

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
 * Invia un frame a 11 bit con DLC 8, come richiede ISO 15765-4. Dopo tre invii falliti di fila
 * (buffer di trasmissione bloccati con TXREQ attivo) reinizializza il MCP2515.
 */
bool canSendFrame(uint16_t id, const uint8_t* frame) {
  uint8_t data[8];
  memcpy(data, frame, 8);
  if (CAN.sendMsgBuf(id, 0, 8, data) == CAN_OK) {
    canTxFailures = 0;
    return true;
  }
  if (++canTxFailures >= 3) {
    Serial.println(F("CAN: trasmissione bloccata, re-init MCP2515"));
    canInit();
    canTxFailures = 0;
  }
  return false;
}

/**
 * Trasporto OBD su CAN nativo: invia la richiesta single frame e riassembla le risposte di ogni
 * centralina. Al first frame di un messaggio multi-frame invia il flow control 30 00 00 all'ID fisico
 * della centralina (risposta - 8) ed estende l'attesa di N_Cr; una risposta 7F xx 78 estende l'attesa
 * a P2*CAN. Legge i frame a raffica e cede la CPU solo a coda vuota, perchè il MCP2515 ha due buffer.
 * @see obd_scan.h obdTransact()
 */
uint8_t obdTransact(uint16_t requestId, const uint8_t* request, uint8_t requestLen, uint16_t windowMs,
                    uint16_t waitForEcuId) {
  obdCollectorReset(obdCollector);
  if (requestLen == 0 || requestLen > 7) return 0;
  flushCANBuffer();
  uint8_t frame[8] = { 0 };
  frame[0] = requestLen;
  memcpy(frame + 1, request, requestLen);
  if (!canSendFrame(requestId, frame)) return 0;

  unsigned long deadline = millis() + windowMs;
  while ((long)(millis() - deadline) < 0) {
    if (CAN.checkReceive() != CAN_MSGAVAIL) {
      yield();
      continue;
    }
    unsigned long rxId;
    uint8_t len;
    uint8_t buf[8];
    CAN.readMsgBuf(&rxId, &len, buf);
    if ((rxId & 0x80000000UL) || !obdIsResponseId(rxId)) continue;
    ObdIsoTpRx* slot = obdCollectorSlot(obdCollector, (uint16_t)rxId);
    if (!slot) continue;

    if (obdIsoTpFeed(*slot, buf, len)) {
      uint8_t flowControl[8] = { 0x30, 0x00, 0x00, 0, 0, 0, 0, 0 };
      canSendFrame(obdPhysicalRequestId((uint16_t)rxId), flowControl);
    }
    if (slot->state == OBD_ISOTP_RECEIVING) {
      unsigned long frameDeadline = millis() + ISOTP_CONSECUTIVE_TIMEOUT_MS;
      if ((long)(frameDeadline - deadline) > 0) deadline = frameDeadline;
      continue;
    }
    if (slot->state != OBD_ISOTP_COMPLETE) continue;

    uint8_t service, nrc;
    if (obdIsNegativeResponse(slot->data, slot->receivedLen, &service, &nrc) && nrc == OBD_NRC_RESPONSE_PENDING) {
      obdIsoTpReset(*slot, (uint16_t)rxId);
      deadline = millis() + RESPONSE_PENDING_TIMEOUT_MS;
      continue;
    }
    if (waitForEcuId != 0 && rxId == waitForEcuId) break;
  }
  return obdCollectorCompleteCount(obdCollector);
}

// ============================================================
// MODALITÀ SCANSIONE
// ============================================================

/**
 * Mostra l'avanzamento dello scan sul display.
 * @see obd_scan.h obdShowScanProgress()
 */
void obdShowScanProgress(const char* status, const char* detail) {
  drawScanScreen(status, detail);
}

/** Attende ms millisecondi continuando a servire il web server (OTA e dashboard). */
void waitServingHttp(unsigned long ms) {
  unsigned long until = millis() + ms;
  while ((long)(millis() - until) < 0) {
    if (otaActive) httpServer.handleClient();
    yield();
  }
}

/**
 * Mostra su OLED il riepilogo dello scan: numero di centraline e PID dichiarati dalle prime due.
 * @since 07/05/26 Mattia Alesi
 */
void drawScanSummaryOLED() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tr);
  drawStrCentered(0, "SCAN COMPLETA");

  char buf[32];
  snprintf(buf, sizeof(buf), "Centraline: %u", (unsigned)ecuCount);
  u8g2.drawStr(0, 16, buf);

  // Una riga per centralina, max 2 (le restanti sono solo su Serial)
  uint8_t shown = ecuCount > 2 ? 2 : ecuCount;
  for (uint8_t e = 0; e < shown; e++) {
    char name[16];
    obdEcuShortName(e, name, sizeof(name));
    snprintf(buf, sizeof(buf), "%s: %u PID", name, (unsigned)obdCountDataIds(ecuResults[e].mode01, 0xFF));
    u8g2.drawStr(0, 32 + 12 * e, buf);
  }
  if (ecuCount > 2) {
    snprintf(buf, sizeof(buf), "+%u altre (Serial)", (unsigned)(ecuCount - 2));
    u8g2.drawStr(0, 56, buf);
  }
  u8g2.sendBuffer();
}

/**
 * Esegue lo scan delle centraline e dei PID (obd_scan.h), stampa il report e prepara la lettura live.
 * Se nessuno risponde o manca il motore 0x7E8 riprova dopo 10 s, servendo comunque il web server.
 */
void executeScanMode() {
  uint8_t outcome = obdRunBootScan();
  if (outcome != OBD_SCAN_OK) {
    if (outcome == OBD_SCAN_NO_ENGINE) {
      obdPrintScanReport();
      showError("Motore non risponde", "Quadro acceso?");
    } else {
      showError("ECU non risp.", "Quadro acceso?");
    }
    Serial.println(F("Scan fallito, nuovo tentativo tra 10 s"));
    waitServingHttp(10000);
    return;
  }

  obdPrintScanReport();
  obdLiveConfigure();
  drawScanSummaryOLED();
  waitServingHttp(2500);

  // Reset stato display e DTC per la nuova sessione monitor
  labelsDrawn = false;
  currentScreen = 0;
  totalScreens = 1;
  dtcCheckRequested = true;
  currentMode = MODE_MONITOR;
}

/**
 * Esegue un passo della scansione approfondita avviata dal web e ne mostra l'avanzamento sul display;
 * al termine il monitor riprende con un ridisegno completo.
 */
void executeProbeStep() {
  static unsigned long lastDraw = 0;
  obdProbeStep();
  bool running = obdProbeRunning();
  if (!running || millis() - lastDraw > 500) {
    char line[22];
    snprintf(line, sizeof(line), "%u%%", (unsigned)obdProbePercent());
    drawStatusScreen("SCANSIONE APPROF.", running ? line : "Completata");
    lastDraw = millis();
  }
  if (!running) {
    labelsDrawn = false;
    lastDrawnDtcScreen = 0xFF;
  }
}

// ============================================================
// MODALITA' MONITOR
// ============================================================

/** Ricalcola il numero di schermate dopo una lettura DTC e forza il ridisegno della pagina. */
void updateScreenCount() {
  totalScreens = dtcCount > 0 ? 1 + (dtcCount + DTC_PER_PAGE - 1) / DTC_PER_PAGE : 1;
  if (currentScreen >= totalScreens) currentScreen = 0;
  lastDrawnDtcScreen = 0xFF;
}

/**
 * Esegue un ciclo del monitor: pulsante, controllo periodico dei DTC, una lettura live (obd_live.h)
 * anche mentre sono visibili le pagine DTC, aggiornamento del display e diagnostica seriale.
 * Se il motore resta muto per SILENT_CYCLES_BEFORE_RECOVERY cicli reinizializza il MCP2515 in bus-off,
 * altrimenti ripete lo scan.
 */
void executeMonitorMode() {
  // Pulsante fisico: short = ciclo schermate, long = clear DTC.
  ButtonEvent ev = buttonPoll();
  if (ev == BTN_SHORT && totalScreens > 1) {
    currentScreen = (currentScreen + 1) % totalScreens;
    lastScreenSwitch = millis();   // posticipa l'auto-cycle
    labelsDrawn = false;           // forza redraw completo al prossimo update
  } else if (ev == BTN_LONG) {
    drawStatusScreen("CLEAR DTC", "Invio Mode 04...");
    char message[64];
    uint8_t result = obdLiveClearDtcs(message, sizeof(message));
    drawStatusScreen("CLEAR DTC", obdClearResultShortText(result));
    delay(1200);
    labelsDrawn = false;
    lastDrawnDtcScreen = 0xFF;
  }

  if (obdLiveDtcCheckDue()) {
    obdLiveCheckDtcs();
    updateScreenCount();
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

  obdLiveStep();
  if (obdLiveSilentStreak >= SILENT_CYCLES_BEFORE_RECOVERY) {
    obdLiveSilentStreak = 0;
    if (CAN.getError() & 0x20) {
      Serial.println(F("CAN bus-off, re-init MCP2515"));
      canInit();
    } else {
      Serial.println(F("Il motore non risponde, nuovo scan"));
      currentMode = MODE_SCAN;
      return;
    }
  }

  if (currentScreen > 0) {
    // Pagina DTC: ridisegno solo al cambio pagina o dei codici; al ritorno ai dati ridisegno completo
    if (currentScreen != lastDrawnDtcScreen) {
      drawDTCScreen(currentScreen - 1);
      lastDrawnDtcScreen = currentScreen;
    }
    labelsDrawn = false;
  } else {
    lastDrawnDtcScreen = 0xFF;
    updateDisplay();
  }

  if (debugMode && millis() - lastDiagPrint > DIAG_PRINT_INTERVAL_MS) {
    obdDiagRefresh(1000);
    obdPrintDiagnostics(obdDiag);
    lastDiagPrint = millis();
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
 */
void clearValueArea(int x, int y, int w, int h) {
  u8g2.setDrawColor(0);
  u8g2.drawBox(x, y, w, h);
  u8g2.setDrawColor(1);
}

/**
 * Disegna il valore boost con 2 cifre decimali.
 * Colonna sinistra, y=15. Font deve essere gia' impostato.
 * @see updateDisplay()
 */
void drawBoostValue() {
  char val[14];
  if (boostAvailable) {
    int bc = (int)(boostBar * 100.0f);
    char s = (bc >= 0) ? '+' : '-';
    int a = abs(bc);
    snprintf(val, sizeof(val), "%c%d.%02d Bar", s, a / 100, a % 100);
  } else {
    snprintf(val, sizeof(val), "N/D");
  }
  u8g2.drawStr(0, 15, val);
}

/**
 * Disegna la coppia motore in Nm, letta dalla centralina o stimata; in rilascio può essere negativa.
 * Colonna sinistra, y=50. Font deve essere già impostato.
 * @see updateDisplay()
 */
void drawTorqueValue() {
  char val[14];
  if (torqueAvailable) {
    snprintf(val, sizeof(val), "%d Nm", torqueNm);
  } else {
    snprintf(val, sizeof(val), "N/D");
  }
  u8g2.drawStr(0, 50, val);
}

/**
 * Disegna il valore temperatura liquido raffreddamento.
 * Colonna destra riga 1 (RIGHT_COL_X), y=15. Font deve essere gia' impostato.
 * @see updateDisplay()
 */
void drawCoolantValue() {
  char val[14];
  if (coolantAvailable) {
    snprintf(val, sizeof(val), "%d C", coolantC);
  } else {
    snprintf(val, sizeof(val), "N/D");
  }
  // Valori larghi (es. "-40 C") traslano a sinistra per toccare il bordo destro
  int w = u8g2.getStrWidth(val);
  int x = (w > (128 - RIGHT_COL_X)) ? (128 - w) : RIGHT_COL_X;
  u8g2.drawStr(x, 15, val);
}

/**
 * Disegna la carica della batteria ibrida (PID 0x5B) nella colonna destra della riga 2, y=50:
 * "100%" occupa esattamente i 28 px fra RIGHT_COL_X e il bordo.
 * @see updateDisplay()
 */
void drawHybBattValue() {
  char val[14];
  if (hybBattAvailable) {
    snprintf(val, sizeof(val), "%d%%", hybBattPct);
  } else {
    snprintf(val, sizeof(val), "N/D");
  }
  u8g2.drawStr(RIGHT_COL_X, 50, val);
}

/**
 * Aggiorna il display OLED con i valori correnti.
 * In landscape usa aggiornamento parziale: ridisegna solo le tile modificate
 * invece dell'intero framebuffer, riducendo il trasferimento I2C da ~1024 a ~256-384 byte.
 * Le etichette statiche (BOOST, TEMP, COPPIA, BATT) vengono disegnate una sola volta.
 *
 * Tile layout landscape (font 7x14B, 14px alto):
 *   Etichette riga 1 (y=0):  pixel 0-13  -> tile rows 0-1
 *   Valori riga 1 (y=15):    pixel 15-28 -> tile rows 1-3
 *   Etichette riga 2 (y=35): pixel 35-48 -> tile rows 4-6
 *   Valori riga 2 (y=50):    pixel 50-63 -> tile rows 6-7
 */
void updateDisplay() {
  // --- LANDSCAPE (128x64) con aggiornamento parziale ---
  u8g2.setFont(u8g2_font_7x14B_tr);

  int curBoostInt = boostAvailable ? (int)(boostBar * 100.0f) : -99999;
  int curTorqueNm = torqueAvailable ? torqueNm : -99999;
  int curCoolantC = coolantAvailable ? coolantC : -99999;
  int curHybBattPct = hybBattAvailable ? hybBattPct : -99999;

  if (!labelsDrawn) {
    // --- RIDISEGNO COMPLETO: etichette + valori + sendBuffer ---
    u8g2.clearBuffer();

    // Etichette statiche
    u8g2.drawStr(0, 0, "BOOST");
    u8g2.drawStr(RIGHT_COL_X, 0, "TEMP");
    u8g2.drawStr(0, 35, "COPPIA");
    u8g2.drawStr(RIGHT_COL_X, 35, "BATT");

    // Valori iniziali
    drawBoostValue();
    drawTorqueValue();
    drawCoolantValue();
    drawHybBattValue();

    u8g2.sendBuffer();
    labelsDrawn = true;

    // Salva valori correnti per dirty tracking (sentinel -99999 = non disponibile)
    prevBoostInt = curBoostInt;
    prevTorqueNm = curTorqueNm;
    prevCoolantC = curCoolantC;
    prevHybBattPct = curHybBattPct;
    return;
  }

  // --- AGGIORNAMENTO PARZIALE: solo valori cambiati ---
  bool row1dirty = false;
  bool row2dirty = false;

  // Boost (colonna sinistra, riga 1)
  if (curBoostInt != prevBoostInt) {
    clearValueArea(0, 15, TEMP_EXTENDED_X, 14);
    drawBoostValue();
    prevBoostInt = curBoostInt;
    row1dirty = true;
  }

  // Temperatura liquido (colonna destra, riga 1 — clear estesa per valori larghi)
  if (curCoolantC != prevCoolantC) {
    clearValueArea(TEMP_EXTENDED_X, 15, 128 - TEMP_EXTENDED_X, 14);
    drawCoolantValue();
    prevCoolantC = curCoolantC;
    row1dirty = true;
  }

  // Coppia (colonna sinistra, riga 2)
  if (curTorqueNm != prevTorqueNm) {
    clearValueArea(0, 50, RIGHT_COL_X, 14);
    drawTorqueValue();
    prevTorqueNm = curTorqueNm;
    row2dirty = true;
  }

  // Batteria ibrida (colonna destra, riga 2)
  if (curHybBattPct != prevHybBattPct) {
    clearValueArea(RIGHT_COL_X, 50, 128 - RIGHT_COL_X, 14);
    drawHybBattValue();
    prevHybBattPct = curHybBattPct;
    row2dirty = true;
  }

  // Invia solo le tile rows modificate (8 pixel per tile row)
  if (row1dirty) {
    u8g2.updateDisplayArea(0, 1, 16, 3);  // Tile rows 1-3 (pixel 8-31)
  }
  if (row2dirty) {
    u8g2.updateDisplayArea(0, 6, 16, 2);  // Tile rows 6-7 (pixel 48-63)
  }
}

/**
 * Disegna una pagina di codici errore DTC, con la centralina che ha memorizzato ciascun codice.
 * @param page indice pagina (0 = prima pagina DTC, 1 = seconda, etc.)
 */
void drawDTCScreen(uint8_t page) {
  u8g2.clearBuffer();

  uint8_t perPage = DTC_PER_PAGE;
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
    char owner[16];
    char line[24];
    decodeDTC(dtcCodes[startIdx + i], codeStr);
    obdEcuShortName(dtcEcuIndex[startIdx + i], owner, sizeof(owner));
    snprintf(line, sizeof(line), "%s %s", codeStr, owner);
    u8g2.drawStr(0, yBase, line);

    const char* desc = getDTCDescription(dtcCodes[startIdx + i]);
    if (desc != NULL) {
      char descBuf[22];
      strncpy_P(descBuf, desc, sizeof(descBuf) - 1);
      descBuf[sizeof(descBuf) - 1] = '\0';
      u8g2.drawStr(0, yBase + 11, descBuf);
    }
  }

  u8g2.sendBuffer();
}

// ============================================================
// UTILITA'
// ============================================================

/**
 * Disegna una stringa centrata orizzontalmente nel display.
 * Se la stringa è piu' larga del display, viene allineata a sinistra (x=0).
 * Il font deve essere gia' impostato prima della chiamata.
 * @since 31/03/26 Mattia Alesi
 */
void drawStrCentered(int y, const char* str) {
  int x = (128 - (int)u8g2.getStrWidth(str)) / 2;
  u8g2.drawStr(x > 0 ? x : 0, y, str);
}

/** Disegna la schermata di scansione PID con stato e dettaglio */
void drawScanScreen(const char* status, const char* detail) {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tr);
  drawStrCentered(11, "SCANSIONE PID");
  drawStrCentered(27, status);
  drawStrCentered(43, detail);
  u8g2.sendBuffer();
}

/** Schermata di stato generica: titolo in alto, messaggio centrato. */
void drawStatusScreen(const char* title, const char* msg) {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tr);
  drawStrCentered(11, title);
  drawStrCentered(32, msg);
  u8g2.sendBuffer();
}

/**
 * Mostra un messaggio di errore su OLED e Serial.
 * Usata per errori fatali (CAN init, ECU non risponde).
 */
void showError(const char* line1, const char* line2) {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tr);
  drawStrCentered(11, "ERRORE:");
  drawStrCentered(27, line1);
  drawStrCentered(43, line2);
  u8g2.sendBuffer();
  Serial.print(F("ERRORE: "));
  Serial.println(line1);
  Serial.println(line2);
}
