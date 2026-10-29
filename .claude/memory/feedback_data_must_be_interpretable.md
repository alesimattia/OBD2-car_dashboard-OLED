---
name: feedback_data_must_be_interpretable
description: Proporre solo dati con decodifica e significato fisico da standard pubblico; in dashboard descrizioni testuali, mai indirizzi hex
metadata:
  type: feedback
---

Proporre solo feature i cui dati siano interpretabili dallo standard pubblico (SAE J1979, ISO 14229) o da fonti chiaramente
affidabili.

**Why:** l'utente ha escluso, con queste motivazioni:
- Mode 06, perchè i TID sono opachi senza le label del costruttore.
- Mode 09 IPT (contatori di completamento dei monitor readiness), pur decodificabili, perchè ritenuti inutili: restano solo
  nella scansione approfondita, mai in dashboard.
- Mode 22 UDS, perchè contiene informazioni proprietarie non interpretabili senza reverse engineering.
- Brute force dei DID a 16 bit, perchè lungo e non interpretabile.
- Indirizzi dei PID in dashboard (per esempio `0x0F`), che non gli interessano.

**How to apply:**
- Prima di proporre un PID, DID o TID per il monitor verificare che formula, unità e significato fisico siano pubblici e chiari.
- Se una proposta è interpretabile solo in parte (per esempio UASID standard ma TID opaco), dichiararlo e aspettarsi un rifiuto.
- In dashboard descrizioni testuali italiane da [pid_descriptions.h](../../pid_descriptions.h), non indirizzi esadecimali.

Vedi anche [[project_scope_obd2]].
