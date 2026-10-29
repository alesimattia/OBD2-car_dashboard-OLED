/**
 * Tabelle dei segnali OBDb interpretabili per la scansione completa, anno modello 2022. File generato da
 * tools/obdb_select.ps1 dai signalset OBDb/SAEJ1979@0669017 e OBDb/Audi-A5@a821497 (licenza
 * CC-BY-SA-4.0, vedi tools/obdb/README.md): non va modificato a mano, si rigenera con lo script.
 *
 * Usato da: obd_scan.h
 *
 * @Mattia Alesi
 */

#ifndef OBDB_SIGNALS_H
#define OBDB_SIGNALS_H

#include "obd_proto.h"

static constexpr uint16_t OBDB_MODEL_YEAR = 2022;
static constexpr uint16_t OBDB_COMMAND_COUNT = 374;
static constexpr uint16_t OBDB_SIGNAL_COUNT = 1277;

// Testi terminati da zero, indirizzati dagli offset dei descrittori
static const char OBDB_TEXT[] OBD_PROGMEM =
	"\000"
	"spento\000"
	"acceso\000"
	"Malfunction indicator lamp\000"
	"Number of DTCs stored in this ECU\000"
	"s\303\254\000"
	"no\000"
	"Comprehensive component monitoring ready\000"
	"Fuel system monitoring ready\000"
	"Misfire monitoring ready\000"
	"EGR system monitoring ready\000"
	"Oxygen sensor heater monitoring ready\000"
	"Oxygen sensor monitoring ready\000"
	"A/C system refrigerant monitoring ready\000"
	"Secondary air system monitoring ready\000"
	"Evaporative system monitoring ready\000"
	"Heated catalyst monitoring ready\000"
	"Catalyst monitoring ready\000"
	"Spark ignition monitors supported\000"
	"Compression ignition monitors supported\000"
	"Compression ignition monitoring supported\000"
	"DTC that caused required freeze frame data storage\000"
	"No status reported: this fuel system is not running or not present, e.g. the engine is off\000"
	"Open loop - has not yet satisfied conditions to go closed loop\000"
	"Closed loop - using oxygen sensor(s) as feedback for fuel control\000"
	"Open loop due to driving conditions (e.g. power enrichment, deceleration enleanment)\000"
	"Open loop - due to detected system fault\000"
	"Closed loop, but fault with at least one oxygen sensor - may be using single oxygen sensor for fuel control\000"
	"Open loop - has not yet satisfied conditions to go closed loop (Bank 2)\000"
	"Open loop due to driving conditions (Bank 2) (e.g. power enrichment, deceleration enleanment, cylinder deactivation)\000"
	"Open loop due to driving conditions on bank 1 and bank 2 (e.g. power enrichment, deceleration enleanment, cylinder deactivation)\000"
	"Open loop - due to detected system fault (Bank 2)\000"
	"Fuel system 1 status\000"
	"Fuel system 2 status\000"
	"%\000"
	"Calculated engine load\000"
	"\302\260C\000"
	"Engine coolant temperature\000"
	"Short term fuel trim (bank 1)\000"
	"Long term fuel trim (bank 1)\000"
	"Short term fuel trim (bank 2)\000"
	"Short term fuel trim (bank 4)\000"
	"Long term fuel trim (bank 2)\000"
	"Long term fuel trim (bank 4)\000"
	"kPa\000"
	"Fuel pressure\000"
	"Intake manifold absolute pressure\000"
	"rpm\000"
	"Engine RPM\000"
	"km/h\000"
	"Vehicle speed\000"
	"\302\260\000"
	"Timing advance\000"
	"Intake air temperature\000"
	"g/s\000"
	"Air flow rate from mass air flow sensor\000"
	"Absolute throttle position\000"
	"Upstream of first catalytic converter\000"
	"Downstream of first catalytic converter inlet\000"
	"Atmosphere / off\000"
	"Pump commanded on for diagnostics\000"
	"Commanded secondary air status\000"
	"02S Bank 2, Sensor 4 present\000"
	"02S Bank 2, Sensor 3 present\000"
	"02S Bank 2, Sensor 2 present\000"
	"02S Bank 2, Sensor 1 present\000"
	"02S Bank 1, Sensor 4 present\000"
	"02S Bank 1, Sensor 3 present\000"
	"02S Bank 1, Sensor 2 present\000"
	"02S Bank 1, Sensor 1 present\000"
	"V\000"
	"O2S Output Voltage Bank 1, Sensor 1\000"
	"SHRTFT associated with O2S11\000"
	"O2S Output Voltage Bank 1, Sensor 2\000"
	"SHRTFT associated with O2S12\000"
	"O2S Output Voltage Bank 1, Sensor 3\000"
	"SHRTFT associated with O2S13\000"
	"O2S Output Voltage Bank 1, Sensor 4\000"
	"SHRTFT associated with O2S14\000"
	"O2S Output Voltage Bank 2, Sensor 1\000"
	"SHRTFT associated with O2S21\000"
	"O2S Output Voltage Bank 2, Sensor 2\000"
	"SHRTFT associated with O2S22\000"
	"O2S Output Voltage Bank 2, Sensor 3\000"
	"SHRTFT associated with O2S23\000"
	"O2S Output Voltage Bank 2, Sensor 4\000"
	"SHRTFT associated with O2S24\000"
	"OBD II (California ARB)\000"
	"OBD (Federal EPA)\000"
	"OBD & OBD II\000"
	"OBD I\000"
	"Not OBD compliant\000"
	"EOBD\000"
	"EOBD & OBD II\000"
	"EOBD & OBD\000"
	"EOBD, OBD, OBD II\000"
	"JOBD\000"
	"JOBD & OBD II\000"
	"JOBD & EOBD\000"
	"JOBD, EOBD, OBD II\000"
	"Heavy duty vehicles (EURO IV) B1\000"
	"Heavy duty vehicles (EURO V) B2\000"
	"Heavy duty vehicles (EURO EEC) C (gas engines)\000"
	"Engine manufacturer diagnostics (EMD)\000"
	"Engine Manufacturer Diagnostics Enhanced (EMD+)\000"
	"Heavy Duty On-Board Diagnostics (Child/Partial)\000"
	"Heavy Duty On-Board Diagnostics\000"
	"World Wide Harmonized OBD\000"
	"Heavy Duty Euro OBD Stage I without NOx Control\000"
	"Heavy Duty Euro OBD Stage I with NOx Control\000"
	"Heavy Duty Euro OBD Stage II without NOx Control\000"
	"Heavy Duty Euro OBD Stage II with NOx Control\000"
	"Heavy Duty ZEV\000"
	"Brazil OBD Phase 1\000"
	"Brazil OBD Phase 2 and Phase 2+\000"
	"Korean OBD\000"
	"India BS4 OBD I\000"
	"India BS4 OBD II\000"
	"Euro VI\000"
	"OBD, OBD II and HD OBD\000"
	"Brazil OBD Phase 3\000"
	"Motorcycle, Euro OBD-I\000"
	"Motorcycle, Euro OBD-II\000"
	"Motorcycle, China OBD-I\000"
	"Motorcycle, Taiwan OBD-I\000"
	"Motorcycle, Japan OBD-I\000"
	"China Nationwide Stage 6\000"
	"Brazil OBD Phase 7\000"
	"China Heavy Duty VI\000"
	"India BS6 OBD I\000"
	"India BS6 OBD II\000"
	"India BSVI HD OBD\000"
	"Brazil OBD Phase 8\000"
	"Japan Heavy Duty OBD-II\000"
	"Korea Heavy Duty OBD-II\000"
	"China Off-Road IV OBD\000"
	"Light Duty ZEV, ACC-II\000"
	"Motorcycle, Japan OBD-II\000"
	"Motorcycle, California (CARB) OBD\000"
	"Motorcycle, Federal (EPA) OBD\000"
	"Motorcycle, 50-State (CARB & EPA) OBD\000"
	"Heavy Duty ZEV, CARB ZEP\000"
	"Light Duty ZEV, CARB ACC-II and EPA Tier 4 (GTR 22)\000"
	"Light Duty ZEV, EPA Tier 4 (GTR 22)\000"
	"EPA HD OBD\000"
	"OBD requirements to which vehicle is designed\000"
	"02S Bank 4, Sensor 2 present\000"
	"02S Bank 4, Sensor 1 present\000"
	"02S Bank 3, Sensor 2 present\000"
	"02S Bank 3, Sensor 1 present\000"
	"Power take off (PTO) status\000"
	"s\000"
	"Time since engine start\000"
	"km\000"
	"Distance traveled while MIL was activated\000"
	"Fuel pressure relative to manifold vacuum\000"
	"Fuel rail pressure\000"
	"Lambda value, Equivalence Ratio Bank 1, Sensor 1\000"
	"Wide Range O2S Voltage, O2 Sensor Bank 1, Sensor 1\000"
	"Lambda value, Equivalence Ratio Bank 1, Sensor 2\000"
	"Wide Range O2S Voltage, O2 Sensor Bank 1, Sensor 2\000"
	"Lambda value, Equivalence Ratio Bank 1, Sensor 3\000"
	"Wide Range O2S Voltage, O2 Sensor Bank 1, Sensor 3\000"
	"Lambda value, Equivalence Ratio Bank 1, Sensor 4\000"
	"Wide Range O2S Voltage, O2 Sensor Bank 1, Sensor 4\000"
	"Lambda value, Equivalence Ratio Bank 2, Sensor 1\000"
	"Wide Range O2S Voltage, O2 Sensor Bank 2, Sensor 1\000"
	"Lambda value, Equivalence Ratio Bank 2, Sensor 2\000"
	"Wide Range O2S Voltage, O2 Sensor Bank 2, Sensor 2\000"
	"Lambda value, Equivalence Ratio Bank 2, Sensor 3\000"
	"Wide Range O2S Voltage, O2 Sensor Bank 2, Sensor 3\000"
	"Lambda value, Equivalence Ratio Bank 2, Sensor 4\000"
	"Wide Range O2S Voltage, O2 Sensor Bank 2, Sensor 4\000"
	"Commanded EGR\000"
	"EGR error\000"
	"Commanded evaporative purge\000"
	"Fuel tank level\000"
	"Number of warm-ups since diagnostic trouble codes cleared\000"
	"Distance traveled since diagnostic trouble codes cleared\000"
	"Evap system vapor pressure\000"
	"Barometric pressure\000"
	"mA\000"
	"Wide Range O2S Current, O2 Sensor Bank 1, Sensor 1\000"
	"Wide Range O2S Current, O2 Sensor Bank 1, Sensor 2\000"
	"Wide Range O2S Current, O2 Sensor Bank 1, Sensor 3\000"
	"Wide Range O2S Current, O2 Sensor Bank 1, Sensor 4\000"
	"Wide Range O2S Current, O2 Sensor Bank 2, Sensor 1\000"
	"Wide Range O2S Current, O2 Sensor Bank 2, Sensor 2\000"
	"Wide Range O2S Current, O2 Sensor Bank 2, Sensor 3\000"
	"Wide Range O2S Current, O2 Sensor Bank 2, Sensor 4\000"
	"Catalyst temperature bank 1, sensor 1\000"
	"Catalyst temperature bank 2, sensor 1\000"
	"Catalyst temperature bank 1, sensor 2\000"
	"Catalyst temperature bank 2, sensor 2\000"
	"Comprehensive component monitoring completed\000"
	"Fuel system monitoring completed\000"
	"Misfire monitoring completed\000"
	"EGR system monitoring completed\000"
	"Oxygen sensor heater monitoring completed\000"
	"Oxygen sensor monitoring completed\000"
	"A/C system refrigerant monitoring completed\000"
	"Secondary air system monitoring completed\000"
	"Evaporative system monitoring completed\000"
	"Heated catalyst monitoring completed\000"
	"Catalyst monitoring completed\000"
	"Control module voltage\000"
	"Absolute load value\000"
	"Commanded equivalence ratio\000"
	"Relative throttle position\000"
	"Ambient air temperature\000"
	"Absolute throttle position B\000"
	"Absolute throttle position C\000"
	"Accelerator pedal position D\000"
	"Accelerator pedal position E\000"
	"Accelerator pedal position F\000"
	"Commanded throttle actuator control\000"
	"min\000"
	"Time run by the engine while MIL was activated\000"
	"Engine run time since diagnostic trouble codes cleared\000"
	"Maximum value for equivalence ratio\000"
	"Maximum value for oxygen sensor voltage\000"
	"Maximum value for oxygen sensor current\000"
	"Maximum value for intake manifold absolute pressure (MAP)\000"
	"Maximum value for air flow rate from mass air flow sensor\000"
	"Gasoline/petrol\000"
	"Methanol\000"
	"Ethanol\000"
	"Diesel\000"
	"Liquefied petroleum gas\000"
	"Compressed natural gas\000"
	"Propane\000"
	"Battery/electric\000"
	"Bi-fuel vehicle using gasoline/petrol\000"
	"Bi-fuel vehicle using methanol\000"
	"Bi-fuel vehicle using ethanol\000"
	"Bi-fuel vehicle using LPG\000"
	"Bi-fuel vehicle using CNG\000"
	"Bi-fuel vehicle using propane\000"
	"Bi-fuel vehicle using battery\000"
	"Bi-fuel vehicle using battery and combustion engine for propulsion\000"
	"Hybrid vehicle using gasoline engine for propulsion\000"
	"Hybrid vehicle using gasoline engine on ethanol for propulsion\000"
	"Hybrid vehicle using diesel engine for propulsion\000"
	"Hybrid vehicle using battery for propulsion\000"
	"Hybrid vehicle using battery and combustion engine for propulsion\000"
	"Hybrid vehicle in regeneration mode\000"
	"Bi-fuel vehicle using diesel\000"
	"Bi-fuel vehicle using natural gas\000"
	"Natural gas\000"
	"Dual fuel vehicle using diesel and CNG\000"
	"Dual fuel vehicle using diesel and LNG\000"
	"Fuel cell utilizing hydrogen\000"
	"Hydrogen Internal Combustion Engine\000"
	"Kerosene\000"
	"Heavy Fuel Oil\000"
	"Type of fuel currently being used by the vehicle\000"
	"Alcohol fuel percentage\000"
	"Absolute evap system vapor pressure\000"
	"Evap system vapor pressure (wide)\000"
	"Short term secondary O2 sensor fuel trim (bank 1)\000"
	"Long term secondary O2 sensor fuel trim (bank 1)\000"
	"Short term secondary O2 sensor fuel trim (bank 2)\000"
	"Long term secondary O2 sensor fuel trim (bank 2)\000"
	"Fuel rail pressure (absolute)\000"
	"Accelerator pedal position\302\240(relative)\000"
	"Hybrid battery pack remaining charge\000"
	"Engine oil temperature\000"
	"Fuel injection timing\000"
	"L/h\000"
	"Engine fuel rate\000"
	"Heavy duty vehicles (EURO EEV) C\000"
	"Heavy Duty Vehicles (Euro VI)\000"
	"Emission requirements to which vehicle is designed\000"
	"Driver's demand engine torque\000"
	"Actual engine torque\000"
	"Nm\000"
	"Engine reference torque\000"
	"Engine Percent Torque At Idle, Point 1\000"
	"Engine Percent Torque At Point 2\000"
	"Engine Percent Torque At Point 3\000"
	"Engine Percent Torque At Point 4\000"
	"Engine Percent Torque At Point 5\000"
	"Recommended gear\000"
	"Glow plug lamp status\000"
	"Manual Trans Neutral Gear Status\000"
	"Auto Trans Neutral Drive Status\000"
	"Power Take Off (PTO) Output Status\000"
	"Mass Air Flow Sensor A\000"
	"Mass Air Flow Sensor B\000"
	"Engine coolant temperature 1\000"
	"Engine coolant temperature 2\000"
	"Intake air temperature, bank 1, sensor 1\000"
	"Intake air temperature, bank 1, sensor 2\000"
	"Intake air temperature, bank 1, sensor 3\000"
	"Intake air temperature, bank 2, sensor 1\000"
	"Intake air temperature, bank 2, sensor 2\000"
	"Intake air temperature, bank 2, sensor 3\000"
	"Commanded EGR A duty cycle/position\000"
	"Actual EGR A duty cycle/position\000"
	"EGR A error\000"
	"Commanded EGR B duty cycle/position\000"
	"Actual EGR B duty cycle/position\000"
	"EGR B error\000"
	"Commanded intake air flow A control\000"
	"Relative intake air flow A position\000"
	"Commanded intake air flow B control\000"
	"Relative intake air flow B position\000"
	"Exhaust Gas Recirculation Temp Sensor A (Bank 1, Sensor 1)\000"
	"Exhaust Gas Recirculation Temp Sensor C (Bank 1, Sensor 2)\000"
	"Exhaust Gas Recirculation Temp Sensor B (Bank 2, Sensor 1)\000"
	"Exhaust Gas Recirculation Temp Sensor D (Bank 2, Sensor 2)\000"
	"Commanded Throttle Actuator A Control\000"
	"Relative Throttle A Position\000"
	"Commanded Throttle Actuator B Control\000"
	"Relative Throttle B Position\000"
	"Commanded Fuel Rail Pressure A\000"
	"Fuel Rail Pressure A\000"
	"Fuel Rail Temperature A\000"
	"Commanded Fuel Rail Pressure B\000"
	"Fuel Rail Pressure B\000"
	"Fuel Rail Temperature B\000"
	"Commanded injection control pressure A\000"
	"Injection control pressure A\000"
	"Commanded injection control pressure B\000"
	"Injection control pressure B\000"
	"Commanded boost pressure A\000"
	"Boost pressure sensor A\000"
	"Commanded boost pressure B\000"
	"Boost pressure sensor B\000"
	"Open loop\000"
	"Closed loop\000"
	"Fault\000"
	"Boost pressure B control status\000"
	"Boost pressure A control status\000"
	"Commanded variable geometry turbo A position\000"
	"Variable geometry turbo A position\000"
	"Commanded variable geometry turbo B position\000"
	"Variable geometry turbo B position\000"
	"Open loop, no fault present\000"
	"Closed loop, no fault present\000"
	"Fault present, VGT data unreliable\000"
	"Variable geometry turbo B control status\000"
	"Variable geometry turbo A control status\000"
	"Commanded wastegate A position\000"
	"Wastegate A position\000"
	"Commanded wastegate B position\000"
	"Wastegate B position\000"
	"Exhaust pressure bank 1, sensor 1\000"
	"Exhaust pressure bank 2, sensor 1\000"
	"Turbocharger A speed\000"
	"Turbocharger B speed\000"
	"Turbocharger A compressor inlet temperature\000"
	"Turbocharger A compressor outlet temperature\000"
	"Turbocharger A turbine inlet temperature\000"
	"Turbocharger A turbine outlet temperature\000"
	"Turbocharger B compressor inlet temperature\000"
	"Turbocharger B compressor outlet temperature\000"
	"Turbocharger B turbine inlet temperature\000"
	"Turbocharger B turbine outlet temperature\000"
	"Charge air cooler temperature bank 1, sensor 1\000"
	"Charge air cooler temperature bank 1, sensor 2\000"
	"Charge air cooler temperature bank 2, sensor 1\000"
	"Charge air cooler temperature bank 2, sensor 2\000"
	"Exhaust gas temperature bank 1, sensor 1\000"
	"Exhaust gas temperature bank 1, sensor 2\000"
	"Exhaust gas temperature bank 1, sensor 3\000"
	"Exhaust gas temperature bank 1, sensor 4\000"
	"Exhaust gas temperature bank 2, sensor 1\000"
	"Exhaust gas temperature bank 2, sensor 2\000"
	"Exhaust gas temperature bank 2, sensor 3\000"
	"Exhaust gas temperature bank 2, sensor 4\000"
	"Particulate filter bank 1 delta pressure\000"
	"Particulate filter bank 1 inlet pressure\000"
	"Particulate filter bank 1 outlet pressure\000"
	"Particulate filter bank 2 delta pressure\000"
	"Particulate filter bank 2 inlet pressure\000"
	"Particulate filter bank 2 outlet pressure\000"
	"Particulate filter bank 1 inlet temperature\000"
	"Particulate filter bank 1 outlet temperature\000"
	"Particulate filter bank 2 inlet temperature\000"
	"Particulate filter bank 2 outlet temperature\000"
	"NTE deficiency for PM active area\000"
	"Inside manufacturer-specific PM NTE carve-out area\000"
	"Outside PM control area\000"
	"Inside PM control area\000"
	"Total Engine Run Time\000"
	"Total Idle Run Time\000"
	"Total Run Time With PTO Active\000"
	"Total run time with EI-AECD #1 timer 1 active\000"
	"Total run time with EI-AECD #1 timer 2 active\000"
	"Total run time with EI-AECD #2 timer 1 active\000"
	"Total run time with EI-AECD #2 timer 2 active\000"
	"Total run time with EI-AECD #3 timer 1 active\000"
	"Total run time with EI-AECD #3 timer 2 active\000"
	"Total run time with EI-AECD #4 timer 1 active\000"
	"Total run time with EI-AECD #4 timer 2 active\000"
	"Total run time with EI-AECD #5 timer 1 active\000"
	"Total run time with EI-AECD #5 timer 2 active\000"
	"Total run time with EI-AECD #6 timer 1 active\000"
	"Total run time with EI-AECD #6 timer 2 active\000"
	"Total run time with EI-AECD #7 timer 1 active\000"
	"Total run time with EI-AECD #7 timer 2 active\000"
	"Total run time with EI-AECD #8 timer 1 active\000"
	"Total run time with EI-AECD #8 timer 2 active\000"
	"Total run time with EI-AECD #9 timer 1 active\000"
	"Total run time with EI-AECD #9 timer 2 active\000"
	"Total run time with EI-AECD #10 timer 1 active\000"
	"Total run time with EI-AECD #10 timer 2 active\000"
	"NOx sensor bank 2, sensor 2 has no result\000"
	"NOx sensor bank 2, sensor 1 has no result\000"
	"NOx sensor bank 1, sensor 2 has no result\000"
	"NOx sensor bank 1, sensor 1 has no result\000"
	"ppm\000"
	"NOx concentration bank 1, sensor 1\000"
	"NOx concentration bank 1, sensor 2\000"
	"NOx concentration bank 2, sensor 1\000"
	"NOx concentration bank 2, sensor 2\000"
	"Manifold surface temperature\000"
	"Average reagent consumption\000"
	"Average demanded reagent consumption\000"
	"Reagent tank level\000"
	"Run time with NOx warning active\000"
	"mg/m\302\263\000"
	"PM mass concentration bank 1, sensor 1\000"
	"PM mass concentration bank 2, sensor 1\000"
	"SCR inducement system active\000"
	"SCR inducement for NOx emissions too high\000"
	"SCR inducement for reagent consumption deviation\000"
	"SCR inducement for incorrect reagent\000"
	"SCR inducement for reagent level too low\000"
	"SCR inducement for NOx emissions too high in the 10,000-20,000 km history block\000"
	"SCR inducement for reagent consumption deviation in the 10,000-20,000 km history block\000"
	"SCR inducement for incorrect reagent in the 10,000-20,000 km history block\000"
	"SCR inducement for reagent level too low in the 10,000-20,000 km history block\000"
	"SCR inducement for NOx emissions too high in the 0-10,000 km history block\000"
	"SCR inducement for reagent consumption deviation in the 0-10,000 km history block\000"
	"SCR inducement for incorrect reagent in the 0-10,000 km history block\000"
	"SCR inducement for reagent level too low in the 0-10,000 km history block\000"
	"SCR inducement for NOx emissions too high in the 30,000-40,000 km history block\000"
	"SCR inducement for reagent consumption deviation in the 30,000-40,000 km history block\000"
	"SCR inducement for incorrect reagent in the 30,000-40,000 km history block\000"
	"SCR inducement for reagent level too low in the 30,000-40,000 km history block\000"
	"SCR inducement for NOx emissions too high in the 20,000-30,000 km history block\000"
	"SCR inducement for reagent consumption deviation in the 20,000-30,000 km history block\000"
	"SCR inducement for incorrect reagent in the 20,000-30,000 km history block\000"
	"SCR inducement for reagent level too low in the 20,000-30,000 km history block\000"
	"Distance with SCR inducement active in the current 10,000 km block\000"
	"Distance travelled in the current 10,000 km block\000"
	"Distance with SCR inducement active in the 10,000-20,000 km block\000"
	"Distance with SCR inducement active in the 20,000-30,000 km block\000"
	"Distance with SCR inducement active in the 30,000-40,000 km block\000"
	"Total run time with EI-AECD #11 timer 1 active\000"
	"Total run time with EI-AECD #11 timer 2 active\000"
	"Total run time with EI-AECD #12 timer 1 active\000"
	"Total run time with EI-AECD #12 timer 2 active\000"
	"Total run time with EI-AECD #13 timer 1 active\000"
	"Total run time with EI-AECD #13 timer 2 active\000"
	"Total run time with EI-AECD #14 timer 1 active\000"
	"Total run time with EI-AECD #14 timer 2 active\000"
	"Total run time with EI-AECD #15 timer 1 active\000"
	"Total run time with EI-AECD #15 timer 2 active\000"
	"Total run time with EI-AECD #16 timer 1 active\000"
	"Total run time with EI-AECD #16 timer 2 active\000"
	"Total run time with EI-AECD #17 timer 1 active\000"
	"Total run time with EI-AECD #17 timer 2 active\000"
	"Total run time with EI-AECD #18 timer 1 active\000"
	"Total run time with EI-AECD #18 timer 2 active\000"
	"Total run time with EI-AECD #19 timer 1 active\000"
	"Total run time with EI-AECD #19 timer 2 active\000"
	"Total run time with EI-AECD #20 timer 1 active\000"
	"Total run time with EI-AECD #20 timer 2 active\000"
	"NOx adsorber desulfurization in progress\000"
	"NOx adsorber regen in progress\000"
	"Particulate filter regen in progress\000"
	"Normalized trigger for particulate filter regen\000"
	"Average time between particulate filter regens\000"
	"Average distance between particulate filter regens\000"
	"Passive regen\000"
	"Active regen\000"
	"Particulate filter regen type\000"
	"O2 concentration bank 1, sensor 1\000"
	"O2 concentration bank 1, sensor 2\000"
	"O2 concentration bank 2, sensor 1\000"
	"O2 concentration bank 2, sensor 2\000"
	"O2 sensor lambda bank 1, sensor 1\000"
	"O2 sensor lambda bank 1, sensor 2\000"
	"O2 sensor lambda bank 2, sensor 1\000"
	"O2 sensor lambda bank 2, sensor 2\000"
	"Absolute Throttle Position G\000"
	"Engine Friction - Percent Torque\000"
	"PM sensor bank 1, sensor 1 regenerating\000"
	"PM sensor bank 1, sensor 1 measuring\000"
	"PM sensor normalized output bank 1, sensor 1\000"
	"PM sensor bank 2, sensor 1 regenerating\000"
	"PM sensor bank 2, sensor 1 measuring\000"
	"PM sensor normalized output bank 2, sensor 1\000"
	"h\000"
	"Engine hours with the continuous MI active\000"
	"All vehicle emission system monitors complete\000"
	"Not all vehicle emission system monitors complete\000"
	"Emission system readiness\000"
	"MI activation mode 1, MI off\000"
	"MI activation mode 2, on-demand MI\000"
	"MI activation mode 3, short MI\000"
	"MI activation mode 4, continuous MI\000"
	"Error\000"
	"Not available or not required of this vehicle\000"
	"Vehicle malfunction indicator status\000"
	"All ECUs employ a non-discriminatory MI display strategy\000"
	"All ECUs employ a discriminatory MI display strategy\000"
	"MI display strategy\000"
	"ECU engine hours with the continuous MI active\000"
	"Highest ECU class B1 malfunction counter\000"
	"Not available or not required of this ECU\000"
	"ECU malfunction indicator status\000"
	"Idle fuel balance control 2 in closed loop\000"
	"Fuel injection timing control 2 in closed loop\000"
	"Fuel injection quantity control 2 in closed loop\000"
	"Fuel pressure control 2 in closed loop\000"
	"Idle fuel balance control 1 in closed loop\000"
	"Fuel injection timing control 1 in closed loop\000"
	"Fuel injection quantity control 1 in closed loop\000"
	"Fuel pressure control 1 in closed loop\000"
	"Cumulative engine hours with the continuous MI active\000"
	"NOx warning system active\000"
	"Engine hours with incorrect reagent quality\000"
	"Engine hours with incorrect reagent consumption\000"
	"Engine hours with reagent dosing interrupted\000"
	"Engine hours with an impeded EGR valve\000"
	"Engine hours with a NOx control monitoring system fault\000"
	"Level three inducement inactive\000"
	"Level three inducement enabled\000"
	"Level three inducement active\000"
	"Level three inducement not supported\000"
	"Level three inducement status\000"
	"Level two inducement inactive\000"
	"Level two inducement enabled\000"
	"Level two inducement active\000"
	"Level two inducement not supported\000"
	"Level two inducement status\000"
	"Level one inducement inactive\000"
	"Level one inducement enabled\000"
	"Level one inducement active\000"
	"Level one inducement not supported\000"
	"Level one inducement status\000"
	"g/L\000"
	"Target SCR catalyst A NH3 storage\000"
	"Actual SCR catalyst A NH3 storage\000"
	"Target SCR catalyst B NH3 storage\000"
	"Actual SCR catalyst B NH3 storage\000"
	"Hydrocarbon doser flow rate\000"
	"Hydrocarbon doser injector duty cycle\000"
	"Aftertreatment fuel pressure\000"
	"Engine-out NOx mass emission rate\000"
	"Tailpipe NOx mass emission rate\000"
	"Hybrid/EV Battery System Voltage\000"
	"A\000"
	"Hybrid/EV Battery System Current\000"
	"Charge sustaining mode\000"
	"Charge depleting mode\000"
	"Charge increasing mode\000"
	"Not PSA\000"
	"Enhanced Hybrid/EV Vehicle Charging State\000"
	"Hybrid/EV Vehicle Charging State\000"
	"DEF concentration\000"
	"DEF tank temperature\000"
	"DEF tank level\000"
	"Urea concentration too high\000"
	"Urea concentration too low\000"
	"Fluid is diesel or another fluid\000"
	"DEF is the proper mixture\000"
	"No concentration sensor fault, fluid type cannot be determined\000"
	"Concentration sensor fault, no result\000"
	"DEF type\000"
	"O2 concentration bank 1, sensor 3\000"
	"O2 concentration bank 1, sensor 4\000"
	"O2 concentration bank 2, sensor 3\000"
	"O2 concentration bank 2, sensor 4\000"
	"O2 sensor lambda bank 1, sensor 3\000"
	"O2 sensor lambda bank 1, sensor 4\000"
	"O2 sensor lambda bank 2, sensor 3\000"
	"O2 sensor lambda bank 2, sensor 4\000"
	"Engine fuel rate (alternate)\000"
	"Vehicle fuel rate\000"
	"kg/h\000"
	"Engine exhaust flow rate\000"
	"Fuel system A use percentage bank 1\000"
	"Fuel system B use percentage bank 1\000"
	"Fuel system A use percentage bank 2\000"
	"Fuel system B use percentage bank 2\000"
	"Fuel system A use percentage bank 3\000"
	"Fuel system B use percentage bank 3\000"
	"Fuel system A use percentage bank 4\000"
	"Fuel system B use percentage bank 4\000"
	"Corrected NOx sensor bank 2, sensor 2 has no result\000"
	"Corrected NOx sensor bank 2, sensor 1 has no result\000"
	"Corrected NOx sensor bank 1, sensor 2 has no result\000"
	"Corrected NOx sensor bank 1, sensor 1 has no result\000"
	"Corrected NOx concentration bank 1, sensor 1\000"
	"Corrected NOx concentration bank 1, sensor 2\000"
	"Corrected NOx concentration bank 2, sensor 1\000"
	"Corrected NOx concentration bank 2, sensor 2\000"
	"mg/corsa\000"
	"Cylinder fuel rate\000"
	"Evap system sealed\000"
	"Evap system vapor pressure A\000"
	"Evap system vapor pressure A, wide range\000"
	"Evap system vapor pressure B\000"
	"Evap system vapor pressure B, wide range\000"
	"Transmission actual gear\000"
	"Transmission actual gear ratio\000"
	"Commanded DEF dosing\000"
	"L\000"
	"DEF used in the current driving cycle\000"
	"Odometer\000"
	"NOx sensor bank 2, sensor 4 has no result\000"
	"NOx sensor bank 2, sensor 3 has no result\000"
	"NOx sensor bank 1, sensor 4 has no result\000"
	"NOx sensor bank 1, sensor 3 has no result\000"
	"NOx concentration bank 1, sensor 3\000"
	"NOx concentration bank 1, sensor 4\000"
	"NOx concentration bank 2, sensor 3\000"
	"NOx concentration bank 2, sensor 4\000"
	"Corrected NOx sensor bank 2, sensor 4 has no result\000"
	"Corrected NOx sensor bank 2, sensor 3 has no result\000"
	"Corrected NOx sensor bank 1, sensor 4 has no result\000"
	"Corrected NOx sensor bank 1, sensor 3 has no result\000"
	"Corrected NOx concentration bank 1, sensor 3\000"
	"Corrected NOx concentration bank 1, sensor 4\000"
	"Corrected NOx concentration bank 2, sensor 3\000"
	"Corrected NOx concentration bank 2, sensor 4\000"
	"Maximum current vehicle speed limit\000"
	"Alternative fuel rail pressure\000"
	"Alternative fuel rail temperature\000"
	"Alternative fuel tank pressure\000"
	"Alternative fuel tank pressure, wide range\000"
	"Alternative fuel tank temperature\000"
	"DEF doser A maximum dosing rate\000"
	"Evap system purge pressure\000"
	"Evap system purge pressure, wide range\000"
	"Commanded fresh air flow\000"
	"EGR mass flow rate A\000"
	"EGR mass flow rate B\000"
	"Fuel system bank 2 in open loop due to a malfunction\000"
	"Fuel system bank 2 in closed loop\000"
	"Fuel system bank 2 in open loop\000"
	"Fuel system bank 1 in open loop due to a malfunction\000"
	"Fuel system bank 1 in closed loop\000"
	"Fuel system bank 1 in open loop\000"
	"Short term fuel trim bank 1, compression ignition\000"
	"Long term fuel trim bank 1, compression ignition\000"
	"Short term fuel trim bank 2, compression ignition\000"
	"Long term fuel trim bank 2, compression ignition\000"
	"Traction battery pack State of Health\000"
	"kW\000"
	"HV battery actual charge rate\000"
	"HV battery pack A actual charge rate\000"
	"HV battery pack B actual charge rate\000"
	"HV battery pack C actual charge rate\000"
	"HV battery pack D actual charge rate\000"
	"HV battery pack E actual charge rate\000"
	"HV battery pack F actual charge rate\000"
	"HV battery pack G actual charge rate\000"
	"HV battery pack H actual charge rate\000"
	"HV battery temperature\000"
	"HV battery pack A temperature\000"
	"HV battery pack B temperature\000"
	"HV battery pack C temperature\000"
	"HV battery pack D temperature\000"
	"HV battery pack E temperature\000"
	"HV battery pack F temperature\000"
	"HV battery pack G temperature\000"
	"HV battery pack H temperature\000"
	"HV battery current\000"
	"HV battery pack A current\000"
	"HV battery pack B current\000"
	"HV battery pack C current\000"
	"HV battery pack D current\000"
	"HV battery pack E current\000"
	"HV battery pack F current\000"
	"HV battery pack G current\000"
	"HV battery pack H current\000"
	"HV battery voltage\000"
	"HV battery pack A voltage\000"
	"HV battery pack B voltage\000"
	"HV battery pack C voltage\000"
	"HV battery pack D voltage\000"
	"HV battery pack E voltage\000"
	"HV battery pack F voltage\000"
	"HV battery pack G voltage\000"
	"HV battery pack H voltage\000"
	"HV battery minimum cell temperature\000"
	"HV battery maximum cell temperature\000"
	"HV battery average cell temperature\000"
	"HV battery bus bar temperature\000"
	"HV battery coolant temperature\000"
	"Thermal system off\000"
	"Active cooling\000"
	"Passive cooling\000"
	"Active heating\000"
	"Passive heating\000"
	"HV battery thermal state\000"
	"HV battery active time since cell balancing\000"
	"HV battery minimum cell voltage\000"
	"HV battery maximum cell voltage\000"
	"HV battery continuous rated power available\000"
	"HV battery continuous charge current limit\000"
	"HV battery continuous discharge current limit\000"
	"kWh\000"
	"HV battery cumulative energy in\000"
	"HV battery pack A cumulative energy in\000"
	"HV battery pack B cumulative energy in\000"
	"HV battery pack C cumulative energy in\000"
	"HV battery pack D cumulative energy in\000"
	"HV battery pack E cumulative energy in\000"
	"HV battery pack F cumulative energy in\000"
	"HV battery pack G cumulative energy in\000"
	"HV battery pack H cumulative energy in\000"
	"HV battery cumulative energy out\000"
	"HV battery pack A cumulative energy out\000"
	"HV battery pack B cumulative energy out\000"
	"HV battery pack C cumulative energy out\000"
	"HV battery pack D cumulative energy out\000"
	"HV battery pack E cumulative energy out\000"
	"HV battery pack F cumulative energy out\000"
	"HV battery pack G cumulative energy out\000"
	"HV battery pack H cumulative energy out\000"
	"HV battery total energy throughput\000"
	"HV battery pack A total energy throughput\000"
	"HV battery pack B total energy throughput\000"
	"HV battery pack C total energy throughput\000"
	"HV battery pack D total energy throughput\000"
	"HV battery pack E total energy throughput\000"
	"HV battery pack F total energy throughput\000"
	"HV battery pack G total energy throughput\000"
	"HV battery pack H total energy throughput\000"
	"HV battery state of health\000"
	"HV battery pack A state of health\000"
	"HV battery pack B state of health\000"
	"HV battery pack C state of health\000"
	"HV battery pack D state of health\000"
	"HV battery pack E state of health\000"
	"HV battery pack F state of health\000"
	"HV battery pack G state of health\000"
	"HV battery pack H state of health\000"
	"HV battery recommended minimum state of charge\000"
	"HV battery pack A recommended minimum state of charge\000"
	"HV battery pack B recommended minimum state of charge\000"
	"HV battery pack C recommended minimum state of charge\000"
	"HV battery pack D recommended minimum state of charge\000"
	"HV battery pack E recommended minimum state of charge\000"
	"HV battery pack F recommended minimum state of charge\000"
	"HV battery pack G recommended minimum state of charge\000"
	"HV battery pack H recommended minimum state of charge\000"
	"HV battery recommended maximum state of charge\000"
	"HV battery pack A recommended maximum state of charge\000"
	"HV battery pack B recommended maximum state of charge\000"
	"HV battery pack C recommended maximum state of charge\000"
	"HV battery pack D recommended maximum state of charge\000"
	"HV battery pack E recommended maximum state of charge\000"
	"HV battery pack F recommended maximum state of charge\000"
	"HV battery pack G recommended maximum state of charge\000"
	"HV battery pack H recommended maximum state of charge\000"
	"HV battery discharge energy capacity\000"
	"HV battery pack A discharge energy capacity\000"
	"HV battery pack B discharge energy capacity\000"
	"HV battery pack C discharge energy capacity\000"
	"HV battery pack D discharge energy capacity\000"
	"HV battery pack E discharge energy capacity\000"
	"HV battery pack F discharge energy capacity\000"
	"HV battery pack G discharge energy capacity\000"
	"HV battery pack H discharge energy capacity\000"
	"Fuel level input A\000"
	"Fuel level input B\000"
	"Exhaust particulate control diagnostic time\000"
	"Exhaust particulate control diagnostic count\000"
	"Low-pressure fuel pressure A\000"
	"Low-pressure fuel pressure B\000"
	"Particulate warning system active\000"
	"Engine hours with the particulate filter removed or blocked\000"
	"Engine hours with a liquid reagent injection system failure\000"
	"Engine hours with a particulate control monitoring system fault\000"
	"Particulate level three inducement status\000"
	"Particulate level two inducement status\000"
	"Particulate level one inducement status\000"
	"Distance since module reflash or replacement\000"
	"Warning lamp off\000"
	"Warning lamp continuously on\000"
	"Warning lamp blinking\000"
	"Not utilized\000"
	"Particulate control (PCD) warning lamp\000"
	"NOx control (NCD) warning lamp\000"
	"Shared NCD and PCD warning lamp\000"
	"O2 concentration bank 1, sensor 5\000"
	"O2 concentration bank 1, sensor 6\000"
	"O2 concentration bank 2, sensor 5\000"
	"O2 concentration bank 2, sensor 6\000"
	"O2 sensor lambda bank 1, sensor 5\000"
	"O2 sensor lambda bank 1, sensor 6\000"
	"O2 sensor lambda bank 2, sensor 5\000"
	"O2 sensor lambda bank 2, sensor 6\000"
	"Exhaust pressure bank 1, sensor 2\000"
	"Exhaust pressure bank 2, sensor 2\000"
	"Electric motor A speed\000"
	"Electric motor B speed\000"
	"Electric motor C speed\000"
	"Electric motor D speed\000"
	"Electric motor E speed\000"
	"Electric motor F speed\000"
	"Electric motor G speed\000"
	"Electric motor H speed\000"
	"Electric motor A torque\000"
	"Electric motor B torque\000"
	"Electric motor C torque\000"
	"Electric motor D torque\000"
	"Electric motor E torque\000"
	"Electric motor F torque\000"
	"Electric motor G torque\000"
	"Electric motor H torque\000"
	"Evap system vapor pressure C\000"
	"Evap system vapor pressure C, wide range\000"
	"Evap system vapor pressure D\000"
	"Evap system vapor pressure D, wide range\000"
	"Catalyst temperature A\000"
	"Catalyst temperature B\000"
	"Catalyst temperature C\000"
	"Catalyst temperature D\000"
	"Distance since state of health update\000"
	"HV battery remaining reserve energy\000"
	"HV battery initial reserve energy\000"
	"HV battery charging limit\000"
	"HV battery charging power\000"
	"State of certified energy\000"
	"State of certified range\000"
	"Engine odometer\000"
	"HV battery power over the last second\000"
	"Fuel cell system voltage\000"
	"Fuel cell fuel consumption\000"
	"Fuel cell current over the last second\000"
	"Fuel cell power over the last second\000"
	"Propulsion system active trips since DTCs cleared\000"
	"DEF doser B commanded dosing\000"
	"DEF doser B maximum dosing rate\000"
	"Not ready\000"
	"Ready\000"
	"Battery capacity calculation\000"
	"HV battery calculated usable energy\000"
	"HV battery current over the last second\000"
	"Total run time with EI-AECD #21 timer 1 active\000"
	"Total run time with EI-AECD #21 timer 2 active\000"
	"Total run time with EI-AECD #22 timer 1 active\000"
	"Total run time with EI-AECD #22 timer 2 active\000"
	"Total run time with EI-AECD #23 timer 1 active\000"
	"Total run time with EI-AECD #23 timer 2 active\000"
	"Total run time with EI-AECD #24 timer 1 active\000"
	"Total run time with EI-AECD #24 timer 2 active\000"
	"Total run time with EI-AECD #25 timer 1 active\000"
	"Total run time with EI-AECD #25 timer 2 active\000"
	"Ah\000"
	"Bze_q_lad\000"
	"Bze_q_elad\000"
	"Start/stop battery target voltage\000"
	"12V battery capacity\000"
	"Start/stop battery voltage\000"
	"Start/stop battery current\000"
	"Start/stop battery temperature\000"
	"Start/stop battery sensor temperature\000"
	"Start/stop battery state of charge\000"
	"m\316\251\000"
	"Start/stop battery resistance\000"
	"Start/stop battery charge\000"
	"Start/stop battery voltage, at rest\000"
	"Energy balance of last driving cycle\000"
	"Overall energy balance\000"
	"Entire energy throughput\000"
	"Generator current\000"
	"Range remaining\000"
	"Average consumption, category 0\000"
	"Average consumption, category 1\000"
	"Average consumption, category 2\000"
	"Average consumption, category 3\000"
	"Average consumption, category 4\000"
	"Average consumption, category 5\000"
	"Average consumption, category 6\000"
	"Generator voltage, actual\000"
	"Steering wheel temperature\000"
	"bar\000"
	"Brake pressure\000"
	"rad\000"
	"Steering wheel angle\000"
	"Odometer, dash\000"
	"Outside temperature\000"
	"Charge air pressure\000"
	"Oil thermal wear\000"
	"Odometer, mileage, HR\000"
	"Refrigerant pressure\000"
	"Radiator fan actual duty\000"
	"Radiator fan commanded duty\000"
	"Outside temperature, system calculated\000"
	"Front left deflector temperature\000"
	"Front right deflector temperature\000"
	"Temperature after evaporator\000"
	"Cabin temperature\000"
	"Rear deflector temperature\000"
	"Satellites, visible\000"
	"Satellites, tracked\000"
	"Control module temperature\000"
	"A/c compressor torque, requested\000"
	"Ambient temperature\000"
	"A/C compressor torque\000"
	"mV\000"
	"Accelerator pedal, sensor 1 voltage\000"
	"Cylinder head temperature\000"
	"ms\000"
	"Injector 2 rail pressure calibration point 1\000"
	"Injector 2, rail pressure calibration point 1\000"
	"Lambda, actual value\000"
	"Injector 1 rail pressure calibration point 2\000"
	"Odometer, alternative\000"
	"Coolant fan 1 duty cycle\000"
	"Camshaft adaptation, intake bank 1, phase position\000"
	"Camshaft adaptation, exhaust bank 1, phase position\000"
	"Fuel consumption\000"
	"Maximum discharge current\000"
	"Maximum charge current\000"
	"Odometer, high resolution\000"
	"Intake camshaft signal edge position adapted edge 2\000"
	"Intake camshaft signal edge position adapted edge 3\000"
	"Oil pressure valve switch, control\000"
	"cm\000"
	"Oil level\000"
	"Oil level warning threshold\000"
	"Low fuel pressure, actual\000"
	"TCO_1_SYS\000"
	"Low fuel pressure, commanded\000"
	"Powertrain overall gear ratio\000"
	"Transmission input shaft speed\000"
	"Post-injection 2 start position\000"
	"Accelerator pedal sensor 1\000"
	"Torque loss adaptation, gear engaged\000"
	"Torque loss adaptation at idle\000"
	"Torque increase request from ESP\000"
	"Camshaft end position\000"
	"Gear\000"
	"Post-injection 3 activation duration\000"
	"Pre-injection 2 duration\000"
	"Ambient air temperature, v1\000"
	"Main cat oxygen storage capacity age correction factor\000"
	"Selected gear\000"
	"Lambda output value\000"
	"Engine oil temperature measured value\000"
	"m/s\302\262\000"
	"Lateral acceleration\000"
	"Coolant circulation pump, control\000"
	"Heater circulation pump, nominal value\000"
	"Piston cooling nozzles, control\000"
	"Early ignition counter, cylinder 1\000"
	"Early ignition counter, cylinder 3\000"
	"Early ignition counter, cylinder 4\000"
	"Early ignition counter, cylinder 2\000"
	"Engine oil temperature 2\000"
	"Manual engine starts counter\000"
	"Automatic engine starts counter\000"
	"Oil pressure valve for piston cooling nozzles, control switch button ratio\000"
	"Engine oil level current value\000"
	"Distance traveled while mil was activated\000"
	"Number of warm-ups since dtcs cleared\000"
	"Distance traveled since dtcs cleared\000"
	"Exhaust gas temperature\000"
	"Accelerator pedal position 1\000"
	"Accelerator pedal position 2\000"
	"Total drive time\000"
	"Driving time, eco\000"
	"Driving time, sport\000"
	"Driving time, manual\000"
	"Brake light reading valid\077\000"
	"Brake pressure reading valid\077\000"
	"Brake\000"
	"Brake light\000"
	"Front right wheel speed\000"
	"Front left wheel speed\000"
	"Rear right wheel speed\000"
	"Rear left wheel speed\000"
	"Vehicle speed ESP\000"
	"Converter loss torque\000"
	"Transmission torque losses\000"
	"Current N215 pressure control valve 1\000"
	"Clutch 1 temperature\000"
	"Clutch 2 temperature\000"
	"M009, desired solenoid out 10\000"
	"M009, desired solenoid out 11\000"
	"M009, desired solenoid out 12\000"
	"M009, desired solenoid out 13\000"
	"M009, desired solenoid out 14\000"
	"Time since shift\000"
	"Time since reset\000"
	"Clutch 1 adaptation basic characteristic point 0 position\000"
	"Clutch 1 adaptation basic characteristic point 1 moment\000"
	"Clutch 1 adaptation basic characteristic point 2 position\000"
	"Clutch 1 adaptation basic characteristic point 2 moment\000"
	"Clutch 1 friction value correction factor\000"
	"mm\000"
	"Clutch 1 characteristic curve shift\000"
	"Clutch 1 temperature time in range 0\000"
	"Analysis, clutch 1 temperature time in range 1\000"
	"Analysis, clutch 1 temperature time in range 2\000"
	"Reserve torque\000"
	"Clutch 2 adaptation basic characteristic point 0 position\000"
	"Clutch 2 characteristic curve shift\000"
	"Gear actuator adaptation engaged position G4\000"
	"Clutch 1 adaptation kiss point part 1 odometer reading\000"
	"Clutch 2 successful adaptations counter\000"
	"Clutch 2 adaptation kiss point part 1 odometer reading\000"
	"Clutch 2 failed adaptations counter\000"
	"Clutch 1 lower adaptation point\000"
	"Clutch 2 lower adaptation point\000"
	"Clutch 1 upper adaptation point\000"
	"Clutch 2 upper adaptation point\000"
	"Clutch 0 temperature calculated\000"
	"Pressure sensor 1 temperature\000"
	"Pressure sensor 2 temperature\000"
	"Clutch 1 absolute pressure\000"
	"Clutch 2 absolute pressure\000"
	"Stiffness point pressure clutch 1\000"
	"Stiffness point pressure clutch 2\000"
	"Clutch 0 commanded pressure offset\000"
	"Clutch temperature calculated\000"
	"Engine torque\000"
	"Transmission input speed\000"
	"Transmission output speed\000"
	"Automatic transmission fluid temperature\000"
	"Torque transmitting function, ratio\000"
	"Vehicle speed, HR\000"
	"First gear\000"
	"Second gear\000"
	"Third gear\000"
	"Fourth gear\000"
	"Fifth gear\000"
	"Sixth gear\000"
	"Seventh gear\000"
	"Current gear\000"
	"Specified torque clutch 1 - data\000"
	"Specified torque clutch 2 - data\000"
	"Clutch 1 pressure\000"
	"Clutch 2 pressure\000"
	"Aux. hydraulic pump RPM\000"
	"Accelerator pedal position\000";
