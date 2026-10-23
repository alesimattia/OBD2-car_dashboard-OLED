/**
 * Lettura live OBD2 condivisa dalle due build: valori mostrati da display e dashboard (boost, coppia,
 * temperatura liquido, carica batteria ibrida), lettura multi-PID con ripiego a PID singolo, stato
 * MIL e DTC di tutte le centraline, cancellazione DTC e istantanea diagnostica usata da /data e dal
 * Serial. Boost e coppia sono letti dalla centralina quando espone i PID 0x70 e 0x62, altrimenti
 * stimati da BoostModel.h e TorqueModel.h.
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
#include "EngineConstants.h"
#include "TorqueModel.h"
#include "BoostModel.h"
#include "dtc_descriptions.h"

static constexpr uint8_t OBD_MAX_DTC = 12;
static constexpr unsigned long OBD_DTC_CHECK_INTERVAL_MS = 30000;
// Dopo 3 letture consecutive fallite il valore viene mostrato come N/D invece di restare congelato
static constexpr uint8_t OBD_STALE_AFTER_FAILURES = 3;
static constexpr uint8_t OBD_MULTI_PID_MAX_MALFORMED = 3;

// Slot della lettura live: ogni slot ha un PID e alimenta un valore o un ingresso dei modelli
static constexpr uint8_t LIVE_BOOST = 0;
static constexpr uint8_t LIVE_MAP = 1;
static constexpr uint8_t LIVE_LOAD = 2;
static constexpr uint8_t LIVE_RPM = 3;
static constexpr uint8_t LIVE_MAF = 4;
static constexpr uint8_t LIVE_TORQUE = 5;
static constexpr uint8_t LIVE_IAT = 6;
static constexpr uint8_t LIVE_BARO = 7;
static constexpr uint8_t LIVE_RAIL = 8;
static constexpr uint8_t LIVE_VOLTS = 9;
static constexpr uint8_t LIVE_COOLANT = 10;
static constexpr uint8_t LIVE_HYBRID = 11;
static constexpr uint8_t LIVE_FRICTION = 12;
static constexpr uint8_t LIVE_SLOT_COUNT = 13;
static const uint8_t OBD_LIVE_SLOT_PID[LIVE_SLOT_COUNT] PROGMEM = {0x70, 0x0B, 0x04, 0x0C, 0x10, 0x62, 0x0F,
																	0x33, 0x23, 0x42, 0x05, 0x5B, 0x8E};

// Valori mostrati da display e dashboard, con la loro disponibilità e l'origine (misurato o stimato)
inline float boostBar = 0.0f;
inline bool boostAvailable = false;
inline bool boostMeasured = false;
inline int torqueNm = 0;
inline bool torqueAvailable = false;
inline bool torqueMeasured = false;
inline int coolantC = 0;
inline bool coolantAvailable = false;
inline int hybBattPct = 0;
inline bool hybBattAvailable = false;

// Ultimi valori letti, NAN se mai letti o scaduti; alimentano i modelli e il boost misurato
inline float cachedMap = NAN, cachedBaro = NAN, cachedMaf = NAN;
inline float cachedRpm = NAN, cachedIat = NAN, cachedLoad = NAN;
inline float cachedRail = NAN, cachedVolts = NAN;
inline float cachedBoostKpa = NAN, cachedTorquePct = NAN, cachedFrictionPct = NAN;
// MAP letto a motore fermo: pressione ambiente di riserva quando la centralina non espone BARO
inline float engineOffMapKpa = NAN;

// Stato DTC aggregato da tutte le centraline
inline bool milOn = false;
inline uint8_t dtcCount = 0;
inline uint16_t dtcCodes[OBD_MAX_DTC];
inline uint8_t dtcEcuIndex[OBD_MAX_DTC];
inline unsigned long lastDtcCheck = 0;
inline bool dtcCheckRequested = true;

// Pianificazione della lettura live
inline bool obdLiveSlotEnabled[LIVE_SLOT_COUNT];
inline uint8_t obdLiveSlotFailures[LIVE_SLOT_COUNT];
inline uint8_t obdLiveFastSlots[5];
inline uint8_t obdLiveFastCount = 0;
inline uint8_t obdLiveSlowSlots[8];
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
 * percentuali A*100/255, temperature A-40, coppie A-125, lambda (256A+B)/32768, rail in kPa.
 */
