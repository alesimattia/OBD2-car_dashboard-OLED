/**
 * Lettura live OBD2 condivisa dalle due build: valori mostrati da display e dashboard (boost, coppia,
 * temperatura liquido, carica batteria al litio del mild hybrid), lettura multi-PID con ripiego a PID
 * singolo, stato MIL e DTC di tutte le centraline, cancellazione DTC e istantanea diagnostica usata da
 * /data e dal Serial. Ogni valore che la centralina espone si legge, mai stimato: boost da 0x70 e 0x33,
 * coppia da 0x62, 0x8E e 0x63; senza quei PID il valore è non disponibile. Al boot non c'è nessuna
 * discovery: i PID del motore e il modo di lettura vengono dal blocco "Profilo vettura", da sostituire
 * con il profilo suggerito dalla scansione completa.
 *
 * Va incluso dopo serial_logger.h e obd_scan.h.
 *
 * Usato da: CANbus_conn.ino, WIFI_conn.ino, web_dashboard.h
 *
 * @Mattia Alesi
 */

#ifndef OBD_LIVE_H
#define OBD_LIVE_H

#include <Arduino.h>
#include <math.h>
#include "obd_proto.h"
#include "obd_scan.h"
#include "dtc_descriptions.h"

// ============================================================
// Profilo vettura: sostituire con il profilo suggerito stampato a fine scansione completa
// ============================================================

// PID Mode 01 del motore 7E8 usati da lettura live e diagnostica, dalla scansione completa del 22/11/2026
static const uint8_t OBD_ENGINE_PIDS[] PROGMEM = {0x70, 0x0C, 0x62, 0x8E, 0x05, 0x33, 0x63, 0x04, 0x0B,
												  0x0D, 0x0F, 0x23, 0x34, 0x44, 0x42, 0x46, 0x49, 0x4A,
												  0x4C, 0x01, 0x1F, 0x21, 0x30, 0x5E, 0x9D, 0xA4};
// SAE J1979 §7.1.1: la centralina accetta fino a 6 PID per richiesta
static constexpr bool OBD_ENGINE_MULTI_PID = true;
// Centraline OBD che rispondono solo alle richieste fisiche (0x7EC: freni ABS/ESP), lista chiusa da 0
static const uint16_t OBD_PHYSICAL_ONLY_ECUS[] PROGMEM = {0x7EC, 0};

// Modo di lettura corrente: parte dal profilo e passa al PID singolo se il motore rifiuta le richieste multiple
inline bool engineMultiPid = OBD_ENGINE_MULTI_PID;

/**
 * Vero se il PID Mode 01 è nel profilo del motore.
 * @since 29/10/26 Mattia Alesi
 */
inline bool obdEngineSupports(uint8_t pid)
{
	for (uint8_t i = 0; i < sizeof(OBD_ENGINE_PIDS); i++)
	{
		if (pgm_read_byte(&OBD_ENGINE_PIDS[i]) == pid)
			return true;
	}
	return false;
}

/**
 * Interroga una centralina OBD dal suo ID di risposta: in fisico se il trasporto lo consente, altrimenti in
 * funzionale attendendo solo la sua risposta. Ritorna il messaggio o nullptr.
 */
inline const ObdIsoTpRx *obdRequestEcu(uint16_t ecuId, const uint8_t *request, uint8_t requestLen, uint16_t windowMs)
{
	if (obdTransportPhysicalAvailable)
		obdTransact(obdPhysicalRequestId(ecuId), request, requestLen, windowMs, ecuId);
	else
		obdTransact(OBD_FUNCTIONAL_REQUEST_ID, request, requestLen, windowMs, ecuId);
	return obdCollectorFind(obdCollector, ecuId);
}

/** Interroga il motore 7E0/7E8 con le regole di obdRequestEcu(). */
inline const ObdIsoTpRx *obdRequestEngine(const uint8_t *request, uint8_t requestLen, uint16_t windowMs)
{
	return obdRequestEcu(OBD_ENGINE_RESPONSE_ID, request, requestLen, windowMs);
}

static constexpr uint8_t OBD_MAX_DTC = 12;
static constexpr unsigned long OBD_DTC_CHECK_INTERVAL_MS = 30000;
// Dopo 3 letture consecutive fallite il valore viene mostrato come N/D invece di restare congelato
static constexpr uint8_t OBD_STALE_AFTER_FAILURES = 3;
static constexpr uint8_t OBD_MULTI_PID_MAX_MALFORMED = 3;

// Slot della lettura live: ogni slot ha un PID e alimenta un valore mostrato o la coppia di riferimento
static constexpr uint8_t LIVE_BOOST = 0;
static constexpr uint8_t LIVE_RPM = 1;
static constexpr uint8_t LIVE_TORQUE = 2;
static constexpr uint8_t LIVE_FRICTION = 3;
static constexpr uint8_t LIVE_COOLANT = 4;
static constexpr uint8_t LIVE_BARO = 5;
static constexpr uint8_t LIVE_REF_TORQUE = 6;
static constexpr uint8_t LIVE_SLOT_COUNT = 7;
static const uint8_t OBD_LIVE_SLOT_PID[LIVE_SLOT_COUNT] PROGMEM = {0x70, 0x0C, 0x62, 0x8E, 0x05, 0x33, 0x63};

// Valori mostrati da display e dashboard, con la loro disponibilità
inline float boostBar = 0.0f;
inline bool boostAvailable = false;
inline int torqueNm = 0;
inline bool torqueAvailable = false;
inline int coolantC = 0;
inline bool coolantAvailable = false;
// Carica della batteria al litio del mild hybrid: nessun DID con decodifica verificata, quindi resta non
// disponibile finchè la prova di tools/pid_scan.ps1 non ne trova uno (README §5.5)
inline int hybBattPct = 0;
inline bool hybBattAvailable = false;

// Ultimi valori letti, NAN se mai letti o scaduti
inline float cachedBaro = NAN, cachedRpm = NAN;
inline float cachedBoostKpa = NAN, cachedTorquePct = NAN, cachedFrictionPct = NAN;
// Coppia di riferimento delle percentuali 0x62 e 0x8E (PID 0x63): costante della centralina, letta una volta
// dopo l'avvio e mai scaduta
inline float referenceTorqueNm = NAN;

// Rapporto aria/carburante stechiometrico della benzina senza etanolo: con la E5 il valore reale è circa il 2% più basso
static constexpr float OBD_FUEL_STOICHIOMETRIC_AFR = 14.7f;
// Costante specifica dei gas dell'aria secca, J/(kg*K)
static constexpr float OBD_GAS_CONSTANT_AIR = 287.05f;

