/**
 * OBD2 WiFi Monitor per ESP8266 LOLIN D1 Mini ed ESP-01
 *
 * Mostra in tempo reale su display OLED SH1106 128x64:
 * - Pressione boost (bar), letta dalla centralina (PID 0x70) o stimata da BoostModel
 * - Coppia motore all'albero (Nm), letta dalla centralina (PID 0x62, 0x8E, 0x63) o stimata da TorqueModel
 * - Temperatura liquido raffreddamento (C)
 * - Carica batteria ibrida (%, PID 0x5B se la vettura lo espone)
 *
 * Comunica con un adattatore ELM327 WiFi tramite comandi AT su TCP, con gli header CAN attivi
 * per distinguere le risposte delle diverse centraline. Dopo la connessione va subito al monitor con il
 * profilo vettura di obd_live.h, senza discovery; la scansione completa di centraline OBD e UDS si avvia
 * solo dalla pagina web /scan (report su Serial e riepilogo nella pagina) e si interrompe con il pulsante.
 *
 * Hardware:
 *   ESP8266 LOLIN D1 Mini oppure ESP-01 (board "Generic ESP8266 Module", 1 MB) + OLED SH1106 1.3" (I2C)
 *   Adattatore ELM327 WiFi (inserito nella presa OBD2 del veicolo)
 *   Il profilo hardware si sceglie da solo in base alla board selezionata nell'IDE.
 *
 * Cablaggio:
 *   OLED VCC -> 3.3V, GND -> GND, SDA -> GPIO0, SCL -> GPIO2 (uguale sulle due board)
 *   Pulsante: NA verso GND, su D5 (GPIO14) nel D1 Mini e su GPIO3 (RX) nell'ESP-01, dove una resistenza
 *             da 1 kΩ in serie protegge l'adattatore USB-seriale se il pulsante è premuto durante il flash.
 *             Click breve = scorri schermate, oppure interrompe la scansione completa; pressione lunga
 *             (>=2.5s) = cancella DTC (OBD2 Mode 04 via ELM327), ignorata a fine scansione finchè il pulsante
 *             non è rilasciato e per altri 5 s.
 *   LDR (solo D1 Mini, opzionale): 3.3V -> LDR -> A0 -> 10k -> GND; senza, contrasto fisso a defaultBrightness.
 *
 * Acquisizione: una richiesta multi-PID al motore per ciclo, con ripiego a un PID per ciclo se la
 * centralina o l'adattatore non accettano richieste multiple; aggiornamento parziale del display.
 *
 * Veicolo: Audi A5 F5 (B9.5) 40 TFSI 150 kW mild hybrid, motore DMSA, cambio S tronic 0CK
 *
 * @since 2026-03-19 mattia.Alesi
 */

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPUpdateServer.h>
#include <U8g2lib.h>
#include <Wire.h>
#include "dtc_descriptions.h"
#include "TorqueModel.h"
#include "BoostModel.h"
// "Generic ESP8266 Module" è il profilo ESP-01: liberi solo GPIO0/GPIO2 (I2C OLED) e RX/TX, nessun ADC,
// quindi pulsante su RX con la seriale in sola trasmissione e niente LDR.
#if defined(ARDUINO_ESP8266_GENERIC)
  #define WIFI_BOARD_ESP01
  #define BUTTON_PIN 3
  #define LDR_ENABLED 0
#else
  // D1 Mini: D3/D4 sono l'I2C dell'OLED; D5 ha il pull-up interno e nessun ruolo al boot
  #define BUTTON_PIN D5
#endif
#include "button_handler.h"
#include "light_sensor.h"
#include "pid_descriptions.h"  // getPIDFullName() per stampa descrizioni SAE

// ============================================================
// CONFIGURAZIONE — Modificare qui se necessario
// ============================================================

// Rete WiFi dell'adattatore ELM327 (varia per modello)
#define ELM327_SSID "WiFi_OBDII"
#define ELM327_PASS ""  // Vuoto se rete aperta

// Indirizzo IP e porta TCP dell'ELM327
#define ELM327_IP "192.168.0.10"
#define ELM327_PORT 35000

// Timeout di lettura della risposta ELM327 (ms): l'adattatore risponde NO DATA dopo circa 200 ms
#define ELM327_TIMEOUT 2000
// Timeout più lungo per la prima richiesta OBD dopo l'inizializzazione
#define ELM327_FIRST_TIMEOUT 5000

// Protocollo OBD2: "ATSP6" = CAN 11bit 500kbps (gruppo VAG), "ATSP0" = ricerca automatica
#define ELM327_PROTOCOL "ATSP6"

// Tentativi massimi di connessione
#define MAX_RETRIES 3
// Cicli consecutivi senza risposta dal motore prima della schermata "Motore non risponde" e della
// riconnessione all'ELM327, che parte dopo ENGINE_SILENT_RETRY_MS
#define SILENT_CYCLES_BEFORE_RECOVERY 10
#define ENGINE_SILENT_RETRY_MS 10000

