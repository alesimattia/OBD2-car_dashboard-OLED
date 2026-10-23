---
name: Endpoint e credenziali
description: SoftAP ESP, endpoint web, impostazioni ELM327 WiFi, file DOCS e anteprime
metadata:
  type: reference
---

**SoftAP ESP:** SSID `OBD2_UPDATE`, password in `OTA_PASS` degli sketch, IP `192.168.4.1`. Attivo per 3 minuti dal boot
(`OTA_WINDOW_MS`), sospeso finchè un client è connesso; dopo la chiusura una riconnessione WiFi non lo riaccende.

**Endpoint:** `/dashboard`, `/scan` (pagina scan), `/data`, `/scan-data`, `/probe?start`, `/probe-status`, `/serial-data`,
`/debug?on|off`, `/brightness?set=N|auto`, `/clear-dtc`; OTA `/ota` (WiFi) o `/update` (CAN). Dettagli in README §11.2.

**ELM327 WiFi (WIFI_conn):** rete `WiFi_OBDII` aperta, TCP `192.168.0.10:35000`, protocollo `ATSP6`, init `ATE0 ATL0 ATS0 ATH1`,
`ATSH 7E0`/`7DF` per fisico/funzionale.

**Documentazione nel repo:** `DOCS/TJA1050.pdf`, `DOCS/MCP2515-...pdf`, la scheda tecnica Audi
`DOCS/eTD-Audi-A5-Sportback-40-TFSI-S_tronic-150kW-MHEV_240613.pdf` e `DOCS/references.lnk` (file di testo con le fonti web:
su Windows l'estensione `.lnk` non si apre con doppio clic, va aperto con un editor).

**Anteprime offline:** `monitor_preview.html` (dashboard con endpoint simulati), `ui_preview.html` (schermate OLED).
