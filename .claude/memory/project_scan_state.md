---
name: project_scan_state
description: Stato della scansione completa sull'auto (report 22/11/2026 in DOCS, README §5 aggiornato) e decisioni aperte con l'utente: profilo obd_live.h, coppia di riferimento 370 Nm, casella BATT, DID VAG non plausibili; procedura per report futuri
metadata:
  type: project
---

Fonti del report, con le stesse fasi e regole: firmware (`/scan`, report su Serial) e `tools/pid_scan.ps1` con vLinker FS
([[reference_vlinker_fs]]), report `pid_scan_<data>_<ora>.txt` accanto allo script.

**Report reale:** `DOCS/pid_scan_20261122_195218.txt` (pid_scan.ps1, motore al minimo da fermo, 337 s, nessun errore di
trasporto). README §5, §5.1-5.3, §5.5, §8, §13, §14 già allineati. Dati della vettura in [[project_vehicle]].

**Decisioni aperte (chiedere all'utente, non applicare da soli):**
- Profilo suggerito non ancora incollato in `obd_live.h`: `{0x70,0x0B,0x04,0x0C,0x62,0x0F,0x33,0x23,0x42,0x05,0x8E,0x0D,0x34,0x44,0x46,0x49,0x4A,0x4C,0x01,0x1F,0x21,0x30,0x5E}`, MULTI_PID true. Il profilo attuale ha `0x10` (gruppo veloce → anomalia multi-PID a ogni ciclo) e `0x5C`, assenti sull'auto. Dopo la sostituzione aggiornare README (nota Stato riga 5, §5.1, §14 "Profilo vettura").
- PID `0x63` = 370 Nm, `obdReferenceTorqueNm()` in obd_live.h restituisce `TORQUE_MAX_NM` 320 → coppia letta −13.5%. Proposta: riferimento OBD separato a 370 senza toccare `TORQUE_MAX_NM` (lo usa TorqueModel, [[feedback_helpers_readonly]]); anche il commento di obd_live.h ("pari al PID 0x63 delle 40 TFSI") è falso.
- `0x5B` assente → casella BATT sempre N/D; alternative possibili: SoC batteria avviamento dal gateway DID `2A0C` (UDS) o `0x42`.
- DID VAG non plausibili al minimo (motore `1340`, `028D`, `11C8`, `11F1`; cambio `10EA`, `10EB`, `2142`/`2143`; clima `F405`/`F40C`): candidati all'esclusione in `tools/obdb_select.ps1` ([[feedback_data_must_be_interpretable]]).
- 7E4/7EC (ESP) fuori dall'elenco ODIS di obd_scan.h: nel report compare come "Centralina senza nome 20" finchè non arriva `F197`.

**How to apply per un report nuovo:** salvarlo in `DOCS/`, aggiornare README §5 (tabelle centraline OBD e UDS, §5.1-5.3);
profilo in `obd_live.h` solo insieme all'utente; se manca `0x62` riproporre la domanda su `TorqueModel.h`
(`railPressureCorrection()` attende 250-950 bar, incompatibile con l'iniezione diretta benzina); se `0x70` o `0x62` mancano
il firmware ripiega su BoostModel/TorqueModel.
