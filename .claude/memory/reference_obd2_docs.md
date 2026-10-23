---
name: Riferimenti documentali OBD2 del progetto
description: Dove vivono le tabelle di verità su PID, Mode e centraline: README §5, obd_proto.h, pid_descriptions.h, dump DOCS
metadata:
  type: reference
---

Per "quali PID espone l'A5", "cosa fa un Mode", "come si decodifica un PID" consultare in ordine:

1. **README §5**: §5 tabella centraline B9 da catture OBDb; §5.1 discovery e scansione approfondita; §5.2 PID letti dal
   firmware con formule; §5.3.1 PID potenziali e già letti (✅); §5.4 Mode OBD2/UDS con colonna Monitored; §5.5 stato adattamento.
2. **`obd_proto.h`**: tabella delle lunghezze SAE di ogni PID Mode 01 (`obdPidDataLen`) e decoder di protocollo.
3. **`pid_descriptions.h`**: nomi italiani brevi (dashboard) ed estesi (Serial) dei PID Mode 01, 256 voci indicizzate per PID;
   **`obd_scan.h`** per i nomi delle InfoType Mode 09.
4. **`DOCS/`**: scheda tecnica eTD Audi della vettura e `references.lnk` con le fonti web consultate; il report dello scan reale,
   quando disponibile, va salvato qui.

Fonti esterne affidabili usate: tabella Wikipedia "OBD-II PIDs", datasheet ELM327 (Elm Electronics), catture OBDb su GitHub
(repo `OBDb/Audi-A4`, `Audi-A5`, `Audi-Q5`, `SAEJ1979`), scheda eTD Audi.