inline bool obdDecodeScalar(uint8_t pid, const uint8_t *data, uint8_t len, float *value)
{
	if (len == 0)
		return false;
	const bool twoBytes = len >= 2;
	const float ab = twoBytes ? (float)((data[0] << 8) | data[1]) : 0.0f;
	switch (pid)
	{
	case 0x04: case 0x11: case 0x2F: case 0x49: case 0x4A: case 0x4C: case 0x5B:
		*value = (float)data[0] * 100.0f / 255.0f;
		return true;
	case 0x05: case 0x0F: case 0x46: case 0x5C:
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
	case 0x10:
		*value = ab / 100.0f;
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
	case 0x24: case 0x34: case 0x44:
		*value = ab / 32768.0f;
		return twoBytes;
	case 0x70:
		return obdDecodeBoostSensorA(data, len, value);
	default:
		return false;
	}
}

/** Azzera valori, cache e contatori della lettura live, ad esempio dopo un nuovo scan. */
inline void obdLiveResetValues()
{
	boostAvailable = torqueAvailable = coolantAvailable = hybBattAvailable = false;
	boostMeasured = torqueMeasured = false;
	cachedMap = cachedBaro = cachedMaf = cachedRpm = cachedIat = cachedLoad = NAN;
	cachedRail = cachedVolts = cachedBoostKpa = cachedTorquePct = cachedFrictionPct = NAN;
	engineOffMapKpa = NAN;
	memset(obdLiveSlotFailures, 0, sizeof(obdLiveSlotFailures));
	obdLiveSlowCursor = obdLiveSingleCursor = 0;
	obdLiveMalformedStreak = obdLiveSilentStreak = 0;
}

/**
 * Prepara la lettura live sui PID che il motore ha dichiarato: un gruppo veloce (boost da 0x70 o MAP,
 * carico, RPM, MAF, coppia 0x62) letto a ogni ciclo e un gruppo lento letto a rotazione, un PID per ciclo.
 */
inline void obdLiveConfigure()
{
	obdLiveResetValues();
	for (uint8_t slot = 0; slot < LIVE_SLOT_COUNT; slot++)
		obdLiveSlotEnabled[slot] = obdEngineSupports(obdLiveSlotPid(slot));
	// L'attrito serve solo a correggere la coppia letta dalla centralina
	if (!obdLiveSlotEnabled[LIVE_TORQUE])
		obdLiveSlotEnabled[LIVE_FRICTION] = false;

	obdLiveFastCount = 0;
	if (obdLiveSlotEnabled[LIVE_BOOST])
		obdLiveFastSlots[obdLiveFastCount++] = LIVE_BOOST;
	else if (obdLiveSlotEnabled[LIVE_MAP])
		obdLiveFastSlots[obdLiveFastCount++] = LIVE_MAP;
	const uint8_t fast[] = {LIVE_LOAD, LIVE_RPM, LIVE_MAF, LIVE_TORQUE};
	for (uint8_t i = 0; i < sizeof(fast); i++)
	{
		if (obdLiveSlotEnabled[fast[i]])
			obdLiveFastSlots[obdLiveFastCount++] = fast[i];
	}

	obdLiveSlowCount = 0;
	const uint8_t slow[] = {LIVE_COOLANT, LIVE_IAT, LIVE_BARO, LIVE_RAIL, LIVE_VOLTS, LIVE_HYBRID, LIVE_FRICTION};
	for (uint8_t i = 0; i < sizeof(slow); i++)
	{
		if (obdLiveSlotEnabled[slow[i]])
			obdLiveSlowSlots[obdLiveSlowCount++] = slow[i];
	}
	// Con il boost letto da 0x70 il MAP serve ancora al modello di coppia e all'efficienza volumetrica
	if (obdLiveSlotEnabled[LIVE_BOOST] && obdLiveSlotEnabled[LIVE_MAP])
		obdLiveSlowSlots[obdLiveSlowCount++] = LIVE_MAP;

	Serial.print(F("Lettura live: "));
	Serial.print(engineMultiPid ? F("multi-PID") : F("PID singolo"));
	Serial.print(F(", "));
	Serial.print(obdLiveFastCount);
	Serial.print(F(" PID veloci e "));
	Serial.print(obdLiveSlowCount);
	Serial.println(F(" lenti"));
}

