/**
 * Auto-brightness OLED tramite fotoresistore (LDR) su A0.
 *
 * Hardware: 3.3V -> LDR -> A0 -> 10k -> GND. Piu' luce -> ADC piu' alto.
 *
 * State machine:
 *   AUTO       contrasto = mapping(ldr filtrato)
 *   MANUAL     l'utente ha mosso lo slider; resta sticky finche' la luce
 *              non varia di LDR_SCENE_DELTA dal valore di snapshot.
 *   FADING     transizione graduale verso il nuovo target (rientro auto
 *              o reset esplicito via /brightness?auto).
 *
 * Auto-calibrazione live: i bounds ADC usati da adcToContrast()
 * (ldrMinObserved/ldrMaxObserved) si adattano runtime alla luce realmente
 * osservata. Vivono solo in RAM (no flash, no EEPROM wear); ad ogni reboot
 * si riparte da un fallback statico finche' la calibrazione non si stabilizza.
 *
 * Uso:
 *   #include "light_sensor.h"        // dichiara `BrightnessState br;` inline
 *   initBrightness();                // in setup() dopo u8g2.begin()
 *   updateBrightness();              // a ogni iter del loop()
 */

#ifndef LIGHT_SENSOR_H
	#define LIGHT_SENSOR_H

	#include <Arduino.h>
	#include <U8g2lib.h>

	// Definito negli .ino (CANbus_conn / WIFI_conn). Stesso tipo, stesso nome.
	extern U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2;

	#define LDR_PIN          A0

	/** Il display reagisce a una variazione netta di luce con tempo di settling 
	 * al 95% ≈ 18 / LDR_SAMPLE_HZ secondi (EMA alpha=0.15)   Es. 10 Hz -> ~1.8 s; 4 Hz -> ~4.5 s.
	 */
	#define LDR_SAMPLE_HZ    10
	#define LDR_SAMPLE_MS    (1000 / LDR_SAMPLE_HZ)  // derivato: usato dal codice esistente

	#define LDR_SCENE_DELTA  150    // |ldr - snapshot| oltre questa soglia -> esce da MANUAL
	#define BR_HYST_DELTA    3      // anti-flicker: setContrast solo se delta target >= questo
	#define BR_FADE_STEP     4      // unita' di contrasto per tick di fade
	#define BR_FADE_TICK_MS  30     // 30 ms * 4 step -> ~1.9 s per range pieno
	#define BR_CONTRAST_MIN  5      // floor: notte estrema, evita di spegnere il pannello
	#define BR_CONTRAST_MAX  240    // ceiling: lascia margine sotto 255 per non saturare

	// Auto-calibrazione live del range LDR (no persistence, ricalibra ad ogni boot)
	#define LDR_CALIBRATION_FALLBACK_MIN  30      // bound minimo di default (sessione appena iniziata)
	#define LDR_CALIBRATION_FALLBACK_MAX  950     // bound massimo di default
	#define LDR_CALIBRATION_MIN_RANGE     100     // sotto questo span -> usa fallback
	#define LDR_CALIBRATION_WARMUP_MS     2000    // 2s post-boot senza calibrare (stabilizza EMA)
	#define LDR_CALIBRATION_DECAY_MS      3000    // 3s: tick del decay anti-outlier
	#define LDR_CALIBRATION_DECAY_STEP    2       // 2 unita' per tick -> ~40 unita'/min di decay
	#define LDR_CALIBRATION_DECAY_MARGIN  20      // bound si riavvicina all'EMA solo se distante > 20

	enum BrightnessMode : uint8_t { BR_AUTO, BR_MANUAL, BR_FADING };

	struct BrightnessState {
		BrightnessMode mode = BR_AUTO;
		float    ldrEma          = 500.0f;  // valore filtrato corrente (EMA)
		int      ldrAtOverride   = 0;       // snapshot LDR al momento dell'override manuale
		uint8_t  currentContrast = 128;     // valore attualmente applicato all'OLED
		uint8_t  targetContrast  = 128;     // destinazione del fade
		uint8_t  manualContrast  = 128;     // valore impostato dallo slider
		unsigned long lastSampleMs = 0;
		unsigned long lastFadeMs   = 0;

		// Auto-calibrazione live: bounds del valore EMA osservati questa sessione.
		// Inizializzati a un range "vuoto" (min=1023, max=0) cosi' qualsiasi
		// primo sample valido li aggiorna via espansione.
		int ldrMinObserved          = 1023;
		int ldrMaxObserved          = 0;
		unsigned long calibrationStartMs     = 0;  // timestamp init (per warm-up)
		unsigned long lastCalibrationDecayMs = 0;  // ultimo tick di decay
	};

	inline BrightnessState br;

	// ADC 0..1023 -> contrasto BR_CONTRAST_MIN..BR_CONTRAST_MAX con curva gamma 2.0.
	// Gamma 2 enfatizza le differenze nella zona buia, dove l'occhio è piu' sensibile.
	// Usa i bounds calibrati live se il range osservato e' significativo, altrimenti
	// fallback a LDR_CALIBRATION_FALLBACK_MIN..MAX (sessione appena iniziata).
	inline uint8_t adcToContrast(int adc) {
		int loBound = br.ldrMinObserved;
		int hiBound = br.ldrMaxObserved;
		if (hiBound - loBound < LDR_CALIBRATION_MIN_RANGE) {
			loBound = LDR_CALIBRATION_FALLBACK_MIN;
			hiBound = LDR_CALIBRATION_FALLBACK_MAX;
		}
		if (adc < loBound) adc = loBound;
		if (adc > hiBound) adc = hiBound;
		float n = (float)(adc - loBound) / (float)(hiBound - loBound);  // 0..1
		float g = n * n;                                                 // gamma 2.0
		return (uint8_t)(BR_CONTRAST_MIN + g * (BR_CONTRAST_MAX - BR_CONTRAST_MIN));
	}

	inline void _tickFade(unsigned long now) {
		if (now - br.lastFadeMs < BR_FADE_TICK_MS) return;
		br.lastFadeMs = now;
		int delta = (int)br.targetContrast - (int)br.currentContrast;
		if (abs(delta) <= 1) {
			br.currentContrast = br.targetContrast;
			br.mode = BR_AUTO;
		} else {
			br.currentContrast += (delta > 0 ? BR_FADE_STEP : -BR_FADE_STEP);
		}
		u8g2.setContrast(br.currentContrast);
	}

	/**
	 * Aggiorna i bounds osservati per la calibrazione live.
	 * Espande se l'EMA esce dal range osservato; applica decay verso l'EMA
	 * corrente per dimenticare outlier accidentali.
	 * @since 30/04/26 Mattia Alesi
	 */
	inline void _updateLdrCalibration(unsigned long now) {
		// Warm-up: lascia che l'EMA si stabilizzi prima di calibrare.
		if (now - br.calibrationStartMs < LDR_CALIBRATION_WARMUP_MS) return;

		int emaInt = (int)br.ldrEma;

		// Espansione: l'EMA filtrato (anti-spike gia' applicato) supera un bound -> aggiorna.
		if (emaInt < br.ldrMinObserved) br.ldrMinObserved = emaInt;
		if (emaInt > br.ldrMaxObserved) br.ldrMaxObserved = emaInt;

		// Decay: ogni LDR_CALIBRATION_DECAY_MS, riavvicina i bound all'EMA
		// di LDR_CALIBRATION_DECAY_STEP unita' se sono "lontani" piu' di LDR_CALIBRATION_DECAY_MARGIN.
		if (now - br.lastCalibrationDecayMs >= LDR_CALIBRATION_DECAY_MS) {
			br.lastCalibrationDecayMs = now;
			if (br.ldrMinObserved < emaInt - LDR_CALIBRATION_DECAY_MARGIN) {
				br.ldrMinObserved += LDR_CALIBRATION_DECAY_STEP;
			}
			if (br.ldrMaxObserved > emaInt + LDR_CALIBRATION_DECAY_MARGIN) {
				br.ldrMaxObserved -= LDR_CALIBRATION_DECAY_STEP;
			}
		}
	}

	inline void updateBrightness() {
		unsigned long now = millis();

		// Il fade gira ad alta frequenza, indipendente dal sample LDR.
		if (br.mode == BR_FADING) _tickFade(now);

		if (now - br.lastSampleMs < LDR_SAMPLE_MS) return;
		br.lastSampleMs = now;

		int raw = analogRead(LDR_PIN);
		// Clamp anti-spike: scarta sample isolati troppo lontani dall'EMA (es. EMI da SPI MCP2515).
		if (abs(raw - (int)br.ldrEma) < 300) {
			br.ldrEma = br.ldrEma * 0.85f + raw * 0.15f;  // EMA, alpha=0.15
		}

		// Aggiorna i bounds della calibrazione live in base all'EMA appena ricalcolato.
		_updateLdrCalibration(now);

		switch (br.mode) {
			case BR_AUTO: {
				uint8_t t = adcToContrast((int)br.ldrEma);
				if (abs((int)t - (int)br.currentContrast) >= BR_HYST_DELTA) {
					br.currentContrast = t;
					u8g2.setContrast(t);
				}
				break;
			}
			case BR_MANUAL:
				// La scena cambia abbastanza? -> esci dallo sticky.
				if (abs((int)br.ldrEma - br.ldrAtOverride) > LDR_SCENE_DELTA) {
					br.targetContrast = adcToContrast((int)br.ldrEma);
					br.mode = BR_FADING;
				}
				break;
			case BR_FADING:
				// Aggiorna sempre il target: la luce continua a cambiare durante il fade.
				br.targetContrast = adcToContrast((int)br.ldrEma);
				break;
		}
	}

	inline void initBrightness() {
		br.calibrationStartMs     = millis();
		br.lastCalibrationDecayMs = br.calibrationStartMs;
		int raw = analogRead(LDR_PIN);
		br.ldrEma = (float)raw;
		br.currentContrast = adcToContrast(raw);  // usera' fallback (range osservato ancora vuoto)
		br.targetContrast  = br.currentContrast;
		u8g2.setContrast(br.currentContrast);
	}

#endif  // LIGHT_SENSOR_H
