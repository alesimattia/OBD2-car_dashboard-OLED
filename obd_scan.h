/**
 * Discovery OBD2 condivisa dalle due build: centraline che rispondono, PID Mode 01 e InfoType Mode 09
 * dichiarati da ciascuna, nome della centralina, contatori OBFCM, capacità del motore (indirizzamento
 * fisico, richieste multi-PID, coppia di riferimento) e scansione approfondita su richiesta dal web,
 * con il report su Serial e il JSON della pagina /scan.
 *
 * Il trasporto è fornito dallo sketch tramite obdTransact() (CAN nativo o ELM327). Va incluso dopo
 * serial_logger.h, perchè scrive su Serial.
 *
 * Usato da: CANbus_conn.ino, WIFI_conn.ino, obd_live.h, web_dashboard.h
 *
 * @Mattia Alesi
 */

#ifndef OBD_SCAN_H
#define OBD_SCAN_H

#include <Arduino.h>
#include <ESP8266WebServer.h>
#include <stdarg.h>
#include "obd_proto.h"
#include "pid_descriptions.h"

/**
 * Invia una richiesta single frame e raccoglie in obdCollector le risposte: requestId 0x7DF interroga
 * tutte le centraline, 0x7E0..0x7E7 una sola. Con waitForEcuId diverso da zero ritorna appena il
 * messaggio di quella centralina è completo; windowMs è la finestra di ascolto del CAN nativo, mentre
 * l'ELM327 applica i propri tempi. Ritorna il numero di messaggi completi.
 * Implementata in CANbus_conn.ino e WIFI_conn.ino.
 */
uint8_t obdTransact(uint16_t requestId, const uint8_t *request, uint8_t requestLen, uint16_t windowMs,
					uint16_t waitForEcuId);

/** Mostra l'avanzamento dello scan sul display; implementata in CANbus_conn.ino e WIFI_conn.ino. */
void obdShowScanProgress(const char *status, const char *detail);

// P2CAN è 50 ms (ISO 15765-4): 150 ms coprono anche la latenza del gateway allo scan, 100 ms a regime
static constexpr uint16_t OBD_SCAN_WINDOW_MS = 150;
static constexpr uint16_t OBD_LIVE_WINDOW_MS = 100;
// Scansione approfondita: ID fisici 0x7E0-0x7E7, PID Mode 01 0x01-0xFF, InfoType Mode 09 0x01-0x1F
static constexpr uint16_t OBD_PROBE_TOTAL_STEPS = 8 + 255 + 31;

static constexpr uint8_t OBD_PROBE_IDLE = 0;
static constexpr uint8_t OBD_PROBE_PHYSICAL = 1;
static constexpr uint8_t OBD_PROBE_MODE01 = 2;
static constexpr uint8_t OBD_PROBE_MODE09 = 3;
static constexpr uint8_t OBD_PROBE_DONE = 4;

static constexpr uint8_t OBD_SCAN_OK = 0;
static constexpr uint8_t OBD_SCAN_NO_ECU = 1;
static constexpr uint8_t OBD_SCAN_NO_ENGINE = 2;

/** Avanzamento della scansione approfondita, eseguita un passo per ogni giro di loop(). */
struct ObdProbeState
{
	uint8_t phase;
	uint16_t step;
	unsigned long startedMs;
	unsigned long durationMs;
};

inline ObdCollector obdCollector;
inline EcuScanResult ecuResults[OBD_MAX_ECUS];
inline uint8_t ecuCount = 0;
inline int8_t engineEcuIndex = -1;
// Capacità del trasporto: sul clone ELM327 senza ATSH o senza ATH1 vengono messe a false dallo sketch
inline bool obdTransportPhysicalAvailable = true;
inline bool obdTransportHeadersAvailable = true;
// Capacità del motore rilevate allo scan
inline bool engineMultiPid = false;
inline float engineReferenceTorqueNm = NAN;
inline bool obfcmAvailable = false;
inline uint16_t obfcmEcuId = 0;
inline ObdObfcm obfcmData;
inline uint8_t obfcmRaw[OBD_MAX_MESSAGE_LEN];
inline uint16_t obfcmRawLen = 0;
// Vero durante scan e scansione approfondita: gli handler web non devono interrogare il bus
inline bool obdBusy = false;
inline ObdProbeState obdProbe = {OBD_PROBE_IDLE, 0, 0, 0};

static const char OBD_ECU_ENGINE[] PROGMEM = "Motore";
static const char OBD_ECU_GEARBOX[] PROGMEM = "Cambio";

// InfoType Mode 09 della tabella SAE J1979 pubblica, più 0x17 OBFCM (regolamento UE 2018/1832)
static const char OBD_IT_01[] PROGMEM = "Conteggio messaggi VIN";
static const char OBD_IT_02[] PROGMEM = "VIN (telaio)";
static const char OBD_IT_03[] PROGMEM = "Conteggio ID calibrazione";
static const char OBD_IT_04[] PROGMEM = "ID calibrazione software";
static const char OBD_IT_05[] PROGMEM = "Conteggio CVN";
static const char OBD_IT_06[] PROGMEM = "CVN (verifica calibrazione)";
static const char OBD_IT_07[] PROGMEM = "Conteggio IPT";
static const char OBD_IT_08[] PROGMEM = "Contatori monitor benzina (IPT)";
static const char OBD_IT_09[] PROGMEM = "Conteggio nome ECU";
static const char OBD_IT_0A[] PROGMEM = "Nome centralina";
static const char OBD_IT_0B[] PROGMEM = "Contatori monitor diesel (IPT)";
static const char OBD_IT_17[] PROGMEM = "Consumo reale di vita (OBFCM)";

