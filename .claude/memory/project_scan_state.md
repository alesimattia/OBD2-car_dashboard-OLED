---
name: Stato della discovery OBD
description: Discovery multi-centralina condivisa dalle due build (obd_scan.h), scansione approfondita da web, report Serial; in attesa del report reale dall'auto per aggiornare README §5.1 e DOCS
metadata:
  type: project
---

La discovery è in `obd_scan.h`, identica per CAN e WiFi: `0100` in broadcast ripetuto, test dell'indirizzamento fisico, catene
Mode 01 (fino a `E0`) e Mode 09 (fino a `20`) per tutte le centraline insieme con retry e richiesta fisica, nome ECU (InfoType
`0A`), OBFCM (`17`), coppia di riferimento `63` e test multi-PID sul motore. Senza il motore `0x7E8` lo scan si ripete.
La scansione approfondita si avvia solo dal web (`/probe?start`, pagina `/scan`): un passo per giro di loop su ogni PID Mode 01,
InfoType Mode 09 e `0100` fisico a `7E0`-`7E7`; report su Serial (hex e byte grezzi, VIN omesso) e riepilogo per nome nella pagina.

La lettura live resta solo sul motore; dalle altre centraline si leggono solo i DTC.

**Why:** l'utente eseguirà discovery e scansione approfondita sull'auto per stabilire i PID realmente leggibili.

**How to apply:**
- Quando l'utente fornisce il report, salvarlo in `DOCS/`, aggiornare README §5.1-5.2 e decidere **insieme a lui**
  eventuali PID da aggiungere alla lettura live.
- Se il report mostra che `0x62` non c'è, riproporre la domanda su `TorqueModel.h`: `railPressureCorrection()` si aspetta
  250-950 bar, valori incompatibili con l'iniezione diretta benzina, e abbassa la stima di circa il 12%. L'utente ha chiesto
  esplicitamente di essere avvisato. Vedi [[feedback_helpers_readonly]].
- Se `0x70` o `0x63` mancano, il firmware ripiega su BoostModel e su `TORQUE_MAX_NM`.
