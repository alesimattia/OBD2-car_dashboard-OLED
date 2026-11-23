---
name: feedback_softap_password_public
description: OTA_PASS del SoftAP OBD2_UPDATE non è un dato sensibile: si scrive in chiaro in sketch e README, senza segnalarla come credenziale esposta
metadata:
  type: feedback
---

La password del SoftAP (`OTA_PASS` negli sketch, SSID `OBD2_UPDATE`) è pubblica per scelta dell'utente: va riportata in chiaro
in README §1, §4.3 e §5.3 e nei commenti, senza mascherarla nè proporla come correzione di sicurezza.

**Why:** il SoftAP è una rete locale creata dall'ESP a bordo, non raggiungibile da internet; chi vi si collega è lo
sviluppatore stesso, fisicamente vicino all'auto, nella finestra di 3 minuti dopo l'avvio.

**How to apply:** non trattarla come credenziale nelle revisioni nè nelle correzioni collaterali. Il rischio residuo
documentato in README §5.3 resta valido e si cita solo se cambia: l'OTA (`/update`, `/ota`) non ha credenziali proprie,
quindi la password è l'unica barriera contro chi è a portata radio durante la finestra. Vedi [[reference_endpoints]].
