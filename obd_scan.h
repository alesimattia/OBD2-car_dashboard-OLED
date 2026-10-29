/**
 * Scansione completa della vettura, avviata solo dal web: centraline OBD e UDS, PID e DID con decodifica pubblica e
 * verificata (tabelle OBDb di obdb_signals.h), monitor Mode 06, identificazione UDS e DTC. Legge solo dati
 * interpretabili, salta le centraline che non rispondono e chiude ogni richiesta entro un tempo massimo; un passo
 * della macchina a stati per giro di loop, così il server web resta servito. Report su Serial, riepilogo nel JSON di
 * /scan-data, profilo motore suggerito per obd_live.h. Contiene anche il contratto del trasporto, che ogni sketch
 * implementa (CAN nativo o ELM327), e i nomi delle centraline.
 *
 * Va incluso dopo serial_logger.h, perchè scrive su Serial.
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
#include "obdb_signals.h"
#include "pid_descriptions.h"
#include "dtc_descriptions.h"

// ============================================================
// Contratto del trasporto
// ============================================================

/**
 * Invia una richiesta single frame e raccoglie in obdCollector le risposte: requestId 0x7DF interroga tutte le
 * centraline OBD, un ID fisico una sola; gli ID a 29 bit portano OBD_ID_EXTENDED. receiveMode (OBD_RX_*) decide
 * quali ID accettare e dove mandare il flow control. Con waitForEcuId diverso da zero ritorna appena il messaggio
 * di quella centralina è completo, nei modi UDS senza waitForEcuId al primo messaggio completo; windowMs è la
 * finestra di ascolto del CAN nativo, mentre l'ELM327 applica i propri tempi. Le attese 7F xx 78 si rinnovano fino a
 * OBD_RESPONSE_PENDING_CAP_MS. Ritorna il numero di messaggi completi.
 * Implementata in CANbus_conn.ino e WIFI_conn.ino.
 */
uint8_t obdTransact(uint32_t requestId, const uint8_t *request, uint8_t requestLen, uint16_t windowMs,
					uint32_t waitForEcuId, uint8_t receiveMode = OBD_RX_OBD11);

static constexpr uint8_t OBD_BURST_UNAVAILABLE = 0xFF;
static constexpr uint8_t OBD_BURST_FAILED = 0xFE;

/**
 * Discovery a raffica: invia 3E 00 a tutti gli ID di requestIds (tutti a 11 bit o tutti a 29 bit) e scrive in
 * responders gli ID che rispondono con 7E 00, 7F 3E o un first frame. Ritorna il numero di ID trovati,
 * OBD_BURST_UNAVAILABLE se il trasporto non la supporta (ELM327) oppure OBD_BURST_FAILED se un invio fallisce o la
 * ricezione va in overflow: la scansione ripiega allora sulle richieste in sequenza.
 * Implementata in CANbus_conn.ino e WIFI_conn.ino.
 */
uint8_t obdTransportDiscoveryBurst(const uint32_t *requestIds, uint16_t count, uint32_t *responders,
								   uint8_t maxResponders);

/**
 * Riporta il trasporto allo stato della lettura live dopo la scansione completa o la sua interruzione: sull'ELM327
 * reinizializza l'adattatore (protocollo, filtri, flow control), sul CAN nativo azzera i flag di overflow.
 * Implementata in CANbus_conn.ino e WIFI_conn.ino.
 */
void obdTransportRestore();

// Esito dell'ultima obdTransact(): un errore di trasporto (CAN ERROR, BUFFER FULL, ?, STOPPED non provocato, TCP
// caduto, invio CAN fallito) è diverso dall'assenza di risposta, che nelle discovery è la norma
static constexpr uint8_t OBD_TRANSPORT_OK = 0;
static constexpr uint8_t OBD_TRANSPORT_ERROR = 1;
inline uint8_t obdTransportLastStatus = OBD_TRANSPORT_OK;

// P2CAN è 50 ms (ISO 15765-4): 150 ms coprono anche la latenza del gateway, 100 ms a regime
static constexpr uint16_t OBD_SCAN_WINDOW_MS = 150;
static constexpr uint16_t OBD_LIVE_WINDOW_MS = 100;

inline ObdCollector obdCollector;
// Capacità del trasporto: sul clone ELM327 senza ATSH o senza ATH1 vengono messe a false dallo sketch
inline bool obdTransportPhysicalAvailable = true;
inline bool obdTransportHeadersAvailable = true;
// Capacità rilevate dal trasporto nei modi UDS e a 29 bit: flow control verso l'ID di richiesta (ATFCSM1 sull'ELM),
// protocollo CAN a 29 bit (ATSP7), gestione autonoma dei 7F xx 78 da parte dell'adattatore
inline bool obdTransportFlowControlAvailable = true;
inline bool obdTransportExtendedAvailable = true;
inline bool obdTransportPendingHandled = true;
// Falso durante la connessione all'ELM327: in quella fase nè la dashboard nè la scansione interrogano il bus
inline bool obdTransportReady = true;
// Vero durante la scansione completa: gli handler web non devono interrogare il bus
inline bool obdBusy = false;
// Ultima lettura del Serial monitor web (/serial-data): la scansione rallenta finchè il browser non ha letto il report
inline uint32_t obdSerialServedSeq = 0;
inline unsigned long obdSerialServedMs = 0;

// Fornita da obd_live.h: PID del motore usati da lettura live e diagnostica, base del profilo suggerito
inline uint8_t obdLiveProfileCandidates(uint8_t *pids, uint8_t maxPids);

// ============================================================
// Nomi delle centraline: elenco ODIS delle coppie UDS VAG (github.com/ConnorHowell/vag-uds-ids), nomi nostri
// ============================================================

/** Coppia richiesta/risposta di una centralina VAG con il suo nome italiano in PROGMEM. */
struct ObdOdisEcu
{
	uint16_t requestId;
	uint16_t responseId;
	const char *name;
};

static const char ODIS_PARKING[] PROGMEM = "Assistente di parcheggio";
static const char ODIS_HIGH_BEAM[] PROGMEM = "Assistente abbaglianti";
static const char ODIS_SPECIAL_2[] PROGMEM = "Funzioni speciali 2";
static const char ODIS_ENGINE[] PROGMEM = "Motore";
static const char ODIS_ENGINE_2[] PROGMEM = "Motore 2";
static const char ODIS_GEARBOX[] PROGMEM = "Cambio";
static const char ODIS_AWD[] PROGMEM = "Trazione integrale";
static const char ODIS_LOCK[] PROGMEM = "Elettronica di bloccaggio";
static const char ODIS_BRAKES[] PROGMEM = "Freni ABS/ESP";
static const char ODIS_ACC[] PROGMEM = "Regolazione distanza ACC";
static const char ODIS_STEER_ANGLE[] PROGMEM = "Sensore angolo di sterzo";
static const char ODIS_DAMPING[] PROGMEM = "Ammortizzatori elettronici";
static const char ODIS_RIDE[] PROGMEM = "Regolazione assetto";
static const char ODIS_ACCESS[] PROGMEM = "Accesso e avviamento (Kessy)";
static const char ODIS_AIRBAG[] PROGMEM = "Airbag";
static const char ODIS_IMMO[] PROGMEM = "Immobilizzatore";
static const char ODIS_SEAT_PASSENGER[] PROGMEM = "Regolazione sedile passeggero";
static const char ODIS_STEER_COLUMN[] PROGMEM = "Elettronica piantone sterzo";
static const char ODIS_ROOF[] PROGMEM = "Tetto elettrico";
static const char ODIS_DASHBOARD[] PROGMEM = "Quadro strumenti";
static const char ODIS_CLIMATE[] PROGMEM = "Climatizzatore";
static const char ODIS_AUX_HEATER[] PROGMEM = "Riscaldamento supplementare";
static const char ODIS_CLIMATE_REAR[] PROGMEM = "Climatizzatore posteriore";
static const char ODIS_CENTRAL[] PROGMEM = "Elettronica centrale";
static const char ODIS_GATEWAY[] PROGMEM = "Gateway";
static const char ODIS_ACTIVE_STEER[] PROGMEM = "Sterzo attivo o display del quadro";
static const char ODIS_STEER_LOCK[] PROGMEM = "Bloccasterzo";
static const char ODIS_POSITION[] PROGMEM = "Rilevamento posizione veicolo";
static const char ODIS_SLIDING_LEFT[] PROGMEM = "Porta scorrevole sinistra";
static const char ODIS_MEDIA_1[] PROGMEM = "Lettore multimediale 1";
static const char ODIS_MEDIA_3[] PROGMEM = "Lettore multimediale 3";
static const char ODIS_AC_COMPRESSOR[] PROGMEM = "Compressore climatizzatore";
static const char ODIS_TACHO[] PROGMEM = "Tachigrafo";
static const char ODIS_DRIVE_MOTOR[] PROGMEM = "Motore elettrico di trazione";
static const char ODIS_BATTERY[] PROGMEM = "Regolazione batteria 12 V";
static const char ODIS_DOOR_DRIVER[] PROGMEM = "Porta lato guida";
static const char ODIS_DOOR_PASSENGER[] PROGMEM = "Porta lato passeggero";
static const char ODIS_PARKING_BRAKE[] PROGMEM = "Freno di stazionamento";
static const char ODIS_STEER_ASSIST[] PROGMEM = "Servosterzo";
static const char ODIS_HEADLIGHTS[] PROGMEM = "Regolazione fari";
static const char ODIS_TYRES[] PROGMEM = "Controllo pressione pneumatici";
static const char ODIS_SEAT_DRIVER[] PROGMEM = "Regolazione sedile guida";
static const char ODIS_COMFORT[] PROGMEM = "Centralina comfort";
static const char ODIS_RADIO[] PROGMEM = "Radio";
static const char ODIS_NAVIGATION[] PROGMEM = "Navigazione";
static const char ODIS_SOUND[] PROGMEM = "Impianto audio";
static const char ODIS_TV[] PROGMEM = "Sintonizzatore TV";
static const char ODIS_TRAILER[] PROGMEM = "Funzioni rimorchio";
static const char ODIS_SENSORS[] PROGMEM = "Elettronica sensori";
static const char ODIS_LANE_CHANGE[] PROGMEM = "Assistente cambio corsia";
static const char ODIS_SPECIAL[] PROGMEM = "Funzioni speciali";
static const char ODIS_INFOTAINMENT[] PROGMEM = "Infotainment";
static const char ODIS_PRETENSIONER_RIGHT[] PROGMEM = "Pretensionatore anteriore destro";
static const char ODIS_CHARGER[] PROGMEM = "Caricabatterie";
static const char ODIS_GEAR_LEVER[] PROGMEM = "Leva del cambio";
static const char ODIS_HUD[] PROGMEM = "Head-up display";
static const char ODIS_NIGHT_VISION[] PROGMEM = "Visione notturna";
static const char ODIS_ARMREST[] PROGMEM = "Bracciolo";
static const char ODIS_TELEMATICS[] PROGMEM = "Telematica";
static const char ODIS_CAMERA[] PROGMEM = "Telecamera di bordo";
static const char ODIS_PHONE[] PROGMEM = "Telefono o comunicazione esterna";
static const char ODIS_SLIDING_RIGHT[] PROGMEM = "Porta scorrevole destra";
static const char ODIS_SEAT_MC_DRIVER[] PROGMEM = "Sedile multicontour guida";
static const char ODIS_SEAT_MC_PASSENGER[] PROGMEM = "Sedile multicontour passeggero";
static const char ODIS_SEAT_MC_REAR_DRIVER[] PROGMEM = "Sedile multicontour posteriore lato guida";
static const char ODIS_ACC_2[] PROGMEM = "Regolazione distanza ACC 2";
static const char ODIS_REAR_CAMERA[] PROGMEM = "Telecamera posteriore";
static const char ODIS_BATTERY_ENERGY[] PROGMEM = "Gestione energia batteria";
static const char ODIS_TAILGATE[] PROGMEM = "Portellone";
static const char ODIS_SEAT_MC_REAR_PASSENGER[] PROGMEM = "Sedile multicontour posteriore lato passeggero";
static const char ODIS_IMAGE[] PROGMEM = "Elaborazione immagini";
static const char ODIS_COMFORT_2[] PROGMEM = "Centralina comfort 2";
static const char ODIS_PRETENSIONER_LEFT[] PROGMEM = "Pretensionatore anteriore sinistro";
static const char ODIS_FRONT_SENSORS[] PROGMEM = "Sensori anteriori di assistenza alla guida";
static const char ODIS_MICRO[] PROGMEM = "Unità microcontrollore";
static const char ODIS_INFOTAINMENT_IF[] PROGMEM = "Interfaccia infotainment";
static const char ODIS_ROOF_2[] PROGMEM = "Tetto elettrico 2";
static const char ODIS_SOUND_ACTUATOR[] PROGMEM = "Attuatore suono strutturale";
static const char ODIS_AUX_DISPLAY[] PROGMEM = "Display ausiliario";
static const char ODIS_BRAKE_REAR_RIGHT[] PROGMEM = "Freno ruota posteriore destra";
static const char ODIS_MOUNTS[] PROGMEM = "Supporti gruppo motopropulsore";
static const char ODIS_BRAKE_REAR_LEFT[] PROGMEM = "Freno ruota posteriore sinistra";
static const char ODIS_DOOR_REAR_DRIVER[] PROGMEM = "Porta posteriore lato guida";
static const char ODIS_REDUCER[] PROGMEM = "Riduttore";
static const char ODIS_DOOR_REAR_PASSENGER[] PROGMEM = "Porta posteriore lato passeggero";
static const char ODIS_BRAKE_SENSORS[] PROGMEM = "Sensori impianto frenante";

