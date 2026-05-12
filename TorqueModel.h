#pragma once
#include <math.h>
#include "/Users/alesimattia/Documents/OBD2-car_dashboard-OLED/EngineConstants.h"  //unluckily no relative path in Arduino IDE so defined in main project file

/*
  TorqueModel.h

  Stima della coppia motore per Audi 2.7 TDI 140 kW usando PID OBD standard.
  Pensato per ESP8266 / Arduino.

  INPUT PRINCIPALI:
    load_pct          -> PID 0x04   [%]
    rpm               -> PID 0x0C   [rpm]
    maf_gps           -> PID 0x10   [g/s]
    map_kpa           -> PID 0x0B   [kPa assoluti]
    iat_c             -> PID 0x0F   [°C]

  INPUT OPZIONALI:
    fuelRail_kpa      -> PID 0x23   [kPa gauge]  (metti NAN se non disponibile)
    moduleVoltage_v   -> PID 0x42   [V]          (metti NAN se non disponibile)
    baro_kpa          -> PID 0x33   [kPa]        (metti NAN se non disponibile)

  NOTE IMPORTANTI:
  - Questa NON è una misura reale ECU della coppia.
  - È una stima fisicamente ragionata, il più accurata possibile con i PID standard.
  - Per la massima stabilità conviene filtrare i PID; qui il filtro EMA è integrato
    e può essere attivato/disattivato tramite useFilter.
*/

namespace Audi27TDI140kW::Torque
{
    struct FilterState
    {
        float load;
        float maf;
        float map;
        bool initialized;
    };

    // Stato statico persistente tra chiamate
    static FilterState filterState = {0.0f, 0.0f, 0.0f, false};

    // =========================
    // Utility
    // =========================
    static inline float clampf(float x, float lo, float hi)
    {
        if (x < lo)
            return lo;
        if (x > hi)
            return hi;
        return x;
    }

    static inline bool isFiniteNumber(float x)
    {
        return isfinite(x);
    }

    static inline float safeOrDefault(float x, float fallback)
    {
        return isFiniteNumber(x) ? x : fallback;
    }

    static inline float ema(float prev, float input, float alpha)
    {
        return prev + alpha * (input - prev);
    }

    // =========================
    // Curva coppia nominale massima
    // =========================
    static inline float nominalMaxTorqueNm(float rpm)
    {
        if (rpm < IDLE_MIN_RPM)
            return 0.0f;

        // Sotto il plateau: salita progressiva.
        // Non lineare pura, per evitare sovrastima ai bassissimi regimi.
        if (rpm < TORQUE_PLATEAU_START_RPM)
        {
            float x = clampf((rpm - IDLE_MIN_RPM) / (TORQUE_PLATEAU_START_RPM - IDLE_MIN_RPM), 0.0f, 1.0f);

            // Curva smoothstep: più realistica di una retta ai bassi giri
            // y = 3x^2 - 2x^3
            float y = (3.0f * x * x) - (2.0f * x * x * x);

            return TORQUE_MAX_NM * y;
        }

        // Plateau pieno
        if (rpm <= TORQUE_PLATEAU_END_RPM)
        {
            return TORQUE_MAX_NM;
        }

        // Sopra 3250 rpm: limitato dalla potenza massima
        float tFromPower = (9550.0f * POWER_MAX_KW) / rpm;

        // Non superare mai il massimo nominale
        return (tFromPower < TORQUE_MAX_NM) ? tFromPower : TORQUE_MAX_NM;
    }

    // =========================
    // Potenza nominale al regime attuale
    // =========================
    static inline float nominalPowerKwAtRpm(float rpm)
    {
        float t = nominalMaxTorqueNm(rpm);
        if (rpm <= 0.0f)
            return 0.0f;
        return (t * rpm) / 9550.0f;
    }

    // =========================
    // MAF nominale a pieno carico
    // =========================
    static inline float nominalFullLoadMafGps(float rpm)
    {
        float pKw = nominalPowerKwAtRpm(rpm);
        float maf = K_AIR_GPS_PER_KW * pKw;
        return (maf < 1.0f) ? 1.0f : maf;
    }

