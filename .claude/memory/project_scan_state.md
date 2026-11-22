---
name: project_scan_state
description: Stato dopo la scansione reale del 22/11/2026 (report in DOCS, firmware allineato) e cosa verificare sul prossimo report di pid_scan.ps1: prova DID batteria Li-ion MHEV, conteggi 19 02, marcia 0xA4 in marcia; procedura per report futuri
metadata:
  type: project
---

Fonti del report, con le stesse fasi e regole: firmware (`/scan`, report su Serial) e `tools/pid_scan.ps1` con vLinker FS
([[reference_vlinker_fs]]), report `pid_scan_<data>_<ora>.txt` accanto allo script.

**Report reale:** `DOCS/pid_scan_20261122_195218.txt` (pid_scan.ps1, motore al minimo da fermo, 337 s, nessun errore di
trasporto). Dati della vettura in [[project_vehicle]].

**Applicato il 22/11/2026 (decisioni dell'utente):** profilo di `obd_live.h` da quello scan (con `0x63`, `0x9D`, `0xA4` e
`OBD_PHYSICAL_ONLY_ECUS = {0x7EC, 0}`), coppia di riferimento `0x63` letta live, modelli cancellati
([[feedback_read_not_estimate]]), `028D` corretto in Kelvin, riga ODIS `7E4/7EC`, ripiego `19 02`, eco obbligatoria, BATT
`N/D` ([[feedback_dashboard_content]]), F4xx del clima lasciati come sono (scelta dell'utente).

**Da verificare sul prossimo report (pid_scan.ps1 a motore acceso):**
- sezione PROVA DID BATTERIA AL LITIO MHEV: se un DID dà una carica plausibile e coerente, proporre all'utente come portarla
  in BATT (UDS a 29 bit verso `17FC00AE`: sul CAN costa una richiesta, sull'ELM327 ~12 comandi AT, quindi bassa frequenza);
  senza esito BATT resta `N/D`;
- conteggi DTC UDS via `19 02` sulle 23 centraline che rifiutano `19 01`;
- `0xA4` in marcia (da fermo vale 0, mentre il cambio in UDS riporta la 1ª);
- DID VAG con valori sospetti al minimo (`1340`, `11C8`, `11F1`, `10EA`/`10EB`/`10ED`, `2142`/`2143`, `1004`, `10D5`, `4F00`,
  `1116`, `102B`/`102C`): l'utente non vuole esclusioni, solo correzioni determinabili ([[feedback_data_must_be_interpretable]]).

**How to apply per un report nuovo:** salvarlo in `DOCS/`, aggiornare README §5 (tabelle centraline OBD e UDS, §5.1-5.3) e
sostituire insieme all'utente il blocco "Profilo vettura" di `obd_live.h` con il profilo suggerito.
