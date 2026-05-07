/**
 * Tabella descrittiva dei PID OBD2 Mode 01 standard SAE J1979.
 *
 * Mappa ogni PID (0x00-0xFF) a un nome breve (per dashboard web)
 * e a una descrizione estesa in italiano (per output Serial).
 *
 * Le stringhe sono salvate in PROGMEM per non occupare la SRAM.
 *
 * Usato da: CANbus_conn.ino (printScanReportSerial), web_dashboard.h
 *           (handleScanData).
 *
 * @since 07/05/26 Mattia Alesi
 */

#ifndef PID_DESCRIPTIONS_H
#define PID_DESCRIPTIONS_H

#include <Arduino.h>
#include <pgmspace.h>

/** Singola riga della tabella descrittiva. */
struct PIDDescription {
  uint8_t pid;
  const char* shortName;  // Sigla compatta (es. "RPM", "MAF", "IAT")
  const char* fullName;   // Descrizione estesa in italiano
};

// Stringhe singole in PROGMEM per ridurre frammentazione flash
static const char PD_S_00[] PROGMEM = "PID supportati 01-20";
static const char PD_S_01[] PROGMEM = "Stato monitor dal clear DTC";
static const char PD_S_02[] PROGMEM = "DTC freeze frame";
static const char PD_S_03[] PROGMEM = "Stato sistema carburante";
static const char PD_S_04[] PROGMEM = "Carico motore calcolato";
static const char PD_S_05[] PROGMEM = "Temperatura liquido raffreddamento";
static const char PD_S_06[] PROGMEM = "Trim carburante a breve termine - Banco 1";
static const char PD_S_07[] PROGMEM = "Trim carburante a lungo termine - Banco 1";
static const char PD_S_08[] PROGMEM = "Trim carburante a breve termine - Banco 2";
static const char PD_S_09[] PROGMEM = "Trim carburante a lungo termine - Banco 2";
static const char PD_S_0A[] PROGMEM = "Pressione carburante";
static const char PD_S_0B[] PROGMEM = "Pressione assoluta collettore aspirazione";
static const char PD_S_0C[] PROGMEM = "Regime motore";
static const char PD_S_0D[] PROGMEM = "Velocita' veicolo";
static const char PD_S_0E[] PROGMEM = "Anticipo accensione";
static const char PD_S_0F[] PROGMEM = "Temperatura aria aspirata";
static const char PD_S_10[] PROGMEM = "Portata massa aria (MAF)";
static const char PD_S_11[] PROGMEM = "Posizione farfalla";
static const char PD_S_12[] PROGMEM = "Stato aria secondaria comandata";
static const char PD_S_13[] PROGMEM = "Sonde lambda presenti (2 banchi)";
static const char PD_S_14[] PROGMEM = "Sonda lambda 1 (B1S1)";
static const char PD_S_15[] PROGMEM = "Sonda lambda 2 (B1S2)";
static const char PD_S_16[] PROGMEM = "Sonda lambda 3";
static const char PD_S_17[] PROGMEM = "Sonda lambda 4";
static const char PD_S_18[] PROGMEM = "Sonda lambda 5";
static const char PD_S_19[] PROGMEM = "Sonda lambda 6";
static const char PD_S_1A[] PROGMEM = "Sonda lambda 7";
static const char PD_S_1B[] PROGMEM = "Sonda lambda 8";
static const char PD_S_1C[] PROGMEM = "Standard OBD del veicolo";
static const char PD_S_1D[] PROGMEM = "Sonde lambda presenti (4 banchi)";
static const char PD_S_1E[] PROGMEM = "Stato ingresso ausiliario";
static const char PD_S_1F[] PROGMEM = "Tempo motore dall'avviamento";
static const char PD_S_20[] PROGMEM = "PID supportati 21-40";
static const char PD_S_21[] PROGMEM = "Distanza percorsa con MIL accesa";
static const char PD_S_22[] PROGMEM = "Pressione rail (relativa al collettore)";
static const char PD_S_23[] PROGMEM = "Pressione rail (diesel/iniezione diretta)";
static const char PD_S_24[] PROGMEM = "Sonda lambda 1 - rapporto aria/carburante + tensione";
static const char PD_S_25[] PROGMEM = "Sonda lambda 2 - rapporto aria/carburante + tensione";
static const char PD_S_26[] PROGMEM = "Sonda lambda 3 - rapporto aria/carburante + tensione";
static const char PD_S_27[] PROGMEM = "Sonda lambda 4 - rapporto aria/carburante + tensione";
static const char PD_S_28[] PROGMEM = "Sonda lambda 5 - rapporto aria/carburante + tensione";
static const char PD_S_29[] PROGMEM = "Sonda lambda 6 - rapporto aria/carburante + tensione";
static const char PD_S_2A[] PROGMEM = "Sonda lambda 7 - rapporto aria/carburante + tensione";
static const char PD_S_2B[] PROGMEM = "Sonda lambda 8 - rapporto aria/carburante + tensione";
static const char PD_S_2C[] PROGMEM = "EGR comandato";
static const char PD_S_2D[] PROGMEM = "Errore EGR";
static const char PD_S_2E[] PROGMEM = "Spurgo evaporativo comandato";
static const char PD_S_2F[] PROGMEM = "Livello serbatoio carburante";
static const char PD_S_30[] PROGMEM = "Riscaldamenti dal clear DTC";
static const char PD_S_31[] PROGMEM = "Distanza percorsa dal clear DTC";
static const char PD_S_32[] PROGMEM = "Pressione vapori sistema evap.";
static const char PD_S_33[] PROGMEM = "Pressione barometrica assoluta";
static const char PD_S_34[] PROGMEM = "Sonda lambda 1 - rapporto aria/carburante + corrente";
static const char PD_S_35[] PROGMEM = "Sonda lambda 2 - rapporto aria/carburante + corrente";
static const char PD_S_36[] PROGMEM = "Sonda lambda 3 - rapporto aria/carburante + corrente";
static const char PD_S_37[] PROGMEM = "Sonda lambda 4 - rapporto aria/carburante + corrente";
static const char PD_S_38[] PROGMEM = "Sonda lambda 5 - rapporto aria/carburante + corrente";
static const char PD_S_39[] PROGMEM = "Sonda lambda 6 - rapporto aria/carburante + corrente";
static const char PD_S_3A[] PROGMEM = "Sonda lambda 7 - rapporto aria/carburante + corrente";
static const char PD_S_3B[] PROGMEM = "Sonda lambda 8 - rapporto aria/carburante + corrente";
static const char PD_S_3C[] PROGMEM = "Temperatura catalizzatore B1S1";
static const char PD_S_3D[] PROGMEM = "Temperatura catalizzatore B2S1";
static const char PD_S_3E[] PROGMEM = "Temperatura catalizzatore B1S2";
static const char PD_S_3F[] PROGMEM = "Temperatura catalizzatore B2S2";
static const char PD_S_40[] PROGMEM = "PID supportati 41-60";
static const char PD_S_41[] PROGMEM = "Stato monitor del drive cycle corrente";
static const char PD_S_42[] PROGMEM = "Tensione modulo di controllo (batteria)";
static const char PD_S_43[] PROGMEM = "Carico assoluto";
static const char PD_S_44[] PROGMEM = "Rapporto stechiometrico comandato";
static const char PD_S_45[] PROGMEM = "Posizione relativa farfalla";
static const char PD_S_46[] PROGMEM = "Temperatura aria ambiente";
static const char PD_S_47[] PROGMEM = "Posizione assoluta farfalla B";
static const char PD_S_48[] PROGMEM = "Posizione assoluta farfalla C";
static const char PD_S_49[] PROGMEM = "Posizione pedale acceleratore D";
static const char PD_S_4A[] PROGMEM = "Posizione pedale acceleratore E";
static const char PD_S_4B[] PROGMEM = "Posizione pedale acceleratore F";
static const char PD_S_4C[] PROGMEM = "Attuatore farfalla comandato";
static const char PD_S_4D[] PROGMEM = "Tempo motore con MIL accesa";
static const char PD_S_4E[] PROGMEM = "Tempo dal clear DTC";
static const char PD_S_4F[] PROGMEM = "Massimi rapporto aria/carburante, V/I sonda, MAP";
static const char PD_S_50[] PROGMEM = "Massimo MAF";
static const char PD_S_51[] PROGMEM = "Tipo carburante";
static const char PD_S_52[] PROGMEM = "Percentuale etanolo";
static const char PD_S_53[] PROGMEM = "Pressione assoluta vapori sistema evap.";
static const char PD_S_54[] PROGMEM = "Pressione vapori sistema evap.";
static const char PD_S_55[] PROGMEM = "Trim sonda lambda secondaria a breve termine - B1/B3";
static const char PD_S_56[] PROGMEM = "Trim sonda lambda secondaria a lungo termine - B1/B3";
static const char PD_S_57[] PROGMEM = "Trim sonda lambda secondaria a breve termine - B2/B4";
static const char PD_S_58[] PROGMEM = "Trim sonda lambda secondaria a lungo termine - B2/B4";
static const char PD_S_59[] PROGMEM = "Pressione assoluta rail carburante";
static const char PD_S_5A[] PROGMEM = "Posizione relativa pedale acceleratore";
static const char PD_S_5B[] PROGMEM = "Vita residua batteria pacco ibrido";
static const char PD_S_5C[] PROGMEM = "Temperatura olio motore";
static const char PD_S_5D[] PROGMEM = "Anticipo iniezione";
static const char PD_S_5E[] PROGMEM = "Consumo carburante motore";
static const char PD_S_5F[] PROGMEM = "Requisiti emissioni del veicolo";
static const char PD_S_60[] PROGMEM = "PID supportati 61-80";
static const char PD_S_61[] PROGMEM = "Coppia richiesta dal pilota (%)";
static const char PD_S_62[] PROGMEM = "Coppia effettiva motore (%)";
static const char PD_S_63[] PROGMEM = "Coppia di riferimento motore";
static const char PD_S_64[] PROGMEM = "Dati coppia motore (%)";
static const char PD_S_65[] PROGMEM = "I/O ausiliari supportati";
static const char PD_S_66[] PROGMEM = "Sensore MAF (esteso)";
static const char PD_S_67[] PROGMEM = "Temperatura liquido (esteso)";
static const char PD_S_68[] PROGMEM = "Sensore temperatura aria aspirata (esteso)";
static const char PD_S_69[] PROGMEM = "EGR comandato + errore (esteso)";
static const char PD_S_6A[] PROGMEM = "Controllo flusso aria diesel comandato";
static const char PD_S_6B[] PROGMEM = "Temperatura ricircolo gas scarico (EGR)";
static const char PD_S_6C[] PROGMEM = "Controllo attuatore farfalla + posizione relativa";
static const char PD_S_6D[] PROGMEM = "Sistema controllo pressione carburante";
static const char PD_S_6E[] PROGMEM = "Sistema controllo pressione iniezione";
static const char PD_S_6F[] PROGMEM = "Pressione ingresso compressore turbo";
static const char PD_S_70[] PROGMEM = "Controllo pressione boost";
static const char PD_S_71[] PROGMEM = "Controllo geometria variabile turbo (VGT)";
static const char PD_S_72[] PROGMEM = "Controllo wastegate";
static const char PD_S_73[] PROGMEM = "Pressione gas di scarico";
static const char PD_S_74[] PROGMEM = "Regime turbo";
static const char PD_S_75[] PROGMEM = "Temperatura turbo (A)";
static const char PD_S_76[] PROGMEM = "Temperatura turbo (B)";
static const char PD_S_77[] PROGMEM = "Temperatura intercooler (CACT)";
static const char PD_S_78[] PROGMEM = "Temperatura gas di scarico Banco 1";
static const char PD_S_79[] PROGMEM = "Temperatura gas di scarico Banco 2";
static const char PD_S_7A[] PROGMEM = "DPF (filtro particolato) - dati";
static const char PD_S_7B[] PROGMEM = "DPF (filtro particolato) - dati";
static const char PD_S_7C[] PROGMEM = "Temperatura DPF";
static const char PD_S_7D[] PROGMEM = "Stato area NOx NTE";
static const char PD_S_7E[] PROGMEM = "Stato area PM NTE";
static const char PD_S_7F[] PROGMEM = "Tempo cumulativo motore";
static const char PD_S_80[] PROGMEM = "PID supportati 81-A0";
static const char PD_S_81[] PROGMEM = "Tempo motore AECD #1-10";
static const char PD_S_82[] PROGMEM = "Tempo motore AECD #11-20";
static const char PD_S_83[] PROGMEM = "Sensore NOx";
static const char PD_S_84[] PROGMEM = "Temperatura superficie collettore";
static const char PD_S_85[] PROGMEM = "Sistema reagente NOx (AdBlue)";
static const char PD_S_86[] PROGMEM = "Sensore particolato (PM)";
static const char PD_S_87[] PROGMEM = "Pressione assoluta collettore (esteso)";
static const char PD_S_88[] PROGMEM = "Sistema induzione SCR";
static const char PD_S_8B[] PROGMEM = "Diagnostica post-trattamento diesel";
static const char PD_S_8D[] PROGMEM = "Sonda lambda Wide Range";
static const char PD_S_8E[] PROGMEM = "Coppia frizione motore (%)";
static const char PD_S_8F[] PROGMEM = "Sensore PM Banco 1 e 2";
static const char PD_S_92[] PROGMEM = "Controllo sistema carburante";
static const char PD_S_9A[] PROGMEM = "Dati sistema veicolo ibrido/EV";
static const char PD_S_9B[] PROGMEM = "Sensore AdBlue";
static const char PD_S_9D[] PROGMEM = "Consumo motore (esteso)";
static const char PD_S_A0[] PROGMEM = "PID supportati A1-C0";
static const char PD_S_A2[] PROGMEM = "Consumo per cilindro";
static const char PD_S_A4[] PROGMEM = "Marcia attuale cambio";
static const char PD_S_A6[] PROGMEM = "Contachilometri";
static const char PD_S_C0[] PROGMEM = "PID supportati C1-E0";