/** Restituisce il nome PROGMEM di una InfoType Mode 09, nullptr se non documentata. */
inline const char *obdInfoTypeName(uint8_t infoType)
{
	switch (infoType)
	{
	case 0x01: return OBD_IT_01;
	case 0x02: return OBD_IT_02;
	case 0x03: return OBD_IT_03;
	case 0x04: return OBD_IT_04;
	case 0x05: return OBD_IT_05;
	case 0x06: return OBD_IT_06;
	case 0x07: return OBD_IT_07;
	case 0x08: return OBD_IT_08;
	case 0x09: return OBD_IT_09;
	case 0x0A: return OBD_IT_0A;
	case 0x0B: return OBD_IT_0B;
	case 0x17: return OBD_IT_17;
	default: return nullptr;
	}
}

/** Restituisce la bitmap di supporto del servizio indicato (0x01 o 0x09) della centralina. */
inline uint8_t *obdServiceBitmap(EcuScanResult &ecu, uint8_t service, bool verified)
{
	if (service == 0x01)
		return verified ? ecu.mode01Verified : ecu.mode01;
	return verified ? ecu.mode09Verified : ecu.mode09;
}

/** Cerca la centralina per ID di risposta; -1 se non è stata rilevata. */
inline int8_t obdFindEcu(uint16_t ecuId)
{
	for (uint8_t i = 0; i < ecuCount; i++)
	{
		if (ecuResults[i].ecuId == ecuId)
			return (int8_t)i;
	}
	return -1;
}

/** Restituisce l'indice della centralina, aggiungendola vuota se non era ancora nota. */
inline int8_t obdFindOrAddEcu(uint16_t ecuId)
{
	int8_t index = obdFindEcu(ecuId);
	if (index >= 0 || ecuCount >= OBD_MAX_ECUS)
		return index;
	EcuScanResult &ecu = ecuResults[ecuCount];
	memset(&ecu, 0, sizeof(ecu));
	ecu.ecuId = ecuId;
	return (int8_t)ecuCount++;
}

/** Vero se la centralina motore ha dichiarato il PID Mode 01. */
inline bool obdEngineSupports(uint8_t pid)
{
	return engineEcuIndex >= 0 && obdBitmapHas(ecuResults[engineEcuIndex].mode01, pid);
}

/**
 * Scrive il nome della centralina mostrato a display e in dashboard: "Motore" e "Cambio" per gli ID
 * convenzionali 0x7E8 e 0x7E9, "Centralina N" per gli altri, numerati dalla posizione dell'ID.
 */
inline void obdEcuDisplayName(uint8_t index, char *out, size_t size)
{
	uint16_t ecuId = ecuResults[index].ecuId;
	if (ecuId == 0x7E8)
		strncpy_P(out, OBD_ECU_ENGINE, size);
	else if (ecuId == 0x7E9)
		strncpy_P(out, OBD_ECU_GEARBOX, size);
	else
		snprintf_P(out, size, PSTR("Centralina %u"), (unsigned)(ecuId - OBD_FIRST_RESPONSE_ID + 1));
	out[size - 1] = '\0';
}

/**
 * Interroga una sola centralina: in fisico se risponde all'indirizzo fisico e il trasporto lo consente,
 * altrimenti in funzionale attendendo solo la sua risposta. Ritorna il messaggio o nullptr.
 */
inline const ObdIsoTpRx *obdRequestEcu(uint8_t index, const uint8_t *request, uint8_t requestLen,
									   uint16_t windowMs)
{
	const EcuScanResult &ecu = ecuResults[index];
	if (ecu.respondsPhysical && obdTransportPhysicalAvailable)
		obdTransact(obdPhysicalRequestId(ecu.ecuId), request, requestLen, windowMs, ecu.ecuId);
	else
		obdTransact(OBD_FUNCTIONAL_REQUEST_ID, request, requestLen, windowMs, ecu.ecuId);
	return obdCollectorFind(obdCollector, ecu.ecuId);
}

/** Interroga la centralina motore con le stesse regole di obdRequestEcu(). */
inline const ObdIsoTpRx *obdRequestEngine(const uint8_t *request, uint8_t requestLen, uint16_t windowMs)
{
	if (engineEcuIndex < 0)
		return nullptr;
	return obdRequestEcu((uint8_t)engineEcuIndex, request, requestLen, windowMs);
}

/** Vero se il messaggio è la risposta positiva al PID o InfoType richiesto. */
inline bool obdIsPositiveFor(const ObdIsoTpRx *msg, uint8_t service, uint8_t id)
{
	return msg && msg->receivedLen >= 2 && msg->data[0] == (uint8_t)(service + 0x40) && msg->data[1] == id;
}

/** Azzera i risultati dello scan precedente. */
inline void obdResetScanResults()
{
	ecuCount = 0;
	engineEcuIndex = -1;
	engineMultiPid = false;
	engineReferenceTorqueNm = NAN;
	obfcmAvailable = false;
	obfcmEcuId = 0;
	obfcmRawLen = 0;
	memset(ecuResults, 0, sizeof(ecuResults));
}

/**
 * Discovery delle centraline: due richieste funzionali 0100 con unione delle risposte, più un terzo
 * tentativo se nessuno ha risposto. Ogni risposta fornisce già il primo range della bitmap Mode 01.
 */