// Le centraline con due coppie (abbaglianti, trazione integrale) hanno due righe; gli ID condivisi da due
// componenti ODIS (70A, 716, 732, 76B) hanno un nome che li comprende entrambi, distinto poi da F197
static const ObdOdisEcu OBD_ODIS_ECUS[] PROGMEM = {
	{0x70A, 0x774, ODIS_PARKING}, {0x730, 0x79A, ODIS_HIGH_BEAM}, {0x748, 0x7B2, ODIS_HIGH_BEAM},
	{0x72B, 0x795, ODIS_SPECIAL_2}, {0x7E0, 0x7E8, ODIS_ENGINE}, {0x7E2, 0x7EA, ODIS_ENGINE_2},
	{0x7E1, 0x7E9, ODIS_GEARBOX}, {0x70F, 0x779, ODIS_AWD}, {0x71D, 0x787, ODIS_AWD}, {0x71E, 0x788, ODIS_LOCK},
	{0x713, 0x77D, ODIS_BRAKES}, {0x757, 0x7C1, ODIS_ACC}, {0x751, 0x7BB, ODIS_STEER_ANGLE},
	{0x772, 0x7DC, ODIS_DAMPING}, {0x755, 0x7BF, ODIS_RIDE}, {0x732, 0x79C, ODIS_ACCESS}, {0x715, 0x77F, ODIS_AIRBAG},
	{0x711, 0x77B, ODIS_IMMO}, {0x74D, 0x7B7, ODIS_SEAT_PASSENGER}, {0x70C, 0x776, ODIS_STEER_COLUMN},
	{0x72D, 0x797, ODIS_ROOF}, {0x714, 0x77E, ODIS_DASHBOARD}, {0x746, 0x7B0, ODIS_CLIMATE},
	{0x76A, 0x7D4, ODIS_AUX_HEATER}, {0x71A, 0x784, ODIS_CLIMATE_REAR}, {0x70E, 0x778, ODIS_CENTRAL},
	{0x710, 0x77A, ODIS_GATEWAY}, {0x716, 0x780, ODIS_ACTIVE_STEER}, {0x731, 0x79B, ODIS_STEER_LOCK},
	{0x750, 0x7BA, ODIS_POSITION}, {0x733, 0x79D, ODIS_SLIDING_LEFT}, {0x770, 0x7DA, ODIS_MEDIA_1},
	{0x76E, 0x7D8, ODIS_MEDIA_3}, {0x719, 0x783, ODIS_AC_COMPRESSOR}, {0x771, 0x7DB, ODIS_TACHO},
	{0x7E6, 0x7EE, ODIS_DRIVE_MOTOR}, {0x728, 0x792, ODIS_BATTERY}, {0x74A, 0x7B4, ODIS_DOOR_DRIVER},
	{0x74B, 0x7B5, ODIS_DOOR_PASSENGER}, {0x752, 0x7BC, ODIS_PARKING_BRAKE}, {0x712, 0x77C, ODIS_STEER_ASSIST},
	{0x754, 0x7BE, ODIS_HEADLIGHTS}, {0x70B, 0x775, ODIS_TYRES}, {0x74C, 0x7B6, ODIS_SEAT_DRIVER},
	{0x70D, 0x777, ODIS_COMFORT}, {0x718, 0x782, ODIS_RADIO}, {0x76C, 0x7D6, ODIS_NAVIGATION},
	{0x76F, 0x7D9, ODIS_SOUND}, {0x76D, 0x7D7, ODIS_TV}, {0x747, 0x7B1, ODIS_TRAILER}, {0x721, 0x78B, ODIS_SENSORS},
	{0x74E, 0x7B8, ODIS_LANE_CHANGE}, {0x72C, 0x796, ODIS_SPECIAL}, {0x773, 0x7DD, ODIS_INFOTAINMENT},
	{0x75F, 0x7C9, ODIS_PRETENSIONER_RIGHT}, {0x765, 0x7CF, ODIS_CHARGER}, {0x753, 0x7BD, ODIS_GEAR_LEVER},
	{0x71B, 0x785, ODIS_HUD}, {0x727, 0x791, ODIS_NIGHT_VISION}, {0x739, 0x7A3, ODIS_ARMREST},
	{0x767, 0x7D1, ODIS_TELEMATICS}, {0x726, 0x790, ODIS_CAMERA}, {0x76B, 0x7D5, ODIS_PHONE},
	{0x734, 0x79E, ODIS_SLIDING_RIGHT}, {0x735, 0x79F, ODIS_SEAT_MC_DRIVER}, {0x736, 0x7A0, ODIS_SEAT_MC_PASSENGER},
	{0x737, 0x7A1, ODIS_SEAT_MC_REAR_DRIVER}, {0x756, 0x7C0, ODIS_ACC_2}, {0x769, 0x7D3, ODIS_REAR_CAMERA},
	{0x7E5, 0x7ED, ODIS_BATTERY_ENERGY}, {0x723, 0x78D, ODIS_TAILGATE}, {0x738, 0x7A2, ODIS_SEAT_MC_REAR_PASSENGER},
	{0x758, 0x7C2, ODIS_IMAGE}, {0x745, 0x7AF, ODIS_COMFORT_2}, {0x75E, 0x7C8, ODIS_PRETENSIONER_LEFT},
	{0x74F, 0x7B9, ODIS_FRONT_SENSORS}, {0x763, 0x7CD, ODIS_MICRO}, {0x749, 0x7B3, ODIS_INFOTAINMENT_IF},
	{0x73D, 0x7A7, ODIS_ROOF_2}, {0x71C, 0x786, ODIS_SOUND_ACTUATOR}, {0x73C, 0x7A6, ODIS_AUX_DISPLAY},
	{0x720, 0x78A, ODIS_BRAKE_REAR_RIGHT}, {0x72E, 0x798, ODIS_MOUNTS}, {0x71F, 0x789, ODIS_BRAKE_REAR_LEFT},
	{0x73E, 0x7A8, ODIS_DOOR_REAR_DRIVER}, {0x72A, 0x794, ODIS_REDUCER}, {0x73F, 0x7A9, ODIS_DOOR_REAR_PASSENGER},
	{0x762, 0x7CC, ODIS_BRAKE_SENSORS}};
static constexpr uint8_t OBD_ODIS_COUNT = sizeof(OBD_ODIS_ECUS) / sizeof(OBD_ODIS_ECUS[0]);

/**
 * Cerca la riga ODIS di un ID di richiesta a 11 bit; ritorna OBDB_NONE se fuori elenco.
 * @since 29/10/26 Mattia Alesi
 */
inline uint8_t obdOdisFindRequest(uint32_t requestId)
{
	for (uint8_t i = 0; i < OBD_ODIS_COUNT; i++)
	{
		if (OBD_READ_WORD(&OBD_ODIS_ECUS[i].requestId) == requestId)
			return i;
	}
	return OBDB_NONE;
}

/** Copia la riga ODIS indicata dalla tabella PROGMEM. */
inline ObdOdisEcu obdOdisRow(uint8_t index)
{
	ObdOdisEcu row;
	OBD_MEMCPY(&row, &OBD_ODIS_ECUS[index], sizeof(row));
	return row;
}

/**
 * Scrive il nome di una centralina OBD per display e dashboard dal suo ID di risposta: il nome ODIS della richiesta
 * fisica (7E8 Motore, 7E9 Cambio), altrimenti "Centralina N" numerata dalla posizione dell'ID.
 */
inline void obdEcuDisplayName(uint16_t ecuId, char *out, size_t size)
{
	uint8_t odis = obdIsResponseId(ecuId) ? obdOdisFindRequest(obdPhysicalRequestId(ecuId)) : OBDB_NONE;
	if (odis != OBDB_NONE)
		strncpy_P(out, obdOdisRow(odis).name, size);
	else
		snprintf_P(out, size, PSTR("Centralina %u"), (unsigned)(ecuId - OBD_FIRST_RESPONSE_ID + 1));
	out[size - 1] = '\0';
}

// ============================================================
// Stato della scansione
// ============================================================

static constexpr uint8_t OBD_MAX_UDS_ECUS = 64;
static constexpr uint32_t OBD_SCAN_ECU_BUDGET_MS = 60000;
static constexpr uint8_t OBD_SCAN_SILENCE_LIMIT = 3;
static constexpr uint8_t OBD_SCAN_TRANSPORT_ERROR_LIMIT = 5;
static constexpr uint8_t OBD_SCAN_F4_NRC_LIMIT = 10;
static constexpr uint16_t OBD_DTC_NOT_READ = 0xFFFF;

static constexpr uint8_t UDS_FLAG_MUTE = 0x01;
static constexpr uint8_t UDS_FLAG_SKIP_22 = 0x02;
static constexpr uint8_t UDS_FLAG_SKIP_19 = 0x04;
static constexpr uint8_t UDS_FLAG_BUDGET = 0x08;
static constexpr uint8_t UDS_FLAG_EXTENDED = 0x10;
static constexpr uint8_t UDS_FLAG_SKIP_F4 = 0x20;

/** Centralina UDS trovata dalla discovery, con lo stato delle sue letture e il riepilogo per la pagina /scan. */
struct UdsEcuResult
{
	uint32_t requestId;
	uint32_t responseId;
	uint32_t startedMs; // inizio delle letture, per il budget per centralina
	uint16_t stdPids;   // PID standard letti via F4xx
	uint16_t vagDids;   // DID VAG OBDb letti
	uint16_t dtcConfirmed;
	uint16_t dtcPending;
	uint16_t dtcFailed;
	uint8_t odisIndex; // OBDB_NONE fuori elenco ODIS
	int8_t obdIndex;   // centralina OBD con la stessa coppia di ID, -1 se nessuna
	uint8_t flags;     // UDS_FLAG_*
	uint8_t silence;   // richieste consecutive senza alcuna risposta
	uint8_t f4NrcStreak;
	uint8_t f4LastNrc;
	char systemName[21]; // F197, vuoto se non letto
};

// Fasi della scansione completa, nell'ordine di esecuzione
static constexpr uint8_t SCAN_IDLE = 0;
static constexpr uint8_t SCAN_OBD_FUNCTIONAL = 1;
static constexpr uint8_t SCAN_OBD_PHYSICAL = 2;
static constexpr uint8_t SCAN_OBD_BITMAPS = 3;
static constexpr uint8_t SCAN_OBD_SCALE = 4;
static constexpr uint8_t SCAN_MODE01 = 5;
static constexpr uint8_t SCAN_MULTI_PID = 6;
static constexpr uint8_t SCAN_MODE02 = 7;
static constexpr uint8_t SCAN_MODE06 = 8;
static constexpr uint8_t SCAN_MODE09 = 9;
static constexpr uint8_t SCAN_OBD_DTC = 10;
static constexpr uint8_t SCAN_OBD29 = 11;
static constexpr uint8_t SCAN_UDS11_DISCOVERY = 12;
static constexpr uint8_t SCAN_UDS29_DISCOVERY = 13;
static constexpr uint8_t SCAN_UDS_CONFIRM = 14;
static constexpr uint8_t SCAN_FLOW_CONTROL_TEST = 15;
static constexpr uint8_t SCAN_UDS_READ = 16;
static constexpr uint8_t SCAN_SUMMARY = 17;
static constexpr uint8_t SCAN_PROFILE = 18;
static constexpr uint8_t SCAN_RESTORE = 19;
static constexpr uint8_t SCAN_DONE = 20;

// Letture per centralina UDS dentro SCAN_UDS_READ
static constexpr uint8_t UDS_READ_IDENT = 0;
static constexpr uint8_t UDS_READ_F4 = 1;
static constexpr uint8_t UDS_READ_F6F8 = 2;
static constexpr uint8_t UDS_READ_OBDB = 3;
static constexpr uint8_t UDS_READ_DTC = 4;

// Esito dell'auto-test del flow control sul gateway
static constexpr uint8_t FC_TEST_NOT_RUN = 0;
static constexpr uint8_t FC_TEST_OK = 1;
static constexpr uint8_t FC_TEST_BROKEN = 2;
static constexpr uint8_t FC_TEST_INCONCLUSIVE = 3;

/** Avanzamento della scansione completa, eseguita un passo per ogni giro di loop(). */
struct ObdProbeState
{
	uint8_t phase;
	uint8_t ecu;       // centralina corrente (OBD o UDS secondo la fase)
	uint8_t read;      // lettura corrente dentro SCAN_UDS_READ
	uint16_t item;     // identificatore corrente della fase
	uint8_t sub;       // sotto-passo (seconda richiesta, conferma)
	uint8_t transportErrors;
	uint8_t flowControlTest;
	bool stopRequested;
	bool stopped;
	unsigned long startedMs;
	unsigned long durationMs;
};

inline EcuScanResult ecuResults[OBD_MAX_ECUS];
inline uint8_t ecuCount = 0;
inline UdsEcuResult udsResults[OBD_MAX_UDS_ECUS];
inline uint8_t udsCount = 0;
inline ObdProbeState obdProbe = {SCAN_IDLE, 0, 0, 0, 0, 0, FC_TEST_NOT_RUN, false, false, 0, 0};
// Rilevati dalla scansione, base del profilo suggerito
inline bool scanMultiPid2 = false;
inline bool scanMultiPid6 = false;
inline double scanReferenceTorqueNm = NAN;
inline uint8_t scanObd29Responders = 0;
// OBFCM (InfoType 0x17) del motore, mostrato solo se plausibile
inline bool obfcmAvailable = false;
inline ObdObfcm obfcmData;

// ============================================================
// Stampa del report
// ============================================================

/** Stampa su Serial un byte in esadecimale a due cifre. */
inline void obdPrintHexByte(uint8_t value)
{
	if (value < 0x10)
		Serial.print('0');
	Serial.print(value, HEX);
}

/** Stampa un ID CAN: 3 cifre a 11 bit, 8 a 29 bit. */
inline void obdPrintCanId(uint32_t id)
{
	char text[12];
	if (id & OBD_ID_EXTENDED)
		snprintf_P(text, sizeof(text), PSTR("%08lX"), (unsigned long)(id & 0x1FFFFFFFUL));
	else
		snprintf_P(text, sizeof(text), PSTR("%03lX"), (unsigned long)id);
	Serial.print(text);
}

/** Stampa un testo del blob OBDB_TEXT. */
inline void obdPrintObdbText(uint16_t offset)
{
	if (offset != OBDB_NO_TEXT)
		Serial.print((const __FlashStringHelper *)(OBDB_TEXT + offset));
}

/** Stampa un numero con al più 3 decimali, senza zeri finali inutili. */
inline void obdPrintNumber(double value)
{
	char text[24];
	if (fabs(value) >= 1e9)
		dtostrf(value, 0, 0, text);
	else
		dtostrf(value, 0, 3, text);
	char *dot = strchr(text, '.');
	if (dot)
	{
		char *end = text + strlen(text) - 1;
		while (end > dot && *end == '0')
			*end-- = '\0';
		if (end == dot)
			*end = '\0';
	}
	Serial.print(text);
}

/** Stampa l'intestazione di riga di una centralina OBD: "  [Motore 7E8] ". */
inline void obdPrintObdTag(uint8_t index)
{
	char name[48];
	obdEcuDisplayName(ecuResults[index].ecuId, name, sizeof(name));
	Serial.print(F("  ["));
	Serial.print(name);
	Serial.print(' ');
	obdPrintCanId(ecuResults[index].ecuId);
	Serial.print(F("] "));
}

/** Scrive il nome di una centralina UDS: nome ODIS, altrimenti F197, altrimenti "Centralina senza nome N". */
inline void obdUdsDisplayName(uint8_t index, char *out, size_t size)
{
	const UdsEcuResult &ecu = udsResults[index];
	if (ecu.odisIndex != OBDB_NONE)
		strncpy_P(out, obdOdisRow(ecu.odisIndex).name, size);
	else if (ecu.systemName[0])
		strncpy(out, ecu.systemName, size);
	else
		snprintf_P(out, size, PSTR("Centralina senza nome %u"), (unsigned)(index + 1));
	out[size - 1] = '\0';
}

/** Stampa l'intestazione di riga di una centralina UDS: "  [Gateway 710/77A] ". */
inline void obdPrintUdsTag(uint8_t index)
{
	char name[48];
	obdUdsDisplayName(index, name, sizeof(name));
	Serial.print(F("  ["));
	Serial.print(name);
	Serial.print(' ');
	obdPrintCanId(udsResults[index].requestId);
	Serial.print('/');
	obdPrintCanId(udsResults[index].responseId);
	Serial.print(F("] "));
}

// ============================================================
// Richieste della scansione: un solo punto applica le regole failsafe
// ============================================================

static constexpr uint8_t SCAN_RESULT_POSITIVE = 0;
static constexpr uint8_t SCAN_RESULT_NEGATIVE = 1;
static constexpr uint8_t SCAN_RESULT_SILENT = 2;
static constexpr uint8_t SCAN_RESULT_INCOMPLETE = 3;
static constexpr uint8_t SCAN_RESULT_TRANSPORT = 4;

/** Esito di una richiesta della scansione: messaggio positivo e, per le risposte negative, il codice NRC. */
struct ScanReply
{
	uint8_t result;
	uint8_t nrc;
	const ObdIsoTpRx *message;
};

/**
 * Invia una richiesta e ne classifica l'esito: risposta positiva o negativa di responseId, multi-frame rimasto a
 * metà, silenzio, errore di trasporto. Gli errori di trasporto consecutivi si contano per saltare la fase (regola 5);
 * le attese 7F xx 78 le gestisce già il trasporto. Il messaggio resta valido fino alla richiesta successiva.
 */
