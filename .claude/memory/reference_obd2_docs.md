---
name: Riferimenti documentali OBD2 del progetto
description: Dove vivono le tabelle di verità su PID/Mode/ECU — README §5, pid_descriptions.h, dump ELM327 in DOCS
type: reference
---

Quando serve sapere "quali PID supporta l'A5", "cosa fa ciascun Mode OBD2/UDS", o "qual è il nome di un PID",
consultare in ordine:

1. **[README.md](../../README.md) §5** — verità di progetto:
   - §5.1 elenco PID Mode 01 supportati (variante WIFI, ECU motore `0x7E8`, ~28 PID con formula di conversione)
   - §5.2 placeholder per la variante CANbus (da popolare con dump reale)
   - §5.3.1 PID Mode 01 *non ancora monitorati* ma potenzialmente abilitabili (con ECU di provenienza)
   - §5.3.2 calcoli derivati ottenibili da PID già letti
   - §5.4 riferimento completo Mode OBD2 e UDS con colonne ECU/PID/Standard/Monitored
2. **[pid_descriptions.h](../../pid_descriptions.h)** — database PROGMEM per il firmware:
   `getPIDShortName(pid)` (sigla compatta italiana per la dashboard) e `getPIDFullName(pid)` (descrizione
   estesa per Serial). Copertura ~110 PID Mode 01 standard SAE J1979.
3. **[DOCS/supported_pid_WIFI audi_A5.md](../../DOCS/supported_pid_WIFI%20audi_A5.md)** — dump originale
   ELM327 con le formule complete.

Per UDS, i soli DID standard ISO 14229 affidabili sono `0xF190` (VIN), `0xF18C` (Serial), `0xF187`
(Part Number). Tutto il resto è proprietario VAG e richiede label files VCDS — non documentato qui.
