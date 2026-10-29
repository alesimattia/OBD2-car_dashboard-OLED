---
name: reference_vlinker_fs
description: Vgate vLinker FS USB per lo scan OBD2 da PC Windows con tools\pid_scan.ps1 (README §5.5): driver FTDI, baud, vincoli USB e seriali; il Ross-Tech HEX-V2 non è utilizzabile da script
metadata:
  type: reference
---

Lo scan OBD2 da PC Windows si fa con il **Vgate vLinker FS USB** e lo script `tools\pid_scan.ps1`, documentato in README §5.5.
Lo script è autonomo, gira con Windows PowerShell 5.1 e usa solo servizi di lettura. Interroga in broadcast su 7DF le bitmap
dei supportati dei Mode 01, 02, 06 e 09 e il nome ECU (Mode 09 InfoType 0A), smista le risposte per ID CAN con `ATH1` e scrive
il report `pid_scan_<data>_<ora>.txt`, con la traccia TX/RX, nella cartella dello script anche dopo un errore o un Ctrl+C.
Interpreta solo il CAN 11 bit (protocolli 6 e 8). È indipendente dalla discovery del firmware in `obd_scan.h`, che riscontra
da PC, e in più legge i supportati dei Mode 02 e 06; non verifica le risposte PID per PID e non legge OBFCM.

**Adattatore**
- Bridge USB FTDI con driver VCP, spesso già presente in Windows: compare come porta COM.
- 115200 baud di fabbrica. Dal firmware 2.2.92 un'utility Vgate può cambiarlo in modo permanente, per questo lo script prova anche 38400, 230400 e 500000.
- A `ATI` risponde come ELM327 v2.2 o v2.3: è un compatibile, non il chip ELM originale. Ha anche comandi proprietari `VT...`.
- Le unità prodotte prima del 7/2/2025 hanno problemi su alcune porte USB 3.0 e vanno collegate tramite un hub USB 2.0.
- La latenza FTDI di default (16 ms) rallenta le risposte ma non le altera.
- La porta COM è esclusiva: con FORScan o VCDS aperti lo script non la apre.

**Seriale**: 8N1 senza handshaking, con RTS e DTR non asseriti, perchè RTS è l'ingresso di
interruzione dell'ELM327, attivo basso. Prima del comando successivo si attende sempre il prompt
`>`. Non si invia mai un CR da solo, perchè l'ELM ripeterebbe l'ultimo comando.

**Ross-Tech HEX-V2**: non è utilizzabile da script. Si presenta come dispositivo HID, senza porta COM
nè set di comandi ELM327, e non ha un driver J2534. Con il solo VCDS i PID OBD2 si leggono dal
pulsante [OBD-II] o dalla Channel Map dell'indirizzo 33.

Collegate: [[reference_endpoints]] per l'adattatore ELM327 WiFi degli sketch, [[project_vehicle]],
[[project_scan_state]].
