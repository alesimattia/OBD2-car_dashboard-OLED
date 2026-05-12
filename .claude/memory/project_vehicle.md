---
name: Veicolo target
description: Audi A5 B8 2.7 TDI CGKA Multitronic — bus CAN, ECU che rispondono, PID supportati, parametri motore/trasmissione, range tipici
type: project
---

**Veicolo:** Audi A5 B8 (2008) 2.7 TDI **Euro 5**, motore codice **CGKA** (140 kW / 400 Nm), cambio automatico
**Multitronic** (CVT), bus **CAN 11-bit 500 kbps**.

**ECU che rispondono sul bus OBD2 standard:**
- `0x7E8` motore — risponde sempre, espone i ~28 PID Mode 01 elencati sotto
- `0x7E9` cambio Multitronic J217 — sui VAG pre-2010 raramente risponde a Mode 01 standard; i dati ricchi
  vivono solo via UDS Mode `0x22` con label files VCDS proprietari
- `0x7EA`-`0x7EF` (ABS, BCM, ecc.) — praticamente mai rispondono a Mode 01

Essendo **pre-Euro 6** non ci sono SCR/AdBlue né sensori NOx: è il vincolo principale di scope
(vedi [[project_scope_obd2]]).

**PID OBD2 supportati (28 totali, da scan reale):**
0x01, 0x04, 0x05, 0x0B, 0x0C, 0x0D, 0x0F, 0x10, 0x13, 0x1C, 0x1F, 0x20, 0x21, 0x23, 0x24, 0x2C, 0x2D, 0x30, 0x31, 0x33, 0x40, 0x41, 0x42, 0x46, 0x49, 0x4A, 0x4C, 0x4F.

**PID NON supportati** (richiedevano fallback nel codice):
- 0x5C (oil temp) → sostituito con 0x05 (coolant)
- 0x62/0x63 (torque %, torque ref) → sostituiti con TorqueModel basato su load×400Nm
- 0x2F (fuel level) → sostituito con calcolo L/100km da MAF + lambda
- 0x5E (consumo l/h diretto) → non esposto dalla CGKA

**Parametri specifici CGKA:**
- Coppia massima: 400 Nm
- Potenza massima: 140 kW (190 CV)
- Cilindrata: 2.698 L (usata in TorqueModel e BoostModel)
- Pressione rail tipica: 250-400 bar idle, 800-1800 bar carico
- Multitronic CVT: costante K=7.9 per stimare il rapporto (RPM / (speed × K))

**Why:** VAG limita i PID standard OBD2 a quelli minimi richiesti per emissioni. Tutto il resto è proprietario VAG-COM/VCDS. Molti PID tipici di altri veicoli non funzionano qui.

**How to apply:** Prima di proporre PID nuovi, verificare che siano nella lista dei 28 supportati. Per dati non disponibili via OBD2 standard suggerire VCDS/OBDeleven o helper di stima (TorqueModel, BoostModel).