// Stato DTC aggregato da tutte le centraline
inline bool milOn = false;
inline uint8_t dtcCount = 0;
inline uint16_t dtcCodes[OBD_MAX_DTC];
// ID di risposta della centralina che ha riportato ciascun DTC
inline uint16_t dtcEcuId[OBD_MAX_DTC];
inline unsigned long lastDtcCheck = 0;
inline bool dtcCheckRequested = true;

// Pianificazione della lettura live
inline bool obdLiveSlotEnabled[LIVE_SLOT_COUNT];
inline uint8_t obdLiveSlotFailures[LIVE_SLOT_COUNT];
inline uint8_t obdLiveFastSlots[4];
inline uint8_t obdLiveFastCount = 0;
inline uint8_t obdLiveSlowSlots[3];
inline uint8_t obdLiveSlowCount = 0;
inline uint8_t obdLiveSlowCursor = 0;
inline uint8_t obdLiveSingleCursor = 0;
inline uint8_t obdLiveMalformedStreak = 0;
// Cicli consecutivi senza alcuna risposta dal motore: lo sketch decide il ripristino
inline uint8_t obdLiveSilentStreak = 0;

/** Restituisce il PID associato allo slot di lettura live. */
inline uint8_t obdLiveSlotPid(uint8_t slot)
{
	return pgm_read_byte(&OBD_LIVE_SLOT_PID[slot]);
}

/**
 * Decodifica i PID a valore singolo usati da display e diagnostica con le formule SAE J1979:
 * percentuali A*100/255, temperature A-40, coppie A-125, lambda (256A+B)/32768, rail in kPa,
 * portata carburante 0x9D in g/s, marcia 0xA4 se il bit di supporto la dichiara.
 */
inline bool obdDecodeScalar(uint8_t pid, const uint8_t *data, uint8_t len, float *value)
{
	if (len == 0)
		return false;
	const bool twoBytes = len >= 2;
	const float ab = twoBytes ? (float)((data[0] << 8) | data[1]) : 0.0f;
	switch (pid)
	{
	case 0x04: case 0x11: case 0x2F: case 0x49: case 0x4A: case 0x4C:
		*value = (float)data[0] * 100.0f / 255.0f;
		return true;
	case 0x05: case 0x0F: case 0x46:
		*value = (float)data[0] - 40.0f;
		return true;
	case 0x0B: case 0x0D: case 0x30: case 0x33:
		*value = (float)data[0];
		return true;
	case 0x61: case 0x62: case 0x8E:
		*value = (float)data[0] - 125.0f;
		return true;
	case 0x0C:
		*value = ab / 4.0f;
		return twoBytes;
	case 0x1F: case 0x21: case 0x63:
		*value = ab;
		return twoBytes;
	case 0x23:
		*value = ab * 10.0f;
		return twoBytes;
	case 0x42:
		*value = ab / 1000.0f;
		return twoBytes;
	case 0x5E:
		*value = ab / 20.0f;
		return twoBytes;
	case 0x9D:
		// Byte A-B: portata del motore (C-D è quella del veicolo)
		*value = ab / 50.0f;
		return twoBytes;
	case 0x34: case 0x44:
		*value = ab / 32768.0f;
		return twoBytes;
	case 0x70:
		return obdDecodeBoostSensorA(data, len, value);
	case 0xA4:
		// Bit 0 di A: marcia dichiarata; nibble alto di B: marcia attuale (OBDb GEAR_ACT)
		if (!twoBytes || (data[0] & 0x01) == 0)
			return false;
		*value = (float)(data[1] >> 4);
		return true;
	default:
		return false;
	}
}

/**
 * Azzera valori, cache e contatori della lettura live, ad esempio dopo una nuova connessione all'adattatore.
 * La coppia di riferimento resta: è una costante della centralina.
 */
inline void obdLiveResetValues()
{
	boostAvailable = torqueAvailable = coolantAvailable = hybBattAvailable = false;
	cachedBaro = cachedRpm = cachedBoostKpa = cachedTorquePct = cachedFrictionPct = NAN;
	memset(obdLiveSlotFailures, 0, sizeof(obdLiveSlotFailures));
	obdLiveSlowCursor = obdLiveSingleCursor = 0;
	obdLiveMalformedStreak = obdLiveSilentStreak = 0;
}

/**
 * Prepara la lettura live sui PID del profilo vettura: un gruppo veloce (boost 0x70, RPM, coppia 0x62 e attrito
 * 0x8E, letti insieme perchè la coppia all'albero è la loro differenza) letto a ogni ciclo e un gruppo lento
 * letto a rotazione, un PID per ciclo; la coppia di riferimento 0x63 resta nel gruppo lento finchè non è letta.
 * Il modo di lettura riparte da quello del profilo.
 */
inline void obdLiveConfigure()
{
	obdLiveResetValues();
	engineMultiPid = OBD_ENGINE_MULTI_PID;
	for (uint8_t slot = 0; slot < LIVE_SLOT_COUNT; slot++)
		obdLiveSlotEnabled[slot] = obdEngineSupports(obdLiveSlotPid(slot));
	// Attrito e coppia di riferimento servono solo alla coppia, BARO solo al boost
	if (!obdLiveSlotEnabled[LIVE_TORQUE])
		obdLiveSlotEnabled[LIVE_FRICTION] = obdLiveSlotEnabled[LIVE_REF_TORQUE] = false;
	if (!obdLiveSlotEnabled[LIVE_BOOST])
		obdLiveSlotEnabled[LIVE_BARO] = false;

	obdLiveFastCount = 0;
	const uint8_t fast[] = {LIVE_BOOST, LIVE_RPM, LIVE_TORQUE, LIVE_FRICTION};
	for (uint8_t i = 0; i < sizeof(fast); i++)
	{
		if (obdLiveSlotEnabled[fast[i]])
			obdLiveFastSlots[obdLiveFastCount++] = fast[i];
	}

	obdLiveSlowCount = 0;
	const uint8_t slow[] = {LIVE_COOLANT, LIVE_BARO, LIVE_REF_TORQUE};
	for (uint8_t i = 0; i < sizeof(slow); i++)
	{
		if (obdLiveSlotEnabled[slow[i]] && !(slow[i] == LIVE_REF_TORQUE && !isnan(referenceTorqueNm)))
			obdLiveSlowSlots[obdLiveSlowCount++] = slow[i];
	}

	Serial.print(F("Lettura live: "));
	Serial.print(engineMultiPid ? F("multi-PID") : F("PID singolo"));
	Serial.print(F(", "));
	Serial.print(obdLiveFastCount);
	Serial.print(F(" PID veloci e "));
	Serial.print(obdLiveSlowCount);
	Serial.println(F(" lenti"));
}

