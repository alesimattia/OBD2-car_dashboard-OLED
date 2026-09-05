---
name: I dati devono essere interpretabili
description: L'utente rifiuta dati grezzi non decodificabili — verificare sempre l'interpretabilità prima di proporre estensioni, e mai indirizzi hex grezzi in dashboard
type: feedback
---

Non proporre feature che producano dati il cui significato non sia direttamente interpretabile dallo standard
pubblico (SAE J1979, ISO 14229) o da fonti chiaramente affidabili.

**Why:** durante il design dello scan multi-ECU l'utente ha esplicitamente escluso, in ordine:
1. Mode 06 perché "TID opachi senza label costruttore"
2. Mode 09 PID 0x0B IPT diesel ("queste informazioni mi sembrano inutili") pur essendo decodificabile — la
   curva di completamento dei monitor readiness non gli interessa
3. Mode 22 UDS perché "contiene informazioni proprietarie non interpretabili senza reverse engineering"
4. PID raw senza descrizione testuale nella dashboard ("nella dashboard web non scrivere gli indirizzi tipo
   0x0F perché non mi interessano")

**How to apply:**
- Prima di proporre l'aggiunta di un PID/DID/TID al monitor, verificare che formula di decodifica e unità di
  misura siano standard pubblici e che il *significato fisico* sia chiaro.
- Se proponi qualcosa parzialmente interpretabile (es. UASID standard ma TID opaco), segnala esplicitamente
  l'incertezza e aspettati un rifiuto.
- Non proporre brute-force scan di range 16-bit DID: escluso "perché impiega molto tempo e non interpretabile".
- Presentazione dei dati: descrizioni testuali in italiano, non indirizzi hex grezzi (database delle sigle in
  [pid_descriptions.h](../../pid_descriptions.h)).

Vedi anche [[project_scope_obd2]].