/** Memorizza nella cache il valore letto per uno slot. */
inline void obdLiveApply(uint8_t slot, const uint8_t *data, uint8_t len)
{
	float value;
	if (!obdDecodeScalar(obdLiveSlotPid(slot), data, len, &value))
		return;
	obdLiveSlotFailures[slot] = 0;
	switch (slot)
	{
	case LIVE_BOOST: cachedBoostKpa = value; break;
	case LIVE_MAP:
		cachedMap = value;
		// A motore fermo (quadro acceso, veleggio o start-stop) il collettore è alla pressione ambiente
		if (!isnan(cachedRpm) && cachedRpm < 1.0f)
			engineOffMapKpa = value;
		break;
	case LIVE_LOAD: cachedLoad = value; break;
	case LIVE_RPM: cachedRpm = value; break;
	case LIVE_MAF: cachedMaf = value; break;
	case LIVE_TORQUE: cachedTorquePct = value; break;
	case LIVE_IAT: cachedIat = value; break;
	case LIVE_BARO: cachedBaro = value; break;
	case LIVE_RAIL: cachedRail = value; break;
	case LIVE_VOLTS: cachedVolts = value; break;
	case LIVE_FRICTION: cachedFrictionPct = value; break;
	case LIVE_COOLANT:
		coolantC = (int)lroundf(value);
		coolantAvailable = true;
		break;
	case LIVE_HYBRID:
		hybBattPct = (int)lroundf(value);
		hybBattAvailable = true;
		break;
	}
}

/** Conta una lettura fallita dello slot e, dopo OBD_STALE_AFTER_FAILURES, ne invalida il valore. */
inline void obdLiveFail(uint8_t slot)
{
	if (++obdLiveSlotFailures[slot] < OBD_STALE_AFTER_FAILURES)
		return;
	switch (slot)
	{
	case LIVE_BOOST: cachedBoostKpa = NAN; break;
	case LIVE_MAP: cachedMap = NAN; break;
	case LIVE_LOAD: cachedLoad = NAN; break;
	case LIVE_RPM: cachedRpm = NAN; break;
	case LIVE_MAF: cachedMaf = NAN; break;
	case LIVE_TORQUE: cachedTorquePct = NAN; break;
	case LIVE_IAT: cachedIat = NAN; break;
	case LIVE_BARO: cachedBaro = NAN; break;
	case LIVE_RAIL: cachedRail = NAN; break;
	case LIVE_VOLTS: cachedVolts = NAN; break;
	case LIVE_FRICTION: cachedFrictionPct = NAN; break;
	case LIVE_COOLANT: coolantAvailable = false; break;
	case LIVE_HYBRID: hybBattAvailable = false; break;
	}
}