inline void obdDiscoverEcus()
{
	const uint8_t request[2] = {0x01, 0x00};
	for (uint8_t attempt = 0; attempt < 3; attempt++)
	{
		if (attempt == 2 && ecuCount > 0)
			break;
		char detail[22];
		snprintf_P(detail, sizeof(detail), PSTR("Tentativo %u"), (unsigned)(attempt + 1));
		obdShowScanProgress("Ricerca centraline", detail);
		obdTransact(OBD_FUNCTIONAL_REQUEST_ID, request, 2, OBD_SCAN_WINDOW_MS, 0);
		for (uint8_t i = 0; i < obdCollector.used; i++)
		{
			const ObdIsoTpRx &slot = obdCollector.slots[i];
			uint8_t four[4];
			if (slot.state != OBD_ISOTP_COMPLETE ||
				!obdParseSupportedRange(slot.data, slot.receivedLen, 0x41, 0x00, four))
				continue;
			int8_t index = obdFindOrAddEcu(slot.ecuId);
			if (index >= 0)
				obdBitmapStoreRange(ecuResults[index].mode01, 0x00, four);
		}
		delay(20);
	}
}

/** Verifica quali centraline rispondono anche all'indirizzo fisico (ID di risposta - 8). */
inline void obdTestPhysicalAddressing()
{
	if (!obdTransportPhysicalAvailable)
		return;
	const uint8_t request[2] = {0x01, 0x00};
	for (uint8_t i = 0; i < ecuCount; i++)
	{
		uint16_t ecuId = ecuResults[i].ecuId;
		obdTransact(obdPhysicalRequestId(ecuId), request, 2, OBD_SCAN_WINDOW_MS, ecuId);
		ecuResults[i].respondsPhysical = obdIsPositiveFor(obdCollectorFind(obdCollector, ecuId), 0x01, 0x00);
	}
}

/**
 * Copia nella bitmap di ogni centralina ancora in attesa la risposta di supporto presente nel collettore
 * e aggiorna il numero di centraline che devono ancora rispondere.
 */
inline void obdStoreRangeResponses(uint8_t service, uint8_t base, bool *expecting, uint8_t &pending)
{
	for (uint8_t i = 0; i < ecuCount; i++)
	{
		if (!expecting[i])
			continue;
		const ObdIsoTpRx *msg = obdCollectorFind(obdCollector, ecuResults[i].ecuId);
		uint8_t four[4];
		if (msg && obdParseSupportedRange(msg->data, msg->receivedLen, (uint8_t)(service + 0x40), base, four))
		{
			obdBitmapStoreRange(obdServiceBitmap(ecuResults[i], service, false), base, four);
			expecting[i] = false;
			pending--;
		}
	}
}

/**
 * Percorre la catena delle bitmap di supporto del servizio (0x01 fino al range 0xE0, 0x09 fino a 0x20)
 * per tutte le centraline insieme: fino a due broadcast per range e infine la richiesta fisica a chi
 * non ha risposto. Una centralina muta interrompe solo la propria catena.
 */
inline void obdScanSupportChain(uint8_t service)
{
	const uint16_t firstBase = (service == 0x01) ? 0x20 : 0x00;
	const uint16_t lastBase = (service == 0x01) ? 0xE0 : 0x20;
	for (uint16_t base = firstBase; base <= lastBase; base += 0x20)
	{
		bool expecting[OBD_MAX_ECUS];
		uint8_t pending = 0;
		for (uint8_t i = 0; i < ecuCount; i++)
		{
			uint8_t *bitmap = obdServiceBitmap(ecuResults[i], service, false);
			expecting[i] = (base == 0) || obdBitmapHas(bitmap, (uint8_t)base);
			if (expecting[i])
				pending++;
		}
		if (pending == 0)
			break;

		char detail[22];
		snprintf_P(detail, sizeof(detail), PSTR("Mode %02X range %02X"), service, (unsigned)base);
		obdShowScanProgress("Lettura PID", detail);

		const uint8_t request[2] = {service, (uint8_t)base};
		for (uint8_t attempt = 0; attempt < 2 && pending > 0; attempt++)
		{
			obdTransact(OBD_FUNCTIONAL_REQUEST_ID, request, 2, OBD_SCAN_WINDOW_MS, 0);
			obdStoreRangeResponses(service, (uint8_t)base, expecting, pending);
		}
		for (uint8_t i = 0; i < ecuCount && pending > 0 && obdTransportPhysicalAvailable; i++)
		{
			if (!expecting[i])
				continue;
			obdTransact(obdPhysicalRequestId(ecuResults[i].ecuId), request, 2, OBD_SCAN_WINDOW_MS,
						ecuResults[i].ecuId);
			obdStoreRangeResponses(service, (uint8_t)base, expecting, pending);
		}
		for (uint8_t i = 0; i < ecuCount; i++)
		{
			// In Mode 09 il silenzio sul range 0x00 significa solo che la centralina non supporta il servizio
			if (expecting[i] && !(service == 0x09 && base == 0))
			{
				Serial.print(F("Scan: ECU 0x"));
				Serial.print(ecuResults[i].ecuId, HEX);
				Serial.print(F(" non ha risposto al range 0x"));
				Serial.print((unsigned)base, HEX);
				Serial.print(F(" del Mode 0"));
				Serial.println(service, HEX);
			}
		}
	}
}

/** Legge il nome (InfoType 0x0A) di ogni centralina che lo dichiara. */
inline void obdReadEcuNames()
{
	const uint8_t request[2] = {0x09, 0x0A};
	for (uint8_t i = 0; i < ecuCount; i++)
	{
		if (!obdBitmapHas(ecuResults[i].mode09, 0x0A))
			continue;
		const ObdIsoTpRx *msg = obdRequestEcu(i, request, 2, OBD_SCAN_WINDOW_MS);
		if (obdIsPositiveFor(msg, 0x09, 0x0A))
			obdParseEcuName(msg->data, msg->receivedLen, ecuResults[i].name, sizeof(ecuResults[i].name));
	}
}

