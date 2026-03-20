/**
 * Tabella descrizioni codici errore DTC (Diagnostic Trouble Codes)
 *
 * Contiene i ~45 codici generici OBD2 piu' comuni, con focus su
 * motori diesel turbo. Descrizioni in italiano, memorizzate in PROGMEM.
 * Per codici non presenti nella tabella viene mostrato solo il codice.
 *
 * Usato da: obd2_monitor.ino, obd2_wifi_monitor.ino
 *
 * @since 2026-03-19 mattia.Alesi
 */

#ifndef DTC_DESCRIPTIONS_H
#define DTC_DESCRIPTIONS_H

#include <Arduino.h>

/** Struttura per una entry della tabella DTC */
typedef struct {
  uint16_t code;          // Codice DTC raw (2 byte come da OBD2)
  const char* description; // Descrizione breve in italiano
} DtcEntry;

// Macro per codice DTC: converte "P0087" → valore uint16_t raw
// Categoria: P=0x0, C=0x4, B=0x8, U=0xC (shifted nei bit 14-15)
// Le cifre 2-5 del codice vanno nei bit rimanenti
#define DTC_CODE(c2, c3, c4, c5) \
  ((uint16_t)((c2) << 12) | ((c3) << 8) | ((c4) << 4) | (c5))

// Descrizioni in PROGMEM
static const char dtc_P0087[] PROGMEM = "Press. rail bassa";
static const char dtc_P0088[] PROGMEM = "Press. rail alta";
static const char dtc_P0093[] PROGMEM = "Perdita sist. fuel";
static const char dtc_P0100[] PROGMEM = "Sensore MAF guasto";
static const char dtc_P0101[] PROGMEM = "MAF fuori range";
static const char dtc_P0102[] PROGMEM = "MAF segnale basso";
static const char dtc_P0103[] PROGMEM = "MAF segnale alto";
static const char dtc_P0110[] PROGMEM = "Sens. temp. aria";
static const char dtc_P0112[] PROGMEM = "Temp. aria bassa";
static const char dtc_P0113[] PROGMEM = "Temp. aria alta";
static const char dtc_P0115[] PROGMEM = "Sens. temp. liquido";
static const char dtc_P0117[] PROGMEM = "Temp. liq. bassa";
static const char dtc_P0118[] PROGMEM = "Temp. liq. alta";
static const char dtc_P0190[] PROGMEM = "Sens. press. rail";
static const char dtc_P0191[] PROGMEM = "Press. rail range";
static const char dtc_P0192[] PROGMEM = "Press. rail bassa";
static const char dtc_P0193[] PROGMEM = "Press. rail alta";
static const char dtc_P0234[] PROGMEM = "Sovrapressione turbo";
static const char dtc_P0299[] PROGMEM = "Sottopressione turbo";
static const char dtc_P0300[] PROGMEM = "Mancata accensione";
static const char dtc_P0301[] PROGMEM = "Mancata acc. cil.1";
static const char dtc_P0302[] PROGMEM = "Mancata acc. cil.2";
static const char dtc_P0303[] PROGMEM = "Mancata acc. cil.3";
static const char dtc_P0304[] PROGMEM = "Mancata acc. cil.4";
static const char dtc_P0380[] PROGMEM = "Candelette guasto";
static const char dtc_P0401[] PROGMEM = "Flusso EGR scarso";
static const char dtc_P0402[] PROGMEM = "Flusso EGR eccessivo";
static const char dtc_P0404[] PROGMEM = "EGR fuori range";
static const char dtc_P0405[] PROGMEM = "EGR segnale basso";
static const char dtc_P0406[] PROGMEM = "EGR segnale alto";
static const char dtc_P0471[] PROGMEM = "Press. scarico range";
static const char dtc_P0473[] PROGMEM = "Press. scarico alta";
static const char dtc_P0480[] PROGMEM = "Ventola guasta";
static const char dtc_P0489[] PROGMEM = "EGR circuito basso";
static const char dtc_P0490[] PROGMEM = "EGR circuito alto";
static const char dtc_P0520[] PROGMEM = "Sens. press. olio";
static const char dtc_P0524[] PROGMEM = "Press. olio bassa";
static const char dtc_P0546[] PROGMEM = "Temp. scarico bassa";
static const char dtc_P0547[] PROGMEM = "Temp. scarico alta";
static const char dtc_P2002[] PROGMEM = "Efficienza DPF bassa";
static const char dtc_P2263[] PROGMEM = "Turbo sovra/sotto";
static const char dtc_P2463[] PROGMEM = "Fuliggine DPF alta";
static const char dtc_P0560[] PROGMEM = "Tensione sistema";
static const char dtc_P0563[] PROGMEM = "Tensione alta";
static const char dtc_P0562[] PROGMEM = "Tensione bassa";