/** Conta una lettura fallita dello slot e, dopo OBD_STALE_AFTER_FAILURES, ne invalida il valore. */
inline void obdLiveFail(uint8_t slot)
{
	if (++obdLiveSlotFailures[slot] < OBD_STALE_AFTER_FAILURES)
		return;
	switch (slot)
	{
	case LIVE_BOOST: cachedBoostKpa = NAN; break;
	case LIVE_RPM: cachedRpm = NAN; break;
	case LIVE_TORQUE: cachedTorquePct = NAN; break;
	case LIVE_FRICTION: cachedFrictionPct = NAN; break;
	case LIVE_BARO: cachedBaro = NAN; break;
	case LIVE_COOLANT: coolantAvailable = false; break;
	}
}

/** Toglie uno slot dal gruppo lento, per la coppia di riferimento una volta letta. */
inline void obdLiveDropSlowSlot(uint8_t slot)
{
	uint8_t kept = 0;
	for (uint8_t i = 0; i < obdLiveSlowCount; i++)
	{
		if (obdLiveSlowSlots[i] != slot)
			obdLiveSlowSlots[kept++] = obdLiveSlowSlots[i];
	}
	obdLiveSlowCount = kept;
	obdLiveSlowCursor = obdLiveSingleCursor = 0;
}

/**
 * Memorizza nella cache il valore letto per uno slot; un PID presente nella risposta ma non decodificabile (per
 * esempio 0x70 con il sensore A non dichiarato) conta come lettura fallita, così il valore precedente scade.
 */
inline void obdLiveApply(uint8_t slot, const uint8_t *data, uint8_t len)
{
	float value;
	if (!obdDecodeScalar(obdLiveSlotPid(slot), data, len, &value))
	{
		obdLiveFail(slot);
		return;
	}
	obdLiveSlotFailures[slot] = 0;
	switch (slot)
	{
	case LIVE_BOOST: cachedBoostKpa = value; break;
	case LIVE_RPM: cachedRpm = value; break;
	case LIVE_TORQUE: cachedTorquePct = value; break;
	case LIVE_FRICTION: cachedFrictionPct = value; break;
	case LIVE_BARO: cachedBaro = value; break;
	case LIVE_COOLANT:
		coolantC = (int)lroundf(value);
		coolantAvailable = true;
		break;
	case LIVE_REF_TORQUE:
		if (value > 0.0f)
		{
			referenceTorqueNm = value;
			obdLiveDropSlowSlot(LIVE_REF_TORQUE);
		}
		break;
	}
}

/**
 * Coppia di riferimento delle percentuali 0x62 e 0x8E secondo SAE J1979: il PID 0x63 letto dalla centralina
 * (370 Nm sulla vettura), NAN finchè non è stato letto.
 */
inline float obdReferenceTorqueNm()
{
	return referenceTorqueNm;
}

/**
 * Coppia all'albero dalla coppia effettiva 0x62 meno l'attrito 0x8E, in percentuale della coppia di
 * riferimento: 0x62 è la coppia indicata (semantica J1939), quindi comprende l'attrito.
 */
inline float obdShaftTorqueNm(float actualPct, float frictionPct)
{
	float friction = isnan(frictionPct) ? 0.0f : frictionPct;
	return (actualPct - friction) * obdReferenceTorqueNm() / 100.0f;
}

/**
 * Ricalcola i valori mostrati dai PID letti: boost dal sensore 0x70 meno la pressione barometrica 0x33, coppia
 * all'albero da 0x62, 0x8E e 0x63; se manca uno dei PID il valore diventa non disponibile.
 */
inline void obdLiveRecalc()
{
	boostAvailable = obdLiveSlotEnabled[LIVE_BOOST] && !isnan(cachedBoostKpa) && !isnan(cachedBaro);
	if (boostAvailable)
		boostBar = (cachedBoostKpa - cachedBaro) / 100.0f;

	torqueAvailable = obdLiveSlotEnabled[LIVE_TORQUE] && !isnan(cachedTorquePct) && !isnan(obdReferenceTorqueNm());
	if (torqueAvailable)
	{
		// A motore fermo (veleggio, start-stop) l'attrito riportato non genera coppia all'albero
		const bool engineStopped = !isnan(cachedRpm) && cachedRpm < 1.0f;
		torqueNm = engineStopped ? 0 : (int)lroundf(obdShaftTorqueNm(cachedTorquePct, cachedFrictionPct));
	}
}

/**
 * Sceglie gli slot da leggere in questo ciclo: in multi-PID tutto il gruppo veloce più un PID lento,
 * a PID singolo un solo slot secondo la sequenza pesata (veloci tre volte, lenti una).
 */
inline uint8_t obdLivePickSlots(uint8_t *slots)
{
	uint8_t count = 0;
	if (engineMultiPid)
	{
		for (uint8_t i = 0; i < obdLiveFastCount; i++)
			slots[count++] = obdLiveFastSlots[i];
		if (obdLiveSlowCount > 0)
		{
			uint8_t index = (uint8_t)(obdLiveSlowCursor % obdLiveSlowCount);
			obdLiveSlowCursor = (uint8_t)((index + 1) % obdLiveSlowCount);
			slots[count++] = obdLiveSlowSlots[index];
		}
		return count;
	}
	uint16_t sequenceLength = (uint16_t)(obdLiveFastCount * 3 + obdLiveSlowCount);
	if (sequenceLength == 0)
		return 0;
	uint16_t position = obdLiveSingleCursor % sequenceLength;
	obdLiveSingleCursor = (uint8_t)((position + 1) % sequenceLength);
	if (position < obdLiveFastCount * 3)
		slots[count++] = obdLiveFastSlots[position % obdLiveFastCount];
	else
		slots[count++] = obdLiveSlowSlots[position - obdLiveFastCount * 3];
	return count;
}

/**
 * Verifica se il problema delle richieste multi-PID è il multi-PID stesso: interroga da solo il PID indicato e, se
 * risponde, passa definitivamente al PID singolo. Un motore spento o un PID non supportato tacciono anche da soli e
 * non cambiano il modo di lettura.
 */
inline void obdLiveConfirmMultiPidFallback(uint8_t pid)
{
	const uint8_t request[2] = {0x01, pid};
	if (obdIsPositiveFor(obdRequestEngine(request, 2, OBD_LIVE_WINDOW_MS), 0x01, pid))
	{
		engineMultiPid = false;
		Serial.println(F("Lettura live: il motore risponde ai PID singoli ma non alle richieste multiple, passo al PID singolo"));
	}
}

/** Vero se il motore ha iniziato a rispondere ma il messaggio ISO-TP non è arrivato completo (frame persi). */
inline bool obdLiveEngineReplyIncomplete()
{
	for (uint8_t s = 0; s < obdCollector.used; s++)
	{
		const ObdIsoTpRx &slot = obdCollector.slots[s];
		if (slot.ecuId == OBD_ENGINE_RESPONSE_ID && (slot.state == OBD_ISOTP_RECEIVING || slot.state == OBD_ISOTP_ERROR))
			return true;
	}
	return false;
}

