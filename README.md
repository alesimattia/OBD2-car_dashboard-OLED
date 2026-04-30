# OBD2 Car Dashboard — OLED

Dashboard motore in tempo reale per **Audi A5 B8 2.7 TDI (CGKA, 140 kW) Multitronic**, basata su ESP8266 e display OLED SH1106 1.3" (128×64). Il progetto offre **due sketch alternativi** che condividono dashboard, modelli fisici e web server, e si differenziano solo per il transport verso l'ECU.

> ℹ️ **Anteprime live (interattive)**: senza clonare la repo puoi vederle direttamente nel browser tramite il proxy raw.githack.com:
> - 🖥️ Display OLED → [ui_preview.html (live)](https://raw.githack.com/alesimattia/OBD2-car_dashboard-OLED/main/ui_preview.html)
> - 🌐 Dashboard web → [monitor_preview.html (live)](https://raw.githack.com/alesimattia/OBD2-car_dashboard-OLED/main/monitor_preview.html)
>
> Le stesse anteprime sono embeddate via `iframe` nelle §4.1 e §11.6, ma GitHub non renderizza gli iframe nei README per motivi di sicurezza: l'embed funziona solo se apri questo file in locale.

---

## 📋 Indice

1. [Panoramica](#1-panoramica)
2. [Veicolo target](#2-veicolo-target)
3. [Le due varianti — confronto rapido](#3-le-due-varianti--confronto-rapido)
4. [Architettura](#4-architettura)
   - [4.1 Anteprima offline del display OLED](#41-anteprima-offline-del-display-oled)
5. [Hardware e cablaggio](#5-hardware-e-cablaggio)
   - [5.1 Componenti comuni](#51-componenti-comuni)
   - [5.2 Variante CANbus_conn](#52-variante-canbus_conn)
   - [5.3 Variante WIFI_conn](#53-variante-wifi_conn)
6. [Software & build](#6-software--build)
7. [Configurazione](#7-configurazione)
8. [Round-robin PID schedule](#8-round-robin-pid-schedule)
9. [Modelli fisici condivisi](#9-modelli-fisici-condivisi)
10. [🔘 Pulsante fisico (cancella DTC)](#10--pulsante-fisico-cancella-dtc)
11. [🌐 Interfaccia web `/dashboard`](#11--interfaccia-web-dashboard)
12. [💡 Auto-brightness OLED](#12--auto-brightness-oled)
13. [⚠️ DTC — codici di errore](#13-️-dtc--codici-di-errore)
14. [📁 Layout file](#14-layout-file)
15. [Note operative & limiti](#15-note-operative--limiti)
16. [Licenza](#16-licenza)

---

## 1. Panoramica

Entrambi gli sketch:

- Eseguono al boot una scansione completa dei PID Mode 01 supportati dalla centralina.
- Mostrano sull'OLED 4 metriche live (**BOOST**, **TEMP**, **COPPIA**, **EGR**) con aggiornamento parziale del framebuffer.
- Espongono via SoftAP un **web dashboard** (`/dashboard`) con 47 parametri e refresh JSON ogni 500 ms.
- Leggono e cancellano i **DTC** (Mode 03 / Mode 04), mostrandoli paginati sul display.
- Supportano **OTA** firmware via browser su `http://192.168.4.1`.
- Sono pilotabili da un **pulsante fisico** per ciclare le schermate e cancellare i DTC.
- Regolano automaticamente la **luminosità dell'OLED** in base alla luce ambientale (LDR su ADC), con override manuale dal web dashboard.

---

## 2. Veicolo target

| Parametro | Valore |
|---|---|
| Modello | Audi A5 B8 (2008) |
| Motore | 2.7 TDI Euro 5 — codice **CGKA** |
| Potenza / Coppia | **140 kW (190 CV)** / **400 Nm** |
| Cambio | Multitronic (CVT) |
| Cilindrata | 2.698 L (`ENGINE_DISPLACEMENT_M3 = 0.002698`) |
| Plateau coppia | 1400–3250 rpm |
| Bus diagnostica | **CAN 11 bit a 500 kbps** (gruppo VAG / OBD2) |

> ⚠️ VAG espone solo i PID Mode 01 minimi richiesti per le emissioni: PID specifici come `0x5C` (oil temp), `0x62/0x63` (torque), `0x2F` (fuel level) **non sono disponibili** sulla CGKA. Per ovviare il firmware usa **modelli di stima** basati su PID standard (vedi §9).

---

## 3. Le due varianti — confronto rapido

| Caratteristica | [`CANbus_conn/`](CANbus_conn/) | [`WIFI_conn/`](WIFI_conn/) |
|---|---|---|
| **Transport ECU** | CAN nativo via MCP2515/TJA1050 (SPI) | TCP via adattatore ELM327 WiFi |
| **Hardware extra** | Modulo MCP2515 + TJA1050 (8 o 16 MHz) | Adattatore ELM327 WiFi commerciale |
| **Refresh OBD** | **~8–10 Hz** | ~2 Hz |
| **Latenza tipica per PID** | ~10 ms | ~50–100 ms |
| **Cablaggio** | SPI + I²C + OBD2 connector | Solo I²C + alimentazione |
| **Costo aggiuntivo** | ~5 € (MCP2515) | ~10–15 € (ELM327 WiFi) |
| **Affidabilità segnale** | Alta (CAN nativo) | Dipende da qualità ELM327 |
| **WiFi mode** | `WIFI_AP` (solo SoftAP per OTA/dashboard) | `WIFI_AP_STA` (STA verso ELM327 + AP per OTA/dashboard) |
| **Endpoint OTA** | `/update` | `/ota` |
| **Compatibile ESP-01** | ❌ No (mancano pin SPI) | ✅ Sì |

**Quale scegliere?** Per uso quotidiano con refresh elevato e affidabilità: **CANbus_conn**. Per un setup veloce senza saldature, riusando un ELM327 esistente: **WIFI_conn**.

---

## 4. Architettura

```
                   ┌──────────────────────────────────┐
                   │         ESP8266 D1 Mini          │
                   │                                  │
   CAN 500 kbps    │  ┌────────────┐   ┌───────────┐  │  I²C 400 kHz
   (OBD2 pin 6/14)─┼──┤  MCP2515   │   │   U8g2    ├──┼─────► OLED SH1106 128×64
                   │  └────┬───────┘   └─────┬─────┘  │
                   │       │ SPI             │        │
                   │  ┌────▼─────────────────▼─────┐  │
                   │  │    Round-robin PID         │  │
                   │  │ (MAP/LOAD/RPM/MAF×3,       │  │
                   │  │  IAT/BARO/RAIL/VOLTS×1,    │  │
                   │  │  COOL/EGR/EGR_ERR×1)       │  │
                   │  └────┬──────────────────┬────┘  │
                   │       │ cache PID        │       │
                   │  ┌────▼─────┐  ┌─────────▼────┐  │
                   │  │  Boost   │  │   Torque     │  │
                   │  │  Model   │  │  Estimator   │  │
                   │  └──────────┘  └──────────────┘  │
                   │                                  │
                   │  ┌─── HTTP server (port 80) ───┐ │
                   │  │ /dashboard /data /debug     │ │
                   │  │ /clear-dtc /brightness      │ │
                   │  │ /update (CAN) /ota (WiFi)   │ │
                   │  └──────────────────────────────┘ │
                   └──────────────────────────────────┘
                              ▲ SoftAP "OBD2_UPDATE"
                              ▼
                          smartphone
```

Nello sketch **WIFI_conn** il blocco MCP2515 è sostituito da un `WiFiClient` TCP verso `192.168.0.10:35000` (porta dell'ELM327); l'ESP8266 lavora in `WIFI_AP_STA` (STA verso l'ELM327 + AP per l'OTA/dashboard).

### 4.1 Anteprima offline del display OLED

Per vedere come si presentano le schermate del display OLED senza ESP, apri [`ui_preview.html`](ui_preview.html) nel browser: simula a rotazione le pagine Monitor / DTC con valori realistici.

▶️ **Versione live nel browser** (senza clonare la repo): [ui_preview.html via raw.githack.com](https://raw.githack.com/alesimattia/OBD2-car_dashboard-OLED/main/ui_preview.html)

<p align="center">
  <iframe src="ui_preview.html" width="700" height="500" frameborder="0"
          style="border:1px solid #444;border-radius:8px;background:#111">
    Il tuo browser non supporta iframe.
    <a href="ui_preview.html">Apri ui_preview.html</a>
  </iframe>
</p>

> ⚠️ GitHub non renderizza gli `iframe` nei README: l'embed sopra funziona solo se apri questo file in locale. Su GitHub usa il link raw.githack.com qui sopra.

---

## 5. Hardware e cablaggio

### 5.1 Componenti comuni

| Componente | Note |
|---|---|
| ESP8266 **D1 Mini** (clone) | 4 MB flash, partizione `4M (no FS)` |
| **OLED SH1106 1.3" I²C** | 128×64 monocromo, indirizzo `0x3C` |
| Pulsante NA verso GND | Debounce software 50 ms |
| Fotoresistore GL5528 + 10 kΩ pull-down | Voltage divider su `A0` per auto-brightness |

### 5.2 Variante `CANbus_conn`

Componenti aggiuntivi:
- Modulo **MCP2515 + TJA1050** con cristallo 8 MHz (configurabile a 16 MHz via `#define MCP2515_CRYSTAL`)
- Cavo OBD2 con accesso a CAN-H (pin 6) / CAN-L (pin 14) / GND (pin 4 o 5)

Cablaggio:

| Segnale | ESP8266 | MCP2515 / OLED / OBD2 |
|---|---|---|
| MCP2515 VCC | 5 V | VCC |
| MCP2515 GND | GND | GND |
| MCP2515 CS  | **D8** (GPIO15) | CS |
| MCP2515 SO  | **D6** (GPIO12) | SO  *(MISO)* |
| MCP2515 SI  | **D7** (GPIO13) | SI  *(MOSI)* |
| MCP2515 SCK | **D5** (GPIO14) | SCK |
| MCP2515 INT | **D0** (GPIO16) | INT (non usato in polling) |
| OLED VCC | 3.3 V | VCC |
| OLED GND | GND | GND |
| OLED SCL | **D1** (GPIO5) | SCL |
| OLED SDA | **D2** (GPIO4) | SDA |
| OBD2 CAN-H | — | MCP2515 CANH ↔ pin 6 OBD2 |
| OBD2 CAN-L | — | MCP2515 CANL ↔ pin 14 OBD2 |
| GND OBD2 | GND | pin 4/5 OBD2 |
| Pulsante | **D3** (GPIO0) | altro lato → GND |
| LDR (lato luce) | 3.3 V | LDR |
| LDR (giunzione) | **A0** | LDR ↔ resistore 10 kΩ ↔ GND |

> ⚠️ Il pin **D3 (GPIO0)** è anche pin di boot dell'ESP8266: il pulsante deve restare *aperto* al power-on, altrimenti l'ESP entra in flash mode. Lo schema NA + `INPUT_PULLUP` è già conforme.

> Il fotoresistore è cablato come voltage divider: la luce alta riduce la resistenza dell'LDR, alza la tensione su A0 e quindi il valore ADC. Il D1 Mini integra già un partitore interno che riporta A0 al range 0–3.3 V.

### 5.3 Variante `WIFI_conn`

Componenti aggiuntivi:
- Adattatore **ELM327 WiFi** generico (rete `WiFi_OBDII` aperta, IP `192.168.0.10:35000`)

Cablaggio (solo display + pulsante + LDR, niente CAN):

| Segnale | D1 Mini | Periferica |
|---|---|---|
| OLED VCC | 3.3 V | VCC |
| OLED GND | GND | GND |
| OLED SDA | **GPIO0** (D3) | SDA |
| OLED SCL | **GPIO2** (D4) | SCL |
| Pulsante | **D5** (GPIO14) | altro lato → GND |
| LDR (lato luce) | 3.3 V | LDR |
| LDR (giunzione) | **A0** | LDR ↔ resistore 10 kΩ ↔ GND |

> ⚠️ Il pulsante è su **D5 (GPIO14)** anziché D3 perché D3/D4 sono qui occupati dall'I²C dell'OLED.

> La variante WiFi è progettata per girare anche su **ESP-01** (8 GPIO totali); su D1 Mini funziona identicamente ma con più pin liberi.

---

## 6. Software & build

### Librerie Arduino

Comuni a entrambi gli sketch:
- `ESP8266WiFi` (core ESP8266 ≥ 3.1.2)
- `ESP8266WebServer`
- `ESP8266HTTPUpdateServer`
- `U8g2` (di olikraus) — driver OLED
- `Wire` (core)

Solo per `CANbus_conn`:
- `mcp_can` (di coryjfowler) — driver MCP2515
- `SPI` (core)

### Toolchain

- **Arduino CLI** + core `esp8266:esp8266 @ 3.1.2`
- VSCode + estensione **Arduino Community Edition** (`vscode-arduino.vscode-arduino-community`)
- FQBN: `esp8266:esp8266:d1_mini_clone`

### Build & flash

Il progetto è strutturato come **due sketch fratelli** in sottocartelle, con header condivisi nella root del repo. Per Arduino IDE / `arduino-cli` ogni sketch deve poter trovare gli `.h` condivisi: il workspace usa `["build.extra_flags", "-I<absolute-project-root>"]` in `arduino.json` (vedi `.vscode/`). Gli `#include` di `EngineConstants.h` in `BoostModel.h` e `TorqueEstimator.h` sono volutamente **path assoluti** perché Arduino IDE non risolve i path relativi fra header e sketch in cartelle diverse.

Comando indicativo con `arduino-cli` (variante CAN):

```bash
arduino-cli compile \
  --fqbn esp8266:esp8266:d1_mini_clone \
  --build-property "build.extra_flags=-I/Users/alesimattia/Documents/OBD2-car_dashboard-OLED" \
  CANbus_conn

arduino-cli upload -p /dev/cu.usbserial-XXXX \
  --fqbn esp8266:esp8266:d1_mini_clone \
  CANbus_conn
```

Sostituire `CANbus_conn` con `WIFI_conn` per l'altra variante.

> ⚠️ Il budget IRAM è già al 93%: evitare di aggiungere funzioni `IRAM_ATTR`.

---

## 7. Configurazione

Tutte le costanti modificabili sono in cima a ciascuno sketch.

### `CANbus_conn/CANbus_conn.ino`

| `#define` | Default | Significato |
|---|---|---|
| `MCP2515_CRYSTAL` | `MCP_8MHZ` | `MCP_8MHZ` o `MCP_16MHZ`, dipende dal modulo |
| `CAN_CS_PIN` | `15` | D8 |
| `CAN_SPEED` | `CAN_500KBPS` | velocità bus OBD2 |
| `OBD2_REQUEST_ID` | `0x7DF` | ID broadcast OBD2 |
| `OBD2_RESPONSE_ID` | `0x7E8` | ID risposta ECU motore |
| `OBD2_TIMEOUT_MS` | `100` | timeout per frame singolo |
| `OTA_SSID` / `OTA_PASS` | `OBD2_UPDATE` / `alesimattia` | SoftAP per OTA |
| `OTA_WINDOW_MS` | `180000` | finestra OTA: 3 min dal boot (estesa se un client si connette) |
| `MAX_DTC` | `6` | max DTC bufferizzati (multi-frame ISO-TP) |
| `DTC_PER_PAGE` | `2` | DTC per pagina display |

### `WIFI_conn/WIFI_conn.ino`

| `#define` | Default | Significato |
|---|---|---|
| `ELM327_SSID` / `ELM327_PASS` | `WiFi_OBDII` / `""` | rete dell'adattatore |
| `ELM327_IP` | `192.168.0.10` | IP TCP dell'ELM327 |
| `ELM327_PORT` | `35000` | porta TCP dell'ELM327 |
| `ELM327_TIMEOUT` | `2000` | timeout AT generico (ms) |
| `ELM327_FIRST_TIMEOUT` | `5000` | timeout sulla prima query (auto-detect protocollo) |
| `ELM327_PROTOCOL` | `ATSP6` | CAN 11 bit 500 kbps (gruppo VAG); usare `ATSP0` per auto |
| `OTA_SSID` / `OTA_PASS` | `OBD2_UPDATE` / `alesimattia` | SoftAP per OTA |
| `BUTTON_PIN` | `D5` | pin del pulsante (D3/D4 occupati dall'I²C) |

---

## 8. Round-robin PID schedule

Per massimizzare il refresh del display il loop legge **un solo PID per ciclo** seguendo uno schedule pesato (uguale nei due sketch):

```cpp
const uint8_t PID_SCHEDULE[] = { 0,1,2,3, 0,1,2,3, 0,1,2,3, 4,5,9,10, 6,7,8 };
```

| Slot | PID | Descrizione | Peso |
|---|---|---|---|
| 0 | `0x0B` | MAP — pressione assoluta collettore (kPa) | 3× |
| 1 | `0x04` | Calculated engine load (%) | 3× |
| 2 | `0x0C` | RPM | 3× |
| 3 | `0x10` | MAF (g/s) | 3× |
| 4 | `0x0F` | IAT (°C) | 1× |
| 5 | `0x33` | Pressione barometrica (kPa) | 1× |
| 6 | `0x05` | Coolant temp (°C) | 1× |
| 7 | `0x2C` | EGR comandata (%) | 1× |
| 8 | `0x2D` | EGR error (%) | 1× |
| 9 | `0x23` | Fuel rail pressure | 1× |
| 10 | `0x42` | Module voltage | 1× |

Dopo ogni lettura dei PID 0–5, 9, 10 il firmware ricalcola **boost** e **coppia** dai valori cachati (`recalcModels()`); per gli slot puramente di display (6–8) salta il ricalcolo per non sprecare cicli.

**Stale detection**: dopo 3 timeout consecutivi sullo stesso PID il valore in cache viene marcato `NAN` (e i flag `*Available` vanno a `false`), così che display e dashboard mostrino `N/D`. Sul ramo CAN, dopo 10 timeout totali, se l'`EFLG` del MCP2515 segnala bus-off il transceiver viene re-inizializzato. Sul ramo WiFi la connessione TCP all'ELM327 viene chiusa e ristabilita.

---

## 9. Modelli fisici condivisi

Tutti i parametri specifici del motore vivono in [`EngineConstants.h`](EngineConstants.h) sotto il namespace `Audi27TDI140kW`. Modificare qui (e non altrove) per adattare il firmware a un motore differente.

### `BoostModel.h` — stima pressione turbo

Output: pressione turbo **relativa in bar** (può essere negativa = depressione).

```
boost = (MAP - BARO) + ΔP_charge_path
```

dove `ΔP_charge_path` stima la perdita di carico fra compressore e collettore in funzione di `MAF`, `RPM`, `IAT`, `LOAD` e dell'efficienza volumetrica apparente. Filtro EMA opzionale su MAP/BARO/MAF/LOAD (`BOOST_EMA_ALPHA_*`). Quando il motore è in vera depressione a basso carico, la perdita di carico aggiunta viene proporzionalmente ridotta per non falsare il segno.

### `TorqueEstimator.h` — stima coppia motore

Output: **coppia in Nm** (clamp `0..420`).

```
T = T_nom(rpm) · loadFactor · cMAF · cVE · cRail · cVolts · cBARO
```

- `T_nom(rpm)` segue la curva tipica del 2.7 TDI: smoothstep da idle al plateau, plateau pieno (400 Nm) fra 1400 e 3250 rpm, sopra il plateau limite imposto dalla potenza (`9550 · 140 / rpm`).
- `loadFactor = LOAD/100`, clampato leggermente sopra 1 per non perdere transitori.
- `cMAF`, `cVE` sono correzioni vere derivate da MAF/MAP/IAT con clamp stretti per evitare overshoot.
- `cRail`, `cVolts`, `cBARO` sono correzioni *deboli* opzionali (NAN ⇒ `1.0`) usate solo per non sovrastimare in undervoltage o in alta quota.

Filtro EMA opzionale su LOAD/MAF/MAP. Quando viene chiamato dal flusso principale il filtro è disattivato (`useFilter=false`) perché lo smoothing è già implicito nella PID cache del round-robin.

### Helper aggiuntivi

Calcolati e mostrati in modalità diagnostica/dashboard (debug=true):
- Potenza istantanea kW/CV (`T·rpm/9549`)
- BSFC `g/kWh` da `MAF` e `λ`
- Efficienza volumetrica
- Efficienza intercooler (richiede MAP, BARO, IAT, ambient)
- Air density, altitudine ISA, pressure ratio compressore
- Drift pedale D vs E, delta farfalla–pedale, taglio iniezione (DFCO)
- Variazione boost (bar/s), accelerazione (m/s²), rapporto CVT (`RPM / (speed × 7.9)`)

---

## 10. 🔘 Pulsante fisico (cancella DTC)

Pulsante NA verso GND, gestito con `INPUT_PULLUP` e debounce software 50 ms ([`button_handler.h`](button_handler.h)). Nessuna ISR, nessun `delay()`. La macchina a stati genera tre eventi: `BTN_NONE`, `BTN_SHORT`, `BTN_LONG`.

| Sketch | Pin | GPIO |
|---|---|---|
| `CANbus_conn` | **D3** | GPIO0 |
| `WIFI_conn`   | **D5** | GPIO14 |

### Funzioni del pulsante

| Azione | Effetto |
|---|---|
| **Click breve** | Scorre ciclicamente le schermate del display: Monitor → DTC pag.1 → DTC pag.2 → … → Monitor. Avanza manualmente la pagina e posticipa l'auto-cycle dei 5 s. |
| **Pressione lunga** (≥ 2.5 s) | 🛠️ **Cancella i codici di errore DTC** della centralina (OBD2 Service 04 / Mode 04). Mostra l'esito sul display per 1.2 s. |

### Cancellazione DTC — dettaglio

La pressione lunga invia il comando OBD2 **Mode 04** all'ECU:
- **Su CAN**: frame `[0x01, 0x04, 0x00, ...]` su ID `0x7DF`. Verifica risposta positiva `0x44` su `0x7E8`.
- **Su WiFi**: comando AT `04` all'ELM327. Cerca `"44"` nella risposta ASCII.

Se la cancellazione va a buon fine, lo stato MIL viene riletto immediatamente al prossimo `DTC_CHECK_INTERVAL` (30 s) o forzato dal pulsante stesso.

> ⚠️ Cancellare i DTC perde lo storico errori e **resetta i monitor di readiness** della centralina. Dopo un reset i monitor possono richiedere diversi cicli di guida (anche 100+ km in condizioni miste) prima di tornare tutti in stato "Ready" — lo stato richiesto per superare la revisione/MOT.

La stessa cancellazione è disponibile dal pulsante **CANCELLA** nel web dashboard (endpoint `/clear-dtc`).

---

## 11. 🌐 Interfaccia web `/dashboard`

Tutta la logica HTTP vive in [`web_dashboard.h`](web_dashboard.h), abilitata via `#define OBD_CONN_CAN` o `#define OBD_CONN_WIFI` nello sketch (entrambi i casi sono già configurati).

### 11.1 SoftAP & accesso

| Sketch | SSID | Password | URL dashboard |
|---|---|---|---|
| `CANbus_conn` | `OBD2_UPDATE` | `alesimattia` | `http://192.168.4.1/dashboard` |
| `WIFI_conn`   | `OBD2_UPDATE` | `alesimattia` | `http://192.168.4.1/dashboard` |

L'AP resta attivo per `OTA_WINDOW_MS` (3 min) dal boot. Se un client si collega il timeout viene **sospeso**; alla disconnessione il timeout riparte da capo. Scaduta la finestra il server HTTP viene fermato e (su `CANbus_conn`) il radio WiFi spento per liberare ~75 mA di consumo.

### 11.2 Endpoint HTTP

| Path | Metodo | Risposta | Note |
|---|---|---|---|
| `/dashboard` | GET | HTML | dashboard mobile-first, polling `/data` ogni 500 ms |
| `/data` | GET | JSON | 4 metriche + DTC + LDR se `debug=false`; 47 parametri se `debug=true` |
| `/debug` | GET | `{"debug":bool}` | toggle runtime modalità debug, query `?on` / `?off` |
| `/brightness` | GET | `{"ldr":N,"contrast":N,"mode":"auto\|manual\|fading","ldrAtOverride":N}` | stato luminosità; `?set=N` (0–255) attiva override manuale, `?auto` rientra in automatico con fade |
| `/clear-dtc` | GET | `{"ok":bool}` | invia OBD2 Mode 04 e forza re-check immediato |
| `/update` (CAN) `/ota` (WiFi) | GET / POST | upload firmware | servito da `ESP8266HTTPUpdateServer` |

> ⚠️ Nessuna autenticazione, CORS o rate limiting: il modello di minaccia è la sola rete locale del SoftAP / dell'ELM327.

### 11.3 Layout della dashboard

La pagina è suddivisa in sezioni (mobile-first, larghezza max 480 px):

1. **Toggle "Diagnostica Serial"** — attiva/disattiva la modalità debug (vedi §11.5).
2. **Dati principali** — sempre visibili: BOOST, TEMP, COPPIA, EGR (4 box grandi, valori a colori).
3. **Sezioni avanzate** — visibili solo con `debug=true`:
   - 🚙 Motore (carico, RPM, MAF, rail, lambda, AFR, IAT, temp liquido)
   - 🚗 Veicolo (velocità, pedale D/E, farfalla, rapporto CVT, accelerazione)
   - ⛽ Consumi (L/100km, cons. specifico g/kWh, potenza kW/CV)
   - 🌪️ Turbo (boost, rapporto compressione, eff. intercooler, variazione boost)
   - 🔧 Diagnostica (batteria, drift pedale, taglio iniezione, eff. volumetrica)
   - 🌍 Ambiente (temp esterna, baro, altitudine, densità aria)
   - ⏱️ Contatori (tempo motore, km con MIL, avviamenti)
4. **Box DTC** (visibile solo se `dtcCount > 0`) — riquadro rosso con MIL badge e lista codici/descrizioni.
5. **Card "Luce ambiente"** — valore LDR live, contrasto applicato, modalità (`auto`/`manual`/`fading`), slider 0–255 e bottone "Reset auto".
6. **Bottone "Cancella DTC"** — invia `/clear-dtc` (con conferma).

I valori vengono colorati di **rosso** (allarme) o **giallo** (warning) quando fuori range:

| Parametro | 🟡 Warning | 🔴 Allarme |
|---|---|---|
| Batteria | 12.5–13.0 V o 14.5–15.0 V | < 12.5 V o > 15.0 V |
| Drift pedale D/E | 3–5 % | > 5 % |
| Errore EGR | — | abs > 10 % |
| Lambda | — | < 0.8 |
| Temp. liquido | 100–110 °C | > 110 °C |
| Press. rail | — | < 200 o > 1850 bar |
| Eff. intercooler | — | < 40 % |
| Cons. specifico | — | > 400 g/kWh |
| Eff. volumetrica | — | < 60 % |
| Boost | — | > 1.5 bar |

### 11.4 Schema JSON `/data`

**Modalità minima** (`debug=false`) — payload ~250 byte, zero query OBD aggiuntive (legge solo le variabili globali del round-robin):
```json
{
  "boost": 0.25, "coolant": 92, "torque": 280,
  "egr": 35, "egrErr": 2,
  "mil": false, "dtcCount": 0,
  "lightLevel": 740, "contrast": 180, "brightnessMode": "auto",
  "debug": false,
  "dtc": []
}
```

**Modalità completa** (`debug=true`) — payload ~750 byte, **legge tutti i 24 PID internamente** ad ogni richiesta:
tutti i campi minimi più `load`, `map`, `rpm`, `speed`, `iat`, `maf`, `railBar`, `lambda`, `o2v`, `baro`, `volts`, `ambient`, `pedalD`, `pedalE`, `throttle`, `runtime`, `kmMil`, `starts`, `powerKw`, `powerCv`, `afr`, `fuelL100`, `altitude`, `airDensity`, `intercoolerEff`, `pressureRatio`, `volEff`, `driftPedal`, `deltaThrottlePedal`, `dfco`, `gearRatio`, `accel`, `boostRate`, `bsfc`.

### 11.5 Toggle debug

Il toggle "Diagnostica Serial" controlla la variabile runtime `debugMode` (default `false` al boot, persistente in RAM finché l'ESP non viene riavviato):

- **Off**: dashboard mostra solo 4 box principali + DTC + LDR. Refresh display ECU rapido.
- **On**: dashboard mostra tutte le sezioni avanzate. `printAdvancedDiagnostics()` stampa i 47 parametri su Serial (115200 baud) ad ogni ciclo. **Rallenta sensibilmente il refresh dell'OLED** ed è pensato come strumento di tuning a banco, non come modalità d'esercizio.

Lo stato viene sincronizzato con il backend via `/debug?on` / `/debug?off` ad ogni cambio del toggle, ed è restituito anche nel JSON `/data` per mantenere il toggle in sync se la pagina viene ricaricata.

### 11.6 Anteprima offline

Per vedere come si presenta la dashboard senza ESP, apri [`monitor_preview.html`](monitor_preview.html) nel browser: simula valori casuali realistici e li aggiorna ogni 500 ms.

▶️ **Versione live nel browser** (senza clonare la repo): [monitor_preview.html via raw.githack.com](https://raw.githack.com/alesimattia/OBD2-car_dashboard-OLED/main/monitor_preview.html)

<p align="center">
  <iframe src="monitor_preview.html" width="700" height="500" frameborder="0"
          style="border:1px solid #444;border-radius:8px;background:#111">
    Il tuo browser non supporta iframe.
    <a href="monitor_preview.html">Apri monitor_preview.html</a>
  </iframe>
</p>

> ⚠️ GitHub non renderizza gli `iframe` nei README: l'embed sopra funziona solo se apri questo file in locale. Su GitHub usa il link raw.githack.com qui sopra.

---

## 12. 💡 Auto-brightness OLED

Il contrasto del display è regolato in funzione della luce ambientale letta da un fotoresistore (LDR) sull'ADC `A0`. La logica vive in [`light_sensor.h`](light_sensor.h) ed è chiamata polled da `loop()` (`updateBrightness()`); nessun `IRAM_ATTR`, nessun ISR.

**Schema cablaggio** (identico nelle due varianti):

```
3.3V ──[ LDR ]── A0 ──[ 10 kΩ ]── GND
```

Più luce → R_LDR ↓ → V_A0 ↑ → ADC ↑ → contrasto ↑.

**State machine** — tre stati:

| Stato | Comportamento |
|---|---|
| `auto` | Contrasto target = `adcToContrast(LDR filtrato)`, applicato con isteresi 3 unità per evitare flicker. |
| `manual` | L'utente ha mosso lo slider del web dashboard. Il contrasto resta al valore manuale finché la luce ambientale non varia di più di `LDR_SCENE_DELTA` (default 150 conteggi ADC) rispetto al valore al momento dell'override. |
| `fading` | Transizione graduale verso il nuovo target automatico (`BR_FADE_STEP` = 4 unità ogni `BR_FADE_TICK_MS` = 30 ms → ~1.9 s per range pieno). Si entra qui da `manual` quando la scena cambia troppo, oppure dal bottone "Reset auto" del dashboard. |

**Filtraggio ADC**: EMA con α = 0.15 e clamp anti-spike che scarta sample isolati con |Δ| > 300 conteggi.

Il tempo di settling al 95% di una variazione netta di luce segue la formula generale dell'EMA:

> `t_95% ≈ ln(0.05) / (LDR_SAMPLE_HZ · ln(1 − α))`  secondi

Con α = 0.15 il fattore `ln(0.05) / ln(0.85) ≈ 18.4`, quindi `t_95% ≈ 18 / LDR_SAMPLE_HZ`. Esempi: a 10 Hz → ~1.8 s; a 4 Hz → ~4.5 s.

**Mapping ADC → contrasto**: gamma 2.0 sul range utile calibrato dinamicamente (vedi sotto), output limitato a `BR_CONTRAST_MIN..BR_CONTRAST_MAX` (5..240 di default) per evitare di spegnere il pannello al buio assoluto e per non saturare il driver in pieno sole.

**Auto-calibrazione live del range LDR**: i limiti ADC usati da `adcToContrast()` (`ldrMinObserved` / `ldrMaxObserved` nello stato `BrightnessState`) si adattano automaticamente alla luce effettivamente osservata in sessione. Non è richiesta alcuna taratura manuale: il sistema parte con un range fallback `30..950` e durante l'uso registra gli estremi del valore EMA filtrato. Un decay attivo (~40 unità/minuto, agisce solo se un bound è > 20 conteggi oltre l'EMA corrente) permette di "dimenticare" rapidamente outlier accidentali (riflessi, flash di sole nel parabrezza). **La calibrazione vive solo in RAM**: ad ogni reboot si riparte dal fallback e si ricalibra da zero — questa scelta è deliberata per evitare wear della EEPROM (numero max scritture).

**Comportamento atteso nel tempo**:

| Fase | Stato bounds osservati | Mappatura usata | Tempi |
|---|---|---|---|
| Boot → +2s (warm-up) | vuoti (min=1023, max=0) | fallback 30..950 | 2 s |
| +2s → primi sample | espansione veloce | fallback finché range < 100 | tipicamente < 10 s |
| Sessione stabilizzata | range > 100 sull'ambiente reale | bounds osservati | regime |
| Outlier isolato (flash sole) | max espanso, poi decay | bounds che si correggono | ~2-3 min per outlier di 100 unità |
| Reboot | reset a vuoti | fallback, poi ricalibra | 2 s + qualche secondo |

**Tempo di risposta del display**: una variazione netta di luce ambientale si riflette sul contrasto in **~1-1.5 secondi** (settling al 95% dell'EMA con sample a 10 Hz).

**Costanti tunabili** in [`light_sensor.h`](light_sensor.h):

| `#define` | Default | Significato |
|---|---|---|
| `LDR_SAMPLE_HZ` | `10` | Frequenza di campionamento LDR (Hz) — `LDR_SAMPLE_MS` derivato come `1000/LDR_SAMPLE_HZ` |
| `LDR_SCENE_DELTA` | `150` | Soglia uscita da `manual` (conteggi ADC) |
| `BR_HYST_DELTA` | `3` | Soglia minima per riapplicare `setContrast` |
| `BR_FADE_STEP` | `4` | Δ contrasto per tick di fade |
| `BR_FADE_TICK_MS` | `30` | Periodo del tick di fade |
| `BR_CONTRAST_MIN` | `5` | Floor (notte estrema) |
| `BR_CONTRAST_MAX` | `240` | Ceiling (sole pieno) |
| `LDR_CALIBRATION_FALLBACK_MIN` | `30` | Bound minimo del fallback ADC (sessione appena iniziata) |
| `LDR_CALIBRATION_FALLBACK_MAX` | `950` | Bound massimo del fallback ADC |
| `LDR_CALIBRATION_MIN_RANGE` | `100` | Span minimo per usare i bound osservati |
| `LDR_CALIBRATION_WARMUP_MS` | `2000` | Warm-up post-boot prima di calibrare |
| `LDR_CALIBRATION_DECAY_MS` | `3000` | Periodo del tick di decay anti-outlier |
| `LDR_CALIBRATION_DECAY_STEP` | `2` | Unità di decay per tick (2/3s ≈ 40/min) |
| `LDR_CALIBRATION_DECAY_MARGIN` | `20` | Distanza dall'EMA oltre la quale parte il decay |

**Dettaglio delle costanti della state machine**:

- **`LDR_SCENE_DELTA`** (150 conteggi ADC) — quanto deve variare la luce ambientale rispetto al valore di snapshot al momento dell'override manuale per uscire dallo stato `manual` e tornare in `auto` (passando per `fading`). Valori alti rendono lo slider manuale più "appiccicoso" (resta attivo anche se la luce cambia un po'); valori bassi lo fanno decadere prima. Su un range tipico 0–950 ADC, 150 corrisponde a ~16% della scala — sufficiente per distinguere "ingresso in galleria" da fluttuazioni normali.

- **`BR_HYST_DELTA`** (3 unità di contrasto, su scala 0–255) — in stato `auto`, il driver OLED viene aggiornato via `u8g2.setContrast()` solo se il nuovo target differisce dal valore corrente di almeno questo delta. Serve sia come anti-flicker (l'EMA oscilla di ±1 anche a luce stabile) sia per ridurre le scritture I²C inutili. Aumentando il valore si ottiene un display più "fermo" ma meno reattivo a piccole variazioni.

- **`BR_FADE_STEP`** (4 unità di contrasto per tick) — durante una transizione `fading`, il contrasto corrente si avvicina al target di questa quantità ad ogni tick. Valori alti = fade più rapido ma con scalini più percepibili; valori bassi = transizione più morbida ma più lenta.

- **`BR_FADE_TICK_MS`** (30 ms) — periodo tra due step di fade. La durata totale di una transizione si ricava come `(|target − current| / BR_FADE_STEP) · BR_FADE_TICK_MS`. Esempio: passaggio da contrasto 5 a 240 con i default → `(235/4)·30 ms ≈ 1.76 s`.

- **`BR_CONTRAST_MIN`** (5) — limite inferiore applicato all'OLED. Impostare 0 spegnerebbe completamente il pannello rendendolo illeggibile anche in penombra; un valore minimo non nullo garantisce che le scritte restino visibili anche di notte assoluta (es. parcheggio buio). Aumentare se di notte il display risulta ancora invisibile, ridurre se abbaglia.

- **`BR_CONTRAST_MAX`** (240) — limite superiore applicato all'OLED. La scala SH1106 arriva a 255, ma saturare il driver al massimo non aumenta significativamente la luminosità e accelera l'usura del pannello (burn-in). 240 lascia un ~6% di margine. Aumentare solo se in pieno sole il display risulta poco leggibile.

> ⚠️ Lo stato override **non è persistente** fra reboot per evitare eccessivo wear della EEPROM (numero max scritture). Anche la calibrazione live del range LDR vive solo in RAM e si resetta ad ogni boot per lo stesso motivo.

---

## 13. ⚠️ DTC — codici di errore

- **Lettura**: PID `0x01` (Mode 01) per stato MIL e numero DTC, ogni `DTC_CHECK_INTERVAL` (30 s). Se `MIL=ON` e count > 0 viene fatta una **Mode 03** per leggere i codici.
  - Su CAN: gestione completa **ISO-TP** con Single Frame, First Frame + Flow Control + Consecutive Frames (fino a `MAX_DTC = 6` codici).
  - Su WiFi: parsing della risposta ASCII multi-line dell'ELM327, fino a 6 codici.
- **Decodifica**: la categoria (P/C/B/U) e i 4 nibble del codice vengono ricostruiti in stringa "P0301".
- **Descrizione breve**: ricerca binaria in [`dtc_descriptions.h`](dtc_descriptions.h) (~45 codici diesel/turbo più comuni in PROGMEM, italiano, ≤ 21 caratteri).
- **Display**: alternanza automatica fra schermata dati e pagine DTC ogni `SCREEN_SWITCH_MS` (5 s). Il click breve sul pulsante avanza manualmente la pagina e posticipa l'auto-cycle.
- **Cancellazione**: Mode 04 dal pulsante (long-press, vedi §10) o dal web dashboard (`/clear-dtc`).

---

## 14. 📁 Layout file

```
.
├── CANbus_conn/
│   └── CANbus_conn.ino       sketch ESP8266 + MCP2515
├── WIFI_conn/
│   └── WIFI_conn.ino         sketch ESP8266 + ELM327 WiFi
├── web_dashboard.h           server HTTP + JSON, condiviso (gating via #define OBD_CONN_*)
├── BoostModel.h              modello pressione turbo (namespace Audi27TDI140kW::Boost)
├── TorqueEstimator.h         modello coppia (namespace Audi27TDI140kW::Torque)
├── EngineConstants.h         tutti i parametri motore (namespace Audi27TDI140kW)
├── button_handler.h          debounce + state machine pulsante (BTN_NONE/SHORT/LONG)
├── light_sensor.h            auto-brightness OLED (LDR su A0, state machine sticky+fade)
├── dtc_descriptions.h        tabella DTC PROGMEM + decodeDTC() / getDTCDescription()
├── ui_preview.html           preview offline delle schermate OLED
├── monitor_preview.html      preview offline della dashboard web /dashboard
├── DOCS/                     datasheet MCP2515, lista PID supportati Audi A5, output seriali
├── .vscode/                  config Arduino Community Edition (FQBN, build.extra_flags)
└── LICENSE
```

---

## 15. Note operative & limiti

- **Stime, non misure**: coppia, potenza e boost sono **stime** ricostruite da PID OBD2 standard. Sull'A5 2.7 TDI nessun PID standard espone direttamente la coppia ECU; il modello è calibrato sui parametri targati 400 Nm / 140 kW.
- **Plateau coppia**: il modello assume un plateau piatto a 400 Nm fra 1400 e 3250 rpm con caduta a potenza costante oltre. Se il motore è mappato/rimappato in modo non standard, riallineare `TORQUE_MAX_NM`, `POWER_MAX_KW` e i limiti del plateau in [`EngineConstants.h`](EngineConstants.h).
- **MAP saturato**: il PID `0x0B` è 1 byte (0–255 kPa). Sopra ~2.5 bar assoluti il valore satura: il boost mostrato è quindi limitato a ~1.5 bar relativi.
- **Refresh display**: l'aggiornamento parziale (`u8g2.updateDisplayArea`) trasmette solo le tile rows modificate, riducendo il traffico I²C da ~1024 byte a ~256–384 byte per frame.
- **OTA dopo finestra**: scaduto `OTA_WINDOW_MS`, per riaggiornare il firmware è necessario un reset (power cycle); è una scelta deliberata per non tenere il radio acceso durante la guida.
- **ESP-01 con ELM327**: la variante WiFi è progettata per girare anche su ESP-01 (8 GPIO totali); su D1 Mini funziona identicamente ma con più pin liberi.

---

## 16. [`LICENSE`](LICENSE).
