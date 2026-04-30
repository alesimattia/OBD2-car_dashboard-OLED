/**
 * CAN Box Serial Sniffer per ESP8266 D1 Mini
 *
 * Intercetta la comunicazione seriale tra il CAN box della head unit Android
 * e la head unit stessa. Rileva automaticamente baud rate e protocollo.
 *
 * Usa SoftwareSerial perche' l'ESP8266 ha una sola UART hardware (usata per il
 * Serial Monitor USB). Il pin RX SoftwareSerial è configurabile.
 *
 * Protocolli CAN box supportati per il riconoscimento:
 *   - RZC-04: header 0x2E, [len], [tipo], [dati...], [checksum]
 *   - Raise:  header 0xFD, [len], [cmd], [dati...], [checksum XOR]
 *   - Hiworld: header 0x5A 0xA5, [len], [dati...], [checksum]
 *
 * Funzionalita':
 *   - Auto-detect baud rate (9600, 19200, 38400, 57600, 115200)
 *   - Stampa byte ricevuti in hex con timestamp
 *   - Riconoscimento automatico protocollo CAN box
 *   - Decodifica frame se protocollo riconosciuto
 *   - Comandi seriali: b=forza baud rate, s=statistiche, p=reset protocollo
 *
 * Hardware:
 *   D2 (GPIO4) --> Linea TX del CAN box (filo seriale verso head unit)
 *   GND        --> GND comune
 *
 * Veicolo: Audi A5 B8 2.7 TDI (CGKA) Multitronic
 *
 * @since 07/04/26 Mattia Alesi
 */

#include <SoftwareSerial.h>

// ============================================================
// CONFIGURAZIONE
// ============================================================

/** Pin RX SoftwareSerial — D2 (GPIO4), collegato alla linea TX del CAN box */
#define CANBOX_RX_PIN 4

/**
 * Pin TX SoftwareSerial — D1 (GPIO5), non collegato fisicamente.
 * Serve solo per l'init della libreria, non trasmette nulla.
 */
#define CANBOX_TX_PIN 5

/** Velocita' seriale per il monitor USB */
#define SERIAL_BAUD 115200

/** Durata test per ogni baud rate durante auto-detect (ms) */
#define BAUD_TEST_DURATION_MS 2000

/** Dimensione buffer per accumulare byte di un frame */
#define FRAME_BUF_SIZE 64

/** Timeout frame: se non arrivano byte per questo tempo, il frame è completo (ms) */
#define FRAME_TIMEOUT_MS 5

// ============================================================
// ENUMERAZIONI
// ============================================================

/** Protocolli CAN box conosciuti */
enum Protocol {
    PROTO_UNKNOWN = 0,
    PROTO_RZC04,
    PROTO_RAISE,
    PROTO_HIWORLD
};

// ============================================================
// VARIABILI GLOBALI
// ============================================================

SoftwareSerial canboxSerial(CANBOX_RX_PIN, CANBOX_TX_PIN);

/** Baud rate da testare in ordine (38400 è il piu' comune per CAN box) */
const long baudRates[] = {38400, 19200, 115200, 57600, 9600};
const int numBaudRates = 5;

long currentBaud = 0;
Protocol detectedProtocol = PROTO_UNKNOWN;
unsigned long totalBytes = 0;
unsigned long totalFrames = 0;

uint8_t frameBuf[FRAME_BUF_SIZE];
int frameLen = 0;
unsigned long lastByteTime = 0;

/** Contatori per rilevamento protocollo */
int headerCount_RZC = 0;     // contatore header 0x2E
int headerCount_Raise = 0;   // contatore header 0xFD
int headerCount_Hiworld = 0; // contatore sequenza 0x5A 0xA5
uint8_t prevByte = 0;

// ============================================================
// FUNZIONI — AUTO-DETECT
// ============================================================

/**
 * Prova un baud rate e conta i byte ricevuti in un intervallo di tempo.
 * Un baud rate corretto produce byte con distribuzione ragionevole.
 * @return numero di byte ricevuti nel periodo di test, 0 se troppi errori framing
 * @since 07/04/26 Mattia Alesi
 */