inline ScanReply obdScanRequest(uint32_t requestId, uint32_t responseId, uint8_t receiveMode, const uint8_t *request,
								uint8_t requestLen, uint16_t windowMs)
{
	ScanReply reply = {SCAN_RESULT_SILENT, 0, nullptr};
	obdTransact(requestId, request, requestLen, windowMs, responseId, receiveMode);
	if (obdTransportLastStatus == OBD_TRANSPORT_ERROR)
	{
		obdProbe.transportErrors++;
		reply.result = SCAN_RESULT_TRANSPORT;
		return reply;
	}
	obdProbe.transportErrors = 0;
	const ObdIsoTpRx *msg = obdCollectorFind(obdCollector, responseId);
	if (msg && msg->receivedLen >= 1)
	{
		uint8_t service, nrc;
		if (obdIsNegativeResponse(msg->data, msg->receivedLen, &service, &nrc))
		{
			reply.result = SCAN_RESULT_NEGATIVE;
			reply.nrc = nrc;
		}
		else if (msg->data[0] == (uint8_t)(request[0] + 0x40))
		{
			reply.result = SCAN_RESULT_POSITIVE;
			reply.message = msg;
		}
		return reply;
	}
	// Un multi-frame fermo al first frame o interrotto da un frame perso (stato ERROR) è una risposta, non un silenzio
	for (uint8_t i = 0; i < obdCollector.used; i++)
	{
		if (obdCollector.slots[i].ecuId == responseId && obdCollector.slots[i].state != OBD_ISOTP_IDLE)
			reply.result = SCAN_RESULT_INCOMPLETE;
	}
	return reply;
}

/**
 * Richiesta a una centralina UDS con le regole failsafe: una centralina muta, oltre il budget o con il servizio
 * escluso non riceve richieste (ritorna SILENT senza inviare); 3 silenzi di fila la marcano muta (regola 2);
 * NRC 11, 7F, 7E o 12 escludono il resto del servizio (regola 4). Un NRC, una risposta incompleta e una positiva
 * azzerano il conteggio dei silenzi.
 */
inline ScanReply obdScanUdsRequest(uint8_t index, const uint8_t *request, uint8_t requestLen)
{
	UdsEcuResult &ecu = udsResults[index];
	ScanReply skipped = {SCAN_RESULT_SILENT, 0, nullptr};
	const uint8_t serviceFlag = request[0] == 0x22 ? UDS_FLAG_SKIP_22 : request[0] == 0x19 ? UDS_FLAG_SKIP_19 : 0;
	if (ecu.flags & (UDS_FLAG_MUTE | UDS_FLAG_BUDGET | serviceFlag))
		return skipped;
	if (millis() - ecu.startedMs > OBD_SCAN_ECU_BUDGET_MS)
	{
		ecu.flags |= UDS_FLAG_BUDGET;
		obdPrintUdsTag(index);
		Serial.println(F("budget di 60 s esaurito, letture restanti saltate"));
		return skipped;
	}
	const uint8_t mode = (ecu.flags & UDS_FLAG_EXTENDED) ? OBD_RX_UDS29 : OBD_RX_UDS11;
	ScanReply reply = obdScanRequest(ecu.requestId, ecu.responseId, mode, request, requestLen, OBD_SCAN_WINDOW_MS);
	// NRC 21 (centralina occupata): un solo nuovo tentativo, poi l'identificatore si salta
	if (reply.result == SCAN_RESULT_NEGATIVE && reply.nrc == 0x21)
		reply = obdScanRequest(ecu.requestId, ecu.responseId, mode, request, requestLen, OBD_SCAN_WINDOW_MS);
	if (reply.result == SCAN_RESULT_SILENT)
	{
		if (++ecu.silence >= OBD_SCAN_SILENCE_LIMIT)
		{
			ecu.flags |= UDS_FLAG_MUTE;
			obdPrintUdsTag(index);
			Serial.println(F("non risponde più: letture restanti saltate"));
		}
		return reply;
	}
	if (reply.result != SCAN_RESULT_TRANSPORT)
		ecu.silence = 0;
	if (reply.result == SCAN_RESULT_NEGATIVE && serviceFlag &&
		(reply.nrc == 0x11 || reply.nrc == 0x7F || reply.nrc == 0x7E || reply.nrc == 0x12))
	{
		ecu.flags |= serviceFlag;
		obdPrintUdsTag(index);
		Serial.print(F("servizio "));
		obdPrintHexByte(request[0]);
		Serial.print(F(" non supportato (NRC "));
		obdPrintHexByte(reply.nrc);
		Serial.println(F("), richieste restanti saltate"));
	}
	return reply;
}

/**
 * Richiesta a una centralina OBD: fisica se il trasporto lo consente e la centralina risponde in fisico, altrimenti
 * funzionale attendendo solo la sua risposta.
 */
inline ScanReply obdScanObdRequest(uint8_t index, const uint8_t *request, uint8_t requestLen)
{
	const EcuScanResult &ecu = ecuResults[index];
	const uint32_t requestId = (ecu.respondsPhysical && obdTransportPhysicalAvailable) ? obdPhysicalRequestId(ecu.ecuId)
																					   : OBD_FUNCTIONAL_REQUEST_ID;
	return obdScanRequest(requestId, ecu.ecuId, OBD_RX_OBD11, request, requestLen, OBD_SCAN_WINDOW_MS);
}

// ============================================================
// Decodifica e stampa dei segnali OBDb
// ============================================================

/** Copia dalla tabella PROGMEM il descrittore di un comando. */
inline ObdbCommandDef obdbCommand(uint16_t index)
{
	ObdbCommandDef command;
	OBD_MEMCPY(&command, &OBDB_COMMANDS[index], sizeof(command));
	return command;
}

/** Copia dalla tabella PROGMEM il descrittore di un segnale. */
inline ObdbSignalDef obdbSignal(uint16_t index)
{
	ObdbSignalDef signal;
	OBD_MEMCPY(&signal, &OBDB_SIGNALS[index], sizeof(signal));
	return signal;
}

/** Cerca il comando SAE Mode 01 di un PID nella tabella; ritorna OBDB_COMMAND_COUNT se il PID non è selezionato. */
inline uint16_t obdbFindSaeCommand(uint8_t pid)
{
	for (uint16_t i = 0; i < OBDB_COMMAND_COUNT; i++)
	{
		ObdbCommandDef command = obdbCommand(i);
		if ((command.flags & OBDB_COMMAND_SAE) && command.identifier == pid)
			return i;
	}
	return OBDB_COMMAND_COUNT;
}

/** Vero per i PID SAE la cui scala dipende dai massimi del PID 4F (lambda, sonde, MAP) o del 50 (MAF). */
inline bool obdScaleDependsOn4F(uint8_t pid)
{
	return pid == 0x0B || (pid >= 0x24 && pid <= 0x2B) || (pid >= 0x34 && pid <= 0x3B);
}

/**
 * Decodifica e stampa i segnali visibili di un comando OBDb dal payload (byte dopo servizio ed eco): valori con
 * unità, booleani con il testo dell'unità, enumerazioni con la descrizione, il DTC del freeze frame. I bit di
 * supporto non si stampano e nascondono i valori non supportati. Ritorna quanti valori ha stampato.
 */
inline uint8_t obdPrintCommandSignals(uint16_t commandIndex, const uint8_t *payload, uint16_t payloadLen,
									  const char *prefix)
{
	const ObdbCommandDef command = obdbCommand(commandIndex);
	ObdbSignalDef cim;
	const bool hasCim = command.cimIndex != OBDB_NONE;
	if (hasCim)
		cim = obdbSignal(command.firstSignal + command.cimIndex);
	uint8_t printed = 0;
	for (uint8_t s = 0; s < command.signalCount; s++)
	{
		const ObdbSignalDef def = obdbSignal(command.firstSignal + s);
		const uint8_t kind = def.flags & OBDB_KIND_MASK;
		if (kind == OBDB_KIND_GATE)
			continue;
		ObdbSignalDef gate;
		if (def.gateIndex != OBDB_NONE)
			gate = obdbSignal(command.firstSignal + def.gateIndex);
		if (!obdbSignalVisible(def, def.gateIndex != OBDB_NONE ? &gate : nullptr, hasCim ? &cim : nullptr, payload,
							   payloadLen))
			continue;
		uint64_t raw = 0;
		ObdbUnitDef unit;
		OBD_MEMCPY(&unit, &OBDB_UNITS[def.unitIndex], sizeof(unit));
		if (kind == OBDB_KIND_ENUM)
		{
			ObdbEnumDef enumDef;
			OBD_MEMCPY(&enumDef, &OBDB_ENUMS[def.enumIndex], sizeof(enumDef));
			obdbExtractRaw(payload, payloadLen, def.bitOffset, def.bitLength, (def.flags & OBDB_FLAG_BYTES_LSB) != 0, true,
						   &raw);
			const uint16_t text = obdbFindEnumText(OBDB_ENUM_ENTRIES, enumDef, raw);
			if (text == OBDB_NO_TEXT)
				continue;
			Serial.print(prefix);
			obdPrintObdbText(def.nameOffset);
			Serial.print(F(": "));
			obdPrintObdbText(text);
		}
		else if (kind == OBDB_KIND_BOOLEAN)
		{
			if (!obdbExtractRaw(payload, payloadLen, def.bitOffset, def.bitLength, false, false, &raw))
				continue;
			Serial.print(prefix);
			obdPrintObdbText(def.nameOffset);
			Serial.print(F(": "));
			obdPrintObdbText(raw ? unit.trueOffset : unit.falseOffset);
		}
		else if (kind == OBDB_KIND_DTC)
		{
			if (!obdbExtractRaw(payload, payloadLen, def.bitOffset, def.bitLength, false, false, &raw))
				continue;
			char code[6];
			decodeDTC((uint16_t)raw, code);
			Serial.print(prefix);
			obdPrintObdbText(def.nameOffset);
			Serial.print(F(": "));
			Serial.print(raw ? code : "nessuno");
		}
		else
		{
			ObdbScale scale;
			ObdbLimit limit;
			OBD_MEMCPY(&scale, &OBDB_SCALES[def.scaleIndex], sizeof(scale));
			OBD_MEMCPY(&limit, &OBDB_LIMITS[def.limitIndex], sizeof(limit));
			double value = 0;
			const uint8_t status = obdbDecodeValue(def, scale, limit, payload, payloadLen, &value, &raw);
			if (status == OBDB_DECODE_NO_DATA)
				continue;
			Serial.print(prefix);
			obdPrintObdbText(def.nameOffset);
			Serial.print(F(": "));
			if (status == OBDB_DECODE_NULL)
			{
				Serial.print(F("nessun valore"));
			}
			else
			{
				obdPrintNumber(value);
				if (unit.symbolOffset != OBDB_NO_TEXT && pgm_read_byte(OBDB_TEXT + unit.symbolOffset))
				{
					Serial.print(' ');
					obdPrintObdbText(unit.symbolOffset);
				}
				// Il PID SAE 63 è la coppia di riferimento del motore, che il profilo suggerito confronta con quella di targa
				if ((command.flags & OBDB_COMMAND_SAE) && command.identifier == 0x63)
					scanReferenceTorqueNm = value;
			}
		}
		if (!(command.flags & OBDB_COMMAND_SAE))
			Serial.print((command.flags & OBDB_COMMAND_CAPTURED) ? F("  (risposto nelle catture OBDb A5 2022)")
																: F("  (assente nelle catture OBDb A5 2022)"));
		Serial.println();
		printed++;
	}
	return printed;
}

// ============================================================
// Mode 06, Mode 09 e identificazione UDS: nomi e stampa
// ============================================================

/** Scrive il nome italiano di un monitor Mode 06 (MID); ritorna false se lo standard non lo nomina. */
inline bool obdMode06MonitorName(uint8_t mid, char *out, size_t size)
{
	unsigned bank = 0, sensor = 0;
	if (mid >= 0x01 && mid <= 0x10)
	{
		bank = (mid - 0x01) / 4 + 1;
		sensor = (mid - 0x01) % 4 + 1;
		snprintf_P(out, size, PSTR("Sonda lambda banco %u sensore %u"), bank, sensor);
	}
	else if (mid >= 0x41 && mid <= 0x50)
	{
		bank = (mid - 0x41) / 4 + 1;
		sensor = (mid - 0x41) % 4 + 1;
		snprintf_P(out, size, PSTR("Riscaldatore sonda lambda banco %u sensore %u"), bank, sensor);
	}
	else if (mid >= 0x21 && mid <= 0x24)
		snprintf_P(out, size, PSTR("Catalizzatore banco %u"), (unsigned)(mid - 0x20));
	else if (mid >= 0x31 && mid <= 0x34)
		snprintf_P(out, size, PSTR("EGR banco %u"), (unsigned)(mid - 0x30));
	else if (mid >= 0x35 && mid <= 0x38)
		snprintf_P(out, size, PSTR("Fasatura variabile banco %u"), (unsigned)(mid - 0x34));
	else if (mid == 0x39)
		strncpy_P(out, PSTR("Evaporazione, tappo aperto (0.150 in)"), size);
	else if (mid == 0x3A)
		strncpy_P(out, PSTR("Evaporazione (0.090 in)"), size);
	else if (mid == 0x3B)
		strncpy_P(out, PSTR("Evaporazione (0.040 in)"), size);
	else if (mid == 0x3C)
		strncpy_P(out, PSTR("Evaporazione (0.020 in)"), size);
	else if (mid == 0x3D)
		strncpy_P(out, PSTR("Flusso di spurgo evaporazione"), size);
	else if (mid >= 0x61 && mid <= 0x64)
		snprintf_P(out, size, PSTR("Catalizzatore riscaldato banco %u"), (unsigned)(mid - 0x60));
	else if (mid >= 0x71 && mid <= 0x74)
		snprintf_P(out, size, PSTR("Aria secondaria %u"), (unsigned)(mid - 0x70));
	else if (mid >= 0x81 && mid <= 0x84)
		snprintf_P(out, size, PSTR("Sistema carburante banco %u"), (unsigned)(mid - 0x80));
	else if (mid >= 0x85 && mid <= 0x86)
		snprintf_P(out, size, PSTR("Controllo pressione di sovralimentazione banco %u"), (unsigned)(mid - 0x84));
	else if (mid >= 0x90 && mid <= 0x91)
		snprintf_P(out, size, PSTR("Assorbitore NOx banco %u"), (unsigned)(mid - 0x8F));
	else if (mid >= 0x98 && mid <= 0x99)
		snprintf_P(out, size, PSTR("Catalizzatore NOx banco %u"), (unsigned)(mid - 0x97));
	else if (mid == 0xA1)
		strncpy_P(out, PSTR("Mancate accensioni, dati generali"), size);
	else if (mid >= 0xA2 && mid <= 0xAD)
		snprintf_P(out, size, PSTR("Mancate accensioni cilindro %u"), (unsigned)(mid - 0xA1));
	else if (mid >= 0xB0 && mid <= 0xB1)
		snprintf_P(out, size, PSTR("Filtro antiparticolato banco %u"), (unsigned)(mid - 0xAF));
	else
		return false;
	out[size - 1] = '\0';
	return true;
}

static const char OBD_TID_01[] PROGMEM = "Soglia di tensione ricco-magro";
static const char OBD_TID_02[] PROGMEM = "Soglia di tensione magro-ricco";
static const char OBD_TID_03[] PROGMEM = "Tensione bassa per il tempo di commutazione";
static const char OBD_TID_04[] PROGMEM = "Tensione alta per il tempo di commutazione";
static const char OBD_TID_05[] PROGMEM = "Tempo di commutazione ricco-magro";
static const char OBD_TID_06[] PROGMEM = "Tempo di commutazione magro-ricco";
static const char OBD_TID_07[] PROGMEM = "Tensione minima nel ciclo";
static const char OBD_TID_08[] PROGMEM = "Tensione massima nel ciclo";
static const char OBD_TID_09[] PROGMEM = "Tempo fra le transizioni";
static const char OBD_TID_0A[] PROGMEM = "Periodo del sensore";
static const char OBD_TID_0B[] PROGMEM = "Media mancate accensioni ultimi 10 cicli";
static const char OBD_TID_0C[] PROGMEM = "Mancate accensioni nel ciclo";
static const char *const OBD_TID_NAMES[] PROGMEM = {OBD_TID_01, OBD_TID_02, OBD_TID_03, OBD_TID_04,
													 OBD_TID_05, OBD_TID_06, OBD_TID_07, OBD_TID_08,
													 OBD_TID_09, OBD_TID_0A, OBD_TID_0B, OBD_TID_0C};

