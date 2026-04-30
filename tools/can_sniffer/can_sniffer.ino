/**
 * CAN Bus Sniffer per ESP8266 D1 Mini + MCP2515/TJA1050
 *
 * Cattura tutto il traffico CAN in modalita' listen-only (passivo, nessuna trasmissione).
 * Pensato per fare reverse engineering dei messaggi CAN sull'Infotainment/Gateway
 * dell'Audi A5 B8 (2 fili collegati alla head unit Android).
 *
 * Funzionalita':
 *   - Auto-detect velocita' bus CAN (500, 250, 125, 100 kbps)
 *   - Output CSV su Serial: timestamp_ms,ID_hex,DLC,B0,B1,...,B7
 *   - Riepilogo statistiche ogni 10 secondi
 *   - Comandi seriali: m=marker, s=statistiche, r=reset contatori
 *
 * Hardware:
 *   ESP8266 Lolin D1 Mini + modulo MCP2515/TJA1050 integrato (cristallo 8 MHz)
 *
 * Cablaggio D1 Mini -> MCP2515 (identico al progetto OBD2):
 *   D8 (GPIO15) --> CS
 *   D5 (GPIO14) --> SCK
 *   D7 (GPIO13) --> SI  (MOSI)
 *   D6 (GPIO12) --> SO  (MISO)
 *   D0 (GPIO16) --> INT
 *   5V          --> VCC
 *   GND         --> GND
 *   MCP2515 CANH --> CAN High (parallelo ai 2 fili head unit)
 *   MCP2515 CANL --> CAN Low (parallelo ai 2 fili head unit)
 *
 * Veicolo: Audi A5 B8 2.7 TDI (CGKA) Multitronic
 *
 * @since 07/04/26 Mattia Alesi
 */

#include <SPI.h>
#include <mcp_can.h>

// ============================================================
// CONFIGURAZIONE
// ============================================================

/** Pin CS del MCP2515 — D8 (GPIO15) */
#define CAN_CS_PIN 15

/** Pin INT del MCP2515 per notifica ricezione — D0 (GPIO16) */
#define CAN_INT_PIN 16

/** Cristallo MCP2515: MCP_8MHZ o MCP_16MHZ (controlla il modulo) */
#define MCP2515_CRYSTAL MCP_8MHZ

/** Velocita' seriale per il monitor */
#define SERIAL_BAUD 115200

/** Intervallo stampa statistiche automatiche (ms) */
#define STATS_INTERVAL_MS 10000

/** Numero massimo di CAN ID distinti da tracciare nelle statistiche */
#define MAX_TRACKED_IDS 128

// ============================================================
// STRUTTURE DATI
// ============================================================

/** Statistiche per singolo CAN ID */
struct CanIdStats {
    uint32_t id;               // CAN ID
    uint32_t count;            // Numero messaggi ricevuti
    unsigned long firstSeen;   // Timestamp primo messaggio (ms)
    unsigned long lastSeen;    // Timestamp ultimo messaggio (ms)
    uint8_t lastData[8];       // Ultimo payload ricevuto
    uint8_t dlc;               // DLC dell'ultimo messaggio
};

// ============================================================
// VARIABILI GLOBALI
// ============================================================

MCP_CAN CAN(CAN_CS_PIN);

CanIdStats idStats[MAX_TRACKED_IDS];
int numTrackedIds = 0;
unsigned long totalMessages = 0;
unsigned long lastStatsTime = 0;
int markerCount = 0;
bool busDetected = false;

/** Velocita' CAN da provare in ordine (le piu' comuni prima) */
const uint8_t speeds[] = {CAN_500KBPS, CAN_250KBPS, CAN_125KBPS, CAN_100KBPS};
const char* speedNames[] = {"500 kbps", "250 kbps", "125 kbps", "100 kbps"};
const int numSpeeds = 4;

// ============================================================
// FUNZIONI
// ============================================================

/**
 * Cerca o crea una entry nelle statistiche per il CAN ID specificato.
 * Se l'ID non esiste e c'è spazio, ne crea uno nuovo.
 * @return indice nell'array idStats, oppure -1 se pieno
 * @since 07/04/26 Mattia Alesi
 */
int findOrCreateId(uint32_t id) {
    for (int i = 0; i < numTrackedIds; i++) {
        if (idStats[i].id == id) return i;
    }
    if (numTrackedIds >= MAX_TRACKED_IDS) return -1;
    int idx = numTrackedIds++;
    idStats[idx].id = id;
    idStats[idx].count = 0;
    idStats[idx].firstSeen = millis();
    idStats[idx].lastSeen = 0;
    idStats[idx].dlc = 0;
    return idx;
}

