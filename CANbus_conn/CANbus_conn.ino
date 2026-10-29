/**
 * OBD2 Monitor per ESP8266 D1 Mini
 *
 * Mostra in tempo reale su display OLED SH1106 128x64:
 * - Pressione boost (bar), letta dalla centralina (PID 0x70) o stimata da BoostModel
 * - Coppia motore all'albero (Nm), letta dalla centralina (PID 0x62, 0x8E, 0x63) o stimata da TorqueModel
 * - Temperatura liquido raffreddamento (C)
 * - Carica batteria ibrida (%, PID 0x5B se la vettura lo espone)
 *
 * All'avvio va subito al monitor con il profilo vettura di obd_live.h, senza discovery; la scansione
 * completa di centraline OBD e UDS si avvia solo dalla pagina web /scan (report su Serial e riepilogo
 * nella pagina) e si interrompe con il pulsante.
 *
 * Hardware:
 *   ESP8266 D1 Mini + MCP2515/TJA1050 CAN (modulo integrato, SPI) + OLED SH1106 1.3" (I2C)
 *
 * Cablaggio:
 *   MCP2515: VCC->5V, GND->GND, CS->D8, SO->D6, SI->D7, SCK->D5, INT->D0
 *   OLED:    VCC->3.3V, GND->GND, SCL->D1, SDA->D2
 *   OBD2:    CANH->Pin6, CANL->Pin14, GND->Pin4/5
 *   Pulsante: NA tra D3 (GPIO0) e GND. Click breve = scorri schermate, oppure interrompe la
 *             scansione completa; pressione lunga (>=2.5s) = cancella DTC (OBD2 Mode 04), ignorata
 *             a fine scansione finchè il pulsante non è rilasciato e per altri 5 s.
 *   LDR (opzionale): 3.3V -> LDR -> A0 -> 10k -> GND; senza, contrasto fisso a defaultBrightness.
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
// Cicli consecutivi senza risposta dal motore prima del ripristino (re-init in bus-off, altrimenti
// schermata "Motore non risponde" e nuovo tentativo dopo ENGINE_SILENT_RETRY_MS)
#define SILENT_CYCLES_BEFORE_RECOVERY 10
#define ENGINE_SILENT_RETRY_MS 10000

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

// Fine dell'attesa dopo "Motore non risponde": fino ad allora il monitor non interroga il motore
unsigned long engineRetryAt = 0;

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

// Scansione completa e lettura OBD condivise con la build WiFi; il trasporto è obdTransact() più sotto
#include "obd_proto.h"
#include "obd_scan.h"
#include "obd_live.h"

// Dashboard web live (endpoint /dashboard, /data, /scan, /probe, /debug)
#define OBD_CONN_CAN
#include "web_dashboard.h"

// ============================================================
// SETUP
// ============================================================

/**
 * Avvia seriale, SoftAP con dashboard, display e MCP2515, poi prepara la lettura live con il profilo vettura:
 * al boot non c'è nessuna scansione.
 * @since 29/10/26 Mattia Alesi
 */
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

  obdLiveConfigure();
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
    // Durante la scansione completa il SoftAP resta acceso: il report si legge solo dal web
    if (!clientNow && millis() > otaDeadline && !obdProbeRunning()) {
      httpServer.stop();
      WiFi.softAPdisconnect(true);
      WiFi.mode(WIFI_OFF);
      otaActive = false;
      Serial.println(F("OTA: finestra chiusa, WiFi spento"));
    }
  }
  if (obdProbeRunning()) {
    executeProbeStep();
  } else {
    executeMonitorMode();
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
 * Invia un frame con DLC 8, come richiede ISO 15765-4, a 29 bit se l'ID porta OBD_ID_EXTENDED. Dopo tre invii
 * falliti di fila (buffer di trasmissione bloccati con TXREQ attivo) reinizializza il MCP2515.
 */
bool canSendFrame(uint32_t id, const uint8_t* frame) {
  uint8_t data[8];
  memcpy(data, frame, 8);
  const uint8_t extended = (id & OBD_ID_EXTENDED) ? 1 : 0;
  if (CAN.sendMsgBuf(id & 0x1FFFFFFFUL, extended, 8, data) == CAN_OK) {
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
 * Trasporto su CAN nativo: invia la richiesta single frame e riassembla le risposte accettate dal modo di
 * ricezione. Al first frame invia il flow control all'ID dato da obdFlowControlId() (risposta - 8 in OBD, ID di
 * richiesta per le coppie UDS VAG) ed estende l'attesa di N_Cr; una risposta 7F xx 78 rinnova l'attesa a P2*CAN,
 * mai oltre OBD_RESPONSE_PENDING_CAP_MS dall'invio. Legge i frame a raffica e cede la CPU solo a coda vuota,
 * perchè il MCP2515 ha due buffer.
 * @see obd_scan.h obdTransact()
 */
uint8_t obdTransact(uint32_t requestId, const uint8_t* request, uint8_t requestLen, uint16_t windowMs,
                    uint32_t waitForEcuId, uint8_t receiveMode) {
  obdCollectorReset(obdCollector);
  obdTransportLastStatus = OBD_TRANSPORT_OK;
  if (requestLen == 0 || requestLen > 7) return 0;
  flushCANBuffer();
  uint8_t frame[8] = { 0 };
  frame[0] = requestLen;
  memcpy(frame + 1, request, requestLen);
  if (!canSendFrame(requestId, frame)) {
    obdTransportLastStatus = OBD_TRANSPORT_ERROR;
    return 0;
  }

  const bool firstMessageEnds = waitForEcuId == 0 && (receiveMode == OBD_RX_UDS11 || receiveMode == OBD_RX_UDS29);
  const unsigned long pendingCap = millis() + OBD_RESPONSE_PENDING_CAP_MS;
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
    // Il bit 30 è il flag RTR di mcp_can: un remote frame non porta dati diagnostici
    if ((rxId & 0x40000000UL) || !obdAcceptsResponse(receiveMode, requestId, rxId)) continue;
    ObdIsoTpRx* slot = obdCollectorSlot(obdCollector, rxId);
    if (!slot) continue;

    if (obdIsoTpFeed(*slot, buf, len)) {
      uint8_t flowControl[8] = { 0x30, 0x00, obdFlowControlSeparationMs(receiveMode), 0, 0, 0, 0, 0 };
      canSendFrame(obdFlowControlId(receiveMode, requestId, rxId), flowControl);
    }
    if (slot->state == OBD_ISOTP_RECEIVING) {
      unsigned long frameDeadline = millis() + ISOTP_CONSECUTIVE_TIMEOUT_MS;
      if ((long)(frameDeadline - deadline) > 0) deadline = frameDeadline;
      continue;
    }
    if (slot->state != OBD_ISOTP_COMPLETE) continue;

    uint8_t service, nrc;
    if (obdIsNegativeResponse(slot->data, slot->receivedLen, &service, &nrc) && nrc == OBD_NRC_RESPONSE_PENDING) {
      obdIsoTpReset(*slot, rxId);
      unsigned long pendingDeadline = millis() + RESPONSE_PENDING_TIMEOUT_MS;
      deadline = (long)(pendingDeadline - pendingCap) > 0 ? pendingCap : pendingDeadline;
      continue;
    }
    if (firstMessageEnds || (waitForEcuId != 0 && rxId == waitForEcuId)) break;
  }
  return obdCollectorCompleteCount(obdCollector);
}

/**
 * Discovery a raffica sul CAN nativo, come il tool dtcsweep: 3E 00 a ogni ID ogni 10 ms, poi ascolto per 1 s, con
 * invio e ricezione intercalati nello stesso ciclo. Fra due richieste arriva al più una risposta, quindi cedere la
 * CPU a coda vuota non riempie i due buffer del MCP2515; se succede comunque (flag di overflow in EFLG) o un invio
 * fallisce, ritorna OBD_BURST_FAILED senza reinizializzare il chip e la scansione procede in sequenza.
 * @see obd_scan.h obdTransportDiscoveryBurst()
 */
uint8_t obdTransportDiscoveryBurst(const uint32_t* requestIds, uint16_t count, uint32_t* responders,
                                   uint8_t maxResponders) {
  if (count == 0) return 0;
  const uint8_t receiveMode = (requestIds[0] & OBD_ID_EXTENDED) ? OBD_RX_UDS29 : OBD_RX_UDS11;
  const uint8_t testerPresent[8] = { 0x02, 0x3E, 0x00, 0, 0, 0, 0, 0 };
  // RX0OVR e RX1OVR restano alzati finchè non si azzerano: un overflow vecchio farebbe fallire la raffica
  if (CAN.getError() & 0xC0) canInit();
  flushCANBuffer();
  uint8_t found = 0;
  uint16_t next = 0;
  unsigned long nextSendMs = millis();
  unsigned long listenUntil = 0;
  while (true) {
    if (next < count && (long)(millis() - nextSendMs) >= 0) {
      uint8_t data[8];
      memcpy(data, testerPresent, 8);
      const uint32_t id = requestIds[next];
      if (CAN.sendMsgBuf(id & 0x1FFFFFFFUL, (id & OBD_ID_EXTENDED) ? 1 : 0, 8, data) != CAN_OK) return OBD_BURST_FAILED;
      next++;
      nextSendMs += 10;
      if (next == count) listenUntil = millis() + 1000;
    }
    if (next == count && (long)(millis() - listenUntil) >= 0) break;
    // Un frame perso renderebbe incompleto l'elenco: si ripiega in sequenza con i flag azzerati
    if (CAN.getError() & 0xC0) {
      canInit();
      return OBD_BURST_FAILED;
    }
    if (CAN.checkReceive() != CAN_MSGAVAIL) {
      yield();
      continue;
    }
    unsigned long rxId;
    uint8_t len;
    uint8_t buf[8];
    CAN.readMsgBuf(&rxId, &len, buf);
    if ((rxId & 0x40000000UL) || len < 3 || !obdAcceptsResponse(receiveMode, 0, rxId)) continue;
    const bool positive = (buf[0] & 0xF0) == 0 && buf[1] == 0x7E && buf[2] == 0x00;
    const bool negative = (buf[0] & 0xF0) == 0 && buf[1] == 0x7F && buf[2] == 0x3E;
    const bool firstFrame = (buf[0] & 0xF0) == 0x10;
    if (!positive && !negative && !firstFrame) continue;
    bool known = false;
    for (uint8_t i = 0; i < found; i++) known = known || responders[i] == rxId;
    if (!known && found < maxResponders) responders[found++] = rxId;
  }
  return found;
}

/**
 * Riporta il CAN nativo allo stato della lettura live dopo la scansione: non c'è configurazione da annullare, restano
 * solo i flag di overflow che un'eventuale raffica può aver lasciato alzati.
 * @see obd_scan.h obdTransportRestore()
 */
void obdTransportRestore() {
  if (CAN.getError() & 0xC0) canInit();
  flushCANBuffer();
}

// ============================================================
// SCANSIONE COMPLETA
// ============================================================

/**
 * Esegue un passo della scansione completa avviata dal web e ne mostra fase e avanzamento sul display; una
 * pressione del pulsante la interrompe. Al termine blocca la pressione lunga, che altrimenti cancellerebbe i DTC
 * a chi tiene premuto, fa ripartire la finestra OTA e fa riprendere il monitor con un ridisegno completo.
 */
void executeProbeStep() {
  static unsigned long lastDraw = 0;
  ButtonEvent ev = buttonPoll();
  if (ev == BTN_SHORT || ev == BTN_LONG) obdProbeStop();
  obdProbeStep();
  bool running = obdProbeRunning();
  if (!running || millis() - lastDraw > 500) {
    char phase[22];
    char line[22];
    strncpy_P(phase, obdProbePhaseName(), sizeof(phase) - 1);
    phase[sizeof(phase) - 1] = '\0';
    snprintf(line, sizeof(line), "%u%% - click: ferma", (unsigned)obdProbePercent());
    drawScanScreen(phase, running ? line : "Report su /scan");
    lastDraw = millis();
  }
  if (!running) {
    buttonBlockLongPress();
    otaDeadline = millis() + OTA_WINDOW_MS;
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
 * altrimenti mostra "Motore non risponde" e riprova dopo ENGINE_SILENT_RETRY_MS.
 */
void executeMonitorMode() {
  if ((long)(millis() - engineRetryAt) < 0) {
    yield();
    return;
  }
  // Pulsante fisico: short = ciclo schermate, long = clear DTC.
  ButtonEvent ev = buttonPoll();
  if (ev == BTN_SHORT && totalScreens > 1) {
    currentScreen = (currentScreen + 1) % totalScreens;
    lastScreenSwitch = millis();   // posticipa l'auto-cycle
    labelsDrawn = false;           // forza redraw completo al prossimo update
  } else if (ev == BTN_LONG && buttonLongPressAllowed()) {
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
      Serial.println(F("Il motore non risponde, nuovo tentativo tra 10 s"));
      showError("Motore non risponde", "Quadro acceso?");
      engineRetryAt = millis() + ENGINE_SILENT_RETRY_MS;
      labelsDrawn = false;
      lastDrawnDtcScreen = 0xFF;
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
    // Il limite a 99.99 bar supera ogni valore decodificabile da 0x70 e tiene la stringa nel buffer e nella colonna
    unsigned a = (unsigned)abs(bc);
    if (a > 9999) a = 9999;
    snprintf(val, sizeof(val), "%c%u.%02u Bar", s, a / 100, a % 100);
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
  // Spazio per i massimi dei contatori uint8_t, non solo per i 12 DTC di OBD_MAX_DTC
  char title[25];
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
    obdEcuShortName(dtcEcuId[startIdx + i], owner, sizeof(owner));
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

/** Disegna la schermata della scansione completa con fase e avanzamento */
void drawScanScreen(const char* status, const char* detail) {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tr);
  drawStrCentered(11, "SCANSIONE COMPLETA");
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