// Sigle compatte per dashboard web — copertura completa dei PID Mode 01
// standard SAE J1979. Una stringa per ogni PID per cui esiste un fullName.
static const char PD_N_00[] PROGMEM = "PIDs 01-20";
static const char PD_N_01[] PROGMEM = "Stato MIL + #DTC";
static const char PD_N_02[] PROGMEM = "Freeze DTC";
static const char PD_N_03[] PROGMEM = "Stato carburante";
static const char PD_N_04[] PROGMEM = "Carico motore";
static const char PD_N_05[] PROGMEM = "Temp. liquido";
static const char PD_N_06[] PROGMEM = "Trim breve B1";
static const char PD_N_07[] PROGMEM = "Trim lungo B1";
static const char PD_N_08[] PROGMEM = "Trim breve B2";
static const char PD_N_09[] PROGMEM = "Trim lungo B2";
static const char PD_N_0A[] PROGMEM = "Press. carburante";
static const char PD_N_0B[] PROGMEM = "Press. collettore";
static const char PD_N_0C[] PROGMEM = "RPM";
static const char PD_N_0D[] PROGMEM = "Velocita'";
static const char PD_N_0E[] PROGMEM = "Anticipo accensione";
static const char PD_N_0F[] PROGMEM = "Temp. aria aspirata";
static const char PD_N_10[] PROGMEM = "MAF";
static const char PD_N_11[] PROGMEM = "Farfalla";
static const char PD_N_12[] PROGMEM = "Aria secondaria";
static const char PD_N_13[] PROGMEM = "Lambda 2 banchi";
static const char PD_N_14[] PROGMEM = "Lambda B1S1";
static const char PD_N_15[] PROGMEM = "Lambda B1S2";
static const char PD_N_16[] PROGMEM = "Lambda 3";
static const char PD_N_17[] PROGMEM = "Lambda 4";
static const char PD_N_18[] PROGMEM = "Lambda 5";
static const char PD_N_19[] PROGMEM = "Lambda 6";
static const char PD_N_1A[] PROGMEM = "Lambda 7";
static const char PD_N_1B[] PROGMEM = "Lambda 8";
static const char PD_N_1C[] PROGMEM = "Standard OBD";
static const char PD_N_1D[] PROGMEM = "Lambda 4 banchi";
static const char PD_N_1E[] PROGMEM = "Aux input";
static const char PD_N_1F[] PROGMEM = "Tempo motore";
static const char PD_N_20[] PROGMEM = "PIDs 21-40";
static const char PD_N_21[] PROGMEM = "Km con MIL";
static const char PD_N_22[] PROGMEM = "Press. rail (rel.)";
static const char PD_N_23[] PROGMEM = "Press. rail";
static const char PD_N_24[] PROGMEM = "Lambda 1 (lambda+V)";
static const char PD_N_25[] PROGMEM = "Lambda 2 (lambda+V)";
static const char PD_N_26[] PROGMEM = "Lambda 3 (lambda+V)";
static const char PD_N_27[] PROGMEM = "Lambda 4 (lambda+V)";
static const char PD_N_28[] PROGMEM = "Lambda 5 (lambda+V)";
static const char PD_N_29[] PROGMEM = "Lambda 6 (lambda+V)";
static const char PD_N_2A[] PROGMEM = "Lambda 7 (lambda+V)";
static const char PD_N_2B[] PROGMEM = "Lambda 8 (lambda+V)";
static const char PD_N_2C[] PROGMEM = "EGR comandato";
static const char PD_N_2D[] PROGMEM = "Errore EGR";
static const char PD_N_2E[] PROGMEM = "Spurgo evap.";
static const char PD_N_2F[] PROGMEM = "Livello carburante";
static const char PD_N_30[] PROGMEM = "Avviamenti";
static const char PD_N_31[] PROGMEM = "Km dal clear DTC";
static const char PD_N_32[] PROGMEM = "Press. vapori evap.";
static const char PD_N_33[] PROGMEM = "Press. barometrica";
static const char PD_N_34[] PROGMEM = "Lambda 1 (lambda+I)";
static const char PD_N_35[] PROGMEM = "Lambda 2 (lambda+I)";
static const char PD_N_36[] PROGMEM = "Lambda 3 (lambda+I)";
static const char PD_N_37[] PROGMEM = "Lambda 4 (lambda+I)";
static const char PD_N_38[] PROGMEM = "Lambda 5 (lambda+I)";
static const char PD_N_39[] PROGMEM = "Lambda 6 (lambda+I)";
static const char PD_N_3A[] PROGMEM = "Lambda 7 (lambda+I)";
static const char PD_N_3B[] PROGMEM = "Lambda 8 (lambda+I)";
static const char PD_N_3C[] PROGMEM = "Temp. cat. B1S1";
static const char PD_N_3D[] PROGMEM = "Temp. cat. B2S1";
static const char PD_N_3E[] PROGMEM = "Temp. cat. B1S2";
static const char PD_N_3F[] PROGMEM = "Temp. cat. B2S2";
static const char PD_N_40[] PROGMEM = "PIDs 41-60";
static const char PD_N_41[] PROGMEM = "Monitor drive cycle";
static const char PD_N_42[] PROGMEM = "Tensione batteria";
static const char PD_N_43[] PROGMEM = "Carico assoluto";
static const char PD_N_44[] PROGMEM = "Stech. comandato";
static const char PD_N_45[] PROGMEM = "Farfalla relativa";
static const char PD_N_46[] PROGMEM = "Temp. ambiente";
static const char PD_N_47[] PROGMEM = "Farfalla B ass.";
static const char PD_N_48[] PROGMEM = "Farfalla C ass.";
static const char PD_N_49[] PROGMEM = "Pedale D";
static const char PD_N_4A[] PROGMEM = "Pedale E";
static const char PD_N_4B[] PROGMEM = "Pedale F";
static const char PD_N_4C[] PROGMEM = "Attuatore farfalla";
static const char PD_N_4D[] PROGMEM = "Tempo MIL on";
static const char PD_N_4E[] PROGMEM = "Tempo dal clear";
static const char PD_N_4F[] PROGMEM = "Massimi calibr.";
static const char PD_N_50[] PROGMEM = "MAF max";
static const char PD_N_51[] PROGMEM = "Tipo carburante";
static const char PD_N_52[] PROGMEM = "% etanolo";
static const char PD_N_53[] PROGMEM = "Press. evap. ass.";
static const char PD_N_54[] PROGMEM = "Press. evap.";
static const char PD_N_55[] PROGMEM = "Trim sec. breve B1/B3";
static const char PD_N_56[] PROGMEM = "Trim sec. lungo B1/B3";
static const char PD_N_57[] PROGMEM = "Trim sec. breve B2/B4";
static const char PD_N_58[] PROGMEM = "Trim sec. lungo B2/B4";
static const char PD_N_59[] PROGMEM = "Press. rail assoluta";
static const char PD_N_5A[] PROGMEM = "Pedale rel.";
static const char PD_N_5B[] PROGMEM = "Batteria ibrido";
static const char PD_N_5C[] PROGMEM = "Temp. olio motore";
static const char PD_N_5D[] PROGMEM = "Anticipo iniezione";
static const char PD_N_5E[] PROGMEM = "Consumo motore";
static const char PD_N_5F[] PROGMEM = "Requisiti emiss.";
static const char PD_N_60[] PROGMEM = "PIDs 61-80";
static const char PD_N_61[] PROGMEM = "Coppia richiesta";
static const char PD_N_62[] PROGMEM = "Coppia effettiva";
static const char PD_N_63[] PROGMEM = "Coppia riferimento";
static const char PD_N_64[] PROGMEM = "Dati coppia";
static const char PD_N_65[] PROGMEM = "I/O aux";
static const char PD_N_66[] PROGMEM = "MAF esteso";
static const char PD_N_67[] PROGMEM = "Coolant esteso";
static const char PD_N_68[] PROGMEM = "IAT esteso";
static const char PD_N_69[] PROGMEM = "EGR esteso";
static const char PD_N_6A[] PROGMEM = "Flusso aria diesel";
static const char PD_N_6B[] PROGMEM = "Temp. EGR";
static const char PD_N_6C[] PROGMEM = "Farfalla esteso";
static const char PD_N_6D[] PROGMEM = "Sist. press. carb.";
static const char PD_N_6E[] PROGMEM = "Sist. press. iniez.";
static const char PD_N_6F[] PROGMEM = "Press. ingresso comp.";
static const char PD_N_70[] PROGMEM = "Boost comandato";
static const char PD_N_71[] PROGMEM = "VGT";
static const char PD_N_72[] PROGMEM = "Wastegate";
static const char PD_N_73[] PROGMEM = "Press. scarico";
static const char PD_N_74[] PROGMEM = "Regime turbo";
static const char PD_N_75[] PROGMEM = "Temp. turbo A";
static const char PD_N_76[] PROGMEM = "Temp. turbo B";
static const char PD_N_77[] PROGMEM = "Temp. intercooler";
static const char PD_N_78[] PROGMEM = "Temp. scarico B1";
static const char PD_N_79[] PROGMEM = "Temp. scarico B2";
static const char PD_N_7A[] PROGMEM = "DPF dati A";
static const char PD_N_7B[] PROGMEM = "DPF dati B";
static const char PD_N_7C[] PROGMEM = "Temp. DPF";
static const char PD_N_7D[] PROGMEM = "NOx NTE";
static const char PD_N_7E[] PROGMEM = "PM NTE";
static const char PD_N_7F[] PROGMEM = "Tempo motore tot.";
static const char PD_N_80[] PROGMEM = "PIDs 81-A0";
static const char PD_N_81[] PROGMEM = "AECD 1-10";
static const char PD_N_82[] PROGMEM = "AECD 11-20";
static const char PD_N_83[] PROGMEM = "Sensore NOx";
static const char PD_N_84[] PROGMEM = "Temp. collettore";
static const char PD_N_85[] PROGMEM = "AdBlue";
static const char PD_N_86[] PROGMEM = "Sensore PM";
static const char PD_N_87[] PROGMEM = "MAP esteso";
static const char PD_N_88[] PROGMEM = "SCR";
static const char PD_N_8B[] PROGMEM = "Post-trattamento";
static const char PD_N_8D[] PROGMEM = "Lambda Wide";
static const char PD_N_8E[] PROGMEM = "Coppia frizione";
static const char PD_N_8F[] PROGMEM = "PM B1/B2";
static const char PD_N_92[] PROGMEM = "Controllo carb.";
static const char PD_N_9A[] PROGMEM = "Ibrido/EV";
static const char PD_N_9B[] PROGMEM = "AdBlue sens.";
static const char PD_N_9D[] PROGMEM = "Consumo esteso";
static const char PD_N_A0[] PROGMEM = "PIDs A1-C0";
static const char PD_N_A2[] PROGMEM = "Consumo cilindro";
static const char PD_N_A4[] PROGMEM = "Marcia";
static const char PD_N_A6[] PROGMEM = "Contachilometri";
static const char PD_N_C0[] PROGMEM = "PIDs C1-E0";

