/**
 * Pulsante fisico — debounce + state machine, no delay() / no ISR.
 *
 * Uso:
 *   #define BUTTON_PIN <pin>   // opzionale, default D3 (GPIO0)
 *   #include "button_handler.h"
 *   ...
 *   buttonInit();              // in setup()
 *   ButtonEvent ev = buttonPoll();   // a ogni iter del loop
 *
 * Pulsante NA tra BUTTON_PIN e GND, INPUT_PULLUP (attivo basso).
 * Eventi: BTN_NONE, BTN_SHORT (rilascio < BUTTON_LONGPRESS_MS),
 * BTN_LONG (premuto >= BUTTON_LONGPRESS_MS, fired one-shot mentre ancora premuto).
 */

#ifndef BUTTON_HANDLER_H
	#define BUTTON_HANDLER_H

	#include <Arduino.h>

	#ifndef BUTTON_PIN
		#define BUTTON_PIN D3
	#endif

	#define BUTTON_DEBOUNCE_MS 50
	#define BUTTON_LONGPRESS_MS 2500

	enum ButtonEvent
	{
		BTN_NONE,
		BTN_SHORT,
		BTN_LONG
	};

	inline void buttonInit()
	{
		pinMode(BUTTON_PIN, INPUT_PULLUP);
	}

	inline ButtonEvent buttonPoll()
	{
		// Stato interno persistente
		static uint8_t lastReading = HIGH;	   // ultimo valore grezzo
		static uint8_t stableLevel = HIGH;	   // livello dopo debounce
		static unsigned long lastChangeMs = 0; // ts ultimo cambio grezzo
		static unsigned long pressStartMs = 0; // ts inizio press (livello stabile LOW)
		static bool longFired = false;		   // BTN_LONG gia' emesso in questa pressione

		unsigned long now = millis();
		uint8_t reading = digitalRead(BUTTON_PIN);

		if (reading != lastReading)
		{
			lastChangeMs = now;
			lastReading = reading;
		}

		if ((now - lastChangeMs) >= BUTTON_DEBOUNCE_MS && reading != stableLevel)
		{
			stableLevel = reading;
			if (stableLevel == LOW)
			{
				// Inizio pressione confermata
				pressStartMs = now;
				longFired = false;
			}
			else
			{
				// Rilascio confermato: se non era un long, è uno short
				if (!longFired)
				{
					return BTN_SHORT;
				}
			}
		}

		// Mentre il pulsante è premuto, controlla soglia long-press una sola volta
		if (stableLevel == LOW && !longFired && (now - pressStartMs) >= BUTTON_LONGPRESS_MS)
		{
			longFired = true;
			return BTN_LONG;
		}

		return BTN_NONE;
	}
#endif // BUTTON_HANDLER_H