---
name: Helper non modificabili
description: TorqueModel.h e BoostModel.h sono forniti dall'utente, modificare solo cio' che è richiesto
type: feedback
---

**Regola:** I file `TorqueModel.h` e `BoostModel.h` sono modelli matematici forniti direttamente dall'utente. Vanno usati come scatole nere — modificare la modalita' di calcolo richiede esplicito permesso.

**Why:** L'utente ha detto piu' volte: "non modificare la modalita' di calcolo di TorqueModel.h" e "non modificare la modalita' di calcolo di BoostModel.h". Sono file che lui stesso aggiorna esternamente con modelli fisici tarati.

Eccezioni concesse:
- E' stato OK pulire BoostModel.h da codice DUPLICATO (era stato incollato due volte per errore)
- E' stato OK rimuovere `estimateTurboAbsolutePressureKpa()` non usata
- E' stato OK ottimizzare struttura (header-only con `static inline`, niente PROGMEM) ma SENZA toccare formule
- Il 25/08/2026 è stato OK sostituire in entrambi l'`#include` di `EngineConstants.h`,
  che portava un path assoluto del Mac, con `#include "EngineConstants.h"`: quel path su
  Windows non esiste e impediva ogni compilazione, mentre il `-I<radice>` già passato da
  `build.extra_flags` lo risolve su entrambe le macchine. Toccata solo la riga di include.

**How to apply:** Quando si lavora su questi file, limitarsi a fix di errori evidenti (duplicazioni, codice morto). Mai modificare formule, costanti fisiche, soglie. Se serve ottimizzare le chiamate, farlo nei .ino o web_dashboard.h, non nei modelli.
