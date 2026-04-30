---
name: Limitazioni OBD2 VAG
description: Perché molti PID standard non sono supportati e quali alternative usare per Audi/VW
type: project
---

VAG (Volkswagen Audi Group) limita intenzionalmente i PID OBD2 standard al solo set richiesto per emissioni. Tutto il resto è accessibile solo via protocolli proprietari (KWP2000/UDS) con tool VCDS o OBDeleven.

**Conseguenze pratiche per questo progetto:**
- Niente PID standard per: oil temp, torque diretto, fuel level, transmission temp, gear position
- I sensori climatronic, sensori parcheggio, posizioni finestrini ecc. sono su CAN comfort separato → non accessibili da OBD2 standard
- Per accedere serve hardware aggiuntivo (gateway K-line) o software che parla UDS sui CAN proprietari

**Mode 03 (DTC):** funziona normalmente, supporta single-frame fino a 3 DTC. Per multi-frame serve flow control ISO-TP (gia' implementato in CANbus_conn.ino). ELM327 gestisce ISO-TP automaticamente.

**Conferma diretta dall'utente:** non vuole introdurre comunicazione su CAN comfort/UDS proprietario, accetta i limiti dei 28 PID standard supportati.

**Why:** L'utente ha gia' verificato cosa risponde la sua ECU (vedi DOCS/serial_output.txt). Le limitazioni sono note e accettate.

**How to apply:** Quando viene chiesto "perche' non posso leggere X?" la risposta è quasi sempre "VAG non lo espone via OBD2 standard, serve VCDS/OBDeleven". Non proporre soluzioni che richiedano UDS proprietario o gateway K-line.