/**
 * Aggiorna le statistiche per un messaggio CAN ricevuto.
 * @since 07/04/26 Mattia Alesi
 */
void updateStats(uint32_t id, uint8_t dlc, uint8_t* data) {
    int idx = findOrCreateId(id);
    if (idx < 0) return;

    idStats[idx].count++;
    idStats[idx].lastSeen = millis();
    idStats[idx].dlc = dlc;
    memcpy(idStats[idx].lastData, data, 8);
    totalMessages++;
}

/**
 * Stampa un messaggio CAN in formato CSV su Serial.
 * Formato: timestamp_ms,ID_hex,DLC,B0,B1,B2,B3,B4,B5,B6,B7
 * @since 07/04/26 Mattia Alesi
 */
void printMessageCSV(unsigned long timestamp, uint32_t id, uint8_t dlc, uint8_t* data) {
    char buf[80];
    snprintf(buf, sizeof(buf), "%lu,0x%03lX,%d,%02X,%02X,%02X,%02X,%02X,%02X,%02X,%02X",
             timestamp, (unsigned long)id, dlc,
             data[0], data[1], data[2], data[3],
             data[4], data[5], data[6], data[7]);
    Serial.println(buf);
}

/**
 * Stampa le statistiche di tutti i CAN ID tracciati: conteggio, frequenza media, ultimo payload.
 * @since 07/04/26 Mattia Alesi
 */
void printStats() {
    Serial.println();
    Serial.println(F("# === STATISTICHE ==="));
    Serial.print(F("# Messaggi totali: "));
    Serial.println(totalMessages);
    Serial.print(F("# CAN ID distinti: "));
    Serial.println(numTrackedIds);
    Serial.println(F("#"));
    Serial.println(F("# ID       | Conteggio | Freq.media | Ultimo payload"));
    Serial.println(F("# ---------|-----------|------------|-------------------------------"));

    for (int i = 0; i < numTrackedIds; i++) {
        float freq = 0;
        if (idStats[i].count > 1) {
            unsigned long span = idStats[i].lastSeen - idStats[i].firstSeen;
            if (span > 0) {
                freq = (float)(idStats[i].count - 1) * 1000.0f / (float)span;
            }
        }

        char line[120];
        snprintf(line, sizeof(line),
                 "# 0x%03lX   | %7lu   | %6.1f Hz  | %02X %02X %02X %02X %02X %02X %02X %02X",
                 (unsigned long)idStats[i].id, idStats[i].count, (double)freq,
                 idStats[i].lastData[0], idStats[i].lastData[1],
                 idStats[i].lastData[2], idStats[i].lastData[3],
                 idStats[i].lastData[4], idStats[i].lastData[5],
                 idStats[i].lastData[6], idStats[i].lastData[7]);
        Serial.println(line);
    }
    Serial.println(F("# === FINE STATISTICHE ==="));
    Serial.println();
}

/**
 * Reset di tutti i contatori e statistiche.
 * @since 07/04/26 Mattia Alesi
 */
void resetStats() {
    numTrackedIds = 0;
    totalMessages = 0;
    markerCount = 0;
    Serial.println(F("# Contatori resettati"));
}

/**
 * Inserisce una riga marker nel log CSV per segnare l'inizio di un'azione.
 * @since 07/04/26 Mattia Alesi
 */
void insertMarker() {
    markerCount++;
    Serial.println();
    Serial.print(F("# >>>>>> MARKER "));
    Serial.print(markerCount);
    Serial.print(F(" - "));
    Serial.print(millis());
    Serial.println(F(" ms <<<<<<"));
    Serial.println();
}

/**
 * Prova ad inizializzare il MCP2515 in listen-only mode con la velocita' specificata.
 * Verifica se riceve messaggi validi entro 3 secondi.
 * @return true se riceve almeno 3 messaggi
 * @since 07/04/26 Mattia Alesi
 */
