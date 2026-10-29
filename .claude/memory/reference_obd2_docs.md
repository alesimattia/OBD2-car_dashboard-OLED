---
name: reference_obd2_docs
description: Dove vivono le tabelle di verità su PID, Mode, centraline e calcoli: README §5 e §9, obd_proto.h, pid_descriptions.h, dump in DOCS
metadata:
  type: reference
---

Per "quali PID espone l'A5", "cosa fa un Mode", "come si decodifica o si calcola una grandezza" consultare in ordine:

1. **README §5 e §9**: §5 dati vettura, rapporti del cambio, logiche mild hybrid e tabella centraline B9 da catture OBDb;
   §5.1 discovery, scansione approfondita e OBFCM; §5.2 PID letti dal firmware con formula e uso; §5.3.1 PID potenziali non
   letti; §5.3.2 calcoli possibili non implementati; §5.4 Mode OBD2/UDS con colonna Monitored; §5.5 scan da PC con
   `tools/pid_scan.ps1`; §9 modelli fisici, formule non adeguate al benzina e calcoli derivati implementati con il campo JSON.
2. **`obd_proto.h`**: tabella delle lunghezze SAE di ogni PID Mode 01 (`obdPidDataLen`) e decoder di protocollo.
3. **`pid_descriptions.h`**: nomi italiani brevi (dashboard) ed estesi (Serial) dei PID Mode 01, 256 voci indicizzate per PID;
   **`obd_scan.h`** per i nomi delle InfoType Mode 09.
4. **`DOCS/`**: scheda tecnica eTD Audi della vettura e `references.lnk` con le fonti web consultate; i report dello scan reale,
   del firmware o di `pid_scan.ps1`, vanno salvati qui.

Fonti esterne affidabili usate: tabella Wikipedia "OBD-II PIDs", datasheet ELM327 (Elm Electronics), catture OBDb su GitHub
(repo `OBDb/Audi-A4`, `Audi-A5`, `Audi-Q5`, `SAEJ1979`), scheda eTD Audi.