/** Stampa i test di un monitor Mode 06: esito di tutti, valore e limiti solo per i TID standard 01-0C. */
inline void obdPrintMode06Tests(const uint8_t *msg, uint16_t len, const char *prefix)
{
	ObdMode06Test tests[8];
	uint8_t count = obdParseMode06(msg, len, tests, 8);
	if (count == 0)
		return;
	char name[56];
	if (!obdMode06MonitorName(tests[0].mid, name, sizeof(name)))
		return;
	uint8_t passed = 0, failed = 0, unknown = 0;
	for (uint8_t i = 0; i < count; i++)
	{
		int8_t outcome = obdMode06Passed(tests[i]);
		if (outcome == 1)
			passed++;
		else if (outcome == 0)
			failed++;
		else
			unknown++;
	}
	Serial.print(prefix);
	Serial.print(F("Monitor "));
	Serial.print(name);
	Serial.print(F(": "));
	Serial.print(passed);
	Serial.print(F(" test superati, "));
	Serial.print(failed);
	Serial.print(F(" falliti"));
	if (unknown)
	{
		Serial.print(F(", "));
		Serial.print(unknown);
		Serial.print(F(" con scala non definita"));
	}
	Serial.println();
	for (uint8_t i = 0; i < count; i++)
	{
		ObdUasid uas;
		if (tests[i].tid < 0x01 || tests[i].tid > 0x0C || !obdUasidLookup(tests[i].uasid, &uas))
			continue;
		Serial.print(prefix);
		Serial.print(F("    "));
		Serial.print((const __FlashStringHelper *)pgm_read_ptr(&OBD_TID_NAMES[tests[i].tid - 1]));
		Serial.print(F(": "));
		obdPrintNumber(obdUasidValue(uas, tests[i].value));
		Serial.print(F(" (limiti "));
		obdPrintNumber(obdUasidValue(uas, tests[i].min));
		Serial.print(F(" - "));
		obdPrintNumber(obdUasidValue(uas, tests[i].max));
		Serial.print(F(") "));
		Serial.println((const __FlashStringHelper *)uas.unit);
	}
}

// Contatori IPT di SAE J1979 nell'ordine della risposta: InfoType 08 (accensione comandata) e 0B (compressione)
static const char OBD_IPT_SPARK[] PROGMEM = "OBDCOND,IGNCNTR,CATCOMP1,CATCOND1,CATCOMP2,CATCOND2,O2SCOMP1,O2SCOND1,"
											"O2SCOMP2,O2SCOND2,EGRCOMP,EGRCOND,AIRCOMP,AIRCOND,EVAPCOMP,EVAPCOND,"
											"SO2SCOMP1,SO2SCOND1,SO2SCOMP2,SO2SCOND2";
static const char OBD_IPT_COMPRESSION[] PROGMEM = "OBDCOND,IGNCNTR,HCCATCOMP,HCCATCOND,NCATCOMP,NCATCOND,NADSCOMP,"
												  "NADSCOND,PMCOMP,PMCOND,EGSCOMP,EGSCOND,EGRCOMP,EGRCOND,BPCOMP,"
												  "BPCOND,FUELCOMP,FUELCOND";

/**
 * Stampa una InfoType Mode 09 interpretabile: ID calibrazione (testo, 16 byte ciascuno), nome centralina, contatori
 * IPT con il loro nome, OBFCM solo se plausibile. data è la risposta dopo 49 InfoType.
 */
inline void obdPrintInfoType(uint8_t infoType, const uint8_t *data, uint16_t len, const char *prefix)
{
	char text[24];
	if (infoType == 0x04 && len >= 1)
	{
		for (uint16_t pos = 1; pos + 16 <= len; pos = (uint16_t)(pos + 16))
		{
			if (!obdCopyPrintableText(data + pos, 16, text, sizeof(text)))
				continue;
			Serial.print(prefix);
			Serial.print(F("ID calibrazione software: "));
			Serial.println(text);
		}
	}
	else if (infoType == 0x08 || infoType == 0x0B)
	{
		Serial.print(prefix);
		Serial.print(F("Contatori di completamento dei monitor (IPT):"));
		PGM_P names = infoType == 0x08 ? OBD_IPT_SPARK : OBD_IPT_COMPRESSION;
		for (uint16_t pos = 1; pos + 2 <= len; pos = (uint16_t)(pos + 2))
		{
			char name[12];
			size_t written = 0;
			char c;
			while ((c = (char)pgm_read_byte(names)) != '\0' && c != ',')
			{
				if (written + 1 < sizeof(name))
					name[written++] = c;
				names++;
			}
			if (c == ',')
				names++;
			name[written] = '\0';
			if (written == 0)
				break;
			Serial.print(' ');
			Serial.print(name);
			Serial.print('=');
			Serial.print((unsigned)((data[pos] << 8) | data[pos + 1]));
		}
		Serial.println();
	}
	else if (infoType == 0x17)
	{
		uint8_t message[OBD_MAX_MESSAGE_LEN];
		uint16_t copy = len + 2 < sizeof(message) ? len : sizeof(message) - 2;
		message[0] = 0x49;
		message[1] = 0x17;
		memcpy(message + 2, data, copy);
		ObdObfcm counters;
		if (!obdParseObfcm(message, (uint16_t)(copy + 2), &counters) || !counters.plausible)
		{
			Serial.print(prefix);
			Serial.println(F("Consumo reale OBFCM: layout non plausibile, non mostrato"));
			return;
		}
		obfcmAvailable = true;
		obfcmData = counters;
		Serial.print(prefix);
		Serial.print(F("Consumo reale di vita (OBFCM): "));
		obdPrintNumber(counters.lifetimeDistanceKm);
		Serial.print(F(" km, "));
		obdPrintNumber(counters.lifetimeFuelL);
		Serial.print(F(" L, media "));
		obdPrintNumber(counters.lifetimeFuelL / counters.lifetimeDistanceKm * 100.0);
		Serial.println(F(" L/100 km"));
	}
}

// DID di identificazione UDS con significato pubblico: ISO 14229 e VAG confermati da dump reali; F190 (VIN) escluso
static const uint16_t OBD_IDENT_DIDS[] PROGMEM = {0xF180, 0xF181, 0xF182, 0xF186, 0xF187, 0xF188, 0xF189, 0xF18A,
												  0xF18B, 0xF18C, 0xF18E, 0xF191, 0xF192, 0xF193, 0xF194, 0xF195,
												  0xF196, 0xF197, 0xF198, 0xF199, 0xF19A, 0xF19B, 0xF19C, 0xF19D,
												  0xF19E, 0xF19F, 0xF1A0, 0xF1A2, 0xF1A3, 0xF1AA};
static constexpr uint8_t OBD_IDENT_DID_COUNT = sizeof(OBD_IDENT_DIDS) / sizeof(OBD_IDENT_DIDS[0]);
static const char OBD_IDENT_F180[] PROGMEM = "ID software di boot";
static const char OBD_IDENT_F181[] PROGMEM = "ID software applicativo";
static const char OBD_IDENT_F182[] PROGMEM = "ID dati applicativi";
static const char OBD_IDENT_F186[] PROGMEM = "Sessione diagnostica attiva";
static const char OBD_IDENT_F187[] PROGMEM = "Codice ricambio costruttore";
static const char OBD_IDENT_F188[] PROGMEM = "Numero software costruttore";
static const char OBD_IDENT_F189[] PROGMEM = "Versione software costruttore";
static const char OBD_IDENT_F18A[] PROGMEM = "Fornitore";
static const char OBD_IDENT_F18B[] PROGMEM = "Data di produzione";
static const char OBD_IDENT_F18C[] PROGMEM = "Numero di serie";
static const char OBD_IDENT_F18E[] PROGMEM = "Codice kit di montaggio";
static const char OBD_IDENT_F191[] PROGMEM = "Numero hardware costruttore";
static const char OBD_IDENT_F192[] PROGMEM = "Numero hardware fornitore";
static const char OBD_IDENT_F193[] PROGMEM = "Versione hardware fornitore";
static const char OBD_IDENT_F194[] PROGMEM = "Numero software fornitore";
static const char OBD_IDENT_F195[] PROGMEM = "Versione software fornitore";
static const char OBD_IDENT_F196[] PROGMEM = "Omologazione emissioni";
static const char OBD_IDENT_F197[] PROGMEM = "Nome di sistema";
static const char OBD_IDENT_F198[] PROGMEM = "Codice officina o tester";
static const char OBD_IDENT_F199[] PROGMEM = "Data di programmazione";
static const char OBD_IDENT_F19A[] PROGMEM = "Officina di calibrazione";
static const char OBD_IDENT_F19B[] PROGMEM = "Data di calibrazione";
static const char OBD_IDENT_F19C[] PROGMEM = "Software di calibrazione";
static const char OBD_IDENT_F19D[] PROGMEM = "Data di installazione";
static const char OBD_IDENT_F19E[] PROGMEM = "File ODX";
static const char OBD_IDENT_F19F[] PROGMEM = "Entità";
static const char OBD_IDENT_F1A0[] PROGMEM = "Numero set dati VW";
static const char OBD_IDENT_F1A2[] PROGMEM = "Versione file ODX";
static const char OBD_IDENT_F1A3[] PROGMEM = "Versione hardware VW";
static const char OBD_IDENT_F1AA[] PROGMEM = "Nome di sistema officina VW";
static const char *const OBD_IDENT_NAMES[] PROGMEM = {
	OBD_IDENT_F180, OBD_IDENT_F181, OBD_IDENT_F182, OBD_IDENT_F186, OBD_IDENT_F187, OBD_IDENT_F188, OBD_IDENT_F189,
	OBD_IDENT_F18A, OBD_IDENT_F18B, OBD_IDENT_F18C, OBD_IDENT_F18E, OBD_IDENT_F191, OBD_IDENT_F192, OBD_IDENT_F193,
	OBD_IDENT_F194, OBD_IDENT_F195, OBD_IDENT_F196, OBD_IDENT_F197, OBD_IDENT_F198, OBD_IDENT_F199, OBD_IDENT_F19A,
	OBD_IDENT_F19B, OBD_IDENT_F19C, OBD_IDENT_F19D, OBD_IDENT_F19E, OBD_IDENT_F19F, OBD_IDENT_F1A0, OBD_IDENT_F1A2,
	OBD_IDENT_F1A3, OBD_IDENT_F1AA};

/**
 * Stampa un DID di identificazione: le date in BCD (o come testo), la sessione attiva come nome, gli altri come
 * testo; un contenuto binario non si mostra. Il nome di sistema F197 diventa anche il nome della centralina.
 */
inline void obdPrintIdentification(uint8_t ecuIndex, uint8_t didIndex, const uint8_t *data, uint16_t len)
{
	const uint16_t did = OBD_READ_WORD(&OBD_IDENT_DIDS[didIndex]);
	char text[48];
	bool shown = false;
	if (did == 0xF186 && len == 1)
	{
		PGM_P session = data[0] == 0x01 ? PSTR("predefinita") : data[0] == 0x02 ? PSTR("programmazione")
																: data[0] == 0x03 ? PSTR("estesa")
																				  : nullptr;
		if (session)
		{
			strncpy_P(text, session, sizeof(text));
			shown = true;
		}
	}
	else if (did == 0xF18B || did == 0xF199 || did == 0xF19B || did == 0xF19D)
	{
		shown = obdDecodeBcdDate(data, len, text, sizeof(text)) || obdCopyPrintableText(data, len, text, sizeof(text));
	}
	else
	{
		shown = obdCopyPrintableText(data, len, text, sizeof(text));
	}
	if (!shown)
		return;
	text[sizeof(text) - 1] = '\0';
	if (did == 0xF197)
	{
		strncpy(udsResults[ecuIndex].systemName, text, sizeof(udsResults[ecuIndex].systemName) - 1);
		udsResults[ecuIndex].systemName[sizeof(udsResults[ecuIndex].systemName) - 1] = '\0';
	}
	obdPrintUdsTag(ecuIndex);
	Serial.print((const __FlashStringHelper *)pgm_read_ptr(&OBD_IDENT_NAMES[didIndex]));
	Serial.print(F(": "));
	Serial.println(text);
}

// ============================================================
// Macchina a stati
// ============================================================

/** Vero mentre la scansione completa è in corso. */
inline bool obdProbeRunning()
{
	return obdProbe.phase != SCAN_IDLE && obdProbe.phase != SCAN_DONE;
}

/** Avanzamento percentuale: le fasi OBD fino al 40%, le discovery fino al 55%, le letture UDS fino al 95%. */
inline uint8_t obdProbePercent()
{
	if (obdProbe.phase == SCAN_DONE)
		return 100;
	if (obdProbe.phase <= SCAN_OBD29)
		return (uint8_t)(obdProbe.phase * 40 / SCAN_OBD29);
	if (obdProbe.phase <= SCAN_FLOW_CONTROL_TEST)
		return 55;
	if (obdProbe.phase == SCAN_UDS_READ)
		return (uint8_t)(55 + (udsCount ? 40u * obdProbe.ecu / udsCount : 40u));
	return 97;
}

/** Nome breve della fase corrente per display e pagina. */
inline PGM_P obdProbePhaseName()
{
	switch (obdProbe.phase)
	{
	case SCAN_OBD_FUNCTIONAL:
	case SCAN_OBD_PHYSICAL:
		return PSTR("Centraline OBD");
	case SCAN_OBD_BITMAPS:
	case SCAN_OBD_SCALE:
		return PSTR("PID supportati");
	case SCAN_MODE01:
	case SCAN_MULTI_PID:
		return PSTR("PID Mode 01");
	case SCAN_MODE02:
		return PSTR("Freeze frame");
	case SCAN_MODE06:
		return PSTR("Monitor Mode 06");
	case SCAN_MODE09:
		return PSTR("Info veicolo");
	case SCAN_OBD_DTC:
		return PSTR("DTC OBD");
	case SCAN_OBD29:
	case SCAN_UDS11_DISCOVERY:
	case SCAN_UDS29_DISCOVERY:
	case SCAN_UDS_CONFIRM:
		return PSTR("Ricerca centraline UDS");
	case SCAN_FLOW_CONTROL_TEST:
	case SCAN_UDS_READ:
		return PSTR("Letture UDS");
	case SCAN_DONE:
		return PSTR("Completata");
	default:
		return PSTR("Riepilogo");
	}
}

/** Passa alla fase indicata azzerando indici e sotto-passi. */
inline void obdProbeEnter(uint8_t phase)
{
	obdProbe.phase = phase;
	obdProbe.ecu = 0;
	obdProbe.read = 0;
	obdProbe.item = 0;
	obdProbe.sub = 0;
	obdProbe.transportErrors = 0;
}

/**
 * Regola failsafe 5: dopo OBD_SCAN_TRANSPORT_ERROR_LIMIT errori di trasporto di fila la fase corrente si salta.
 * Ritorna true se la fase è stata saltata.
 */
inline bool obdProbeSkipOnTransportErrors(uint8_t nextPhase)
{
	if (obdProbe.transportErrors < OBD_SCAN_TRANSPORT_ERROR_LIMIT)
		return false;
	Serial.print(F("  Errori di trasporto ripetuti, fase saltata: "));
	Serial.println((const __FlashStringHelper *)obdProbePhaseName());
	obdProbeEnter(nextPhase);
	return true;
}