/**
 * Esegue un ciclo di lettura live: una richiesta al motore con gli slot scelti, separazione della risposta,
 * aggiornamento dei valori. Per le richieste multi-PID contano come anomalia il silenzio, una risposta negativa,
 * una malformata o una con meno PID di quelli chiesti; dopo OBD_MULTI_PID_MAX_MALFORMED anomalie di fila una
 * richiesta singola conferma se ripiegare sul PID singolo. Un messaggio rimasto a metà per frame persi non è
 * un'anomalia del multi-PID nè un silenzio del motore. Ritorna false se il motore non ha risposto affatto.
 */
inline bool obdLiveStep()
{
	uint8_t slots[OBD_MAX_PIDS_PER_REQUEST];
	uint8_t count = obdLivePickSlots(slots);
	if (count == 0)
		return true;

	uint8_t request[1 + OBD_MAX_PIDS_PER_REQUEST];
	request[0] = 0x01;
	for (uint8_t i = 0; i < count; i++)
		request[1 + i] = obdLiveSlotPid(slots[i]);

	const ObdIsoTpRx *msg = obdRequestEngine(request, (uint8_t)(1 + count), OBD_LIVE_WINDOW_MS);
	const bool incomplete = !msg && obdLiveEngineReplyIncomplete();
	const bool engineAnswered = msg != nullptr || incomplete;
	bool answered[OBD_MAX_PIDS_PER_REQUEST] = {false};
	uint8_t answeredCount = 0;
	bool wellFormed = false;
	if (msg && msg->receivedLen >= 2 && msg->data[0] == 0x41)
	{
		ObdPidField fields[OBD_MAX_PIDS_PER_REQUEST];
		uint8_t fieldCount = obdParseMultiPid(msg->data, msg->receivedLen, fields, OBD_MAX_PIDS_PER_REQUEST, &wellFormed);
		// Con una lunghezza sbagliata i campi successivi risultano spostati: una risposta malformata non si usa
		if (!wellFormed)
			fieldCount = 0;
		for (uint8_t f = 0; f < fieldCount; f++)
		{
			for (uint8_t i = 0; i < count; i++)
			{
				if (!answered[i] && obdLiveSlotPid(slots[i]) == fields[f].pid)
				{
					obdLiveApply(slots[i], fields[f].data, fields[f].len);
					answered[i] = true;
					answeredCount++;
					break;
				}
			}
		}
	}
	for (uint8_t i = 0; i < count; i++)
	{
		if (!answered[i])
			obdLiveFail(slots[i]);
	}
	obdLiveRecalc();
	obdLiveSilentStreak = engineAnswered ? 0 : (uint8_t)(obdLiveSilentStreak + 1);

	if (count > 1 && !incomplete)
	{
		const bool anomaly = !wellFormed || answeredCount < count;
		obdLiveMalformedStreak = anomaly ? (uint8_t)(obdLiveMalformedStreak + 1) : 0;
		if (obdLiveMalformedStreak >= OBD_MULTI_PID_MAX_MALFORMED)
		{
			obdLiveMalformedStreak = 0;
			// msg punta al collettore, che la richiesta di conferma riusa: da qui in poi non va più letto
			uint8_t probePid = request[1];
			for (uint8_t i = 0; i < count; i++)
			{
				if (!answered[i])
				{
					probePid = request[1 + i];
					break;
				}
			}
			obdLiveConfirmMultiPidFallback(probePid);
		}
	}
	return engineAnswered;
}

/**
 * Scrive l'abbreviazione della centralina per il display OLED dal suo ID di risposta: il nome ODIS ("Motore",
 * "Cambio", "Freni ABS/ESP"), altrimenti "Centr.N".
 */
inline void obdEcuShortName(uint16_t ecuId, char *out, size_t size)
{
	if (obdIsResponseId(ecuId) && obdOdisFindRequest(obdPhysicalRequestId(ecuId)) != OBDB_NONE)
		obdEcuDisplayName(ecuId, out, size);
	else
		snprintf_P(out, size, PSTR("Centr.%u"), (unsigned)(ecuId - OBD_FIRST_RESPONSE_ID + 1));
}

/** Attesa prima di ripetere un controllo DTC fallito (nessuna risposta o Mode 03 perso). */
static constexpr unsigned long OBD_DTC_RETRY_MS = 2000;

/**
 * Legge MIL e DTC da tutte le centraline: 0101 funzionale, più quello fisico alle centraline che rispondono solo
 * così (OBD_PHYSICAL_ONLY_ECUS), per stato MIL e numero di DTC confermati di ciascuna, poi Mode 03 a ogni
 * centralina con almeno un DTC, anche a MIL spenta. Senza nessuna risposta a 0101, o se il Mode 03 di una
 * centralina con DTC non risponde, lo stato precedente resta invariato, per non far sparire i codici su un
 * timeout, e il controllo si ripete dopo OBD_DTC_RETRY_MS.
 */