/** Legge i contatori OBFCM dalla prima centralina che dichiara la InfoType 0x17, motore per primo. */
inline void obdReadObfcm()
{
	const uint8_t request[2] = {0x09, 0x17};
	for (int8_t pass = 0; pass < 2 && !obfcmAvailable; pass++)
	{
		for (uint8_t i = 0; i < ecuCount && !obfcmAvailable; i++)
		{
			bool isEngine = (int8_t)i == engineEcuIndex;
			if ((pass == 0) != isEngine || !obdBitmapHas(ecuResults[i].mode09, 0x17))
				continue;
			const ObdIsoTpRx *msg = obdRequestEcu(i, request, 2, OBD_SCAN_WINDOW_MS);
			if (!obdIsPositiveFor(msg, 0x09, 0x17))
				continue;
			obfcmRawLen = msg->receivedLen;
			memcpy(obfcmRaw, msg->data, obfcmRawLen);
			obfcmAvailable = obdParseObfcm(msg->data, msg->receivedLen, &obfcmData);
			obfcmEcuId = ecuResults[i].ecuId;
		}
	}
}

/**
 * Rileva le capacità del motore usate dalla lettura live: coppia di riferimento (0x63) e risposta
 * a una richiesta con due PID, che abilita la lettura multi-PID.
 */
inline void obdProbeEngineCapabilities()
{
	if (obdEngineSupports(0x63))
	{
		const uint8_t request[2] = {0x01, 0x63};
		const ObdIsoTpRx *msg = obdRequestEngine(request, 2, OBD_SCAN_WINDOW_MS);
		if (obdIsPositiveFor(msg, 0x01, 0x63) && msg->receivedLen >= 4)
			engineReferenceTorqueNm = (float)((msg->data[2] << 8) | msg->data[3]);
	}
	const uint8_t candidates[] = {0x0C, 0x0D, 0x05, 0x04, 0x0B, 0x0F};
	uint8_t request[3] = {0x01, 0, 0};
	uint8_t chosen = 0;
	for (uint8_t i = 0; i < sizeof(candidates) && chosen < 2; i++)
	{
		if (obdEngineSupports(candidates[i]))
			request[1 + chosen++] = candidates[i];
	}
	if (chosen < 2)
		return;
	const ObdIsoTpRx *msg = obdRequestEngine(request, 3, OBD_SCAN_WINDOW_MS);
	if (!msg || msg->data[0] != 0x41)
		return;
	ObdPidField fields[OBD_MAX_PIDS_PER_REQUEST];
	bool wellFormed = false;
	uint8_t count = obdParseMultiPid(msg->data, msg->receivedLen, fields, OBD_MAX_PIDS_PER_REQUEST, &wellFormed);
	engineMultiPid = wellFormed && count == 2;
}

/**
 * Esegue lo scan completo al boot: centraline, indirizzamento fisico, catene Mode 01 e Mode 09, nomi,
 * OBFCM e capacità del motore. Ritorna OBD_SCAN_NO_ECU se nessuno risponde e OBD_SCAN_NO_ENGINE se
 * manca la centralina motore 0x7E8, da cui dipende tutta la lettura live.
 */
inline uint8_t obdRunBootScan()
{
	obdBusy = true;
	obdProbe.phase = OBD_PROBE_IDLE;
	obdResetScanResults();
	obdDiscoverEcus();
	if (ecuCount == 0)
	{
		obdBusy = false;
		return OBD_SCAN_NO_ECU;
	}
	engineEcuIndex = obdFindEcu(OBD_ENGINE_RESPONSE_ID);
	obdTestPhysicalAddressing();
	obdScanSupportChain(0x01);
	obdShowScanProgress("Lettura Mode 09", "");
	obdScanSupportChain(0x09);
	obdReadEcuNames();
	obdReadObfcm();
	if (engineEcuIndex >= 0)
	{
		obdShowScanProgress("Verifica motore", "");
		obdProbeEngineCapabilities();
	}
	obdBusy = false;
	return engineEcuIndex >= 0 ? OBD_SCAN_OK : OBD_SCAN_NO_ENGINE;
}

/** Stampa su Serial un byte in esadecimale a due cifre. */
inline void obdPrintHexByte(uint8_t value)
{
	if (value < 0x10)
		Serial.print('0');
	Serial.print(value, HEX);
}

/** Stampa su Serial una stringa PROGMEM, oppure il testo alternativo se il puntatore è nullo. */
inline void obdPrintProgmemOr(const char *text, const __FlashStringHelper *fallback)
{
	if (text)
		Serial.print((const __FlashStringHelper *)text);
	else
		Serial.print(fallback);
}

/** Stampa su Serial l'elenco con descrizione degli ID impostati nella bitmap del servizio. */
inline void obdPrintBitmapList(const uint8_t *bitmap, uint8_t service, uint16_t maxId)
{
	for (uint16_t id = 1; id <= maxId; id++)
	{
		if (!obdBitmapHas(bitmap, (uint8_t)id))
			continue;
		Serial.print(F("    0x"));
		obdPrintHexByte((uint8_t)id);
		Serial.print(F("  "));
		const char *name = (service == 0x01) ? getPIDFullName((uint8_t)id) : obdInfoTypeName((uint8_t)id);
		obdPrintProgmemOr(name, F("(non documentato)"));
		Serial.println();
	}
}

/**
 * Stampa su Serial il report dello scan: per ogni centralina PID Mode 01 e InfoType Mode 09 dichiarati
 * con codice e descrizione, poi OBFCM e capacità del motore. È il dump da conservare per sapere quali
 * PID la vettura espone davvero.
 */
