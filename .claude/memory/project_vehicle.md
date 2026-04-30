---
name: Veicolo target
description: Audi A5 B8 2.7 TDI CGKA Multitronic — PID supportati, parametri motore/trasmissione, range tipici
type: project
---

**Veicolo:** Audi A5 B8 2.7 TDI Euro5, motore codice **CGKA** (140kW), cambio automatico **Multitronic** (CVT).

**PID OBD2 supportati (28 totali, da scan reale):**
0x01, 0x04, 0x05, 0x0B, 0x0C, 0x0D, 0x0F, 0x10, 0x13, 0x1C, 0x1F, 0x20, 0x21, 0x23, 0x24, 0x2C, 0x2D, 0x30, 0x31, 0x33, 0x40, 0x41, 0x42, 0x46, 0x49, 0x4A, 0x4C, 0x4F.

**PID NON supportati** (richiedevano fallback nel codice):
- 0x5C (oil temp) → sostituito con 0x05 (coolant)
- 0x62/0x63 (torque %, torque ref) → sostituiti con TorqueEstimator basato su load×400Nm
- 0x2F (fuel level) → sostituito con calcolo L/100km da MAF + lambda

**Parametri specifici CGKA:**
- Coppia massima: 400 Nm
- Potenza massima: 140 kW (190 CV)
- Cilindrata: 2.698 L (usata in TorqueEstimator e BoostModel)
- Pressione rail tipica: 250-400 bar idle, 800-1800 bar carico
- Multitronic CVT: costante K=7.9 per stimare il rapporto (RPM / (speed × K))

**Why:** VAG limita i PID standard OBD2 a quelli minimi richiesti per emissioni. Tutto il resto è proprietario VAG-COM/VCDS. Molti PID tipici di altri veicoli non funzionano qui.

**How to apply:** Prima di proporre PID nuovi, verificare che siano nella lista dei 28 supportati. Per dati non disponibili via OBD2 standard suggerire VCDS/OBDeleven o helper di stima (TorqueEstimator, BoostModel).
