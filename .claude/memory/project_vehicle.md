---
name: project_vehicle
description: Audi A5 F5 B9.5 40 TFSI DMSA Euro 6d mild hybrid 12 V, S-tronic 0CK FWD: dati motore e cambio con le fonti in DOCS, centraline OBD e UDS con indirizzi VCDS, PID reali dalla scansione del 22/11/2026, limiti del mild hybrid
metadata:
  type: project
---

**Veicolo:** Audi A5 F5 (B9.5, MLBevo) 40 TFSI, motore **DMSA** (2.0 TFSI EA888 evo4 a ciclo B, 1984 cm³, 82.5 × 92.8 mm,
compressione 12.2, accensione 1-3-4-2, iniezione fino a 350 bar), 150 kW a 4475-6000 rpm, 320 Nm a 1450-4475 rpm, benzina
95 EN 228, GPF, **Euro 6d (EU6 AP, da SSP 683)**: l'Euro 6e della scheda eTD 2024 vale per la produzione successiva. Cambio
**S-tronic DL382 `0CK`**, trazione anteriore, rapporti 3.188 / 2.190 / 1.517 / 1.057 / 0.738 / 0.557 / 0.433, coppia finale
4.234, pneumatico di serie 225/50 R17. Anno modello **2022** (10ª cifra del VIN `N`, produzione maggio 2022): è l'anno usato
per i filtri OBDb.

**Mild hybrid 12 V:** BAS (slave LIN della centralina motore), recupero fino a 5 kW, batteria Li-ion 12 V 10 Ah. Veleggio a
motore spento fra 55 e 160 km/h, start-stop sotto 22 km/h con cambio automatico: la centralina motore risponde con RPM e
carico a zero. Potenza del BAS e carica della batteria Li-ion non sono esposte dall'OBD standard nè da DID pubblici: la
MQ-BMS `17FC00AE` (A1, ODX `EV_BMS12VLiIonSDIAU49X`) risponde in default session ai DID di identificazione `F1xx` e
rifiuta i PID standard `F4xx`/`F6xx`/`F800` (NRC 31), ma un DID di misura della carica si può leggere solo dopo averlo
individuato con la prova DID ([[project_scan_state]]); i DID del gateway `710/77A` (`2A0x`, sensore J367 Hella, sottosistema
del gateway) sono della batteria AGM da 68 Ah. Relè interno della batteria Li-ion chiuso dall'avviamento (SSP A6).

**Fonti in `DOCS/`:** scheda eTD Audi (dati, rapporti, masse, capacità, 3.1 kW elettrici; nessun dato MHEV), `DMSA_engine.pdf`
= SSP 683 (ciclo B, 350 bar, minimo 100 ± 20 bar, EU6 AP; non tratta il mild hybrid), `references.lnk`
con le fonti web, fra cui la cartella stampa Audi A5 2020 su audi.com (unica fonte di 5 kW, 10 Ah, 55-160 e 22 km/h: si
cita col link, senza copiarne il testo nel repo pubblico), `vcds_autoscan_20261122.log` (autoscan VCDS
della stessa sera dello scan) e il report `pid_scan_20261122_195218.log`
([[project_scan_state]]).

**Centraline OBD** (CAN 11/500, nessuna risposta OBD a 29 bit):
- `0x7E8` motore J623 `R4 2.0l TFSI`: 48 PID `01 03 04 05 06 07 0B 0C 0D 0E 0F 11 13 15 1C 1F 21 23 2E 2F 30 31 33 34 3C 41 42 43 44 45 46 47 49 4A 4C 51 56 5E 62 63 67 70 8B 8E 9D 9E A4 A6`; multi-PID a 6 ok.
- Assenti: `10` MAF, `5C` olio, `61`, `24` (lambda solo `34`), `5B`, `9A`, `7A`-`7C`, `2C`/`2D`, `3E`, `4D`, `4E`, `4F`, `48`, `59`, `74`, `77`, `78`.
- `63` = **370 Nm** (non 320); `A4` sul motore, vale 0 da fermo; `8B` = stato rigenerazione GPF; rail `23` 101 bar al minimo.
- Mode 02 dichiarato su tutte ma nessun freeze frame; Mode 06 solo motore (MID 01 02 21 35 3D 41 42 A2-A5, più B2 senza nome
  standard); Mode 09 motore `02 04 06 08 0A 16 17`, cambio ed ESP `04 06 0A`.
- `0x7E9` cambio J217 `2.0T 150kW EU`: `01 04 05 0C 0D 41 42 49`.
- `0x7EC` (richiesta `0x7E4`) = ESP J104 `ESP9 Plus` (`BSCM-BrakeSystem`), stessa centralina di UDS `713/77D`; solo fisico: `01 04 05 0C 0D 41 42 46 49`.
- OBFCM plausibile: distanza di vita = odometro `A6` (~127600 km), 8.26 L/100 km.
- `0x70` sensore A è pressione assoluta; l'indirizzamento fisico `7E0` funziona attraverso il gateway; SFD non blocca Mode 01/03/09.

**Centraline UDS (25, tutte in default session):** 11 bit `70C 70E 710 712 713 714 715 723 73F(muta dopo discovery) 746 74A
74B 74F 753 767 769 773` + `7E0 7E1 7E4`; 29 bit `17FC008B` BCM2, `0096`/`0097` fari LMS, `00A5` ricarica wireless, `00AE`
MQ-BMS batteria 12 V Li-ion. DID VAG letti: motore 55, cambio 61, gateway 19, clima 10, quadro 6, ESP 2, telematica 1.
Indirizzi VCDS per sigla: 16 J527, 09 J519, 19 J533, 44 J500, 03 J104, 17 J285, 15 J234, 6D J605, 08 E87, 42 J386, 52 J387,
A5 R242, 81 J587, 75 J949, 6C J772, 5F J794, 01 motore, 02 cambio; per nome le 29 bit (46 BCM2, D6/D7 fari, DE ricarica,
80 batteria ausiliaria), che VCDS con l'interfaccia dell'utente non raggiunge, e `73F` = BC. VCDS trova anche BB (J388,
porta posteriore guida), che la scansione non vede. Tabella completa in README §7.5, PID e DID per Mode in
README §8.

**How to apply:** prima di proporre PID nuovi verificare che il motore li abbia letti nella scansione completa; boost, coppia,
marcia e consumi si leggono sempre dalla centralina, senza modelli ([[feedback_read_not_estimate]]).