int testBaudRate(long baud) {
    canboxSerial.end();
    canboxSerial.begin(baud);
    delay(50);

    /** Svuota il buffer iniziale */
    while (canboxSerial.available()) canboxSerial.read();

    int count = 0;
    int framing_errors = 0;
    unsigned long start = millis();

    while (millis() - start < BAUD_TEST_DURATION_MS) {
        if (canboxSerial.available()) {
            uint8_t b = canboxSerial.read();
            count++;
            /** Conta byte 0x00 e 0xFF come possibili errori di framing */
            if (b == 0x00 || b == 0xFF) framing_errors++;
        }
        yield();
    }

    /**
     * Un baud rate corretto dovrebbe avere < 30% di byte 0x00/0xFF.
     * Un baud rate sbagliato produce quasi tutti byte "spazzatura".
     */
    float errorRate = (count > 0) ? (float)framing_errors / count : 1.0f;

    Serial.print(F("# "));
    Serial.print(baud);
    Serial.print(F(" baud: "));
    Serial.print(count);
    Serial.print(F(" byte, "));
    Serial.print((int)(errorRate * 100));
    Serial.println(F("% errori framing"));

    if (count > 10 && errorRate < 0.30f) {
        return count;
    }
    return 0;
}

/**
 * Prova tutte le velocita' e seleziona quella con piu' byte validi ricevuti.
 * @return true se un baud rate è stato rilevato
 * @since 07/04/26 Mattia Alesi
 */
bool autoDetectBaud() {
    Serial.println(F("# Auto-detect baud rate CAN box..."));
    Serial.println(F("#"));

    long bestBaud = 0;
    int bestCount = 0;

    for (int i = 0; i < numBaudRates; i++) {
        int count = testBaudRate(baudRates[i]);
        if (count > bestCount) {
            bestCount = count;
            bestBaud = baudRates[i];
        }
    }

    if (bestBaud > 0) {
        currentBaud = bestBaud;
        canboxSerial.end();
        canboxSerial.begin(currentBaud);
        Serial.print(F("# >> Baud rate rilevato: "));
        Serial.print(currentBaud);
        Serial.println(F(" <<"));
        return true;
    }

    Serial.println(F("# ERRORE: nessun baud rate valido rilevato."));
    Serial.println(F("# Verifica collegamento D2 (GPIO4) alla linea TX del CAN box."));
    return false;
}

// ============================================================
// FUNZIONI — RILEVAMENTO PROTOCOLLO
// ============================================================

/**
 * Analizza un byte ricevuto per cercare pattern di header noti.
 * Dopo aver accumulato abbastanza evidenza, identifica il protocollo.
 * @since 07/04/26 Mattia Alesi
 */
void analyzeByteForProtocol(uint8_t b) {
    if (b == 0x2E) headerCount_RZC++;
    if (b == 0xFD) headerCount_Raise++;
    if (prevByte == 0x5A && b == 0xA5) headerCount_Hiworld++;
    prevByte = b;
}

/**
 * Verifica se abbiamo abbastanza dati per identificare il protocollo.
 * Richiede almeno 5 header dello stesso tipo per confermare.
 * @since 07/04/26 Mattia Alesi
 */
void checkProtocolDetection() {
    if (detectedProtocol != PROTO_UNKNOWN) return;

    const int threshold = 5;

    if (headerCount_Hiworld >= threshold) {
        detectedProtocol = PROTO_HIWORLD;
        Serial.println(F("# >> Protocollo rilevato: HIWORLD (header 0x5A 0xA5) <<"));
    } else if (headerCount_Raise >= threshold && headerCount_Raise > headerCount_RZC) {
        detectedProtocol = PROTO_RAISE;
        Serial.println(F("# >> Protocollo rilevato: RAISE (header 0xFD) <<"));
    } else if (headerCount_RZC >= threshold && headerCount_RZC > headerCount_Raise) {
        detectedProtocol = PROTO_RZC04;
        Serial.println(F("# >> Protocollo rilevato: RZC-04 (header 0x2E) <<"));
    }
}

// ============================================================
// FUNZIONI — DECODIFICA FRAME
// ============================================================

/**
 * Nomi dei tipi di messaggio per il protocollo Raise.
 * Usato da decodeRaiseFrame() per mostrare descrizioni leggibili.
 * @since 07/04/26 Mattia Alesi
 */
const char* getRaiseTypeName(uint8_t cmd) {
    switch (cmd) {
        case 0x01: return "VEHICLE_INFO";
        case 0x02: return "DOORS";
        case 0x03: return "CLIMATE_AC";
        case 0x04: return "RADAR_PARK";
        case 0x05: return "MEDIA_KEY";
        case 0x06: return "STEERING_WHEEL";
        case 0x07: return "LIGHTS";
        case 0x08: return "WINDOW";
        case 0x10: return "VEHICLE_TYPE";
        case 0x20: return "HANDBRAKE";
        case 0x24: return "AMBIENT_LIGHT";
        default:   return "SCONOSCIUTO";
    }
}

/**
 * Nomi dei tipi di messaggio per il protocollo RZC-04.
 * @since 07/04/26 Mattia Alesi
 */
