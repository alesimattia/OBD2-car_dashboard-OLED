---
name: reference_vlinker_fs
description: Vgate vLinker FS USB per la scansione completa da PC Windows con tools\pid_scan.ps1 (README §5.5): cosa fa lo script, comandi ELM/STN usati, driver FTDI, baud, vincoli USB e seriali; il Ross-Tech HEX-V2 non è utilizzabile da script
metadata:
  type: reference
---

La scansione completa da PC Windows si fa con il **Vgate vLinker FS USB** e `tools\pid_scan.ps1` (README §5.5), Windows
PowerShell 5.1 senza dipendenze, file UTF-8 con BOM. Esegue le stesse fasi e regole failsafe del firmware
([[project_scope_obd2]]), legge segnali da `tools\obdb\selection.json` e tabelle dagli header del firmware, e scrive
`pid_scan_<data>_<ora>.txt` nella cartella dello script anche dopo un errore o un Ctrl+C, con riepilogo, profilo suggerito e
traccia TX/RX in coda. Ogni richiesta passa da `Assert-ReadOnlyRequest`, che ammette solo i servizi di lettura e blocca il VIN.
Con dot-source non parte (guard finale): così i test nello scratchpad lo caricano e sostituiscono `Invoke-ElmCommand`
con uno stub.

**Comandi ELM327/STN dello script:** `ATS1` e `ATH1` (riga a 29 bit in 4 token di ID), protocollo trovato con `ATSP0` e poi
fissato, gemello a 29 bit (`ATSP7` per il 6, `ATSP9` per l'8) con `ATCP`; `ATAT0` per le richieste funzionali, `ATAT1` per
le fisiche; in UDS `ATFCSH`, `ATFCSD300005`, `ATFCSM1` e, se `STI` risponde, la coppia registrata con `STCFCPA` o `STCAFCP`.
Si ferma se le righe non portano l'header. Non verificati su adattatore reale: gestione dei `78`, `ATFCSM1`, `ATAT1`,
`ATSP7`, `STCFCPA`.

**Adattatore**
- Bridge USB FTDI con driver VCP, spesso già presente in Windows: compare come porta COM.
- 115200 baud di fabbrica. Dal firmware 2.2.92 un'utility Vgate può cambiarlo in modo permanente, per questo lo script prova anche 38400, 230400 e 500000.
- A `ATI` risponde come ELM327 v2.2 o v2.3: è un compatibile, non il chip ELM originale. Ha anche comandi proprietari `VT...`.
- Le unità prodotte prima del 7/2/2025 hanno problemi su alcune porte USB 3.0 e vanno collegate tramite un hub USB 2.0.
- La latenza FTDI di default (16 ms) rallenta le risposte ma non le altera.
- La porta COM è esclusiva: con FORScan o VCDS aperti lo script non la apre.

**Seriale**: 8N1 senza handshaking, con RTS e DTR non asseriti, perchè RTS è l'ingresso di interruzione dell'ELM327, attivo
basso. Prima del comando successivo si attende sempre il prompt `>`. Non si invia mai un CR da solo, perchè l'ELM
ripeterebbe l'ultimo comando: per interromperlo si invia uno spazio.

**Ross-Tech HEX-V2**: non è utilizzabile da script. Si presenta come dispositivo HID, senza porta COM nè set di comandi
ELM327, e non ha un driver J2534. Con il solo VCDS i PID OBD2 si leggono dal pulsante [OBD-II] o dalla Channel Map
dell'indirizzo 33.

Collegate: [[reference_endpoints]] per l'adattatore ELM327 WiFi degli sketch, [[project_vehicle]], [[project_scan_state]].