/** Azzera i risultati e avvia la scansione completa; rifiutata se è già in corso o il trasporto non è pronto. */
inline bool obdProbeStart()
{
	if (obdProbeRunning() || obdBusy || !obdTransportReady)
		return false;
	memset(ecuResults, 0, sizeof(ecuResults));
	ecuCount = 0;
	memset(udsResults, 0, sizeof(udsResults));
	udsCount = 0;
	scanMultiPid2 = scanMultiPid6 = false;
	scanReferenceTorqueNm = NAN;
	scanObd29Responders = 0;
	obfcmAvailable = false;
	obdProbe.flowControlTest = FC_TEST_NOT_RUN;
	obdProbe.stopRequested = false;
	obdProbe.stopped = false;
	obdProbe.startedMs = millis();
	obdProbe.durationMs = 0;
	obdBusy = true;
	obdProbeEnter(SCAN_OBD_FUNCTIONAL);
	Serial.println(F("\n=== SCANSIONE COMPLETA: OBD2 e UDS in sola lettura, solo dati interpretabili ==="));
	return true;
}

/** Chiede l'interruzione: la scansione passa al riepilogo e al ripristino del trasporto al passo successivo. */
inline void obdProbeStop()
{
	if (obdProbeRunning())
		obdProbe.stopRequested = true;
}

/** Cerca una centralina OBD per ID di risposta; -1 se non trovata. */
inline int8_t obdFindEcu(uint16_t ecuId)
{
	for (uint8_t i = 0; i < ecuCount; i++)
	{
		if (ecuResults[i].ecuId == ecuId)
			return (int8_t)i;
	}
	return -1;
}

/** Restituisce l'indice della centralina OBD, aggiungendola se non era ancora nota (-1 se l'elenco è pieno). */
inline int8_t obdFindOrAddEcu(uint16_t ecuId)
{
	int8_t index = obdFindEcu(ecuId);
	if (index >= 0 || ecuCount >= OBD_MAX_ECUS)
		return index;
	memset(&ecuResults[ecuCount], 0, sizeof(EcuScanResult));
	ecuResults[ecuCount].ecuId = ecuId;
	return (int8_t)ecuCount++;
}

/** Restituisce il bitmap dichiarato del servizio OBD indicato (01, 02, 06 o 09). */
inline uint8_t *obdServiceBitmap(EcuScanResult &ecu, uint8_t service)
{
	switch (service)
	{
	case 0x02:
		return ecu.mode02;
	case 0x06:
		return ecu.mode06;
	case 0x09:
		return ecu.mode09;
	default:
		return ecu.mode01;
	}
}

/**
 * Richiesta funzionale a tutte le centraline OBD che rispondono in funzionale, contando gli errori di trasporto per la
 * regola 5. Ritorna false senza inviare se nessuna centralina la ascolterebbe.
 */
inline bool obdScanFunctional(const uint8_t *request, uint8_t requestLen)
{
	bool anyFunctional = false;
	for (uint8_t i = 0; i < ecuCount && !anyFunctional; i++)
		anyFunctional = ecuResults[i].respondsFunctional;
	if (!anyFunctional)
		return false;
	obdTransact(OBD_FUNCTIONAL_REQUEST_ID, request, requestLen, OBD_SCAN_WINDOW_MS, 0);
	if (obdTransportLastStatus == OBD_TRANSPORT_ERROR)
		obdProbe.transportErrors++;
	else
		obdProbe.transportErrors = 0;
	return true;
}

/** Risposta di una centralina alla richiesta funzionale appena inviata, solo se la centralina risponde in funzionale. */
inline const ObdIsoTpRx *obdScanFunctionalReply(uint8_t index)
{
	return ecuResults[index].respondsFunctional ? obdCollectorFind(obdCollector, ecuResults[index].ecuId) : nullptr;
}

/** Fasi 1-2: centraline OBD in funzionale (due 0100) e in fisico (0100 a 7E0-7E7). */
inline void obdProbeStepObdDiscovery()
{
	const uint8_t request[2] = {0x01, 0x00};
	if (obdProbe.phase == SCAN_OBD_FUNCTIONAL)
	{
		obdTransact(OBD_FUNCTIONAL_REQUEST_ID, request, 2, OBD_SCAN_WINDOW_MS, 0);
		for (uint8_t i = 0; i < obdCollector.used; i++)
		{
			const ObdIsoTpRx &slot = obdCollector.slots[i];
			uint8_t four[4];
			if (slot.state != OBD_ISOTP_COMPLETE || !obdParseSupportedRange(slot.data, slot.receivedLen, 0x41, 0x00, four))
				continue;
			int8_t index = obdFindOrAddEcu((uint16_t)slot.ecuId);
			if (index < 0)
				continue;
			ecuResults[index].respondsFunctional = true;
			obdBitmapStoreRange(ecuResults[index].mode01, 0x00, four);
		}
		if (++obdProbe.item >= 2)
			obdProbeEnter(SCAN_OBD_PHYSICAL);
		return;
	}
	if (obdTransportPhysicalAvailable)
	{
		const uint16_t ecuId = (uint16_t)(OBD_FIRST_RESPONSE_ID + obdProbe.item);
		obdTransact(obdPhysicalRequestId(ecuId), request, 2, OBD_SCAN_WINDOW_MS, ecuId);
		const ObdIsoTpRx *msg = obdCollectorFind(obdCollector, ecuId);
		uint8_t four[4];
		if (msg && obdParseSupportedRange(msg->data, msg->receivedLen, 0x41, 0x00, four))
		{
			int8_t index = obdFindOrAddEcu(ecuId);
			if (index >= 0)
			{
				ecuResults[index].respondsPhysical = true;
				obdBitmapStoreRange(ecuResults[index].mode01, 0x00, four);
			}
		}
	}
	if (++obdProbe.item >= 8)
	{
		// Senza header le risposte funzionali non si distinguono per centralina: chi risponde in fisico si interroga solo così
		if (!obdTransportHeadersAvailable)
		{
			for (uint8_t i = 0; i < ecuCount; i++)
			{
				if (ecuResults[i].respondsPhysical)
					ecuResults[i].respondsFunctional = false;
			}
		}
		Serial.print(F("Centraline OBD: "));
		Serial.println(ecuCount);
		for (uint8_t i = 0; i < ecuCount; i++)
		{
			obdPrintObdTag(i);
			Serial.print(F("risponde in funzionale "));
			Serial.print(ecuResults[i].respondsFunctional ? F("sì") : F("no"));
			Serial.print(F(", in fisico "));
			Serial.println(ecuResults[i].respondsPhysical ? F("sì") : F("no"));
		}
		obdProbeEnter(SCAN_OBD_BITMAPS);
	}
}

/**
 * Fase 3: catene delle bitmap di supporto dei servizi 01, 02 (freeze frame 0), 06 e 09, un range per passo: la
 * richiesta funzionale vale per tutte, le centraline che rispondono solo in fisico la ricevono in fisico. Una catena
 * si ferma quando nessuna centralina dichiara il range successivo.
 */
inline void obdProbeStepBitmaps()
{
	static const uint8_t services[4] = {0x01, 0x02, 0x06, 0x09};
	const uint8_t serviceIndex = (uint8_t)(obdProbe.item / 8);
	if (serviceIndex >= 4)
	{
		obdProbeEnter(SCAN_OBD_SCALE);
		return;
	}
	const uint8_t service = services[serviceIndex];
	const uint8_t base = (uint8_t)((obdProbe.item % 8) * 0x20);
	bool expected = base == 0;
	for (uint8_t i = 0; i < ecuCount && !expected; i++)
		expected = obdBitmapHas(obdServiceBitmap(ecuResults[i], service), base);
	if (!expected)
	{
		obdProbe.item = (uint16_t)((serviceIndex + 1) * 8);
		return;
	}
	const uint8_t request[3] = {service, base, 0x00};
	const uint8_t requestLen = service == 0x02 ? 3 : 2;
	obdScanFunctional(request, requestLen);
	for (uint8_t i = 0; i < ecuCount; i++)
	{
		const ObdIsoTpRx *msg = obdScanFunctionalReply(i);
		uint8_t four[4];
		if (msg && obdParseSupportedRange(msg->data, msg->receivedLen, (uint8_t)(service + 0x40), base, four))
			obdBitmapStoreRange(obdServiceBitmap(ecuResults[i], service), base, four);
	}
	for (uint8_t i = 0; i < ecuCount; i++)
	{
		if (ecuResults[i].respondsFunctional || !ecuResults[i].respondsPhysical)
			continue;
		ScanReply reply = obdScanObdRequest(i, request, requestLen);
		uint8_t four[4];
		if (reply.result == SCAN_RESULT_POSITIVE &&
			obdParseSupportedRange(reply.message->data, reply.message->receivedLen, (uint8_t)(service + 0x40), base, four))
			obdBitmapStoreRange(obdServiceBitmap(ecuResults[i], service), base, four);
	}
	obdProbe.item++;
}

/**
 * Fase 4: legge 4F e 50 dove dichiarati. Valori diversi da zero sono massimi non predefiniti, che cambiano la scala di
 * lambda, sonde e MAP (4F) o del MAF (50): quei PID non si mostrano.
 */
inline void obdProbeStepScale()
{
	if (obdProbe.ecu >= ecuCount)
	{
		obdProbeEnter(SCAN_MODE01);
		return;
	}
	EcuScanResult &ecu = ecuResults[obdProbe.ecu];
	const uint8_t pid = obdProbe.sub == 0 ? 0x4F : 0x50;
	if (obdBitmapHas(ecu.mode01, pid))
	{
		const uint8_t request[2] = {0x01, pid};
		ScanReply reply = obdScanObdRequest(obdProbe.ecu, request, 2);
		if (reply.result == SCAN_RESULT_POSITIVE && reply.message->receivedLen >= 6)
		{
			bool nonDefault = false;
			for (uint16_t i = 2; i < reply.message->receivedLen; i++)
				nonDefault = nonDefault || reply.message->data[i] != 0;
			if (pid == 0x4F)
				ecu.scaleFrom4F = nonDefault;
			else
				ecu.scaleFrom50 = nonDefault;
		}
	}
	if (++obdProbe.sub >= 2)
	{
		obdProbe.sub = 0;
		obdProbe.ecu++;
	}
}

/** Stampa i segnali di un comando SAE per la centralina OBD indicata, con le esclusioni di scala da 4F/50. */
inline void obdPrintSaePid(uint8_t ecuIndex, uint16_t commandIndex, uint8_t pid, const uint8_t *payload,
						   uint16_t payloadLen, bool declared, PGM_P label)
{
	EcuScanResult &ecu = ecuResults[ecuIndex];
	char prefix[56];
	char name[24];
	obdEcuDisplayName(ecu.ecuId, name, sizeof(name));
	snprintf_P(prefix, sizeof(prefix), PSTR("  [%s %03X] %S %02X%s "), name, (unsigned)ecu.ecuId, label, (unsigned)pid,
			   declared ? "" : " (non dichiarato)");
	if ((obdScaleDependsOn4F(pid) && ecu.scaleFrom4F) || (pid == 0x10 && ecu.scaleFrom50))
	{
		Serial.print(prefix);
		Serial.println(F("scala non predefinita dichiarata in 4F/50: valore non mostrato"));
		return;
	}
	obdPrintCommandSignals(commandIndex, payload, payloadLen, prefix);
}

/**
 * Fase 5: ogni PID SAE selezionato, dichiarato o no, in funzionale per tutte le centraline insieme; le centraline
 * raggiungibili solo in fisico ricevono in fisico i PID che dichiarano. I PID non dichiarati che rispondono si
 * marcano; quelli del motore alimentano il profilo suggerito.
 */
inline void obdProbeStepMode01()
{
	while (obdProbe.item < OBDB_COMMAND_COUNT && !(obdbCommand(obdProbe.item).flags & OBDB_COMMAND_SAE))
		obdProbe.item++;
	if (obdProbe.item >= OBDB_COMMAND_COUNT)
	{
		obdProbeEnter(SCAN_MULTI_PID);
		return;
	}
	const ObdbCommandDef command = obdbCommand(obdProbe.item);
	const uint8_t pid = (uint8_t)command.identifier;
	const uint8_t request[2] = {0x01, pid};
	obdScanFunctional(request, 2);
	for (uint8_t i = 0; i < ecuCount; i++)
	{
		const ObdIsoTpRx *msg = obdScanFunctionalReply(i);
		if (!obdIsPositiveFor(msg, 0x01, pid))
			continue;
		obdBitmapSet(ecuResults[i].mode01Verified, pid);
		obdPrintSaePid(i, obdProbe.item, pid, msg->data + 2, (uint16_t)(msg->receivedLen - 2),
					   obdBitmapHas(ecuResults[i].mode01, pid), PSTR("PID"));
	}
	for (uint8_t i = 0; i < ecuCount; i++)
	{
		if (ecuResults[i].respondsFunctional || !ecuResults[i].respondsPhysical || !obdBitmapHas(ecuResults[i].mode01, pid))
			continue;
		ScanReply reply = obdScanObdRequest(i, request, 2);
		if (reply.result != SCAN_RESULT_POSITIVE || reply.message->receivedLen < 2 || reply.message->data[1] != pid)
			continue;
		obdBitmapSet(ecuResults[i].mode01Verified, pid);
		obdPrintSaePid(i, obdProbe.item, pid, reply.message->data + 2, (uint16_t)(reply.message->receivedLen - 2), true,
					   PSTR("PID"));
	}
	obdProbe.item++;
}

/**
 * Fase 5bis: prova sul motore di una richiesta con 2 PID e di una con 6 PID dichiarati: le risposte complete e ben
 * formate stabiliscono il modo di lettura del profilo suggerito.
 */
inline void obdProbeStepMultiPid()
{
	int8_t engine = obdFindEcu(OBD_ENGINE_RESPONSE_ID);
	if (engine < 0 || obdProbe.sub >= 2)
	{
		obdProbeEnter(SCAN_MODE02);
		return;
	}
	const uint8_t wanted = obdProbe.sub == 0 ? 2 : 6;
	uint8_t request[1 + OBD_MAX_PIDS_PER_REQUEST] = {0x01};
	uint8_t count = 0;
	uint8_t candidates[32];
	uint8_t candidateCount = obdLiveProfileCandidates(candidates, sizeof(candidates));
	for (uint8_t i = 0; i < candidateCount && count < wanted; i++)
	{
		if (obdBitmapHas(ecuResults[engine].mode01, candidates[i]) && obdPidDataLen(candidates[i]) != 0)
			request[1 + count++] = candidates[i];
	}
	if (count == wanted)
	{
		ScanReply reply = obdScanObdRequest((uint8_t)engine, request, (uint8_t)(1 + count));
		ObdPidField fields[OBD_MAX_PIDS_PER_REQUEST];
		bool wellFormed = false;
		uint8_t fieldCount = reply.result == SCAN_RESULT_POSITIVE
								 ? obdParseMultiPid(reply.message->data, reply.message->receivedLen, fields,
													OBD_MAX_PIDS_PER_REQUEST, &wellFormed)
								 : 0;
		const bool ok = wellFormed && fieldCount == count;
		if (wanted == 2)
			scanMultiPid2 = ok;
		else
			scanMultiPid6 = ok;
		obdPrintObdTag((uint8_t)engine);
		Serial.print(F("richiesta con "));
		Serial.print(wanted);
		Serial.print(F(" PID: "));
		Serial.println(ok ? F("risposta completa") : F("risposta assente o incompleta"));
	}
	else if (wanted == 6)
	{
		// Con meno di 6 PID dichiarati la lettura live non chiede di più: vale l'esito della prova a 2
		scanMultiPid6 = scanMultiPid2;
	}
	obdProbe.sub++;
}

/**
 * Fase 6: freeze frame per centralina. Prima 02 02 00 (DTC che l'ha generato): con 0000 non c'è freeze frame e si
 * passa oltre; poi i PID dichiarati in Mode 02 e selezionati, con la risposta [42 PID 00 dati].
 */