inline void obdPrintScanReport()
{
	Serial.println(F("\n=== REPORT SCAN OBD2 ==="));
	Serial.print(F("Adattatore: header "));
	Serial.print(obdTransportHeadersAvailable ? F("si") : F("NO, elenco limitato alla prima centralina"));
	Serial.print(F(", indirizzamento fisico "));
	Serial.println(obdTransportPhysicalAvailable ? F("si") : F("no"));
	for (uint8_t i = 0; i < ecuCount; i++)
	{
		const EcuScanResult &ecu = ecuResults[i];
		char displayName[24];
		obdEcuDisplayName(i, displayName, sizeof(displayName));
		Serial.print(F("\nECU 0x"));
		Serial.print(ecu.ecuId, HEX);
		Serial.print(F(" ("));
		Serial.print(displayName);
		if (ecu.name[0])
		{
			Serial.print(F(", "));
			Serial.print(ecu.name);
		}
		Serial.print(F(") risposta fisica: "));
		Serial.println(ecu.respondsPhysical ? F("si") : F("no"));
		Serial.print(F("  Mode 01: "));
		Serial.print(obdCountDataIds(ecu.mode01, 0xFF));
		Serial.println(F(" PID"));
		obdPrintBitmapList(ecu.mode01, 0x01, 0xFF);
		Serial.print(F("  Mode 09: "));
		Serial.print(obdCountDataIds(ecu.mode09, 0x40));
		Serial.println(F(" InfoType"));
		obdPrintBitmapList(ecu.mode09, 0x09, 0x40);
	}
	if (obfcmAvailable || obfcmRawLen > 0)
	{
		Serial.print(F("\nOBFCM (ECU 0x"));
		Serial.print(obfcmEcuId, HEX);
		Serial.print(F(") grezzo:"));
		for (uint16_t i = 0; i < obfcmRawLen; i++)
		{
			Serial.print(' ');
			obdPrintHexByte(obfcmRaw[i]);
		}
		Serial.println();
		if (obfcmAvailable)
		{
			Serial.print(F("  Distanza di vita: "));
			Serial.print(obfcmData.lifetimeDistanceKm, 1);
			Serial.print(F(" km, carburante di vita: "));
			Serial.print(obfcmData.lifetimeFuelL, 2);
			Serial.print(F(" L, layout "));
			Serial.println(obfcmData.plausible ? F("plausibile") : F("NON plausibile (ordine dei campi da verificare)"));
		}
	}
	if (engineEcuIndex >= 0)
	{
		Serial.print(F("\nMotore: multi-PID "));
		Serial.print(engineMultiPid ? F("si") : F("no"));
		Serial.print(F(", coppia "));
		Serial.print(obdEngineSupports(0x62) ? F("letta dalla centralina") : F("stimata dal modello"));
		Serial.print(F(", boost "));
		Serial.println(obdEngineSupports(0x70) ? F("letto dalla centralina") : F("stimato dal modello"));
		if (!isnan(engineReferenceTorqueNm))
		{
			Serial.print(F("  Coppia di riferimento: "));
			Serial.print((int)engineReferenceTorqueNm);
			Serial.println(F(" Nm"));
		}
	}
	Serial.println();
}

/** Vero mentre la scansione approfondita è in corso. */
inline bool obdProbeRunning()
{
	return obdProbe.phase >= OBD_PROBE_PHYSICAL && obdProbe.phase <= OBD_PROBE_MODE09;
}

/** Avanzamento percentuale della scansione approfondita. */
inline uint8_t obdProbePercent()
{
	uint16_t done = 0;
	if (obdProbe.phase == OBD_PROBE_PHYSICAL)
		done = obdProbe.step;
	else if (obdProbe.phase == OBD_PROBE_MODE01)
		done = 8 + obdProbe.step - 1;
	else if (obdProbe.phase == OBD_PROBE_MODE09)
		done = 8 + 255 + obdProbe.step - 1;
	else if (obdProbe.phase == OBD_PROBE_DONE)
		done = OBD_PROBE_TOTAL_STEPS;
	return (uint8_t)((uint32_t)done * 100 / OBD_PROBE_TOTAL_STEPS);
}

/**
 * Avvia la scansione approfondita, solo dopo uno scan riuscito e se non è già in corso. L'handler web
 * imposta soltanto lo stato: il bus viene interrogato da obdProbeStep() nel loop.
 */
inline bool obdProbeStart()
{
	if (obdProbeRunning() || obdBusy || ecuCount == 0)
		return false;
	for (uint8_t i = 0; i < ecuCount; i++)
	{
		memset(ecuResults[i].mode01Verified, 0, sizeof(ecuResults[i].mode01Verified));
		memset(ecuResults[i].mode09Verified, 0, sizeof(ecuResults[i].mode09Verified));
		ecuResults[i].probed = false;
	}
	obdProbe.phase = OBD_PROBE_PHYSICAL;
	obdProbe.step = 0;
	obdProbe.startedMs = millis();
	obdProbe.durationMs = 0;
	obdBusy = true;
	Serial.println(F("\n=== SCANSIONE APPROFONDITA: ogni PID Mode 01 e InfoType Mode 09 ==="));
	return true;
}

