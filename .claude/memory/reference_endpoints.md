---
name: Endpoint e credenziali
description: URL dashboard, OTA, WiFi ELM327, IP statici — riferimenti rapidi
type: reference
---

**SoftAP ESP (creato dallo sketch):**
- SSID: `OBD2_UPDATE`
- Password: `alesimattia`
- IP fisso: `192.168.4.1`
- Attivo solo nei primi 3 minuti dal boot (OTA_WINDOW_MS = 180000)
- Resta attivo finche' un client è connesso (timer si resetta a disconnessione)

**Endpoint web disponibili:**
- `http://192.168.4.1/dashboard` — dashboard live HTML, refresh 500ms
- `http://192.168.4.1/data` — JSON dati live
- `http://192.168.4.1/debug?on` `?off` — toggle diagnostica seriale
- `http://192.168.4.1/ota` — upload OTA (sketch WIFI_conn)
- `http://192.168.4.1/update` — upload OTA (sketch CANbus_conn)

**Adattatore ELM327 WiFi (per WIFI_conn.ino):**
- SSID: `WiFi_OBDII`
- Password: vuota (rete aperta)
- IP TCP: `192.168.0.10`
- Porta: `35000`
- Protocollo: `ATSP6` (CAN 11bit 500kbps, standard VAG B8)

**Documentazione tecnica:**
- `c:\obd\DOCS\TJA1050.pdf` — datasheet transceiver CAN
- `c:\obd\DOCS\MCP2515-Stand-Alone-CAN-Controller-with-SPI-20001801J.pdf` — datasheet MCP2515
- `c:\obd\DOCS\serial_output.txt` — output serial reale dello scan PID dell'auto
- `c:\obd\DOCS\supported_pid audi_A5.md` — lista commentata PID supportati

**Anteprime browser locali (no ESP):**
- `c:\obd\monitor_preview.html` — simulazione dashboard live con dati casuali
- `c:\obd\ui_preview.html` — simulazione schermate display OLED
