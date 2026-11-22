---
name: project_vehicle
description: Audi A5 F5 B9.5 40 TFSI DMSA mild hybrid 12 V, S-tronic 0CK FWD: dati motore e cambio, centraline OBD e UDS e PID reali dalla scansione del 22/11/2026, limiti del mild hybrid
metadata:
  type: project
---

**Veicolo:** Audi A5 F5 (B9.5) 40 TFSI, motore **DMSA** (2.0 TFSI EA888, 1984 cm³, 82.5 × 92.8 mm, compressione 12.2),
150 kW a 4475-6000 rpm, 320 Nm a 1450-4475 rpm, benzina 95 EN 228, GPF, Euro 6d/6e. Cambio **S-tronic DL382 `0CK`**,
trazione anteriore, rapporti 3.188 / 2.190 / 1.517 / 1.057 / 0.738 / 0.557 / 0.433, coppia finale 4.234, pneumatico di serie
225/50 R17. Fonte: scheda tecnica Audi eTD della A5 Sportback 40 TFSI S-tronic 150 kW MHEV. Anno modello **2022** (10ª
cifra del VIN `N`, produzione maggio 2022): è l'anno usato per i filtri OBDb.

**Mild hybrid 12 V:** BAS (slave LIN della centralina motore) e batteria Li-ion 12 V 10 Ah. Veleggio a motore spento fra
55 e 160 km/h, start-stop sotto 22 km/h: la centralina motore risponde con RPM, carico e MAF a zero. Potenza del BAS e carica
della batteria non sono esposte dall'OBD standard; i dati della batteria 12 V stanno nei DID UDS del gateway `710/77A`.

**Centraline OBD della vettura** (scan reale 22/11/2026, [[project_scan_state]]; CAN 11/500, nessuna risposta OBD a 29 bit):
- `0x7E8` motore J623 `R4 2.0l TFSI`: 48 PID `01 03 04 05 06 07 0B 0C 0D 0E 0F 11 13 15 1C 1F 21 23 2E 2F 30 31 33 34 3C 41 42 43 44 45 46 47 49 4A 4C 51 56 5E 62 63 67 70 8B 8E 9D 9E A4 A6`; multi-PID a 6 ok.
- Assenti: `10` MAF, `5C` olio, `61`, `24` (lambda solo `34`), `5B`, `9A`, `7A`-`7C`, `2C`/`2D`, `3E`, `4D`, `4E`, `4F`, `48`, `59`, `74`, `77`, `78`.
- `63` = **370 Nm** (non 320); `A4` sul motore, vale 0 da fermo; `8B` = stato rigenerazione GPF; rail `23` 101 bar al minimo.
- `0x7E9` cambio J217 `2.0T 150kW EU`: `01 04 05 0C 0D 41 42 49`.
- `0x7EC` (richiesta `0x7E4`) = ESP J104 `ESP9 Plus` (`BSCM-BrakeSystem`), stessa centralina di UDS `713/77D`; solo fisico: `01 04 05 0C 0D 41 42 46 49`.
- OBFCM plausibile: distanza di vita = odometro `A6` (~127600 km), 8.26 L/100 km.
- `0x70` sensore A è pressione assoluta; l'indirizzamento fisico `7E0` funziona attraverso il gateway; SFD non blocca Mode 01/03/09.

**Centraline UDS della vettura (25, tutte in default session):** 11 bit `70C 70E 710 712 713 714 715 723 73F(muta dopo
discovery) 746 74A 74B 74F 753 767 769 773` + `7E0 7E1 7E4`; 29 bit `17FC008B` BCM2, `0096`/`0097` fari LMS, `00A5` ricarica
wireless, `00AE` MQ-BMS batteria 12 V Li-ion. DID VAG letti: motore 55, cambio 61, gateway 19, clima 10, quadro 6, ESP 2,
telematica 1. Elenco completo in README §5 "Centraline UDS della vettura".

**Fonti nel repo:** in `DOCS/` la scheda eTD Audi, `references.lnk` con le fonti web e il report dello scan reale
`pid_scan_20261122_195218.txt` ([[project_scan_state]]).

**How to apply:** prima di proporre PID nuovi verificare che il motore li abbia letti nella scansione completa; per coppia e boost la
centralina è la fonte primaria, `TorqueModel.h`/`BoostModel.h` sono il ripiego (vedi [[feedback_helpers_readonly]]).