/** Stampa su Serial una risposta della scansione approfondita con descrizione e byte grezzi. */
inline void obdPrintProbeLine(uint16_t ecuId, uint8_t service, uint8_t id, const ObdIsoTpRx &msg, bool declared)
{
	Serial.print(F("  ECU 0x"));
	Serial.print(ecuId, HEX);
	Serial.print(F(" Mode 0"));
	Serial.print(service, HEX);
	Serial.print(F(" 0x"));
	obdPrintHexByte(id);
	Serial.print(F(" "));
	obdPrintProgmemOr(service == 0x01 ? getPIDFullName(id) : obdInfoTypeName(id), F("(non documentato)"));
	uint16_t dataLen = msg.receivedLen >= 2 ? msg.receivedLen - 2 : 0;
	Serial.print(F(" | "));
	Serial.print(dataLen);
	Serial.print(F(" byte"));
	if (service == 0x09 && id == 0x02)
	{
		Serial.print(F(" (VIN non riportato)"));
	}
	else
	{
		Serial.print(':');
		for (uint16_t i = 2; i < msg.receivedLen; i++)
		{
			Serial.print(' ');
			obdPrintHexByte(msg.data[i]);
		}
	}
	if (service == 0x01 && obdPidDataLen(id) != 0 && dataLen != obdPidDataLen(id))
	{
		Serial.print(F(" | ATTESI "));
		Serial.print(obdPidDataLen(id));
	}
	if (!declared)
		Serial.print(F(" | NON DICHIARATO"));
	Serial.println();
}

/** Registra le risposte raccolte per un PID o una InfoType della scansione approfondita. */
inline void obdProbeRecordResponses(uint8_t service, uint8_t id)
{
	for (uint8_t s = 0; s < obdCollector.used; s++)
	{
		const ObdIsoTpRx &msg = obdCollector.slots[s];
		if (msg.state != OBD_ISOTP_COMPLETE || !obdIsPositiveFor(&msg, service, id))
			continue;
		int8_t index = obdFindOrAddEcu(msg.ecuId);
		if (index < 0)
			continue;
		EcuScanResult &ecu = ecuResults[index];
		obdBitmapSet(obdServiceBitmap(ecu, service, true), id);
		obdPrintProbeLine(ecu.ecuId, service, id, msg, obdBitmapHas(obdServiceBitmap(ecu, service, false), id));
	}
}

/** Stampa su Serial gli ID presenti in una sola delle due bitmap, con la descrizione. */
inline void obdPrintBitmapDifference(const uint8_t *present, const uint8_t *absent, uint8_t service,
									 uint16_t maxId, const __FlashStringHelper *title)
{
	bool header = false;
	for (uint16_t id = 1; id <= maxId; id++)
	{
		if (obdIsRangePid((uint8_t)id) || !obdBitmapHas(present, (uint8_t)id) || obdBitmapHas(absent, (uint8_t)id))
			continue;
		if (!header)
		{
			Serial.print(F("  "));
			Serial.println(title);
			header = true;
		}
		Serial.print(F("    0x"));
		obdPrintHexByte((uint8_t)id);
		Serial.print(F("  "));
		obdPrintProgmemOr(service == 0x01 ? getPIDFullName((uint8_t)id) : obdInfoTypeName((uint8_t)id),
						  F("(non documentato)"));
		Serial.println();
	}
}

/** Stampa il riepilogo finale della scansione approfondita per ogni centralina. */
inline void obdPrintProbeSummary()
{
	Serial.print(F("\n=== FINE SCANSIONE APPROFONDITA ("));
	Serial.print(obdProbe.durationMs / 1000);
	Serial.println(F(" s) ==="));
	for (uint8_t i = 0; i < ecuCount; i++)
	{
		const EcuScanResult &ecu = ecuResults[i];
		Serial.print(F("ECU 0x"));
		Serial.print(ecu.ecuId, HEX);
		Serial.print(F(": Mode 01 dichiarati "));
		Serial.print(obdCountDataIds(ecu.mode01, 0xFF));
		Serial.print(F(", rispondono "));
		Serial.print(obdCountDataIds(ecu.mode01Verified, 0xFF));
		Serial.print(F("; Mode 09 dichiarate "));
		Serial.print(obdCountDataIds(ecu.mode09, 0x40));
		Serial.print(F(", rispondono "));
		Serial.println(obdCountDataIds(ecu.mode09Verified, 0x1F));
		obdPrintBitmapDifference(ecu.mode01, ecu.mode01Verified, 0x01, 0xFF, F("Mode 01 dichiarati ma muti:"));
		obdPrintBitmapDifference(ecu.mode01Verified, ecu.mode01, 0x01, 0xFF, F("Mode 01 non dichiarati ma rispondono:"));
		obdPrintBitmapDifference(ecu.mode09, ecu.mode09Verified, 0x09, 0x1F, F("Mode 09 dichiarate ma mute:"));
		obdPrintBitmapDifference(ecu.mode09Verified, ecu.mode09, 0x09, 0x1F, F("Mode 09 non dichiarate ma rispondono:"));
	}
	Serial.println();
}

/**
 * Esegue un passo della scansione approfondita: prima 0100 fisico a 0x7E0-0x7E7 per scoprire centraline
 * che non rispondono in funzionale, poi ogni PID Mode 01 e ogni InfoType Mode 09 in funzionale. Ogni
 * passo completa la propria richiesta, così fra un passo e l'altro gli handler web restano serviti.
 */