inline void obdLiveCheckDtcs()
{
	const uint8_t statusRequest[2] = {0x01, 0x01};
	obdTransact(OBD_FUNCTIONAL_REQUEST_ID, statusRequest, 2, OBD_SCAN_WINDOW_MS, 0);
	// Il collettore viene riusato dalle richieste successive: ID e conteggi vanno copiati prima
	uint16_t responders[OBD_MAX_ECUS];
	uint8_t perEcu[OBD_MAX_ECUS];
	uint8_t responderCount = 0;
	bool mil = false;
	for (uint8_t s = 0; s < obdCollector.used; s++)
	{
		const ObdIsoTpRx *msg = &obdCollector.slots[s];
		if (msg->state != OBD_ISOTP_COMPLETE || !obdIsPositiveFor(msg, 0x01, 0x01) || msg->receivedLen < 3)
			continue;
		mil = mil || (msg->data[2] & 0x80) != 0;
		responders[responderCount] = (uint16_t)msg->ecuId;
		perEcu[responderCount++] = msg->data[2] & 0x7F;
	}
	for (uint8_t i = 0; responderCount < OBD_MAX_ECUS; i++)
	{
		const uint16_t ecuId = pgm_read_word(&OBD_PHYSICAL_ONLY_ECUS[i]);
		if (ecuId == 0)
			break;
		const ObdIsoTpRx *msg = obdRequestEcu(ecuId, statusRequest, 2, OBD_SCAN_WINDOW_MS);
		if (!obdIsPositiveFor(msg, 0x01, 0x01) || msg->receivedLen < 3)
			continue;
		mil = mil || (msg->data[2] & 0x80) != 0;
		responders[responderCount] = ecuId;
		perEcu[responderCount++] = msg->data[2] & 0x7F;
	}
	lastDtcCheck = millis();
	dtcCheckRequested = true;
	if (responderCount == 0)
		return;

	uint8_t found = 0;
	uint16_t codes[OBD_MAX_DTC];
	uint16_t owners[OBD_MAX_DTC];
	const uint8_t dtcRequest[1] = {0x03};
	for (uint8_t i = 0; i < responderCount && found < OBD_MAX_DTC; i++)
	{
		if (perEcu[i] == 0)
			continue;
		const ObdIsoTpRx *msg = obdRequestEcu(responders[i], dtcRequest, 1, OBD_SCAN_WINDOW_MS);
		if (!msg)
			return;
		uint8_t added = obdParseDtcs(msg->data, msg->receivedLen, codes + found, (uint8_t)(OBD_MAX_DTC - found));
		for (uint8_t k = 0; k < added; k++)
			owners[found + k] = responders[i];
		found = (uint8_t)(found + added);
	}
	dtcCheckRequested = false;
	milOn = mil;
	dtcCount = found;
	memcpy(dtcCodes, codes, sizeof(uint16_t) * found);
	memcpy(dtcEcuId, owners, sizeof(uint16_t) * found);

	Serial.print(F("MIL: "));
	Serial.print(milOn ? F("ON") : F("OFF"));
	Serial.print(F(", DTC: "));
	Serial.println(dtcCount);
	for (uint8_t i = 0; i < dtcCount; i++)
	{
		char code[6];
		char owner[16];
		decodeDTC(dtcCodes[i], code);
		obdEcuShortName(dtcEcuId[i], owner, sizeof(owner));
		Serial.print(F("  DTC: "));
		Serial.print(code);
		Serial.print(F(" ("));
		Serial.print(owner);
		Serial.println(F(")"));
	}
}

/**
 * Vero quando è ora di rileggere i DTC: allo scadere dell'intervallo, oppure dopo una richiesta esplicita o un
 * controllo fallito, con almeno OBD_DTC_RETRY_MS fra due tentativi (il primo al boot è immediato).
 */
inline bool obdLiveDtcCheckDue()
{
	if (dtcCheckRequested)
		return lastDtcCheck == 0 || millis() - lastDtcCheck >= OBD_DTC_RETRY_MS;
	return millis() - lastDtcCheck > OBD_DTC_CHECK_INTERVAL_MS;
}

static constexpr uint8_t OBD_CLEAR_OK = 0;
static constexpr uint8_t OBD_CLEAR_PARTIAL = 1;
static constexpr uint8_t OBD_CLEAR_ENGINE_RUNNING = 2;
static constexpr uint8_t OBD_CLEAR_REFUSED = 3;
static constexpr uint8_t OBD_CLEAR_NO_RESPONSE = 4;

/** Testo breve dell'esito della cancellazione, adatto alla riga da 21 caratteri dell'OLED. */
inline const char *obdClearResultShortText(uint8_t result)
{
	switch (result)
	{
	case OBD_CLEAR_OK: return "DTC cancellati";
	case OBD_CLEAR_PARTIAL: return "Cancellati in parte";
	case OBD_CLEAR_ENGINE_RUNNING: return "Spegnere il motore";
	case OBD_CLEAR_REFUSED: return "Rifiutato";
	default: return "Nessuna risposta";
	}
}

/**
 * Cancella i DTC di tutte le centraline (Mode 04 funzionale, poi fisico a quelle di OBD_PHYSICAL_ONLY_ECUS) e
 * scrive in message l'esito leggibile: le centraline rifiutano con 7F 04 22 quando il motore è acceso.
 * Ritorna un codice OBD_CLEAR_*.
 */
inline uint8_t obdLiveClearDtcs(char *message, size_t size)
{
	const uint8_t request[1] = {0x04};
	uint8_t accepted = 0;
	uint8_t refused = 0;
	bool engineRunning = false;
	// Funzionale per tutte, poi fisico alle centraline che rispondono solo così
	for (uint8_t i = 0;; i++)
	{
		uint16_t ecuId = 0;
		if (i > 0)
		{
			ecuId = pgm_read_word(&OBD_PHYSICAL_ONLY_ECUS[i - 1]);
			if (ecuId == 0 || !obdTransportPhysicalAvailable)
				break;
		}
		// La cancellazione può richiedere più di P2CAN: le risposte 7F 04 78 estendono comunque l'attesa
		obdTransact(i == 0 ? OBD_FUNCTIONAL_REQUEST_ID : obdPhysicalRequestId(ecuId), request, 1, 300, ecuId);
		for (uint8_t s = 0; s < obdCollector.used; s++)
		{
			const ObdIsoTpRx &msg = obdCollector.slots[s];
			if (msg.state != OBD_ISOTP_COMPLETE || msg.receivedLen == 0)
				continue;
			uint8_t service, nrc;
			if (msg.data[0] == 0x44)
				accepted++;
			// NRC 11 e 12: la centralina non ha il Mode 04, non ha rifiutato la cancellazione
			else if (obdIsNegativeResponse(msg.data, msg.receivedLen, &service, &nrc) && nrc != 0x11 && nrc != 0x12)
			{
				refused++;
				engineRunning = engineRunning || nrc == OBD_NRC_CONDITIONS_NOT_CORRECT;
			}
		}
	}
	uint8_t result;
	if (accepted > 0 && refused == 0)
	{
		result = OBD_CLEAR_OK;
		snprintf_P(message, size, PSTR("DTC cancellati (%u centraline)"), (unsigned)accepted);
	}
	else if (accepted > 0)
	{
		result = OBD_CLEAR_PARTIAL;
		snprintf_P(message, size, PSTR("Cancellati su %u centraline, rifiutati da %u"), (unsigned)accepted,
				   (unsigned)refused);
	}
	else if (engineRunning)
	{
		result = OBD_CLEAR_ENGINE_RUNNING;
		snprintf_P(message, size, PSTR("Rifiutato: spegnere il motore, quadro acceso"));
	}
	else if (refused > 0)
	{
		result = OBD_CLEAR_REFUSED;
		snprintf_P(message, size, PSTR("Rifiutato dalle centraline"));
	}
	else
	{
		result = OBD_CLEAR_NO_RESPONSE;
		snprintf_P(message, size, PSTR("Nessuna risposta"));
	}
	if (accepted > 0)
		dtcCheckRequested = true;
	Serial.print(F("Clear DTC: "));
	Serial.println(message);
	return result;
}