inline void obdProbeStepMode02()
{
	if (obdProbe.ecu >= ecuCount)
	{
		obdProbeEnter(SCAN_MODE06);
		return;
	}
	EcuScanResult &ecu = ecuResults[obdProbe.ecu];
	if (obdProbe.sub == 0)
	{
		bool any = false;
		for (uint16_t pid = 1; pid <= 0xFF && !any; pid++)
			any = !obdIsRangePid((uint8_t)pid) && obdBitmapHas(ecu.mode02, (uint8_t)pid);
		const uint8_t request[3] = {0x02, 0x02, 0x00};
		ScanReply reply = any ? obdScanObdRequest(obdProbe.ecu, request, 3) : ScanReply{SCAN_RESULT_SILENT, 0, nullptr};
		const bool frozen = reply.result == SCAN_RESULT_POSITIVE && reply.message->receivedLen >= 5 &&
							(reply.message->data[3] | reply.message->data[4]) != 0;
		if (!frozen)
		{
			obdProbe.ecu++;
			return;
		}
		obdProbe.sub = 1;
		obdProbe.item = 0;
		return;
	}
	while (obdProbe.item < OBDB_COMMAND_COUNT)
	{
		const ObdbCommandDef command = obdbCommand(obdProbe.item);
		if ((command.flags & OBDB_COMMAND_SAE) && obdBitmapHas(ecu.mode02, (uint8_t)command.identifier))
			break;
		obdProbe.item++;
	}
	if (obdProbe.item >= OBDB_COMMAND_COUNT)
	{
		obdProbe.ecu++;
		obdProbe.sub = 0;
		return;
	}
	const uint8_t pid = (uint8_t)obdbCommand(obdProbe.item).identifier;
	const uint8_t request[3] = {0x02, pid, 0x00};
	ScanReply reply = obdScanObdRequest(obdProbe.ecu, request, 3);
	if (reply.result == SCAN_RESULT_POSITIVE && reply.message->receivedLen >= 3 && reply.message->data[1] == pid)
		obdPrintSaePid(obdProbe.ecu, obdProbe.item, pid, reply.message->data + 3, (uint16_t)(reply.message->receivedLen - 3),
					   true, PSTR("Freeze frame PID"));
	obdProbe.item++;
}

/** Fase 7: monitor Mode 06 dichiarati con nome standard, un MID per passo per ogni centralina. */
inline void obdProbeStepMode06()
{
	if (obdProbe.ecu >= ecuCount)
	{
		obdProbeEnter(SCAN_MODE09);
		return;
	}
	EcuScanResult &ecu = ecuResults[obdProbe.ecu];
	char name[56];
	while (obdProbe.item <= 0xFF &&
		   (obdProbe.item == 0 || obdIsRangePid((uint8_t)obdProbe.item) || !obdBitmapHas(ecu.mode06, (uint8_t)obdProbe.item) ||
			!obdMode06MonitorName((uint8_t)obdProbe.item, name, sizeof(name))))
		obdProbe.item++;
	if (obdProbe.item > 0xFF)
	{
		obdProbe.ecu++;
		obdProbe.item = 0;
		return;
	}
	const uint8_t request[2] = {0x06, (uint8_t)obdProbe.item};
	ScanReply reply = obdScanObdRequest(obdProbe.ecu, request, 2);
	if (reply.result == SCAN_RESULT_POSITIVE)
	{
		char prefix[40];
		char ecuName[24];
		obdEcuDisplayName(ecu.ecuId, ecuName, sizeof(ecuName));
		snprintf_P(prefix, sizeof(prefix), PSTR("  [%s %03X] "), ecuName, (unsigned)ecu.ecuId);
		obdPrintMode06Tests(reply.message->data, reply.message->receivedLen, prefix);
	}
	obdProbe.item++;
}

/** Fase 8: InfoType Mode 09 interpretabili (04, 08, 0A, 0B, 17) dichiarate da ogni centralina. */
inline void obdProbeStepMode09()
{
	static const uint8_t infoTypes[5] = {0x04, 0x08, 0x0A, 0x0B, 0x17};
	if (obdProbe.ecu >= ecuCount)
	{
		obdProbeEnter(SCAN_OBD_DTC);
		return;
	}
	if (obdProbe.item >= sizeof(infoTypes))
	{
		obdProbe.ecu++;
		obdProbe.item = 0;
		return;
	}
	EcuScanResult &ecu = ecuResults[obdProbe.ecu];
	const uint8_t infoType = infoTypes[obdProbe.item++];
	if (!obdBitmapHas(ecu.mode09, infoType))
		return;
	const uint8_t request[2] = {0x09, infoType};
	ScanReply reply = obdScanObdRequest(obdProbe.ecu, request, 2);
	if (reply.result != SCAN_RESULT_POSITIVE || reply.message->receivedLen < 3 || reply.message->data[1] != infoType)
		return;
	char prefix[40];
	char ecuName[24];
	obdEcuDisplayName(ecu.ecuId, ecuName, sizeof(ecuName));
	snprintf_P(prefix, sizeof(prefix), PSTR("  [%s %03X] "), ecuName, (unsigned)ecu.ecuId);
	if (infoType == 0x0A)
	{
		if (obdParseEcuName(reply.message->data, reply.message->receivedLen, ecu.name, sizeof(ecu.name)))
		{
			Serial.print(prefix);
			Serial.print(F("Nome centralina: "));
			Serial.println(ecu.name);
		}
		return;
	}
	obdPrintInfoType(infoType, reply.message->data + 2, (uint16_t)(reply.message->receivedLen - 2), prefix);
}

// Servizi DTC OBD nell'ordine dei tipi di EcuScanResult: confermati, in attesa, permanenti
static const uint8_t OBD_SCAN_DTC_SERVICES[OBD_SCAN_DTC_KINDS] = {0x03, 0x07, 0x0A};

/**
 * Memorizza e stampa i DTC di tipo kind di una centralina dalla sua risposta: positiva con codici e descrizioni,
 * negativa come servizio non supportato; senza risposta i DTC restano non letti, diversi da "nessuno".
 */
inline void obdStoreObdDtcs(uint8_t index, uint8_t kind, const ObdIsoTpRx *msg)
{
	if (!msg || msg->receivedLen < 1)
		return;
	const bool positive = msg->data[0] == (uint8_t)(OBD_SCAN_DTC_SERVICES[kind] + 0x40);
	if (!positive && msg->data[0] != 0x7F)
		return;
	obdPrintObdTag(index);
	Serial.print(kind == 0 ? F("DTC confermati: ") : kind == 1 ? F("DTC in attesa: ") : F("DTC permanenti: "));
	if (!positive)
	{
		Serial.println(F("servizio non supportato"));
		return;
	}
	EcuScanResult &ecu = ecuResults[index];
	uint16_t codes[OBD_SCAN_DTC_PER_KIND];
	const uint8_t found = obdParseDtcs(msg->data, msg->receivedLen, codes, OBD_SCAN_DTC_PER_KIND);
	ecu.dtcRead |= (uint8_t)(1 << kind);
	ecu.dtcCount[kind] = found;
	memcpy(ecu.dtc[kind], codes, sizeof(uint16_t) * found);
	if (found == 0)
		Serial.print(F("nessuno"));
	for (uint8_t k = 0; k < found; k++)
	{
		char code[6];
		decodeDTC(codes[k], code);
		Serial.print(k ? F(", ") : F(""));
		Serial.print(code);
		const char *description = getDTCDescription(codes[k]);
		if (description)
		{
			Serial.print(F(" ("));
			Serial.print((const __FlashStringHelper *)description);
			Serial.print(')');
		}
	}
	Serial.println();
}

/**
 * Fase 9: DTC confermati (03), in attesa (07) e permanenti (0A) di tutte le centraline OBD, un servizio per passo:
 * in funzionale per chi risponde così, in fisico per le altre.
 */
inline void obdProbeStepObdDtc()
{
	if (obdProbe.item >= OBD_SCAN_DTC_KINDS)
	{
		obdProbeEnter(SCAN_OBD29);
		return;
	}
	const uint8_t kind = (uint8_t)obdProbe.item++;
	const uint8_t request[1] = {OBD_SCAN_DTC_SERVICES[kind]};
	obdScanFunctional(request, 1);
	for (uint8_t i = 0; i < ecuCount; i++)
		obdStoreObdDtcs(i, kind, obdScanFunctionalReply(i));
	for (uint8_t i = 0; i < ecuCount; i++)
	{
		if (ecuResults[i].respondsFunctional || !ecuResults[i].respondsPhysical)
			continue;
		ScanReply reply = obdScanObdRequest(i, request, 1);
		const bool answered = reply.result == SCAN_RESULT_POSITIVE || reply.result == SCAN_RESULT_NEGATIVE;
		obdStoreObdDtcs(i, kind, answered ? obdCollectorFind(obdCollector, ecuResults[i].ecuId) : nullptr);
	}
}

/** Fase 10: 0100 sull'indirizzo funzionale a 29 bit, solo per sapere se qualche centralina risponde così. */
inline void obdProbeStepObd29()
{
	const uint8_t request[2] = {0x01, 0x00};
	if (obdTransportExtendedAvailable)
		scanObd29Responders = obdTransact(OBD_FUNCTIONAL_REQUEST_ID_29, request, 2, OBD_SCAN_WINDOW_MS, 0, OBD_RX_OBD29);
	// L'ELM327 può rifiutare ATSP7 proprio in questa richiesta: il flag e l'esito si guardano dopo
	if (!obdTransportExtendedAvailable || obdTransportLastStatus == OBD_TRANSPORT_ERROR)
	{
		Serial.println(F("OBD a 29 bit: non supportato dall'adattatore, saltato"));
	}
	else
	{
		Serial.print(F("OBD a 29 bit: "));
		Serial.print(scanObd29Responders);
		Serial.println(F(" centraline rispondono"));
	}
	obdProbeEnter(SCAN_UDS11_DISCOVERY);
}

/** Restituisce l'indice della centralina UDS con quella coppia, aggiungendola se nuova (-1 se l'elenco è pieno). */
inline int8_t obdUdsFindOrAdd(uint32_t requestId, uint32_t responseId)
{
	for (uint8_t i = 0; i < udsCount; i++)
	{
		if (udsResults[i].requestId == requestId)
			return (int8_t)i;
	}
	if (udsCount >= OBD_MAX_UDS_ECUS)
		return -1;
	UdsEcuResult &ecu = udsResults[udsCount];
	memset(&ecu, 0, sizeof(ecu));
	ecu.requestId = requestId;
	ecu.responseId = responseId;
	ecu.odisIndex = (requestId & OBD_ID_EXTENDED) ? OBDB_NONE : obdOdisFindRequest(requestId);
	ecu.obdIndex = obdIsResponseId(responseId) ? obdFindEcu((uint16_t)responseId) : -1;
	ecu.flags = (requestId & OBD_ID_EXTENDED) ? UDS_FLAG_EXTENDED : 0;
	ecu.dtcConfirmed = ecu.dtcPending = ecu.dtcFailed = OBD_DTC_NOT_READ;
	return (int8_t)udsCount++;
}

/**
 * ID candidati della discovery UDS: a 11 bit le richieste VAG 700-773 e 7E0-7E7 (sopra 773 ci sono le risposte,
 * convenzione richiesta + 6A), a 29 bit 17FC0000-17FC00FF. Ritorna quanti ID ha scritto.
 */
inline uint16_t obdUdsCandidates(bool extended, uint32_t *ids)
{
	uint16_t count = 0;
	if (extended)
	{
		for (uint16_t low = 0; low <= 0xFF; low++)
			ids[count++] = OBD_ID_EXTENDED | 0x17FC0000UL | low;
		return count;
	}
	for (uint32_t id = 0x700; id <= 0x773; id++)
		ids[count++] = id;
	for (uint32_t id = 0x7E0; id <= 0x7E7; id++)
		ids[count++] = id;
	return count;
}

/** Richiesta attesa da una risposta trovata dalla raffica: risposta - 8 per 7E8-7EF, - 6A a 11 bit, 17FC00xx a 29. */
inline uint32_t obdUdsInferRequest(uint32_t responseId)
{
	if (responseId & OBD_ID_EXTENDED)
		return OBD_ID_EXTENDED | 0x17FC0000UL | (responseId & 0xFFUL);
	return obdIsResponseId(responseId) ? responseId - 8 : responseId - 0x6A;
}

// Risposte della raffica in attesa di conferma (fase SCAN_UDS_CONFIRM)
inline uint32_t obdBurstResponders[OBD_MAX_UDS_ECUS];
inline uint8_t obdBurstResponderCount = 0;
// Vero se la discovery a 11 bit è stata fatta a raffica: allora gli ID ODIS muti ricevono 22 F197 in SCAN_UDS_CONFIRM
inline bool obdBurstUsed = false;

/**
 * Fasi 11-12: discovery UDS. Sul CAN nativo una raffica raccoglie le risposte, poi confermate una per una; se la
 * raffica non è disponibile (ELM327) o fallisce, 3E 00 in sequenza a ogni candidato, con attesa mirata sull'ID ODIS
 * atteso. Per gli ID ODIS muti a 3E 00 si prova una volta 22 F197. Valgono 7E 00, 7F 3E, un first frame e 62 F1 97.
 */
inline void obdProbeStepUdsDiscovery()
{
	const bool extended = obdProbe.phase == SCAN_UDS29_DISCOVERY;
	const uint8_t nextPhase = extended ? SCAN_UDS_CONFIRM : SCAN_UDS29_DISCOVERY;
	if (extended && !obdTransportExtendedAvailable)
	{
		Serial.println(F("UDS a 29 bit: non supportato dall'adattatore, saltato"));
		obdProbeEnter(nextPhase);
		return;
	}
	static uint32_t candidates[256];
	const uint16_t count = obdUdsCandidates(extended, candidates);
	if (obdProbe.item == 0 && obdProbe.sub == 0)
	{
		uint32_t responders[OBD_MAX_UDS_ECUS];
		uint8_t found = obdTransportDiscoveryBurst(candidates, count, responders, OBD_MAX_UDS_ECUS);
		if (found != OBD_BURST_UNAVAILABLE && found != OBD_BURST_FAILED)
		{
			if (!extended)
				obdBurstUsed = true;
			for (uint8_t i = 0; i < found && obdBurstResponderCount < OBD_MAX_UDS_ECUS; i++)
				obdBurstResponders[obdBurstResponderCount++] = responders[i];
			Serial.print(extended ? F("UDS a 29 bit (raffica): ") : F("UDS a 11 bit (raffica): "));
			Serial.print(found);
			Serial.println(F(" risposte da confermare"));
			obdProbeEnter(nextPhase);
			return;
		}
		if (found == OBD_BURST_FAILED)
			Serial.println(F("Raffica di discovery non riuscita (invio fallito o overflow): discovery in sequenza"));
		obdProbe.sub = 1;
	}
	if (obdProbe.item >= count)
	{
		obdProbeEnter(nextPhase);
		return;
	}
	const uint32_t requestId = candidates[obdProbe.item];
	const uint8_t odis = extended ? OBDB_NONE : obdOdisFindRequest(requestId);
	const uint32_t expected = odis != OBDB_NONE ? obdOdisRow(odis).responseId : 0;
	const uint8_t mode = extended ? OBD_RX_UDS29 : OBD_RX_UDS11;
	const bool fallback = obdProbe.sub == 2;
	const uint8_t testerPresent[2] = {0x3E, 0x00};
	const uint8_t systemName[3] = {0x22, 0xF1, 0x97};
	obdTransact(requestId, fallback ? systemName : testerPresent, fallback ? 3 : 2, OBD_LIVE_WINDOW_MS, expected, mode);
	if (obdTransportLastStatus == OBD_TRANSPORT_ERROR)
		obdProbe.transportErrors++;
	else
		obdProbe.transportErrors = 0;
	uint32_t responseId = 0;
	for (uint8_t i = 0; i < obdCollector.used && !responseId; i++)
	{
		const ObdIsoTpRx &slot = obdCollector.slots[i];
		if (expected != 0 && slot.ecuId != expected)
			continue;
		const bool firstFrame = slot.state == OBD_ISOTP_RECEIVING;
		const bool complete = slot.state == OBD_ISOTP_COMPLETE && slot.receivedLen >= 2;
		const bool valid = firstFrame || (complete && ((slot.data[0] == 0x7E && slot.data[1] == 0x00) ||
													   (slot.data[0] == 0x7F && (slot.data[1] == 0x3E || slot.data[1] == 0x22)) ||
													   (slot.data[0] == 0x62 && slot.receivedLen >= 3 && slot.data[1] == 0xF1 &&
														slot.data[2] == 0x97)));
		if (valid)
			responseId = slot.ecuId;
	}
	if (responseId != 0)
	{
		// Una coppia fuori elenco ODIS si conferma nella fase SCAN_UDS_CONFIRM, come quelle della raffica
		if (odis != OBDB_NONE)
			obdUdsFindOrAdd(requestId, responseId);
		else if (obdBurstResponderCount < OBD_MAX_UDS_ECUS)
			obdBurstResponders[obdBurstResponderCount++] = responseId;
	}
	if (responseId == 0 && odis != OBDB_NONE && !fallback)
	{
		obdProbe.sub = 2;
		return;
	}
	obdProbe.sub = 1;
	obdProbe.item++;
}

