# OBD2 Car Dashboard — OLED

Dashboard motore in tempo reale per **Audi A5 B8 2.7 TDI (CGKA, 140 kW) Multitronic**, basata su ESP8266 D1 Mini e display OLED SH1106 1.3" (128×64). Il progetto contiene **due sketch alternativi** che condividono la stessa logica di dashboard, gli stessi modelli fisici e lo stesso web server, ma differiscono solo per il transport verso l'ECU:

| Sketch | Transport | Hardware extra | Refresh OBD |
|---|---|---|---|
| [`CANbus_conn/`](CANbus_conn/) | **CAN nativo** via MCP2515/TJA1050 (SPI) | modulo MCP2515 8 MHz/16 MHz | ~8–10 Hz |
| [`WIFI_conn/`](WIFI_conn/) | **ELM327 WiFi** (TCP, comandi AT) | adattatore ELM327 WiFi nella presa OBD2 | ~2 Hz |

Entrambi gli sketch:
- Eseguono al boot una scansione completa dei PID Mode 01 supportati dalla centralina.
- Mostrano sull'OLED 4 metriche live (BOOST, COPPIA, TEMP, EGR) con aggiornamento parziale del framebuffer.
- Espongono via SoftAP un **web dashboard** (`/dashboard`) con 47 parametri diretti + calcolati e refresh JSON ogni 500 ms.
- Leggono/cancellano i **DTC** (Mode 03 / Mode 04) e li mostrano paginati sul display.
- Supportano **OTA** firmware via browser su `http://192.168.4.1/update` (CAN) o `/ota` (WiFi).
- Sono pilotabili da un **pulsante fisico** per ciclare le schermate e cancellare i DTC.
- Regolano automaticamente la **luminosità dell'OLED** in base alla luce ambientale tramite un fotoresistore (LDR) sull'ADC, con override manuale dal web dashboard.

---

## 1. Architettura

```
                   ┌──────────────────────────────────┐
                   │         ESP8266 D1 Mini          │
                   │                                  │
   CAN 500kbps     │  ┌────────────┐   ┌───────────┐  │  I²C 400 kHz
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
                   │  │ /clear-dtc /update          │ │
                   │  └──────────────────────────────┘ │
                   └──────────────────────────────────┘
                              ▲ SoftAP
                              │  "OBD2_UPDATE" (CAN) / "Audi_Dashboard" (WiFi)
                              ▼
                          smartphone
```