const char* getRzcTypeName(uint8_t type) {
    switch (type) {
        case 0x01: return "CLIMATE";
        case 0x02: return "RADAR_PARK";
        case 0x03: return "DOOR_STATUS";
        case 0x04: return "MEDIA_KEY";
        case 0x05: return "VEHICLE_INFO";
        case 0x06: return "STEERING_KEY";
        case 0x07: return "AIR_COND";
        case 0x08: return "WINDOW";
        case 0x10: return "LIGHT_STATUS";
        default:   return "SCONOSCIUTO";
    }
}

/**
 * Decodifica un frame del protocollo Raise.
 * Formato: [0xFD] [len] [cmd] [data...] [checksum XOR]
 * @since 07/04/26 Mattia Alesi
 */
void decodeRaiseFrame(uint8_t* buf, int len) {
    if (len < 4) return;
    if (buf[0] != 0xFD) return;

    uint8_t fLen = buf[1];
    uint8_t cmd = buf[2];

    /** Verifica checksum XOR */
    uint8_t xorCheck = 0;
    for (int i = 0; i < len - 1; i++) {
        xorCheck ^= buf[i];
    }

    bool checksumOk = (xorCheck == buf[len - 1]);

    Serial.print(F("# [RAISE] cmd=0x"));
    Serial.print(cmd, HEX);
    Serial.print(F(" ("));
    Serial.print(getRaiseTypeName(cmd));
    Serial.print(F(") len="));
    Serial.print(fLen);
    Serial.print(F(" chk="));
    Serial.print(checksumOk ? "OK" : "ERR");
    Serial.print(F(" dati="));
    for (int i = 3; i < len - 1; i++) {
        if (buf[i] < 0x10) Serial.print('0');
        Serial.print(buf[i], HEX);
        Serial.print(' ');
    }
    Serial.println();
}

/**
 * Decodifica un frame del protocollo RZC-04.
 * Formato: [0x2E] [len] [tipo] [data...] [checksum]
 * @since 07/04/26 Mattia Alesi
 */
void decodeRzcFrame(uint8_t* buf, int len) {
    if (len < 4) return;
    if (buf[0] != 0x2E) return;

    uint8_t fLen = buf[1];
    uint8_t type = buf[2];

    /** Verifica checksum (somma di tutti i byte mod 256) */
    uint8_t sum = 0;
    for (int i = 0; i < len - 1; i++) {
        sum += buf[i];
    }
    bool checksumOk = (sum == buf[len - 1]);

    Serial.print(F("# [RZC-04] tipo=0x"));
    Serial.print(type, HEX);
    Serial.print(F(" ("));
    Serial.print(getRzcTypeName(type));
    Serial.print(F(") len="));
    Serial.print(fLen);
    Serial.print(F(" chk="));
    Serial.print(checksumOk ? "OK" : "ERR");
    Serial.print(F(" dati="));
    for (int i = 3; i < len - 1; i++) {
        if (buf[i] < 0x10) Serial.print('0');
        Serial.print(buf[i], HEX);
        Serial.print(' ');
    }
    Serial.println();
}

/**
 * Decodifica un frame del protocollo Hiworld.
 * Formato: [0x5A] [0xA5] [len] [data...] [checksum]
 * @since 07/04/26 Mattia Alesi
 */
void decodeHiworldFrame(uint8_t* buf, int len) {
    if (len < 5) return;
    if (buf[0] != 0x5A || buf[1] != 0xA5) return;

    uint8_t fLen = buf[2];

    /** Verifica checksum (somma dati mod 256) */
    uint8_t sum = 0;
    for (int i = 2; i < len - 1; i++) {
        sum += buf[i];
    }
    bool checksumOk = (sum == buf[len - 1]);

    Serial.print(F("# [HIWORLD] len="));
    Serial.print(fLen);
    Serial.print(F(" chk="));
    Serial.print(checksumOk ? "OK" : "ERR");
    Serial.print(F(" dati="));
    for (int i = 3; i < len - 1; i++) {
        if (buf[i] < 0x10) Serial.print('0');
        Serial.print(buf[i], HEX);
        Serial.print(' ');
    }
    Serial.println();
}

/**
 * Processa un frame completo accumulato nel buffer.
 * Stampa i byte raw in hex e, se il protocollo è noto, decodifica il frame.
 * @since 07/04/26 Mattia Alesi
 */
