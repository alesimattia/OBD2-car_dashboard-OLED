/**
 * Primitive del protocollo OBD2 su CAN (ISO 15765-4, SAE J1979) indipendenti dal trasporto:
 * bitmap dei PID supportati, riassemblaggio ISO-TP, lettura delle righe ELM327 con header,
 * decodifica di DTC, risposte negative, risposte multi-PID, boost 0x70, OBFCM e nome centralina.
 * Non dipende da Arduino, quindi compila anche su PC.
 *
 * Usato da: obd_scan.h, obd_live.h, CANbus_conn.ino, WIFI_conn.ino
 *
 * @Mattia Alesi
 */

#ifndef OBD_PROTO_H
#define OBD_PROTO_H

#include <math.h>
#include <stdint.h>
#include <string.h>

// Con non32xfer=fast (core 3.1.2) un accesso diretto a 1-2 byte in flash va in eccezione: si legge solo con queste
#if defined(ARDUINO)
#include <pgmspace.h>
#define OBD_PROGMEM PROGMEM
#define OBD_READ_BYTE(p) pgm_read_byte(p)
#define OBD_READ_WORD(p) pgm_read_word(p)
#define OBD_MEMCPY(dst, src, size) memcpy_P((dst), (src), (size))
#else
#define OBD_PROGMEM
#define OBD_READ_BYTE(p) (*(const uint8_t *)(p))
#define OBD_READ_WORD(p) (*(const uint16_t *)(p))
#define OBD_MEMCPY(dst, src, size) memcpy((dst), (src), (size))
#endif

// ISO 15765-4: al massimo 8 centraline OBD, risposte su 0x7E8-0x7EF, richiesta funzionale su 0x7DF
static constexpr uint8_t OBD_MAX_ECUS = 8;
static constexpr uint16_t OBD_FUNCTIONAL_REQUEST_ID = 0x7DF;
static constexpr uint16_t OBD_FIRST_RESPONSE_ID = 0x7E8;
static constexpr uint16_t OBD_LAST_RESPONSE_ID = 0x7EF;
static constexpr uint16_t OBD_ENGINE_RESPONSE_ID = 0x7E8;
// Messaggio più lungo gestito: 6 PID con i loro dati o i 41 byte dei PID AECD, più l'intestazione
static constexpr uint16_t OBD_MAX_MESSAGE_LEN = 72;
// Una richiesta Mode 01 può contenere al massimo 6 PID (SAE J1979, ISO 15031-5)
static constexpr uint8_t OBD_MAX_PIDS_PER_REQUEST = 6;
static constexpr uint8_t OBD_NRC_RESPONSE_PENDING = 0x78;
static constexpr uint8_t OBD_NRC_CONDITIONS_NOT_CORRECT = 0x22;
// Tetto complessivo delle attese rinnovate dalle risposte 7F xx 78: una centralina che le ripete non blocca il loop
static constexpr uint16_t OBD_RESPONSE_PENDING_CAP_MS = 10000;

// ID CAN a 29 bit: flag al bit 31, la stessa convenzione di readMsgBuf()/sendMsgBuf() di mcp_can
static constexpr uint32_t OBD_ID_EXTENDED = 0x80000000UL;
static constexpr uint32_t OBD_FUNCTIONAL_REQUEST_ID_29 = OBD_ID_EXTENDED | 0x18DB33F1UL;

// Modi di ricezione di obdTransact(): quali ID accettare come risposta e dove mandare il flow control
static constexpr uint8_t OBD_RX_OBD11 = 0; // OBD 11 bit: risposte 7E8-7EF, flow control a risposta - 8
static constexpr uint8_t OBD_RX_UDS11 = 1; // UDS 11 bit: risposte 700-7FF diverse dalla richiesta, flow control alla richiesta
static constexpr uint8_t OBD_RX_OBD29 = 2; // OBD 29 bit: risposte 18DAF1xx, flow control a 18DAxxF1
static constexpr uint8_t OBD_RX_UDS29 = 3; // UDS 29 bit VAG: risposte 17FE00xx, flow control alla richiesta

static constexpr uint8_t OBD_ISOTP_IDLE = 0;
static constexpr uint8_t OBD_ISOTP_RECEIVING = 1;
static constexpr uint8_t OBD_ISOTP_COMPLETE = 2;
static constexpr uint8_t OBD_ISOTP_ERROR = 3;

// DTC conservati per tipo (confermati Mode 03, pending Mode 07, permanenti Mode 0A) nel riepilogo della scansione
static constexpr uint8_t OBD_SCAN_DTC_KINDS = 3;
static constexpr uint8_t OBD_SCAN_DTC_PER_KIND = 6;

/**
 * Risultato della scansione completa per una centralina OBD: bitmap dei PID, freeze frame, monitor e InfoType
 * dichiarati, PID che hanno risposto, nome, raggiungibilità e DTC per tipo.
 */
struct EcuScanResult
{
	uint16_t ecuId;             // ID di risposta 0x7E8..0x7EF
	uint8_t mode01[32];         // bitmap PID 0x01-0xFF dichiarati
	uint8_t mode01Verified[32]; // bitmap PID che hanno risposto
	uint8_t mode02[32];         // bitmap PID del freeze frame 0 dichiarati
	uint8_t mode06[32];         // bitmap monitor Mode 06 dichiarati
	uint8_t mode09[32];         // bitmap InfoType dichiarate
	char name[21];              // nome da InfoType 0x0A, vuoto se non letto
	bool respondsFunctional;    // risponde alle richieste funzionali 7DF
	bool respondsPhysical;      // risponde anche alle richieste indirizzate a ecuId - 8
	bool scaleFrom4F;           // 4F dichiara massimi non predefiniti per lambda, sonde e MAP
	bool scaleFrom50;           // 50 dichiara un massimo non predefinito per il MAF
	uint8_t dtcRead;            // bit k a 1 se il servizio DTC di tipo k (03, 07, 0A) ha risposto in positivo
	uint8_t dtcCount[OBD_SCAN_DTC_KINDS];
	uint16_t dtc[OBD_SCAN_DTC_KINDS][OBD_SCAN_DTC_PER_KIND];
};

/** Stato di ricezione ISO-TP di un messaggio proveniente da una singola centralina. */
struct ObdIsoTpRx
{
	uint32_t ecuId; // ID di risposta, con OBD_ID_EXTENDED se a 29 bit
	uint16_t expectedLen;
	uint16_t receivedLen;   // byte memorizzati in data, al massimo OBD_MAX_MESSAGE_LEN
	uint16_t totalReceived; // byte ricevuti, compresi quelli oltre la capacità
	uint8_t nextSequence;
	uint8_t state;
	bool truncated;
	uint8_t data[OBD_MAX_MESSAGE_LEN];
};