    // =========================
    // Fresh-air VE da MAF + MAP + IAT
    // =========================
    static inline float freshAirVolumetricEfficiency(float rpm, float maf_gps, float map_kpa, float iat_c)
    {
        if (rpm < IDLE_MIN_RPM)
            return 0.0f;
        if (map_kpa < MIN_VALID_MAP_KPA)
            return 0.0f;
        if (maf_gps < 0.0f)
            return 0.0f;

        float T_kelvin = iat_c + 273.15f;
        if (T_kelvin < 200.0f)
            T_kelvin = 200.0f;

        float map_pa = map_kpa * 1000.0f;
        float maf_kgs = maf_gps / 1000.0f;

        // 4T: una aspirazione ogni 2 giri => fattore 120
        float ve =
            (maf_kgs * 120.0f * GAS_CONSTANT_AIR * T_kelvin) /
            (rpm * map_pa * ENGINE_DISPLACEMENT_M3);

        // Non bloccare troppo: utile come diagnostica interna
        return clampf(ve, 0.0f, 2.0f);
    }

    // =========================
    // Correttore aria (MAF)
    // =========================
    static inline float mafCorrection(float rpm, float maf_gps)
    {
        float mafNom = nominalFullLoadMafGps(rpm);
        float ratio = maf_gps / mafNom;

        // Clamp stretto per evitare che il MAF domini tutto
        return clampf(ratio, 0.70f, 1.15f);
    }

    // =========================
    // Correttore VE (MAP + IAT + MAF)
    // =========================
    static inline float veCorrection(float rpm, float maf_gps, float map_kpa, float iat_c)
    {
        float ve = freshAirVolumetricEfficiency(rpm, maf_gps, map_kpa, iat_c);

        // In un turbo diesel a carico elevato VE apparente ~0.85..1.05 è plausibile
        // Correttivo molto moderato per non introdurre rumore
        float c = 0.85f + 0.15f * (ve / 0.90f);

        return clampf(c, 0.85f, 1.05f);
    }

    // =========================
    // Correttore rail pressure
    // =========================
    static inline float railPressureCorrection(float fuelRail_kpa, float rpm, float load_pct)
    {
        if (!isFiniteNumber(fuelRail_kpa))
            return 1.0f;

        // Correttivo volutamente debole.
        // Su diesel il rail pressure da solo non dice la massa iniettata,
        // ma se è molto basso rispetto al carico richiesto può indicare limitazione.
        float load = clampf(load_pct / 100.0f, 0.0f, 1.0f);

        // Attese grossolane ma utili:
        // basso carico -> rail anche modesto va bene
        // alto carico -> ci si aspetta rail ben più alto
        float expectedMin;
        if (load < 0.25f)
            expectedMin = 25000.0f;
        else if (load < 0.50f)
            expectedMin = 45000.0f;
        else if (load < 0.75f)
            expectedMin = 70000.0f;
        else
            expectedMin = 95000.0f;

        // Ai bassi giri concedi un po' più tolleranza
        if (rpm < 1500.0f)
            expectedMin *= 0.90f;

        float ratio = fuelRail_kpa / expectedMin;

        // Solo limiter verso il basso
        return clampf(ratio, 0.88f, 1.00f);
    }

    // =========================
    // Correttore tensione modulo
    // =========================
    static inline float voltageCorrection(float moduleVoltage_v)
    {
        if (!isFiniteNumber(moduleVoltage_v))
            return 1.0f;

        // Correzione minuscola: serve solo per non sovrastimare in undervoltage
        // 12.0 V -> ~0.97 ; 14.0 V -> ~1.00
        float c = 0.97f + 0.015f * (moduleVoltage_v - 12.0f);
        return clampf(c, 0.95f, 1.01f);
    }

    // =========================
    // Correttore BARO opzionale
    // =========================
    static inline float baroCorrection(float map_kpa, float baro_kpa, float rpm)
    {
        if (!isFiniteNumber(baro_kpa))
            return 1.0f;

        if (baro_kpa < 70.0f || baro_kpa > 110.0f)
            return 1.0f;

        if (map_kpa < baro_kpa)
            return 0.97f;

        // rapporto boost assoluto / ambiente
        float pr = map_kpa / baro_kpa;

        // Correzione leggerissima: serve solo a rendere il modello un po' più coerente
        float c = 0.98f + 0.02f * clampf((pr - 1.0f) / 1.2f, 0.0f, 1.0f);

        // ai bassi giri meglio ancora più prudente
        if (rpm < 1300.0f)
            c = 0.99f + 0.01f * clampf((pr - 1.0f) / 1.2f, 0.0f, 1.0f);

        return clampf(c, 0.97f, 1.00f);
    }

    // =========================
    // Reset manuale filtro
    // =========================
    static inline void resetTorqueModelFilter()
    {
        filterState.load = 0.0f;
        filterState.maf = 0.0f;
        filterState.map = 0.0f;
        filterState.initialized = false;
    }

