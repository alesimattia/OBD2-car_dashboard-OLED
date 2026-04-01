#pragma once

namespace Audi27TDI140kW
{
    // =========================
    // Parametri fisici condivisi
    // =========================
    static constexpr float ENGINE_DISPLACEMENT_M3 = 0.002698f; // m^3, 2.698 L
    static constexpr float GAS_CONSTANT_AIR = 287.05f;         // J/(kg*K)

    // =========================
    // Parametri motore / modello coppia
    // =========================
    static constexpr float TORQUE_MAX_NM = 400.0f;
    static constexpr float POWER_MAX_KW = 140.0f;
    static constexpr float TORQUE_PLATEAU_START_RPM = 1400.0f;
    static constexpr float TORQUE_PLATEAU_END_RPM = 3250.0f;
    static constexpr float K_AIR_GPS_PER_KW = 1.22f;

    // =========================
    // Range sanity / plausibili
    // =========================
    static constexpr float IDLE_MIN_RPM = 550.0f;
    static constexpr float MAX_VALID_RPM = 5500.0f;
    static constexpr float MIN_VALID_MAP_KPA = 20.0f;
    static constexpr float MAX_VALID_MAP_KPA = 350.0f;
    static constexpr float MIN_VALID_MAF_GPS = 0.0f;
    static constexpr float MAX_VALID_MAF_GPS = 400.0f;

    // =========================
    // Parametri filtro EMA - TorqueEstimator
    // =========================
    // Load più lento, MAF/MAP un po' più reattivi
    static constexpr float TORQUE_EMA_ALPHA_LOAD = 0.12f;
    static constexpr float TORQUE_EMA_ALPHA_MAF = 0.25f;
    static constexpr float TORQUE_EMA_ALPHA_MAP = 0.25f;

    // =========================
    // Parametri filtro EMA - BoostModel
    // =========================
    static constexpr float BOOST_EMA_ALPHA_MAP = 0.22f;
    static constexpr float BOOST_EMA_ALPHA_BARO = 0.10f;
    static constexpr float BOOST_EMA_ALPHA_MAF = 0.20f;
    static constexpr float BOOST_EMA_ALPHA_LOAD = 0.12f;
}