/** Raccolta delle risposte di più centraline alla stessa richiesta, una slot per ECU. */
struct ObdCollector
{
	ObdIsoTpRx slots[OBD_MAX_ECUS];
	uint8_t used;
};

/** Un PID estratto da una risposta Mode 01 che ne contiene più d'uno. */
struct ObdPidField
{
	uint8_t pid;
	uint8_t len;
	const uint8_t *data;
};

/** Contatori OBFCM (Mode 09 InfoType 0x17): distanza e carburante recenti e di vita. */
struct ObdObfcm
{
	bool recentDistanceValid;
	bool lifetimeDistanceValid;
	bool recentFuelValid;
	bool lifetimeFuelValid;
	float recentDistanceKm;
	float lifetimeDistanceKm;
	float recentFuelL;
	float lifetimeFuelL;
	bool plausible; // consumo medio di vita fra 3 e 30 L/100 km
};

// Byte di dati di ogni PID Mode 01 secondo la tabella SAE J1979 pubblica; 0 = lunghezza non nota.
// Serve a separare i PID di una risposta multi-PID e a segnalare risposte di lunghezza anomala.
static const uint8_t OBD_PID_DATA_LEN[256] OBD_PROGMEM = {
	/* 0x00 */ 4, 4, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1, 2, 1, 1, 1,
	/* 0x10 */ 2, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2, 1, 1, 1, 2,
	/* 0x20 */ 4, 2, 2, 2, 4, 4, 4, 4, 4, 4, 4, 4, 1, 1, 1, 1,
	/* 0x30 */ 1, 2, 2, 1, 4, 4, 4, 4, 4, 4, 4, 4, 2, 2, 2, 2,
	/* 0x40 */ 4, 4, 2, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 4,
	/* 0x50 */ 4, 1, 1, 2, 2, 2, 2, 2, 2, 2, 1, 1, 1, 2, 2, 1,
	/* 0x60 */ 4, 1, 1, 2, 5, 2, 5, 3, 3, 7, 5, 5, 5, 11, 9, 3,
	/* 0x70 */ 10, 6, 5, 5, 5, 7, 7, 5, 9, 9, 7, 7, 9, 1, 1, 13,
	/* 0x80 */ 4, 41, 41, 9, 1, 10, 5, 5, 13, 41, 41, 7, 17, 1, 1, 7,
	/* 0x90 */ 3, 5, 2, 3, 12, 0, 0, 0, 9, 9, 6, 4, 17, 4, 2, 9,
	/* 0xA0 */ 4, 9, 2, 9, 4, 4, 4, 4, 4, 4, 0, 0, 0, 0, 0, 0,
	/* 0xB0 */ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	/* 0xC0 */ 4, 0, 0, 2, 8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	/* 0xD0 */ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	/* 0xE0 */ 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	/* 0xF0 */ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

/**
 * Restituisce il numero di byte di dati del PID Mode 01 secondo SAE J1979, 0 se non noto.
 * @since 23/10/26 Mattia Alesi
 */
static inline uint8_t obdPidDataLen(uint8_t pid)
{
	return OBD_READ_BYTE(&OBD_PID_DATA_LEN[pid]);
}

/** Restituisce l'ID di richiesta fisica della centralina che risponde su responseId (0x7E8 -> 0x7E0). */
static inline uint16_t obdPhysicalRequestId(uint16_t responseId)
{
	return (uint16_t)(responseId - 8);
}

/** Vero se l'ID CAN rientra nel range delle risposte OBD a 11 bit. */
static inline bool obdIsResponseId(uint32_t canId)
{
	return canId >= OBD_FIRST_RESPONSE_ID && canId <= OBD_LAST_RESPONSE_ID;
}

/**
 * Vero per gli indirizzi funzionali OBD (7DF e 18DB33F1), a cui rispondono più centraline.
 * @since 29/10/26 Mattia Alesi
 */
static inline bool obdIsFunctionalId(uint32_t canId)
{
	return canId == OBD_FUNCTIONAL_REQUEST_ID || canId == OBD_FUNCTIONAL_REQUEST_ID_29;
}

/**
 * Vero se rxId è una risposta accettabile nel modo di ricezione indicato: in OBD 11 bit le risposte 7E8-7EF, in UDS
 * 11 bit ogni ID 700-7FF diverso dalla richiesta, a 29 bit le risposte OBD 18DAF1xx o quelle VAG 17FE00xx.
 */
static inline bool obdAcceptsResponse(uint8_t receiveMode, uint32_t requestId, uint32_t rxId)
{
	const bool extended = (rxId & OBD_ID_EXTENDED) != 0;
	const uint32_t raw = rxId & 0x1FFFFFFFUL;
	switch (receiveMode)
	{
	case OBD_RX_UDS11:
		return !extended && raw >= 0x700 && raw <= 0x7FF && rxId != requestId;
	case OBD_RX_OBD29:
		return extended && (raw & 0x1FFFFF00UL) == 0x18DAF100UL;
	case OBD_RX_UDS29:
		return extended && (raw & 0x1FFFFF00UL) == 0x17FE0000UL;
	default:
		return !extended && obdIsResponseId(raw);
	}
}

/**
 * ID a cui inviare il flow control dopo il first frame ricevuto da rxId: in OBD l'indirizzo fisico della centralina
 * (risposta - 8 a 11 bit, 18DAxxF1 a 29 bit), in UDS l'ID di richiesta, perchè le coppie VAG non seguono il - 8
 * (714 risponde su 77E).
 */
static inline uint32_t obdFlowControlId(uint8_t receiveMode, uint32_t requestId, uint32_t rxId)
{
	switch (receiveMode)
	{
	case OBD_RX_UDS11:
	case OBD_RX_UDS29:
		return requestId;
	case OBD_RX_OBD29:
		return OBD_ID_EXTENDED | 0x18DA00F1UL | ((rxId & 0xFFUL) << 8);
	default:
		return obdPhysicalRequestId((uint16_t)rxId);
	}
}

/**
 * Separazione minima (STmin, ms) chiesta nel flow control: 5 ms nei modi UDS, dove le risposte VAG arrivano a decine
 * di frame e i due buffer del MCP2515 andrebbero in overflow; 0 in OBD, per non rallentare la lettura live.
 */
static inline uint8_t obdFlowControlSeparationMs(uint8_t receiveMode)
{
	return (receiveMode == OBD_RX_UDS11 || receiveMode == OBD_RX_UDS29) ? 5 : 0;
}

/** Vero per i PID che riportano una bitmap di supporto (0x00, 0x20, ..., 0xE0) invece di un dato. */
static inline bool obdIsRangePid(uint8_t id)
{
	return (id & 0x1F) == 0;
}

/**
 * Legge un bit della bitmap di supporto: il bit 7 del primo byte corrisponde all'ID 0x01,
 * come nelle risposte ai PID 0x00/0x20/... di SAE J1979. L'ID 0 è sempre supportato.
 */
static inline bool obdBitmapHas(const uint8_t *bitmap, uint8_t id)
{
	if (id == 0)
		return true;
	uint8_t index = id - 1;
	return (bitmap[index >> 3] & (0x80 >> (index & 7))) != 0;
}

/** Imposta un bit della bitmap di supporto, con la stessa numerazione di obdBitmapHas(). */
static inline void obdBitmapSet(uint8_t *bitmap, uint8_t id)
{
	if (id == 0)
		return;
	uint8_t index = id - 1;
	bitmap[index >> 3] |= (uint8_t)(0x80 >> (index & 7));
}

/** Copia i 4 byte di una risposta di supporto nella posizione del range base (0x00, 0x20, ...). */
static inline void obdBitmapStoreRange(uint8_t *bitmap, uint8_t base, const uint8_t *fourBytes)
{
	memcpy(bitmap + (base >> 3), fourBytes, 4);
}

/** Conta gli ID dichiarati fra 1 e maxId, esclusi quelli che annunciano solo il range successivo. */
static inline uint16_t obdCountDataIds(const uint8_t *bitmap, uint16_t maxId)
{
	uint16_t count = 0;
	for (uint16_t id = 1; id <= maxId; id++)
	{
		if (!obdIsRangePid((uint8_t)id) && obdBitmapHas(bitmap, (uint8_t)id))
			count++;
	}
	return count;
}

/** Azzera lo stato di ricezione e lo associa alla centralina ecuId. */
static inline void obdIsoTpReset(ObdIsoTpRx &rx, uint32_t ecuId)
{
	rx.ecuId = ecuId;
	rx.expectedLen = 0;
	rx.receivedLen = 0;
	rx.totalReceived = 0;
	rx.nextSequence = 0;
	rx.state = OBD_ISOTP_IDLE;
	rx.truncated = false;
}

/** Accoda byte al messaggio rispettando la capacità del buffer; l'eccedenza viene solo contata. */
static inline void obdIsoTpAppend(ObdIsoTpRx &rx, const uint8_t *bytes, uint16_t count)
{
	for (uint16_t i = 0; i < count; i++)
	{
		if (rx.receivedLen < OBD_MAX_MESSAGE_LEN)
			rx.data[rx.receivedLen++] = bytes[i];
		else
			rx.truncated = true;
		rx.totalReceived++;
	}
}

/**
 * Elabora un frame CAN (PCI compreso) appartenente alla centralina di rx: single frame, first frame
 * e consecutive frame, con controllo del numero di sequenza e della lunghezza dichiarata, ignorando
 * il riempimento finale. Ritorna true sul first frame, quando su CAN nativo va inviato il flow control.
 */
static inline bool obdIsoTpFeed(ObdIsoTpRx &rx, const uint8_t *frame, uint8_t frameLen)
{
	if (frameLen == 0)
		return false;
	uint8_t type = frame[0] >> 4;

	if (type == 0)
	{
		uint8_t len = frame[0] & 0x0F;
		if (len == 0 || len > 7 || len + 1 > frameLen)
			return false;
		obdIsoTpReset(rx, rx.ecuId);
		rx.expectedLen = len;
		obdIsoTpAppend(rx, frame + 1, len);
		rx.state = OBD_ISOTP_COMPLETE;
		return false;
	}

	if (type == 1)
	{
		if (frameLen < 3)
			return false;
		uint16_t total = (uint16_t)(((frame[0] & 0x0F) << 8) | frame[1]);
		if (total < 8)
			return false;
		obdIsoTpReset(rx, rx.ecuId);
		rx.expectedLen = total;
		obdIsoTpAppend(rx, frame + 2, (uint16_t)(frameLen - 2));
		rx.nextSequence = 1;
		rx.state = OBD_ISOTP_RECEIVING;
		return true;
	}

	if (type == 2)
	{
		if (rx.state != OBD_ISOTP_RECEIVING)
			return false;
		if ((frame[0] & 0x0F) != rx.nextSequence)
		{
			// Frame perso o fuori ordine (rollover RXB0/RXB1 del MCP2515): il messaggio va scartato
			rx.state = OBD_ISOTP_ERROR;
			return false;
		}
		uint16_t remaining = rx.expectedLen - rx.totalReceived;
		uint16_t available = (uint16_t)(frameLen - 1);
		obdIsoTpAppend(rx, frame + 1, available < remaining ? available : remaining);
		rx.nextSequence = (uint8_t)((rx.nextSequence + 1) & 0x0F);
		if (rx.totalReceived >= rx.expectedLen)
			rx.state = OBD_ISOTP_COMPLETE;
	}
	return false;
}

/** Svuota la raccolta prima di una nuova richiesta. */
static inline void obdCollectorReset(ObdCollector &collector)
{
	collector.used = 0;
}

/** Restituisce la slot della centralina ecuId, creandola se serve; nullptr se le slot sono finite. */
static inline ObdIsoTpRx *obdCollectorSlot(ObdCollector &collector, uint32_t ecuId)
{
	for (uint8_t i = 0; i < collector.used; i++)
	{
		if (collector.slots[i].ecuId == ecuId)
			return &collector.slots[i];
	}
	if (collector.used >= OBD_MAX_ECUS)
		return nullptr;
	ObdIsoTpRx *slot = &collector.slots[collector.used++];
	obdIsoTpReset(*slot, ecuId);
	return slot;
}

/** Restituisce il messaggio completo ricevuto da ecuId, nullptr se assente o incompleto. */
static inline const ObdIsoTpRx *obdCollectorFind(const ObdCollector &collector, uint32_t ecuId)
{
	for (uint8_t i = 0; i < collector.used; i++)
	{
		if (collector.slots[i].ecuId == ecuId && collector.slots[i].state == OBD_ISOTP_COMPLETE)
			return &collector.slots[i];
	}
	return nullptr;
}

/** Conta i messaggi completi presenti nella raccolta. */
static inline uint8_t obdCollectorCompleteCount(const ObdCollector &collector)
{
	uint8_t count = 0;
	for (uint8_t i = 0; i < collector.used; i++)
	{
		if (collector.slots[i].state == OBD_ISOTP_COMPLETE)
			count++;
	}
	return count;
}

/** Converte una cifra esadecimale nel suo valore, -1 se il carattere non è esadecimale. */
static inline int obdHexValue(char c)
{
	if (c >= '0' && c <= '9')
		return c - '0';
	if (c >= 'A' && c <= 'F')
		return c - 'A' + 10;
	if (c >= 'a' && c <= 'f')
		return c - 'a' + 10;
	return -1;
}

/**
 * Converte una riga dell'ELM327 con header attivi (ATH1) in un frame CAN, con o senza spazi: a 11 bit
 * "7E8064100BE3EB813" o "7E8 06 41 00 BE 3E B8 13 00", a 29 bit con idDigits 8 "18DAF11006410098..." o
 * "18 DA F1 10 06 41 00 ...", con OBD_ID_EXTENDED nell'ID restituito. Le righe di stato (NO DATA, STOPPED, ?, ...)
 * contengono caratteri non esadecimali e vengono scartate; a 11 bit una riga senza header ha un numero pari
 * di cifre e quindi non può essere scambiata per una con header, che ne ha 3 più un numero pari.
 */
static inline bool obdElmParseHeaderLine(const char *line, uint32_t *canId, uint8_t *frame, uint8_t *frameLen,
										 uint8_t idDigits = 3)
{
	uint8_t nibbles = 0;
	uint32_t id = 0;
	uint8_t byteCount = 0;
	int pending = -1;
	for (const char *p = line; *p != '\0'; p++)
	{
		if (*p == ' ')
			continue;
		int value = obdHexValue(*p);
		if (value < 0)
			return false;
		if (nibbles < idDigits)
		{
			id = (id << 4) | (uint32_t)value;
		}
		else if (pending < 0)
		{
			pending = value;
		}
		else
		{
			if (byteCount >= 8)
				return false;
			frame[byteCount++] = (uint8_t)((pending << 4) | value);
			pending = -1;
		}
		nibbles++;
	}
	if (nibbles < idDigits + 2 || pending >= 0)
		return false;
	*canId = idDigits > 3 ? (id | OBD_ID_EXTENDED) : id;
	*frameLen = byteCount;
	return true;
}

/**
 * Converte una riga senza header (ATH0 con CAF1, adattatore che non supporta ATH1) in un single
 * frame sintetico ricostruendo il PCI dalla lunghezza; le righe multi-frame "0:", "1:" non sono gestite.
 */
static inline bool obdElmParsePayloadLine(const char *line, uint8_t *frame, uint8_t *frameLen)
{
	uint8_t byteCount = 0;
	int pending = -1;
	for (const char *p = line; *p != '\0'; p++)
	{
		if (*p == ' ')
			continue;
		int value = obdHexValue(*p);
		if (value < 0)
			return false;
		if (pending < 0)
		{
			pending = value;
			continue;
		}
		if (byteCount >= 7)
			return false;
		frame[1 + byteCount++] = (uint8_t)((pending << 4) | value);
		pending = -1;
	}
	if (byteCount == 0 || pending >= 0)
		return false;
	frame[0] = byteCount;
	*frameLen = (uint8_t)(byteCount + 1);
	return true;
}

/** Riconosce una risposta negativa 7F <service> <NRC> e ne estrae servizio e codice. */
static inline bool obdIsNegativeResponse(const uint8_t *msg, uint16_t len, uint8_t *service, uint8_t *nrc)
{
	if (len < 3 || msg[0] != 0x7F)
		return false;
	*service = msg[1];
	*nrc = msg[2];
	return true;
}

/** Vero se il messaggio è la risposta positiva al PID o InfoType richiesto. */
static inline bool obdIsPositiveFor(const ObdIsoTpRx *msg, uint8_t service, uint8_t id)
{
	return msg && msg->receivedLen >= 2 && msg->data[0] == (uint8_t)(service + 0x40) && msg->data[1] == id;
}

/**
 * Estrae i DTC da una risposta Mode 03/07/0A su CAN: dopo il byte di servizio c'e' il conteggio
 * (ISO 15765-4), poi due byte per codice. I codici 0000 di riempimento vengono saltati.
 */
static inline uint8_t obdParseDtcs(const uint8_t *msg, uint16_t len, uint16_t *out, uint8_t maxOut)
{
	if (len < 2 || (msg[0] != 0x43 && msg[0] != 0x47 && msg[0] != 0x4A))
		return 0;
	uint16_t declared = msg[1];
	uint16_t pairs = (uint16_t)((len - 2) / 2);
	uint16_t limit = declared < pairs ? declared : pairs;
	uint8_t found = 0;
	for (uint16_t i = 0; i < limit && found < maxOut; i++)
	{
		uint16_t code = (uint16_t)((msg[2 + i * 2] << 8) | msg[3 + i * 2]);
		if (code != 0x0000)
			out[found++] = code;
	}
	return found;
}

/**
 * Estrae i 4 byte di bitmap da una risposta di supporto del servizio responseService (0x41, 0x42, 0x46 o 0x49):
 * Mode 01 e 06 hanno la forma [41 base A B C D], Mode 02 porta il numero del freeze frame prima della bitmap
 * [42 base 00 A B C D], Mode 09 può avere il byte NODI.
 */
static inline bool obdParseSupportedRange(const uint8_t *msg, uint16_t len, uint8_t responseService,
										  uint8_t base, uint8_t *fourBytes)
{
	uint16_t offset = (responseService == 0x42 || (len >= 7 && responseService == 0x49)) ? 3 : 2;
	if (len < offset + 4 || msg[0] != responseService || msg[1] != base)
		return false;
	memcpy(fourBytes, msg + offset, 4);
	return true;
}

/**
 * Separa una risposta Mode 01 multi-PID [41 pid dati pid dati ...] usando le lunghezze SAE.
 * wellFormed diventa false se la risposta contiene un PID di lunghezza ignota o è troncata.
 */
static inline uint8_t obdParseMultiPid(const uint8_t *msg, uint16_t len, ObdPidField *out, uint8_t maxOut,
									   bool *wellFormed)
{
	*wellFormed = false;
	if (len < 2 || msg[0] != 0x41)
		return 0;
	uint8_t count = 0;
	uint16_t pos = 1;
	while (pos < len && count < maxOut)
	{
		uint8_t pid = msg[pos];
		uint8_t dataLen = obdPidDataLen(pid);
		if (dataLen == 0 || pos + 1 + dataLen > len)
			return count;
		out[count].pid = pid;
		out[count].len = dataLen;
		out[count].data = msg + pos + 1;
		count++;
		pos = (uint16_t)(pos + 1 + dataLen);
	}
	*wellFormed = (pos == len);
	return count;
}

/**
 * Decodifica dal PID 0x70 la pressione del sensore boost A, assoluta in kPa (byte D-E / 32).
 * Il valore è presente solo se il bit 1 del byte A lo dichiara.
 */
static inline bool obdDecodeBoostSensorA(const uint8_t *data, uint8_t len, float *kPa)
{
	if (len < 5 || (data[0] & 0x02) == 0)
		return false;
	*kPa = (float)((data[3] << 8) | data[4]) / 32.0f;
	return true;
}

/** Legge un intero a 32 bit big-endian. */
static inline uint32_t obdReadU32(const uint8_t *p)
{
	return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

/**
 * Decodifica i contatori OBFCM da [49 17 NODI + 16 byte]: distanza recente e di vita (0,1 km),
 * carburante recente e di vita (0,01 L), FFFFFFFF = non disponibile. Layout da implementazione
 * community, quindi il consumo medio viene marcato plausibile solo fra 3 e 30 L/100 km.
 */
static inline bool obdParseObfcm(const uint8_t *msg, uint16_t len, ObdObfcm *out)
{
	if (len < 19 || msg[0] != 0x49 || msg[1] != 0x17)
		return false;
	const uint8_t *p = msg + 3;
	uint32_t raw[4];
	for (uint8_t i = 0; i < 4; i++)
		raw[i] = obdReadU32(p + i * 4);
	out->recentDistanceValid = raw[0] != 0xFFFFFFFFUL;
	out->lifetimeDistanceValid = raw[1] != 0xFFFFFFFFUL;
	out->recentFuelValid = raw[2] != 0xFFFFFFFFUL;
	out->lifetimeFuelValid = raw[3] != 0xFFFFFFFFUL;
	out->recentDistanceKm = (float)raw[0] / 10.0f;
	out->lifetimeDistanceKm = (float)raw[1] / 10.0f;
	out->recentFuelL = (float)raw[2] / 100.0f;
	out->lifetimeFuelL = (float)raw[3] / 100.0f;
	out->plausible = false;
	if (out->lifetimeDistanceValid && out->lifetimeFuelValid && out->lifetimeDistanceKm > 1.0f)
	{
		float litersPer100 = out->lifetimeFuelL / out->lifetimeDistanceKm * 100.0f;
		out->plausible = litersPer100 >= 3.0f && litersPer100 <= 30.0f;
	}
	return true;
}

/**
 * Estrae il nome della centralina da [49 0A NODI + 20 byte ASCII] ("ECM" + riempimento + "-EngineControl"),
 * scartando i byte di riempimento a zero e i caratteri non stampabili.
 */
static inline bool obdParseEcuName(const uint8_t *msg, uint16_t len, char *out, uint8_t outSize)
{
	if (outSize == 0)
		return false;
	out[0] = '\0';
	if (len < 4 || msg[0] != 0x49 || msg[1] != 0x0A)
		return false;
	uint8_t written = 0;
	for (uint16_t i = 3; i < len && written + 1 < outSize; i++)
	{
		uint8_t c = msg[i];
		if (c >= 0x20 && c <= 0x7E)
			out[written++] = (char)c;
	}
	out[written] = '\0';
	return written > 0;
}

// ============================================================
// Segnali OBDb: tipi della tabella generata da tools/obdb_select.ps1 e interprete
// ============================================================

static constexpr uint8_t OBDB_FLAG_SIGNED = 0x01;
static constexpr uint8_t OBDB_FLAG_BYTES_LSB = 0x02;
static constexpr uint8_t OBDB_FLAG_SPARK_ONLY = 0x04;
static constexpr uint8_t OBDB_KIND_MASK = 0x70;
static constexpr uint8_t OBDB_KIND_VALUE = 0x00;
static constexpr uint8_t OBDB_KIND_BOOLEAN = 0x10;
static constexpr uint8_t OBDB_KIND_ENUM = 0x20;
static constexpr uint8_t OBDB_KIND_DTC = 0x30;
static constexpr uint8_t OBDB_KIND_GATE = 0x40;
static constexpr uint8_t OBDB_NONE = 0xFF;
static constexpr uint16_t OBDB_NO_TEXT = 0xFFFF;

static constexpr uint8_t OBDB_COMMAND_SAE = 0x01;
static constexpr uint8_t OBDB_COMMAND_CAPTURED = 0x02;
static constexpr uint8_t OBDB_COMMAND_REFUSED_IN_CAPTURES = 0x04;

static constexpr uint8_t OBDB_DECODE_VALUE = 0;
static constexpr uint8_t OBDB_DECODE_NULL = 1;
static constexpr uint8_t OBDB_DECODE_NO_DATA = 2;
static constexpr uint8_t OBDB_DECODE_UNMAPPED = 3;

/** Descrittore di un segnale nella tabella PROGMEM; i campi si leggono solo dopo OBD_MEMCPY. */
struct ObdbSignalDef
{
	uint16_t bitOffset;  // bix: bit MSB0 contati dopo servizio ed eco di PID/DID
	uint16_t nameOffset; // nome nel blob dei testi, OBDB_NO_TEXT per i bit di supporto
	uint8_t bitLength;
	uint8_t flags; // OBDB_FLAG_* e OBDB_KIND_*
	uint8_t unitIndex;
	uint8_t scaleIndex;
	uint8_t limitIndex;
	uint8_t enumIndex; // OBDB_NONE se non è un'enumerazione
	uint8_t gateIndex; // segnale di supporto nello stesso comando, OBDB_NONE se nessuno
	uint8_t reserved;
};

/** Terna di scala condivisa fra i segnali: valore = raw × mul / div + add. */
struct ObdbScale
{
	double mul;
	double div;
	double add;
};

/** Limiti condivisi: min vale 0 se OBDb non lo dichiara, i limiti null assenti valgono NAN. */
struct ObdbLimit
{
	double min;
	double max;
	double nullMin;
	double nullMax;
};

/** Enumerazione: voci consecutive nella tabella delle voci. */
struct ObdbEnumDef
{
	uint16_t firstEntry;
	uint8_t count;
	uint8_t reserved;
};

/** Voce di un'enumerazione: valore grezzo e testo descrittivo. */
struct ObdbEnumEntry
{
	uint16_t raw;
	uint16_t textOffset;
};

/** Unità: simbolo, e per i booleani i testi del bit a 0 e a 1. */
struct ObdbUnitDef
{
	uint16_t symbolOffset;
	uint16_t falseOffset;
	uint16_t trueOffset;
};

/** Comando OBDb: richiesta, segnali che decodifica e provenienza. */
struct ObdbCommandDef
{
	uint32_t header;     // ID di richiesta, 0x7DF per i PID SAE
	uint16_t responseId; // ID di risposta atteso, 0 per i PID SAE
	uint16_t identifier; // PID (servizio 01) o DID (servizio 22)
	uint16_t firstSignal;
	uint8_t service;
	uint8_t signalCount;
	uint8_t flags;      // OBDB_COMMAND_*
	uint8_t cimIndex;   // selettore accensione comandata/compressione del PID 01/41, OBDB_NONE se assente
	uint8_t reserved[2];
};

/**
 * Estrae il valore grezzo di un segnale con la convenzione OBDb (python/can/signals.py): bit MSB0 da bitOffset;
 * con bytesLsb e più di 8 bit i byte del campo si leggono in ordine inverso. Senza dati sufficienti ritorna false,
 * salvo con missingAsZero, che replica le enumerazioni di OBDb, dove i bit mancanti valgono zero.
 */
static inline bool obdbExtractRaw(const uint8_t *data, uint16_t len, uint16_t bitOffset, uint8_t bitLength,
								  bool bytesLsb, bool missingAsZero, uint64_t *raw)
{
	uint32_t endBit = (uint32_t)bitOffset + bitLength;
	if (bitLength == 0 || bitLength > 64 || (!missingAsZero && endBit > (uint32_t)len * 8))
		return false;
	uint32_t startByte = bitOffset >> 3;
	uint32_t endByte = startByte + ((bitLength + 7u) >> 3);
	if (endByte > len)
		endByte = len;
	const bool reverse = bytesLsb && bitLength > 8 && endByte > startByte;
	uint64_t result = 0;
	for (uint32_t bit = bitOffset; bit < endBit; bit++)
	{
		uint32_t byteIndex = bit >> 3;
		if (reverse && byteIndex >= startByte && byteIndex < endByte)
			byteIndex = startByte + endByte - 1 - byteIndex;
		if (byteIndex < len && ((data[byteIndex] >> (7 - (bit & 7))) & 1))
			result |= (uint64_t)1 << (endBit - bit - 1);
	}
	*raw = result;
	return true;
}

/**
 * Decodifica un segnale numerico come il decoder ufficiale OBDb: complemento a due, raw × mul / div + add in double
 * (il float a 24 bit di mantissa non regge i contatori a 32 bit), clamp fra min e max quando max > min, poi i limiti
 * null con l'epsilon di Python. Ritorna OBDB_DECODE_VALUE, OBDB_DECODE_NULL o OBDB_DECODE_NO_DATA.
 */
static inline uint8_t obdbDecodeValue(const ObdbSignalDef &def, const ObdbScale &scale, const ObdbLimit &limit,
									  const uint8_t *data, uint16_t len, double *value, uint64_t *raw)
{
	if (!obdbExtractRaw(data, len, def.bitOffset, def.bitLength, (def.flags & OBDB_FLAG_BYTES_LSB) != 0, false, raw))
		return OBDB_DECODE_NO_DATA;
	double number = (double)*raw;
	if ((def.flags & OBDB_FLAG_SIGNED) && ((*raw >> (def.bitLength - 1)) & 1))
		number -= ldexp(1.0, def.bitLength);
	double result = number * scale.mul / scale.div + scale.add;
	if (limit.max > limit.min)
	{
		if (result < limit.min)
			result = limit.min;
		if (result > limit.max)
			result = limit.max;
	}
	const double pythonEpsilon = 2.220446049250313e-16;
	if (!isnan(limit.nullMin) && result <= limit.nullMin + pythonEpsilon)
		return OBDB_DECODE_NULL;
	if (!isnan(limit.nullMax) && result >= limit.nullMax - pythonEpsilon)
		return OBDB_DECODE_NULL;
	*value = result;
	return OBDB_DECODE_VALUE;
}

/**
 * Vero se il segnale va mostrato: il suo bit di supporto, se c'è, vale 1 (noyes) e, per i monitor del PID 01/41
 * che OBDb definisce solo per l'accensione comandata, il selettore CIM vale 0 (SPARK). gate e cim sono copie in RAM
 * dei descrittori, nullptr se il comando non li ha; senza dati per leggerli il valore non si mostra.
 */
static inline bool obdbSignalVisible(const ObdbSignalDef &def, const ObdbSignalDef *gate, const ObdbSignalDef *cim,
									 const uint8_t *data, uint16_t len)
{
	uint64_t raw;
	if (gate && (!obdbExtractRaw(data, len, gate->bitOffset, gate->bitLength, false, false, &raw) || raw != 1))
		return false;
	if ((def.flags & OBDB_FLAG_SPARK_ONLY) &&
		(!cim || !obdbExtractRaw(data, len, cim->bitOffset, cim->bitLength, false, false, &raw) || raw != 0))
		return false;
	return true;
}

/**
 * Cerca nella tabella PROGMEM delle voci il testo di un'enumerazione per il valore grezzo raw; ritorna l'offset
 * del testo oppure OBDB_NO_TEXT se il valore non è mappato, che OBDb considera dato assente.
 */
static inline uint16_t obdbFindEnumText(const ObdbEnumEntry *entries, const ObdbEnumDef &enumDef, uint64_t raw)
{
	for (uint8_t i = 0; i < enumDef.count; i++)
	{
		ObdbEnumEntry entry;
		OBD_MEMCPY(&entry, &entries[enumDef.firstEntry + i], sizeof(entry));
		if (entry.raw == raw)
			return entry.textOffset;
	}
	return OBDB_NO_TEXT;
}

// ============================================================
// Mode 06: risultati dei monitor con scala UASID (SAE J1979 Appendix E, trascritta da python-OBD)
// ============================================================

/** Un test di un monitor Mode 06: identificatori, scala e valori grezzi. */
struct ObdMode06Test
{
	uint8_t mid;
	uint8_t tid;
	uint8_t uasid;
	uint16_t value;
	uint16_t min;
	uint16_t max;
};

/** Scala di un UASID; unit è un testo PROGMEM. */
struct ObdUasid
{
	uint8_t id;
	bool isSigned;
	double scale;
	double offset;
	const char *unit;
};

static const char OBD_UAS_NONE[] OBD_PROGMEM = "";
static const char OBD_UAS_RPM[] OBD_PROGMEM = "rpm";
static const char OBD_UAS_KMH[] OBD_PROGMEM = "km/h";
static const char OBD_UAS_MV[] OBD_PROGMEM = "mV";
static const char OBD_UAS_V[] OBD_PROGMEM = "V";
static const char OBD_UAS_MA[] OBD_PROGMEM = "mA";
static const char OBD_UAS_A[] OBD_PROGMEM = "A";
static const char OBD_UAS_MS[] OBD_PROGMEM = "ms";
static const char OBD_UAS_S[] OBD_PROGMEM = "s";
static const char OBD_UAS_MOHM[] OBD_PROGMEM = "mOhm";
static const char OBD_UAS_OHM[] OBD_PROGMEM = "Ohm";
static const char OBD_UAS_KOHM[] OBD_PROGMEM = "kOhm";
static const char OBD_UAS_C[] OBD_PROGMEM = "\xC2\xB0" "C";
static const char OBD_UAS_KPA[] OBD_PROGMEM = "kPa";
static const char OBD_UAS_PA[] OBD_PROGMEM = "Pa";
static const char OBD_UAS_DEG[] OBD_PROGMEM = "\xC2\xB0";
static const char OBD_UAS_MHZ[] OBD_PROGMEM = "mHz";
static const char OBD_UAS_HZ[] OBD_PROGMEM = "Hz";
static const char OBD_UAS_KHZ[] OBD_PROGMEM = "kHz";
static const char OBD_UAS_KM[] OBD_PROGMEM = "km";
static const char OBD_UAS_MV_MS[] OBD_PROGMEM = "mV/ms";
static const char OBD_UAS_GS[] OBD_PROGMEM = "g/s";
static const char OBD_UAS_PAS[] OBD_PROGMEM = "Pa/s";
static const char OBD_UAS_KGH[] OBD_PROGMEM = "kg/h";
static const char OBD_UAS_G[] OBD_PROGMEM = "g";
static const char OBD_UAS_G_CYL[] OBD_PROGMEM = "g/cilindro";
static const char OBD_UAS_MG_STROKE[] OBD_PROGMEM = "mg/corsa";
static const char OBD_UAS_PCT[] OBD_PROGMEM = "%";
static const char OBD_UAS_L[] OBD_PROGMEM = "L";
static const char OBD_UAS_IN[] OBD_PROGMEM = "in";
static const char OBD_UAS_MIN[] OBD_PROGMEM = "min";
static const char OBD_UAS_US[] OBD_PROGMEM = "\xC2\xB5s";
static const char OBD_UAS_MM2[] OBD_PROGMEM = "mm\xC2\xB2";
static const char OBD_UAS_PPM[] OBD_PROGMEM = "ppm";
static const char OBD_UAS_UA[] OBD_PROGMEM = "\xC2\xB5" "A";
static const char OBD_UAS_MV_S[] OBD_PROGMEM = "mV/s";

// 0x2E (vero/falso) non ha scala numerica e viene gestito a parte da obdUasidValue()
static const ObdUasid OBD_UASID_TABLE[] OBD_PROGMEM = {
	{0x01, false, 1, 0, OBD_UAS_NONE}, {0x02, false, 0.1, 0, OBD_UAS_NONE}, {0x03, false, 0.01, 0, OBD_UAS_NONE},
	{0x04, false, 0.001, 0, OBD_UAS_NONE}, {0x05, false, 0.0000305, 0, OBD_UAS_NONE},
	{0x06, false, 0.000305, 0, OBD_UAS_NONE}, {0x07, false, 0.25, 0, OBD_UAS_RPM}, {0x08, false, 0.01, 0, OBD_UAS_KMH},
	{0x09, false, 1, 0, OBD_UAS_KMH}, {0x0A, false, 0.122, 0, OBD_UAS_MV}, {0x0B, false, 0.001, 0, OBD_UAS_V},
	{0x0C, false, 0.01, 0, OBD_UAS_V}, {0x0D, false, 0.00390625, 0, OBD_UAS_MA}, {0x0E, false, 0.001, 0, OBD_UAS_A},
	{0x0F, false, 0.01, 0, OBD_UAS_A}, {0x10, false, 1, 0, OBD_UAS_MS}, {0x11, false, 100, 0, OBD_UAS_MS},
	{0x12, false, 1, 0, OBD_UAS_S}, {0x13, false, 1, 0, OBD_UAS_MOHM}, {0x14, false, 1, 0, OBD_UAS_OHM},
	{0x15, false, 1, 0, OBD_UAS_KOHM}, {0x16, false, 0.1, -40.0, OBD_UAS_C}, {0x17, false, 0.01, 0, OBD_UAS_KPA},
	{0x18, false, 0.0117, 0, OBD_UAS_KPA}, {0x19, false, 0.079, 0, OBD_UAS_KPA}, {0x1A, false, 1, 0, OBD_UAS_KPA},
	{0x1B, false, 10, 0, OBD_UAS_KPA}, {0x1C, false, 0.01, 0, OBD_UAS_DEG}, {0x1D, false, 0.5, 0, OBD_UAS_DEG},
	{0x1E, false, 0.0000305, 0, OBD_UAS_NONE}, {0x1F, false, 0.05, 0, OBD_UAS_NONE},
	{0x20, false, 0.00390625, 0, OBD_UAS_NONE}, {0x21, false, 1, 0, OBD_UAS_MHZ}, {0x22, false, 1, 0, OBD_UAS_HZ},
	{0x23, false, 1, 0, OBD_UAS_KHZ}, {0x24, false, 1, 0, OBD_UAS_NONE}, {0x25, false, 1, 0, OBD_UAS_KM},
	{0x26, false, 0.1, 0, OBD_UAS_MV_MS}, {0x27, false, 0.01, 0, OBD_UAS_GS}, {0x28, false, 1, 0, OBD_UAS_GS},
	{0x29, false, 0.25, 0, OBD_UAS_PAS}, {0x2A, false, 0.001, 0, OBD_UAS_KGH}, {0x2B, false, 1, 0, OBD_UAS_NONE},
	{0x2C, false, 0.01, 0, OBD_UAS_G_CYL}, {0x2D, false, 0.01, 0, OBD_UAS_MG_STROKE},
	{0x2E, false, 1, 0, OBD_UAS_NONE}, {0x2F, false, 0.01, 0, OBD_UAS_PCT}, {0x30, false, 0.001526, 0, OBD_UAS_PCT},
	{0x31, false, 0.001, 0, OBD_UAS_L}, {0x32, false, 0.0000305, 0, OBD_UAS_IN},
	{0x33, false, 0.00024414, 0, OBD_UAS_NONE}, {0x34, false, 1, 0, OBD_UAS_MIN}, {0x35, false, 10, 0, OBD_UAS_MS},
	{0x36, false, 0.01, 0, OBD_UAS_G}, {0x37, false, 0.1, 0, OBD_UAS_G}, {0x38, false, 1, 0, OBD_UAS_G},
	{0x39, false, 0.01, -327.68, OBD_UAS_PCT}, {0x3A, false, 0.001, 0, OBD_UAS_G}, {0x3B, false, 0.0001, 0, OBD_UAS_G},
	{0x3C, false, 0.1, 0, OBD_UAS_US}, {0x3D, false, 0.01, 0, OBD_UAS_MA},
	{0x3E, false, 0.00006103516, 0, OBD_UAS_MM2}, {0x3F, false, 0.01, 0, OBD_UAS_L}, {0x40, false, 1, 0, OBD_UAS_PPM},
	{0x41, false, 0.01, 0, OBD_UAS_UA}, {0x81, true, 1, 0, OBD_UAS_NONE}, {0x82, true, 0.1, 0, OBD_UAS_NONE},
	{0x83, true, 0.01, 0, OBD_UAS_NONE}, {0x84, true, 0.001, 0, OBD_UAS_NONE}, {0x85, true, 0.0000305, 0, OBD_UAS_NONE},
	{0x86, true, 0.000305, 0, OBD_UAS_NONE}, {0x87, true, 1, 0, OBD_UAS_PPM}, {0x8A, true, 0.122, 0, OBD_UAS_MV},
	{0x8B, true, 0.001, 0, OBD_UAS_V}, {0x8C, true, 0.01, 0, OBD_UAS_V}, {0x8D, true, 0.00390625, 0, OBD_UAS_MA},
	{0x8E, true, 0.001, 0, OBD_UAS_A}, {0x90, true, 1, 0, OBD_UAS_MS}, {0x96, true, 0.1, 0, OBD_UAS_C},
	{0x99, true, 0.1, 0, OBD_UAS_KPA}, {0x9C, true, 0.01, 0, OBD_UAS_DEG}, {0x9D, true, 0.5, 0, OBD_UAS_DEG},
	{0xA8, true, 1, 0, OBD_UAS_GS}, {0xA9, true, 0.25, 0, OBD_UAS_PAS}, {0xAD, true, 0.01, 0, OBD_UAS_MG_STROKE},
	{0xAE, true, 0.1, 0, OBD_UAS_MG_STROKE}, {0xAF, true, 0.01, 0, OBD_UAS_PCT}, {0xB0, true, 0.003052, 0, OBD_UAS_PCT},
	{0xB1, true, 2, 0, OBD_UAS_MV_S}, {0xFC, true, 0.01, 0, OBD_UAS_KPA}, {0xFD, true, 0.001, 0, OBD_UAS_KPA},
	{0xFE, true, 0.25, 0, OBD_UAS_PA}};

/** Cerca la scala di un UASID; ritorna false se lo standard (nella trascrizione di python-OBD) non lo definisce. */
static inline bool obdUasidLookup(uint8_t uasid, ObdUasid *out)
{
	for (size_t i = 0; i < sizeof(OBD_UASID_TABLE) / sizeof(OBD_UASID_TABLE[0]); i++)
	{
		if (OBD_READ_BYTE(&OBD_UASID_TABLE[i].id) != uasid)
			continue;
		OBD_MEMCPY(out, &OBD_UASID_TABLE[i], sizeof(*out));
		return true;
	}
	return false;
}

/** Applica la scala UASID a un valore grezzo a 16 bit: complemento a due per gli UASID con segno, poi scala e offset. */
static inline double obdUasidValue(const ObdUasid &uas, uint16_t raw)
{
	if (uas.id == 0x2E)
		return raw != 0 ? 1.0 : 0.0;
	double number = uas.isSigned ? (double)(int16_t)raw : (double)raw;
	return number * uas.scale + uas.offset;
}

/**
 * Separa una risposta Mode 06 su CAN [46 (MID TID UASID valore min max)...] nei test da 9 byte; un blocco
 * incompleto in coda (risposta troncata) viene scartato. Ritorna il numero di test.
 */
static inline uint8_t obdParseMode06(const uint8_t *msg, uint16_t len, ObdMode06Test *out, uint8_t maxOut)
{
	if (len < 1 || msg[0] != 0x46)
		return 0;
	uint8_t count = 0;
	for (uint16_t pos = 1; pos + 9 <= len && count < maxOut; pos = (uint16_t)(pos + 9))
	{
		out[count].mid = msg[pos];
		out[count].tid = msg[pos + 1];
		out[count].uasid = msg[pos + 2];
		out[count].value = (uint16_t)((msg[pos + 3] << 8) | msg[pos + 4]);
		out[count].min = (uint16_t)((msg[pos + 5] << 8) | msg[pos + 6]);
		out[count].max = (uint16_t)((msg[pos + 7] << 8) | msg[pos + 8]);
		count++;
	}
	return count;
}

/**
 * Esito di un test Mode 06: 1 superato, 0 fallito, -1 non determinabile (UASID non definito). Il confronto usa
 * i valori scalati, quindi è con segno per gli UASID da 0x81.
 */
static inline int8_t obdMode06Passed(const ObdMode06Test &test)
{
	ObdUasid uas;
	if (!obdUasidLookup(test.uasid, &uas))
		return -1;
	double value = obdUasidValue(uas, test.value);
	return (value >= obdUasidValue(uas, test.min) && value <= obdUasidValue(uas, test.max)) ? 1 : 0;
}

// ============================================================
// UDS: conteggio DTC, date e testi di identificazione
// ============================================================

/** Legge il numero di DTC da una risposta positiva 59 01 [maschera disponibile, formato, numero a 16 bit]. */
static inline bool obdParseDtcCount(const uint8_t *msg, uint16_t len, uint16_t *count)
{
	if (len < 6 || msg[0] != 0x59 || msg[1] != 0x01)
		return false;
	*count = (uint16_t)((msg[4] << 8) | msg[5]);
	return true;
}

/** Vero se il byte è una coppia di cifre BCD valide. */
static inline bool obdIsBcdByte(uint8_t value)
{
	return (value >> 4) <= 9 && (value & 0x0F) <= 9;
}

/**
 * Scrive in out come AAAA-MM-GG una data BCD di identificazione UDS su 3 byte (AA MM GG) o 4 (AAAA MM GG); ritorna
 * false se i byte non sono BCD o il mese e il giorno non sono plausibili.
 */
static inline bool obdDecodeBcdDate(const uint8_t *data, uint16_t len, char *out, size_t size)
{
	if ((len != 3 && len != 4) || size < 11)
		return false;
	for (uint16_t i = 0; i < len; i++)
	{
		if (!obdIsBcdByte(data[i]))
			return false;
	}
	unsigned year = (len == 4) ? (unsigned)((data[0] >> 4) * 1000 + (data[0] & 0x0F) * 100 + (data[1] >> 4) * 10 + (data[1] & 0x0F))
							   : 2000u + (unsigned)((data[0] >> 4) * 10 + (data[0] & 0x0F));
	uint8_t month = (uint8_t)((data[len - 2] >> 4) * 10 + (data[len - 2] & 0x0F));
	uint8_t day = (uint8_t)((data[len - 1] >> 4) * 10 + (data[len - 1] & 0x0F));
	if (month < 1 || month > 12 || day < 1 || day > 31)
		return false;
	out[0] = (char)('0' + year / 1000 % 10);
	out[1] = (char)('0' + year / 100 % 10);
	out[2] = (char)('0' + year / 10 % 10);
	out[3] = (char)('0' + year % 10);
	out[4] = '-';
	out[5] = (char)('0' + month / 10);
	out[6] = (char)('0' + month % 10);
	out[7] = '-';
	out[8] = (char)('0' + day / 10);
	out[9] = (char)('0' + day % 10);
	out[10] = '\0';
	return true;
}

/**
 * Copia in out un DID di identificazione se è testo: caratteri stampabili, con riempimento finale di zeri, spazi
 * o 0xFF ammesso e tolto. Ritorna false per un contenuto binario, che non si mostra.
 */
static inline bool obdCopyPrintableText(const uint8_t *data, uint16_t len, char *out, size_t size)
{
	if (size == 0)
		return false;
	uint16_t end = len;
	while (end > 0 && (data[end - 1] == 0x00 || data[end - 1] == 0x20 || data[end - 1] == 0xFF))
		end--;
	if (end == 0)
		return false;
	size_t written = 0;
	for (uint16_t i = 0; i < end; i++)
	{
		if (data[i] < 0x20 || data[i] > 0x7E)
			return false;
		if (written + 1 < size)
			out[written++] = (char)data[i];
	}
	out[written] = '\0';
	return true;
}

#endif
