---
name: reference_obd2_docs
description: Dove vivono le tabelle di verità su PID, Mode, DID, centraline e calcoli: README §5 e §9, tools/obdb (signalset OBDb e selezione), obd_proto.h, obd_scan.h, pid_descriptions.h, report in DOCS
metadata:
  type: reference
---

Per "quali PID espone l'A5", "cosa fa un Mode", "come si decodifica o si calcola una grandezza" consultare in ordine:

1. **README §5 e §9**: §5 dati vettura, rapporti del cambio, logiche mild hybrid e tabella centraline; §5.1 scansione
   completa (fasi, failsafe, profilo suggerito, OBFCM); §5.2 PID letti dal firmware con formula e uso; §5.3 PID e calcoli
   potenziali non implementati; §5.4 Mode OBD2/UDS con colonna Monitored; §5.5 scansione da PC con `tools/pid_scan.ps1`;
   §9 modelli fisici e calcoli derivati con il campo JSON; §15.1 fonti e licenze.
2. **`tools/obdb/`**: copie dei signalset `SAEJ1979.json` e `Audi-A5.json` e del supporto per anno delle catture A5 ai
   commit fissati in `tools/obdb_select.ps1` (CC-BY-SA-4.0), `selection.txt` con ogni segnale incluso o escluso e il
   motivo, `selection.json`. Le risposte reali con i valori attesi stanno nei casi di test di `OBDb/Audi-A5`
   (`tests/test_cases/<anno>`), da scaricare nello scratchpad quando serve verificare un interprete.
3. **`obd_proto.h`**: lunghezze SAE dei PID Mode 01 (`obdPidDataLen`), tabella UASID del Mode 06 (da python-OBD), decoder.
4. **`obd_scan.h`**: elenco ODIS delle coppie UDS VAG con i nomi italiani, nomi dei monitor Mode 06, TID, contatori IPT e DID
   di identificazione.
5. **`pid_descriptions.h`**: nomi italiani brevi (dashboard) ed estesi (Serial) dei PID Mode 01, 256 voci indicizzate per PID.
6. **`DOCS/`**: scheda tecnica eTD Audi e `references.lnk` con le fonti web; i report della scansione completa, del firmware
   o di `pid_scan.ps1`, vanno salvati qui.

Fonti esterne affidabili usate: signalset e decoder OBDb su GitHub (`OBDb/SAEJ1979`, `Audi-A5`, `Audi-A4`, `Audi-Q5`,
`.schemas`), python-OBD per UASID e OBDMID, `ConnorHowell/vag-uds-ids` per le coppie ODIS, dtcsweep per la discovery UDS VAG,
datasheet ELM327 (Elm Electronics) e STN1100, tabella Wikipedia "OBD-II PIDs", scheda eTD Audi.