// Tabella indicizzata: index = PID. Slot vuoti = nullptr (PID non documentato).
// NB: usiamo array di puntatori PROGMEM per accesso O(1) in lookup.
// Layout speculare a PID_FULL_NAMES per coerenza.
static const char* const PID_SHORT_NAMES[256] PROGMEM = {
  /* 0x00 */ PD_N_00, PD_N_01, PD_N_02, PD_N_03, PD_N_04, PD_N_05, PD_N_06, PD_N_07,
  /* 0x08 */ PD_N_08, PD_N_09, PD_N_0A, PD_N_0B, PD_N_0C, PD_N_0D, PD_N_0E, PD_N_0F,
  /* 0x10 */ PD_N_10, PD_N_11, PD_N_12, PD_N_13, PD_N_14, PD_N_15, PD_N_16, PD_N_17,
  /* 0x18 */ PD_N_18, PD_N_19, PD_N_1A, PD_N_1B, PD_N_1C, PD_N_1D, PD_N_1E, PD_N_1F,
  /* 0x20 */ PD_N_20, PD_N_21, PD_N_22, PD_N_23, PD_N_24, PD_N_25, PD_N_26, PD_N_27,
  /* 0x28 */ PD_N_28, PD_N_29, PD_N_2A, PD_N_2B, PD_N_2C, PD_N_2D, PD_N_2E, PD_N_2F,
  /* 0x30 */ PD_N_30, PD_N_31, PD_N_32, PD_N_33, PD_N_34, PD_N_35, PD_N_36, PD_N_37,
  /* 0x38 */ PD_N_38, PD_N_39, PD_N_3A, PD_N_3B, PD_N_3C, PD_N_3D, PD_N_3E, PD_N_3F,
  /* 0x40 */ PD_N_40, PD_N_41, PD_N_42, PD_N_43, PD_N_44, PD_N_45, PD_N_46, PD_N_47,
  /* 0x48 */ PD_N_48, PD_N_49, PD_N_4A, PD_N_4B, PD_N_4C, PD_N_4D, PD_N_4E, PD_N_4F,
  /* 0x50 */ PD_N_50, PD_N_51, PD_N_52, PD_N_53, PD_N_54, PD_N_55, PD_N_56, PD_N_57,
  /* 0x58 */ PD_N_58, PD_N_59, PD_N_5A, PD_N_5B, PD_N_5C, PD_N_5D, PD_N_5E, PD_N_5F,
  /* 0x60 */ PD_N_60, PD_N_61, PD_N_62, PD_N_63, PD_N_64, PD_N_65, PD_N_66, PD_N_67,
  /* 0x68 */ PD_N_68, PD_N_69, PD_N_6A, PD_N_6B, PD_N_6C, PD_N_6D, PD_N_6E, PD_N_6F,
  /* 0x70 */ PD_N_70, PD_N_71, PD_N_72, PD_N_73, PD_N_74, PD_N_75, PD_N_76, PD_N_77,
  /* 0x78 */ PD_N_78, PD_N_79, PD_N_7A, PD_N_7B, PD_N_7C, PD_N_7D, PD_N_7E, PD_N_7F,
  /* 0x80 */ PD_N_80, PD_N_81, PD_N_82, PD_N_83, PD_N_84, PD_N_85, PD_N_86, PD_N_87,
  /* 0x88 */ PD_N_88, nullptr, nullptr, PD_N_8B, nullptr, PD_N_8D, PD_N_8E, PD_N_8F,
  /* 0x90 */ nullptr, nullptr, PD_N_92, nullptr, nullptr, nullptr, nullptr, nullptr,
  /* 0x98 */ nullptr, nullptr, PD_N_9A, PD_N_9B, nullptr, PD_N_9D, nullptr, nullptr,
  /* 0xA0 */ PD_N_A0, nullptr, PD_N_A2, nullptr, PD_N_A4, nullptr, PD_N_A6, nullptr,
  /* 0xA8-0xBF tutti nullptr */
  nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
  nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
  /* 0xC0 */ PD_N_C0, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
  /* 0xC8-0xFF tutti nullptr */
  nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
  nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
  nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
  nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
  nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
  nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
  nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr
};

