---
name: project_scan_state
description: Report reale in DOCS con firmware allineato, cosa verificare sul prossimo report di pid_scan.ps1 (prova DID batteria Li-ion MHEV, DTC 19 02 contro VCDS, sessione estesa, prove DID, marcia 0xA4, porte posteriori, DID sospetti) e procedura per report futuri
metadata:
  type: project
---

Fonti del report, con le stesse fasi: firmware (`/scan`, report su Serial) e `tools/pid_scan.ps1` con vLinker FS
([[reference_vlinker_fs]]), che estrae in più quanto deciso in [[project_scope_obd2]]; report `pid_scan_<data>_<ora>.log`
accanto allo script.

**Report reale:** `DOCS/pid_scan_20261122_195218.log` (pid_scan.ps1, motore al minimo da fermo, liquido 65 °C, 337 s, nessun
errore di trasporto), prodotto da una versione dello script precedente al profilo attuale: stampa un profilo senza `0x63`,
`0x9D`, `0xA4` e `OBD_PHYSICAL_ONLY_ECUS`, e `028D` come 150 °C. Confronto con `DOCS/vcds_autoscan_20261122.log`. Dati della
vettura in [[project_vehicle]].

**Firmware allineato al report (decisioni dell'utente):** profilo di `obd_live.h` con `0x63`, `0x9D`, `0xA4` e
`OBD_PHYSICAL_ONLY_ECUS = {0x7EC, 0}`, coppia di riferimento `0x63` letta live, nessun modello ([[feedback_read_not_estimate]]),
`028D` corretto in Kelvin (24.8 °C), riga ODIS `7E4/7EC`, DTC UDS con una sola `19 02 0D`, eco obbligatoria, BATT `N/D`
([[feedback_dashboard_content]]), F4xx del clima lasciati come sono (scelta dell'utente).

**Da verificare sul prossimo report (pid_scan.ps1 a motore acceso):**
- sezione PROVA DID BATTERIA AL LITIO MHEV: se un DID dà una carica plausibile e coerente, proporre all'utente come portarla
  in BATT (UDS a 29 bit verso `17FC00AE`: sul CAN costa una richiesta, sull'ELM327 ~12 comandi AT, quindi bassa frequenza);
  senza esito BATT resta `N/D`;
- DTC UDS da `19 02 AF`: numeri e stati contro l'autoscan VCDS (17 DTC: gateway 6, quadro 6, ESP 2 con stato `0x20`, porta
  lato guida 1, infotainment 2 con `0x08` e `0x09`); verificare che il decimale coincida con la prima colonna di VCDS
  (es. 12648705 = `0xC10101`, 473088 = `0x073800`); sulla prossima scansione dal firmware, conteggi della `19 02 0D`
  uguali a quelli VCDS per confermati, in attesa e falliti ora (l'ESP, con il solo stato `0x20`, deve dare 0);
- sessione estesa: quali centraline accettano `10 03`, se ci sono riletture in default (sessione sospetta) e se i DID
  verificati rispondono come in default;
- prove DID: candidati positivi (attesi soprattutto i `din` di motore e `70E`), tempo delle prove e dei cambi di sessione;
  `7E4` alias di `713`; CVN contro VCDS (motore `7F90A5B6`, cambio `F0C0F06A`, ESP `B6FF353B`); 24 test Mode 06 sul motore
  più il MID `B2`; DID `1010` del cambio intero (201 byte);
- `0xA4` in marcia (da fermo vale 0, mentre il cambio `210F` e il motore `15AA` riportano la 1ª);
- porte posteriori: VCDS raggiunge BB (J388) e BC (J389), la scansione trova solo `73F/7A9`, che resta muta;
- DID VAG con valori incoerenti al minimo, marcati ⚠️ in README §8.8 con la lettura che li contraddice (`1340`, `11C8`,
  `11F1`, `1004`, `10D5`, `1116`, `1160`, `4F00`, `10EA`/`10EB`/`10ED`, `102B`/`102C`, `2142`/`2143`, clima `F405`/`F40C`):
  l'utente non vuole esclusioni, solo correzioni determinabili ([[feedback_data_must_be_interpretable]]).

**How to apply per un report nuovo:** salvarlo in `DOCS/`, aggiornare README §7.4-7.5 (tabelle centraline OBD e UDS) e §8 (PID e
DID per Mode con i valori letti, [[reference_obd2_docs]]), e sostituire insieme all'utente il blocco "Profilo vettura" di
`obd_live.h` con il profilo suggerito.
