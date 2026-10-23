#pragma once

// Parametri dell'Audi A5 F5 (B9.5) 40 TFSI 150 kW MHEV, motore DMSA, cambio S tronic 0CK a trazione
// anteriore, ricavati dalla scheda tecnica Audi (eTD).
namespace Audi40TFSI150kW
{
    // =========================
    // Parametri fisici condivisi
    // =========================
    static constexpr float ENGINE_DISPLACEMENT_M3 = 0.001984f; // m^3, 1.984 L
    static constexpr float GAS_CONSTANT_AIR = 287.05f;         // J/(kg*K)

    // =========================
    // Parametri motore / modello coppia
    // =========================
    static constexpr float TORQUE_MAX_NM = 320.0f;
    static constexpr float POWER_MAX_KW = 150.0f;
    static constexpr float TORQUE_PLATEAU_START_RPM = 1450.0f;
    static constexpr float TORQUE_PLATEAU_END_RPM = 4475.0f;
    // Aria a pieno carico per kW: BSFC benzina di circa 250 g/kWh per AFR 14.7 a lambda 1, diviso 3600.
    static constexpr float K_AIR_GPS_PER_KW = 1.02f;

    // =========================
    // Carburante (benzina 95 EN 228)
    // =========================
    // AFR stechiometrico della benzina senza etanolo: con la E5 il valore reale è circa il 2% più basso.
    static constexpr float FUEL_STOICHIOMETRIC_AFR = 14.7f;
    static constexpr float FUEL_DENSITY_G_PER_L = 745.0f;     // EN 228 ammette 720-775 g/L

    // =========================
    // Trasmissione e ruote
    // =========================
    static constexpr float FINAL_DRIVE_RATIO = 4.234f;        // coppia finale S tronic 0CK
    // Pneumatico di serie 225/50 R17: 2.06 m di circonferenza geometrica, ridotta di circa il 3% sotto carico.
    static constexpr float TYRE_ROLLING_CIRCUMFERENCE_M = 2.00f;

    // =========================
    // Range sanity / plausibili
    // =========================
    static constexpr float IDLE_MIN_RPM = 550.0f;
    static constexpr float MAX_VALID_RPM = 6500.0f;           // potenza massima fino a 6000 rpm, più margine
    static constexpr float MIN_VALID_MAP_KPA = 20.0f;
    static constexpr float MAX_VALID_MAP_KPA = 350.0f;
    static constexpr float MIN_VALID_MAF_GPS = 0.0f;
    static constexpr float MAX_VALID_MAF_GPS = 400.0f;

    // =========================
    // Parametri filtro EMA - TorqueModel
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