// OTA — Aggiornamento firmware via browser (http://192.168.4.1/ota)
#define OTA_SSID "OBD2_UPDATE"  // SSID del SoftAP per OTA
#define OTA_PASS "alesimattia"     // Password WPA2 (min 8 chars)
#define OTA_PORT 80                // Porta web server
#define OTA_WINDOW_MS 180000       // Finestra OTA: 3 minuti dal boot

// Diagnostica avanzata su Serial: attivabile runtime da /dashboard
bool debugMode = false;
#define DIAG_PRINT_INTERVAL_MS 5000  // Intervallo minimo fra due stampe della diagnostica

// Layout display landscape 128x64 (font 7x14B, 7px wide)
// Colonna DX: "100%" = 4 chars × 7px = 28px → 128-28 = 100
// I valori della temperatura più larghi traslano a sinistra dinamicamente.
#define RIGHT_COL_X 100
#define TEMP_EXTENDED_X 93    // Min x TEMP: "-40 C"/"215 C" = 35px → 128-35

// DTC (Diagnostic Trouble Codes)
#define DTC_PER_PAGE 2            // DTC per pagina display
#define SCREEN_SWITCH_MS 5000     // Alternanza schermate dati/errori (ms)

// ============================================================
// Oggetti globali
// ============================================================

WiFiClient elmClient;
ESP8266WebServer httpServer(OTA_PORT);
ESP8266HTTPUpdateServer httpUpdater;

// Display OLED SH1106 128x64, I2C hardware, landscape
U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);

// ============================================================
// Stato applicazione
// ============================================================

enum AppMode { MODE_CONNECT,
               MODE_MONITOR };
AppMode currentMode = MODE_CONNECT;
// Fine dell'attesa dopo "Motore non risponde": fino ad allora la riconnessione non parte
unsigned long engineRetryAt = 0;

unsigned long lastScreenSwitch = 0;
// Schermata corrente: 0 = dati, 1 = pagina DTC 1, 2 = pagina DTC 2, etc.
uint8_t currentScreen = 0;
uint8_t totalScreens = 1;
// Ultima pagina DTC disegnata: la pagina si ridisegna solo quando cambia (0xFF = forza ridisegno)
uint8_t lastDrawnDtcScreen = 0xFF;

// Stato dell'adattatore ELM327
uint32_t elmCurrentHeader = 0x7DF;  // header CAN impostato con ATSH (0x7DF = funzionale, default), 0 = da reimpostare
bool elmCountSuffixOk = false;      // accetta il numero di risposte attese dopo la richiesta (v1.3+)
uint32_t elmFlowControlHeader = 0;      // ID impostato con ATFCSH, 0 = nessuno
bool elmFlowControlUserMode = false;    // ATFCSM1 attivo
bool elmLongTimeout = false;            // ATSTFF attivo: adattatore che non gestisce da sè i 7F xx 78
unsigned long elmNextTimeout = ELM327_FIRST_TIMEOUT;
char elmResponse[1024];             // risposta dell'ultimo comando, righe separate da '\n'

// OTA: true finche' la finestra di aggiornamento è attiva
bool otaActive = true;
unsigned long otaDeadline = OTA_WINDOW_MS;  // Deadline OTA (resettata se client si disconnette)
bool otaClientWasConnected = false;         // Traccia se un client era connesso

// Dirty tracking: valori precedenti per aggiornamento parziale display
int prevBoostInt = -99999;  // (int)(boostBar * 100) per confronto senza errori float
int prevTorqueNm = -99999;
int prevCoolantC = -99999;
int prevHybBattPct = -99999;
bool labelsDrawn = false;  // Etichette statiche gia' disegnate nel buffer
unsigned long lastDiagPrint = 0;

// Logger seriale: cattura Serial.* nel buffer circolare leggibile via /serial-data.
// Va incluso DOPO tutti gli header di sistema (Arduino.h ecc.) per non
// rompere "extern HardwareSerial Serial" nelle loro dichiarazioni.
#include "serial_logger.h"

// Scansione completa e lettura OBD condivise con la build CAN; il trasporto è obdTransact() più sotto
#include "obd_proto.h"
#include "obd_scan.h"
#include "obd_live.h"

// Modo di ricezione configurato sull'ELM327 (protocollo, filtri, flow control): qui perchè usa le costanti OBD_RX_*;
// OBD_RX_UNSET dopo un filtro rifiutato, così la richiesta successiva riapplica tutto
static constexpr uint8_t OBD_RX_UNSET = 0xFF;
uint8_t elmReceiveMode = OBD_RX_OBD11;
// Protocollo attivo sull'adattatore, a 29 bit dopo ATSP7: guida il cambio di protocollo e il parser delle righe
bool elmExtendedProtocol = false;

#define OBD_CONN_WIFI
#include "web_dashboard.h"

// ============================================================
// SETUP
// ============================================================

/**
 * Avvia seriale, display e web server; connessione all'ELM327 e lettura live partono dal loop.
 * @since 29/10/26 Mattia Alesi
 */