/** Tabella DTC in PROGMEM — ordinata per codice per ricerca binaria */
static const DtcEntry dtcTable[] PROGMEM = {
  { DTC_CODE(0,0,8,7), dtc_P0087 },
  { DTC_CODE(0,0,8,8), dtc_P0088 },
  { DTC_CODE(0,0,9,3), dtc_P0093 },
  { DTC_CODE(0,1,0,0), dtc_P0100 },
  { DTC_CODE(0,1,0,1), dtc_P0101 },
  { DTC_CODE(0,1,0,2), dtc_P0102 },
  { DTC_CODE(0,1,0,3), dtc_P0103 },
  { DTC_CODE(0,1,1,0), dtc_P0110 },
  { DTC_CODE(0,1,1,2), dtc_P0112 },
  { DTC_CODE(0,1,1,3), dtc_P0113 },
  { DTC_CODE(0,1,1,5), dtc_P0115 },
  { DTC_CODE(0,1,1,7), dtc_P0117 },
  { DTC_CODE(0,1,1,8), dtc_P0118 },
  { DTC_CODE(0,1,9,0), dtc_P0190 },
  { DTC_CODE(0,1,9,1), dtc_P0191 },
  { DTC_CODE(0,1,9,2), dtc_P0192 },
  { DTC_CODE(0,1,9,3), dtc_P0193 },
  { DTC_CODE(0,2,3,4), dtc_P0234 },
  { DTC_CODE(0,2,9,9), dtc_P0299 },
  { DTC_CODE(0,3,0,0), dtc_P0300 },
  { DTC_CODE(0,3,0,1), dtc_P0301 },
  { DTC_CODE(0,3,0,2), dtc_P0302 },
  { DTC_CODE(0,3,0,3), dtc_P0303 },
  { DTC_CODE(0,3,0,4), dtc_P0304 },
  { DTC_CODE(0,3,8,0), dtc_P0380 },
  { DTC_CODE(0,4,0,1), dtc_P0401 },
  { DTC_CODE(0,4,0,2), dtc_P0402 },
  { DTC_CODE(0,4,0,4), dtc_P0404 },
  { DTC_CODE(0,4,0,5), dtc_P0405 },
  { DTC_CODE(0,4,0,6), dtc_P0406 },
  { DTC_CODE(0,4,7,1), dtc_P0471 },
  { DTC_CODE(0,4,7,3), dtc_P0473 },
  { DTC_CODE(0,4,8,0), dtc_P0480 },
  { DTC_CODE(0,4,8,9), dtc_P0489 },
  { DTC_CODE(0,4,9,0), dtc_P0490 },
  { DTC_CODE(0,5,2,0), dtc_P0520 },
  { DTC_CODE(0,5,2,4), dtc_P0524 },
  { DTC_CODE(0,5,4,6), dtc_P0546 },
  { DTC_CODE(0,5,4,7), dtc_P0547 },
  { DTC_CODE(0,5,6,0), dtc_P0560 },
  { DTC_CODE(0,5,6,2), dtc_P0562 },
  { DTC_CODE(0,5,6,3), dtc_P0563 },
  { DTC_CODE(2,0,0,2), dtc_P2002 },
  { DTC_CODE(2,2,6,3), dtc_P2263 },
  { DTC_CODE(2,4,6,3), dtc_P2463 },
};

static const uint8_t DTC_TABLE_SIZE = sizeof(dtcTable) / sizeof(dtcTable[0]);

/**
 * Decodifica un DTC raw (2 byte) in stringa leggibile "P0301".
 * output deve avere almeno 6 byte.
 */
void decodeDTC(uint16_t code, char* output) {
  const char categories[] = "PCBU";
  uint8_t cat = (code >> 14) & 0x03;
  uint8_t d2  = (code >> 12) & 0x03;
  uint8_t d3  = (code >> 8)  & 0x0F;
  uint8_t d4  = (code >> 4)  & 0x0F;
  uint8_t d5  = code         & 0x0F;
  snprintf(output, 6, "%c%d%X%X%X", categories[cat], d2, d3, d4, d5);
}

/**
 * Cerca la descrizione di un DTC nella tabella PROGMEM.
 * Usa ricerca binaria (tabella ordinata per codice).
 * Restituisce la descrizione o NULL se non trovato.
 */
const char* getDTCDescription(uint16_t code) {
  int low = 0;
  int high = DTC_TABLE_SIZE - 1;

  while (low <= high) {
    int mid = (low + high) / 2;
    uint16_t midCode = pgm_read_word(&dtcTable[mid].code);

    if (midCode == code) {
      return (const char*)pgm_read_ptr(&dtcTable[mid].description);
    } else if (midCode < code) {
      low = mid + 1;
    } else {
      high = mid - 1;
    }
  }
  return NULL;
}

#endif
