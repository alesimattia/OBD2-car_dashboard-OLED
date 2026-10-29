---
name: project_architecture
description: Header OBD condivisi (obd_proto/obd_scan/obd_live/obdb_signals) con contratto di trasporto per sketch, scansione completa a passi, lettura live multi-PID, calcoli derivati, display, endpoint web, strumenti in tools/
metadata:
  type: project
---

**Struttura:** due sketch (`CANbus_conn`, `WIFI_conn`) con header condivisi nella root, inclusi via `-I<root>`.
- `obd_proto.h`: primitive pure C++ senza Arduino (bitmap, ISO-TP con ID a 32 bit e flag 29 bit al bit 31 come `mcp_can`,
  righe ELM327, interprete dei segnali OBDb in double, Mode 06 con UASID, OBFCM, `59 01`, date BCD); compila su PC.
- `obdb_signals.h`: tabelle PROGMEM generate da `tools/obdb_select.ps1`, mai modificate a mano.
- `obd_scan.h`: scansione completa come macchina a stati, un passo per giro di `loop()` (`obdProbeStep`), regole failsafe in
  `obdScanRequest`/`obdScanUdsRequest`, nomi ODIS, report Serial, JSON `/scan-data`, flag `obdBusy` e `obdTransportReady`.
  Le fasi `SCAN_*` sono numerate nell'ordine di esecuzione: la regola dei 5 errori di trasporto salta a `phase + 1`.
- `obd_live.h`: blocco "Profilo vettura", richieste al motore su `7E0`, lettura live, DTC dalle centraline che rispondono
  al `0101`, clear DTC, istantanea diagnostica condivisa da `/data` e Serial; calcoli derivati in `obdDiagCompute()`.
- Ogni sketch implementa il trasporto: `obdTransact()` con modo di ricezione (`OBD_RX_OBD11/UDS11/OBD29/UDS29`),
  `obdTransportDiscoveryBurst()` (raffica `3E 00` sul CAN nativo, non disponibile sull'ELM327) e `obdTransportRestore()`.

**Trasporto:** CAN nativo con flow control all'ID dato da `obdFlowControlId()` (risposta − 8 in OBD, ID di richiesta in UDS,
STmin 5 ms in UDS); attese `7F xx 78` rinnovate fino a 10 s complessivi. ELM327 con `ATH1`, `ATSH`/`ATCP`, filtri
`ATCF`/`ATCM` nei modi UDS e `ATAR` per tornare all'automatico, ripiego `ATSTFF` per i cloni che non gestiscono i `78`.

**Lettura live:** una richiesta multi-PID al motore per ciclo; ripiego a PID singolo dopo 3 anomalie di fila (silenzio, NRC,
malformata o con meno PID), confermato con una richiesta singola. Boost `(70 − BARO)/100`, coppia `(62 − 8E)/100 ×
TORQUE_MAX_NM`; senza i PID, BoostModel/TorqueModel. Motore muto per 10 cicli: re-init se CAN in bus-off, altrimenti
schermata "Motore non risponde" e nuovo tentativo dopo 10 s (WiFi: riconnessione all'ELM).

**Pulsante:** durante la scansione un click la interrompe; a fine scansione la pressione lunga (Mode 04) è ignorata finchè
`buttonPoll()` non vede il pulsante rilasciato e per altri 5 s.

**OLED:** caselle BOOST, TEMP, COPPIA, BATT (PID `5B`); pagine DTC con centralina di provenienza; schermata della scansione
con fase e avanzamento.

**Luminosità (`light_sensor.h`, condiviso):** LDR opzionale su A0, filtrato con mediana mobile su 5 campioni e poi EMA
(α 0.15). LDR assente (A0 a massa) e buio totale sono indistinguibili, mentre la presenza si prova: finchè nessuna lettura
filtrata arriva a 12 il contrasto resta a `defaultBrightness` = 128 e le letture non entrano in EMA e calibrazione; dalla
prima lettura ≥ 12 la presenza vale fino al riavvio. Scelta dell'utente: niente sensore I²C, rischio accettato che un filo
interrotto in marcia lasci il display al minimo fino al riavvio. `LDR_ENABLED` a 0 (profilo ESP-01) non legge mai l'ADC.

**Web:** handler senza `handleClient()` annidati e senza bus durante scansione e connessione (`busy`); `/probe?start|stop`
imposta solo lo stato. `/serial-data` aggiorna `obdSerialServedSeq`/`Ms`: la scansione attende se un browser che legge il log
ha più di 2 KB di arretrato. Durante la scansione il SoftAP non scade e la pagina legge il log anche fuori dalla sua tab.

**Strumenti in `tools/`:** `obdb_select.ps1` (selezione OBDb, `obdb_signals.h`, `obdb/selection.json`, `obdb/selection.txt`)
e `pid_scan.ps1` (scansione da PC); nel repository restano solo file usati dall'utente o dal firmware, i test di verifica
stanno nello scratchpad ([[reference_build]]). Lo
script PC legge a runtime con regex le tabelle di `obd_scan.h`, `obd_proto.h` e `obd_live.h`: rinominare o riformattare
`OBD_ODIS_ECUS`, `ODIS_*`, `OBD_IDENT_*`, `OBD_TID_*`, `OBD_IPT_*`, `OBD_UASID_TABLE`, `OBD_UAS_*`, `OBD_LIVE_SLOT_PID` o
`OBD_DIAG_PIDS` lo rompe.

**How to apply:** nuovi PID live negli slot di `obd_live.h`; nuove letture nella scansione completa, in `obd_scan.h` e
`pid_scan.ps1` insieme; nuovi calcoli in `obdDiagCompute()`; dopo modifiche alla selezione rigenerare con `obdb_select.ps1` e
verificare nello scratchpad; mai dati opachi in UI (vedi [[feedback_data_must_be_interpretable]]).