/**
 * Fase 13: conferma delle risposte della raffica e delle coppie fuori elenco ODIS con una seconda richiesta 3E 00
 * alla richiesta ricavata; poi, se la raffica è stata usata, 22 F197 agli ID ODIS non trovati.
 */
inline void obdProbeStepUdsConfirm()
{
	if (obdProbe.item < obdBurstResponderCount)
	{
		const uint32_t responseId = obdBurstResponders[obdProbe.item++];
		const uint32_t requestId = obdUdsInferRequest(responseId);
		const uint8_t mode = (responseId & OBD_ID_EXTENDED) ? OBD_RX_UDS29 : OBD_RX_UDS11;
		const uint8_t testerPresent[2] = {0x3E, 0x00};
		ScanReply reply = obdScanRequest(requestId, responseId, mode, testerPresent, 2, OBD_LIVE_WINDOW_MS);
		if (reply.result == SCAN_RESULT_POSITIVE || reply.result == SCAN_RESULT_NEGATIVE ||
			reply.result == SCAN_RESULT_INCOMPLETE)
			obdUdsFindOrAdd(requestId, responseId);
		return;
	}
	const uint16_t odisItem = (uint16_t)(obdProbe.item - obdBurstResponderCount);
	if (!obdBurstUsed || odisItem >= OBD_ODIS_COUNT)
	{
		Serial.print(F("Centraline UDS trovate: "));
		Serial.println(udsCount);
		obdProbeEnter(SCAN_FLOW_CONTROL_TEST);
		return;
	}
	obdProbe.item++;
	const ObdOdisEcu row = obdOdisRow((uint8_t)odisItem);
	bool known = false;
	for (uint8_t i = 0; i < udsCount && !known; i++)
		known = udsResults[i].requestId == row.requestId;
	if (known)
		return;
	const uint8_t systemName[3] = {0x22, 0xF1, 0x97};
	ScanReply reply = obdScanRequest(row.requestId, row.responseId, OBD_RX_UDS11, systemName, 3, OBD_LIVE_WINDOW_MS);
	if (reply.result == SCAN_RESULT_POSITIVE || reply.result == SCAN_RESULT_NEGATIVE ||
		reply.result == SCAN_RESULT_INCOMPLETE)
		obdUdsFindOrAdd(row.requestId, row.responseId);
}

/**
 * Fase 14: auto-test del flow control sul gateway 710/77A, sempre presente e diverso dalla coppia 7E0/7E8 che l'ELM
 * gestisce già: F187 è più lungo di un frame. Esiti: completo, fermo al first frame, non conclusivo.
 */
inline void obdProbeStepFlowControlTest()
{
	obdProbe.flowControlTest = FC_TEST_INCONCLUSIVE;
	for (uint8_t i = 0; i < udsCount; i++)
	{
		if (udsResults[i].requestId != 0x710)
			continue;
		const uint8_t request[3] = {0x22, 0xF1, 0x87};
		ScanReply reply = obdScanRequest(0x710, udsResults[i].responseId, OBD_RX_UDS11, request, 3, OBD_SCAN_WINDOW_MS);
		if (reply.result == SCAN_RESULT_POSITIVE && reply.message->receivedLen > 7)
			obdProbe.flowControlTest = FC_TEST_OK;
		// Guasto solo se la risposta resta al first frame: un frame perso a metà (stato ERROR) non dice nulla sul flow control
		for (uint8_t s = 0; s < obdCollector.used && reply.result == SCAN_RESULT_INCOMPLETE; s++)
		{
			const ObdIsoTpRx &slot = obdCollector.slots[s];
			if (slot.ecuId == udsResults[i].responseId && slot.state == OBD_ISOTP_RECEIVING && slot.nextSequence == 1)
				obdProbe.flowControlTest = FC_TEST_BROKEN;
		}
	}
	Serial.print(F("Flow control verso le centraline UDS: "));
	Serial.println(obdProbe.flowControlTest == FC_TEST_OK	  ? F("funziona")
				   : obdProbe.flowControlTest == FC_TEST_BROKEN ? F("NON funziona, le risposte lunghe resteranno incomplete")
																: F("non verificabile, si valuta richiesta per richiesta"));
	obdProbeEnter(SCAN_UDS_READ);
}

/** Vero se le letture UDS su più frame vanno saltate perchè l'auto-test ha trovato guasto il flow control (regola 6). */
inline bool obdUdsSingleFrameOnly()
{
	return obdProbe.flowControlTest == FC_TEST_BROKEN;
}

/** Byte di dati dopo servizio ed eco letti dai segnali di un comando OBDb: oltre 4 la risposta non sta in un frame. */
inline uint16_t obdbCommandPayloadBytes(uint16_t commandIndex)
{
	const ObdbCommandDef command = obdbCommand(commandIndex);
	uint16_t bits = 0;
	for (uint8_t s = 0; s < command.signalCount; s++)
	{
		const ObdbSignalDef def = obdbSignal(command.firstSignal + s);
		const uint16_t end = (uint16_t)(def.bitOffset + def.bitLength);
		if (end > bits)
			bits = end;
	}
	return (uint16_t)((bits + 7) / 8);
}

/** Stampa la nota di una risposta UDS incompleta (multi-frame fermo al first frame). */
inline void obdPrintIncomplete(uint8_t ecuIndex, uint16_t did)
{
	obdPrintUdsTag(ecuIndex);
	Serial.print(F("22 "));
	obdPrintHexByte((uint8_t)(did >> 8));
	obdPrintHexByte((uint8_t)did);
	Serial.println(F(": risposta non completata dall'adattatore"));
}

/**
 * Lettura UDS dei PID standard (DID F4xx): con F400 positivo la catena delle bitmap e i PID dichiarati; altrimenti
 * il piccolo insieme visto su centraline VAG non OBD. Saltata per le centraline OBD, che li hanno già dati in Mode 01.
 * 10 NRC uguali di fila chiudono la lettura. Ritorna false quando la lettura è finita.
 */
inline bool obdProbeUdsF4(uint8_t index)
{
	static uint8_t declared[32];
	static bool chainOk;
	static const uint8_t probeSet[6] = {0x05, 0x0C, 0x0D, 0x1B, 0x42, 0x46};
	UdsEcuResult &ecu = udsResults[index];
	if (ecu.obdIndex >= 0 || (ecu.flags & (UDS_FLAG_SKIP_F4 | UDS_FLAG_SKIP_22)))
		return false;
	// Solo all'inizio della catena: anche la seconda sotto-fase riparte da item 0 e deve trovare le bitmap lette
	if (obdProbe.item == 0 && obdProbe.sub == 0)
	{
		memset(declared, 0, sizeof(declared));
		chainOk = false;
	}
	// Catena F400, F420, ... finchè l'ultimo bit dichiara il range successivo
	if (obdProbe.sub == 0)
	{
		const uint8_t base = (uint8_t)(obdProbe.item * 0x20);
		const uint8_t request[3] = {0x22, 0xF4, base};
		ScanReply reply = obdScanUdsRequest(index, request, 3);
		if (reply.result == SCAN_RESULT_POSITIVE && reply.message->receivedLen >= 7)
		{
			chainOk = true;
			obdBitmapStoreRange(declared, base, reply.message->data + 3);
			if (base < 0xE0 && (reply.message->data[6] & 0x01))
			{
				obdProbe.item++;
				return true;
			}
		}
		obdProbe.sub = 1;
		obdProbe.item = 0;
		return true;
	}
	uint8_t pid = 0;
	if (chainOk)
	{
		while (obdProbe.item <= 0xFF &&
			   (obdIsRangePid((uint8_t)obdProbe.item) || !obdBitmapHas(declared, (uint8_t)obdProbe.item) ||
				obdbFindSaeCommand((uint8_t)obdProbe.item) >= OBDB_COMMAND_COUNT))
			obdProbe.item++;
		if (obdProbe.item > 0xFF)
			return false;
		pid = (uint8_t)obdProbe.item;
	}
	else
	{
		while (obdProbe.item < sizeof(probeSet) && obdbFindSaeCommand(probeSet[obdProbe.item]) >= OBDB_COMMAND_COUNT)
			obdProbe.item++;
		if (obdProbe.item >= sizeof(probeSet))
			return false;
		pid = probeSet[obdProbe.item];
	}
	obdProbe.item++;
	if (obdUdsSingleFrameOnly() && (obdPidDataLen(pid) == 0 || obdPidDataLen(pid) > 4))
		return true;
	const uint8_t request[3] = {0x22, 0xF4, pid};
	ScanReply reply = obdScanUdsRequest(index, request, 3);
	if (reply.result == SCAN_RESULT_POSITIVE && reply.message->receivedLen >= 3)
	{
		ecu.f4NrcStreak = 0;
		ecu.stdPids++;
		char prefix[64];
		char name[40];
		obdUdsDisplayName(index, name, sizeof(name));
		snprintf_P(prefix, sizeof(prefix), PSTR("  [%s] PID standard F4%02X "), name, (unsigned)pid);
		obdPrintCommandSignals(obdbFindSaeCommand(pid), reply.message->data + 3, (uint16_t)(reply.message->receivedLen - 3),
							   prefix);
	}
	else if (reply.result == SCAN_RESULT_NEGATIVE)
	{
		ecu.f4NrcStreak = reply.nrc == ecu.f4LastNrc ? (uint8_t)(ecu.f4NrcStreak + 1) : 1;
		ecu.f4LastNrc = reply.nrc;
		if (ecu.f4NrcStreak >= OBD_SCAN_F4_NRC_LIMIT)
			ecu.flags |= UDS_FLAG_SKIP_F4;
	}
	else if (reply.result == SCAN_RESULT_INCOMPLETE)
	{
		obdPrintIncomplete(index, (uint16_t)(0xF400 | pid));
	}
	return true;
}

/**
 * Lettura dei DID VAG OBDb della centralina: i comandi della tabella con la stessa coppia di ID, uno per passo.
 * Ritorna false quando la lettura è finita.
 */
inline bool obdProbeUdsObdb(uint8_t index)
{
	UdsEcuResult &ecu = udsResults[index];
	if (ecu.flags & (UDS_FLAG_SKIP_22 | UDS_FLAG_EXTENDED))
		return false;
	while (obdProbe.item < OBDB_COMMAND_COUNT)
	{
		const ObdbCommandDef command = obdbCommand(obdProbe.item);
		if (!(command.flags & OBDB_COMMAND_SAE) && command.header == ecu.requestId && command.responseId == ecu.responseId &&
			(!obdUdsSingleFrameOnly() || obdbCommandPayloadBytes(obdProbe.item) <= 4))
			break;
		obdProbe.item++;
	}
	if (obdProbe.item >= OBDB_COMMAND_COUNT)
		return false;
	const uint16_t commandIndex = obdProbe.item++;
	const ObdbCommandDef command = obdbCommand(commandIndex);
	// I DID F4xx del dataset A5 ripetono il Mode 01 delle centraline OBD, che li ha già letti
	if ((command.identifier & 0xFF00) == 0xF400 && ecu.obdIndex >= 0)
		return true;
	const uint8_t request[3] = {0x22, (uint8_t)(command.identifier >> 8), (uint8_t)command.identifier};
	ScanReply reply = obdScanUdsRequest(index, request, 3);
	if (reply.result == SCAN_RESULT_POSITIVE && reply.message->receivedLen >= 3)
	{
		char prefix[72];
		char name[40];
		obdUdsDisplayName(index, name, sizeof(name));
		snprintf_P(prefix, sizeof(prefix), PSTR("  [%s] DID %04X "), name, (unsigned)command.identifier);
		if (obdPrintCommandSignals(commandIndex, reply.message->data + 3, (uint16_t)(reply.message->receivedLen - 3),
								   prefix) > 0)
			ecu.vagDids++;
	}
	else if (reply.result == SCAN_RESULT_INCOMPLETE)
	{
		obdPrintIncomplete(index, command.identifier);
	}
	return true;
}

/**
 * Fase 15: letture UDS per centralina, in ordine identificazione, PID standard F4xx, catene F600/F800, DID VAG OBDb,
 * conteggio DTC. Un passo per richiesta; il budget di 60 s parte dalla prima lettura della centralina.
 */
inline void obdProbeStepUdsRead()
{
	if (obdProbe.ecu >= udsCount)
	{
		obdProbeEnter(SCAN_SUMMARY);
		return;
	}
	UdsEcuResult &ecu = udsResults[obdProbe.ecu];
	if (obdProbe.read == UDS_READ_IDENT && obdProbe.item == 0)
		ecu.startedMs = millis();
	bool more = false;
	switch (obdProbe.read)
	{
	case UDS_READ_IDENT:
		if (obdProbe.item < OBD_IDENT_DID_COUNT)
		{
			const uint16_t did = OBD_READ_WORD(&OBD_IDENT_DIDS[obdProbe.item]);
			// Con il flow control guasto resta solo la sessione attiva: gli altri DID sono testi su più frame
			if (!obdUdsSingleFrameOnly() || did == 0xF186)
			{
				const uint8_t request[3] = {0x22, (uint8_t)(did >> 8), (uint8_t)did};
				ScanReply reply = obdScanUdsRequest(obdProbe.ecu, request, 3);
				if (reply.result == SCAN_RESULT_POSITIVE && reply.message->receivedLen > 3)
					obdPrintIdentification(obdProbe.ecu, (uint8_t)obdProbe.item, reply.message->data + 3,
										   (uint16_t)(reply.message->receivedLen - 3));
				else if (reply.result == SCAN_RESULT_INCOMPLETE)
					obdPrintIncomplete(obdProbe.ecu, did);
			}
			obdProbe.item++;
			more = true;
		}
		break;
	case UDS_READ_F4:
		more = obdProbeUdsF4(obdProbe.ecu);
		break;
	case UDS_READ_F6F8:
		if (obdProbe.item < 2)
		{
			const uint8_t request[3] = {0x22, obdProbe.item == 0 ? (uint8_t)0xF6 : (uint8_t)0xF8, 0x00};
			ScanReply reply = obdScanUdsRequest(obdProbe.ecu, request, 3);
			if (reply.result == SCAN_RESULT_POSITIVE)
			{
				obdPrintUdsTag(obdProbe.ecu);
				Serial.println(obdProbe.item == 0 ? F("dichiara monitor OBD via UDS (F600)")
												  : F("dichiara InfoType OBD via UDS (F800)"));
			}
			obdProbe.item++;
			more = true;
		}
		break;
	case UDS_READ_OBDB:
		more = obdProbeUdsObdb(obdProbe.ecu);
		break;
	case UDS_READ_DTC:
		if (obdProbe.item < 3)
		{
			static const uint8_t masks[3] = {0x08, 0x04, 0x01};
			const uint8_t request[3] = {0x19, 0x01, masks[obdProbe.item]};
			ScanReply reply = obdScanUdsRequest(obdProbe.ecu, request, 3);
			uint16_t count;
			if (reply.result == SCAN_RESULT_POSITIVE &&
				obdParseDtcCount(reply.message->data, reply.message->receivedLen, &count))
			{
				if (obdProbe.item == 0)
					ecu.dtcConfirmed = count;
				else if (obdProbe.item == 1)
					ecu.dtcPending = count;
				else
					ecu.dtcFailed = count;
			}
			obdProbe.item++;
			more = true;
		}
		break;
	}
	if (more)
		return;
	obdProbe.item = 0;
	obdProbe.sub = 0;
	if (++obdProbe.read > UDS_READ_DTC)
	{
		obdProbe.read = UDS_READ_IDENT;
		obdProbe.ecu++;
	}
}