static_assert(sizeof(OBDB_TEXT) <= 65535, "Gli offset a 16 bit non indirizzano oltre 64 KB di testi");

static const ObdbUnitDef OBDB_UNITS[] OBD_PROGMEM = {
	{0, 1, 8},
	{0, 0xFFFF, 0xFFFF},
	{0, 76, 80},
	{0, 80, 76},
	{0, 0xFFFF, 0xFFFF},
	{1479, 0xFFFF, 0xFFFF},
	{1504, 0xFFFF, 0xFFFF},
	{1712, 0xFFFF, 0xFFFF},
	{1764, 0xFFFF, 0xFFFF},
	{1779, 0xFFFF, 0xFFFF},
	{1798, 0xFFFF, 0xFFFF},
	{1839, 0xFFFF, 0xFFFF},
	{2308, 0xFFFF, 0xFFFF},
	{4447, 0xFFFF, 0xFFFF},
	{4473, 0xFFFF, 0xFFFF},
	{5609, 0xFFFF, 0xFFFF},
	{6884, 0xFFFF, 0xFFFF},
	{8657, 0xFFFF, 0xFFFF},
	{8843, 0xFFFF, 0xFFFF},
	{13818, 0xFFFF, 0xFFFF},
	{14108, 0xFFFF, 0xFFFF},
	{17800, 0xFFFF, 0xFFFF},
	{19608, 0xFFFF, 0xFFFF},
	{19942, 0xFFFF, 0xFFFF},
	{20725, 0xFFFF, 0xFFFF},
	{21431, 0xFFFF, 0xFFFF},
	{21695, 0xFFFF, 0xFFFF},
	{23288, 0xFFFF, 0xFFFF},
	{24851, 0xFFFF, 0xFFFF},
	{30365, 0xFFFF, 0xFFFF},
	{30602, 0xFFFF, 0xFFFF},
	{31094, 0xFFFF, 0xFFFF},
	{31113, 0xFFFF, 0xFFFF},
	{31628, 0xFFFF, 0xFFFF},
	{31693, 0xFFFF, 0xFFFF},
	{32234, 0xFFFF, 0xFFFF},
	{32805, 0xFFFF, 0xFFFF},
	{34312, 0xFFFF, 0xFFFF}
};