Nello sketch WiFi il blocco MCP2515 è sostituito da un `WiFiClient` TCP verso `192.168.0.10:35000` (porta dell'ELM327); l'ESP8266 lavora in `WIFI_AP_STA` (STA verso l'ELM327 + AP per l'OTA).

---

## 2. Hardware

### Veicolo target
- **Audi A5 B8 2.7 TDI (motore CGKA, 140 kW / 190 CV) Multitronic**
- Cilindrata: 2.698 L (`ENGINE_DISPLACEMENT_M3 = 0.002698`)
- Coppia max nominale: 400 Nm fra ~1400 e 3250 rpm
- Bus diagnostica: **CAN 11 bit a 500 kbps** (gruppo VAG / OBD2)

### Variante CAN — `CANbus_conn`

Componenti:
- ESP8266 **D1 Mini** (clone)
- Modulo **MCP2515 + TJA1050** con cristallo 8 MHz (configurabile a 16 MHz via `#define MCP2515_CRYSTAL`)
- **OLED SH1106 1.3"** I²C (compatibile U8g2)
- Pulsante NA → GND
- Fotoresistore (LDR, es. GL5528) + resistore 10 kΩ per auto-brightness (vedi §10)
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

### Variante WiFi — `WIFI_conn`

Componenti:
- ESP8266 **D1 Mini** (clone)
- **OLED SH1106 1.3"** I²C
- Adattatore **ELM327 WiFi** generico (rete `WiFi_OBDII` aperta, 192.168.0.10:35000)
- Pulsante NA → GND
- Fotoresistore (LDR, es. GL5528) + resistore 10 kΩ per auto-brightness (vedi §10)

Cablaggio (solo display + pulsante + LDR):

| Segnale | D1 Mini | Periferica |
|---|---|---|
| OLED VCC | 3.3 V | VCC |
| OLED GND | GND | GND |
| OLED SDA | **GPIO0** (D3) | SDA |
| OLED SCL | **GPIO2** (D4) | SCL |
| Pulsante | **D5** (GPIO14) | altro lato → GND |
| LDR (lato luce) | 3.3 V | LDR |
| LDR (giunzione) | **A0** | LDR ↔ resistore 10 kΩ ↔ GND |

> Il pulsante è su **D5 (GPIO14)** anziché D3 perché D3/D4 sono qui occupati dall'I²C dell'OLED.

> Il fotoresistore è cablato come voltage divider: la luce alta riduce la resistenza dell'LDR, alza la tensione su A0 e quindi il valore ADC. Il D1 Mini integra già un partitore interno che riporta A0 al range 0–3.3 V.

---

## 3. Dipendenze software

Librerie Arduino richieste (entrambi gli sketch):
- `ESP8266WiFi` (core ESP8266 ≥ 3.1.2)
- `ESP8266WebServer`
- `ESP8266HTTPUpdateServer`
- `U8g2` (di olikraus) — driver OLED
- `Wire` (core)

In più solo per `CANbus_conn`:
- `mcp_can` (di coryjfowler) — driver MCP2515
- `SPI` (core)

Toolchain:
- **Arduino CLI** + core `esp8266:esp8266 @ 3.1.2`
- VSCode + estensione **Arduino Community Edition** (`vscode-arduino.vscode-arduino-community`)
- FQBN: `esp8266:esp8266:d1_mini_clone`

---

## 4. Build & flash

Il progetto è strutturato come **due sketch fratelli** in sottocartelle, con header condivisi nella root del repo. Per Arduino IDE / `arduino-cli` ogni sketch deve poter trovare gli `.h` condivisi: il workspace usa `["build.extra_flags", "-I<absolute-project-root>"]` in `arduino.json` (vedi `.vscode/`). Gli `#include` di `EngineConstants.h` in `BoostModel.h` e `TorqueEstimator.h` sono volutamente **path assoluti** perché Arduino IDE non risolve i path relativi fra header e sketch in cartelle diverse.

Comando indicativo con arduino-cli (variante CAN):

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

> Il budget IRAM è già al 93%: evitare di aggiungere funzioni `IRAM_ATTR`.

---

## 5. Configurazione

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
| `OTA_SSID` / `OTA_PASS` | `OBD2_UPDATE` / `obd2flash` | SoftAP per OTA |
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
| `OTA_SSID` / `OTA_PASS` | `Audi_Dashboard` / `alesimattia` | SoftAP per OTA |
| `BUTTON_PIN` | `D5` | pin del pulsante (D3/D4 occupati dall'I²C) |

---

## 6. Round-robin PID schedule

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

## 7. Modelli fisici condivisi

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

Helper aggiuntivi calcolati e mostrati in modalità diagnostica/dashboard:
- Potenza istantanea kW/CV (`T·rpm/9549`)
- BSFC `g/kWh` da `MAF` e `λ`
- Efficienza volumetrica
- Efficienza intercooler (richiede MAP, BARO, IAT, ambient)
- Air density, altitudine ISA, pressure ratio compressore
- Drift pedale D vs E, delta farfalla–pedale, taglio iniezione (DFCO)
- Variazione boost (bar/s), accelerazione (m/s²), rapporto CVT

---

## 8. Pulsante fisico

NA verso GND, gestito con `INPUT_PULLUP` e debounce software 50 ms (vedi [`button_handler.h`](button_handler.h)). Nessuna ISR, nessun `delay()`.

| Sketch | Pin | GPIO |
|---|---|---|
| `CANbus_conn` | **D3** | GPIO0 |
| `WIFI_conn`   | **D5** | GPIO14 |

Eventi:

- **Click breve** → scorre ciclicamente le schermate del display: Monitor → DTC pag.1 → DTC pag.2 → … → Monitor.
- **Pressione lunga (≥ 2.5 s)** → invia OBD2 **Service 04** (Clear DTC) e cancella i codici di errore della centralina; mostra l'esito per 1.2 s.

> ⚠️ Cancellare i DTC perde lo storico errori e resetta i monitor di readiness della centralina.

La stessa cancellazione è disponibile dal pulsante **CANCELLA** nel web dashboard (endpoint `/clear-dtc`).

---

## 9. Web dashboard & OTA

Tutta la logica HTTP vive in [`web_dashboard.h`](web_dashboard.h), abilitata via `#define OBD_CONN_CAN` o `#define OBD_CONN_WIFI` nello sketch (entrambi i casi sono già configurati).

### SoftAP

| Sketch | SSID | Password | URL |
|---|---|---|---|
| `CANbus_conn` | `OBD2_UPDATE` | `obd2flash` | `http://192.168.4.1` |
| `WIFI_conn`   | `Audi_Dashboard` | `alesimattia` | `http://192.168.4.1` |

L'AP resta attivo per `OTA_WINDOW_MS` (3 min) dal boot. Se un client si collega il timeout viene **sospeso**; alla disconnessione il timeout riparte da capo. Scaduta la finestra il server HTTP viene fermato e (su `CANbus_conn`) il radio WiFi spento per liberare ~75 mA.

### Endpoint HTTP

| Path | Metodo | Risposta | Note |
|---|---|---|---|
| `/dashboard` | GET | HTML | dashboard mobile-first, polling `/data` ogni 500 ms |
| `/data` | GET | JSON | 4 metriche + DTC + stato LDR se `debug=false`, 47 parametri se `debug=true` |
| `/debug` | GET | `{"debug":bool}` | toggle runtime modalità debug, query `?on` / `?off` |
| `/brightness` | GET | `{"ldr":N,"contrast":N,"mode":"auto\|manual\|fading","ldrAtOverride":N}` | stato luminosità; `?set=N` (0–255) attiva override manuale, `?auto` rientra in automatico con fade |
| `/clear-dtc` | GET | `{"ok":bool}` | invia OBD2 Mode 04 e forza re-check immediato |
| `/update` (CAN) `/ota` (WiFi) | GET / POST | upload firmware | servito da `ESP8266HTTPUpdateServer` |

Nessuna autenticazione, CORS o rate limiting: il modello di minaccia è la sola rete locale del SoftAP / dell'ELM327.

### Schema JSON `/data`

Modalità minima (`debug=false`):
```json
{
  "boost": 0.25, "coolant": 92, "torque": 280,
  "egr": 35, "egrErr": 2,
  "mil": false, "dtcCount": 0,
  "lightLevel": 740, "contrast": 180, "brightnessMode": "auto",
  "debug": false,
  "dtc": [ { "code": "P1234", "desc": "..." } ]
}
```

Modalità completa (`debug=true`) — tutti i campi sopra più:
`load`, `map`, `rpm`, `speed`, `iat`, `maf`, `railBar`, `lambda`, `o2v`, `baro`, `volts`, `ambient`, `pedalD`, `pedalE`, `throttle`, `runtime`, `kmMil`, `starts`, `powerKw`, `powerCv`, `afr`, `fuelL100`, `altitude`, `airDensity`, `intercoolerEff`, `pressureRatio`, `volEff`, `driftPedal`, `deltaThrottlePedal`, `dfco`, `gearRatio`, `accel`, `boostRate`, `bsfc`.

In modalità debug `printAdvancedDiagnostics()` stampa tutti i 47 parametri anche su Serial (115200 baud) ad ogni ciclo: il flusso seriale rallenta sensibilmente il refresh dell'OLED, ed è infatti pensato come strumento di tuning a banco, non come modalità d'esercizio.

---

## 10. Auto-brightness OLED

Il contrasto del display è regolato in funzione della luce ambientale letta da un fotoresistore (LDR) sull'ADC `A0`. La logica vive in [`light_sensor.h`](light_sensor.h) ed è chiamata polled da `loop()` (`updateBrightness()`); nessun `IRAM_ATTR`, nessun ISR.

**Schema cablaggio** (identico nelle due varianti):

```
3.3V ──[ LDR ]── A0 ──[ 10 kΩ ]── GND
```

Più luce → R_LDR ↓ → V_A0 ↑ → ADC ↑ → contrasto ↑.

**State machine** — tre stati:

| Stato | Comportamento |
|---|---|
| `auto` | Contrasto target = `adcToContrast(LDR filtrato)`, applicato con isteresi 4 unità per evitare flicker. |
| `manual` | L'utente ha mosso lo slider del web dashboard. Il contrasto resta al valore manuale finché la luce ambientale non varia di più di `LDR_SCENE_DELTA` (default 150 conteggi ADC) rispetto al valore al momento dell'override. |
| `fading` | Transizione graduale verso il nuovo target automatico (`BR_FADE_STEP` = 2 unità ogni `BR_FADE_TICK_MS` = 30 ms → ~3.8 s per range pieno). Si entra qui da `manual` quando la scena cambia troppo, oppure dal bottone "Reset auto" del dashboard. |

**Filtraggio ADC**: EMA con α = 0.15 (≈ 2 s di assestamento al 95%) e clamp anti-spike che scarta sample isolati con |Δ| > 300 conteggi.

**Mapping ADC → contrasto**: gamma 2.0 sul range utile 30–950, output limitato a `BR_CONTRAST_MIN..BR_CONTRAST_MAX` (5..240 di default) per evitare di spegnere il pannello al buio assoluto e per non saturare il driver in pieno sole.

**Costanti tunabili** in [`light_sensor.h`](light_sensor.h):

| `#define` | Default | Significato |
|---|---|---|
| `LDR_SAMPLE_MS` | `250` | Periodo di campionamento (4 Hz) |
| `LDR_SCENE_DELTA` | `150` | Soglia uscita da `manual` (conteggi ADC) |
| `BR_HYST_DELTA` | `4` | Soglia minima per riapplicare `setContrast` |
| `BR_FADE_STEP` | `2` | Δ contrasto per tick di fade |
| `BR_FADE_TICK_MS` | `30` | Periodo del tick di fade |
| `BR_CONTRAST_MIN` | `5` | Floor (notte estrema) |
| `BR_CONTRAST_MAX` | `240` | Ceiling (sole pieno) |

**Web dashboard**: la card "Luce ambiente" mostra valore LDR live, contrasto applicato e modalità (`auto`/`manual`/`fading`); slider 0–255 attiva l'override manuale (debounced 200 ms verso `/brightness?set=N`); il bottone "Reset auto" forza il rientro automatico (`/brightness?auto`) con fade graduale verso il target calcolato. Lo stato override **non è persistente** fra reboot.

---

## 11. DTC (codici di errore)

- **Lettura**: PID `0x01` (Mode 01) per stato MIL e numero DTC, ogni `DTC_CHECK_INTERVAL` (30 s). Se `MIL=ON` e count > 0 viene fatta una **Mode 03** per leggere i codici.
  - Su CAN: gestione completa **ISO-TP** con Single Frame, First Frame + Flow Control + Consecutive Frames (fino a `MAX_DTC = 6` codici).
  - Su WiFi: parsing della risposta ASCII multi-line dell'ELM327, fino a 6 codici.
- **Decodifica**: la categoria (P/C/B/U) e i 4 nibble del codice vengono ricostruiti in stringa "P0301".
- **Descrizione breve**: ricerca binaria in [`dtc_descriptions.h`](dtc_descriptions.h) (~45 codici diesel/turbo più comuni in PROGMEM, italiano, ≤ 21 caratteri).
- **Display**: alternanza automatica fra schermata dati e pagine DTC ogni `SCREEN_SWITCH_MS` (5 s). Il click breve sul pulsante avanza manualmente la pagina e posticipa l'auto-cycle.
- **Cancellazione**: Mode 04 dal pulsante (long-press) o dal web dashboard. Sul ramo CAN si verifica la risposta positiva `0x44`; sul ramo WiFi si cerca `"44"` nel testo restituito dall'ELM327.

---

## 12. Layout file

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
├── ui_preview.html           preview offline dell'HTML servito da /dashboard
├── monitor_preview.html      preview offline della schermata OLED
├── DOCS/                     datasheet MCP2515, lista PID supportati Audi A5, output seriali
├── .vscode/                  config Arduino Community Edition (FQBN, build.extra_flags)
└── LICENSE
```

---

## 13. Note operative & limiti

- **Stime, non misure**: coppia, potenza e boost sono **stime** ricostruite da PID OBD2 standard. Sull'A5 2.7 TDI nessun PID standard espone direttamente la coppia ECU; il modello è calibrato sui parametri targati 400 Nm / 140 kW.
- **Plateau coppia**: il modello assume un plateau piatto a 400 Nm fra 1400 e 3250 rpm con caduta a potenza costante oltre. Se il motore è mappato/rimappato in modo non standard, riallineare `TORQUE_MAX_NM`, `POWER_MAX_KW` e i limiti del plateau in [`EngineConstants.h`](EngineConstants.h).
- **MAP saturato**: il PID 0x0B è 1 byte (0–255 kPa). Sopra ~2.5 bar assoluti il valore satura: il boost mostrato è quindi limitato a ~1.5 bar relativi.
- **Refresh display**: l'aggiornamento parziale (`u8g2.updateDisplayArea`) trasmette solo le tile rows modificate, riducendo il traffico I²C da ~1024 byte a ~256–384 byte per frame.
- **OTA dopo finestra**: scaduto `OTA_WINDOW_MS`, per riaggiornare il firmware è necessario un reset (power cycle); è una scelta deliberata per non tenere il radio acceso durante la guida.
- **ESP-01 con ELM327**: la variante WiFi è progettata per girare anche su ESP-01 (8 GPIO totali); su D1 Mini funziona identicamente ma con più pin liberi.

---

## 14. Licenza

Vedi [`LICENSE`](LICENSE).
