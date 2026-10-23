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

#include <stdint.h>
#include <string.h>

#if defined(ARDUINO)
#include <pgmspace.h>
#define OBD_PROGMEM PROGMEM
#define OBD_READ_BYTE(p) pgm_read_byte(p)
#else
#define OBD_PROGMEM
#define OBD_READ_BYTE(p) (*(const uint8_t *)(p))
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

static constexpr uint8_t OBD_ISOTP_IDLE = 0;
static constexpr uint8_t OBD_ISOTP_RECEIVING = 1;
static constexpr uint8_t OBD_ISOTP_COMPLETE = 2;
static constexpr uint8_t OBD_ISOTP_ERROR = 3;

/**
 * Risultato della discovery di una centralina: PID Mode 01 e InfoType Mode 09 dichiarati nelle
 * bitmap di supporto e, dopo la scansione approfondita, quelli che hanno risposto davvero.
 */
struct EcuScanResult
{
	uint16_t ecuId;             // ID di risposta 0x7E8..0x7EF
	uint8_t mode01[32];         // bitmap PID 0x01-0xFF dichiarati
	uint8_t mode01Verified[32]; // bitmap PID che hanno risposto alla scansione approfondita
	uint8_t mode09[8];          // bitmap InfoType 0x01-0x40 dichiarate
	uint8_t mode09Verified[8];  // bitmap InfoType che hanno risposto alla scansione approfondita
	char name[21];              // nome da InfoType 0x0A, vuoto se non letto
	bool respondsPhysical;      // risponde anche alle richieste indirizzate a ecuId - 8
	bool probed;                // scansione approfondita eseguita
};

/** Stato di ricezione ISO-TP di un messaggio proveniente da una singola centralina. */
struct ObdIsoTpRx
{
	uint16_t ecuId;
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
static inline void obdIsoTpReset(ObdIsoTpRx &rx, uint16_t ecuId)
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
static inline ObdIsoTpRx *obdCollectorSlot(ObdCollector &collector, uint16_t ecuId)
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
static inline const ObdIsoTpRx *obdCollectorFind(const ObdCollector &collector, uint16_t ecuId)
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
 * Converte una riga dell'ELM327 con header attivi (ATH1) in un frame CAN a 11 bit, con o senza spazi:
 * "7E8064100BE3EB813" o "7E8 06 41 00 BE 3E B8 13 00". Le righe di stato (NO DATA, STOPPED, ?, ...)
 * contengono caratteri non esadecimali e vengono scartate; una riga senza header ha un numero pari
 * di cifre e quindi non può essere scambiata per una con header, che ne ha 3 più un numero pari.
 */
static inline bool obdElmParseHeaderLine(const char *line, uint16_t *canId, uint8_t *frame, uint8_t *frameLen)
{
	uint8_t nibbles = 0;
	uint16_t id = 0;
	uint8_t byteCount = 0;
	int pending = -1;
	for (const char *p = line; *p != '\0'; p++)
	{
		if (*p == ' ')
			continue;
		int value = obdHexValue(*p);
		if (value < 0)
			return false;
		if (nibbles < 3)
		{
			id = (uint16_t)((id << 4) | value);
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
	if (nibbles < 5 || pending >= 0)
		return false;
	*canId = id;
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
 * Estrae i 4 byte di bitmap da una risposta di supporto del servizio responseService (0x41 o 0x49):
 * Mode 01 ha la forma [41 base A B C D], Mode 09 può avere anche il byte NODI prima della bitmap.
 */
static inline bool obdParseSupportedRange(const uint8_t *msg, uint16_t len, uint8_t responseService,
										  uint8_t base, uint8_t *fourBytes)
{
	if (len < 6 || msg[0] != responseService || msg[1] != base)
		return false;
	uint16_t offset = (len >= 7 && responseService == 0x49) ? 3 : 2;
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

#endif