void setup() {
#ifdef WIFI_BOARD_ESP01
  // RX resta libero per il pulsante: il firmware non legge mai dalla seriale
  Serial.begin(115200, SERIAL_8N1, SERIAL_TX_ONLY);
#else
  Serial.begin(115200);
#endif
  Serial.println(F("\n=== OBD2 WiFi Monitor ==="));
  Serial.println(F("Audi A5 F5 (B9.5) 40 TFSI DMSA S tronic"));

  // I2C su pin ESP01
  Wire.begin(0, 2);
  Wire.setClock(400000);

  // Inizializza display OLED (400kHz I2C esplicito anche via u8g2)
  u8g2.setBusClock(400000);
  u8g2.begin();
  u8g2.setFontPosTop();
  initBrightness();  // legge LDR e applica contrasto iniziale

  // OTA web server (attivo anche prima della connessione ELM327)
  httpUpdater.setup(&httpServer, "/ota");
  setupWebDashboard(httpServer);
  httpServer.begin();
  Serial.println(F("OTA pronto: http://192.168.4.1/ota"));

  // Splash screen — centrato verticalmente
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tr);
  u8g2.drawStr(25, 11, "OBD2 WIFI MON");
  u8g2.drawStr(19, 27, "A5 B9.5 40 TFSI");
  u8g2.drawStr(16, 54, "OTA: 192.168.4.1");
  u8g2.sendBuffer();
  delay(2000);

  // Pulsante fisico (clear DTC + cycle schermate)
  buttonInit();

  // Fino a fine executeConnectMode() nè la dashboard nè la scansione devono interrogare l'adattatore
  obdTransportReady = false;
  currentMode = MODE_CONNECT;
}

// ============================================================
// LOOP
// ============================================================