inline void obdProbeStep()
{
	if (obdProbe.phase == OBD_PROBE_PHYSICAL)
	{
		if (obdTransportPhysicalAvailable)
		{
			uint16_t requestId = (uint16_t)(0x7E0 + obdProbe.step);
			uint16_t ecuId = (uint16_t)(requestId + 8);
			const uint8_t request[2] = {0x01, 0x00};
			obdTransact(requestId, request, 2, OBD_LIVE_WINDOW_MS, ecuId);
			const ObdIsoTpRx *msg = obdCollectorFind(obdCollector, ecuId);
			uint8_t four[4];
			if (msg && obdParseSupportedRange(msg->data, msg->receivedLen, 0x41, 0x00, four))
			{
				bool known = obdFindEcu(ecuId) >= 0;
				int8_t index = obdFindOrAddEcu(ecuId);
				if (index >= 0)
				{
					ecuResults[index].respondsPhysical = true;
					if (!known)
					{
						obdBitmapStoreRange(ecuResults[index].mode01, 0x00, four);
						Serial.print(F("  Centralina 0x"));
						Serial.print(ecuId, HEX);
						Serial.println(F(" risponde solo all'indirizzo fisico"));
					}
				}
			}
		}
		if (++obdProbe.step >= 8)
		{
			obdProbe.phase = OBD_PROBE_MODE01;
			obdProbe.step = 1;
		}
		return;
	}

	if (obdProbe.phase == OBD_PROBE_MODE01)
	{
		const uint8_t request[2] = {0x01, (uint8_t)obdProbe.step};
		obdTransact(OBD_FUNCTIONAL_REQUEST_ID, request, 2, OBD_LIVE_WINDOW_MS, 0);
		obdProbeRecordResponses(0x01, (uint8_t)obdProbe.step);
		if (++obdProbe.step > 0xFF)
		{
			obdProbe.phase = OBD_PROBE_MODE09;
			obdProbe.step = 1;
		}
		return;
	}

	if (obdProbe.phase == OBD_PROBE_MODE09)
	{
		const uint8_t request[2] = {0x09, (uint8_t)obdProbe.step};
		obdTransact(OBD_FUNCTIONAL_REQUEST_ID, request, 2, OBD_SCAN_WINDOW_MS, 0);
		obdProbeRecordResponses(0x09, (uint8_t)obdProbe.step);
		if (++obdProbe.step > 0x1F)
		{
			for (uint8_t i = 0; i < ecuCount; i++)
				ecuResults[i].probed = true;
			obdProbe.phase = OBD_PROBE_DONE;
			obdProbe.durationMs = millis() - obdProbe.startedMs;
			obdBusy = false;
			obdPrintProbeSummary();
		}
	}
}

/**
 * Scrittore JSON a blocchi per ESP8266WebServer: accumula in un buffer statico e invia in chunked
 * encoding, senza mai spedire frammenti vuoti a metà risposta (chiuderebbero lo stream).
 */
struct ObdJsonStream
{
	ESP8266WebServer *server;
	size_t length;

	/** Restituisce il buffer statico condiviso dalle risposte JSON (gli handler non sono concorrenti). */
	static char *buffer()
	{
		static char storage[512];
		return storage;
	}

	/** Avvia una risposta chunked application/json. */
	void begin(ESP8266WebServer &target)
	{
		server = &target;
		length = 0;
		server->sendHeader(F("Cache-Control"), F("no-store"));
		// Stesso schema di chunkedResponseModeStart(): con un client HTTP/1.0 lo stream si chiude a fine risposta
		server->setContentLength(CONTENT_LENGTH_UNKNOWN);
		server->send_P(200, PSTR("application/json"), "");
	}

	/** Invia il contenuto accumulato, se presente. */
	void flush()
	{
		if (length == 0)
			return;
		server->sendContent(buffer(), length);
		length = 0;
	}

	/** Accoda un carattere, svuotando il buffer quando è pieno. */
	void put(char c)
	{
		if (length >= 511)
			flush();
		buffer()[length++] = c;
	}

	/** Accoda testo già in formato JSON. */
	void raw(const char *text)
	{
		while (*text)
			put(*text++);
	}

	/** Accoda testo PROGMEM già in formato JSON. */
	void rawP(PGM_P text)
	{
		char c;
		while ((c = (char)pgm_read_byte(text++)) != '\0')
			put(c);
	}

	/** Accoda testo formattato con formato PROGMEM. */
	void printfP(PGM_P format, ...)
	{
		char temp[96];
		va_list args;
		va_start(args, format);
		vsnprintf_P(temp, sizeof(temp), format, args);
		va_end(args);
		raw(temp);
	}

	/** Accoda una stringa JSON tra virgolette, con escape di virgolette, backslash e controllo. */
	void string(const char *text, bool progmem)
	{
		put('"');
		char c;
		while ((c = progmem ? (char)pgm_read_byte(text) : *text) != '\0')
		{
			text++;
			if (c == '"' || c == '\\')
			{
				put('\\');
				put(c);
			}
			else if ((uint8_t)c >= 0x20)
			{
				put(c);
			}
		}
		put('"');
	}

	/** Chiude la risposta: invia il resto e il chunk terminale. */
	void end()
	{
		flush();
		server->sendContent("");
	}
};

/** Accoda al JSON l'elenco dei nomi degli ID impostati e restituisce quanti non hanno descrizione. */
inline uint16_t obdJsonNameList(ObdJsonStream &json, const uint8_t *present, const uint8_t *absent, uint8_t service,
								uint16_t maxId)
{
	uint16_t undocumented = 0;
	bool first = true;
	json.put('[');
	for (uint16_t id = 1; id <= maxId; id++)
	{
		if (obdIsRangePid((uint8_t)id) || !obdBitmapHas(present, (uint8_t)id))
			continue;
		if (absent && obdBitmapHas(absent, (uint8_t)id))
			continue;
		const char *name = (service == 0x01) ? getPIDShortName((uint8_t)id) : obdInfoTypeName((uint8_t)id);
		if (!name)
		{
			undocumented++;
			continue;
		}
		if (!first)
			json.put(',');
		json.string(name, true);
		first = false;
	}
	json.put(']');
	return undocumented;
}

