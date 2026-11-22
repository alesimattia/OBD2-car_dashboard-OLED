---
name: feedback_dashboard_content
description: Cosa mostrano BATT e gli indicatori di OLED e web: carica della batteria Li-ion del mild hybrid, mai la tensione della batteria 12 V; nessun indicatore che il cockpit dell'auto mostra già
metadata:
  type: feedback
---

- BATT (casella OLED, "BATTERIA IBRIDA" della dashboard, campo `hybBatt`) = carica della batteria Li-ion 12 V del mild
  hybrid. Finchè non c'è un DID con decodifica verificata resta `N/D`: non sostituirla con la tensione `0x42` nè con la
  carica della batteria AGM del gateway (`2A0C`).
- `0x42` (tensione 12 V) resta solo nella diagnostica web e Serial.
- Niente indicatori che duplicano il quadro strumenti: rifiutato il pallino di stato del mild hybrid, perchè veleggio e
  start-stop si vedono già nel cockpit.

**Why:** scelte dell'utente del 22/11/2026.

**How to apply:** prima di proporre un indicatore nuovo verificare che il quadro non lo mostri già; per BATT la strada è la
prova DID di `pid_scan.ps1` ([[project_scan_state]]).
