---
name: project_architecture
description: Header OBD condivisi (obd_proto/obd_scan/obd_live) con trasporto obdTransact per sketch, lettura live multi-PID, calcoli derivati, display, endpoint web
metadata:
  type: project
---

**Struttura:** due sketch (`CANbus_conn`, `WIFI_conn`) con header condivisi nella root, inclusi via `-I<root>`.
- `obd_proto.h`: primitive OBD pure C++ senza Arduino (bitmap, ISO-TP, righe ELM327 con header, decoder DTC/NRC/multi-PID/`70`/OBFCM/nome ECU, tabella lunghezze SAE dei PID); compila anche su PC, quindi è testabile fuori dall'ESP.
- `obd_scan.h`: discovery, scansione approfondita, report Serial, JSON `/scan-data` in chunked encoding, flag `obdBusy`.
- `obd_live.h`: lettura live, valori mostrati, DTC di tutte le centraline, clear DTC, istantanea diagnostica condivisa da `/data` e Serial; i calcoli derivati stanno tutti in `obdDiagCompute()` (formule e campi JSON in README §9).
- Ogni sketch implementa solo `obdTransact()` (trasporto) e `obdShowScanProgress()`, più display, pulsante, OTA, connessione.

**Trasporto:** CAN nativo con flow control `30 00 00` all'ID fisico (risposta − 8), DLC 8, `7F xx 78` estende l'attesa a 5 s;
ELM327 con `ATH1` (riga = ID + PCI + dati), `ATSH` per l'indirizzamento fisico, modalità degradate se il clone rifiuta `ATH1` o `ATSH`.

**Lettura live:** una richiesta multi-PID al motore per ciclo (gruppo veloce boost `70` o MAP, carico, RPM, MAF, coppia `62`;
più un PID lento a rotazione), ripiego a PID singolo se il test allo scan fallisce o dopo 3 risposte malformate. Boost misurato
`(70 − BARO)/100`, coppia `(62 − 8E)/100 × 63`; senza i PID, BoostModel/TorqueModel. Valore N/D dopo 3 letture fallite.

**OLED:** caselle BOOST, TEMP, COPPIA, BATT (PID `5B`); pagine DTC con centralina di provenienza, ridisegnate solo al cambio,
con la lettura OBD che continua.

**Luminosità (`light_sensor.h`, condiviso):** LDR opzionale su A0, filtrato con mediana mobile su 5 campioni e poi EMA
(α 0.15), così i disturbi isolati spariscono e un cambio di luce persistente passa sempre. LDR assente (A0 a massa) e
buio totale sono elettricamente indistinguibili, mentre la presenza si prova: finchè nessuna lettura filtrata arriva a 12 il
contrasto resta a `defaultBrightness` = 128 (50% del registro SH1106) e le letture non entrano in EMA e calibrazione; dalla
prima lettura ≥ 12 la presenza vale fino al riavvio e una lettura vicina a 0 è buio, contrasto al minimo. Scelta
dell'utente fra le alternative: niente sensore I²C al posto dell'LDR; rischio accettato, un filo interrotto in marcia lascia
il display al minimo fino al riavvio. `LDR_ENABLED` a 0 (profilo ESP-01) non legge mai l'ADC. Il fade limita l'ultimo passo
al residuo e termina esattamente sul target. Dashboard: `lightLevel`/`ldr` null e modo `fixed` finchè la presenza non è provata.

**Web:** `/dashboard`, `/scan` (stessa pagina sulla tab PID), `/data`, `/scan-data`, `/probe?start`, `/probe-status`,
`/serial-data`, `/debug`, `/brightness`, `/clear-dtc`; handler senza `handleClient()` annidati e senza bus durante `obdBusy`.
`monitor_preview.html` è `DASHBOARD_HTML` più uno script che simula gli endpoint.

**Strumenti PC:** `tools/pid_scan.ps1` legge da PC le bitmap dei supportati via ELM327 USB, indipendente dal firmware
([[reference_vlinker_fs]]).

**How to apply:** nuovi PID live vanno negli slot di `obd_live.h` (PID, apply, fail); nuove letture una tantum in `obd_scan.h`;
nuovi calcoli in `obdDiagCompute()`; mai dati opachi in UI (vedi [[feedback_data_must_be_interpretable]]).
