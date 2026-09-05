---
name: Stato dello scan multi-ECU e prossimo passo
description: Lo scan al boot è pura discovery dei PID; il monitor live resta single-ECU su 0x7E8. Il prossimo passo dipende dal report dello scan reale dell'utente
type: project
---

Lo scan al boot (`executeScanMode` in [CANbus_conn.ino](../../CANbus_conn/CANbus_conn.ino)) interroga tutti
gli ECU `0x7E8`-`0x7EF` con catena Mode 01 PID `0x00/0x20/.../0xE0` e popola la struct
`ecuResults[MAX_ECUS=4]`. La pagina web `/scan` (e il tab "PID supportati" in `/dashboard`) mostra i PID
supportati con descrizioni testuali da [pid_descriptions.h](../../pid_descriptions.h) — copertura ~110 sigle
SAE J1979.

**Why:** l'utente ha richiesto solo la **discovery diagnostica** dei PID disponibili. Il monitor live
continua a leggere SOLO dall'ECU motore `0x7E8` con il round-robin invariato: nessun PID nuovo viene letto
attivamente come conseguenza dello scan.

**How to apply:**
- Quando l'utente fornirà il report effettivo dello scan dalla sua A5 CGKA reale, decidere **insieme a lui**
  quali PID promuovere nel monitor live. Ha detto esplicitamente "Quando farò uno scan delle ECU e PID
  effettivamente disponibili ti farò sapere come procedere" → attendere il dato reale prima di proporre
  nuove letture.
- I candidati pronti per il monitor (se supportati) sono in §5.3.1 del [README.md](../../README.md):
  `0x5C`, `0x5E`, `0x62`, `0x63`, `0x70`, `0x77`, `0x7C`, ecc.
- Per estendere il monitor a un PID nuovo: aggiungere slot al `PID_SCHEDULE`, variabile globale, decoder,
  esposizione su OLED e dashboard (vedi [[project_architecture]]).
- Per leggere PID da un ECU diverso da `0x7E8` (es. il cambio): serve estendere `readOBD2Response()`
  accettando il response ID specifico, oppure scrivere una variante dedicata. Oggi il filtro è hardcoded.
