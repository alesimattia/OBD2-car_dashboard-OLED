---
name: feedback_read_not_estimate
description: Un parametro che le centraline dell'auto espongono si legge, mai stimato con modelli, ripieghi o costanti assunte; i modelli inutilizzati si cancellano; quali grandezze restano stimate
metadata:
  type: feedback
---

Se una centralina dell'auto espone un parametro, il firmware lo legge: niente modelli, ripieghi stimati o conversioni con
costanti assunte per quel parametro. I modelli rimasti senza uso si cancellano: TorqueModel.h, BoostModel.h ed
EngineConstants.h sono stati cancellati il 22/11/2026 su richiesta dell'utente.

**Why:** l'utente l'ha chiesto più volte nella stessa sessione ("non usare i modelli per i parametri che è possibile leggere
realmente dalle centraline", "cancella i modelli non utilizzati") e ha contestato una bozza che teneva rapporto del cambio e
densità del carburante calcolati.

**How to apply:**
- prima di calcolare una grandezza cercare il PID o DID che la espone sulla vettura ([[project_vehicle]], report in DOCS):
  boost `0x70 − 0x33`, coppia `(0x62 − 0x8E) × 0x63`, carburante `0x5E` (L/h) e `0x9D` (g/s), marcia `0xA4`, taglio
  iniezione da `0x5E` = 0 a motore in rotazione, pressione ambiente solo `0x33`;
- PID mancante → `N/D`, mai un ripiego stimato;
- restano stimate solo grandezze che la vettura non espone: AFR (λ × 14.7), altitudine ISA, efficienza intercooler
  (isoentropica, solo con ΔT teorico ≥ 10 K); calcoli esatti da letture: potenza, L/100 km, BSFC, densità aria, rapporto di
  pressione, deriva pedali, scarto farfalla-pedale, derivate nel tempo;
- quando l'utente chiede l'elenco dei parametri stimati, non includere quelli cancellati.
