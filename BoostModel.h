#pragma once
#include <math.h>

/*
  TurboPressureEstimatorAudi27TDI.h

  Stima della pressione turbo per Audi 2.7 TDI usando PID OBD standard.

  --------------------------------------------------
  INPUT:
    map_kpa   -> PID 0x0B   [kPa assoluti]
    baro_kpa  -> PID 0x33   [kPa assoluti]
    maf_gps   -> PID 0x10   [g/s]
    rpm       -> PID 0x0C   [rpm]
    iat_c     -> PID 0x0F   [°C]
    load_pct  -> PID 0x04   [%]

  --------------------------------------------------
  OUTPUT:
    pressione turbo RELATIVA in bar

  CONVENZIONE:
    0 bar   = pressione ambiente
    > 0 bar = sovralimentazione
    < 0 bar = depressione

  --------------------------------------------------
  MODELLO:
  1) Base:
       boost = MAP - BARO

  2) Correzione:
       aggiunge perdita di carico tra compressore e collettore
       stimata tramite MAF + VE + condizioni operative

  --------------------------------------------------
  NOTE:
  - NON è una misura diretta del compressore
  - È la miglior stima ottenibile con PID standard
  - Mantiene valori negativi (NO clamp a 0)
*/

namespace Audi27TDI140kW
{
    // =========================
    // Parametri fisici
    // =========================

    static constexpr float ENGINE_DISPLACEMENT_M3 = 0.002698f;
    static constexpr float GAS_CONSTANT_AIR       = 287.05f;

    static constexpr float IDLE_MIN_RPM = 550.0f;

    // Range plausibili
    static constexpr float MIN_VALID_MAF_GPS  = 0.0f;
    static constexpr float MAX_VALID_MAP_KPA  = 350.0f;
    static constexpr float MAX_VALID_RPM      = 5500.0f;
    static constexpr float MAX_VALID_MAF_GPS  = 400.0f;

    // =========================
    // Parametri filtro EMA
    // =========================

    static constexpr float EMA_ALPHA_MAP  = 0.22f;
    static constexpr float EMA_ALPHA_BARO = 0.10f;
    static constexpr float EMA_ALPHA_MAF  = 0.20f;
    static constexpr float EMA_ALPHA_LOAD = 0.12f;

    struct TurboFilterState
    {
        float map;
        float baro;
        float maf;
        float load;
        bool initialized;
    };

    static TurboFilterState turboFilterState = {0,0,0,0,false};

    // =========================
    // Utility
    // =========================

    static inline float clampf(float x, float lo, float hi)
    {
        if (x < lo) return lo;
        if (x > hi) return hi;
        return x;
    }

    static inline bool isFiniteNumber(float x)
    {
        return isfinite(x);
    }

    static inline float ema(float prev, float input, float alpha)
    {
        return prev + alpha * (input - prev);
    }

    // =========================
    // VE apparente
    // =========================
    static inline float freshAirVE(float rpm, float maf_gps, float map_kpa, float iat_c)
    {
        if (rpm < IDLE_MIN_RPM || maf_gps <= 0.0f)
            return 0.0f;

        float T = iat_c + 273.15f;
        if (T < 200.0f) T = 200.0f;

        float map_pa  = map_kpa * 1000.0f;
        float maf_kgs = maf_gps / 1000.0f;

        float ve =
            (maf_kgs * 120.0f * GAS_CONSTANT_AIR * T) /
            (rpm * map_pa * ENGINE_DISPLACEMENT_M3);

        return clampf(ve, 0.0f, 2.0f);
    }

    // =========================
    // Perdita di carico
    // =========================
    static inline float estimateChargePathPressureDropKpa(
        float maf_gps,
        float rpm,
        float iat_c,
        float map_kpa,
        float load_pct
    )
    {
        if (rpm < 900.0f || maf_gps < 8.0f)
            return 0.0f;

        float load = clampf(load_pct / 100.0f, 0.0f, 1.0f);

        // Modello base
        float dp =
            (0.00075f * maf_gps * maf_gps) +
            (0.0120f  * maf_gps);

        // Correzione VE
        float ve = freshAirVE(rpm, maf_gps, map_kpa, iat_c);

        float c_ve;
        if (ve < 0.45f)      c_ve = 0.60f;
        else if (ve < 0.65f) c_ve = 0.80f;
        else if (ve < 1.10f) c_ve = 1.00f;
        else                 c_ve = 1.05f;

        // Correzione carico
        float c_load = 0.35f + (0.65f * load);

        // Correzione regime
        float c_rpm = 0.92f + 0.10f * clampf((rpm - 1200.0f) / 2500.0f, 0.0f, 1.0f);

        dp *= c_ve * c_load * c_rpm;

        return clampf(dp, 0.0f, 18.0f);
    }

