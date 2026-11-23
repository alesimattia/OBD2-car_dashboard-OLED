---
name: project_scope_obd2
description: Ambito deciso dall'utente: boot senza scansione con profilo statico, scansione completa OBD2 + UDS in sola lettura solo dal web, servizi ammessi e vietati, lettura live solo sul motore, eccezioni di estrazione completa solo in pid_scan.ps1 (sessione estesa, prove DID, DTC con numeri)
metadata:
  type: project
---

Non rifare queste discussioni: l'utente ha già deciso.

**Why:** dati solo interpretabili e firmware always-on sicuro (vedi [[feedback_data_must_be_interpretable]]).

## Boot e lettura live
- Al boot nessuna scansione nè discovery, mai: il monitor parte dal blocco "Profilo vettura" di `obd_live.h`
  (`OBD_ENGINE_PIDS`, `OBD_ENGINE_MULTI_PID`), che l'utente codifica a mano dal profilo suggerito dalla scansione completa.
- Lettura live solo sul motore `0x7E8`; dalle altre centraline solo i DTC (Mode 03 a chi risponde al `0101`, in fisico a
  quelle di `OBD_PHYSICAL_ONLY_ECUS`) e il clear DTC (Mode 04 dal pulsante o da `/clear-dtc`, anche in fisico alle
  solo-fisiche). Non estendere la lettura live ad altre centraline senza richiesta esplicita: la carica della batteria Li-ion
  MHEV la porterà in BATT solo un DID verificato ([[project_scan_state]]).

## Scansione completa (firmware dal web, e `tools/pid_scan.ps1` da PC)
- Parte solo dal web (`/scan`), risultati su Serial più riepilogo per nome in `/scan`; si interrompe da web o con un click.
- Servizi ammessi nel firmware: OBD `01 02 03 06 07 09 0A`, OBD a 29 bit solo per vedere se qualcuno risponde, UDS `3E 00`,
  `22`, `19 02` (una sola richiesta con maschera `0D`, conteggi dai byte di stato) e `19 01` solo come ripiego, nella sola
  default session. Il buffer da 72 byte del firmware basta finchè nessun comando selezionato risponde con più di 72 byte
  (il più lungo, PID `82`, ne usa 44): da ricontrollare se cambia la selezione OBDb.
  Mai Mode `04`/`08` dalla scansione, mai UDS `11 14 27 28 2E 2F 31 85` nè `10` (nemmeno `10 02`), mai il VIN (`0902`, `F190`).
- Mode 06 solo per i MID con nome standard; Mode 09 solo `04 08 0A 0B 17`; DID UDS solo identificazione con significato
  pubblico e DID VAG con decodifica OBDb selezionata da `tools/obdb_select.ps1`; brute force dei DID escluso ovunque.
- Failsafe: le centraline che non rispondono si saltano senza bloccare il resto; una risposta positiva vale solo se ripete
  l'identificatore chiesto (DID, sub-funzione, PID).

## Eccezioni solo in `tools/pid_scan.ps1` (estrazione completa per il reverse engineering, firmware invariato)
- Ogni dato una volta sola: nessuna richiesta ripetuta fra sessioni, indirizzi o fasi; Mode 01 resta su tutti i 205 PID
  della selezione, anche non dichiarati (scelta dell'utente).
- Sessione estesa `10 03` prima delle letture e `10 01` alla fine, mai su piantone `70C`, servosterzo `712`, ESP `713`/`7E4`,
  airbag `715`, leva del cambio `753` (`$DefaultSessionOnlyRequestIds`); `74F` inclusa. Rilettura in default solo dei DID
  rifiutati: sempre quelli verificati visti rispondere nelle catture OBDb, tutti se la sessione è sospetta (`F189` rifiutato,
  NRC 7F).
- Prove DID: i DID `Audi-A5` interpretabili ma non verificati per il 2022 (`dbg`, `dbgfilter`, filtro anni) o solo in
  sessione 03 (`din`), dall'array `candidates` di `selection.json`, con valori, verifica, catture e byte grezzi; nessun altro
  signalset. Prova MHEV sulla MQ-BMS `17FC00AE` (028C F45B 7448 1E3D 1E3B 1E3C 2A0B), byte grezzi in sezione separata.
- Stessa centralina su due indirizzi (firma `F187` + `F19E`, sulla vettura ESP `713` e `7E4`): letta una volta, la primaria
  VAG eredita il legame OBD e non rilegge i `F4xx`.
- DTC UDS con una sola `19 02 AF` (bit `20` incluso per i DTC che VCDS mostra con il solo stato `0x20`, bit `10`/`40`
  esclusi), numero in decimale e esadecimale e stato per nome; `19 01` solo come ripiego.
- Mode 06 anche con MID senza nome standard e TID del costruttore (valore, limiti, unità UASID); Mode 09 anche CVN `06`.
- Nessun limite dei buffer del firmware: risposte fino a 4095 byte, tutti i DTC OBD e i test Mode 06, testi interi.
- Anno modello per i filtri OBDb: 2022, parametro del solo generatore.
- Inclusi per scelta dell'utente quattro segnali di confine: Torque transmitting ratio, Cat OSC age factor, Oil pressure
  valve control, Piston cooling nozzles.

**How to apply:** nuove letture vanno nella scansione completa (`obd_scan.h` e `pid_scan.ps1` insieme), mai al boot; prima di
proporre un servizio verificare che non sia fra quelli vietati. Vedi [[project_obd2_vag]] e [[project_scan_state]].
