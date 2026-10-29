---
name: project_scope_obd2
description: Ambito deciso dall'utente: boot senza scansione con profilo statico, scansione completa OBD2 + UDS in sola lettura solo dal web, servizi ammessi e vietati, lettura live solo sul motore
metadata:
  type: project
---

Non rifare queste discussioni: l'utente ha già deciso.

**Why:** dati solo interpretabili e firmware always-on sicuro (vedi [[feedback_data_must_be_interpretable]]).

## Boot e lettura live
- Al boot nessuna scansione nè discovery, mai: il monitor parte dal blocco "Profilo vettura" di `obd_live.h`
  (`OBD_ENGINE_PIDS`, `OBD_ENGINE_MULTI_PID`), che l'utente codifica a mano dal profilo suggerito dalla scansione completa.
- Lettura live solo sul motore `0x7E8`; dalle altre centraline solo i DTC (Mode 03 a chi risponde al `0101`) e il clear DTC
  (Mode 04 dal pulsante o da `/clear-dtc`). Non estendere la lettura live ad altre centraline senza richiesta esplicita.

## Scansione completa (firmware dal web, e `tools/pid_scan.ps1` da PC)
- Parte solo dal web (`/scan`), risultati su Serial più riepilogo per nome in `/scan`; si interrompe da web o con un click.
- Servizi ammessi: OBD `01 02 03 06 07 09 0A`, OBD a 29 bit solo per vedere se qualcuno risponde, UDS `3E 00`, `22` e
  `19 01` nella sola default session. Mai Mode `04`/`08` dalla scansione, mai UDS `10 11 14 27 28 2E 2F 31 85`, mai il VIN
  (`0902`, `F190`).
- Mode 06 solo per i MID con nome standard; Mode 09 solo `04 08 0A 0B 17`; DID UDS solo identificazione con significato
  pubblico e DID VAG con decodifica OBDb selezionata da `tools/obdb_select.ps1`; brute force dei DID escluso.
- Failsafe: le centraline che non rispondono si saltano senza bloccare il resto.
- Anno modello per i filtri OBDb: 2022, parametro del solo generatore.
- Inclusi per scelta dell'utente quattro segnali di confine: Torque transmitting ratio, Cat OSC age factor, Oil pressure
  valve control, Piston cooling nozzles.

**How to apply:** nuove letture vanno nella scansione completa (`obd_scan.h` e `pid_scan.ps1` insieme), mai al boot; prima di
proporre un servizio verificare che non sia fra quelli vietati. Vedi [[project_obd2_vag]] e [[project_scan_state]].