// PID letti dall'istantanea diagnostica, se il motore li dichiara; i gruppi rispettano i 6 PID per richiesta
static const uint8_t OBD_DIAG_PIDS[] PROGMEM = {0x04, 0x05, 0x0B, 0x0C, 0x0D, 0x0F, 0x23, 0x34, 0x44,
												0x33, 0x42, 0x46, 0x49, 0x4A, 0x4C, 0x01, 0x1F, 0x21,
												0x30, 0x70, 0x62, 0x8E, 0x5E, 0x9D, 0xA4};
static constexpr uint8_t OBD_DIAG_PID_COUNT = sizeof(OBD_DIAG_PIDS);

/**
 * Scrive in pids, senza ripetizioni, i PID del motore usati da lettura live e diagnostica: sono i candidati del
 * profilo suggerito e della prova multi-PID della scansione completa. Ritorna quanti ne ha scritti.
 */
inline uint8_t obdLiveProfileCandidates(uint8_t *pids, uint8_t maxPids)
{
	uint8_t count = 0;
	for (uint8_t i = 0; i < LIVE_SLOT_COUNT + OBD_DIAG_PID_COUNT && count < maxPids; i++)
	{
		const uint8_t pid = i < LIVE_SLOT_COUNT ? obdLiveSlotPid(i) : pgm_read_byte(&OBD_DIAG_PIDS[i - LIVE_SLOT_COUNT]);
		bool known = false;
		for (uint8_t k = 0; k < count && !known; k++)
			known = pids[k] == pid;
		if (!known)
			pids[count++] = pid;
	}
	return count;
}

/** Istantanea dei PID diagnostici e delle grandezze calcolate, condivisa da /data e dal Serial. */
struct ObdDiagSnapshot
{
	unsigned long timestampMs;
	float value[OBD_DIAG_PID_COUNT];
	bool has[OBD_DIAG_PID_COUNT];
	float o2CurrentMa;
	bool hasO2Current;
	bool mil, hasMil;
	int dtcReported;
	// Grandezze calcolate; i flag *Ok indicano se i dati bastavano a calcolarle
	float boostBar, powerKw, afr, fuelL100, airDensity, pressureRatio, accel, boostRate;
	int torqueNm, bsfc, altitude, driftPedal, deltaThrottlePedal, intercoolerEff;
	bool boostOk, torqueOk, powerOk, afrOk, fuelL100Ok, bsfcOk, altitudeOk, airDensityOk, pressureRatioOk;
	bool driftOk, deltaOk, dfco, dfcoOk, intercoolerOk, accelOk, boostRateOk, engineStopped;
};

inline ObdDiagSnapshot obdDiag;
inline bool obdDiagValid = false;

/** Cerca nell'istantanea il valore del PID; false se non letto. */
inline bool obdDiagGet(const ObdDiagSnapshot &snap, uint8_t pid, float *value)
{
	for (uint8_t i = 0; i < OBD_DIAG_PID_COUNT; i++)
	{
		if (pgm_read_byte(&OBD_DIAG_PIDS[i]) == pid)
		{
			if (!snap.has[i])
				return false;
			*value = snap.value[i];
			return true;
		}
	}
	return false;
}

/** Memorizza nell'istantanea un PID letto, compresi i campi secondari di 0x01 e 0x34. */
inline void obdDiagApply(ObdDiagSnapshot &snap, uint8_t pid, const uint8_t *data, uint8_t len)
{
	if (pid == 0x01 && len >= 1)
	{
		snap.mil = (data[0] & 0x80) != 0;
		snap.dtcReported = data[0] & 0x7F;
		snap.hasMil = true;
		return;
	}
	if (pid == 0x34 && len >= 4)
	{
		snap.o2CurrentMa = (float)((data[2] << 8) | data[3]) / 256.0f - 128.0f;
		snap.hasO2Current = true;
	}
	for (uint8_t i = 0; i < OBD_DIAG_PID_COUNT; i++)
	{
		if (pgm_read_byte(&OBD_DIAG_PIDS[i]) == pid)
		{
			snap.has[i] = obdDecodeScalar(pid, data, len, &snap.value[i]);
			return;
		}
	}
}

/** Legge un gruppo di PID dal motore in un'unica richiesta e li memorizza nell'istantanea. */
inline void obdDiagReadGroup(ObdDiagSnapshot &snap, const uint8_t *pids, uint8_t count)
{
	uint8_t request[1 + OBD_MAX_PIDS_PER_REQUEST];
	request[0] = 0x01;
	memcpy(request + 1, pids, count);
	const ObdIsoTpRx *msg = obdRequestEngine(request, (uint8_t)(1 + count), OBD_LIVE_WINDOW_MS);
	if (!msg || msg->receivedLen < 2 || msg->data[0] != 0x41)
		return;
	ObdPidField fields[OBD_MAX_PIDS_PER_REQUEST];
	bool wellFormed = false;
	uint8_t fieldCount = obdParseMultiPid(msg->data, msg->receivedLen, fields, OBD_MAX_PIDS_PER_REQUEST, &wellFormed);
	// Con una lunghezza sbagliata i campi successivi risultano spostati: una risposta malformata non si usa
	if (!wellFormed)
		return;
	for (uint8_t f = 0; f < fieldCount; f++)
		obdDiagApply(snap, fields[f].pid, fields[f].data, fields[f].len);
}

/**
 * Calcola le grandezze derivate dell'istantanea dai PID letti: ogni valore che la centralina espone viene dal suo
 * PID, mai stimato. Restano stime, perchè la centralina non le espone, AFR, altitudine ed efficienza
 * dell'intercooler. previous serve alle derivate (accelerazione, variazione boost), calcolate solo se entrambe le
 * letture sono valide.
 */