static const char* const PID_FULL_NAMES[256] PROGMEM = {
  /* 0x00 */ PD_S_00, PD_S_01, PD_S_02, PD_S_03, PD_S_04, PD_S_05, PD_S_06, PD_S_07,
  /* 0x08 */ PD_S_08, PD_S_09, PD_S_0A, PD_S_0B, PD_S_0C, PD_S_0D, PD_S_0E, PD_S_0F,
  /* 0x10 */ PD_S_10, PD_S_11, PD_S_12, PD_S_13, PD_S_14, PD_S_15, PD_S_16, PD_S_17,
  /* 0x18 */ PD_S_18, PD_S_19, PD_S_1A, PD_S_1B, PD_S_1C, PD_S_1D, PD_S_1E, PD_S_1F,
  /* 0x20 */ PD_S_20, PD_S_21, PD_S_22, PD_S_23, PD_S_24, PD_S_25, PD_S_26, PD_S_27,
  /* 0x28 */ PD_S_28, PD_S_29, PD_S_2A, PD_S_2B, PD_S_2C, PD_S_2D, PD_S_2E, PD_S_2F,
  /* 0x30 */ PD_S_30, PD_S_31, PD_S_32, PD_S_33, PD_S_34, PD_S_35, PD_S_36, PD_S_37,
  /* 0x38 */ PD_S_38, PD_S_39, PD_S_3A, PD_S_3B, PD_S_3C, PD_S_3D, PD_S_3E, PD_S_3F,
  /* 0x40 */ PD_S_40, PD_S_41, PD_S_42, PD_S_43, PD_S_44, PD_S_45, PD_S_46, PD_S_47,
  /* 0x48 */ PD_S_48, PD_S_49, PD_S_4A, PD_S_4B, PD_S_4C, PD_S_4D, PD_S_4E, PD_S_4F,
  /* 0x50 */ PD_S_50, PD_S_51, PD_S_52, PD_S_53, PD_S_54, PD_S_55, PD_S_56, PD_S_57,
  /* 0x58 */ PD_S_58, PD_S_59, PD_S_5A, PD_S_5B, PD_S_5C, PD_S_5D, PD_S_5E, PD_S_5F,
  /* 0x60 */ PD_S_60, PD_S_61, PD_S_62, PD_S_63, PD_S_64, PD_S_65, PD_S_66, PD_S_67,
  /* 0x68 */ PD_S_68, PD_S_69, PD_S_6A, PD_S_6B, PD_S_6C, PD_S_6D, PD_S_6E, PD_S_6F,
  /* 0x70 */ PD_S_70, PD_S_71, PD_S_72, PD_S_73, PD_S_74, PD_S_75, PD_S_76, PD_S_77,
  /* 0x78 */ PD_S_78, PD_S_79, PD_S_7A, PD_S_7B, PD_S_7C, PD_S_7D, PD_S_7E, PD_S_7F,
  /* 0x80 */ PD_S_80, PD_S_81, PD_S_82, PD_S_83, PD_S_84, PD_S_85, PD_S_86, PD_S_87,
  /* 0x88 */ PD_S_88, nullptr, nullptr, PD_S_8B, nullptr, PD_S_8D, PD_S_8E, PD_S_8F,
  /* 0x90 */ nullptr, nullptr, PD_S_92, nullptr, nullptr, nullptr, nullptr, nullptr,
  /* 0x98 */ nullptr, nullptr, PD_S_9A, PD_S_9B, nullptr, PD_S_9D, nullptr, nullptr,
  /* 0xA0 */ PD_S_A0, nullptr, PD_S_A2, nullptr, PD_S_A4, nullptr, PD_S_A6, nullptr,
  /* 0xA8-0xBF tutti nullptr */
  nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
  nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
  /* 0xC0 */ PD_S_C0, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
  /* 0xC8-0xFF tutti nullptr */
  nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
  nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
  nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
  nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
  nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
  nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
  nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr
};

/**
 * Restituisce la sigla compatta del PID (es. "RPM", "MAF") in PROGMEM.
 * Ritorna nullptr se il PID non e' documentato.
 * Pensata per uso in dashboard web (descrizioni concise).
 */
inline const char* getPIDShortName(uint8_t pid) {
  return (const char*)pgm_read_ptr(&PID_SHORT_NAMES[pid]);
}

/**
 * Restituisce la descrizione estesa in italiano del PID in PROGMEM.
 * Ritorna nullptr se il PID non e' documentato.
 * Pensata per uso in output Serial.
 */
inline const char* getPIDFullName(uint8_t pid) {
  return (const char*)pgm_read_ptr(&PID_FULL_NAMES[pid]);
}

#endif