/** Coppia di riferimento della centralina (0x63), oppure quella di targa se non letta. */
inline float obdReferenceTorqueNm()
{
	return (isnan(engineReferenceTorqueNm) || engineReferenceTorqueNm <= 0.0f) ? Audi40TFSI150kW::TORQUE_MAX_NM
																				 : engineReferenceTorqueNm;
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

/** Pressione ambiente di riferimento per il boost: BARO, oppure il MAP letto a motore fermo. */
inline float obdAmbientPressureKpa(float baroKpa)
{
	return !isnan(baroKpa) ? baroKpa : engineOffMapKpa;
}

/**
 * Ricalcola i valori mostrati: boost e coppia letti dalla centralina quando disponibili, altrimenti
 * stimati dai modelli con la cache; senza dati sufficienti il valore diventa non disponibile.
 */
inline void obdLiveRecalc()
{
	float ambient = obdAmbientPressureKpa(cachedBaro);
	if (obdLiveSlotEnabled[LIVE_BOOST] && !isnan(cachedBoostKpa) && !isnan(ambient))
	{
		boostBar = (cachedBoostKpa - ambient) / 100.0f;
		boostAvailable = true;
		boostMeasured = true;
	}
	else
	{
		float estimate = Audi40TFSI150kW::Boost::estimateTurboPressureBar(cachedMap, cachedBaro, cachedMaf, cachedRpm,
																		  cachedIat, cachedLoad, true, false);
		boostAvailable = !isnan(estimate);
		boostMeasured = false;
		if (boostAvailable)
			boostBar = estimate;
	}

	if (obdLiveSlotEnabled[LIVE_TORQUE] && !isnan(cachedTorquePct))
	{
		// A motore fermo (veleggio, start-stop) l'attrito riportato non genera coppia all'albero
		const bool engineStopped = !isnan(cachedRpm) && cachedRpm < 1.0f;
		torqueNm = engineStopped ? 0 : (int)lroundf(obdShaftTorqueNm(cachedTorquePct, cachedFrictionPct));
		torqueAvailable = true;
		torqueMeasured = true;
	}
	else if (!isnan(cachedLoad) && !isnan(cachedRpm) && !isnan(cachedMaf) && !isnan(cachedMap) && !isnan(cachedIat))
	{
		torqueNm = (int)Audi40TFSI150kW::Torque::estimateEngineTorqueNm(cachedLoad, cachedRpm, cachedMaf, cachedMap,
																	   cachedIat, cachedRail, cachedVolts, cachedBaro,
																	   false);
		torqueAvailable = true;
		torqueMeasured = false;
	}
	else
	{
		torqueAvailable = false;
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
 * Esegue un ciclo di lettura live: una richiesta al motore con gli slot scelti, separazione della
 * risposta, aggiornamento dei valori. Dopo OBD_MULTI_PID_MAX_MALFORMED risposte multi-PID malformate
 * passa definitivamente al PID singolo. Ritorna false se il motore non ha risposto affatto.
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
	bool answered[OBD_MAX_PIDS_PER_REQUEST] = {false};
	if (msg && msg->receivedLen >= 2 && msg->data[0] == 0x41)
	{
		ObdPidField fields[OBD_MAX_PIDS_PER_REQUEST];
		bool wellFormed = false;
		uint8_t fieldCount = obdParseMultiPid(msg->data, msg->receivedLen, fields, OBD_MAX_PIDS_PER_REQUEST, &wellFormed);
		if (count > 1)
		{
			obdLiveMalformedStreak = wellFormed ? 0 : (uint8_t)(obdLiveMalformedStreak + 1);
			if (obdLiveMalformedStreak >= OBD_MULTI_PID_MAX_MALFORMED)
			{
				engineMultiPid = false;
				Serial.println(F("Lettura live: risposte multi-PID malformate, passo al PID singolo"));
			}
		}
		for (uint8_t f = 0; f < fieldCount; f++)
		{
			for (uint8_t i = 0; i < count; i++)
			{
				if (!answered[i] && obdLiveSlotPid(slots[i]) == fields[f].pid)
				{
					obdLiveApply(slots[i], fields[f].data, fields[f].len);
					answered[i] = true;
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
	obdLiveSilentStreak = msg ? 0 : (uint8_t)(obdLiveSilentStreak + 1);
	return msg != nullptr;
}

/** Scrive l'abbreviazione della centralina per il display OLED: "Motore", "Cambio", "Centr. N". */
inline void obdEcuShortName(uint8_t index, char *out, size_t size)
{
	if (index >= ecuCount)
	{
		strncpy(out, "?", size);
		return;
	}
	uint16_t ecuId = ecuResults[index].ecuId;
	if (ecuId == 0x7E8 || ecuId == 0x7E9)
		obdEcuDisplayName(index, out, size);
	else
		snprintf_P(out, size, PSTR("Centr.%u"), (unsigned)(ecuId - OBD_FIRST_RESPONSE_ID + 1));
}

/**
 * Legge MIL e DTC da tutte le centraline: 0101 funzionale per stato MIL e numero di DTC confermati di
 * ciascuna, poi Mode 03 a ogni centralina con almeno un DTC, anche a MIL spenta. Senza nessuna risposta
 * a 0101 lo stato precedente resta invariato, per non far sparire i codici su un timeout.
 */
inline void obdLiveCheckDtcs()
{
	const uint8_t statusRequest[2] = {0x01, 0x01};
	obdTransact(OBD_FUNCTIONAL_REQUEST_ID, statusRequest, 2, OBD_SCAN_WINDOW_MS, 0);
	uint8_t perEcu[OBD_MAX_ECUS] = {0};
	bool anyResponse = false;
	bool mil = false;
	for (uint8_t i = 0; i < ecuCount; i++)
	{
		const ObdIsoTpRx *msg = obdCollectorFind(obdCollector, ecuResults[i].ecuId);
		if (!obdIsPositiveFor(msg, 0x01, 0x01) || msg->receivedLen < 3)
			continue;
		anyResponse = true;
		mil = mil || (msg->data[2] & 0x80) != 0;
		perEcu[i] = msg->data[2] & 0x7F;
	}
	lastDtcCheck = millis();
	dtcCheckRequested = false;
	if (!anyResponse)
		return;

	uint8_t found = 0;
	uint16_t codes[OBD_MAX_DTC];
	uint8_t owners[OBD_MAX_DTC];
	const uint8_t dtcRequest[1] = {0x03};
	for (uint8_t i = 0; i < ecuCount && found < OBD_MAX_DTC; i++)
	{
		if (perEcu[i] == 0)
			continue;
		const ObdIsoTpRx *msg = obdRequestEcu(i, dtcRequest, 1, OBD_SCAN_WINDOW_MS);
		if (!msg)
			continue;
		uint8_t added = obdParseDtcs(msg->data, msg->receivedLen, codes + found, (uint8_t)(OBD_MAX_DTC - found));
		for (uint8_t k = 0; k < added; k++)
			owners[found + k] = i;
		found = (uint8_t)(found + added);
	}
	milOn = mil;
	dtcCount = found;
	memcpy(dtcCodes, codes, sizeof(uint16_t) * found);
	memcpy(dtcEcuIndex, owners, found);

	Serial.print(F("MIL: "));
	Serial.print(milOn ? F("ON") : F("OFF"));
	Serial.print(F(", DTC: "));
	Serial.println(dtcCount);
	for (uint8_t i = 0; i < dtcCount; i++)
	{
		char code[6];
		char owner[16];
		decodeDTC(dtcCodes[i], code);
		obdEcuShortName(dtcEcuIndex[i], owner, sizeof(owner));
		Serial.print(F("  DTC: "));
		Serial.print(code);
		Serial.print(F(" ("));
		Serial.print(owner);
		Serial.println(F(")"));
	}
}

/** Vero quando è ora di rileggere i DTC: allo scadere dell'intervallo o dopo una richiesta esplicita. */
inline bool obdLiveDtcCheckDue()
{
	return dtcCheckRequested || millis() - lastDtcCheck > OBD_DTC_CHECK_INTERVAL_MS;
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
 * Cancella i DTC di tutte le centraline (Mode 04 funzionale) e scrive in message l'esito leggibile:
 * le centraline rifiutano con 7F 04 22 quando il motore è acceso. Ritorna un codice OBD_CLEAR_*.
 */
inline uint8_t obdLiveClearDtcs(char *message, size_t size)
{
	const uint8_t request[1] = {0x04};
	// La cancellazione può richiedere più di P2CAN: le risposte 7F 04 78 estendono comunque l'attesa
	obdTransact(OBD_FUNCTIONAL_REQUEST_ID, request, 1, 300, 0);
	uint8_t accepted = 0;
	uint8_t refused = 0;
	bool engineRunning = false;
	for (uint8_t s = 0; s < obdCollector.used; s++)
	{
		const ObdIsoTpRx &msg = obdCollector.slots[s];
		if (msg.state != OBD_ISOTP_COMPLETE || msg.receivedLen == 0)
			continue;
		uint8_t service, nrc;
		if (msg.data[0] == 0x44)
			accepted++;
		else if (obdIsNegativeResponse(msg.data, msg.receivedLen, &service, &nrc))
		{
			refused++;
			engineRunning = engineRunning || nrc == OBD_NRC_CONDITIONS_NOT_CORRECT;
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
static const uint8_t OBD_DIAG_PIDS[] PROGMEM = {0x04, 0x05, 0x0B, 0x0C, 0x0D, 0x0F, 0x10, 0x23, 0x24,
												0x34, 0x44, 0x33, 0x42, 0x46, 0x49, 0x4A, 0x4C, 0x01,
												0x1F, 0x21, 0x30, 0x70, 0x62, 0x8E, 0x5C, 0x5B, 0x5E};
static constexpr uint8_t OBD_DIAG_PID_COUNT = sizeof(OBD_DIAG_PIDS);

/** Istantanea dei PID diagnostici e delle grandezze calcolate, condivisa da /data e dal Serial. */
struct ObdDiagSnapshot
{
	unsigned long timestampMs;
	float value[OBD_DIAG_PID_COUNT];
	bool has[OBD_DIAG_PID_COUNT];
	float o2Voltage, o2CurrentMa;
	bool hasO2Voltage, hasO2Current;
	bool mil, hasMil;
	int dtcReported;
	// Grandezze calcolate; i flag *Ok indicano se i dati bastavano a calcolarle
	float boostBar, powerKw, afr, fuelLh, fuelL100, airDensity, pressureRatio, gearRatio, accel, boostRate;
	int torqueNm, bsfc, altitude, volEff, driftPedal, deltaThrottlePedal, intercoolerEff;
	bool boostOk, boostMeasured, torqueOk, torqueMeasured, powerOk, afrOk, fuelOk, fuelFromEcu, fuelL100Ok;
	bool bsfcOk, altitudeOk, airDensityOk, pressureRatioOk, volEffOk, driftOk, deltaOk, dfco, dfcoOk;
	bool gearOk, intercoolerOk, accelOk, boostRateOk, engineStopped;
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

/** Memorizza nell'istantanea un PID letto, compresi i campi secondari di 0x01, 0x24 e 0x34. */
inline void obdDiagApply(ObdDiagSnapshot &snap, uint8_t pid, const uint8_t *data, uint8_t len)
{
	if (pid == 0x01 && len >= 1)
	{
		snap.mil = (data[0] & 0x80) != 0;
		snap.dtcReported = data[0] & 0x7F;
		snap.hasMil = true;
		return;
	}
	if (pid == 0x24 && len >= 4)
	{
		snap.o2Voltage = (float)((data[2] << 8) | data[3]) / 8192.0f;
		snap.hasO2Voltage = true;
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
	for (uint8_t f = 0; f < fieldCount; f++)
		obdDiagApply(snap, fields[f].pid, fields[f].data, fields[f].len);
}

/**
 * Calcola le grandezze derivate dell'istantanea con le costanti di EngineConstants.h; previous serve
 * alle derivate (accelerazione, variazione boost), calcolate solo se entrambe le letture sono valide.
 */
inline void obdDiagCompute(ObdDiagSnapshot &s, const ObdDiagSnapshot *previous)
{
	float load = 0, map = 0, rpm = 0, speed = 0, iat = 0, maf = 0, rail = 0, lambda = 0, baro = 0, ambient = 0;
	float pedalD = 0, pedalE = 0, throttle = 0, boostKpa = 0, torquePct = 0, frictionPct = 0, fuelRate = 0, volts = 0;
	const bool hLoad = obdDiagGet(s, 0x04, &load);
	const bool hMap = obdDiagGet(s, 0x0B, &map);
	const bool hRpm = obdDiagGet(s, 0x0C, &rpm);
	const bool hSpeed = obdDiagGet(s, 0x0D, &speed);
	const bool hIat = obdDiagGet(s, 0x0F, &iat);
	const bool hMaf = obdDiagGet(s, 0x10, &maf);
	const bool hRail = obdDiagGet(s, 0x23, &rail);
	const bool hBaro = obdDiagGet(s, 0x33, &baro);
	const bool hAmb = obdDiagGet(s, 0x46, &ambient);
	const bool hLambda = obdDiagGet(s, 0x24, &lambda) || obdDiagGet(s, 0x34, &lambda);
	const bool hPedD = obdDiagGet(s, 0x49, &pedalD);
	const bool hPedE = obdDiagGet(s, 0x4A, &pedalE);
	const bool hThrottle = obdDiagGet(s, 0x4C, &throttle);
	const bool hBoost = obdDiagGet(s, 0x70, &boostKpa);
	const bool hTorque = obdDiagGet(s, 0x62, &torquePct);
	const bool hFriction = obdDiagGet(s, 0x8E, &frictionPct);
	const bool hFuelRate = obdDiagGet(s, 0x5E, &fuelRate);
	const bool hVolts = obdDiagGet(s, 0x42, &volts);

	float ambientKpa = obdAmbientPressureKpa(hBaro ? baro : NAN);
	s.engineStopped = hRpm && rpm < 1.0f;

	s.boostMeasured = hBoost && !isnan(ambientKpa);
	if (s.boostMeasured)
	{
		s.boostBar = (boostKpa - ambientKpa) / 100.0f;
		s.boostOk = true;
	}
	else
	{
		s.boostBar = Audi40TFSI150kW::Boost::estimateTurboPressureBar(hMap ? map : NAN, hBaro ? baro : NAN, hMaf ? maf : NAN,
																	 hRpm ? rpm : NAN, hIat ? iat : NAN,
																	 hLoad ? load : NAN, true, false);
		s.boostOk = !isnan(s.boostBar);
	}

	s.torqueMeasured = hTorque;
	if (hTorque)
	{
		s.torqueNm = s.engineStopped ? 0 : (int)lroundf(obdShaftTorqueNm(torquePct, hFriction ? frictionPct : NAN));
		s.torqueOk = true;
	}
	else
	{
		s.torqueOk = hLoad && hRpm && hMaf && hMap && hIat;
		s.torqueNm = s.torqueOk ? (int)Audi40TFSI150kW::Torque::estimateEngineTorqueNm(
									  load, rpm, maf, map, iat, hRail ? rail : NAN, hVolts ? volts : NAN,
									  hBaro ? baro : NAN, false)
								: 0;
	}

	s.powerOk = s.torqueOk && hRpm && rpm > 0.0f;
	s.powerKw = s.powerOk ? (float)s.torqueNm * rpm / 9549.0f : 0.0f;

	s.afrOk = hLambda;
	s.afr = hLambda ? lambda * Audi40TFSI150kW::FUEL_STOICHIOMETRIC_AFR : 0.0f;

	// Portata di carburante: dalla centralina (0x5E, L/h) se disponibile, altrimenti da MAF e lambda
	s.fuelFromEcu = hFuelRate;
	float fuelGs = 0.0f;
	if (hFuelRate)
	{
		s.fuelLh = fuelRate;
		fuelGs = fuelRate * Audi40TFSI150kW::FUEL_DENSITY_G_PER_L / 3600.0f;
		s.fuelOk = true;
	}
	else if (hMaf)
	{
		float lam = (hLambda && lambda > 0.5f) ? lambda : 1.0f;
		fuelGs = maf / (Audi40TFSI150kW::FUEL_STOICHIOMETRIC_AFR * lam);
		s.fuelLh = fuelGs * 3600.0f / Audi40TFSI150kW::FUEL_DENSITY_G_PER_L;
		s.fuelOk = true;
	}
	else
	{
		s.fuelLh = 0.0f;
		s.fuelOk = false;
	}
	s.fuelL100Ok = s.fuelOk && hSpeed && speed > 3.0f;
	s.fuelL100 = s.fuelL100Ok ? s.fuelLh / speed * 100.0f : 0.0f;
	s.bsfcOk = s.fuelOk && s.powerOk && s.powerKw > 1.0f;
	s.bsfc = s.bsfcOk ? (int)(fuelGs * 3600.0f / s.powerKw) : 0;

	s.altitudeOk = hBaro && baro > 0.0f;
	s.altitude = s.altitudeOk ? (int)(44330.0f * (1.0f - powf(baro / 101.325f, 0.1903f))) : 0;
	s.airDensityOk = hMap && hIat;
	s.airDensity = s.airDensityOk ? (map * 1000.0f) / (Audi40TFSI150kW::GAS_CONSTANT_AIR * (iat + 273.15f)) : 0.0f;

	// Pressione all'uscita del compressore: sensore boost 0x70 prima della farfalla se presente, altrimenti MAP
	float compressorKpa = hBoost ? boostKpa : (hMap ? map : NAN);
	s.pressureRatioOk = !isnan(compressorKpa) && hBaro && baro > 0.0f;
	s.pressureRatio = s.pressureRatioOk ? compressorKpa / baro : 0.0f;

	// Efficienza volumetrica in %: portata d'aria reale su quella teorica alla densità del collettore
	s.volEffOk = hMaf && hRpm && rpm > 0.0f && s.airDensityOk && s.airDensity > 0.0f;
	s.volEff = s.volEffOk ? (int)((maf * 12000.0f) /
								 (Audi40TFSI150kW::ENGINE_DISPLACEMENT_M3 * 1000.0f * rpm * s.airDensity))
						  : 0;

	s.driftOk = hPedD && hPedE;
	s.driftPedal = s.driftOk ? abs((int)pedalD - (int)pedalE) : 0;
	s.deltaOk = hThrottle && hPedD;
	s.deltaThrottlePedal = s.deltaOk ? (int)throttle - (int)pedalD : 0;
	s.dfcoOk = hLoad;
	s.dfco = hLoad && load < 1.0f && !s.engineStopped;

	// Rapporto del solo cambio, al netto della coppia finale: a marcia innestata vale 3.188 ... 0.433
	s.gearOk = hRpm && hSpeed && speed > 5.0f && rpm > 0.0f;
	if (s.gearOk)
	{
		float wheelRpm = speed * 1000.0f / (60.0f * Audi40TFSI150kW::TYRE_ROLLING_CIRCUMFERENCE_M);
		s.gearRatio = rpm / (wheelRpm * Audi40TFSI150kW::FINAL_DRIVE_RATIO);
	}
	else
	{
		s.gearRatio = 0.0f;
	}

	s.intercoolerOk = false;
	s.intercoolerEff = 0;
	if (!isnan(compressorKpa) && hBaro && hIat && hAmb && compressorKpa > baro)
	{
		float theoretical = (ambient + 273.15f) * powf(compressorKpa / baro, 0.286f) - 273.15f;
		float denominator = theoretical - ambient;
		if (denominator > 1.0f)
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
 * valori letti, poi grandezze calcolate, indicando se boost, coppia e consumo sono misurati o stimati.
 */
inline void obdPrintDiagnostics(const ObdDiagSnapshot &s)
{
	Serial.println(F("\n========== DIAGNOSTICA (A5 F5 40 TFSI) =========="));
	obdPrintDiagPid(s, 0x04, F("Carico motore"), 1, 1, F(" %  [minimo: 15-25, pieno carico: 80-100]"));
	obdPrintDiagPid(s, 0x05, F("Temp. liquido"), 1, 0, F(" C  [regime: 90-107]"));
	obdPrintDiagPid(s, 0x5C, F("Temp. olio"), 1, 0, F(" C  [regime: 90-110]"));
	obdPrintDiagPid(s, 0x0B, F("MAP"), 1, 0, F(" kPa  [minimo: 30-40, boost: fino a ~230]"));
	obdPrintDiagPid(s, 0x70, F("Pressione boost"), 1, 1, F(" kPa ass.  [prima della farfalla]"));
	obdPrintDiagPid(s, 0x0C, F("RPM"), 1, 0, F(" rpm  [minimo: 650-750, max: 6500]"));
	obdPrintDiagPid(s, 0x0D, F("Velocità"), 1, 0, F(" km/h"));
	obdPrintDiagPid(s, 0x0F, F("Temp. aria (IAT)"), 1, 0, F(" C  [ambiente +10-30]"));
	obdPrintDiagPid(s, 0x10, F("MAF"), 1, 1, F(" g/s  [minimo: 2-4, pieno carico: ~150]"));
	obdPrintDiagPid(s, 0x23, F("Pressione rail"), 0.01f, 0, F(" bar  [minimo: 70-100, carico: fino a 350]"));
	float lambda = 0.0f;
	const bool hLambda = obdDiagGet(s, 0x24, &lambda) || obdDiagGet(s, 0x34, &lambda);
	obdPrintDiagLine(F("Lambda"), hLambda, lambda, 3, F("  [stechiometrico: 1.000, arricchimento: < 1]"));
	obdPrintDiagPid(s, 0x44, F("Lambda comandata"), 1, 3, F(""));
	obdPrintDiagLine(F("Tensione sonda O2"), s.hasO2Voltage, s.o2Voltage, 3, F(" V"));
	obdPrintDiagLine(F("Corrente sonda O2"), s.hasO2Current, s.o2CurrentMa, 2, F(" mA"));
	obdPrintDiagPid(s, 0x33, F("Pressione baro"), 1, 0, F(" kPa  [livello mare: 101]"));
	obdPrintDiagPid(s, 0x42, F("Batteria 12 V"), 1, 2, F(" V  [ricarica: 13.5-14.9, motore spento: ~12.8]"));
	obdPrintDiagPid(s, 0x5B, F("Batteria ibrida"), 1, 0, F(" %"));
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
	obdPrintDiagLine(s.boostMeasured ? F("Boost (misurato)") : F("Boost (stimato)"), s.boostOk, s.boostBar, 2,
					 F(" bar  [aspirazione: < 0, sovralimentazione: fino a ~1.2]"));
	obdPrintDiagLine(s.torqueMeasured ? F("Coppia all'albero (ECU)") : F("Coppia (stimata)"), s.torqueOk,
					 (float)s.torqueNm, 0, F(" Nm  [max: 320]"));
	obdPrintDiagLine(F("Potenza"), s.powerOk, s.powerKw, 1, F(" kW  [max: 150]"));
	obdPrintDiagLine(F("Potenza"), s.powerOk, s.powerKw * 1.36f, 1, F(" CV  [max: 204]"));
	obdPrintDiagLine(F("Air/Fuel Ratio"), s.afrOk, s.afr, 1, F("  [stechiometrico: 14.7]"));
	obdPrintDiagLine(s.fuelFromEcu ? F("Portata carburante (ECU)") : F("Portata carburante (stimata)"), s.fuelOk, s.fuelLh,
					 2, F(" L/h"));
	obdPrintDiagLine(F("Consumo"), s.fuelL100Ok, s.fuelL100, 1, F(" L/100km"));
	obdPrintDiagLine(F("Cons. specifico"), s.bsfcOk, (float)s.bsfc, 0, F(" g/kWh  [ottimo: 230-260]"));
	obdPrintDiagLine(F("Altitudine"), s.altitudeOk, (float)s.altitude, 0, F(" m"));
	obdPrintDiagLine(F("Densità aria"), s.airDensityOk, s.airDensity, 3, F(" kg/m3"));
	obdPrintDiagLine(F("Eff. intercooler"), s.intercoolerOk, (float)s.intercoolerEff, 0, F(" %  [buono: 60-85]"));
	obdPrintDiagLine(F("Rapporto compressione"), s.pressureRatioOk, s.pressureRatio, 2, F(""));
	obdPrintDiagLine(F("Eff. volumetrica"), s.volEffOk, (float)s.volEff, 0, F(" %  [normale: 80-100]"));
	obdPrintDiagLine(F("Drift pedale"), s.driftOk, (float)s.driftPedal, 0, F(" %  [normale: < 3, allarme: > 5]"));
	obdPrintDiagLine(F("Delta farfalla-pedale"), s.deltaOk, (float)s.deltaThrottlePedal, 0, F(" %"));
	if (s.dfcoOk)
	{
		Serial.print(F("  Taglio iniezione: "));
		Serial.println(s.dfco ? F("ATTIVO") : F("OFF"));
	}
	obdPrintDiagLine(F("Rapporto cambio"), s.gearOk, s.gearRatio, 2, F("  [1a: 3.19 ... 7a: 0.43]"));
	obdPrintDiagLine(F("Accelerazione"), s.accelOk, s.accel, 2, F(" m/s2"));
	obdPrintDiagLine(F("Variazione boost"), s.boostRateOk, s.boostRate, 2, F(" bar/s"));
	Serial.println(F("=================================================\n"));
}

#endif