static const ObdbScale OBDB_SCALES[] OBD_PROGMEM = {
	{1.0, 1.0, 0.0},
	{100.0, 255.0, 0.0},
	{1.0, 1.0, -40.0},
	{100.0, 128.0, -100.0},
	{3.0, 1.0, 0.0},
	{1.0, 4.0, 0.0},
	{1.0, 2.0, -64.0},
	{1.0, 100.0, 0.0},
	{1.0, 200.0, 0.0},
	{0.079, 1.0, 0.0},
	{10.0, 1.0, 0.0},
	{1.0, 32768.0, 0.0},
	{1.0, 8192.0, 0.0},
	{1.0, 4000.0, 0.0},
	{1.0, 256.0, -128.0},
	{1.0, 10.0, -40.0},
	{1.0, 1000.0, 0.0},
	{1.0, 128.0, -38665.0},
	{1.0, 20.0, 0.0},
	{1.0, 1.0, -125.0},
	{1.0, 32.0, 0.0},
	{1.0, 80.0, 0.0},
	{100.0, 65535.0, 0.0},
	{1.0, 10000.0, 0.0},
	{1.0, 1200.0, 0.0},
	{1.0, 10.0, 0.0},
	{1.0, 64.0, 0.0},
	{1.0, 50.0, 0.0},
	{1.0, 5.0, 0.0},
	{1.0, 500.0, 0.0},
	{1.0, 2.0, 0.0},
	{1.0, 2000.0, 0.0},
	{1.0, 8.0, 0.0},
	{2.0, 1.0, -256.0},
	{3.0, 10000.0, 0.0},
	{1.0, 20.0, -1600.0},
	{2.0, 5.0, 0.0},
	{1.0, 640.0, 0.0},
	{79.0, 1000.0, 0.0},
	{36.0, 10000000.0, 0.0},
	{72.0, 1000.0, 0.0},
	{36.0, 100000.0, 0.0},
	{36.0, 10000.0, 0.0},
	{1.0, 10.0, 10.6},
	{1.0, 1000.0, 4.0},
	{2.0, 10.0, 0.0},
	{1.0, 10.0, 9.0},
	{21.0, 20.0, 0.0},
	{1.609344, 1.0, 0.0},
	{152999.0, 10000000.0, -42.503},
	{1.0, 2.0, -50.0},
	{1.0, 2.5, 0.0},
	{1.0, 128.0, 0.0},
	{5.0, 50.0, -273.33333333333337},
	{1.0, 20000.0, 0.0},
	{12207.0, 50000000.0, 0.0},
	{360.0, 16384.0, 0.0},
	{1.0, 16.0, -32.0},
	{1.0, 2500.0, 0.0},
	{1.0, 10.0, -273.1},
	{1.0, 256.0, 0.0},
	{0.4, 1000.0, 0.0},
	{0.1, 1.0, -273.33333333333337},
	{2.0, 65535.0, 0.0},
	{199997.0, 6553500000.0, 0.0},
	{20.0, 51.0, 0.0},
	{30.0, 1.0, 0.0},
	{0.09766, 1.0, 0.0},
	{9.8, 10000.0, 0.0},
	{1.0, 10.0, -1.0},
	{32.0, 1.0, 0.0}
};

static const ObdbLimit OBDB_LIMITS[] OBD_PROGMEM = {
	{0.0, 1.0, NAN, NAN},
	{0.0, 127.0, NAN, NAN},
	{0.0, NAN, NAN, NAN},
	{0.0, 65535.0, NAN, NAN},
	{0.0, 100.0, NAN, NAN},
	{-40.0, 215.0, NAN, NAN},
	{-100.0, 99.2, NAN, NAN},
	{0.0, 765.0, NAN, NAN},
	{0.0, 255.0, NAN, NAN},
	{0.0, 16383.75, NAN, 16383.5},
	{0.0, 255.0, NAN, 255.0},
	{-64.0, 63.5, NAN, NAN},
	{0.0, 655.35, NAN, NAN},
	{0.0, 1.275, NAN, NAN},
	{-100.0, 99.22, NAN, 99.2},
	{0.0, 5177.27, NAN, NAN},
	{0.0, 655350.0, NAN, NAN},
	{0.0, 1.99, NAN, NAN},
	{0.0, 7.999, NAN, NAN},
	{-100.0, 99.22, NAN, NAN},
	{0.0, 100.0, 0.0, NAN},
	{-8192.0, 8191.75, NAN, NAN},
	{-128.0, 127.996, NAN, NAN},
	{-40.0, 6513.5, NAN, NAN},
	{0.0, 65.535, NAN, NAN},
	{0.0, 25700.0, NAN, NAN},
	{0.0, 2550.0, NAN, NAN},
	{0.0, 327.675, NAN, NAN},
	{-32767.0, 32768.0, NAN, NAN},
	{-210.0, 301.992, NAN, NAN},
	{0.0, 3276.75, NAN, NAN},
	{-125.0, 130.0, NAN, NAN},
	{0.0, 15.0, NAN, NAN},
	{0.0, 2047.96875, NAN, NAN},
	{-40.0, 215.0, -40.0, NAN},
	{-327.68, 327.67, NAN, NAN},
	{0.0, 4294967295.0, NAN, NAN},
	{0.0, 65535.0, NAN, 65535.0},
	{0.0, 819.1875, NAN, NAN},
	{0.0, 7.9998779297, NAN, NAN},
	{0.0, 6.5535, NAN, NAN},
	{0.0, 54.6125, NAN, NAN},
	{0.0, 6553.5, NAN, NAN},
	{0.0, 6.5535, NAN, 6.5535},
	{0.0, 1023.98, NAN, NAN},
	{-3276.8, 3276.7, NAN, NAN},
	{0.0, 63.75, NAN, NAN},
	{0.0, 1310.7, NAN, NAN},
	{0.0, 13107.0, NAN, NAN},
	{-8.192, 8.19175, NAN, NAN},
	{-65.536, 65.534, NAN, NAN},
	{0.0, 32.7675, NAN, NAN},
	{0.0, 429496729.5, NAN, NAN},
	{0.0, 8191.875, NAN, NAN},
	{-256.0, 254.0, NAN, NAN},
	{0.0, 19.6605, NAN, NAN},
	{-100.0, 99.21875, NAN, NAN},
	{-1600.0, 1676.75, NAN, NAN},
	{0.0, 102.0, NAN, NAN},
	{0.0, 102.3984375, NAN, NAN},
	{0.0, 16777.215, NAN, NAN},
	{0.0, 5177.265, NAN, NAN},
	{-32768.0, 32767.0, NAN, NAN},
	{-7730.9411328, 7730.9411292, NAN, NAN},
	{0.0, 4718.52, NAN, NAN},
	{0.0, 1546188.2262, NAN, NAN},
	{0.0, 19.2765, NAN, NAN},
	{0.0, 4294967.295, NAN, NAN},
	{-7730941.1328, 7730941.1292, NAN, NAN},
	{0.0, 429496729.0, NAN, NAN},
	{0.0, 36.1, NAN, NAN},
	{4.0, 69.53, NAN, NAN},
	{-2000.0, 2000.0, NAN, NAN},
	{0.0, 51.0, NAN, NAN},
	{9.0, 34.5, NAN, NAN},
	{-327.0, 327.0, NAN, NAN},
	{-8808038.0, 8808037.0, NAN, NAN},
	{-838860.0, 838860.0, NAN, NAN},
	{0.0, 6562.5, NAN, NAN},
	{0.0, 105468.35904000001, NAN, NAN},
	{0.0, 6553.0, NAN, NAN},
	{0.0, 150.0, NAN, 150.0},
	{-543.8501, 458.8288, NAN, NAN},
	{-32.768, 32.767, NAN, NAN},
	{-50.0, 77.5, NAN, NAN},
	{0.0, 16777215.0, NAN, NAN},
	{0.0, 4294967.0, NAN, NAN},
	{-3276.8, 100.0, NAN, 100.0},
	{-3276.8, 400.0, NAN, 400.0},
	{-40.0, 150.0, NAN, NAN},
	{-3276.0, 3276.0, NAN, NAN},
	{-255.0, 255.0, NAN, NAN},
	{-273.33333333333337, 315.5555555555556, NAN, 315.5555555555556},
	{-1.6384, 1.63, NAN, NAN},
	{-1.63, 1.63, NAN, NAN},
	{-7.9999, 7.9997, NAN, NAN},
	{-720.0, 719.97, NAN, NAN},
	{-3200.0, 3200.0, NAN, NAN},
	{-32.0, 4063.937, NAN, NAN},
	{0.0, 26.21, NAN, NAN},
	{0.0, 26.2, NAN, NAN},
	{-40.0, 500.0, -40.0, 500.0},
	{0.0, 255.0, 0.0, NAN},
	{-16384.0, 10000.0, NAN, 10000.0},
	{-655.36, 655.34, NAN, NAN},
	{-6553.6, 6553.4, NAN, NAN},
	{-214748364.0, 214748364.0, NAN, NAN},
	{-13.1072, 13.1068, NAN, NAN},
	{-273.33333333333337, 6280.166666666667, -273.33333333333337, NAN},
	{0.0, 2.0, NAN, NAN},
	{-273.0, 6280.0, -273.0, NAN},
	{-273.1, 6280.4, -273.1, NAN},
	{-327.68, 100.0, NAN, NAN},
	{0.0, 7000.0, NAN, NAN},
	{-40.0, 1200.0, NAN, 1200.0},
	{0.0, 65.0, NAN, NAN},
	{0.0, 16383.0, NAN, NAN},
	{0.0, 655.0, NAN, NAN},
	{-1638.4, 1638.35, NAN, NAN},
	{0.0, 7650.0, NAN, NAN},
	{-32.11, 32.111, NAN, NAN},
	{0.0, 655350.0, NAN, 655350.0},
	{-32.76, 32.76, NAN, NAN},
	{-1.0, 24.5, NAN, NAN},
	{0.0, 16383.75, NAN, NAN},
	{-1638.0, 1638.0, NAN, NAN},
	{0.0, 8160.0, NAN, NAN}
};

static const ObdbEnumDef OBDB_ENUMS[] OBD_PROGMEM = {
	{0, 2, 0},
	{2, 10, 0},
	{12, 10, 0},
	{22, 4, 0},
	{26, 58, 0},
	{84, 2, 0},
	{86, 32, 0},
	{118, 4, 0},
	{122, 3, 0},
	{125, 3, 0},
	{128, 3, 0},
	{131, 3, 0},
	{134, 2, 0},
	{136, 2, 0},
	{138, 6, 0},
	{144, 3, 0},
	{147, 6, 0},
	{153, 4, 0},
	{157, 4, 0},
	{161, 4, 0},
	{165, 4, 0},
	{169, 2, 0},
	{171, 6, 0},
	{177, 5, 0},
	{182, 4, 0},
	{186, 4, 0},
	{190, 4, 0},
	{194, 4, 0},
	{198, 4, 0},
	{202, 4, 0},
	{206, 2, 0},
	{208, 7, 0}
};

static const ObdbEnumEntry OBDB_ENUM_ENTRIES[] OBD_PROGMEM = {
	{0, 448},
	{1, 482},
	{0, 615},
	{1, 706},
	{2, 769},
	{4, 835},
	{8, 920},
	{16, 961},
	{32, 1069},
	{64, 1141},
	{68, 1258},
	{128, 1387},
	{0, 615},
	{1, 706},
	{2, 769},
	{4, 835},
	{8, 920},
	{16, 961},
	{32, 1069},
	{64, 1141},
	{68, 1258},
	{128, 1387},
	{1, 1910},
	{2, 1948},
	{4, 1994},
	{8, 2011},
	{1, 2830},
	{2, 2854},
	{3, 2872},
	{4, 2885},
	{5, 2891},
	{6, 2909},
	{7, 2914},
	{8, 2928},
	{9, 2939},
	{10, 2957},
	{11, 2962},
	{12, 2976},
	{13, 2988},
	{14, 3007},
	{15, 3040},
	{16, 3072},
	{17, 3119},
	{18, 3157},
	{19, 3205},
	{20, 3253},
	{21, 3285},
	{23, 3311},
	{24, 3359},
	{25, 3404},
	{26, 3453},
	{27, 3499},
	{28, 3514},
	{29, 3533},
	{30, 3565},
	{31, 3576},
	{32, 3592},
	{33, 3609},
	{34, 3617},
	{35, 3640},
	{36, 3659},
	{37, 3682},
	{38, 3706},
	{39, 3730},
	{40, 3755},
	{41, 3779},
	{42, 3804},
	{43, 3823},
	{44, 3843},
	{45, 3859},
	{46, 3876},
	{47, 3894},
	{48, 3913},
	{49, 3937},
	{50, 3961},
	{51, 3983},
	{52, 4006},
	{53, 4031},
	{54, 4065},
	{55, 4095},
	{56, 4133},
	{57, 4158},
	{58, 4210},
	{59, 4246},
	{0, 448},
	{1, 482},
	{1, 7222},
	{2, 7238},
	{3, 7247},
	{4, 7255},
	{5, 7262},
	{6, 7286},
	{7, 7309},
	{8, 7317},
	{9, 7334},
	{10, 7372},
	{11, 7403},
	{12, 7433},
	{13, 7459},
	{14, 7485},
	{15, 7515},
	{16, 7545},
	{17, 7612},
	{18, 7664},
	{19, 7727},
	{20, 7777},
	{21, 7821},
	{22, 7887},
	{23, 7923},
	{24, 7952},
	{25, 7923},
	{26, 7986},
	{27, 7998},
	{28, 8037},
	{29, 8076},
	{30, 8105},
	{31, 8141},
	{32, 8150},
	{14, 3007},
	{15, 3040},
	{16, 8678},
	{17, 8711},
	{1, 10596},
	{2, 10606},
	{3, 10618},
	{1, 10596},
	{2, 10606},
	{3, 10618},
	{1, 10848},
	{2, 10876},
	{3, 10906},
	{1, 10848},
	{2, 10876},
	{3, 10906},
	{0, 17165},
	{1, 17179},
	{0, 17845},
	{1, 17891},
	{0, 17967},
	{1, 17996},
	{2, 18031},
	{3, 18062},
	{14, 18098},
	{15, 18104},
	{0, 18187},
	{1, 18244},
	{3, 18104},
	{0, 17967},
	{1, 17996},
	{2, 18031},
	{3, 18062},
	{14, 18098},
	{15, 18405},
	{0, 19148},
	{1, 19180},
	{2, 19211},
	{3, 19241},
	{0, 19308},
	{1, 19338},
	{2, 19367},
	{3, 19395},
	{0, 19458},
	{1, 19488},
	{2, 19517},
	{3, 19545},
	{0, 19977},
	{1, 20000},
	{2, 20022},
	{3, 20045},
	{0, 19977},
	{1, 20000},
	{0, 20182},
	{1, 20210},
	{2, 20237},
	{3, 20270},
	{13, 20296},
	{14, 20359},
	{0, 24504},
	{1, 24523},
	{2, 24538},
	{3, 24554},
	{4, 24569},
	{0, 19148},
	{1, 19180},
	{2, 19211},
	{3, 19241},
	{0, 19308},
	{1, 19338},
	{2, 19367},
	{3, 19395},
	{0, 19458},
	{1, 19488},
	{2, 19517},
	{3, 19545},
	{0, 28139},
	{1, 28156},
	{2, 28185},
	{3, 28207},
	{0, 28139},
	{1, 28156},
	{2, 28185},
	{3, 28207},
	{0, 28139},
	{1, 28156},
	{2, 28185},
	{3, 28207},
	{0, 29774},
	{1, 29784},
	{1, 35389},
	{2, 35400},
	{3, 35412},
	{4, 35423},
	{5, 35435},
	{6, 35446},
	{7, 35457}
};