/** Restituisce lo stato della scansione approfondita come stringa PROGMEM per il JSON. */
inline PGM_P obdProbeStateName()
{
	if (obdProbeRunning())
		return PSTR("running");
	if (obdProbe.phase == OBD_PROBE_DONE)
		return PSTR("done");
	return PSTR("idle");
}

/** Accoda al JSON l'oggetto con stato e avanzamento della scansione approfondita. */
inline void obdJsonProbeStatus(ObdJsonStream &json)
{
	json.rawP(PSTR("{\"state\":\""));
	json.rawP(obdProbeStateName());
	json.printfP(PSTR("\",\"percent\":%u,\"seconds\":%lu}"), (unsigned)obdProbePercent(),
				 (unsigned long)((obdProbeRunning() ? millis() - obdProbe.startedMs : obdProbe.durationMs) / 1000));
}

/**
 * Risponde a /scan-data con il risultato dello scan e della scansione approfondita: per ogni centralina
 * nome, PID e InfoType per descrizione (senza codici esadecimali), conteggio dei non documentati,
 * differenze fra dichiarati e verificati; poi OBFCM e capacità del motore.
 * @see web_dashboard.h setupWebDashboard()
 */
inline void obdHandleScanData(ESP8266WebServer &server)
{
	ObdJsonStream json;
	json.begin(server);
	json.rawP(PSTR("{\"probe\":"));
	obdJsonProbeStatus(json);
	json.printfP(PSTR(",\"adapter\":{\"headers\":%s,\"physical\":%s}"),
				 obdTransportHeadersAvailable ? "true" : "false", obdTransportPhysicalAvailable ? "true" : "false");
	if (engineEcuIndex >= 0)
	{
		json.printfP(PSTR(",\"engine\":{\"multiPid\":%s,\"torque\":\"%s\",\"boost\":\"%s\",\"refTorque\":%d}"),
					 engineMultiPid ? "true" : "false", obdEngineSupports(0x62) ? "ECU" : "stima",
					 obdEngineSupports(0x70) ? "ECU" : "stima",
					 isnan(engineReferenceTorqueNm) ? -1 : (int)engineReferenceTorqueNm);
	}
	if (obfcmAvailable)
	{
		float average = (obfcmData.lifetimeDistanceValid && obfcmData.lifetimeFuelValid && obfcmData.lifetimeDistanceKm > 1.0f)
							? obfcmData.lifetimeFuelL / obfcmData.lifetimeDistanceKm * 100.0f
							: -1.0f;
		json.printfP(PSTR(",\"obfcm\":{\"km\":%.1f,\"liters\":%.2f,\"avg\":%.2f,"),
					 obfcmData.lifetimeDistanceValid ? obfcmData.lifetimeDistanceKm : -1.0f,
					 obfcmData.lifetimeFuelValid ? obfcmData.lifetimeFuelL : -1.0f, average);
		json.printfP(PSTR("\"recentKm\":%.1f,\"recentLiters\":%.2f,\"plausible\":%s}"),
					 obfcmData.recentDistanceValid ? obfcmData.recentDistanceKm : -1.0f,
					 obfcmData.recentFuelValid ? obfcmData.recentFuelL : -1.0f, obfcmData.plausible ? "true" : "false");
	}
	json.rawP(PSTR(",\"ecus\":["));
	for (uint8_t i = 0; i < ecuCount; i++)
	{
		const EcuScanResult &ecu = ecuResults[i];
		char displayName[24];
		obdEcuDisplayName(i, displayName, sizeof(displayName));
		if (i > 0)
			json.put(',');
		json.rawP(PSTR("{\"name\":"));
		json.string(displayName, false);
		json.rawP(PSTR(",\"detail\":"));
		json.string(ecu.name, false);
		json.printfP(PSTR(",\"physical\":%s,\"probed\":%s,\"pids\":"), ecu.respondsPhysical ? "true" : "false",
					 ecu.probed ? "true" : "false");
		uint16_t undocumented = obdJsonNameList(json, ecu.mode01, nullptr, 0x01, 0xFF);
		json.printfP(PSTR(",\"undoc\":%u,\"mode09\":"), (unsigned)undocumented);
		uint16_t undocumented09 = obdJsonNameList(json, ecu.mode09, nullptr, 0x09, 0x40);
		json.printfP(PSTR(",\"undoc09\":%u"), (unsigned)undocumented09);
		if (ecu.probed)
		{
			json.rawP(PSTR(",\"silent\":"));
			uint16_t silentUndoc = obdJsonNameList(json, ecu.mode01, ecu.mode01Verified, 0x01, 0xFF);
			json.rawP(PSTR(",\"extra\":"));
			uint16_t extraUndoc = obdJsonNameList(json, ecu.mode01Verified, ecu.mode01, 0x01, 0xFF);
			json.rawP(PSTR(",\"extra09\":"));
			uint16_t extra09Undoc = obdJsonNameList(json, ecu.mode09Verified, ecu.mode09, 0x09, 0x1F);
			json.printfP(PSTR(",\"silentUndoc\":%u,\"extraUndoc\":%u"), (unsigned)silentUndoc,
						 (unsigned)(extraUndoc + extra09Undoc));
		}
		json.put('}');
	}
	json.rawP(PSTR("]}"));
	json.end();
}

/** Risponde a /probe-status con stato e avanzamento della scansione approfondita. */
inline void obdHandleProbeStatus(ESP8266WebServer &server)
{
	ObdJsonStream json;
	json.begin(server);
	obdJsonProbeStatus(json);
	json.end();
}

#endif
