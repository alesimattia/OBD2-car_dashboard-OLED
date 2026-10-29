---
name: project_vehicle
description: Audi A5 F5 B9.5 40 TFSI DMSA mild hybrid 12 V, S-tronic 0CK FWD: dati motore e cambio, centraline OBD osservate sulle B9, PID attesi e assenti, limiti del mild hybrid
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

**Centraline UDS nelle catture OBDb A5 MY2022** (vetture miste, contengono anche `7E5`/`7E6`/`FC00` da ibride plug-in, che
sulla A5 non esistono): motore `7E0`, cambio `7E1`, gateway `710`, clima `746`, quadro `714`, freni `713`; la selezione dei DID
VAG è quasi tutta risposta in quelle catture.

**Centraline OBD sulle B9** (catture OBDb di A4/A5/Q5 2019-2024, nessuna con certezza DMSA):
- `0x7E8` motore: PID completo, fra cui `5C`, `61`/`62`/`63` (320 Nm sulle 40 TFSI), `8E`, `70`, `23` (70-100 bar al minimo), `2F`, `5E`, `9D`, `9E`, `A6`.
- `0x7E9` cambio: solo `01 04 05 0C 0D 41 42 49`.
- `0x7EC` (richiesta `0x7E4`): funzione non identificata, `01 04 05 0C 0D 41 42 46 49`.
- Mai visti sulle B9 termiche: `5B`, `9A`, `A4`, `7A`-`7C`, EGR `2C`/`2D`.
- `0x70` sensore A è pressione assoluta (verificato con BARO sulle catture); l'indirizzamento fisico `7E0` funziona attraverso il gateway; SFD non blocca Mode 01/03/09.

**Fonti nel repo:** in `DOCS/` la scheda eTD Audi e `references.lnk` con le fonti web; il report dello scan reale va salvato
lì ([[project_scan_state]]).

**How to apply:** prima di proporre PID nuovi verificare che il motore li abbia letti nella scansione completa; per coppia e boost la
centralina è la fonte primaria, `TorqueModel.h`/`BoostModel.h` sono il ripiego (vedi [[feedback_helpers_readonly]]).