void processFrame() {
    if (frameLen == 0) return;

    totalFrames++;

    /** Stampa raw hex */
    Serial.print(millis());
    Serial.print(F(": "));
    for (int i = 0; i < frameLen; i++) {
        if (frameBuf[i] < 0x10) Serial.print('0');
        Serial.print(frameBuf[i], HEX);
        Serial.print(' ');
    }
    Serial.println();

    /** Decodifica se protocollo noto */
    switch (detectedProtocol) {
        case PROTO_RAISE:
            if (frameBuf[0] == 0xFD) decodeRaiseFrame(frameBuf, frameLen);
            break;
        case PROTO_RZC04:
            if (frameBuf[0] == 0x2E) decodeRzcFrame(frameBuf, frameLen);
            break;
        case PROTO_HIWORLD:
            if (frameBuf[0] == 0x5A) decodeHiworldFrame(frameBuf, frameLen);
            break;
        default:
            break;
    }

    frameLen = 0;
}

/**
 * Stampa statistiche della sessione corrente.
 * @since 07/04/26 Mattia Alesi
 */
void printStats() {
    Serial.println();
    Serial.println(F("# === STATISTICHE CANBOX ==="));
    Serial.print(F("# Baud rate: "));
    Serial.println(currentBaud);
    Serial.print(F("# Protocollo: "));
    switch (detectedProtocol) {
        case PROTO_RZC04:   Serial.println(F("RZC-04")); break;
        case PROTO_RAISE:   Serial.println(F("Raise")); break;
        case PROTO_HIWORLD: Serial.println(F("Hiworld")); break;
        default:            Serial.println(F("sconosciuto")); break;
    }
    Serial.print(F("# Byte totali: "));
    Serial.println(totalBytes);
    Serial.print(F("# Frame totali: "));
    Serial.println(totalFrames);
    Serial.print(F("# Header RZC (0x2E): "));
    Serial.println(headerCount_RZC);
    Serial.print(F("# Header Raise (0xFD): "));
    Serial.println(headerCount_Raise);
    Serial.print(F("# Header Hiworld (0x5A 0xA5): "));
    Serial.println(headerCount_Hiworld);
    Serial.println(F("# === FINE ==="));
    Serial.println();
}

/**
 * Gestisce i comandi ricevuti dal Serial Monitor USB.
 *   s = statistiche
 *   b = forza baud rate (attende un numero sulla seriale)
 *   p = reset rilevamento protocollo
 * @since 07/04/26 Mattia Alesi
 */
void handleSerialCommands() {
    while (Serial.available()) {
        char c = Serial.read();
        switch (c) {
            case 's':
            case 'S':
                printStats();
                break;
            case 'b':
            case 'B': {
                Serial.println(F("# Inserisci baud rate (es. 38400):"));
                delay(3000);
                if (Serial.available()) {
                    long newBaud = Serial.parseInt();
                    if (newBaud > 0) {
                        currentBaud = newBaud;
                        canboxSerial.end();
                        canboxSerial.begin(currentBaud);
                        Serial.print(F("# Baud rate forzato a: "));
                        Serial.println(currentBaud);
                    }
                }
                break;
            }
            case 'p':
            case 'P':
                detectedProtocol = PROTO_UNKNOWN;
                headerCount_RZC = 0;
                headerCount_Raise = 0;
                headerCount_Hiworld = 0;
                Serial.println(F("# Rilevamento protocollo resettato"));
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
    Serial.println(F("# CAN Box Serial Sniffer - Audi A5 B8"));
    Serial.println(F("# D1 Mini — SoftwareSerial su D2 (GPIO4)"));
    Serial.println(F("# ============================================"));
    Serial.println(F("#"));
    Serial.println(F("# Comandi: s=statistiche  b=forza baud  p=reset protocollo"));
    Serial.println(F("#"));

    if (autoDetectBaud()) {
        Serial.println(F("# In ascolto... i frame saranno stampati in hex."));
        Serial.println(F("# Se un protocollo viene riconosciuto, i frame saranno anche decodificati."));
        Serial.println(F("#"));
    }
}

void loop() {
    if (currentBaud == 0) {
        Serial.println(F("# Riprovo auto-detect..."));
        if (!autoDetectBaud()) {
            delay(5000);
        }
        return;
    }

    /** Leggi byte dalla SoftwareSerial del CAN box e accumulali nel frame buffer */
    while (canboxSerial.available()) {
        uint8_t b = canboxSerial.read();
        totalBytes++;

        analyzeByteForProtocol(b);

        if (frameLen < FRAME_BUF_SIZE) {
            frameBuf[frameLen++] = b;
        }
        lastByteTime = millis();
    }

    /** Se il buffer ha dati e non arrivano piu' byte, processa il frame */
    if (frameLen > 0 && (millis() - lastByteTime) > FRAME_TIMEOUT_MS) {
        processFrame();
    }

    /** Controlla periodicamente se il protocollo è stato identificato */
    if (totalBytes > 0 && totalBytes % 100 == 0) {
        checkProtocolDetection();
    }

    handleSerialCommands();
    yield();
}
