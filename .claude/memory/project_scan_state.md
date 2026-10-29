---
name: project_scan_state
description: Procedura quando arriva il report dello scan reale dall'auto (firmware o tools/pid_scan.ps1): DOCS, README §5, PID live da decidere insieme, domanda su TorqueModel se manca 0x62
metadata:
  type: project
---

I PID realmente leggibili sulla vettura si stabiliscono con un report eseguito sull'auto, da due fonti:
- **firmware**: discovery al boot in `obd_scan.h`, identica per CAN e WiFi (`0100` in broadcast ripetuto, test
  dell'indirizzamento fisico, catene Mode 01 fino a `E0` e Mode 09 fino a `20` con retry e richiesta fisica, nome ECU `0A`,
  OBFCM `17`, coppia di riferimento `63`, test multi-PID sul motore; senza il motore `0x7E8` lo scan si ripete), più la
  scansione approfondita avviata dal web (`/probe?start`), che verifica ogni PID Mode 01, InfoType Mode 09 e `0100` fisico a
  `7E0`-`7E7` e stampa su Serial byte grezzi e riepilogo, VIN omesso (README §5.1);
- **PC**: `tools/pid_scan.ps1` con il vLinker FS USB, report `pid_scan_<data>_<ora>.txt` nella cartella dello script, con le
  sole bitmap dei supportati dei Mode 01, 02, 06 e 09 e i nomi ECU (README §5.5, [[reference_vlinker_fs]]).

La lettura live resta solo sul motore; dalle altre centraline si leggono solo i DTC.

**Why:** senza un report della vettura, centraline e PID nel README vengono dalle catture OBDb di altre B9 ([[project_vehicle]]).

**How to apply:**
- Quando l'utente fornisce un report: salvarlo in `DOCS/`, aggiornare README §5 (tabella centraline, §5.1-5.3) e decidere
  **insieme all'utente** gli eventuali PID da aggiungere alla lettura live.
- Il report di `pid_scan.ps1` dice quali PID sono dichiarati, non che rispondono: la conferma viene dalla scansione approfondita.
- Se il report mostra che `0x62` manca, riproporre la domanda su `TorqueModel.h`: `railPressureCorrection()` si aspetta
  250-950 bar, incompatibili con l'iniezione diretta benzina, e abbassa la stima di circa il 12%. L'utente ha chiesto
  esplicitamente di essere avvisato. Vedi [[feedback_helpers_readonly]].
- Se mancano `0x70` o `0x63`, il firmware ripiega su BoostModel e su `TORQUE_MAX_NM`.