    // =========================
    // Funzione principale
    // =========================
    static inline float estimateEngineTorqueNm(
        float load_pct,              // PID 0x04
        float rpm,                   // PID 0x0C
        float maf_gps,               // PID 0x10
        float map_kpa,               // PID 0x0B
        float iat_c,                 // PID 0x0F
        float fuelRail_kpa = NAN,    // PID 0x23
        float moduleVoltage_v = NAN, // PID 0x42
        float baro_kpa = NAN,        // PID 0x33
        bool useFilter = true)
    {
        // Sanity check input
        if (!isFiniteNumber(load_pct) || !isFiniteNumber(rpm) || !isFiniteNumber(maf_gps) ||
            !isFiniteNumber(map_kpa) || !isFiniteNumber(iat_c))
        {
            return 0.0f;
        }

        load_pct = clampf(load_pct, 0.0f, 100.0f);
        rpm = clampf(rpm, 0.0f, MAX_VALID_RPM);
        maf_gps = clampf(maf_gps, MIN_VALID_MAF_GPS, MAX_VALID_MAF_GPS);
        map_kpa = clampf(map_kpa, 0.0f, MAX_VALID_MAP_KPA);
        iat_c = clampf(iat_c, -40.0f, 120.0f);

        // =========================
        // Filtro EMA automatico opzionale
        // =========================
        if (useFilter)
        {
            // Se motore spento o quasi spento, reset filtro e ritorno 0
            if (rpm < IDLE_MIN_RPM)
            {
                resetTorqueModelFilter();
                return 0.0f;
            }

            // Prima inizializzazione filtro
            if (!filterState.initialized)
            {
                filterState.load = load_pct;
                filterState.maf = maf_gps;
                filterState.map = map_kpa;
                filterState.initialized = true;
            }
            else
            {
                filterState.load = ema(filterState.load, load_pct, TORQUE_EMA_ALPHA_LOAD);
                filterState.maf = ema(filterState.maf, maf_gps, TORQUE_EMA_ALPHA_MAF);
                filterState.map = ema(filterState.map, map_kpa, TORQUE_EMA_ALPHA_MAP);
            }

            // Sovrascrive input con valori filtrati
            load_pct = filterState.load;
            maf_gps = filterState.maf;
            map_kpa = filterState.map;
        }
        else
        {
            // Se non si usa il filtro, evita che rimanga stato sporco
            resetTorqueModelFilter();
        }

        // Protezioni base
        if (rpm < IDLE_MIN_RPM || map_kpa < MIN_VALID_MAP_KPA)
        {
            return 0.0f;
        }

        // 1) Coppia massima nominale disponibile al regime attuale
        float T_nom = nominalMaxTorqueNm(rpm);

        // 2) Fattore base da calculated load
        float loadFactor = clampf(load_pct / 100.0f, 0.0f, 1.05f);

        // 3) Correzione aria reale da MAF
        float cMaf = mafCorrection(rpm, maf_gps);

        // 4) Correzione fisica da VE apparente
        float cVe = veCorrection(rpm, maf_gps, map_kpa, iat_c);

        // 5) Correzioni deboli opzionali
        float cRail = railPressureCorrection(fuelRail_kpa, rpm, load_pct);
        float cVolt = voltageCorrection(moduleVoltage_v);
        float cBaro = baroCorrection(map_kpa, baro_kpa, rpm);

        // 6) Fattore finale
        float effectiveLoad = loadFactor * cMaf * cVe * cRail * cVolt * cBaro;

        // Limita l'overshoot
        effectiveLoad = clampf(effectiveLoad, 0.0f, 1.05f);

        // 7) Coppia finale
        float torqueNm = T_nom * effectiveLoad;

        // Clamp finale
        torqueNm = clampf(torqueNm, 0.0f, 420.0f);

        return torqueNm;
    }

    // =========================
    // Variante leggera: solo PID fondamentali
    // =========================
    static inline float estimateEngineTorqueNmBasic(
        float load_pct, // PID 0x04
        float rpm,      // PID 0x0C
        float maf_gps,  // PID 0x10
        float map_kpa,  // PID 0x0B
        float iat_c,    // PID 0x0F
        bool useFilter = true)
    {
        return estimateEngineTorqueNm(load_pct, rpm, maf_gps, map_kpa, iat_c, NAN, NAN, NAN, useFilter);
    }
}