---
name: Architettura sketch
description: Round-robin PID, helper header-only condivisi, web endpoint, struttura comune ai due sketch
type: project
---

**Round-robin PID nei due .ino** (PID_SCHEDULE pesato {0,1,0,1,2,0,1,0,1,3,4}):
- Slot 0 = Boost — legge MAP, baro, MAF, RPM, IAT, load → chiama `Audi27TDI140kW::estimateTurboPressureBar()`
- Slot 1 = Coppia — legge load, RPM, MAF, MAP, IAT, rail, volts, baro → chiama `Audi27TDI140kW::Torque::estimateEngineTorqueNm()`
- Slot 2 = Temp liquido (0x05)
- Slot 3 = EGR (0x2C)
- Slot 4 = Errore EGR (0x2D)

Slot 0 e 1 sono lenti (~400-800ms WiFi) perche' leggono molti PID per i modelli. Slot 2-4 sono veloci (1 PID).

**Helper header-only condivisi (root progetto):**
- `dtc_descriptions.h` — tabella ~45 codici DTC con descrizione italiana in PROGMEM, ricerca binaria
- `TorqueModel.h` — namespace `Audi27TDI140kW::Torque` per stima coppia precisa
- `BoostModel.h` — namespace `Audi27TDI140kW` per stima boost in bar
- `web_dashboard.h` — serve endpoint /dashboard /data /debug. Richiede #define OBD_CONN_WIFI o OBD_CONN_CAN prima dell'include

**Web endpoint disponibili (solo durante finestra OTA):**
- `/dashboard` — pagina HTML con 3 tab (Dashboard, PID supportati, Serial monitor), auto-refresh 500ms
- `/data` — JSON. Quando debugMode=false invia solo i 4 valori principali + DTC (zero query OBD aggiuntive, usa variabili globali). Quando true legge tutti i PID internamente.
- `/debug?on` `/debug?off` — toggle diagnostica
- `/brightness` — stato luminosità (`?set=N` override manuale, `?auto` rientro automatico)
- `/clear-dtc` — invia OBD2 Mode 04
- `/serial-data?since=N` — buffer circolare WebSerial (4 KB) come text/plain con cursor `X-Seq` e `X-Dropped`; alimenta la tab "Serial monitor", polling 250 ms
- `/scan` `/scan-data` — solo build CAN (multi-ECU); alimentano la tab "PID supportati"
- `/ota` (WiFi) o `/update` (CAN) — upload firmware
- SSID SoftAP: `OBD2_UPDATE`, password `alesimattia`, IP `192.168.4.1`

**Variabili globali condivise tra .ino e web_dashboard.h** (dichiarate `extern` nel .h):
boostBar, coolantC, torqueNm, loadPct, egrPct, egrErrPct, milOn, dtcCount, dtcCodes[], debugMode

**Dashboard HTML:** sezioni extra hanno `class="s debug"` (display:none di default). JS le mostra/nasconde con `querySelectorAll('.debug')` in base a `d.debug` dal JSON. Sempre visibili: dati principali, DTC.

**OTA:** SoftAP attivo solo nei primi `OTA_WINDOW_MS` (180000 = 3 min). Se un client si connette il timer si sospende; quando si disconnette il timer riparte da capo. debugMode persistente in RAM finche' ESP non riavviato.

**Layout display landscape 128x64:**
- Font: `u8g2_font_7x14B_tr` (14px, monospace)
- RIGHT_COL_X = 100 (BOOST/TEMP a sinistra, COPPIA/EGR a destra ma con allineamento dinamico se la stringa eccede 28px disponibili)
- Coordinate: y=0 label1, y=15 value1, y=35 label2, y=50 value2

**Why:** Architettura comune ai due sketch per facilitare manutenzione. Slot 0/1 lenti perche' hanno bisogno di molti PID per stima accurata via modelli fisici.

**How to apply:** Per aggiungere un nuovo valore al display, aggiungerlo al round-robin (nuovo slot o estendere uno esistente) e aggiornare drawValue/clearValueArea. Per la dashboard web, aggiungere campo al JSON in web_dashboard.h e relativo aggiornamento JS.