    // =========================
    // Funzione principale (kPa)
    // =========================
    /*
      Ritorna pressione turbo relativa [kPa].

      PARAMETRI:
        map_kpa   : PID 0x0B (assoluta)
        baro_kpa  : PID 0x33 (assoluta)
        maf_gps   : PID 0x10
        rpm       : PID 0x0C
        iat_c     : PID 0x0F
        load_pct  : PID 0x04

        includeChargePathCorrection:
          false -> ritorna il valore più affidabile e diretto:
                   MAP - BARO
          true  -> aggiunge una stima della perdita tra turbo e collettore,
                   ottenendo una stima più vicina alla pressione lato turbo

        useFilter:
          true  -> filtra MAP, BARO, MAF, LOAD con EMA
          false -> usa i valori raw

      NOTE:
      - Nessun crop a zero: i valori negativi restano negativi.
    */
    static inline float estimateTurboPressureKpa(
        float map_kpa,
        float baro_kpa,
        float maf_gps,
        float rpm,
        float iat_c,
        float load_pct,
        bool includeChargePathCorrection = true,
        bool useFilter = true
    )
    {
        // Validazione base
        if (!isFiniteNumber(map_kpa)  ||
            !isFiniteNumber(baro_kpa) ||
            !isFiniteNumber(maf_gps)  ||
            !isFiniteNumber(rpm)      ||
            !isFiniteNumber(iat_c)    ||
            !isFiniteNumber(load_pct))
        {
            return NAN;
        }

        map_kpa  = clampf(map_kpa,  0.0f, MAX_VALID_MAP_KPA);
        baro_kpa = clampf(baro_kpa, 0.0f, 150.0f);
        maf_gps  = clampf(maf_gps,  MIN_VALID_MAF_GPS, MAX_VALID_MAF_GPS);
        rpm      = clampf(rpm,      0.0f, MAX_VALID_RPM);
        load_pct = clampf(load_pct, 0.0f, 100.0f);
        iat_c    = clampf(iat_c,   -40.0f, 120.0f);

        // Filtro opzionale
        if (useFilter)
        {
            if (!turboFilterState.initialized)
            {
                turboFilterState.map  = map_kpa;
                turboFilterState.baro = baro_kpa;
                turboFilterState.maf  = maf_gps;
                turboFilterState.load = load_pct;
                turboFilterState.initialized = true;
            }
            else
            {
                turboFilterState.map  = ema(turboFilterState.map,  map_kpa,  EMA_ALPHA_MAP);
                turboFilterState.baro = ema(turboFilterState.baro, baro_kpa, EMA_ALPHA_BARO);
                turboFilterState.maf  = ema(turboFilterState.maf,  maf_gps,  EMA_ALPHA_MAF);
                turboFilterState.load = ema(turboFilterState.load, load_pct, EMA_ALPHA_LOAD);
            }

            map_kpa  = turboFilterState.map;
            baro_kpa = turboFilterState.baro;
            maf_gps  = turboFilterState.maf;
            load_pct = turboFilterState.load;
        }

        // Base: pressione relativa nel collettore
        // Può essere negativa e NON la tagliamo.
        float boostGauge_kpa = map_kpa - baro_kpa;

        // Se non vogliamo il modello raffinato, ritorniamo direttamente il valore base.
        if (!includeChargePathCorrection)
            return boostGauge_kpa;

        // Correzione: stima della perdita di carico lato aria compressa
        float dpCharge_kpa = estimateChargePathPressureDropKpa(
            maf_gps,
            rpm,
            iat_c,
            map_kpa,
            load_pct
        );

        // Se la macchina è in vera depressione e a bassissimo carico,
        // ha poco senso aggiungere tutta la perdita di carico.
        // La riduciamo senza annullare il segno negativo.
        if (boostGauge_kpa < 0.0f)
        {
            float lowLoadFactor = clampf(load_pct / 25.0f, 0.0f, 1.0f);
            dpCharge_kpa *= lowLoadFactor;
        }

        // Stima finale più vicina alla pressione lato turbo/compressore
        return boostGauge_kpa + dpCharge_kpa;
    }

    // =========================
    // Helper opzionali
    // =========================

    // Pressione turbo relativa in bar
    static inline float estimateTurboPressureBar(
        float map_kpa,
        float baro_kpa,
        float maf_gps,
        float rpm,
        float iat_c,
        float load_pct,
        bool includeChargePathCorrection = true,
        bool useFilter = true
    )
    {
        return estimateTurboPressureKpa(
            map_kpa,
            baro_kpa,
            maf_gps,
            rpm,
            iat_c,
            load_pct,
            includeChargePathCorrection,
            useFilter
        ) / 100.0f;
    }

}