void loop() {
  updateBrightness();  // sample LDR + state machine auto-brightness (non bloccante)

  // OTA: attivo finche' otaDeadline non scade E nessun client è connesso.
  // Se un client si connette, il timeout si sospende.
  // Quando il client si disconnette, il timeout riparte da OTA_WINDOW_MS.
  if (otaActive) {
    httpServer.handleClient();
    bool clientNow = (WiFi.softAPgetStationNum() > 0);
    if (clientNow) {
      otaClientWasConnected = true;
    } else if (otaClientWasConnected) {
      // Client appena disconnesso: reset deadline
      otaDeadline = millis() + OTA_WINDOW_MS;
      otaClientWasConnected = false;
      Serial.println(F("OTA: client disconnesso, timeout ripartito"));
    }
    // Durante la scansione completa il SoftAP resta acceso: su ESP-01 il report si legge solo dal web
    if (!clientNow && millis() > otaDeadline && !obdProbeRunning()) {
      httpServer.stop();
      WiFi.softAPdisconnect(true);
      WiFi.mode(WIFI_STA);
      otaActive = false;
      Serial.println(F("OTA: finestra chiusa, SoftAP spento"));
    }
  }
  switch (currentMode) {
    case MODE_CONNECT:
      executeConnectMode();
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
// COMUNICAZIONE ELM327
// ============================================================

/** Attende ms millisecondi continuando a servire il web server (OTA e dashboard). */
void waitServingHttp(unsigned long ms) {
  unsigned long until = millis() + ms;
  while ((long)(millis() - until) < 0) {
    if (otaActive) httpServer.handleClient();
    yield();
  }
}

/** Cifre dell'ID nelle righe con header per il protocollo configurato: 8 a 29 bit, 3 a 11 bit. */
uint8_t elmIdDigits() {
  return elmExtendedProtocol ? 8 : 3;
}

/** Vero se la riga con header è una risposta 7F xx 78: la centralina chiede altro tempo prima della risposta. */
bool elmIsPendingLine(const char* line) {
  uint32_t id;
  uint8_t frame[8];
  uint8_t frameLen;
  return obdElmParseHeaderLine(line, &id, frame, &frameLen, elmIdDigits()) && frameLen >= 4 && frame[0] == 0x03 &&
         frame[1] == 0x7F && frame[3] == OBD_NRC_RESPONSE_PENDING;
}

/**
 * Invia un comando all'ELM327 e legge la risposta fino al prompt '>' nel buffer statico elmResponse,
 * con le righe separate da '\n': servono a distinguere le centraline quando gli header sono attivi.
 * Una riga 7F xx 78 prolunga l'attesa di 5 s, come fa l'ELM327 dalla v2.1, mai oltre OBD_RESPONSE_PENDING_CAP_MS.
 * Senza prompt continua a scartare dati per 1 s, perchè un comando inviato mentre l'adattatore lavora ancora
 * interromperebbe la richiesta in corso; poi lo interrompe con uno spazio, che a riposo l'ELM ignora (un CR
 * ripeterebbe l'ultimo comando), e attende STOPPED e il prompt.
 * @since 23/10/26 Mattia Alesi
 */
const char* elmCommand(const char* cmd, unsigned long timeout) {
  elmResponse[0] = '\0';
  if (!elmClient.connected()) return elmResponse;

  // Svuota eventuale dati residui nel buffer TCP
  while (elmClient.available()) {
    elmClient.read();
    yield();
  }
  elmClient.print(cmd);
  elmClient.print('\r');

  size_t len = 0;
  size_t lineStart = 0;
  bool atLineStart = true;
  bool prompt = false;
  const unsigned long start = millis();
  const unsigned long pendingCap = start + OBD_RESPONSE_PENDING_CAP_MS;
  unsigned long deadline = start + timeout;
  while ((long)(millis() - deadline) < 0) {
    if (!elmClient.available()) {
      yield();
      continue;
    }
    char c = elmClient.read();
    if (c == '>') {
      prompt = true;
      break;
    }
    if (c == '\r' || c == '\n') {
      if (!atLineStart && len < sizeof(elmResponse) - 1) {
        elmResponse[len] = '\0';
        if (elmIsPendingLine(elmResponse + lineStart)) {
          unsigned long extended = millis() + 5000;
          if ((long)(extended - pendingCap) > 0) extended = pendingCap;
          if ((long)(extended - deadline) > 0) deadline = extended;
        }
        elmResponse[len++] = '\n';
        lineStart = len;
      }
      atLineStart = true;
      continue;
    }
    if (len < sizeof(elmResponse) - 1) {
      elmResponse[len++] = c;
      atLineStart = false;
    }
  }
  elmResponse[len] = '\0';
  if (!prompt) {
    unsigned long drainStart = millis();
    while (millis() - drainStart < 1000 && !prompt) {
      if (elmClient.available() && elmClient.read() == '>') prompt = true;
      yield();
    }
  }
  if (!prompt) {
    elmClient.print(' ');
    unsigned long stopStart = millis();
    while (millis() - stopStart < 1000) {
      if (elmClient.available() && elmClient.read() == '>') break;
      yield();
    }
  }

  // Traccia completa solo durante la connessione: a regime e nella scansione completa saturerebbe il buffer del
  // Serial monitor web
  if (currentMode == MODE_CONNECT) {
    Serial.print(F("TX: "));
    Serial.print(cmd);
    Serial.print(F("  RX: "));
    for (size_t i = 0; i < len; i++) Serial.print(elmResponse[i] == '\n' ? '|' : elmResponse[i]);
    Serial.println();
  }
  return elmResponse;
}

/** Invia un comando AT e verifica che l'adattatore risponda OK (anche preceduto dall'eco). */
bool elmCommandOk(const char* cmd) {
  bool ok = strstr(elmCommand(cmd, ELM327_TIMEOUT), "OK") != nullptr;
  if (!ok) {
    Serial.print(F("ELM327: comando non accettato "));
    Serial.println(cmd);
  }
  return ok;
}

/**
 * Imposta l'header CAN delle richieste solo se diverso da quello corrente: a 11 bit ATSH con 3 cifre, a 29 bit
 * ATCP con i 5 bit di priorità e ATSH con i 24 bit bassi (17FC007B = ATCP17 + ATSHFC007B).
 */
bool elmSetHeader(uint32_t requestId) {
  if (requestId == elmCurrentHeader) return true;
  char cmd[16];
  if (requestId & OBD_ID_EXTENDED) {
    const uint32_t raw = requestId & 0x1FFFFFFFUL;
    snprintf(cmd, sizeof(cmd), "ATCP%02X", (unsigned)((raw >> 24) & 0x1F));
    if (!elmCommandOk(cmd)) return false;
    snprintf(cmd, sizeof(cmd), "ATSH%06lX", (unsigned long)(raw & 0xFFFFFFUL));
  } else {
    snprintf(cmd, sizeof(cmd), "ATSH%03X", (unsigned)requestId);
  }
  if (!elmCommandOk(cmd)) return false;
  elmCurrentHeader = requestId;
  return true;
}

/**
 * Indirizza il flow control all'ID di richiesta, come vogliono le coppie UDS VAG: in modo 0 l'ELM lo manderebbe a
 * risposta - 8 (772 invece di 710). L'ordine è vincolato dal datasheet: ATFCSH e ATFCSD prima di ATFCSM1, altrimenti
 * risponde '?'. Se l'adattatore rifiuta i comandi le risposte multi-frame restano incomplete e lo si dichiara.
 */
bool elmSetFlowControlHeader(uint32_t requestId) {
  if (!obdTransportFlowControlAvailable) return false;
  if (requestId == elmFlowControlHeader && elmFlowControlUserMode) return true;
  char cmd[16];
  if (requestId & OBD_ID_EXTENDED) snprintf(cmd, sizeof(cmd), "ATFCSH%08lX", (unsigned long)(requestId & 0x1FFFFFFFUL));
  else snprintf(cmd, sizeof(cmd), "ATFCSH%03X", (unsigned)requestId);
  if (!elmCommandOk(cmd) || (!elmFlowControlUserMode && !elmCommandOk("ATFCSM1"))) {
    obdTransportFlowControlAvailable = false;
    return false;
  }
  elmFlowControlUserMode = true;
  elmFlowControlHeader = requestId;
  return true;
}

/**
 * Configura l'adattatore per il modo di ricezione: protocollo a 11 (ATSP6) o 29 bit (ATSP7), filtri di ricezione
 * sull'intervallo delle risposte UDS (ATCF/ATCM) oppure ricezione automatica (ATAR, che il datasheet indica per
 * annullare CF e CM), dati del flow control con STmin 5 ms. Dopo ATSP header e flow control vanno reimpostati.
 * Ritorna false se il protocollo a 29 bit o i filtri non sono accettati.
 */
bool elmApplyReceiveMode(uint8_t receiveMode) {
  if (receiveMode == elmReceiveMode) return true;
  const bool extended = receiveMode == OBD_RX_OBD29 || receiveMode == OBD_RX_UDS29;
  if (extended != elmExtendedProtocol) {
    if (extended && !obdTransportExtendedAvailable) return false;
    if (!elmCommandOk(extended ? "ATSP7" : ELM327_PROTOCOL)) {
      if (extended) obdTransportExtendedAvailable = false;
      return false;
    }
    elmExtendedProtocol = extended;
    elmCurrentHeader = 0;
    elmFlowControlHeader = 0;
  }
  bool ok = true;
  if (receiveMode == OBD_RX_UDS11) ok = elmCommandOk("ATCF700") && elmCommandOk("ATCM700");
  else if (receiveMode == OBD_RX_UDS29) ok = elmCommandOk("ATCF17FE0000") && elmCommandOk("ATCM1FFFFF00");
  else ok = elmCommandOk("ATAR");
  if (receiveMode == OBD_RX_UDS11 || receiveMode == OBD_RX_UDS29) {
    if (obdTransportFlowControlAvailable && !elmCommandOk("ATFCSD300005")) obdTransportFlowControlAvailable = false;
  } else if (elmFlowControlUserMode) {
    elmCommandOk("ATFCSM0");
    elmFlowControlUserMode = false;
    elmFlowControlHeader = 0;
  }
  // Con un filtro rifiutato il modo resta da applicare: la richiesta successiva lo ritenta invece di lavorare senza filtri
  elmReceiveMode = ok ? receiveMode : OBD_RX_UNSET;
  return ok;
}

/**
 * Inizializza l'ELM327 con eco, spazi e linefeed disattivati, header attivi e protocollo CAN 11 bit
 * 500 kbps, poi verifica cosa supporta il clone: header nelle risposte (senza, lo scan vede una sola
 * centralina) e ATSH (senza, solo richieste funzionali).
 */
void elmInit() {
  elmCommand("ATZ", 3000);
  delay(500);
  elmCommandOk("ATE0");
  elmCommandOk("ATL0");
  elmCommandOk("ATS0");
  elmCommandOk("ATH1");
  elmCommandOk(ELM327_PROTOCOL);
  Serial.print(F("ELM327: "));
  Serial.println(elmCommand("ATI", ELM327_TIMEOUT));

  elmCurrentHeader = 0x7DF;
  elmCountSuffixOk = false;
  elmReceiveMode = OBD_RX_OBD11;
  elmExtendedProtocol = false;
  elmFlowControlHeader = 0;
  elmFlowControlUserMode = false;
  elmLongTimeout = false;
  obdTransportHeadersAvailable = true;
  obdTransportPhysicalAvailable = true;
  obdTransportFlowControlAvailable = true;
  obdTransportExtendedAvailable = true;
  obdTransportPendingHandled = true;

  // Prima richiesta OBD: con gli header ogni riga inizia con l'ID della centralina (7E8...)
  const char* cursor = elmCommand("0100", ELM327_FIRST_TIMEOUT);
  bool withHeader = false;
  bool withoutHeader = false;
  char line[64];
  while (elmNextLineAt(cursor, line, sizeof(line))) {
    uint32_t id;
    uint8_t frame[8];
    uint8_t frameLen;
    if (obdElmParseHeaderLine(line, &id, frame, &frameLen) && obdIsResponseId(id)) withHeader = true;
    else if (obdElmParsePayloadLine(line, frame, &frameLen) && frame[1] == 0x41) withoutHeader = true;
  }
  if (!withHeader && withoutHeader) {
    obdTransportHeadersAvailable = false;
    Serial.println(F("ELM327: header non supportati, discovery limitata a una centralina"));
  }

  obdTransportPhysicalAvailable = elmCommandOk("ATSH7E0");
  if (obdTransportPhysicalAvailable) {
    elmCurrentHeader = 0x7E0;
    elmSetHeader(0x7DF);
  } else {
    Serial.println(F("ELM327: ATSH non supportato, solo richieste funzionali"));
  }
}

/**
 * Copia in line la prossima riga di elmResponse a partire dal cursore cursor, che avanza;
 * restituisce false a fine risposta. Le righe troppo lunghe vengono troncate.
 */
bool elmNextLineAt(const char*& cursor, char* line, size_t size) {
  if (*cursor == '\0') return false;
  size_t len = 0;
  while (*cursor != '\0' && *cursor != '\n') {
    if (len < size - 1) line[len++] = *cursor;
    cursor++;
  }
  if (*cursor == '\n') cursor++;
  line[len] = '\0';
  return true;
}

/**
 * Lunghezza della risposta a una richiesta Mode 01 (servizio più PID e dati), per sapere se sta in
 * un single frame; 0xFF se contiene un PID di lunghezza ignota.
 */
uint8_t expectedMode01ResponseLength(const uint8_t* request, uint8_t requestLen) {
  uint16_t total = 1;
  for (uint8_t i = 1; i < requestLen; i++) {
    uint8_t dataLen = obdPidDataLen(request[i]);
    if (dataLen == 0) return 0xFF;
    total += 1 + dataLen;
  }
  return total > 0xFE ? 0xFF : (uint8_t)total;
}

/** Vero se la risposta contiene una riga di errore dell'adattatore, diversa dalla semplice assenza di risposta. */
bool elmResponseHasTransportError(const char* response) {
  return strstr(response, "CAN ERROR") != nullptr || strstr(response, "BUFFER FULL") != nullptr ||
         strstr(response, "STOPPED") != nullptr || strstr(response, "?") != nullptr;
}

/**
 * Invia la richiesta già impostata e riassembla le righe della risposta per centralina. Con 7F xx 78 l'adattatore
 * continua ad attendere la risposta definitiva sulla riga successiva; ritorna true se una centralina ha chiesto altro
 * tempo senza poi rispondere, segno di un adattatore che non gestisce da sè i 78.
 */
bool elmExchange(const char* cmd, uint32_t requestId, uint32_t waitForEcuId, uint8_t receiveMode) {
  const char* response = elmCommand(cmd, elmNextTimeout);
  elmNextTimeout = ELM327_TIMEOUT;
  if (!elmClient.connected() || elmResponseHasTransportError(response)) obdTransportLastStatus = OBD_TRANSPORT_ERROR;
  uint32_t pendingIds[OBD_MAX_ECUS];
  uint8_t pendingCount = 0;
  const char* cursor = response;
  char line[64];
  while (elmNextLineAt(cursor, line, sizeof(line))) {
    uint32_t id;
    uint8_t frame[8];
    uint8_t frameLen;
    if (obdTransportHeadersAvailable) {
      if (!obdElmParseHeaderLine(line, &id, frame, &frameLen, elmIdDigits()) ||
          !obdAcceptsResponse(receiveMode, requestId, id)) continue;
    } else {
      if (!obdElmParsePayloadLine(line, frame, &frameLen)) continue;
      id = waitForEcuId != 0 ? waitForEcuId : OBD_ENGINE_RESPONSE_ID;
    }
    ObdIsoTpRx* slot = obdCollectorSlot(obdCollector, id);
    if (!slot) continue;
    obdIsoTpFeed(*slot, frame, frameLen);
    uint8_t service, nrc;
    if (slot->state == OBD_ISOTP_COMPLETE && obdIsNegativeResponse(slot->data, slot->receivedLen, &service, &nrc) &&
        nrc == OBD_NRC_RESPONSE_PENDING) {
      obdIsoTpReset(*slot, id);
      if (pendingCount < OBD_MAX_ECUS) pendingIds[pendingCount++] = id;
    }
  }
  for (uint8_t i = 0; i < pendingCount; i++) {
    if (!obdCollectorFind(obdCollector, pendingIds[i])) return true;
  }
  return false;
}

/**
 * Trasporto via ELM327: configura il modo di ricezione, imposta l'header (funzionale o fisico, a 11 o 29 bit) e nei
 * modi UDS il flow control verso l'ID di richiesta, invia la richiesta in esadecimale e riassembla le righe per
 * centralina. Il suffisso di conteggio si usa solo per i Mode 01 fisici a 11 bit con risposta in un frame, mai in
 * UDS, dove una riga 78 chiuderebbe il comando. Se una centralina chiede altro tempo e l'adattatore chiude senza
 * risposta, attiva ATSTFF (circa 1 s) e ripete la richiesta una volta. windowMs non serve: l'ELM applica i suoi tempi.
 * @see obd_scan.h obdTransact()
 */
uint8_t obdTransact(uint32_t requestId, const uint8_t* request, uint8_t requestLen, uint16_t windowMs,
                    uint32_t waitForEcuId, uint8_t receiveMode) {
  (void)windowMs;
  obdCollectorReset(obdCollector);
  obdTransportLastStatus = OBD_TRANSPORT_OK;
  if (requestLen == 0 || requestLen > 7 || !elmClient.connected()) {
    obdTransportLastStatus = OBD_TRANSPORT_ERROR;
    return 0;
  }
  const bool uds = receiveMode == OBD_RX_UDS11 || receiveMode == OBD_RX_UDS29;
  if (!obdIsFunctionalId(requestId) && !obdTransportPhysicalAvailable) return 0;
  // Senza header nelle righe non si distinguono le centraline UDS
  if (uds && !obdTransportHeadersAvailable) return 0;
  if (!elmApplyReceiveMode(receiveMode) || !elmSetHeader(requestId)) {
    obdTransportLastStatus = OBD_TRANSPORT_ERROR;
    return 0;
  }
  if (uds) elmSetFlowControlHeader(requestId);

  char cmd[18];
  size_t pos = 0;
  for (uint8_t i = 0; i < requestLen; i++) pos += snprintf(cmd + pos, sizeof(cmd) - pos, "%02X", request[i]);
  if (elmCountSuffixOk && receiveMode == OBD_RX_OBD11 && !obdIsFunctionalId(requestId) && request[0] == 0x01 &&
      expectedMode01ResponseLength(request, requestLen) <= 7) {
    cmd[pos++] = '1';
    cmd[pos] = '\0';
  }

  bool pendingWithoutAnswer = elmExchange(cmd, requestId, waitForEcuId, receiveMode);
  if (pendingWithoutAnswer && uds && !elmLongTimeout) {
    obdTransportPendingHandled = false;
    if (elmCommandOk("ATSTFF")) {
      elmLongTimeout = true;
      obdCollectorReset(obdCollector);
      elmExchange(cmd, requestId, waitForEcuId, receiveMode);
    }
  }
  return obdCollectorCompleteCount(obdCollector);
}

/**
 * Discovery a raffica non disponibile sull'ELM327, che invia una richiesta per comando: la scansione procede in
 * sequenza.
 * @see obd_scan.h obdTransportDiscoveryBurst()
 */
uint8_t obdTransportDiscoveryBurst(const uint32_t* requestIds, uint16_t count, uint32_t* responders,
                                   uint8_t maxResponders) {
  (void)requestIds;
  (void)count;
  (void)responders;
  (void)maxResponders;
  return OBD_BURST_UNAVAILABLE;
}

/**
 * Riporta l'ELM327 allo stato della lettura live dopo la scansione completa: la reinizializzazione rimette protocollo
 * a 11 bit, ricezione automatica, flow control automatico e timeout di default, più semplice e sicura di annullare i
 * singoli comandi.
 * @see obd_scan.h obdTransportRestore()
 */
void obdTransportRestore() {
  if (!elmClient.connected()) return;
  elmInit();
  elmTestCountSuffix();
}

/**
 * Verifica se il clone accetta il numero di risposte attese dopo la richiesta (ELM327 v1.3+),
 * provandolo sul motore in indirizzamento fisico; senza conferma il suffisso non viene usato.
 */
void elmTestCountSuffix() {
  elmCountSuffixOk = false;
  if (!obdTransportPhysicalAvailable) return;
  if (!elmSetHeader(obdPhysicalRequestId(OBD_ENGINE_RESPONSE_ID))) return;
  const char* cursor = elmCommand("01001", ELM327_TIMEOUT);
  char line[64];
  while (elmNextLineAt(cursor, line, sizeof(line))) {
    uint32_t id;
    uint8_t frame[8];
    uint8_t frameLen;
    if (obdElmParseHeaderLine(line, &id, frame, &frameLen) && id == OBD_ENGINE_RESPONSE_ID && frameLen >= 3 &&
        frame[1] == 0x41 && frame[2] == 0x00) {
      elmCountSuffixOk = true;
    }
  }
  Serial.print(F("ELM327: suffisso numero risposte "));
  Serial.println(elmCountSuffixOk ? F("supportato") : F("non supportato"));
}

// ============================================================
// MODALITA' CONNESSIONE
// ============================================================

/**
 * Connette l'ESP8266 alla rete WiFi dell'ELM327, stabilisce la connessione TCP, inizializza l'ELM327 con
 * i comandi AT e passa al monitor con il profilo vettura. Dopo "Motore non risponde" parte solo allo
 * scadere dell'attesa, servendo intanto il web server dal loop.
 */
void executeConnectMode() {
  if ((long)(millis() - engineRetryAt) < 0) {
    yield();
    return;
  }
  obdTransportReady = false;

  // --- Fase 1: Connessione WiFi ---
  String ssidLine = String("SSID: ") + ELM327_SSID;
  String pswLine = String("PSW: ") + (strlen(ELM327_PASS) > 0 ? ELM327_PASS : "(aperta)");
  drawConnectScreen(ssidLine.c_str(), pswLine.c_str());

  Serial.print(F("Connessione WiFi a: "));
  Serial.println(F(ELM327_SSID));

  // SoftAP per OTA in parallelo alla connessione ELM327, solo finchè la finestra OTA è aperta:
  // dopo la chiusura una riconnessione non deve riaccenderlo senza il web server attivo
  if (otaActive) {
    WiFi.mode(WIFI_AP_STA);
    WiFi.softAP(OTA_SSID, OTA_PASS);
  } else {
    WiFi.mode(WIFI_STA);
  }
  WiFi.begin(ELM327_SSID, ELM327_PASS);

  int wifiAttempts = 0;
  while (WiFi.status() != WL_CONNECTED) {
    if (otaActive) httpServer.handleClient();
    delay(500);
    Serial.print(F("."));
    wifiAttempts++;
    if (wifiAttempts > 30) {
      Serial.println(F("\nRete non trovata!"));
      showError(("Rete " + String(ELM327_SSID)).c_str(), " non trovata");
      waitServingHttp(3000);
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
      Serial.println(F("TCP fallito, retry tra 10s"));
      showError("TCP fallito", ELM327_IP);
      waitServingHttp(10000);
      return;  // executeConnectMode rientra dal loop e ritenta
    }
    waitServingHttp(1000);
  }

  delay(500);

  // --- Fase 3: Inizializzazione ELM327 ---
  drawConnectScreen("Init ELM327...", "");
  Serial.println(F("Inizializzazione ELM327..."));
  elmNextTimeout = ELM327_FIRST_TIMEOUT;
  elmInit();
  elmTestCountSuffix();
  Serial.println(F("ELM327 inizializzato"));
  drawConnectScreen("ELM327 OK!", obdTransportHeadersAvailable ? "" : "senza header");
  delay(1000);

  // Nuova sessione monitor, anche dopo una riconnessione
  obdLiveConfigure();
  labelsDrawn = false;
  currentScreen = 0;
  totalScreens = 1;
  lastDrawnDtcScreen = 0xFF;
  dtcCheckRequested = true;
  obdTransportReady = true;
  currentMode = MODE_MONITOR;
}

// ============================================================
// SCANSIONE COMPLETA
// ============================================================

/**
 * Esegue un passo della scansione completa avviata dal web e ne mostra fase e avanzamento sul display; una
 * pressione del pulsante la interrompe, la caduta del TCP la chiude con il riepilogo parziale. Al termine blocca
 * la pressione lunga, che altrimenti cancellerebbe i DTC a chi tiene premuto, fa ripartire la finestra OTA e fa
 * riprendere il monitor con un ridisegno completo.
 */
void executeProbeStep() {
  static unsigned long lastDraw = 0;
  ButtonEvent ev = buttonPoll();
  if (ev == BTN_SHORT || ev == BTN_LONG || !elmClient.connected()) obdProbeStop();
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
 * Esegue un ciclo del monitor: controllo della connessione TCP, pulsante, controllo periodico dei DTC,
 * una lettura live (obd_live.h) anche mentre sono visibili le pagine DTC, aggiornamento del display e
 * diagnostica seriale. Se il motore resta muto per SILENT_CYCLES_BEFORE_RECOVERY cicli mostra "Motore non
 * risponde" e si riconnette dopo ENGINE_SILENT_RETRY_MS.
 */
void executeMonitorMode() {
  // Verifica connessione TCP attiva
  if (!elmClient.connected()) {
    Serial.println(F("Connessione TCP persa! Riconnessione..."));
    drawConnectScreen("Riconnessione..", "");
    elmClient.stop();
    obdTransportReady = false;
    delay(1000);
    labelsDrawn = false;
    currentMode = MODE_CONNECT;
    return;
  }

  // Pulsante fisico: short = ciclo schermate, long = clear DTC.
  ButtonEvent ev = buttonPoll();
  if (ev == BTN_SHORT && totalScreens > 1) {
    currentScreen = (currentScreen + 1) % totalScreens;
    lastScreenSwitch = millis();
    labelsDrawn = false;
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
    // Recovery: l'ELM327 non ottiene risposte dal motore, connessione e inizializzazione si ripetono dopo l'attesa
    Serial.println(F("ELM327 senza risposte dal motore, riconnessione TCP tra 10 s"));
    showError("Motore non risponde", "Quadro acceso?");
    obdLiveSilentStreak = 0;
    elmClient.stop();
    obdTransportReady = false;
    engineRetryAt = millis() + ENGINE_SILENT_RETRY_MS;
    labelsDrawn = false;
    currentMode = MODE_CONNECT;
    return;
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
 * @since 24/03/26 Mattia Alesi
 */
void clearValueArea(int x, int y, int w, int h) {
  u8g2.setDrawColor(0);
  u8g2.drawBox(x, y, w, h);
  u8g2.setDrawColor(1);
}

/**
 * Disegna il valore boost con 2 cifre decimali.
 * Colonna sinistra, y=15. Font t0_14b.
 * @see updateDisplay()
 * @since 24/03/26 Mattia Alesi
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
 * Colonna sinistra, y=50.
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
 * Colonna destra riga 1 (RIGHT_COL_X), y=15.
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

/** Disegna la schermata di connessione WiFi/TCP */
void drawConnectScreen(const char* status, const char* detail) {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tr);
  drawStrCentered(11, "CONNESSIONE");
  drawStrCentered(27, status);
  drawStrCentered(43, detail);
  u8g2.sendBuffer();
}

/** Disegna la schermata della scansione completa con fase e avanzamento, centrati come nella build CAN. */
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
 * Usata per errori fatali (connessione, ECU non risponde).
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
