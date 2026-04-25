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
 * Uso:
 *   #include "light_sensor.h"        // dichiara `BrState br;` inline
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
#define LDR_SAMPLE_MS    250    // 4 Hz: piu' veloce e' inutile, piu' lento rende laggy il fade
#define LDR_SCENE_DELTA  150    // |ldr - snapshot| oltre questa soglia -> esce da MANUAL
#define BR_HYST_DELTA    4      // anti-flicker: setContrast solo se delta target >= questo
#define BR_FADE_STEP     2      // unita' di contrasto per tick di fade
#define BR_FADE_TICK_MS  30     // 30 ms * 2 step -> ~3.8 s per range pieno
#define BR_CONTRAST_MIN  5      // floor: notte estrema, evita di spegnere il pannello
#define BR_CONTRAST_MAX  240    // ceiling: lascia margine sotto 255 per non saturare

enum BrightnessMode : uint8_t { BR_AUTO, BR_MANUAL, BR_FADING };

struct BrState {
  BrightnessMode mode = BR_AUTO;
  float    ldrEma          = 500.0f;  // valore filtrato corrente (EMA)
  int      ldrAtOverride   = 0;       // snapshot LDR al momento dell'override manuale
  uint8_t  currentContrast = 128;     // valore attualmente applicato all'OLED
  uint8_t  targetContrast  = 128;     // destinazione del fade
  uint8_t  manualContrast  = 128;     // valore impostato dallo slider
  unsigned long lastSampleMs = 0;
  unsigned long lastFadeMs   = 0;
};

inline BrState br;

// ADC 0..1023 -> contrasto BR_CONTRAST_MIN..BR_CONTRAST_MAX con curva gamma 2.0.
// Gamma 2 enfatizza le differenze nella zona buia, dove l'occhio e' piu' sensibile.
inline uint8_t adcToContrast(int adc) {
  if (adc < 30) adc = 30;
  if (adc > 950) adc = 950;
  float n = (adc - 30) / 920.0f;       // 0..1
  float g = n * n;                     // gamma 2.0
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
  int raw = analogRead(LDR_PIN);
  br.ldrEma = (float)raw;
  br.currentContrast = adcToContrast(raw);
  br.targetContrast  = br.currentContrast;
  u8g2.setContrast(br.currentContrast);
}

#endif  // LIGHT_SENSOR_H
