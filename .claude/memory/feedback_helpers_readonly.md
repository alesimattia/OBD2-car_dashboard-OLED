---
name: feedback_helpers_readonly
description: TorqueModel.h e BoostModel.h sono modelli dell'utente; formule, costanti fisiche e soglie si toccano solo con permesso esplicito
metadata:
  type: feedback
---

`TorqueModel.h` e `BoostModel.h` sono modelli fisici forniti e tarati dall'utente, che li aggiorna esternamente: si usano come
scatole nere e la modalità di calcolo si modifica solo con permesso esplicito.

**Why:** l'utente ha chiesto più volte di non modificare la modalità di calcolo dei due file.

**Ammesso senza permesso:** rimozione di codice duplicato o morto, struttura header-only con `static inline` senza PROGMEM,
righe di include (`EngineConstants.h` si include per nome e lo risolve il `-I` sulla radice del progetto). Formule, costanti
fisiche e soglie restano intatte.

**How to apply:** limitarsi agli errori evidenti; le ottimizzazioni delle chiamate vanno negli sketch, in `obd_live.h` o in
`web_dashboard.h`. Le formule non ancora adeguate al benzina (correzioni rail e VE di `TorqueModel.h`, perdita di carico di
`BoostModel.h`, elencate in README §9) si segnalano all'utente, non si correggono. Vedi [[project_scan_state]].
