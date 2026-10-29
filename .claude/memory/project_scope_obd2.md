---
name: project_scope_obd2
description: Mode e protocolli dentro e fuori ambito del firmware, con le ragioni dell'utente; easy wins non implementati
metadata:
  type: project
---

Non rifare queste discussioni: l'utente ha già deciso.

**Why:** evitare proposte che producono dati non interpretabili o di valore basso (vedi [[feedback_data_must_be_interpretable]]).

## In ambito
- **Mode 01** su tutte le centraline in discovery; lettura live solo sul motore `0x7E8`.
- **Mode 03 / Mode 04** su tutte le centraline (DTC anche a MIL spenta).
- **Mode 09**: bitmap InfoType, nome ECU `0A`, OBFCM `17` (consumo reale di vita); gli altri InfoType solo nella scansione approfondita.

## Esclusioni
- **UDS (Mode `0x10`+)**: DID applicativi VAG proprietari; brute force dei DID 16 bit escluso per tempo e interpretabilità.
- **Mode 06**: TID opachi senza label costruttore.
- **Mode 08**: forza attuatori, pericoloso in un firmware always-on.
- **Lettura live da centraline diverse dal motore** (es. marcia `A4` dal cambio): non estendere senza richiesta esplicita.

`tools/pid_scan.ps1` legge da PC anche le bitmap dei supportati dei Mode 02 e 06 come sola informazione: non cambia l'ambito
del firmware.

## Easy wins identificati e non implementati
- **Mode 07 Pending DTC**: riusa la lettura Mode 03 cambiando servizio; early warning prima della spia.
- **Mode 0A Permanent DTC**: idem; DTC non cancellabili.
- **Mode 02 Freeze Frame**: snapshot al momento di un DTC.

**How to apply:** a richieste di "altre estensioni OBD2" proporre gli easy wins in quest'ordine.