inline void obdDiagCompute(ObdDiagSnapshot &s, const ObdDiagSnapshot *previous)
{
	float map = 0, rpm = 0, speed = 0, iat = 0, lambda = 0, baro = 0, ambient = 0, pedalD = 0, pedalE = 0, throttle = 0;
	float boostKpa = 0, torquePct = 0, frictionPct = 0, fuelRate = 0, fuelGs = 0;
	const bool hMap = obdDiagGet(s, 0x0B, &map);
	const bool hRpm = obdDiagGet(s, 0x0C, &rpm);
	const bool hSpeed = obdDiagGet(s, 0x0D, &speed);
	const bool hIat = obdDiagGet(s, 0x0F, &iat);
	const bool hBaro = obdDiagGet(s, 0x33, &baro) && baro > 0.0f;
	const bool hAmb = obdDiagGet(s, 0x46, &ambient);
	const bool hLambda = obdDiagGet(s, 0x34, &lambda);
	const bool hPedD = obdDiagGet(s, 0x49, &pedalD);
	const bool hPedE = obdDiagGet(s, 0x4A, &pedalE);
	const bool hThrottle = obdDiagGet(s, 0x4C, &throttle);
	const bool hBoost = obdDiagGet(s, 0x70, &boostKpa);
	const bool hTorque = obdDiagGet(s, 0x62, &torquePct);
	const bool hFriction = obdDiagGet(s, 0x8E, &frictionPct);
	const bool hFuelRate = obdDiagGet(s, 0x5E, &fuelRate);
	const bool hFuelGs = obdDiagGet(s, 0x9D, &fuelGs);

	s.engineStopped = hRpm && rpm < 1.0f;

	s.boostOk = hBoost && hBaro;
	s.boostBar = s.boostOk ? (boostKpa - baro) / 100.0f : 0.0f;

	s.torqueOk = hTorque && !isnan(obdReferenceTorqueNm());
	s.torqueNm = s.torqueOk && !s.engineStopped ? (int)lroundf(obdShaftTorqueNm(torquePct, hFriction ? frictionPct : NAN)) : 0;

	s.powerOk = s.torqueOk && hRpm && rpm > 0.0f;
	s.powerKw = s.powerOk ? (float)s.torqueNm * rpm / 9549.0f : 0.0f;

	s.afrOk = hLambda;
	s.afr = hLambda ? lambda * OBD_FUEL_STOICHIOMETRIC_AFR : 0.0f;

	s.fuelL100Ok = hFuelRate && hSpeed && speed > 3.0f;
	s.fuelL100 = s.fuelL100Ok ? fuelRate / speed * 100.0f : 0.0f;
	s.bsfcOk = hFuelGs && s.powerOk && s.powerKw > 1.0f;
	s.bsfc = s.bsfcOk ? (int)(fuelGs * 3600.0f / s.powerKw) : 0;

	s.altitudeOk = hBaro;
	s.altitude = s.altitudeOk ? (int)(44330.0f * (1.0f - powf(baro / 101.325f, 0.1903f))) : 0;
	s.airDensityOk = hMap && hIat;
	s.airDensity = s.airDensityOk ? (map * 1000.0f) / (OBD_GAS_CONSTANT_AIR * (iat + 273.15f)) : 0.0f;

	// Pressione all'uscita del compressore: sensore boost 0x70, prima della farfalla
	s.pressureRatioOk = hBoost && hBaro;
	s.pressureRatio = s.pressureRatioOk ? boostKpa / baro : 0.0f;

	s.driftOk = hPedD && hPedE;
	s.driftPedal = s.driftOk ? abs((int)pedalD - (int)pedalE) : 0;
	s.deltaOk = hThrottle && hPedD;
	s.deltaThrottlePedal = s.deltaOk ? (int)throttle - (int)pedalD : 0;
	// Taglio iniezione in rilascio: la centralina riporta portata nulla con il motore in rotazione
	s.dfcoOk = hFuelRate && hRpm;
	s.dfco = s.dfcoOk && fuelRate <= 0.0f && rpm > 0.0f;

	// Stima dalla compressione isentropica: con un riscaldamento teorico sotto 10 K il rapporto non è significativo
	s.intercoolerOk = false;
	s.intercoolerEff = 0;
	if (hBoost && hBaro && hIat && hAmb && boostKpa > baro)
	{
		float theoretical = (ambient + 273.15f) * powf(boostKpa / baro, 0.286f) - 273.15f;
		float denominator = theoretical - ambient;
		if (denominator >= 10.0f)
		{
			s.intercoolerEff = (int)(((theoretical - iat) / denominator) * 100.0f);
			s.intercoolerOk = true;
		}
	}

	s.accelOk = s.boostRateOk = false;
	s.accel = s.boostRate = 0.0f;
	if (previous)
	{
		float dt = (float)(s.timestampMs - previous->timestampMs) / 1000.0f;
		float prevSpeed;
		if (dt > 0.05f && dt < 10.0f && hSpeed && obdDiagGet(*previous, 0x0D, &prevSpeed))
		{
			s.accel = ((speed - prevSpeed) / 3.6f) / dt;
			s.accelOk = true;
		}
		if (dt > 0.05f && dt < 10.0f && s.boostOk && previous->boostOk)
		{
			s.boostRate = (s.boostBar - previous->boostBar) / dt;
			s.boostRateOk = true;
		}
	}
}

/**
 * Legge dal motore i PID diagnostici dichiarati, in gruppi da al massimo 6 PID con risposta entro il
 * buffer ISO-TP, oppure uno per richiesta se il motore non accetta il multi-PID; poi calcola i derivati.
 */
inline void obdDiagRead()
{
	static ObdDiagSnapshot previous;
	bool havePrevious = obdDiagValid;
	if (havePrevious)
		previous = obdDiag;

	ObdDiagSnapshot &snap = obdDiag;
	memset(&snap, 0, sizeof(snap));
	snap.timestampMs = millis();

	uint8_t group[OBD_MAX_PIDS_PER_REQUEST];
	uint8_t groupCount = 0;
	uint16_t groupResponseLen = 1;
	for (uint8_t i = 0; i < OBD_DIAG_PID_COUNT; i++)
	{
		uint8_t pid = pgm_read_byte(&OBD_DIAG_PIDS[i]);
		uint8_t dataLen = obdPidDataLen(pid);
		if (!obdEngineSupports(pid) || dataLen == 0)
			continue;
		uint8_t maxCount = engineMultiPid ? OBD_MAX_PIDS_PER_REQUEST : 1;
		if (groupCount > 0 && (groupCount >= maxCount || groupResponseLen + 1 + dataLen > OBD_MAX_MESSAGE_LEN))
		{
			obdDiagReadGroup(snap, group, groupCount);
			groupCount = 0;
			groupResponseLen = 1;
		}
		group[groupCount++] = pid;
		groupResponseLen = (uint16_t)(groupResponseLen + 1 + dataLen);
	}
	if (groupCount > 0)
		obdDiagReadGroup(snap, group, groupCount);

	obdDiagCompute(snap, havePrevious ? &previous : nullptr);
	obdDiagValid = true;
}

/** Rilegge l'istantanea diagnostica solo se è più vecchia di maxAgeMs. */
inline void obdDiagRefresh(unsigned long maxAgeMs)
{
	if (!obdDiagValid || millis() - obdDiag.timestampMs > maxAgeMs)
		obdDiagRead();
}

/** Stampa su Serial una riga "etichetta: valore unità [riferimento]" o "N/D" se il valore manca. */
inline void obdPrintDiagLine(const __FlashStringHelper *label, bool ok, float value, uint8_t decimals,
							 const __FlashStringHelper *unitAndRange)
{
	Serial.print(F("  "));
	Serial.print(label);
	Serial.print(F(": "));
	if (!ok)
	{
		Serial.println(F("N/D"));
		return;
	}
	Serial.print(value, decimals);
	Serial.println(unitAndRange);
}