bool trySpeed(int speedIndex) {
    Serial.print(F("# Provo "));
    Serial.print(speedNames[speedIndex]);
    Serial.print(F("... "));

    /**
     * MCP_LISTENONLY = modalita' solo ascolto, non trasmette ACK.
     * Sicuro per il bus: non interferisce con nessun modulo.
     */
    if (CAN.begin(MCP_LISTENONLY, speeds[speedIndex], MCP2515_CRYSTAL) != CAN_OK) {
        Serial.println(F("errore init MCP2515"));
        return false;
    }

    CAN.setMode(MCP_LISTENONLY);

    unsigned long start = millis();
    int received = 0;
    unsigned long canId;
    uint8_t dlc;
    uint8_t buf[8];

    while (millis() - start < 3000) {
        if (CAN.checkReceive() == CAN_MSGAVAIL) {
            if (CAN.readMsgBuf(&canId, &dlc, buf) == CAN_OK) {
                received++;
                if (received >= 3) {
                    Serial.print(F("OK! Ricevuti "));
                    Serial.print(received);
                    Serial.println(F(" messaggi"));
                    return true;
                }
            }
        }
        yield();
    }

    Serial.print(F("solo "));
    Serial.print(received);
    Serial.println(F(" messaggi - provo altra velocita'"));
    return false;
}

/**
 * Tenta l'auto-detect della velocita' del bus CAN provando tutte le velocita' note.
 * @return true se il bus è stato rilevato con successo
 * @since 07/04/26 Mattia Alesi
 */
bool autoDetectSpeed() {
    Serial.println(F("# Auto-detect velocita' bus CAN..."));
    Serial.println(F("#"));

    for (int i = 0; i < numSpeeds; i++) {
        if (trySpeed(i)) {
            Serial.print(F("# >> Bus CAN rilevato a "));
            Serial.print(speedNames[i]);
            Serial.println(F(" <<"));
            Serial.println(F("#"));
            return true;
        }
    }

    Serial.println(F("# ERRORE: nessun bus CAN rilevato a nessuna velocita'."));
    Serial.println(F("# Verifica cablaggio MCP2515 e connessione CANH/CANL ai 2 fili."));
    return false;
}

/**
 * Gestisce i comandi ricevuti da Serial Monitor.
 *   m = inserisci marker
 *   s = stampa statistiche
 *   r = reset contatori
 * @since 07/04/26 Mattia Alesi
 */
void handleSerialCommands() {
    while (Serial.available()) {
        char c = Serial.read();
        switch (c) {
            case 'm':
            case 'M':
                insertMarker();
                break;
            case 's':
            case 'S':
                printStats();
                break;
            case 'r':
            case 'R':
                resetStats();
                break;
        }
    }
}

// ============================================================
// SETUP E LOOP
// ============================================================

void setup() {
    Serial.begin(SERIAL_BAUD);
    delay(500);

    Serial.println();
    Serial.println(F("# ============================================"));
    Serial.println(F("# CAN Bus Sniffer - Audi A5 B8"));
    Serial.println(F("# D1 Mini + MCP2515/TJA1050 (LISTEN-ONLY)"));
    Serial.println(F("# ============================================"));
    Serial.println(F("#"));
    Serial.println(F("# Comandi: m=marker  s=statistiche  r=reset"));
    Serial.println(F("#"));

    /** Pin SPI sono quelli di default ESP8266: SCK=D5, MISO=D6, MOSI=D7, CS=D8 */
    pinMode(CAN_INT_PIN, INPUT);

    busDetected = autoDetectSpeed();

    if (busDetected) {
        Serial.println(F("# Inizio cattura. Header CSV:"));
        Serial.println(F("timestamp_ms,ID_hex,DLC,B0,B1,B2,B3,B4,B5,B6,B7"));
        lastStatsTime = millis();
    }
}

void loop() {
    if (!busDetected) {
        Serial.println(F("# Riprovo auto-detect..."));
        busDetected = autoDetectSpeed();
        if (busDetected) {
            Serial.println(F("# Inizio cattura. Header CSV:"));
            Serial.println(F("timestamp_ms,ID_hex,DLC,B0,B1,B2,B3,B4,B5,B6,B7"));
            lastStatsTime = millis();
        } else {
            delay(5000);
        }
        return;
    }

    /** Leggi messaggi CAN dal MCP2515 (via interrupt pin o polling) */
    while (digitalRead(CAN_INT_PIN) == LOW || CAN.checkReceive() == CAN_MSGAVAIL) {
        unsigned long canId;
        uint8_t dlc;
        uint8_t data[8] = {0};

        if (CAN.readMsgBuf(&canId, &dlc, data) == CAN_OK) {
            unsigned long now = millis();

            /** Maschera bit extended frame (bit 31) per avere solo l'ID */
            canId &= 0x7FF;  // 11-bit standard ID

            printMessageCSV(now, canId, dlc, data);
            updateStats(canId, dlc, data);
        } else {
            break;
        }
    }

    /** Stampa statistiche periodiche */
    if (millis() - lastStatsTime >= STATS_INTERVAL_MS) {
        printStats();
        lastStatsTime = millis();
    }

    handleSerialCommands();
    yield();
}
