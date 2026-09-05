---
name: Decisioni di scope sul firmware OBD2
description: Cosa è esplicitamente fuori scope per il firmware — Mode/protocolli rifiutati e ragioni; easy wins identificati ma non implementati
type: project
---

Decisioni di scope concordate per il firmware OBD2 (sketch CANbus_conn e WIFI_conn).
Non rifare queste discussioni: l'utente ha già rifiutato queste estensioni.

**Why:** evitare di reintrodurre proposte che producono dati non interpretabili o di valore basso,
da cui l'utente ha esplicitamente preso le distanze (vedi [[feedback_data_must_be_interpretable]]).

**How to apply:** quando si propongono nuove feature, NON proporre quanto sotto a meno che l'utente
non lo richieda esplicitamente.

## Esclusioni

- **UDS (Mode `0x10`+)**: brute force `0x22 ReadDataByIdentifier` su 65k DID per ECU è impraticabile in
  tempo, e i DID applicativi VAG sono proprietari (richiedono label files VCDS/ODIS). Solo i DID standard
  ISO 14229 (`0xF190` VIN, `0xF18C` Serial, `0xF187` Part Number) sono decodificabili in chiaro, ma sono
  info statiche di valore basso.
- **Mode 06 (On-board monitoring)**: i Test ID sono opachi senza label costruttore. Lo scaling è standard
  (UASID), ma il significato dei TID no. Escluso.
- **Mode 09 (Vehicle Information)**: VIN, Cal ID, CVN, ECU Name, IPT diesel — l'utente ha valutato non
  utili (statici, non monitorabili live, IPT ritenuto inutile). Non implementare.
- **Mode 08 (Control on-board)**: forza attuatori — pericoloso in firmware always-on, e comunque non
  esposto via OBD2 standard sulla CGKA (VAG usa UDS Mode `0x2F`). Non implementare.
- **Lettura live multi-ECU**: lo scan al boot scopre risposte da tutti gli ECU `0x7E8`-`0x7EF`, ma il
  monitor mode resta **single-ECU su `0x7E8`** (filtro in `readOBD2Response()`). Non estendere senza
  richiesta esplicita. Vedi [[project_scan_state]].

## Easy wins identificati ma NON ancora implementati

- **Mode 07 Pending DTC**: ~30 righe, riusa la logica di `readDTCCodes()` cambiando service ID
  `0x03` → `0x07`. Fornisce early warning prima della spia.
- **Mode 0A Permanent DTC**: ~30 righe, idem cambiando in `0x0A`. Visibilità DTC non cancellabili.
- **Mode 02 Freeze Frame**: ~80 righe, snapshot dei live data al momento di un DTC stored. Richiede DTC
  presenti per essere utile.

Se l'utente in futuro chiede "altre estensioni OBD2", proporre questi nell'ordine elencato.