static const ObdbSignalDef OBDB_SIGNALS[] OBD_PROGMEM = {
	{0, 15, 1, 0x10, 0, 0, 0, 255, 255, 0},
	{1, 42, 7, 0x00, 1, 0, 1, 255, 255, 0},
	{9, 83, 1, 0x10, 2, 0, 0, 255, 5, 0},
	{10, 124, 1, 0x10, 2, 0, 0, 255, 6, 0},
	{11, 153, 1, 0x10, 2, 0, 0, 255, 7, 0},
	{13, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{14, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{15, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{16, 65535, 1, 0x44, 3, 0, 0, 255, 255, 0},
	{17, 65535, 1, 0x44, 3, 0, 0, 255, 255, 0},
	{18, 65535, 1, 0x44, 3, 0, 0, 255, 255, 0},
	{19, 65535, 1, 0x44, 3, 0, 0, 255, 255, 0},
	{20, 65535, 1, 0x44, 3, 0, 0, 255, 255, 0},
	{21, 65535, 1, 0x44, 3, 0, 0, 255, 255, 0},
	{22, 65535, 1, 0x44, 3, 0, 0, 255, 255, 0},
	{23, 65535, 1, 0x44, 3, 0, 0, 255, 255, 0},
	{24, 178, 1, 0x14, 2, 0, 0, 255, 8, 0},
	{25, 206, 1, 0x14, 2, 0, 0, 255, 9, 0},
	{26, 244, 1, 0x14, 2, 0, 0, 255, 10, 0},
	{27, 275, 1, 0x14, 2, 0, 0, 255, 11, 0},
	{28, 315, 1, 0x14, 2, 0, 0, 255, 12, 0},
	{29, 353, 1, 0x14, 2, 0, 0, 255, 13, 0},
	{30, 389, 1, 0x14, 2, 0, 0, 255, 14, 0},
	{31, 422, 1, 0x14, 2, 0, 0, 255, 15, 0},
	{12, 522, 1, 0x20, 4, 0, 2, 0, 255, 0},
	{0, 564, 16, 0x30, 4, 0, 3, 255, 255, 0},
	{0, 1437, 8, 0x20, 4, 0, 2, 1, 255, 0},
	{8, 1458, 8, 0x20, 4, 0, 2, 2, 255, 0},
	{0, 1481, 8, 0x00, 5, 1, 4, 255, 255, 0},
	{0, 1508, 8, 0x00, 6, 2, 5, 255, 255, 0},
	{0, 1535, 8, 0x00, 5, 3, 6, 255, 255, 0},
	{0, 1565, 8, 0x00, 5, 3, 6, 255, 255, 0},
	{0, 1594, 8, 0x00, 5, 3, 6, 255, 255, 0},
	{8, 1624, 8, 0x00, 5, 3, 6, 255, 255, 0},
	{0, 1654, 8, 0x00, 5, 3, 6, 255, 255, 0},
	{8, 1683, 8, 0x00, 5, 3, 6, 255, 255, 0},
	{0, 1716, 8, 0x00, 7, 4, 7, 255, 255, 0},
	{0, 1730, 8, 0x00, 7, 0, 8, 255, 255, 0},
	{0, 1768, 16, 0x00, 8, 5, 9, 255, 255, 0},
	{0, 1784, 8, 0x00, 9, 0, 10, 255, 255, 0},
	{0, 1801, 8, 0x00, 10, 6, 11, 255, 255, 0},
	{0, 1816, 8, 0x00, 6, 2, 5, 255, 255, 0},
	{0, 1843, 16, 0x00, 11, 7, 12, 255, 255, 0},
	{0, 1883, 8, 0x00, 5, 1, 4, 255, 255, 0},
	{5, 2045, 3, 0x20, 4, 0, 2, 3, 255, 0},
	{0, 2076, 1, 0x10, 3, 0, 0, 255, 255, 0},
	{1, 2105, 1, 0x10, 3, 0, 0, 255, 255, 0},
	{2, 2134, 1, 0x10, 3, 0, 0, 255, 255, 0},
	{3, 2163, 1, 0x10, 3, 0, 0, 255, 255, 0},
	{4, 2192, 1, 0x10, 3, 0, 0, 255, 255, 0},
	{5, 2221, 1, 0x10, 3, 0, 0, 255, 255, 0},
	{6, 2250, 1, 0x10, 3, 0, 0, 255, 255, 0},
	{7, 2279, 1, 0x10, 3, 0, 0, 255, 255, 0},
	{0, 2310, 8, 0x00, 12, 8, 13, 255, 255, 0},
	{8, 2346, 8, 0x00, 5, 3, 14, 255, 255, 0},
	{0, 2375, 8, 0x00, 12, 8, 13, 255, 255, 0},
	{8, 2411, 8, 0x00, 5, 3, 14, 255, 255, 0},
	{0, 2440, 8, 0x00, 12, 8, 13, 255, 255, 0},
	{8, 2476, 8, 0x00, 5, 3, 14, 255, 255, 0},
	{0, 2505, 8, 0x00, 12, 8, 13, 255, 255, 0},
	{8, 2541, 8, 0x00, 5, 3, 14, 255, 255, 0},
	{0, 2570, 8, 0x00, 12, 8, 13, 255, 255, 0},
	{8, 2606, 8, 0x00, 5, 3, 14, 255, 255, 0},
	{0, 2635, 8, 0x00, 12, 8, 13, 255, 255, 0},
	{8, 2671, 8, 0x00, 5, 3, 14, 255, 255, 0},
	{0, 2700, 8, 0x00, 12, 8, 13, 255, 255, 0},
	{8, 2736, 8, 0x00, 5, 3, 14, 255, 255, 0},
	{0, 2765, 8, 0x00, 12, 8, 13, 255, 255, 0},
	{8, 2801, 8, 0x00, 5, 3, 14, 255, 255, 0},
	{0, 4257, 8, 0x20, 4, 0, 2, 4, 255, 0},
	{0, 4303, 1, 0x10, 3, 0, 0, 255, 255, 0},
	{1, 4332, 1, 0x10, 3, 0, 0, 255, 255, 0},
	{2, 4361, 1, 0x10, 3, 0, 0, 255, 255, 0},
	{3, 4390, 1, 0x10, 3, 0, 0, 255, 255, 0},
	{4, 2134, 1, 0x10, 3, 0, 0, 255, 255, 0},
	{5, 2163, 1, 0x10, 3, 0, 0, 255, 255, 0},
	{6, 2250, 1, 0x10, 3, 0, 0, 255, 255, 0},
	{7, 2279, 1, 0x10, 3, 0, 0, 255, 255, 0},
	{7, 4419, 1, 0x10, 0, 0, 0, 255, 255, 0},
	{0, 4449, 16, 0x00, 13, 0, 3, 255, 255, 0},
	{0, 4476, 16, 0x00, 14, 0, 3, 255, 255, 0},
	{0, 4518, 16, 0x00, 7, 9, 15, 255, 255, 0},
	{0, 4560, 16, 0x00, 7, 10, 16, 255, 255, 0},
	{0, 4579, 16, 0x00, 1, 11, 17, 255, 255, 0},
	{16, 4628, 16, 0x00, 12, 12, 18, 255, 255, 0},
	{0, 4679, 16, 0x00, 1, 11, 17, 255, 255, 0},
	{16, 4728, 16, 0x00, 12, 12, 18, 255, 255, 0},
	{0, 4779, 16, 0x00, 1, 11, 17, 255, 255, 0},
	{16, 4828, 16, 0x00, 12, 12, 18, 255, 255, 0},
	{0, 4879, 16, 0x00, 1, 11, 17, 255, 255, 0},
	{16, 4928, 16, 0x00, 12, 12, 18, 255, 255, 0},
	{0, 4979, 16, 0x00, 1, 11, 17, 255, 255, 0},
	{16, 5028, 16, 0x00, 12, 12, 18, 255, 255, 0},
	{0, 5079, 16, 0x00, 1, 11, 17, 255, 255, 0},
	{16, 5128, 16, 0x00, 12, 12, 18, 255, 255, 0},
	{0, 5179, 16, 0x00, 1, 11, 17, 255, 255, 0},
	{16, 5228, 16, 0x00, 12, 12, 18, 255, 255, 0},
	{0, 5279, 16, 0x00, 1, 11, 17, 255, 255, 0},
	{16, 5328, 16, 0x00, 12, 12, 18, 255, 255, 0},
	{0, 5379, 8, 0x00, 5, 1, 4, 255, 255, 0},
	{0, 5393, 8, 0x00, 5, 3, 19, 255, 255, 0},
	{0, 5403, 8, 0x00, 5, 1, 4, 255, 255, 0},
	{0, 5431, 8, 0x00, 5, 1, 20, 255, 255, 0},
	{0, 5447, 8, 0x00, 1, 0, 8, 255, 255, 0},
	{0, 5505, 16, 0x00, 14, 0, 3, 255, 255, 0},
	{0, 5562, 16, 0x01, 7, 13, 21, 255, 255, 0},
	{0, 5589, 8, 0x00, 7, 0, 8, 255, 255, 0},
	{0, 4579, 16, 0x00, 1, 11, 17, 255, 255, 0},
	{16, 5612, 16, 0x00, 15, 14, 22, 255, 255, 0},
	{0, 4679, 16, 0x00, 1, 11, 17, 255, 255, 0},
	{16, 5663, 16, 0x00, 15, 14, 22, 255, 255, 0},
	{0, 4779, 16, 0x00, 1, 11, 17, 255, 255, 0},
	{16, 5714, 16, 0x00, 15, 14, 22, 255, 255, 0},
	{0, 4879, 16, 0x00, 1, 11, 17, 255, 255, 0},
	{16, 5765, 16, 0x00, 15, 14, 22, 255, 255, 0},
	{0, 4979, 16, 0x00, 1, 11, 17, 255, 255, 0},
	{16, 5816, 16, 0x00, 15, 14, 22, 255, 255, 0},
	{0, 5079, 16, 0x00, 1, 11, 17, 255, 255, 0},
	{16, 5867, 16, 0x00, 15, 14, 22, 255, 255, 0},
	{0, 5179, 16, 0x00, 1, 11, 17, 255, 255, 0},
	{16, 5918, 16, 0x00, 15, 14, 22, 255, 255, 0},
	{0, 5279, 16, 0x00, 1, 11, 17, 255, 255, 0},
	{16, 5969, 16, 0x00, 15, 14, 22, 255, 255, 0},
	{0, 6020, 16, 0x00, 6, 15, 23, 255, 255, 0},
	{0, 6058, 16, 0x00, 6, 15, 23, 255, 255, 0},
	{0, 6096, 16, 0x00, 6, 15, 23, 255, 255, 0},
	{0, 6134, 16, 0x00, 6, 15, 23, 255, 255, 0},
	{9, 6172, 1, 0x10, 2, 0, 0, 255, 3, 0},
	{10, 6217, 1, 0x10, 2, 0, 0, 255, 4, 0},
	{11, 6250, 1, 0x10, 2, 0, 0, 255, 5, 0},
	{13, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{14, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{15, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{16, 65535, 1, 0x44, 3, 0, 0, 255, 255, 0},
	{17, 65535, 1, 0x44, 3, 0, 0, 255, 255, 0},
	{18, 65535, 1, 0x44, 3, 0, 0, 255, 255, 0},
	{19, 65535, 1, 0x44, 3, 0, 0, 255, 255, 0},
	{20, 65535, 1, 0x44, 3, 0, 0, 255, 255, 0},
	{21, 65535, 1, 0x44, 3, 0, 0, 255, 255, 0},
	{22, 65535, 1, 0x44, 3, 0, 0, 255, 255, 0},
	{23, 65535, 1, 0x44, 3, 0, 0, 255, 255, 0},
	{24, 6279, 1, 0x14, 2, 0, 0, 255, 6, 0},
	{25, 6311, 1, 0x14, 2, 0, 0, 255, 7, 0},
	{26, 6353, 1, 0x14, 2, 0, 0, 255, 8, 0},
	{27, 6388, 1, 0x14, 2, 0, 0, 255, 9, 0},
	{28, 6432, 1, 0x14, 2, 0, 0, 255, 10, 0},
	{29, 6474, 1, 0x14, 2, 0, 0, 255, 11, 0},
	{30, 6514, 1, 0x14, 2, 0, 0, 255, 12, 0},
	{31, 6551, 1, 0x14, 2, 0, 0, 255, 13, 0},
	{12, 522, 1, 0x20, 4, 0, 2, 5, 255, 0},
	{0, 6581, 16, 0x00, 12, 16, 24, 255, 255, 0},
	{0, 6604, 16, 0x00, 5, 1, 25, 255, 255, 0},
	{0, 6624, 16, 0x00, 1, 11, 17, 255, 255, 0},
	{0, 6652, 8, 0x00, 5, 1, 4, 255, 255, 0},
	{0, 6679, 8, 0x00, 6, 2, 5, 255, 255, 0},
	{0, 6703, 8, 0x00, 5, 1, 4, 255, 255, 0},
	{0, 6732, 8, 0x00, 5, 1, 4, 255, 255, 0},
	{0, 6761, 8, 0x00, 5, 1, 4, 255, 255, 0},
	{0, 6790, 8, 0x00, 5, 1, 4, 255, 255, 0},
	{0, 6819, 8, 0x00, 5, 1, 4, 255, 255, 0},
	{0, 6848, 8, 0x00, 5, 1, 4, 255, 255, 0},
	{0, 6888, 16, 0x00, 16, 0, 3, 255, 255, 0},
	{0, 6935, 16, 0x00, 16, 0, 3, 255, 255, 0},
	{0, 6990, 8, 0x00, 1, 0, 8, 255, 255, 0},
	{8, 7026, 8, 0x00, 12, 0, 8, 255, 255, 0},
	{16, 7066, 8, 0x00, 15, 0, 8, 255, 255, 0},
	{24, 7106, 8, 0x00, 7, 10, 26, 255, 255, 0},
	{0, 7164, 8, 0x00, 11, 10, 26, 255, 255, 0},
	{0, 8165, 8, 0x20, 4, 0, 2, 6, 255, 0},
	{0, 8214, 8, 0x00, 5, 1, 4, 255, 255, 0},
	{0, 8238, 16, 0x00, 7, 8, 27, 255, 255, 0},
	{0, 8274, 16, 0x01, 7, 16, 28, 255, 255, 0},
	{0, 8308, 8, 0x00, 5, 3, 19, 255, 255, 0},
	{0, 8358, 8, 0x00, 5, 3, 19, 255, 255, 0},
	{0, 8407, 8, 0x00, 5, 3, 19, 255, 255, 0},
	{0, 8457, 8, 0x00, 5, 3, 19, 255, 255, 0},
	{0, 8506, 16, 0x00, 7, 10, 16, 255, 255, 0},
	{0, 8536, 8, 0x00, 5, 1, 4, 255, 255, 0},
	{0, 8575, 8, 0x00, 5, 1, 4, 255, 255, 0},
	{0, 8612, 8, 0x00, 6, 2, 5, 255, 255, 0},
	{0, 8635, 16, 0x00, 10, 17, 29, 255, 255, 0},
	{0, 8661, 16, 0x00, 17, 18, 30, 255, 255, 0},
	{0, 8741, 8, 0x20, 4, 0, 2, 7, 255, 0},
	{0, 8792, 8, 0x00, 5, 19, 31, 255, 255, 0},
	{0, 8822, 8, 0x00, 5, 19, 31, 255, 255, 0},
	{0, 8846, 16, 0x00, 18, 0, 3, 255, 255, 0},
	{0, 8870, 8, 0x00, 5, 19, 31, 255, 255, 0},
	{8, 8909, 8, 0x00, 5, 19, 31, 255, 255, 0},
	{16, 8942, 8, 0x00, 5, 19, 31, 255, 255, 0},
	{24, 8975, 8, 0x00, 5, 19, 31, 255, 255, 0},
	{32, 9008, 8, 0x00, 5, 19, 31, 255, 255, 0},
	{3, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{4, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{5, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{6, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{7, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{8, 9041, 4, 0x10, 3, 0, 32, 255, 0, 0},
	{12, 9058, 1, 0x10, 3, 0, 0, 255, 1, 0},
	{13, 9080, 1, 0x10, 3, 0, 0, 255, 2, 0},
	{14, 9113, 1, 0x10, 3, 0, 0, 255, 3, 0},
	{15, 9145, 1, 0x10, 3, 0, 0, 255, 4, 0},
	{6, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{7, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{8, 9180, 16, 0x00, 11, 20, 33, 255, 1, 0},
	{24, 9203, 16, 0x00, 11, 20, 33, 255, 0, 0},
	{6, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{7, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{8, 9226, 8, 0x00, 6, 2, 5, 255, 1, 0},
	{16, 9255, 8, 0x00, 6, 2, 5, 255, 0, 0},
	{2, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{3, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{4, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{5, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{6, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{7, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{8, 9284, 8, 0x00, 6, 2, 34, 255, 5, 0},
	{16, 9325, 8, 0x00, 6, 2, 34, 255, 4, 0},
	{24, 9366, 8, 0x00, 6, 2, 34, 255, 3, 0},
	{32, 9407, 8, 0x00, 6, 2, 34, 255, 2, 0},
	{40, 9448, 8, 0x00, 6, 2, 34, 255, 1, 0},
	{48, 9489, 8, 0x00, 6, 2, 34, 255, 0, 0},
	{2, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{3, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{4, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{5, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{6, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{7, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{8, 9530, 8, 0x00, 5, 1, 4, 255, 5, 0},
	{16, 9566, 8, 0x00, 5, 1, 4, 255, 4, 0},
	{24, 9599, 8, 0x00, 5, 3, 19, 255, 3, 0},
	{32, 9611, 8, 0x00, 5, 1, 4, 255, 2, 0},
	{40, 9647, 8, 0x00, 5, 1, 4, 255, 1, 0},
	{48, 9680, 8, 0x00, 5, 3, 19, 255, 0, 0},
	{4, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{5, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{6, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{7, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{8, 9692, 8, 0x00, 5, 1, 4, 255, 3, 0},
	{16, 9728, 8, 0x00, 5, 1, 4, 255, 2, 0},
	{24, 9764, 8, 0x00, 5, 1, 4, 255, 1, 0},
	{32, 9800, 8, 0x00, 5, 1, 4, 255, 0, 0},
	{0, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{1, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{2, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{3, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{4, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{5, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{6, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{7, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{8, 9836, 8, 0x00, 6, 2, 5, 255, 7, 0},
	{16, 9895, 8, 0x00, 6, 2, 5, 255, 6, 0},
	{24, 9954, 8, 0x00, 6, 2, 5, 255, 5, 0},
	{32, 10013, 8, 0x00, 6, 2, 5, 255, 4, 0},
	{4, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{5, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{6, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{7, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{8, 10072, 8, 0x00, 5, 1, 4, 255, 3, 0},
	{16, 10110, 8, 0x00, 5, 1, 4, 255, 2, 0},
	{24, 10139, 8, 0x00, 5, 1, 4, 255, 1, 0},
	{32, 10177, 8, 0x00, 5, 1, 4, 255, 0, 0},
	{2, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{3, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{4, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{5, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{6, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{7, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{8, 10206, 16, 0x00, 7, 10, 16, 255, 5, 0},
	{24, 10237, 16, 0x00, 7, 10, 16, 255, 4, 0},
	{40, 10258, 8, 0x00, 6, 2, 5, 255, 3, 0},
	{48, 10282, 16, 0x00, 7, 10, 16, 255, 2, 0},
	{64, 10313, 16, 0x00, 7, 10, 16, 255, 1, 0},
	{80, 10334, 8, 0x00, 6, 2, 5, 255, 0, 0},
	{4, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{5, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{6, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{7, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{8, 10358, 16, 0x00, 7, 10, 16, 255, 3, 0},
	{24, 10397, 16, 0x00, 7, 10, 16, 255, 2, 0},
	{40, 10426, 16, 0x00, 7, 10, 16, 255, 1, 0},
	{56, 10465, 16, 0x00, 7, 10, 16, 255, 0, 0},
	{2, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{3, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{4, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{5, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{6, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{7, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{8, 10494, 16, 0x00, 7, 20, 33, 255, 5, 0},
	{24, 10521, 16, 0x00, 7, 20, 33, 255, 4, 0},
	{40, 10545, 16, 0x00, 7, 20, 33, 255, 2, 0},
	{56, 10572, 16, 0x00, 7, 20, 33, 255, 1, 0},
	{76, 10624, 2, 0x20, 4, 0, 2, 8, 0, 0},
	{78, 10656, 2, 0x20, 4, 0, 2, 9, 3, 0},
	{2, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{3, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{4, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{5, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{6, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{7, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{8, 10688, 8, 0x00, 5, 1, 4, 255, 5, 0},
	{16, 10733, 8, 0x00, 5, 1, 4, 255, 4, 0},
	{24, 10768, 8, 0x00, 5, 1, 4, 255, 2, 0},
	{32, 10813, 8, 0x00, 5, 1, 4, 255, 1, 0},
	{44, 10941, 2, 0x20, 4, 0, 2, 10, 0, 0},
	{46, 10982, 2, 0x20, 4, 0, 2, 11, 3, 0},
	{4, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{5, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{6, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{7, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{8, 11023, 8, 0x00, 5, 1, 4, 255, 3, 0},
	{16, 11054, 8, 0x00, 5, 1, 4, 255, 2, 0},
	{24, 11075, 8, 0x00, 5, 1, 4, 255, 1, 0},
	{32, 11106, 8, 0x00, 5, 1, 4, 255, 0, 0},
	{6, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{7, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{8, 11127, 16, 0x00, 7, 7, 12, 255, 1, 0},
	{24, 11161, 16, 0x00, 7, 7, 12, 255, 0, 0},
	{6, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{7, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{8, 11195, 16, 0x00, 8, 10, 16, 255, 1, 0},
	{24, 11216, 16, 0x00, 8, 10, 16, 255, 0, 0},
	{4, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{5, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{6, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{7, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{8, 11237, 8, 0x00, 6, 2, 5, 255, 3, 0},
	{16, 11281, 8, 0x00, 6, 2, 5, 255, 2, 0},
	{24, 11326, 16, 0x00, 6, 15, 23, 255, 1, 0},
	{40, 11367, 16, 0x00, 6, 15, 23, 255, 0, 0},
	{4, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{5, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{6, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{7, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{8, 11409, 8, 0x00, 6, 2, 5, 255, 3, 0},
	{16, 11453, 8, 0x00, 6, 2, 5, 255, 2, 0},
	{24, 11498, 16, 0x00, 6, 15, 23, 255, 1, 0},
	{40, 11539, 16, 0x00, 6, 15, 23, 255, 0, 0},
	{4, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{5, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{6, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{7, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{8, 11581, 8, 0x00, 6, 2, 5, 255, 3, 0},
	{16, 11628, 8, 0x00, 6, 2, 5, 255, 2, 0},
	{24, 11675, 8, 0x00, 6, 2, 5, 255, 1, 0},
	{32, 11722, 8, 0x00, 6, 2, 5, 255, 0, 0},
	{4, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{5, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{6, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{7, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{8, 11769, 16, 0x00, 6, 15, 23, 255, 3, 0},
	{24, 11810, 16, 0x00, 6, 15, 23, 255, 2, 0},
	{40, 11851, 16, 0x00, 6, 15, 23, 255, 1, 0},
	{56, 11892, 16, 0x00, 6, 15, 23, 255, 0, 0},
	{4, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{5, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{6, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{7, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{8, 11933, 16, 0x00, 6, 15, 23, 255, 3, 0},
	{24, 11974, 16, 0x00, 6, 15, 23, 255, 2, 0},
	{40, 12015, 16, 0x00, 6, 15, 23, 255, 1, 0},
	{56, 12056, 16, 0x00, 6, 15, 23, 255, 0, 0},
	{5, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{6, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{7, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{8, 12097, 16, 0x01, 7, 7, 35, 255, 2, 0},
	{24, 12138, 16, 0x00, 7, 7, 12, 255, 1, 0},
	{40, 12179, 16, 0x00, 7, 7, 12, 255, 0, 0},
	{5, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{6, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{7, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{8, 12221, 16, 0x01, 7, 7, 35, 255, 2, 0},
	{24, 12262, 16, 0x00, 7, 7, 12, 255, 1, 0},
	{40, 12303, 16, 0x00, 7, 7, 12, 255, 0, 0},
	{4, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{5, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{6, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{7, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{8, 12345, 16, 0x00, 6, 15, 23, 255, 3, 0},
	{24, 12389, 16, 0x00, 6, 15, 23, 255, 2, 0},
	{40, 12434, 16, 0x00, 6, 15, 23, 255, 1, 0},
	{56, 12478, 16, 0x00, 6, 15, 23, 255, 0, 0},
	{4, 12523, 1, 0x10, 3, 0, 0, 255, 255, 0},
	{5, 12557, 1, 0x10, 3, 0, 0, 255, 255, 0},
	{6, 12608, 1, 0x10, 3, 0, 0, 255, 255, 0},
	{7, 12632, 1, 0x10, 3, 0, 0, 255, 255, 0},
	{5, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{6, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{7, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{8, 12655, 32, 0x00, 13, 0, 36, 255, 2, 0},
	{40, 12677, 32, 0x00, 13, 0, 36, 255, 1, 0},
	{72, 12697, 32, 0x00, 13, 0, 36, 255, 0, 0},
	{3, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{4, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{5, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{6, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{7, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{8, 12728, 32, 0x00, 13, 0, 36, 255, 4, 0},
	{40, 12774, 32, 0x00, 13, 0, 36, 255, 4, 0},
	{72, 12820, 32, 0x00, 13, 0, 36, 255, 3, 0},
	{104, 12866, 32, 0x00, 13, 0, 36, 255, 3, 0},
	{136, 12912, 32, 0x00, 13, 0, 36, 255, 2, 0},
	{168, 12958, 32, 0x00, 13, 0, 36, 255, 2, 0},
	{200, 13004, 32, 0x00, 13, 0, 36, 255, 1, 0},
	{232, 13050, 32, 0x00, 13, 0, 36, 255, 1, 0},
	{264, 13096, 32, 0x00, 13, 0, 36, 255, 0, 0},
	{296, 13142, 32, 0x00, 13, 0, 36, 255, 0, 0},
	{3, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{4, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{5, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{6, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{7, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{8, 13188, 32, 0x00, 13, 0, 36, 255, 4, 0},
	{40, 13234, 32, 0x00, 13, 0, 36, 255, 4, 0},
	{72, 13280, 32, 0x00, 13, 0, 36, 255, 3, 0},
	{104, 13326, 32, 0x00, 13, 0, 36, 255, 3, 0},
	{136, 13372, 32, 0x00, 13, 0, 36, 255, 2, 0},
	{168, 13418, 32, 0x00, 13, 0, 36, 255, 2, 0},
	{200, 13464, 32, 0x00, 13, 0, 36, 255, 1, 0},
	{232, 13510, 32, 0x00, 13, 0, 36, 255, 1, 0},
	{264, 13556, 32, 0x00, 13, 0, 36, 255, 0, 0},
	{296, 13603, 32, 0x00, 13, 0, 36, 255, 0, 0},
	{0, 13650, 1, 0x10, 3, 0, 0, 255, 4, 0},
	{1, 13692, 1, 0x10, 3, 0, 0, 255, 5, 0},
	{2, 13734, 1, 0x10, 3, 0, 0, 255, 6, 0},
	{3, 13776, 1, 0x10, 3, 0, 0, 255, 7, 0},
	{4, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{5, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{6, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{7, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{8, 13822, 16, 0x00, 19, 0, 37, 255, 7, 0},
	{24, 13857, 16, 0x00, 19, 0, 37, 255, 6, 0},
	{40, 13892, 16, 0x00, 19, 0, 37, 255, 5, 0},
	{56, 13927, 16, 0x00, 19, 0, 37, 255, 4, 0},
	{0, 13962, 8, 0x00, 6, 2, 5, 255, 255, 0},
	{4, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{5, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{6, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{7, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{8, 13991, 16, 0x00, 17, 8, 27, 255, 3, 0},
	{24, 14019, 16, 0x00, 17, 8, 27, 255, 2, 0},
	{40, 14056, 8, 0x00, 5, 1, 4, 255, 1, 0},
	{48, 14075, 32, 0x00, 13, 0, 36, 255, 0, 0},
	{6, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{7, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{8, 14115, 16, 0x00, 20, 21, 38, 255, 1, 0},
	{24, 14154, 16, 0x00, 20, 21, 38, 255, 0, 0},
	{0, 14193, 1, 0x10, 3, 0, 0, 255, 255, 0},
	{4, 14222, 1, 0x10, 3, 0, 0, 255, 255, 0},
	{5, 14264, 1, 0x10, 3, 0, 0, 255, 255, 0},
	{6, 14313, 1, 0x10, 3, 0, 0, 255, 255, 0},
	{7, 14350, 1, 0x10, 3, 0, 0, 255, 255, 0},
	{8, 14391, 1, 0x10, 3, 0, 0, 255, 255, 0},
	{9, 14471, 1, 0x10, 3, 0, 0, 255, 255, 0},
	{10, 14558, 1, 0x10, 3, 0, 0, 255, 255, 0},
	{11, 14633, 1, 0x10, 3, 0, 0, 255, 255, 0},
	{12, 14712, 1, 0x10, 3, 0, 0, 255, 255, 0},
	{13, 14787, 1, 0x10, 3, 0, 0, 255, 255, 0},
	{14, 14869, 1, 0x10, 3, 0, 0, 255, 255, 0},
	{15, 14939, 1, 0x10, 3, 0, 0, 255, 255, 0},
	{16, 15013, 1, 0x10, 3, 0, 0, 255, 255, 0},
	{17, 15093, 1, 0x10, 3, 0, 0, 255, 255, 0},
	{18, 15180, 1, 0x10, 3, 0, 0, 255, 255, 0},
	{19, 15255, 1, 0x10, 3, 0, 0, 255, 255, 0},
	{20, 15334, 1, 0x10, 3, 0, 0, 255, 255, 0},
	{21, 15414, 1, 0x10, 3, 0, 0, 255, 255, 0},
	{22, 15501, 1, 0x10, 3, 0, 0, 255, 255, 0},
	{23, 15576, 1, 0x10, 3, 0, 0, 255, 255, 0},
	{24, 15655, 16, 0x00, 14, 0, 3, 255, 255, 0},
	{40, 15722, 16, 0x00, 14, 0, 3, 255, 255, 0},
	{56, 15772, 16, 0x00, 14, 0, 3, 255, 255, 0},
	{72, 15838, 16, 0x00, 14, 0, 3, 255, 255, 0},
	{88, 15904, 16, 0x00, 14, 0, 3, 255, 255, 0},
	{3, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{4, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{5, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{6, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{7, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{8, 15970, 32, 0x00, 13, 0, 36, 255, 4, 0},
	{40, 16017, 32, 0x00, 13, 0, 36, 255, 4, 0},
	{72, 16064, 32, 0x00, 13, 0, 36, 255, 3, 0},
	{104, 16111, 32, 0x00, 13, 0, 36, 255, 3, 0},
	{136, 16158, 32, 0x00, 13, 0, 36, 255, 2, 0},
	{168, 16205, 32, 0x00, 13, 0, 36, 255, 2, 0},
	{200, 16252, 32, 0x00, 13, 0, 36, 255, 1, 0},
	{232, 16299, 32, 0x00, 13, 0, 36, 255, 1, 0},
	{264, 16346, 32, 0x00, 13, 0, 36, 255, 0, 0},
	{296, 16393, 32, 0x00, 13, 0, 36, 255, 0, 0},
	{3, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{4, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{5, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{6, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{7, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{8, 16440, 32, 0x00, 13, 0, 36, 255, 4, 0},
	{40, 16487, 32, 0x00, 13, 0, 36, 255, 4, 0},
	{72, 16534, 32, 0x00, 13, 0, 36, 255, 3, 0},
	{104, 16581, 32, 0x00, 13, 0, 36, 255, 3, 0},
	{136, 16628, 32, 0x00, 13, 0, 36, 255, 2, 0},
	{168, 16675, 32, 0x00, 13, 0, 36, 255, 2, 0},
	{200, 16722, 32, 0x00, 13, 0, 36, 255, 1, 0},
	{232, 16769, 32, 0x00, 13, 0, 36, 255, 1, 0},
	{264, 16816, 32, 0x00, 13, 0, 36, 255, 0, 0},
	{296, 16863, 32, 0x00, 13, 0, 36, 255, 0, 0},
	{1, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{2, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{3, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{4, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{5, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{6, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{7, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{12, 16910, 1, 0x10, 3, 0, 0, 255, 3, 0},
	{13, 16951, 1, 0x10, 3, 0, 0, 255, 4, 0},
	{15, 16982, 1, 0x10, 3, 0, 0, 255, 6, 0},
	{16, 17019, 8, 0x00, 5, 1, 4, 255, 2, 0},
	{24, 17067, 16, 0x00, 16, 0, 3, 255, 1, 0},
	{40, 17114, 16, 0x00, 14, 0, 3, 255, 0, 0},
	{14, 17192, 1, 0x20, 4, 0, 2, 12, 5, 0},
	{0, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{1, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{2, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{3, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{4, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{5, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{6, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{7, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{8, 17222, 16, 0x00, 5, 22, 4, 255, 7, 0},
	{24, 17256, 16, 0x00, 5, 22, 4, 255, 6, 0},
	{40, 17290, 16, 0x00, 5, 22, 4, 255, 5, 0},
	{56, 17324, 16, 0x00, 5, 22, 4, 255, 4, 0},
	{72, 17358, 16, 0x00, 1, 12, 39, 255, 3, 0},
	{88, 17392, 16, 0x00, 1, 12, 39, 255, 2, 0},
	{104, 17426, 16, 0x00, 1, 12, 39, 255, 1, 0},
	{120, 17460, 16, 0x00, 1, 12, 39, 255, 0, 0},
	{0, 17494, 8, 0x00, 5, 1, 4, 255, 255, 0},
	{0, 17523, 8, 0x00, 5, 19, 31, 255, 255, 0},
	{4, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{5, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{6, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{7, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{14, 17556, 1, 0x10, 3, 0, 0, 255, 3, 0},
	{15, 17596, 1, 0x10, 3, 0, 0, 255, 3, 0},
	{16, 17633, 16, 0x01, 5, 7, 35, 255, 2, 0},
	{38, 17678, 1, 0x10, 3, 0, 0, 255, 1, 0},
	{39, 17718, 1, 0x10, 3, 0, 0, 255, 1, 0},
	{40, 17755, 16, 0x01, 5, 7, 35, 255, 0, 0},
	{8, 17802, 16, 0x00, 21, 0, 3, 255, 255, 0},
	{1, 17941, 1, 0x20, 4, 0, 2, 13, 255, 0},
	{2, 18150, 4, 0x20, 4, 0, 2, 14, 255, 0},
	{6, 18297, 2, 0x20, 4, 0, 2, 15, 255, 0},
	{8, 18317, 16, 0x00, 21, 0, 3, 255, 255, 0},
	{24, 18364, 16, 0x00, 21, 0, 3, 255, 255, 0},
	{4, 18447, 4, 0x20, 4, 0, 2, 16, 255, 0},
	{0, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{1, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{2, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{3, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{4, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{5, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{6, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{7, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{8, 18480, 1, 0x10, 3, 0, 0, 255, 0, 0},
	{9, 18523, 1, 0x10, 3, 0, 0, 255, 1, 0},
	{10, 18570, 1, 0x10, 3, 0, 0, 255, 2, 0},
	{11, 18619, 1, 0x10, 3, 0, 0, 255, 3, 0},
	{12, 18658, 1, 0x10, 3, 0, 0, 255, 4, 0},
	{13, 18701, 1, 0x10, 3, 0, 0, 255, 5, 0},
	{14, 18748, 1, 0x10, 3, 0, 0, 255, 6, 0},
	{15, 18797, 1, 0x10, 3, 0, 0, 255, 7, 0},
	{7, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{8, 18836, 16, 0x00, 21, 0, 3, 255, 0, 0},
	{2, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{3, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{4, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{5, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{6, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{7, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{15, 18890, 1, 0x10, 3, 0, 0, 255, 5, 0},
	{16, 18916, 16, 0x00, 21, 0, 37, 255, 4, 0},
	{32, 18960, 16, 0x00, 21, 0, 37, 255, 3, 0},
	{48, 19008, 16, 0x00, 21, 0, 37, 255, 2, 0},
	{64, 19053, 16, 0x00, 21, 0, 37, 255, 1, 0},
	{80, 19092, 16, 0x00, 21, 0, 37, 255, 0, 0},
	{9, 19278, 2, 0x20, 4, 0, 2, 17, 255, 0},
	{11, 19430, 2, 0x20, 4, 0, 2, 18, 255, 0},
	{13, 19580, 2, 0x20, 4, 0, 2, 19, 255, 0},
	{4, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{5, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{6, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{7, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{8, 19612, 16, 0x00, 22, 23, 40, 255, 3, 0},
	{24, 19646, 16, 0x00, 22, 23, 40, 255, 2, 0},
	{40, 19680, 16, 0x00, 22, 23, 40, 255, 1, 0},
	{56, 19714, 16, 0x00, 22, 23, 40, 255, 0, 0},
	{5, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{6, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{7, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{8, 19748, 16, 0x00, 11, 24, 41, 255, 2, 0},
	{24, 19776, 8, 0x00, 5, 1, 4, 255, 1, 0},
	{32, 19814, 16, 0x00, 7, 25, 42, 255, 0, 0},
	{6, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{7, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{8, 19843, 16, 0x00, 11, 23, 43, 255, 1, 0},
	{24, 19877, 16, 0x00, 11, 23, 43, 255, 0, 0},
	{4, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{5, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{6, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{7, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{16, 19909, 16, 0x00, 12, 26, 44, 255, 2, 0},
	{32, 19944, 16, 0x01, 23, 25, 45, 255, 1, 0},
	{13, 20053, 2, 0x20, 4, 0, 2, 20, 0, 0},
	{15, 20095, 1, 0x20, 4, 0, 2, 21, 3, 0},
	{4, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{5, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{6, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{7, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{8, 20128, 8, 0x00, 5, 5, 46, 255, 2, 0},
	{16, 20146, 8, 0x00, 6, 2, 5, 255, 1, 0},
	{24, 20167, 8, 0x00, 5, 1, 4, 255, 0, 0},
	{0, 20397, 4, 0x20, 4, 0, 2, 22, 3, 0},
	{0, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{1, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{2, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{3, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{4, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{5, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{6, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{7, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{8, 20406, 16, 0x00, 5, 22, 4, 255, 7, 0},
	{24, 20440, 16, 0x00, 5, 22, 4, 255, 6, 0},
	{40, 20474, 16, 0x00, 5, 22, 4, 255, 5, 0},
	{56, 20508, 16, 0x00, 5, 22, 4, 255, 4, 0},
	{72, 20542, 16, 0x00, 1, 12, 39, 255, 3, 0},
	{88, 20576, 16, 0x00, 1, 12, 39, 255, 2, 0},
	{104, 20610, 16, 0x00, 1, 12, 39, 255, 1, 0},
	{120, 20644, 16, 0x00, 1, 12, 39, 255, 0, 0},
	{0, 20678, 16, 0x00, 11, 27, 47, 255, 255, 0},
	{16, 20707, 16, 0x00, 11, 27, 47, 255, 255, 0},
	{0, 20730, 16, 0x00, 24, 28, 48, 255, 255, 0},
	{0, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{1, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{2, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{3, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{4, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{5, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{6, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{7, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{8, 20755, 8, 0x00, 5, 1, 4, 255, 7, 0},
	{16, 20791, 8, 0x00, 5, 1, 4, 255, 6, 0},
	{24, 20827, 8, 0x00, 5, 1, 4, 255, 5, 0},
	{32, 20863, 8, 0x00, 5, 1, 4, 255, 4, 0},
	{40, 20899, 8, 0x00, 5, 1, 4, 255, 3, 0},
	{48, 20935, 8, 0x00, 5, 1, 4, 255, 2, 0},
	{56, 20971, 8, 0x00, 5, 1, 4, 255, 1, 0},
	{64, 21007, 8, 0x00, 5, 1, 4, 255, 0, 0},
	{0, 21043, 1, 0x10, 3, 0, 0, 255, 4, 0},
	{1, 21095, 1, 0x10, 3, 0, 0, 255, 5, 0},
	{2, 21147, 1, 0x10, 3, 0, 0, 255, 6, 0},
	{3, 21199, 1, 0x10, 3, 0, 0, 255, 7, 0},
	{4, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{5, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{6, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{7, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{8, 21251, 16, 0x00, 19, 0, 37, 255, 7, 0},
	{24, 21296, 16, 0x00, 19, 0, 37, 255, 6, 0},
	{40, 21341, 16, 0x00, 19, 0, 37, 255, 5, 0},
	{56, 21386, 16, 0x00, 19, 0, 37, 255, 4, 0},
	{0, 21440, 16, 0x00, 25, 20, 33, 255, 255, 0},
	{2, 21459, 1, 0x10, 3, 0, 0, 255, 1, 0},
	{3, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{4, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{5, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{6, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{7, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{8, 21478, 16, 0x01, 7, 13, 49, 255, 5, 0},
	{24, 21507, 16, 0x01, 7, 29, 50, 255, 4, 0},
	{40, 21548, 16, 0x01, 7, 13, 49, 255, 3, 0},
	{56, 21577, 16, 0x01, 7, 29, 50, 255, 2, 0},
	{6, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{7, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{8, 21618, 4, 0x00, 1, 0, 32, 255, 1, 0},
	{16, 21643, 16, 0x00, 1, 16, 24, 255, 0, 0},
	{6, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{7, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{8, 21674, 8, 0x00, 5, 30, 4, 255, 1, 0},
	{16, 21697, 16, 0x00, 26, 31, 51, 255, 0, 0},
	{0, 21735, 32, 0x00, 14, 25, 52, 255, 255, 0},
	{0, 21744, 1, 0x10, 3, 0, 0, 255, 4, 0},
	{1, 21786, 1, 0x10, 3, 0, 0, 255, 5, 0},
	{2, 21828, 1, 0x10, 3, 0, 0, 255, 6, 0},
	{3, 21870, 1, 0x10, 3, 0, 0, 255, 7, 0},
	{4, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{5, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{6, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{7, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{8, 21912, 16, 0x00, 19, 0, 37, 255, 7, 0},
	{24, 21947, 16, 0x00, 19, 0, 37, 255, 6, 0},
	{40, 21982, 16, 0x00, 19, 0, 37, 255, 5, 0},
	{56, 22017, 16, 0x00, 19, 0, 37, 255, 4, 0},
	{0, 22052, 1, 0x10, 3, 0, 0, 255, 4, 0},
	{1, 22104, 1, 0x10, 3, 0, 0, 255, 5, 0},
	{2, 22156, 1, 0x10, 3, 0, 0, 255, 6, 0},
	{3, 22208, 1, 0x10, 3, 0, 0, 255, 7, 0},
	{4, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{5, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{6, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{7, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{8, 22260, 16, 0x00, 19, 0, 37, 255, 7, 0},
	{24, 22305, 16, 0x00, 19, 0, 37, 255, 6, 0},
	{40, 22350, 16, 0x00, 19, 0, 37, 255, 5, 0},
	{56, 22395, 16, 0x00, 19, 0, 37, 255, 4, 0},
	{0, 22440, 8, 0x00, 9, 0, 8, 255, 255, 0},
	{3, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{4, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{5, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{6, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{7, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{8, 22476, 16, 0x00, 7, 20, 33, 255, 4, 0},
	{24, 22507, 8, 0x00, 6, 2, 5, 255, 3, 0},
	{32, 22541, 16, 0x00, 7, 32, 53, 255, 2, 0},
	{48, 22572, 16, 0x00, 7, 0, 3, 255, 1, 0},
	{64, 22615, 8, 0x00, 6, 33, 54, 255, 0, 0},
	{6, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{7, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{16, 22649, 16, 0x00, 24, 34, 55, 255, 0, 0},
	{6, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{7, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{8, 22681, 16, 0x01, 7, 13, 49, 255, 1, 0},
	{24, 22708, 16, 0x01, 7, 29, 50, 255, 0, 0},
	{0, 22747, 16, 0x00, 24, 18, 30, 255, 255, 0},
	{6, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{7, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{8, 22772, 16, 0x00, 24, 18, 30, 255, 1, 0},
	{24, 22793, 16, 0x00, 24, 18, 30, 255, 0, 0},
	{2, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{3, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{4, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{5, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{6, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{7, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{9, 22814, 1, 0x10, 3, 0, 0, 255, 4, 0},
	{10, 22867, 1, 0x10, 3, 0, 0, 255, 4, 0},
	{11, 22901, 1, 0x10, 3, 0, 0, 255, 4, 0},
	{13, 22933, 1, 0x10, 3, 0, 0, 255, 5, 0},
	{14, 22986, 1, 0x10, 3, 0, 0, 255, 5, 0},
	{15, 23020, 1, 0x10, 3, 0, 0, 255, 5, 0},
	{16, 23052, 8, 0x00, 5, 3, 56, 255, 3, 0},
	{24, 23102, 8, 0x00, 5, 3, 56, 255, 2, 0},
	{32, 23151, 8, 0x00, 5, 3, 56, 255, 1, 0},
	{40, 23201, 8, 0x00, 5, 3, 56, 255, 0, 0},
	{0, 23250, 8, 0x00, 5, 1, 4, 255, 255, 0},
	{0, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{1, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{2, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{3, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{4, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{5, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{6, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{7, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{8, 23291, 16, 0x00, 27, 18, 30, 255, 255, 0},
	{24, 23321, 16, 0x00, 27, 18, 30, 255, 7, 0},
	{40, 23358, 16, 0x00, 27, 18, 30, 255, 6, 0},
	{56, 23395, 16, 0x00, 27, 18, 30, 255, 5, 0},
	{72, 23432, 16, 0x00, 27, 18, 30, 255, 4, 0},
	{88, 23469, 16, 0x00, 27, 18, 30, 255, 3, 0},
	{104, 23506, 16, 0x00, 27, 18, 30, 255, 2, 0},
	{120, 23543, 16, 0x00, 27, 18, 30, 255, 1, 0},
	{136, 23580, 16, 0x00, 27, 18, 30, 255, 0, 0},
	{0, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{1, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{2, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{3, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{4, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{5, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{6, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{7, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{8, 23617, 8, 0x00, 6, 2, 5, 255, 255, 0},
	{16, 23640, 8, 0x00, 6, 2, 5, 255, 7, 0},
	{24, 23670, 8, 0x00, 6, 2, 5, 255, 6, 0},
	{32, 23700, 8, 0x00, 6, 2, 5, 255, 5, 0},
	{40, 23730, 8, 0x00, 6, 2, 5, 255, 4, 0},
	{48, 23760, 8, 0x00, 6, 2, 5, 255, 3, 0},
	{56, 23790, 8, 0x00, 6, 2, 5, 255, 2, 0},
	{64, 23820, 8, 0x00, 6, 2, 5, 255, 1, 0},
	{72, 23850, 8, 0x00, 6, 2, 5, 255, 0, 0},
	{0, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{1, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{2, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{3, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{4, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{5, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{6, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{7, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{8, 23880, 16, 0x00, 23, 35, 57, 255, 255, 0},
	{24, 23899, 16, 0x00, 23, 35, 57, 255, 7, 0},
	{40, 23925, 16, 0x00, 23, 35, 57, 255, 6, 0},
	{56, 23951, 16, 0x00, 23, 35, 57, 255, 5, 0},
	{72, 23977, 16, 0x00, 23, 35, 57, 255, 4, 0},
	{88, 24003, 16, 0x00, 23, 35, 57, 255, 3, 0},
	{104, 24029, 16, 0x00, 23, 35, 57, 255, 2, 0},
	{120, 24055, 16, 0x00, 23, 35, 57, 255, 1, 0},
	{136, 24081, 16, 0x00, 23, 35, 57, 255, 0, 0},
	{0, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{1, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{2, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{3, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{4, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{5, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{6, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{7, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{8, 24107, 16, 0x00, 12, 18, 30, 255, 255, 0},
	{24, 24126, 16, 0x00, 12, 18, 30, 255, 7, 0},
	{40, 24152, 16, 0x00, 12, 18, 30, 255, 6, 0},
	{56, 24178, 16, 0x00, 12, 18, 30, 255, 5, 0},
	{72, 24204, 16, 0x00, 12, 18, 30, 255, 4, 0},
	{88, 24230, 16, 0x00, 12, 18, 30, 255, 3, 0},
	{104, 24256, 16, 0x00, 12, 18, 30, 255, 2, 0},
	{120, 24282, 16, 0x00, 12, 18, 30, 255, 1, 0},
	{136, 24308, 16, 0x00, 12, 18, 30, 255, 0, 0},
	{2, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{3, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{4, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{5, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{6, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{7, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{8, 24334, 8, 0x00, 6, 2, 5, 255, 5, 0},
	{16, 24370, 8, 0x00, 6, 2, 5, 255, 4, 0},
	{24, 24406, 8, 0x00, 6, 2, 5, 255, 3, 0},
	{32, 24442, 8, 0x00, 6, 2, 5, 255, 2, 0},
	{40, 24473, 8, 0x00, 6, 2, 5, 255, 1, 0},
	{48, 24585, 8, 0x20, 4, 0, 2, 23, 0, 0},
	{0, 24610, 16, 0x00, 16, 0, 3, 255, 255, 0},
	{0, 24654, 16, 0x00, 12, 16, 24, 255, 255, 0},
	{16, 24686, 16, 0x00, 12, 16, 24, 255, 255, 0},
	{0, 24718, 8, 0x00, 5, 1, 4, 255, 255, 0},
	{8, 24762, 16, 0x01, 23, 25, 45, 255, 255, 0},
	{24, 24805, 16, 0x01, 23, 25, 45, 255, 255, 0},
	{0, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{1, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{2, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{3, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{4, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{5, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{6, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{7, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{8, 24855, 32, 0x00, 28, 0, 36, 255, 255, 0},
	{40, 24887, 32, 0x00, 28, 0, 36, 255, 7, 0},
	{72, 24926, 32, 0x00, 28, 0, 36, 255, 6, 0},
	{104, 24965, 32, 0x00, 28, 0, 36, 255, 5, 0},
	{136, 25004, 32, 0x00, 28, 0, 36, 255, 4, 0},
	{168, 25043, 32, 0x00, 28, 0, 36, 255, 3, 0},
	{200, 25082, 32, 0x00, 28, 0, 36, 255, 2, 0},
	{232, 25121, 32, 0x00, 28, 0, 36, 255, 1, 0},
	{264, 25160, 32, 0x00, 28, 0, 36, 255, 0, 0},
	{0, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{1, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{2, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{3, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{4, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{5, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{6, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{7, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{8, 25199, 32, 0x00, 28, 0, 36, 255, 255, 0},
	{40, 25232, 32, 0x00, 28, 0, 36, 255, 7, 0},
	{72, 25272, 32, 0x00, 28, 0, 36, 255, 6, 0},
	{104, 25312, 32, 0x00, 28, 0, 36, 255, 5, 0},
	{136, 25352, 32, 0x00, 28, 0, 36, 255, 4, 0},
	{168, 25392, 32, 0x00, 28, 0, 36, 255, 3, 0},
	{200, 25432, 32, 0x00, 28, 0, 36, 255, 2, 0},
	{232, 25472, 32, 0x00, 28, 0, 36, 255, 1, 0},
	{264, 25512, 32, 0x00, 28, 0, 36, 255, 0, 0},
	{0, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{1, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{2, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{3, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{4, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{5, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{6, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{7, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{8, 25552, 32, 0x00, 28, 25, 52, 255, 255, 0},
	{40, 25587, 32, 0x00, 28, 25, 52, 255, 7, 0},
	{72, 25629, 32, 0x00, 28, 25, 52, 255, 6, 0},
	{104, 25671, 32, 0x00, 28, 25, 52, 255, 5, 0},
	{136, 25713, 32, 0x00, 28, 25, 52, 255, 4, 0},
	{168, 25755, 32, 0x00, 28, 25, 52, 255, 3, 0},
	{200, 25797, 32, 0x00, 28, 25, 52, 255, 2, 0},
	{232, 25839, 32, 0x00, 28, 25, 52, 255, 1, 0},
	{264, 25881, 32, 0x00, 28, 25, 52, 255, 0, 0},
	{0, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{1, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{2, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{3, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{4, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{5, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{6, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{7, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{8, 25923, 8, 0x00, 5, 36, 58, 255, 255, 0},
	{16, 25950, 8, 0x00, 5, 36, 58, 255, 7, 0},
	{24, 25984, 8, 0x00, 5, 36, 58, 255, 6, 0},
	{32, 26018, 8, 0x00, 5, 36, 58, 255, 5, 0},
	{40, 26052, 8, 0x00, 5, 36, 58, 255, 4, 0},
	{48, 26086, 8, 0x00, 5, 36, 58, 255, 3, 0},
	{56, 26120, 8, 0x00, 5, 36, 58, 255, 2, 0},
	{64, 26154, 8, 0x00, 5, 36, 58, 255, 1, 0},
	{72, 26188, 8, 0x00, 5, 36, 58, 255, 0, 0},
	{0, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{1, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{2, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{3, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{4, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{5, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{6, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{7, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{8, 26222, 16, 0x00, 5, 37, 59, 255, 255, 0},
	{24, 26269, 16, 0x00, 5, 37, 59, 255, 7, 0},
	{40, 26323, 16, 0x00, 5, 37, 59, 255, 6, 0},
	{56, 26377, 16, 0x00, 5, 37, 59, 255, 5, 0},
	{72, 26431, 16, 0x00, 5, 37, 59, 255, 4, 0},
	{88, 26485, 16, 0x00, 5, 37, 59, 255, 3, 0},
	{104, 26539, 16, 0x00, 5, 37, 59, 255, 2, 0},
	{120, 26593, 16, 0x00, 5, 37, 59, 255, 1, 0},
	{136, 26647, 16, 0x00, 5, 37, 59, 255, 0, 0},
	{0, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{1, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{2, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{3, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{4, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{5, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{6, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{7, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{8, 26701, 16, 0x00, 5, 37, 59, 255, 255, 0},
	{24, 26748, 16, 0x00, 5, 37, 59, 255, 7, 0},
	{40, 26802, 16, 0x00, 5, 37, 59, 255, 6, 0},
	{56, 26856, 16, 0x00, 5, 37, 59, 255, 5, 0},
	{72, 26910, 16, 0x00, 5, 37, 59, 255, 4, 0},
	{88, 26964, 16, 0x00, 5, 37, 59, 255, 3, 0},
	{104, 27018, 16, 0x00, 5, 37, 59, 255, 2, 0},
	{120, 27072, 16, 0x00, 5, 37, 59, 255, 1, 0},
	{136, 27126, 16, 0x00, 5, 37, 59, 255, 0, 0},
	{0, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{1, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{2, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{3, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{4, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{5, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{6, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{7, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{8, 27180, 24, 0x00, 28, 16, 60, 255, 255, 0},
	{32, 27217, 24, 0x00, 28, 16, 60, 255, 7, 0},
	{56, 27261, 24, 0x00, 28, 16, 60, 255, 6, 0},
	{80, 27305, 24, 0x00, 28, 16, 60, 255, 5, 0},
	{104, 27349, 24, 0x00, 28, 16, 60, 255, 4, 0},
	{128, 27393, 24, 0x00, 28, 16, 60, 255, 3, 0},
	{152, 27437, 24, 0x00, 28, 16, 60, 255, 2, 0},
	{176, 27481, 24, 0x00, 28, 16, 60, 255, 1, 0},
	{200, 27525, 24, 0x00, 28, 16, 60, 255, 0, 0},
	{6, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{7, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{8, 27569, 8, 0x00, 5, 1, 4, 255, 1, 0},
	{16, 27588, 8, 0x00, 5, 1, 4, 255, 0, 0},
	{0, 27607, 32, 0x00, 13, 0, 36, 255, 255, 0},
	{32, 27651, 32, 0x00, 1, 0, 36, 255, 255, 0},
	{6, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{7, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{8, 27696, 16, 0x00, 7, 38, 61, 255, 1, 0},
	{24, 27725, 16, 0x00, 7, 38, 61, 255, 0, 0},
	{4, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{5, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{6, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{7, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{15, 27754, 1, 0x10, 3, 0, 0, 255, 3, 0},
	{16, 27788, 16, 0x00, 21, 0, 37, 255, 2, 0},
	{32, 27848, 16, 0x00, 21, 0, 37, 255, 1, 0},
	{48, 27908, 16, 0x00, 21, 0, 37, 255, 0, 0},
	{9, 27972, 2, 0x20, 4, 0, 2, 24, 255, 0},
	{11, 28014, 2, 0x20, 4, 0, 2, 25, 255, 0},
	{13, 28054, 2, 0x20, 4, 0, 2, 26, 255, 0},
	{0, 28094, 16, 0x00, 14, 0, 3, 255, 255, 0},
	{2, 28220, 2, 0x20, 4, 0, 2, 27, 255, 0},
	{4, 28259, 2, 0x20, 4, 0, 2, 28, 255, 0},
	{6, 28290, 2, 0x20, 4, 0, 2, 29, 255, 0},
	{0, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{1, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{2, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{3, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{4, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{5, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{6, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{7, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{8, 28322, 16, 0x00, 5, 22, 4, 255, 7, 0},
	{24, 28356, 16, 0x00, 5, 22, 4, 255, 6, 0},
	{40, 28390, 16, 0x00, 5, 22, 4, 255, 5, 0},
	{56, 28424, 16, 0x00, 5, 22, 4, 255, 4, 0},
	{72, 28458, 16, 0x00, 1, 12, 39, 255, 3, 0},
	{88, 28492, 16, 0x00, 1, 12, 39, 255, 2, 0},
	{104, 28526, 16, 0x00, 1, 12, 39, 255, 1, 0},
	{120, 28560, 16, 0x00, 1, 12, 39, 255, 0, 0},
	{6, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{7, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{8, 28594, 16, 0x00, 7, 7, 12, 255, 1, 0},
	{24, 28628, 16, 0x00, 7, 7, 12, 255, 0, 0},
	{0, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{1, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{2, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{3, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{4, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{5, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{6, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{7, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{8, 28662, 16, 0x01, 8, 0, 62, 255, 7, 0},
	{24, 28685, 16, 0x01, 8, 0, 62, 255, 6, 0},
	{40, 28708, 16, 0x01, 8, 0, 62, 255, 5, 0},
	{56, 28731, 16, 0x01, 8, 0, 62, 255, 4, 0},
	{72, 28754, 16, 0x01, 8, 0, 62, 255, 3, 0},
	{88, 28777, 16, 0x01, 8, 0, 62, 255, 2, 0},
	{104, 28800, 16, 0x01, 8, 0, 62, 255, 1, 0},
	{120, 28823, 16, 0x01, 8, 0, 62, 255, 0, 0},
	{0, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{1, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{2, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{3, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{4, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{5, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{6, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{7, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{8, 28846, 16, 0x01, 18, 0, 62, 255, 7, 0},
	{24, 28870, 16, 0x01, 18, 0, 62, 255, 6, 0},
	{40, 28894, 16, 0x01, 18, 0, 62, 255, 5, 0},
	{56, 28918, 16, 0x01, 18, 0, 62, 255, 4, 0},
	{72, 28942, 16, 0x01, 18, 0, 62, 255, 3, 0},
	{88, 28966, 16, 0x01, 18, 0, 62, 255, 2, 0},
	{104, 28990, 16, 0x01, 18, 0, 62, 255, 1, 0},
	{120, 29014, 16, 0x01, 18, 0, 62, 255, 0, 0},
	{4, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{5, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{6, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{7, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{8, 29038, 16, 0x01, 7, 13, 49, 255, 3, 0},
	{24, 29067, 16, 0x01, 7, 29, 50, 255, 2, 0},
	{40, 29108, 16, 0x01, 7, 13, 49, 255, 1, 0},
	{56, 29137, 16, 0x01, 7, 29, 50, 255, 0, 0},
	{4, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{5, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{6, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{7, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{8, 29178, 16, 0x00, 6, 15, 23, 255, 3, 0},
	{24, 29201, 16, 0x00, 6, 15, 23, 255, 2, 0},
	{40, 29224, 16, 0x00, 6, 15, 23, 255, 1, 0},
	{56, 29247, 16, 0x00, 6, 15, 23, 255, 0, 0},
	{5, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{6, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{7, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{8, 29270, 16, 0x00, 14, 0, 3, 255, 2, 0},
	{24, 29308, 16, 0x00, 28, 25, 42, 255, 1, 0},
	{40, 29344, 16, 0x00, 28, 25, 42, 255, 0, 0},
	{0, 29378, 16, 0x00, 27, 18, 30, 255, 255, 0},
	{16, 29404, 16, 0x00, 27, 18, 30, 255, 255, 0},
	{6, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{7, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{8, 29430, 8, 0x00, 5, 1, 4, 255, 1, 0},
	{16, 29456, 8, 0x00, 5, 1, 4, 255, 0, 0},
	{0, 29481, 32, 0x00, 14, 25, 52, 255, 255, 0},
	{0, 29497, 32, 0x01, 27, 39, 63, 255, 255, 0},
	{4, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{5, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{6, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{7, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{8, 29535, 16, 0x00, 12, 18, 30, 255, 3, 0},
	{24, 29560, 16, 0x00, 11, 31, 51, 255, 2, 0},
	{40, 29587, 16, 0x00, 23, 40, 64, 255, 1, 0},
	{56, 29626, 32, 0x00, 27, 41, 65, 255, 0, 0},
	{0, 29663, 16, 0x00, 1, 0, 3, 255, 255, 0},
	{5, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{6, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{7, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{8, 29713, 8, 0x00, 5, 30, 4, 255, 2, 0},
	{24, 29742, 16, 0x00, 24, 34, 66, 255, 0, 0},
	{0, 29790, 8, 0x20, 4, 0, 2, 30, 255, 0},
	{0, 29819, 32, 0x00, 28, 16, 67, 255, 255, 0},
	{0, 29855, 32, 0x01, 23, 42, 68, 255, 255, 0},
	{3, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{4, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{5, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{6, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{7, 65535, 1, 0x40, 3, 0, 0, 255, 255, 0},
	{8, 29895, 32, 0x00, 13, 0, 36, 255, 4, 0},
	{40, 29942, 32, 0x00, 13, 0, 36, 255, 4, 0},
	{72, 29989, 32, 0x00, 13, 0, 36, 255, 3, 0},
	{104, 30036, 32, 0x00, 13, 0, 36, 255, 3, 0},
	{136, 30083, 32, 0x00, 13, 0, 36, 255, 2, 0},
	{168, 30130, 32, 0x00, 13, 0, 36, 255, 2, 0},
	{200, 30177, 32, 0x00, 13, 0, 36, 255, 1, 0},
	{232, 30224, 32, 0x00, 13, 0, 36, 255, 1, 0},
	{264, 30271, 32, 0x00, 13, 0, 36, 255, 0, 0},
	{296, 30318, 32, 0x00, 13, 0, 36, 255, 0, 0},
	{0, 30368, 32, 0x00, 29, 25, 69, 255, 255, 0},
	{32, 30378, 32, 0x00, 29, 25, 69, 255, 255, 0},
	{0, 30389, 8, 0x00, 12, 43, 70, 255, 255, 0},
	{16, 30423, 8, 0x00, 29, 0, 8, 255, 255, 0},
	{0, 30444, 16, 0x00, 12, 44, 71, 255, 255, 0},
	{0, 30471, 32, 0x01, 23, 16, 72, 255, 255, 0},
	{0, 30498, 8, 0x00, 6, 2, 5, 255, 255, 0},
	{0, 30529, 8, 0x00, 6, 2, 5, 255, 255, 0},
	{0, 30567, 8, 0x00, 5, 0, 4, 255, 255, 0},
	{0, 30606, 8, 0x00, 30, 45, 73, 255, 255, 0},
	{0, 30636, 8, 0x00, 29, 0, 8, 255, 255, 0},
	{0, 30662, 8, 0x00, 12, 46, 74, 255, 255, 0},
	{0, 30698, 16, 0x01, 29, 7, 75, 255, 255, 0},
	{0, 30735, 24, 0x01, 29, 47, 76, 255, 255, 0},
	{0, 30758, 24, 0x01, 29, 25, 77, 255, 255, 0},
	{0, 30783, 16, 0x00, 23, 46, 78, 255, 255, 0},
	{0, 30801, 16, 0x00, 14, 48, 79, 255, 255, 0},
	{0, 30817, 16, 0x00, 28, 25, 80, 255, 255, 0},
	{16, 30849, 16, 0x00, 28, 25, 80, 255, 255, 0},
	{32, 30881, 16, 0x00, 28, 25, 80, 255, 255, 0},
	{48, 30913, 16, 0x00, 28, 25, 80, 255, 255, 0},
	{64, 30945, 16, 0x00, 28, 25, 80, 255, 255, 0},
	{80, 30977, 16, 0x00, 28, 25, 80, 255, 255, 0},
	{96, 31009, 16, 0x00, 28, 25, 80, 255, 255, 0},
	{0, 31041, 8, 0x00, 12, 43, 70, 255, 255, 0},
	{0, 31067, 8, 0x00, 6, 0, 81, 255, 255, 0},
	{0, 31098, 16, 0x01, 31, 49, 82, 255, 255, 0},
	{0, 31117, 16, 0x01, 32, 16, 83, 255, 255, 0},
	{0, 6679, 8, 0x00, 6, 50, 84, 255, 255, 0},
	{0, 31138, 24, 0x00, 14, 0, 85, 255, 255, 0},
	{0, 31153, 8, 0x00, 6, 50, 84, 255, 255, 0},
	{0, 31173, 8, 0x00, 5, 1, 4, 255, 255, 0},
	{0, 31193, 16, 0x00, 14, 0, 3, 255, 255, 0},
	{0, 31210, 32, 0x00, 14, 16, 86, 255, 255, 0},
	{0, 31232, 8, 0x00, 31, 28, 73, 255, 255, 0},
	{0, 31253, 8, 0x00, 5, 51, 4, 255, 255, 0},
	{0, 31278, 8, 0x00, 5, 51, 4, 255, 255, 0},
	{0, 31153, 16, 0x01, 6, 25, 87, 255, 255, 0},
	{0, 31306, 16, 0x01, 6, 25, 87, 255, 255, 0},
	{0, 31345, 16, 0x01, 6, 25, 45, 255, 255, 0},
	{0, 31378, 16, 0x01, 6, 25, 45, 255, 255, 0},
	{0, 31412, 16, 0x01, 6, 25, 45, 255, 255, 0},
	{0, 31441, 16, 0x01, 6, 25, 87, 255, 255, 0},
	{0, 31459, 16, 0x01, 6, 25, 88, 255, 255, 0},
	{0, 31486, 8, 0x00, 1, 0, 8, 255, 255, 0},
	{8, 31506, 8, 0x00, 1, 0, 8, 255, 255, 0},
	{0, 31526, 16, 0x01, 6, 25, 89, 255, 255, 0},
	{0, 31553, 16, 0x01, 18, 25, 90, 255, 255, 0},
	{0, 31586, 16, 0x01, 6, 52, 91, 255, 255, 0},
	{0, 31606, 16, 0x01, 18, 25, 45, 255, 255, 0},
	{0, 31631, 16, 0x00, 33, 28, 48, 255, 255, 0},
	{0, 31667, 16, 0x00, 6, 53, 92, 255, 255, 0},
	{0, 31696, 16, 0x01, 34, 54, 93, 255, 255, 0},
	{0, 31741, 16, 0x01, 34, 54, 94, 255, 255, 0},
	{0, 31787, 16, 0x01, 1, 55, 95, 255, 255, 0},
	{0, 31808, 16, 0x01, 34, 54, 93, 255, 255, 0},
	{0, 31853, 32, 0x00, 14, 0, 36, 255, 255, 0},
	{0, 31875, 16, 0x01, 5, 7, 4, 255, 255, 0},
	{0, 31900, 16, 0x01, 10, 56, 96, 255, 255, 0},
	{0, 31951, 16, 0x01, 10, 56, 96, 255, 255, 0},
	{0, 32003, 16, 0x00, 26, 7, 12, 255, 255, 0},
	{0, 32020, 16, 0x01, 23, 25, 97, 255, 255, 0},
	{0, 32046, 16, 0x01, 23, 25, 97, 255, 255, 0},
	{0, 32069, 32, 0x00, 14, 16, 67, 255, 255, 0},
	{0, 32095, 16, 0x00, 10, 57, 98, 255, 255, 0},
	{0, 32147, 16, 0x00, 10, 57, 98, 255, 255, 0},
	{0, 32199, 8, 0x00, 1, 0, 8, 255, 255, 0},
	{0, 32237, 16, 0x00, 35, 58, 99, 255, 255, 0},
	{0, 32247, 16, 0x00, 35, 58, 100, 255, 255, 0},
	{0, 8612, 16, 0x00, 6, 59, 101, 255, 255, 0},
	{0, 32275, 16, 0x00, 7, 25, 42, 255, 255, 0},
	{0, 32301, 16, 0x00, 6, 60, 102, 255, 255, 0},
	{0, 32311, 16, 0x00, 7, 25, 42, 255, 255, 0},
	{0, 32340, 16, 0x00, 1, 7, 12, 255, 255, 0},
	{0, 32370, 16, 0x01, 8, 30, 103, 255, 255, 0},
	{0, 32401, 16, 0x01, 10, 27, 104, 255, 255, 0},
	{0, 32433, 16, 0x01, 33, 28, 105, 255, 255, 0},
	{0, 32460, 16, 0x01, 18, 25, 45, 255, 255, 0},
	{0, 32497, 16, 0x01, 18, 25, 45, 255, 255, 0},
	{0, 32528, 32, 0x01, 18, 25, 106, 255, 255, 0},
	{0, 32561, 16, 0x01, 10, 28, 105, 255, 255, 0},
	{0, 32583, 8, 0x00, 1, 0, 8, 255, 255, 0},
	{0, 32588, 16, 0x01, 34, 61, 107, 255, 255, 0},
	{0, 32625, 16, 0x01, 34, 61, 107, 255, 255, 0},
	{0, 32650, 16, 0x00, 6, 62, 108, 255, 255, 0},
	{0, 32678, 16, 0x00, 1, 63, 109, 255, 255, 0},
	{0, 32733, 8, 0x00, 1, 0, 8, 255, 255, 0},
	{0, 32747, 16, 0x00, 1, 64, 17, 255, 255, 0},
	{0, 32767, 16, 0x00, 6, 59, 110, 255, 255, 0},
	{0, 32811, 16, 0x01, 36, 7, 35, 255, 255, 0},
	{0, 32832, 16, 0x00, 5, 7, 4, 255, 255, 0},
	{0, 32866, 16, 0x00, 5, 7, 4, 255, 255, 0},
	{0, 32905, 8, 0x00, 1, 0, 8, 255, 255, 0},
	{0, 32937, 16, 0x00, 1, 0, 3, 255, 255, 0},
	{0, 32972, 16, 0x00, 1, 0, 3, 255, 255, 0},
	{0, 33007, 16, 0x00, 1, 0, 37, 255, 255, 0},
	{0, 33042, 16, 0x00, 1, 0, 3, 255, 255, 0},
	{0, 33077, 16, 0x00, 6, 59, 111, 255, 255, 0},
	{0, 33102, 16, 0x00, 1, 0, 3, 255, 255, 0},
	{0, 33131, 16, 0x00, 1, 0, 3, 255, 255, 0},
	{0, 33163, 16, 0x01, 5, 7, 75, 255, 255, 0},
	{64, 33238, 16, 0x01, 5, 7, 112, 255, 255, 0},
	{0, 1768, 16, 0x00, 8, 5, 113, 255, 255, 0},
	{0, 1784, 8, 0x00, 9, 0, 8, 255, 255, 0},
	{0, 4449, 16, 0x00, 13, 0, 3, 255, 255, 0},
	{0, 33269, 16, 0x00, 14, 0, 3, 255, 255, 0},
	{0, 33311, 8, 0x00, 1, 0, 8, 255, 255, 0},
	{0, 33349, 16, 0x00, 14, 0, 3, 255, 255, 0},
	{0, 5589, 8, 0x00, 7, 0, 8, 255, 255, 0},
	{0, 33386, 16, 0x00, 6, 15, 114, 255, 255, 0},
	{0, 6581, 16, 0x00, 12, 16, 115, 255, 255, 0},
	{0, 6679, 8, 0x00, 6, 2, 5, 255, 255, 0},
	{0, 33410, 8, 0x00, 5, 65, 4, 255, 255, 0},
	{0, 33439, 8, 0x00, 5, 65, 4, 255, 255, 0},
	{0, 31526, 8, 0x00, 6, 2, 5, 255, 255, 0},
	{0, 33468, 16, 0x00, 21, 5, 116, 255, 255, 0},
	{0, 33485, 8, 0x00, 5, 51, 4, 255, 255, 0},
	{0, 33503, 8, 0x00, 5, 0, 4, 255, 255, 0},
	{0, 33523, 8, 0x00, 5, 0, 4, 255, 255, 0},
	{4, 33544, 1, 0x10, 3, 0, 0, 255, 255, 0},
	{5, 33571, 1, 0x10, 3, 0, 0, 255, 255, 0},
	{6, 33601, 1, 0x10, 0, 0, 0, 255, 255, 0},
	{7, 33607, 1, 0x10, 0, 0, 0, 255, 255, 0},
	{0, 33619, 16, 0x00, 9, 25, 8, 255, 255, 0},
	{0, 33643, 16, 0x00, 9, 25, 8, 255, 255, 0},
	{0, 33666, 16, 0x00, 9, 25, 8, 255, 255, 0},
	{0, 33689, 16, 0x00, 9, 25, 8, 255, 255, 0},
	{0, 33711, 16, 0x00, 9, 7, 117, 255, 255, 0},
	{0, 33729, 16, 0x01, 18, 18, 118, 255, 255, 0},
	{0, 33751, 16, 0x01, 18, 0, 62, 255, 255, 0},
	{0, 33778, 16, 0x00, 15, 25, 80, 255, 255, 0},
	{0, 33816, 8, 0x00, 6, 0, 10, 255, 255, 0},
	{0, 33837, 8, 0x00, 6, 0, 10, 255, 255, 0},
	{0, 33858, 1, 0x10, 0, 0, 0, 255, 255, 0},
	{1, 33888, 1, 0x10, 0, 0, 0, 255, 255, 0},
	{2, 33918, 1, 0x10, 0, 0, 0, 255, 255, 0},
	{3, 33948, 1, 0x10, 0, 0, 0, 255, 255, 0},
	{4, 33978, 1, 0x10, 0, 0, 0, 255, 255, 0},
	{0, 34008, 8, 0x00, 34, 66, 119, 255, 255, 0},
	{0, 34025, 8, 0x00, 34, 66, 119, 255, 255, 0},
	{0, 34042, 16, 0x03, 31, 7, 35, 255, 255, 0},
	{0, 34100, 16, 0x03, 18, 18, 118, 255, 255, 0},
	{0, 34156, 16, 0x03, 31, 7, 35, 255, 255, 0},
	{0, 34214, 16, 0x03, 18, 18, 118, 255, 255, 0},
	{0, 34270, 16, 0x03, 5, 67, 4, 255, 255, 0},
	{0, 34315, 16, 0x03, 37, 68, 120, 255, 255, 0},
	{0, 34351, 16, 0x02, 13, 27, 47, 255, 255, 0},
	{0, 34388, 16, 0x02, 13, 27, 47, 255, 255, 0},
	{0, 34435, 16, 0x02, 13, 27, 47, 255, 255, 0},
	{0, 34482, 16, 0x01, 18, 0, 62, 255, 255, 0},
	{0, 34497, 16, 0x03, 31, 7, 35, 255, 255, 0},
	{0, 34555, 16, 0x03, 37, 68, 120, 255, 255, 0},
	{0, 34591, 16, 0x03, 37, 7, 35, 255, 255, 0},
	{0, 34636, 16, 0x02, 14, 10, 121, 255, 255, 0},
	{0, 34691, 16, 0x02, 1, 0, 3, 255, 255, 0},
	{0, 34731, 16, 0x02, 14, 10, 16, 255, 255, 0},
	{0, 34786, 16, 0x00, 1, 0, 3, 255, 255, 0},
	{0, 34822, 16, 0x01, 31, 16, 122, 255, 255, 0},
	{0, 34854, 16, 0x01, 31, 16, 122, 255, 255, 0},
	{0, 34886, 16, 0x01, 31, 16, 122, 255, 255, 0},
	{0, 34918, 16, 0x01, 31, 16, 122, 255, 255, 0},
	{0, 34950, 16, 0x01, 6, 0, 62, 255, 255, 0},
	{0, 34982, 16, 0x01, 6, 0, 62, 255, 255, 0},
	{0, 35012, 16, 0x01, 6, 0, 62, 255, 255, 0},
	{0, 35042, 8, 0x00, 31, 69, 123, 255, 255, 0},
	{0, 35069, 8, 0x00, 31, 69, 123, 255, 255, 0},
	{0, 35096, 16, 0x01, 31, 16, 83, 255, 255, 0},
	{0, 35130, 16, 0x01, 31, 16, 83, 255, 255, 0},
	{0, 35164, 16, 0x01, 31, 16, 83, 255, 255, 0},
	{0, 35199, 16, 0x01, 6, 0, 62, 255, 255, 0},
	{0, 35229, 16, 0x00, 18, 0, 3, 255, 255, 0},
	{0, 35243, 16, 0x00, 8, 5, 124, 255, 255, 0},
	{0, 35268, 16, 0x00, 8, 5, 124, 255, 255, 0},
	{0, 35294, 8, 0x00, 6, 2, 5, 255, 255, 0},
	{0, 35335, 16, 0x00, 1, 25, 80, 255, 255, 0},
	{0, 35371, 16, 0x00, 9, 7, 117, 255, 255, 0},
	{0, 35470, 8, 0x20, 4, 0, 2, 31, 255, 0},
	{0, 35483, 16, 0x01, 18, 18, 125, 255, 255, 0},
	{0, 35516, 16, 0x01, 18, 18, 125, 255, 255, 0},
	{0, 35549, 8, 0x00, 31, 69, 123, 255, 255, 0},
	{0, 35567, 8, 0x00, 31, 69, 123, 255, 255, 0},
	{0, 35585, 8, 0x00, 8, 70, 126, 255, 255, 0},
	{0, 1784, 8, 0x00, 9, 0, 8, 255, 255, 0},
	{0, 35609, 8, 0x00, 5, 65, 4, 255, 255, 0}
};

static const ObdbCommandDef OBDB_COMMANDS[] OBD_PROGMEM = {
	{0x7DF, 0x0, 0x1, 0, 0x01, 25, 0x01, 24, {0, 0}},
	{0x7DF, 0x0, 0x2, 25, 0x01, 1, 0x01, 255, {0, 0}},
	{0x7DF, 0x0, 0x3, 26, 0x01, 2, 0x01, 255, {0, 0}},
	{0x7DF, 0x0, 0x4, 28, 0x01, 1, 0x01, 255, {0, 0}},
	{0x7DF, 0x0, 0x5, 29, 0x01, 1, 0x01, 255, {0, 0}},
	{0x7DF, 0x0, 0x6, 30, 0x01, 1, 0x01, 255, {0, 0}},
	{0x7DF, 0x0, 0x7, 31, 0x01, 1, 0x01, 255, {0, 0}},
	{0x7DF, 0x0, 0x8, 32, 0x01, 2, 0x01, 255, {0, 0}},
	{0x7DF, 0x0, 0x9, 34, 0x01, 2, 0x01, 255, {0, 0}},
	{0x7DF, 0x0, 0xA, 36, 0x01, 1, 0x01, 255, {0, 0}},
	{0x7DF, 0x0, 0xB, 37, 0x01, 1, 0x01, 255, {0, 0}},
	{0x7DF, 0x0, 0xC, 38, 0x01, 1, 0x01, 255, {0, 0}},
	{0x7DF, 0x0, 0xD, 39, 0x01, 1, 0x01, 255, {0, 0}},
	{0x7DF, 0x0, 0xE, 40, 0x01, 1, 0x01, 255, {0, 0}},
	{0x7DF, 0x0, 0xF, 41, 0x01, 1, 0x01, 255, {0, 0}},
	{0x7DF, 0x0, 0x10, 42, 0x01, 1, 0x01, 255, {0, 0}},
	{0x7DF, 0x0, 0x11, 43, 0x01, 1, 0x01, 255, {0, 0}},
	{0x7DF, 0x0, 0x12, 44, 0x01, 1, 0x01, 255, {0, 0}},
	{0x7DF, 0x0, 0x13, 45, 0x01, 8, 0x01, 255, {0, 0}},
	{0x7DF, 0x0, 0x14, 53, 0x01, 2, 0x01, 255, {0, 0}},
	{0x7DF, 0x0, 0x15, 55, 0x01, 2, 0x01, 255, {0, 0}},
	{0x7DF, 0x0, 0x16, 57, 0x01, 2, 0x01, 255, {0, 0}},
	{0x7DF, 0x0, 0x17, 59, 0x01, 2, 0x01, 255, {0, 0}},
	{0x7DF, 0x0, 0x18, 61, 0x01, 2, 0x01, 255, {0, 0}},
	{0x7DF, 0x0, 0x19, 63, 0x01, 2, 0x01, 255, {0, 0}},
	{0x7DF, 0x0, 0x1A, 65, 0x01, 2, 0x01, 255, {0, 0}},
	{0x7DF, 0x0, 0x1B, 67, 0x01, 2, 0x01, 255, {0, 0}},
	{0x7DF, 0x0, 0x1C, 69, 0x01, 1, 0x01, 255, {0, 0}},
	{0x7DF, 0x0, 0x1D, 70, 0x01, 8, 0x01, 255, {0, 0}},
	{0x7DF, 0x0, 0x1E, 78, 0x01, 1, 0x01, 255, {0, 0}},
	{0x7DF, 0x0, 0x1F, 79, 0x01, 1, 0x01, 255, {0, 0}},
	{0x7DF, 0x0, 0x21, 80, 0x01, 1, 0x01, 255, {0, 0}},
	{0x7DF, 0x0, 0x22, 81, 0x01, 1, 0x01, 255, {0, 0}},
	{0x7DF, 0x0, 0x23, 82, 0x01, 1, 0x01, 255, {0, 0}},
	{0x7DF, 0x0, 0x24, 83, 0x01, 2, 0x01, 255, {0, 0}},
	{0x7DF, 0x0, 0x25, 85, 0x01, 2, 0x01, 255, {0, 0}},
	{0x7DF, 0x0, 0x26, 87, 0x01, 2, 0x01, 255, {0, 0}},
	{0x7DF, 0x0, 0x27, 89, 0x01, 2, 0x01, 255, {0, 0}},
	{0x7DF, 0x0, 0x28, 91, 0x01, 2, 0x01, 255, {0, 0}},
	{0x7DF, 0x0, 0x29, 93, 0x01, 2, 0x01, 255, {0, 0}},
	{0x7DF, 0x0, 0x2A, 95, 0x01, 2, 0x01, 255, {0, 0}},
	{0x7DF, 0x0, 0x2B, 97, 0x01, 2, 0x01, 255, {0, 0}},
	{0x7DF, 0x0, 0x2C, 99, 0x01, 1, 0x01, 255, {0, 0}},
	{0x7DF, 0x0, 0x2D, 100, 0x01, 1, 0x01, 255, {0, 0}},
	{0x7DF, 0x0, 0x2E, 101, 0x01, 1, 0x01, 255, {0, 0}},
	{0x7DF, 0x0, 0x2F, 102, 0x01, 1, 0x01, 255, {0, 0}},
	{0x7DF, 0x0, 0x30, 103, 0x01, 1, 0x01, 255, {0, 0}},
	{0x7DF, 0x0, 0x31, 104, 0x01, 1, 0x01, 255, {0, 0}},
	{0x7DF, 0x0, 0x32, 105, 0x01, 1, 0x01, 255, {0, 0}},
	{0x7DF, 0x0, 0x33, 106, 0x01, 1, 0x01, 255, {0, 0}},
	{0x7DF, 0x0, 0x34, 107, 0x01, 2, 0x01, 255, {0, 0}},
	{0x7DF, 0x0, 0x35, 109, 0x01, 2, 0x01, 255, {0, 0}},
	{0x7DF, 0x0, 0x36, 111, 0x01, 2, 0x01, 255, {0, 0}},
	{0x7DF, 0x0, 0x37, 113, 0x01, 2, 0x01, 255, {0, 0}},
	{0x7DF, 0x0, 0x38, 115, 0x01, 2, 0x01, 255, {0, 0}},
	{0x7DF, 0x0, 0x39, 117, 0x01, 2, 0x01, 255, {0, 0}},
	{0x7DF, 0x0, 0x3A, 119, 0x01, 2, 0x01, 255, {0, 0}},
	{0x7DF, 0x0, 0x3B, 121, 0x01, 2, 0x01, 255, {0, 0}},
	{0x7DF, 0x0, 0x3C, 123, 0x01, 1, 0x01, 255, {0, 0}},
	{0x7DF, 0x0, 0x3D, 124, 0x01, 1, 0x01, 255, {0, 0}},
	{0x7DF, 0x0, 0x3E, 125, 0x01, 1, 0x01, 255, {0, 0}},
	{0x7DF, 0x0, 0x3F, 126, 0x01, 1, 0x01, 255, {0, 0}},
	{0x7DF, 0x0, 0x41, 127, 0x01, 23, 0x01, 22, {0, 0}},
	{0x7DF, 0x0, 0x42, 150, 0x01, 1, 0x01, 255, {0, 0}},
	{0x7DF, 0x0, 0x43, 151, 0x01, 1, 0x01, 255, {0, 0}},
	{0x7DF, 0x0, 0x44, 152, 0x01, 1, 0x01, 255, {0, 0}},
	{0x7DF, 0x0, 0x45, 153, 0x01, 1, 0x01, 255, {0, 0}},
	{0x7DF, 0x0, 0x46, 154, 0x01, 1, 0x01, 255, {0, 0}},
	{0x7DF, 0x0, 0x47, 155, 0x01, 1, 0x01, 255, {0, 0}},
	{0x7DF, 0x0, 0x48, 156, 0x01, 1, 0x01, 255, {0, 0}},
	{0x7DF, 0x0, 0x49, 157, 0x01, 1, 0x01, 255, {0, 0}},
	{0x7DF, 0x0, 0x4A, 158, 0x01, 1, 0x01, 255, {0, 0}},
	{0x7DF, 0x0, 0x4B, 159, 0x01, 1, 0x01, 255, {0, 0}},
	{0x7DF, 0x0, 0x4C, 160, 0x01, 1, 0x01, 255, {0, 0}},
	{0x7DF, 0x0, 0x4D, 161, 0x01, 1, 0x01, 255, {0, 0}},
	{0x7DF, 0x0, 0x4E, 162, 0x01, 1, 0x01, 255, {0, 0}},
	{0x7DF, 0x0, 0x4F, 163, 0x01, 4, 0x01, 255, {0, 0}},
	{0x7DF, 0x0, 0x50, 167, 0x01, 1, 0x01, 255, {0, 0}},
	{0x7DF, 0x0, 0x51, 168, 0x01, 1, 0x01, 255, {0, 0}},
	{0x7DF, 0x0, 0x52, 169, 0x01, 1, 0x01, 255, {0, 0}},
	{0x7DF, 0x0, 0x53, 170, 0x01, 1, 0x01, 255, {0, 0}},
	{0x7DF, 0x0, 0x54, 171, 0x01, 1, 0x01, 255, {0, 0}},
	{0x7DF, 0x0, 0x55, 172, 0x01, 1, 0x01, 255, {0, 0}},
	{0x7DF, 0x0, 0x56, 173, 0x01, 1, 0x01, 255, {0, 0}},
	{0x7DF, 0x0, 0x57, 174, 0x01, 1, 0x01, 255, {0, 0}},
	{0x7DF, 0x0, 0x58, 175, 0x01, 1, 0x01, 255, {0, 0}},
	{0x7DF, 0x0, 0x59, 176, 0x01, 1, 0x01, 255, {0, 0}},
	{0x7DF, 0x0, 0x5A, 177, 0x01, 1, 0x01, 255, {0, 0}},
	{0x7DF, 0x0, 0x5B, 178, 0x01, 1, 0x01, 255, {0, 0}},
	{0x7DF, 0x0, 0x5C, 179, 0x01, 1, 0x01, 255, {0, 0}},
	{0x7DF, 0x0, 0x5D, 180, 0x01, 1, 0x01, 255, {0, 0}},
	{0x7DF, 0x0, 0x5E, 181, 0x01, 1, 0x01, 255, {0, 0}},
	{0x7DF, 0x0, 0x5F, 182, 0x01, 1, 0x01, 255, {0, 0}},
	{0x7DF, 0x0, 0x61, 183, 0x01, 1, 0x01, 255, {0, 0}},
	{0x7DF, 0x0, 0x62, 184, 0x01, 1, 0x01, 255, {0, 0}},
	{0x7DF, 0x0, 0x63, 185, 0x01, 1, 0x01, 255, {0, 0}},
	{0x7DF, 0x0, 0x64, 186, 0x01, 5, 0x01, 255, {0, 0}},
	{0x7DF, 0x0, 0x65, 191, 0x01, 10, 0x01, 255, {0, 0}},
	{0x7DF, 0x0, 0x66, 201, 0x01, 4, 0x01, 255, {0, 0}},
	{0x7DF, 0x0, 0x67, 205, 0x01, 4, 0x01, 255, {0, 0}},
	{0x7DF, 0x0, 0x68, 209, 0x01, 12, 0x01, 255, {0, 0}},
	{0x7DF, 0x0, 0x69, 221, 0x01, 12, 0x01, 255, {0, 0}},
	{0x7DF, 0x0, 0x6A, 233, 0x01, 8, 0x01, 255, {0, 0}},
	{0x7DF, 0x0, 0x6B, 241, 0x01, 12, 0x01, 255, {0, 0}},
	{0x7DF, 0x0, 0x6C, 253, 0x01, 8, 0x01, 255, {0, 0}},
	{0x7DF, 0x0, 0x6D, 261, 0x01, 12, 0x01, 255, {0, 0}},
	{0x7DF, 0x0, 0x6E, 273, 0x01, 8, 0x01, 255, {0, 0}},
	{0x7DF, 0x0, 0x70, 281, 0x01, 12, 0x01, 255, {0, 0}},
	{0x7DF, 0x0, 0x71, 293, 0x01, 12, 0x01, 255, {0, 0}},
	{0x7DF, 0x0, 0x72, 305, 0x01, 8, 0x01, 255, {0, 0}},
	{0x7DF, 0x0, 0x73, 313, 0x01, 4, 0x01, 255, {0, 0}},
	{0x7DF, 0x0, 0x74, 317, 0x01, 4, 0x01, 255, {0, 0}},
	{0x7DF, 0x0, 0x75, 321, 0x01, 8, 0x01, 255, {0, 0}},
	{0x7DF, 0x0, 0x76, 329, 0x01, 8, 0x01, 255, {0, 0}},
	{0x7DF, 0x0, 0x77, 337, 0x01, 8, 0x01, 255, {0, 0}},
	{0x7DF, 0x0, 0x78, 345, 0x01, 8, 0x01, 255, {0, 0}},
	{0x7DF, 0x0, 0x79, 353, 0x01, 8, 0x01, 255, {0, 0}},
	{0x7DF, 0x0, 0x7A, 361, 0x01, 6, 0x01, 255, {0, 0}},
	{0x7DF, 0x0, 0x7B, 367, 0x01, 6, 0x01, 255, {0, 0}},
	{0x7DF, 0x0, 0x7C, 373, 0x01, 8, 0x01, 255, {0, 0}},
	{0x7DF, 0x0, 0x7E, 381, 0x01, 4, 0x01, 255, {0, 0}},
	{0x7DF, 0x0, 0x7F, 385, 0x01, 6, 0x01, 255, {0, 0}},
	{0x7DF, 0x0, 0x81, 391, 0x01, 15, 0x01, 255, {0, 0}},
	{0x7DF, 0x0, 0x82, 406, 0x01, 15, 0x01, 255, {0, 0}},
	{0x7DF, 0x0, 0x83, 421, 0x01, 12, 0x01, 255, {0, 0}},
	{0x7DF, 0x0, 0x84, 433, 0x01, 1, 0x01, 255, {0, 0}},
	{0x7DF, 0x0, 0x85, 434, 0x01, 8, 0x01, 255, {0, 0}},
	{0x7DF, 0x0, 0x86, 442, 0x01, 4, 0x01, 255, {0, 0}},
	{0x7DF, 0x0, 0x88, 446, 0x01, 26, 0x01, 255, {0, 0}},
	{0x7DF, 0x0, 0x89, 472, 0x01, 15, 0x01, 255, {0, 0}},
	{0x7DF, 0x0, 0x8A, 487, 0x01, 15, 0x01, 255, {0, 0}},
	{0x7DF, 0x0, 0x8B, 502, 0x01, 14, 0x01, 255, {0, 0}},
	{0x7DF, 0x0, 0x8C, 516, 0x01, 16, 0x01, 255, {0, 0}},
	{0x7DF, 0x0, 0x8D, 532, 0x01, 1, 0x01, 255, {0, 0}},
	{0x7DF, 0x0, 0x8E, 533, 0x01, 1, 0x01, 255, {0, 0}},
	{0x7DF, 0x0, 0x8F, 534, 0x01, 10, 0x01, 255, {0, 0}},
	{0x7DF, 0x0, 0x90, 544, 0x01, 4, 0x01, 255, {0, 0}},
	{0x7DF, 0x0, 0x91, 548, 0x01, 3, 0x01, 255, {0, 0}},
	{0x7DF, 0x0, 0x92, 551, 0x01, 16, 0x01, 255, {0, 0}},
	{0x7DF, 0x0, 0x93, 567, 0x01, 2, 0x01, 255, {0, 0}},
	{0x7DF, 0x0, 0x94, 569, 0x01, 15, 0x01, 255, {0, 0}},
	{0x7DF, 0x0, 0x95, 584, 0x01, 8, 0x01, 255, {0, 0}},
	{0x7DF, 0x0, 0x96, 592, 0x01, 6, 0x01, 255, {0, 0}},
	{0x7DF, 0x0, 0x97, 598, 0x01, 4, 0x01, 255, {0, 0}},
	{0x7DF, 0x0, 0x9A, 602, 0x01, 8, 0x01, 255, {0, 0}},
	{0x7DF, 0x0, 0x9B, 610, 0x01, 8, 0x01, 255, {0, 0}},
	{0x7DF, 0x0, 0x9C, 618, 0x01, 16, 0x01, 255, {0, 0}},
	{0x7DF, 0x0, 0x9D, 634, 0x01, 2, 0x01, 255, {0, 0}},
	{0x7DF, 0x0, 0x9E, 636, 0x01, 1, 0x01, 255, {0, 0}},
	{0x7DF, 0x0, 0x9F, 637, 0x01, 16, 0x01, 255, {0, 0}},
	{0x7DF, 0x0, 0xA1, 653, 0x01, 12, 0x01, 255, {0, 0}},
	{0x7DF, 0x0, 0xA2, 665, 0x01, 1, 0x01, 255, {0, 0}},
	{0x7DF, 0x0, 0xA3, 666, 0x01, 10, 0x01, 255, {0, 0}},
	{0x7DF, 0x0, 0xA4, 676, 0x01, 4, 0x01, 255, {0, 0}},
	{0x7DF, 0x0, 0xA5, 680, 0x01, 4, 0x01, 255, {0, 0}},
	{0x7DF, 0x0, 0xA6, 684, 0x01, 1, 0x01, 255, {0, 0}},
	{0x7DF, 0x0, 0xA7, 685, 0x01, 12, 0x01, 255, {0, 0}},
	{0x7DF, 0x0, 0xA8, 697, 0x01, 12, 0x01, 255, {0, 0}},
	{0x7DF, 0x0, 0xAA, 709, 0x01, 1, 0x01, 255, {0, 0}},
	{0x7DF, 0x0, 0xAB, 710, 0x01, 10, 0x01, 255, {0, 0}},
	{0x7DF, 0x0, 0xAC, 720, 0x01, 3, 0x01, 255, {0, 0}},
	{0x7DF, 0x0, 0xAE, 723, 0x01, 4, 0x01, 255, {0, 0}},
	{0x7DF, 0x0, 0xAF, 727, 0x01, 1, 0x01, 255, {0, 0}},
	{0x7DF, 0x0, 0xB0, 728, 0x01, 4, 0x01, 255, {0, 0}},
	{0x7DF, 0x0, 0xB1, 732, 0x01, 16, 0x01, 255, {0, 0}},
	{0x7DF, 0x0, 0xB2, 748, 0x01, 1, 0x01, 255, {0, 0}},
	{0x7DF, 0x0, 0xB3, 749, 0x01, 17, 0x01, 255, {0, 0}},
	{0x7DF, 0x0, 0xB4, 766, 0x01, 17, 0x01, 255, {0, 0}},
	{0x7DF, 0x0, 0xB5, 783, 0x01, 17, 0x01, 255, {0, 0}},
	{0x7DF, 0x0, 0xB6, 800, 0x01, 17, 0x01, 255, {0, 0}},
	{0x7DF, 0x0, 0xB7, 817, 0x01, 12, 0x01, 255, {0, 0}},
	{0x7DF, 0x0, 0xB8, 829, 0x01, 1, 0x01, 255, {0, 0}},
	{0x7DF, 0x0, 0xB9, 830, 0x01, 2, 0x01, 255, {0, 0}},
	{0x7DF, 0x0, 0xBA, 832, 0x01, 3, 0x01, 255, {0, 0}},
	{0x7DF, 0x0, 0xBB, 835, 0x01, 17, 0x01, 255, {0, 0}},
	{0x7DF, 0x0, 0xBC, 852, 0x01, 17, 0x01, 255, {0, 0}},
	{0x7DF, 0x0, 0xBD, 869, 0x01, 17, 0x01, 255, {0, 0}},
	{0x7DF, 0x0, 0xBE, 886, 0x01, 17, 0x01, 255, {0, 0}},
	{0x7DF, 0x0, 0xBF, 903, 0x01, 17, 0x01, 255, {0, 0}},
	{0x7DF, 0x0, 0xC1, 920, 0x01, 17, 0x01, 255, {0, 0}},
	{0x7DF, 0x0, 0xC2, 937, 0x01, 17, 0x01, 255, {0, 0}},
	{0x7DF, 0x0, 0xC3, 954, 0x01, 4, 0x01, 255, {0, 0}},
	{0x7DF, 0x0, 0xC4, 958, 0x01, 2, 0x01, 255, {0, 0}},
	{0x7DF, 0x0, 0xC5, 960, 0x01, 4, 0x01, 255, {0, 0}},
	{0x7DF, 0x0, 0xC6, 964, 0x01, 11, 0x01, 255, {0, 0}},
	{0x7DF, 0x0, 0xC7, 975, 0x01, 1, 0x01, 255, {0, 0}},
	{0x7DF, 0x0, 0xC8, 976, 0x01, 3, 0x01, 255, {0, 0}},
	{0x7DF, 0x0, 0xC9, 979, 0x01, 16, 0x01, 255, {0, 0}},
	{0x7DF, 0x0, 0xCB, 995, 0x01, 4, 0x01, 255, {0, 0}},
	{0x7DF, 0x0, 0xCC, 999, 0x01, 16, 0x01, 255, {0, 0}},
	{0x7DF, 0x0, 0xCD, 1015, 0x01, 16, 0x01, 255, {0, 0}},
	{0x7DF, 0x0, 0xCE, 1031, 0x01, 8, 0x01, 255, {0, 0}},
	{0x7DF, 0x0, 0xCF, 1039, 0x01, 8, 0x01, 255, {0, 0}},
	{0x7DF, 0x0, 0xD0, 1047, 0x01, 6, 0x01, 255, {0, 0}},
	{0x7DF, 0x0, 0xD1, 1053, 0x01, 2, 0x01, 255, {0, 0}},
	{0x7DF, 0x0, 0xD2, 1055, 0x01, 4, 0x01, 255, {0, 0}},
	{0x7DF, 0x0, 0xD3, 1059, 0x01, 1, 0x01, 255, {0, 0}},
	{0x7DF, 0x0, 0xD4, 1060, 0x01, 1, 0x01, 255, {0, 0}},
	{0x7DF, 0x0, 0xD5, 1061, 0x01, 8, 0x01, 255, {0, 0}},
	{0x7DF, 0x0, 0xD6, 1069, 0x01, 1, 0x01, 255, {0, 0}},
	{0x7DF, 0x0, 0xD7, 1070, 0x01, 5, 0x01, 255, {0, 0}},
	{0x7DF, 0x0, 0xD8, 1075, 0x01, 1, 0x01, 255, {0, 0}},
	{0x7DF, 0x0, 0xD9, 1076, 0x01, 1, 0x01, 255, {0, 0}},
	{0x7DF, 0x0, 0xDA, 1077, 0x01, 1, 0x01, 255, {0, 0}},
	{0x7DF, 0x0, 0xDC, 1078, 0x01, 15, 0x01, 255, {0, 0}},
	{0x710, 0x77A, 0x123A, 1093, 0x22, 2, 0x02, 255, {0, 0}},
	{0x710, 0x77A, 0x2A00, 1095, 0x22, 1, 0x02, 255, {0, 0}},
	{0x710, 0x77A, 0x2A06, 1096, 0x22, 1, 0x02, 255, {0, 0}},
	{0x710, 0x77A, 0x2A07, 1097, 0x22, 1, 0x02, 255, {0, 0}},
	{0x710, 0x77A, 0x2A09, 1098, 0x22, 1, 0x02, 255, {0, 0}},
	{0x710, 0x77A, 0x2A0A, 1099, 0x22, 1, 0x02, 255, {0, 0}},
	{0x710, 0x77A, 0x2A0B, 1100, 0x22, 1, 0x02, 255, {0, 0}},
	{0x710, 0x77A, 0x2A0C, 1101, 0x22, 1, 0x02, 255, {0, 0}},
	{0x710, 0x77A, 0x2A0E, 1102, 0x22, 1, 0x02, 255, {0, 0}},
	{0x710, 0x77A, 0x2A0F, 1103, 0x22, 1, 0x02, 255, {0, 0}},
	{0x710, 0x77A, 0x2A10, 1104, 0x22, 1, 0x02, 255, {0, 0}},
	{0x710, 0x77A, 0x2A15, 1105, 0x22, 1, 0x02, 255, {0, 0}},
	{0x710, 0x77A, 0x2A17, 1106, 0x22, 1, 0x02, 255, {0, 0}},
	{0x710, 0x77A, 0x2A18, 1107, 0x22, 1, 0x02, 255, {0, 0}},
	{0x710, 0x77A, 0x2A1C, 1108, 0x22, 1, 0x02, 255, {0, 0}},
	{0x710, 0x77A, 0x2AB6, 1109, 0x22, 1, 0x02, 255, {0, 0}},
	{0x710, 0x77A, 0x2AB7, 1110, 0x22, 7, 0x02, 255, {0, 0}},
	{0x710, 0x77A, 0x4F00, 1117, 0x22, 1, 0x02, 255, {0, 0}},
	{0x710, 0x77A, 0x5505, 1118, 0x22, 1, 0x02, 255, {0, 0}},
	{0x713, 0x77D, 0x2B21, 1119, 0x22, 1, 0x02, 255, {0, 0}},
	{0x713, 0x77D, 0x2B29, 1120, 0x22, 1, 0x02, 255, {0, 0}},
	{0x714, 0x77E, 0x1014, 1121, 0x22, 1, 0x02, 255, {0, 0}},
	{0x714, 0x77E, 0x2203, 1122, 0x22, 1, 0x02, 255, {0, 0}},
	{0x714, 0x77E, 0x220C, 1123, 0x22, 1, 0x02, 255, {0, 0}},
	{0x714, 0x77E, 0x2211, 1124, 0x22, 1, 0x02, 255, {0, 0}},
	{0x714, 0x77E, 0x224B, 1125, 0x22, 1, 0x02, 255, {0, 0}},
	{0x714, 0x77E, 0x22B8, 1126, 0x22, 1, 0x02, 255, {0, 0}},
	{0x746, 0x7B0, 0x2605, 1127, 0x22, 1, 0x02, 255, {0, 0}},
	{0x746, 0x7B0, 0x2606, 1128, 0x22, 1, 0x02, 255, {0, 0}},
	{0x746, 0x7B0, 0x2607, 1129, 0x22, 1, 0x02, 255, {0, 0}},
	{0x746, 0x7B0, 0x2609, 1130, 0x22, 1, 0x02, 255, {0, 0}},
	{0x746, 0x7B0, 0x260A, 1131, 0x22, 1, 0x02, 255, {0, 0}},
	{0x746, 0x7B0, 0x260C, 1132, 0x22, 1, 0x02, 255, {0, 0}},
	{0x746, 0x7B0, 0x260D, 1133, 0x22, 1, 0x02, 255, {0, 0}},
	{0x746, 0x7B0, 0x2612, 1134, 0x22, 1, 0x02, 255, {0, 0}},
	{0x746, 0x7B0, 0x2613, 1135, 0x22, 1, 0x02, 255, {0, 0}},
	{0x746, 0x7B0, 0x263D, 1136, 0x22, 1, 0x02, 255, {0, 0}},
	{0x767, 0x7D1, 0x2431, 1137, 0x22, 2, 0x02, 255, {0, 0}},
	{0x7E0, 0x7E8, 0x28D, 1139, 0x22, 1, 0x02, 255, {0, 0}},
	{0x7E0, 0x7E8, 0x1002, 1140, 0x22, 1, 0x02, 255, {0, 0}},
	{0x7E0, 0x7E8, 0x1004, 1141, 0x22, 1, 0x02, 255, {0, 0}},
	{0x7E0, 0x7E8, 0x101A, 1142, 0x22, 1, 0x02, 255, {0, 0}},
	{0x7E0, 0x7E8, 0x102A, 1143, 0x22, 1, 0x02, 255, {0, 0}},
	{0x7E0, 0x7E8, 0x1065, 1144, 0x22, 1, 0x02, 255, {0, 0}},
	{0x7E0, 0x7E8, 0x10D3, 1145, 0x22, 1, 0x02, 255, {0, 0}},
	{0x7E0, 0x7E8, 0x10D4, 1146, 0x22, 1, 0x02, 255, {0, 0}},
	{0x7E0, 0x7E8, 0x10D5, 1147, 0x22, 1, 0x02, 255, {0, 0}},
	{0x7E0, 0x7E8, 0x10D6, 1148, 0x22, 1, 0x02, 255, {0, 0}},
	{0x7E0, 0x7E8, 0x10E0, 1149, 0x22, 1, 0x02, 255, {0, 0}},
	{0x7E0, 0x7E8, 0x1116, 1150, 0x22, 1, 0x02, 255, {0, 0}},
	{0x7E0, 0x7E8, 0x1125, 1151, 0x22, 1, 0x02, 255, {0, 0}},
	{0x7E0, 0x7E8, 0x1128, 1152, 0x22, 1, 0x02, 255, {0, 0}},
	{0x7E0, 0x7E8, 0x113F, 1153, 0x22, 1, 0x02, 255, {0, 0}},
	{0x7E0, 0x7E8, 0x1158, 1154, 0x22, 1, 0x02, 255, {0, 0}},
	{0x7E0, 0x7E8, 0x1159, 1155, 0x22, 1, 0x02, 255, {0, 0}},
	{0x7E0, 0x7E8, 0x1160, 1156, 0x22, 1, 0x02, 255, {0, 0}},
	{0x7E0, 0x7E8, 0x1189, 1157, 0x22, 1, 0x02, 255, {0, 0}},
	{0x7E0, 0x7E8, 0x118A, 1158, 0x22, 1, 0x02, 255, {0, 0}},
	{0x7E0, 0x7E8, 0x11B9, 1159, 0x22, 1, 0x02, 255, {0, 0}},
	{0x7E0, 0x7E8, 0x11BA, 1160, 0x22, 1, 0x02, 255, {0, 0}},
	{0x7E0, 0x7E8, 0x11BD, 1161, 0x22, 1, 0x02, 255, {0, 0}},
	{0x7E0, 0x7E8, 0x11BE, 1162, 0x22, 1, 0x02, 255, {0, 0}},
	{0x7E0, 0x7E8, 0x11C8, 1163, 0x22, 1, 0x02, 255, {0, 0}},
	{0x7E0, 0x7E8, 0x11CD, 1164, 0x22, 1, 0x04, 255, {0, 0}},
	{0x7E0, 0x7E8, 0x11CE, 1165, 0x22, 1, 0x02, 255, {0, 0}},
	{0x7E0, 0x7E8, 0x11DF, 1166, 0x22, 1, 0x02, 255, {0, 0}},
	{0x7E0, 0x7E8, 0x11F1, 1167, 0x22, 1, 0x02, 255, {0, 0}},
	{0x7E0, 0x7E8, 0x11F3, 1168, 0x22, 1, 0x02, 255, {0, 0}},
	{0x7E0, 0x7E8, 0x1221, 1169, 0x22, 1, 0x02, 255, {0, 0}},
	{0x7E0, 0x7E8, 0x1224, 1170, 0x22, 1, 0x02, 255, {0, 0}},
	{0x7E0, 0x7E8, 0x1225, 1171, 0x22, 1, 0x02, 255, {0, 0}},
	{0x7E0, 0x7E8, 0x1340, 1172, 0x22, 1, 0x02, 255, {0, 0}},
	{0x7E0, 0x7E8, 0x1341, 1173, 0x22, 1, 0x02, 255, {0, 0}},
	{0x7E0, 0x7E8, 0x14CB, 1174, 0x22, 1, 0x02, 255, {0, 0}},
	{0x7E0, 0x7E8, 0x14CE, 1175, 0x22, 1, 0x02, 255, {0, 0}},
	{0x7E0, 0x7E8, 0x14D1, 1176, 0x22, 1, 0x02, 255, {0, 0}},
	{0x7E0, 0x7E8, 0x14F8, 1177, 0x22, 1, 0x02, 255, {0, 0}},
	{0x7E0, 0x7E8, 0x14F9, 1178, 0x22, 1, 0x02, 255, {0, 0}},
	{0x7E0, 0x7E8, 0x15AA, 1179, 0x22, 1, 0x02, 255, {0, 0}},
	{0x7E0, 0x7E8, 0x15D4, 1180, 0x22, 1, 0x02, 255, {0, 0}},
	{0x7E0, 0x7E8, 0x1632, 1181, 0x22, 1, 0x02, 255, {0, 0}},
	{0x7E0, 0x7E8, 0x1644, 1182, 0x22, 1, 0x02, 255, {0, 0}},
	{0x7E0, 0x7E8, 0x167E, 1183, 0x22, 1, 0x02, 255, {0, 0}},
	{0x7E0, 0x7E8, 0x167F, 1184, 0x22, 1, 0x02, 255, {0, 0}},
	{0x7E0, 0x7E8, 0x16F4, 1185, 0x22, 1, 0x02, 255, {0, 0}},
	{0x7E0, 0x7E8, 0x1722, 1186, 0x22, 1, 0x02, 255, {0, 0}},
	{0x7E0, 0x7E8, 0x1723, 1187, 0x22, 1, 0x02, 255, {0, 0}},
	{0x7E0, 0x7E8, 0x1724, 1188, 0x22, 1, 0x02, 255, {0, 0}},
	{0x7E0, 0x7E8, 0x1725, 1189, 0x22, 1, 0x02, 255, {0, 0}},
	{0x7E0, 0x7E8, 0x179C, 1190, 0x22, 1, 0x02, 255, {0, 0}},
	{0x7E0, 0x7E8, 0x2998, 1191, 0x22, 1, 0x02, 255, {0, 0}},
	{0x7E0, 0x7E8, 0x2999, 1192, 0x22, 1, 0x02, 255, {0, 0}},
	{0x7E0, 0x7E8, 0x2CD8, 1193, 0x22, 1, 0x02, 255, {0, 0}},
	{0x7E0, 0x7E8, 0x58D1, 1194, 0x22, 1, 0x02, 255, {0, 0}},
	{0x7E0, 0x7E8, 0xF40C, 1195, 0x22, 1, 0x02, 255, {0, 0}},
	{0x7E0, 0x7E8, 0xF40D, 1196, 0x22, 1, 0x02, 255, {0, 0}},
	{0x7E0, 0x7E8, 0xF41F, 1197, 0x22, 1, 0x02, 255, {0, 0}},
	{0x7E0, 0x7E8, 0xF421, 1198, 0x22, 1, 0x02, 255, {0, 0}},
	{0x7E0, 0x7E8, 0xF430, 1199, 0x22, 1, 0x02, 255, {0, 0}},
	{0x7E0, 0x7E8, 0xF431, 1200, 0x22, 1, 0x02, 255, {0, 0}},
	{0x7E0, 0x7E8, 0xF433, 1201, 0x22, 1, 0x02, 255, {0, 0}},
	{0x7E0, 0x7E8, 0xF43C, 1202, 0x22, 1, 0x02, 255, {0, 0}},
	{0x7E0, 0x7E8, 0xF442, 1203, 0x22, 1, 0x02, 255, {0, 0}},
	{0x7E0, 0x7E8, 0xF446, 1204, 0x22, 1, 0x02, 255, {0, 0}},
	{0x7E0, 0x7E8, 0xF449, 1205, 0x22, 1, 0x02, 255, {0, 0}},
	{0x7E0, 0x7E8, 0xF44A, 1206, 0x22, 1, 0x02, 255, {0, 0}},
	{0x7E1, 0x7E9, 0x301, 1207, 0x22, 1, 0x02, 255, {0, 0}},
	{0x7E1, 0x7E9, 0x302, 1208, 0x22, 1, 0x02, 255, {0, 0}},
	{0x7E1, 0x7E9, 0x303, 1209, 0x22, 1, 0x02, 255, {0, 0}},
	{0x7E1, 0x7E9, 0x304, 1210, 0x22, 1, 0x02, 255, {0, 0}},
	{0x7E1, 0x7E9, 0x305, 1211, 0x22, 1, 0x02, 255, {0, 0}},
	{0x7E1, 0x7E9, 0x805, 1212, 0x22, 4, 0x02, 255, {0, 0}},
	{0x7E1, 0x7E9, 0x1001, 1216, 0x22, 1, 0x02, 255, {0, 0}},
	{0x7E1, 0x7E9, 0x1002, 1217, 0x22, 1, 0x02, 255, {0, 0}},
	{0x7E1, 0x7E9, 0x1003, 1218, 0x22, 1, 0x02, 255, {0, 0}},
	{0x7E1, 0x7E9, 0x1004, 1219, 0x22, 1, 0x02, 255, {0, 0}},
	{0x7E1, 0x7E9, 0x1005, 1220, 0x22, 1, 0x02, 255, {0, 0}},
	{0x7E1, 0x7E9, 0x1010, 1221, 0x22, 1, 0x02, 255, {0, 0}},
	{0x7E1, 0x7E9, 0x1022, 1222, 0x22, 1, 0x02, 255, {0, 0}},
	{0x7E1, 0x7E9, 0x1024, 1223, 0x22, 1, 0x02, 255, {0, 0}},
	{0x7E1, 0x7E9, 0x102B, 1224, 0x22, 1, 0x02, 255, {0, 0}},
	{0x7E1, 0x7E9, 0x102C, 1225, 0x22, 1, 0x02, 255, {0, 0}},
	{0x7E1, 0x7E9, 0x102E, 1226, 0x22, 5, 0x02, 255, {0, 0}},
	{0x7E1, 0x7E9, 0x1033, 1231, 0x22, 1, 0x02, 255, {0, 0}},
	{0x7E1, 0x7E9, 0x103B, 1232, 0x22, 1, 0x02, 255, {0, 0}},
	{0x7E1, 0x7E9, 0x1057, 1233, 0x22, 1, 0x02, 255, {0, 0}},
	{0x7E1, 0x7E9, 0x105A, 1234, 0x22, 1, 0x02, 255, {0, 0}},
	{0x7E1, 0x7E9, 0x105B, 1235, 0x22, 1, 0x02, 255, {0, 0}},
	{0x7E1, 0x7E9, 0x105C, 1236, 0x22, 1, 0x02, 255, {0, 0}},
	{0x7E1, 0x7E9, 0x1062, 1237, 0x22, 1, 0x02, 255, {0, 0}},
	{0x7E1, 0x7E9, 0x1063, 1238, 0x22, 1, 0x02, 255, {0, 0}},
	{0x7E1, 0x7E9, 0x1065, 1239, 0x22, 1, 0x02, 255, {0, 0}},
	{0x7E1, 0x7E9, 0x1066, 1240, 0x22, 1, 0x02, 255, {0, 0}},
	{0x7E1, 0x7E9, 0x1067, 1241, 0x22, 1, 0x02, 255, {0, 0}},
	{0x7E1, 0x7E9, 0x106A, 1242, 0x22, 1, 0x02, 255, {0, 0}},
	{0x7E1, 0x7E9, 0x106C, 1243, 0x22, 1, 0x02, 255, {0, 0}},
	{0x7E1, 0x7E9, 0x1078, 1244, 0x22, 1, 0x02, 255, {0, 0}},
	{0x7E1, 0x7E9, 0x1094, 1245, 0x22, 1, 0x02, 255, {0, 0}},
	{0x7E1, 0x7E9, 0x10CA, 1246, 0x22, 1, 0x02, 255, {0, 0}},
	{0x7E1, 0x7E9, 0x10EA, 1247, 0x22, 1, 0x02, 255, {0, 0}},
	{0x7E1, 0x7E9, 0x10EB, 1248, 0x22, 1, 0x02, 255, {0, 0}},
	{0x7E1, 0x7E9, 0x10ED, 1249, 0x22, 1, 0x02, 255, {0, 0}},
	{0x7E1, 0x7E9, 0x1120, 1250, 0x22, 1, 0x02, 255, {0, 0}},
	{0x7E1, 0x7E9, 0x1121, 1251, 0x22, 1, 0x02, 255, {0, 0}},
	{0x7E1, 0x7E9, 0x1124, 1252, 0x22, 1, 0x02, 255, {0, 0}},
	{0x7E1, 0x7E9, 0x1125, 1253, 0x22, 1, 0x02, 255, {0, 0}},
	{0x7E1, 0x7E9, 0x120F, 1254, 0x22, 1, 0x02, 255, {0, 0}},
	{0x7E1, 0x7E9, 0x132A, 1255, 0x22, 1, 0x02, 255, {0, 0}},
	{0x7E1, 0x7E9, 0x132B, 1256, 0x22, 1, 0x02, 255, {0, 0}},
	{0x7E1, 0x7E9, 0x13CC, 1257, 0x22, 1, 0x02, 255, {0, 0}},
	{0x7E1, 0x7E9, 0x13CD, 1258, 0x22, 1, 0x02, 255, {0, 0}},
	{0x7E1, 0x7E9, 0x1419, 1259, 0x22, 1, 0x02, 255, {0, 0}},
	{0x7E1, 0x7E9, 0x141A, 1260, 0x22, 1, 0x02, 255, {0, 0}},
	{0x7E1, 0x7E9, 0x1426, 1261, 0x22, 1, 0x02, 255, {0, 0}},
	{0x7E1, 0x7E9, 0x18E3, 1262, 0x22, 1, 0x02, 255, {0, 0}},
	{0x7E1, 0x7E9, 0x2002, 1263, 0x22, 1, 0x02, 255, {0, 0}},
	{0x7E1, 0x7E9, 0x2102, 1264, 0x22, 1, 0x02, 255, {0, 0}},
	{0x7E1, 0x7E9, 0x2103, 1265, 0x22, 1, 0x02, 255, {0, 0}},
	{0x7E1, 0x7E9, 0x2104, 1266, 0x22, 1, 0x02, 255, {0, 0}},
	{0x7E1, 0x7E9, 0x2105, 1267, 0x22, 1, 0x02, 255, {0, 0}},
	{0x7E1, 0x7E9, 0x2106, 1268, 0x22, 1, 0x02, 255, {0, 0}},
	{0x7E1, 0x7E9, 0x210F, 1269, 0x22, 1, 0x02, 255, {0, 0}},
	{0x7E1, 0x7E9, 0x2142, 1270, 0x22, 1, 0x02, 255, {0, 0}},
	{0x7E1, 0x7E9, 0x2143, 1271, 0x22, 1, 0x02, 255, {0, 0}},
	{0x7E1, 0x7E9, 0x214C, 1272, 0x22, 1, 0x02, 255, {0, 0}},
	{0x7E1, 0x7E9, 0x214D, 1273, 0x22, 1, 0x02, 255, {0, 0}},
	{0x7E1, 0x7E9, 0x70D6, 1274, 0x22, 1, 0x02, 255, {0, 0}},
	{0x7E1, 0x7E9, 0xF40D, 1275, 0x22, 1, 0x02, 255, {0, 0}},
	{0x7E1, 0x7E9, 0xF449, 1276, 0x22, 1, 0x02, 255, {0, 0}}
};

#endif
