---
name: reference_endpoints
description: SoftAP ESP, endpoint web, impostazioni ELM327 WiFi, file DOCS, anteprime offline, come si rigenerano e come si ricatturano gli screenshot del README
metadata:
  type: reference
---

**SoftAP ESP:** SSID `OBD2_UPDATE`, password in `OTA_PASS` degli sketch, IP `192.168.4.1`. Attivo per 3 minuti dal boot
(`OTA_WINDOW_MS`), sospeso finchè un client è connesso e durante la scansione completa, con la finestra che riparte a fine
scansione; dopo la chiusura una riconnessione WiFi non lo riaccende.

**Endpoint:** `/dashboard`, `/scan` (stessa pagina sulla tab "PID supportati"), `/data` (campi `busy` e `scan`),
`/scan-data`, `/probe?start|stop`, `/probe-status`, `/serial-data`, `/debug?on|off`, `/brightness?set=N|auto`, `/clear-dtc`;
OTA `/ota` (WiFi) o `/update` (CAN). Schema JSON in README §11.2-11.4.

**ELM327 WiFi (WIFI_conn):** rete `WiFi_OBDII` aperta, TCP `192.168.0.10:35000`, init `ATZ ATE0 ATL0 ATS0 ATH1`, protocollo
`ATSP6`, `ATSH 7E0`/`7DF` per fisico/funzionale; la scansione completa aggiunge `ATSP7`/`ATCP`, filtri `ATCF`/`ATCM`/`ATAR`
e flow control `ATFCSH`/`ATFCSD`/`ATFCSM1`, e a fine scansione reinizializza l'adattatore.

**Documentazione nel repo:** `DOCS/TJA1050.pdf`, `DOCS/MCP2515-...pdf`, la scheda tecnica Audi
`DOCS/eTD-Audi-A5-Sportback-40-TFSI-S_tronic-150kW-MHEV_240613.pdf` e `DOCS/references.lnk` (file di testo con le fonti web:
su Windows l'estensione `.lnk` non si apre con doppio clic, va aperto con un editor).

**Anteprime offline:** `monitor_preview.html` è `DASHBOARD_HTML` di `web_dashboard.h` con titolo e banner di anteprima più
uno script che simula gli endpoint, compresa una scansione completa con una centralina UDS muta: quando cambia la pagina si
rigenera copiando l'HTML del firmware e tenendo lo script, fine riga CRLF. `ui_preview.html` ha 20 schede OLED. Il README le
mostra con gli screenshot `DOCS/monitor_preview.png` (tab Dashboard) e `DOCS/ui_preview.png`, cliccabili verso la versione
live su raw.githack.com, perchè GitHub non renderizza gli iframe. Gli screenshot si ricatturano quando cambiano le parti
mostrate, con Edge già presente (`C:\Program Files (x86)\Microsoft\Edge\Application\msedge.exe`) in `--headless=new` con
`--screenshot`, `--virtual-time-budget=4000` e `--user-data-dir` nello scratchpad: dashboard a `--window-size=480,1600`; OLED
a `--window-size=1200,940` da una copia di `ui_preview.html` nello scratchpad filtrata sulle schede 7, 10, 11 e 13c. Con il
simulatore che ha timer attivi Edge può non terminare: va lanciato su una copia nello scratchpad e fermato se si blocca.
