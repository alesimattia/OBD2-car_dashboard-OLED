---
name: project_scan_state
description: Procedura quando arriva il report della scansione completa dall'auto (firmware o tools/pid_scan.ps1): DOCS, README §5, codifica del profilo suggerito con l'utente, domanda su TorqueModel se manca 0x62
metadata:
  type: project
---

I dati realmente leggibili sulla vettura si stabiliscono con la scansione completa eseguita sull'auto, da due fonti con le
stesse fasi e regole: il firmware, avviata da `/scan` (report su Serial, su ESP-01 solo dal Serial monitor web con "Salva
log"), e `tools/pid_scan.ps1` con il vLinker FS USB (report `pid_scan_<data>_<ora>.txt` nella cartella dello script,
[[reference_vlinker_fs]]). Entrambi i report finiscono con il "profilo suggerito": `OBD_ENGINE_PIDS` con i PID del motore che
hanno risposto fra quelli di lettura live e diagnostica, `OBD_ENGINE_MULTI_PID` dalla prova a 6 PID, coppia del PID `63` letta.

**Why:** senza un report della vettura, centraline e PID vengono dalle catture OBDb di altre A5 ([[project_vehicle]]) e il
profilo di `obd_live.h` è solo iniziale.

**How to apply:**
- Quando l'utente fornisce un report: salvarlo in `DOCS/`, aggiornare README §5 (tabella centraline, §5.1-5.3) e
  sostituire **insieme all'utente** il blocco "Profilo vettura" di `obd_live.h` con il profilo suggerito.
- Finchè il profilo contiene un PID del gruppo veloce che il motore non espone, ogni risposta multi-PID è un'anomalia e ogni
  3 cicli parte una conferma a PID singolo in più; il profilo suggerito lo elimina.
- Se il report mostra che `0x62` manca, riproporre la domanda su `TorqueModel.h`: `railPressureCorrection()` si aspetta
  250-950 bar, incompatibili con l'iniezione diretta benzina, e abbassa la stima di circa il 12%. L'utente ha chiesto
  esplicitamente di essere avvisato. Vedi [[feedback_helpers_readonly]].
- Se il PID `63` letto differisce da 320 Nm, segnalarlo: la coppia di riferimento live è `TORQUE_MAX_NM`.
- Se mancano `0x70` o `0x62`, il firmware ripiega su BoostModel e TorqueModel.