/** Stampa su Serial il PID dell'istantanea moltiplicato per scale, oppure N/D se non letto. */
inline void obdPrintDiagPid(const ObdDiagSnapshot &s, uint8_t pid, const __FlashStringHelper *label, float scale,
							uint8_t decimals, const __FlashStringHelper *unitAndRange)
{
	float value = 0.0f;
	const bool ok = obdDiagGet(s, pid, &value);
	obdPrintDiagLine(label, ok, value * scale, decimals, unitAndRange);
}

/**
 * Stampa su Serial l'istantanea diagnostica con i riferimenti tipici del 2.0 TFSI EA888 (DMSA):
 * valori letti, poi grandezze calcolate da quei valori.
 */
inline void obdPrintDiagnostics(const ObdDiagSnapshot &s)
{
	Serial.println(F("\n========== DIAGNOSTICA (A5 F5 40 TFSI) =========="));
	obdPrintDiagPid(s, 0x04, F("Carico motore"), 1, 1, F(" %  [minimo: 15-25, pieno carico: 80-100]"));
	obdPrintDiagPid(s, 0x05, F("Temp. liquido"), 1, 0, F(" C  [regime: 90-107]"));
	obdPrintDiagPid(s, 0x0B, F("MAP"), 1, 0, F(" kPa  [minimo: 30-40, boost: fino a ~230]"));
	obdPrintDiagPid(s, 0x70, F("Pressione boost"), 1, 1, F(" kPa ass.  [prima della farfalla]"));
	obdPrintDiagPid(s, 0x0C, F("RPM"), 1, 0, F(" rpm  [minimo: 650-750, max: 6500]"));
	obdPrintDiagPid(s, 0x0D, F("Velocità"), 1, 0, F(" km/h"));
	obdPrintDiagPid(s, 0xA4, F("Marcia"), 1, 0, F(""));
	obdPrintDiagPid(s, 0x5E, F("Portata carburante"), 1, 2, F(" L/h"));
	obdPrintDiagPid(s, 0x0F, F("Temp. aria (IAT)"), 1, 0, F(" C  [ambiente +10-30]"));
	obdPrintDiagPid(s, 0x23, F("Pressione rail"), 0.01f, 0, F(" bar  [minimo: 80-120, carico: fino a 350]"));
	obdPrintDiagPid(s, 0x34, F("Lambda"), 1, 3, F("  [stechiometrico: 1.000, arricchimento: < 1]"));
	obdPrintDiagPid(s, 0x44, F("Lambda comandata"), 1, 3, F(""));
	obdPrintDiagLine(F("Corrente sonda O2"), s.hasO2Current, s.o2CurrentMa, 2, F(" mA"));
	obdPrintDiagPid(s, 0x33, F("Pressione baro"), 1, 0, F(" kPa  [livello mare: 101]"));
	obdPrintDiagPid(s, 0x42, F("Batteria 12 V"), 1, 2, F(" V  [ricarica: 13.5-14.9, motore spento: ~12.8]"));
	obdPrintDiagPid(s, 0x46, F("Temp. esterna"), 1, 0, F(" C"));
	obdPrintDiagPid(s, 0x49, F("Pedale D"), 1, 0, F(" %"));
	obdPrintDiagPid(s, 0x4A, F("Pedale E"), 1, 0, F(" %"));
	obdPrintDiagPid(s, 0x4C, F("Farfalla comandata"), 1, 0, F(" %"));
	obdPrintDiagPid(s, 0x62, F("Coppia effettiva ECU"), 1, 0, F(" %  [indicata, comprende l'attrito]"));
	obdPrintDiagPid(s, 0x8E, F("Coppia di attrito"), 1, 0, F(" %"));
	if (s.hasMil)
	{
		Serial.print(F("  MIL: "));
		Serial.print(s.mil ? F("ACCESA") : F("SPENTA"));
		Serial.print(F(", DTC confermati del motore: "));
		Serial.println(s.dtcReported);
	}
	obdPrintDiagPid(s, 0x1F, F("Tempo motore"), 1, 0, F(" s"));
	obdPrintDiagPid(s, 0x21, F("Km con MIL"), 1, 0, F(" km"));
	obdPrintDiagPid(s, 0x30, F("Riscaldamenti dal clear"), 1, 0, F(""));

	Serial.println(F("  --- CALCOLATI ---"));
	if (s.engineStopped)
		Serial.println(F("  Motore fermo con quadro acceso (veleggio, start-stop o sosta)"));
	obdPrintDiagLine(F("Boost"), s.boostOk, s.boostBar, 2, F(" bar  [aspirazione: < 0, sovralimentazione: fino a ~1.2]"));
	obdPrintDiagLine(F("Coppia all'albero"), s.torqueOk, (float)s.torqueNm, 0, F(" Nm  [max: 320]"));
	obdPrintDiagLine(F("Potenza"), s.powerOk, s.powerKw, 1, F(" kW  [max: 150]"));
	obdPrintDiagLine(F("Potenza"), s.powerOk, s.powerKw * 1.36f, 1, F(" CV  [max: 204]"));
	obdPrintDiagLine(F("Air/Fuel Ratio"), s.afrOk, s.afr, 1, F("  [stechiometrico: 14.7]"));
	obdPrintDiagLine(F("Consumo"), s.fuelL100Ok, s.fuelL100, 1, F(" L/100km"));
	obdPrintDiagLine(F("Cons. specifico"), s.bsfcOk, (float)s.bsfc, 0, F(" g/kWh  [ottimo: 230-260]"));
	obdPrintDiagLine(F("Altitudine"), s.altitudeOk, (float)s.altitude, 0, F(" m"));
	obdPrintDiagLine(F("Densità aria"), s.airDensityOk, s.airDensity, 3, F(" kg/m3"));
	obdPrintDiagLine(F("Eff. intercooler"), s.intercoolerOk, (float)s.intercoolerEff, 0, F(" %  [buono: 60-85]"));
	obdPrintDiagLine(F("Rapporto compressione"), s.pressureRatioOk, s.pressureRatio, 2, F(""));
	obdPrintDiagLine(F("Drift pedale"), s.driftOk, (float)s.driftPedal, 0, F(" %  [normale: < 3, allarme: > 5]"));
	obdPrintDiagLine(F("Delta farfalla-pedale"), s.deltaOk, (float)s.deltaThrottlePedal, 0, F(" %"));
	if (s.dfcoOk)
	{
		Serial.print(F("  Taglio iniezione: "));
		Serial.println(s.dfco ? F("ATTIVO") : F("OFF"));
	}
	obdPrintDiagLine(F("Accelerazione"), s.accelOk, s.accel, 2, F(" m/s2"));
	obdPrintDiagLine(F("Variazione boost"), s.boostRateOk, s.boostRate, 2, F(" bar/s"));
	Serial.println(F("=================================================\n"));
}

#endif