/** Stampa e chiude la riga UDS del riepilogo: decisioni failsafe, DTC (solo conteggi), PID standard e DID VAG letti. */
inline void obdPrintUdsSummary(const UdsEcuResult &ecu)
{
	if (ecu.flags & UDS_FLAG_MUTE)
		Serial.print(F("muta dopo la discovery; "));
	if (ecu.flags & UDS_FLAG_BUDGET)
		Serial.print(F("budget esaurito; "));
	Serial.print(F("DTC confermati "));
	if (ecu.dtcConfirmed == OBD_DTC_NOT_READ)
		Serial.print(F("non letti"));
	else
		Serial.print(ecu.dtcConfirmed);
	Serial.print(F(", in attesa "));
	if (ecu.dtcPending == OBD_DTC_NOT_READ)
		Serial.print(F("non letti"));
	else
		Serial.print(ecu.dtcPending);
	if (ecu.dtcFailed != OBD_DTC_NOT_READ)
	{
		Serial.print(F(", falliti ora "));
		Serial.print(ecu.dtcFailed);
	}
	Serial.print(F("; PID standard "));
	Serial.print(ecu.stdPids);
	Serial.print(F(", DID VAG "));
	Serial.println(ecu.vagDids);
}

/**
 * Fase 16: riepilogo per centralina, una per passo: OBD con PID letti e DTC, UDS con DTC (solo conteggi: i codici
 * VAG non seguono la codifica SAE), PID standard e DID VAG letti, decisioni failsafe. Una centralina OBD che risponde
 * anche in UDS (7E0/7E8) compare una volta sola, con la riga UDS sotto quella OBD.
 */
inline void obdProbeStepSummary()
{
	if (obdProbe.item == 0)
	{
		obdProbe.durationMs = millis() - obdProbe.startedMs;
		Serial.print(F("\n=== RIEPILOGO SCANSIONE COMPLETA ("));
		Serial.print(obdProbe.durationMs / 1000);
		Serial.print(F(" s"));
		Serial.print(obdProbe.stopped ? F(", interrotta") : F(""));
		Serial.println(F(") ==="));
	}
	if (obdProbe.item < ecuCount)
	{
		const EcuScanResult &ecu = ecuResults[obdProbe.item];
		obdPrintObdTag((uint8_t)obdProbe.item);
		Serial.print(F("PID dichiarati "));
		Serial.print(obdCountDataIds(ecu.mode01, 0xFF));
		Serial.print(F(", letti "));
		Serial.print(obdCountDataIds(ecu.mode01Verified, 0xFF));
		for (uint8_t kind = 0; kind < OBD_SCAN_DTC_KINDS; kind++)
		{
			Serial.print(kind == 0 ? F("; DTC confermati ") : kind == 1 ? F(", in attesa ") : F(", permanenti "));
			if (ecu.dtcRead & (1 << kind))
				Serial.print(ecu.dtcCount[kind]);
			else
				Serial.print(F("non letti"));
		}
		Serial.println();
		for (uint8_t u = 0; u < udsCount; u++)
		{
			if (udsResults[u].obdIndex != (int8_t)obdProbe.item)
				continue;
			Serial.print(F("      UDS "));
			obdPrintCanId(udsResults[u].requestId);
			Serial.print('/');
			obdPrintCanId(udsResults[u].responseId);
			Serial.print(F(": "));
			obdPrintUdsSummary(udsResults[u]);
		}
	}
	else if (obdProbe.item < ecuCount + udsCount)
	{
		const uint8_t index = (uint8_t)(obdProbe.item - ecuCount);
		if (udsResults[index].obdIndex < 0)
		{
			obdPrintUdsTag(index);
			obdPrintUdsSummary(udsResults[index]);
		}
	}
	else
	{
		obdProbeEnter(SCAN_PROFILE);
		return;
	}
	obdProbe.item++;
}

/** Fase 17: profilo motore suggerito, pronto da incollare nel blocco "Profilo vettura" di obd_live.h. */
inline void obdProbeStepProfile()
{
	int8_t engine = obdFindEcu(OBD_ENGINE_RESPONSE_ID);
	Serial.println(F("\n// Profilo suggerito dalla scansione completa: sostituire il blocco \"Profilo vettura\" di obd_live.h"));
	if (engine < 0)
	{
		Serial.println(F("// Motore 7E8 non trovato: profilo non disponibile"));
	}
	else
	{
		uint8_t candidates[32];
		uint8_t count = obdLiveProfileCandidates(candidates, sizeof(candidates));
		Serial.print(F("static const uint8_t OBD_ENGINE_PIDS[] PROGMEM = {"));
		bool first = true;
		for (uint8_t i = 0; i < count; i++)
		{
			if (!obdBitmapHas(ecuResults[engine].mode01Verified, candidates[i]))
				continue;
			Serial.print(first ? F("0x") : F(", 0x"));
			obdPrintHexByte(candidates[i]);
			first = false;
		}
		Serial.println(F("};"));
		Serial.print(F("static constexpr bool OBD_ENGINE_MULTI_PID = "));
		Serial.println(scanMultiPid6 ? F("true;") : F("false;"));
		if (!isnan(scanReferenceTorqueNm))
		{
			Serial.print(F("// Coppia di riferimento letta (PID 63): "));
			obdPrintNumber(scanReferenceTorqueNm);
			Serial.println(F(" Nm, confrontarla con TORQUE_MAX_NM di EngineConstants.h"));
		}
	}
	obdProbeEnter(SCAN_RESTORE);
}

/**
 * Esegue un passo della scansione completa. Con un'interruzione richiesta salta al riepilogo; finchè il Serial
 * monitor web ha più di 2 KB non letti e il browser li sta leggendo, attende, così il buffer da 4 KB non perde il
 * report (l'ESP-01 non ha la seriale USB).
 */
inline void obdProbeStep()
{
	if (!obdProbeRunning())
		return;
	if (millis() - obdSerialServedMs < 3000 && WebSerial::logHeadSeq() - obdSerialServedSeq > 2048)
		return;
	if (obdProbe.stopRequested && obdProbe.phase < SCAN_SUMMARY)
	{
		obdProbe.stopRequested = false;
		obdProbe.stopped = true;
		Serial.println(F("Scansione interrotta su richiesta"));
		obdProbeEnter(SCAN_SUMMARY);
		return;
	}
	switch (obdProbe.phase)
	{
	case SCAN_OBD_FUNCTIONAL:
	case SCAN_OBD_PHYSICAL:
		obdProbeStepObdDiscovery();
		break;
	case SCAN_OBD_BITMAPS:
		obdProbeStepBitmaps();
		break;
	case SCAN_OBD_SCALE:
		obdProbeStepScale();
		break;
	case SCAN_MODE01:
		obdProbeStepMode01();
		break;
	case SCAN_MULTI_PID:
		obdProbeStepMultiPid();
		break;
	case SCAN_MODE02:
		obdProbeStepMode02();
		break;
	case SCAN_MODE06:
		obdProbeStepMode06();
		break;
	case SCAN_MODE09:
		obdProbeStepMode09();
		break;
	case SCAN_OBD_DTC:
		obdProbeStepObdDtc();
		break;
	case SCAN_OBD29:
		obdProbeStepObd29();
		break;
	case SCAN_UDS11_DISCOVERY:
		if (obdProbe.item == 0 && obdProbe.sub == 0)
		{
			obdBurstResponderCount = 0;
			obdBurstUsed = false;
		}
		obdProbeStepUdsDiscovery();
		break;
	case SCAN_UDS29_DISCOVERY:
		obdProbeStepUdsDiscovery();
		break;
	case SCAN_UDS_CONFIRM:
		obdProbeStepUdsConfirm();
		break;
	case SCAN_FLOW_CONTROL_TEST:
		obdProbeStepFlowControlTest();
		break;
	case SCAN_UDS_READ:
		obdProbeStepUdsRead();
		break;
	case SCAN_SUMMARY:
		obdProbeStepSummary();
		break;
	case SCAN_PROFILE:
		obdProbeStepProfile();
		break;
	case SCAN_RESTORE:
		obdTransportRestore();
		obdProbe.phase = SCAN_DONE;
		obdBusy = false;
		Serial.println(F("=== FINE SCANSIONE COMPLETA ===\n"));
		break;
	}
	// Regola 5 per ogni fase che interroga il bus: le fasi sono numerate nell'ordine di esecuzione
	if (obdProbe.phase < SCAN_SUMMARY)
		obdProbeSkipOnTransportErrors((uint8_t)(obdProbe.phase + 1));
}

// ============================================================
// JSON per la pagina /scan
// ============================================================

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
inline uint16_t obdJsonNameList(ObdJsonStream &json, const uint8_t *present, const uint8_t *absent, uint16_t maxId)
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
		const char *name = getPIDShortName((uint8_t)id);
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

/** Restituisce lo stato della scansione come stringa PROGMEM per il JSON. */
inline PGM_P obdProbeStateName()
{
	if (obdProbeRunning())
		return PSTR("running");
	if (obdProbe.phase == SCAN_DONE)
		return PSTR("done");
	return PSTR("idle");
}

/** Accoda al JSON l'oggetto con stato, avanzamento e fase della scansione. */
inline void obdJsonProbeStatus(ObdJsonStream &json)
{
	json.rawP(PSTR("{\"state\":\""));
	json.rawP(obdProbeStateName());
	json.printfP(PSTR("\",\"percent\":%u,\"seconds\":%lu,\"stopped\":%s,\"phase\":"), (unsigned)obdProbePercent(),
				 (unsigned long)((obdProbeRunning() ? millis() - obdProbe.startedMs : obdProbe.durationMs) / 1000),
				 obdProbe.stopped ? "true" : "false");
	json.string(obdProbePhaseName(), true);
	json.put('}');
}

/**
 * Risponde a /scan-data con il riepilogo della scansione completa: capacità dell'adattatore, esito per il profilo
 * suggerito, per ogni centralina OBD nome, PID per descrizione, PID che rispondono senza essere dichiarati, DTC per
 * codice e descrizione; per ogni centralina UDS nome e conteggi. Solo nomi e testi, senza indirizzi esadecimali.
 * @see web_dashboard.h setupWebDashboard()
 */
inline void obdHandleScanData(ESP8266WebServer &server)
{
	ObdJsonStream json;
	json.begin(server);
	json.rawP(PSTR("{\"probe\":"));
	obdJsonProbeStatus(json);
	json.printfP(PSTR(",\"adapter\":{\"headers\":%s,\"physical\":%s,\"flowControl\":%s,\"pending78\":%s,\"can29\":%s}"),
				 obdTransportHeadersAvailable ? "true" : "false", obdTransportPhysicalAvailable ? "true" : "false",
				 obdProbe.flowControlTest == FC_TEST_BROKEN || !obdTransportFlowControlAvailable ? "false" : "true",
				 obdTransportPendingHandled ? "true" : "false", obdTransportExtendedAvailable ? "true" : "false");
	if (obdProbe.phase == SCAN_DONE && obdFindEcu(OBD_ENGINE_RESPONSE_ID) >= 0)
	{
		json.printfP(PSTR(",\"engine\":{\"multiPid\":%s,\"refTorque\":%d}"),
					 scanMultiPid6 ? "true" : "false",
					 isnan(scanReferenceTorqueNm) ? -1 : (int)lround(scanReferenceTorqueNm));
	}
	if (obfcmAvailable)
	{
		json.printfP(PSTR(",\"obfcm\":{\"km\":%.1f,\"liters\":%.2f,\"avg\":%.2f}"), obfcmData.lifetimeDistanceKm,
					 obfcmData.lifetimeFuelL, obfcmData.lifetimeFuelL / obfcmData.lifetimeDistanceKm * 100.0f);
	}
	json.rawP(PSTR(",\"ecus\":["));
	for (uint8_t i = 0; i < ecuCount; i++)
	{
		const EcuScanResult &ecu = ecuResults[i];
		char displayName[40];
		obdEcuDisplayName(ecu.ecuId, displayName, sizeof(displayName));
		if (i > 0)
			json.put(',');
		json.rawP(PSTR("{\"name\":"));
		json.string(displayName, false);
		json.rawP(PSTR(",\"detail\":"));
		json.string(ecu.name, false);
		json.printfP(PSTR(",\"physical\":%s,\"dtcRead\":%s,\"pids\":"), ecu.respondsPhysical ? "true" : "false",
					 (ecu.dtcRead & 0x01) ? "true" : "false");
		uint16_t undocumented = obdJsonNameList(json, ecu.mode01Verified, nullptr, 0xFF);
		json.printfP(PSTR(",\"undoc\":%u,\"silent\":"), (unsigned)undocumented);
		obdJsonNameList(json, ecu.mode01, ecu.mode01Verified, 0xFF);
		json.rawP(PSTR(",\"extra\":"));
		obdJsonNameList(json, ecu.mode01Verified, ecu.mode01, 0xFF);
		json.rawP(PSTR(",\"dtc\":["));
		bool first = true;
		for (uint8_t kind = 0; kind < OBD_SCAN_DTC_KINDS; kind++)
		{
			for (uint8_t k = 0; k < ecu.dtcCount[kind]; k++)
			{
				char code[6];
				decodeDTC(ecu.dtc[kind][k], code);
				if (!first)
					json.put(',');
				first = false;
				json.printfP(PSTR("{\"code\":\"%s\",\"kind\":%u,\"desc\":"), code, (unsigned)kind);
				const char *description = getDTCDescription(ecu.dtc[kind][k]);
				json.string(description ? description : PSTR(""), true);
				json.put('}');
			}
		}
		json.rawP(PSTR("]}"));
	}
	json.rawP(PSTR("],\"uds\":["));
	for (uint8_t i = 0; i < udsCount; i++)
	{
		const UdsEcuResult &ecu = udsResults[i];
		char displayName[48];
		obdUdsDisplayName(i, displayName, sizeof(displayName));
		if (i > 0)
			json.put(',');
		json.rawP(PSTR("{\"name\":"));
		json.string(displayName, false);
		json.rawP(PSTR(",\"system\":"));
		json.string(ecu.systemName, false);
		// obd è l'indice in ecus della stessa centralina vista in OBD (7E0/7E8), che la pagina mostra una volta sola
		json.printfP(PSTR(",\"obd\":%d,\"dtcConfirmed\":%d,\"dtcPending\":%d,\"stdPids\":%u,\"vagDids\":%u,\"silent\":%s}"),
					 (int)ecu.obdIndex, ecu.dtcConfirmed == OBD_DTC_NOT_READ ? -1 : (int)ecu.dtcConfirmed,
					 ecu.dtcPending == OBD_DTC_NOT_READ ? -1 : (int)ecu.dtcPending, (unsigned)ecu.stdPids,
					 (unsigned)ecu.vagDids, (ecu.flags & UDS_FLAG_MUTE) ? "true" : "false");
	}
	json.rawP(PSTR("]}"));
	json.end();
}

/** Risponde a /probe-status con stato, avanzamento e fase della scansione. */
inline void obdHandleProbeStatus(ESP8266WebServer &server)
{
	ObdJsonStream json;
	json.begin(server);
	obdJsonProbeStatus(json);
	json.end();
}

#endif
