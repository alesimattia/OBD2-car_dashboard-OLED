# OBD2 Car Dashboard — OLED

Dashboard motore in tempo reale per **Audi A5 B8 2.7 TDI (CGKA, 140 kW) Multitronic**, basata su ESP8266 e display OLED SH1106 1.3" (128×64). Il progetto offre **due sketch alternativi** che condividono dashboard, modelli fisici e web server, e si differenziano solo per il transport verso l'ECU.

> ℹ️ **Anteprime live (interattive)**: senza clonare la repo puoi vederle direttamente nel browser tramite il proxy raw.githack.com:
> - 🖥️ Display OLED → [ui_preview.html (live)](https://raw.githack.com/alesimattia/OBD2-car_dashboard-OLED/main/ui_preview.html)
> - 🌐 Dashboard web → [monitor_preview.html (live)](https://raw.githack.com/alesimattia/OBD2-car_dashboard-OLED/main/monitor_preview.html)
>
> Le stesse anteprime sono embeddate via `iframe` nelle §3.1 e §11.6, ma GitHub non renderizza gli iframe nei README per motivi di sicurezza: l'embed funziona solo se apri questo file in locale.

---

## 📋 Indice

1. [Panoramica](#1-panoramica)
2. [Le due varianti — confronto rapido](#2-le-due-varianti--confronto-rapido)
3. [Architettura](#3-architettura)
   - [3.1 Anteprima offline del display OLED](#31-anteprima-offline-del-display-oled)
4. [Hardware e cablaggio](#4-hardware-e-cablaggio)
   - [4.1 Componenti comuni](#41-componenti-comuni)
   - [4.2 Variante CANbus_conn](#42-variante-canbus_conn)
   - [4.3 Variante WIFI_conn](#43-variante-wifi_conn)
5. [Veicolo target](#5-veicolo-target)
   - [5.1 PID supportati — variante WIFI_conn](#51-pid-supportati--variante-wifi_conn)
   - [5.2 PID supportati — variante CANbus_conn](#52-pid-supportati--variante-canbus_conn)
   - [5.3 Parametri monitorabili in più](#53-parametri-monitorabili-in-pi)
     - [5.3.1 Parametri "letti diretti" potenzialmente nuovi (post scan multi-ECU)](#531-parametri-letti-diretti-potenzialmente-nuovi-post-scan-multi-ecu)
     - [5.3.2 Parametri/calcoli ottenibili dai PID già letti](#532-parametricalcoli-ottenibili-dai-pid-gi-letti)
   - [5.4 Mode OBD2 e UDS — riferimento](#54-mode-obd2-e-uds--riferimento)
     - [5.4.1 Mode OBD2 standard (SAE J1979 / ISO 15031-5)](#541-mode-obd2-standard-sae-j1979--iso-15031-5)
     - [5.4.2 Mode UDS (ISO 14229-1 / ISO 15765-3)](#542-mode-uds-iso-14229-1--iso-15765-3)
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

## 2. Le due varianti — confronto rapido

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

## 3. Architettura

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

### 3.1 Anteprima offline del display OLED

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

## 4. Hardware e cablaggio

### 4.1 Componenti comuni

| Componente | Note |
|---|---|
| ESP8266 **D1 Mini** (clone) | 4 MB flash, partizione `4M (no FS)` |
| **OLED SH1106 1.3" I²C** | 128×64 monocromo, indirizzo `0x3C` |
| Pulsante NA verso GND | Debounce software 50 ms |
| Fotoresistore GL5528 + 10 kΩ pull-down | Voltage divider su `A0` per auto-brightness |

### 4.2 Variante `CANbus_conn`

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

### 4.3 Variante `WIFI_conn`

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

## 5. Veicolo target

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

#### Convenzione ECU ID OBD2

Sul bus CAN 11-bit le richieste OBD2 vengono inviate al **broadcast** `0x7DF`; ogni controllore presente risponde con il proprio **ID di risposta** nell'intervallo `0x7E8`–`0x7EF`. Sull'A5 B8 i moduli rilevanti sono:

| Response ID | Modulo | Ruolo |
|---|---|---|
| `0x7E8` | **ECU motore** (J623, ME17.5/EDC17 — TDI CGKA) | espone tutti i PID standard Mode 01 di motore, emissioni, EGR, turbo |
| `0x7E9` | **ECU cambio** (J217 — Multitronic CVT) | sui VAG pre-2010 risponde *raramente* in OBD2 standard; eventuali PID esposti riguardano stato CVT/temperatura |
| `0x7EA`–`0x7EF` | altri controllori (ABS, BCM, …) | tipicamente non rispondono al Mode 01 standard |

Nelle tabelle che seguono ogni PID è etichettato con l'ECU di provenienza. Lo scan al boot (§ scan multi-ECU in [`CANbus_conn/CANbus_conn.ino`](CANbus_conn/CANbus_conn.ino)) rivela quali response ID sono effettivamente attivi sul tuo veicolo specifico.

### 5.1 PID supportati — variante WIFI_conn

Elenco completo dei PID Mode 01 esposti dall'ECU rilevati tramite l'adattatore ELM327 WiFi (vedi [`DOCS/supported_pid_WIFI audi_A5.md`](DOCS/supported_pid_WIFI%20audi_A5.md) per le formule complete di conversione).

> 🔌 **Tutti i PID sotto sono esposti dall'ECU motore (`0x7E8`)**: l'ELM327 di default dialoga solo con il modulo motore (instaurato dal comando `ATSP6` in initialization), quindi questa tabella è limitata al *single-ECU view*.

| ECU | PID | Descrizione | Formula | Unità |
|---|---|---|---|---|
| `0x7E8` motore | `0x01` | Monitor status since DTCs cleared (MIL + #DTC) | bitmap | — |
| `0x7E8` motore | `0x04` | Calculated engine load | `A × 100 / 255` | % |
| `0x7E8` motore | `0x05` | Engine coolant temperature | `A − 40` | °C |
| `0x7E8` motore | `0x0B` | Intake manifold absolute pressure (MAP) | `A` | kPa |
| `0x7E8` motore | `0x0C` | Engine speed (RPM) | `(256 × A + B) / 4` | rpm |
| `0x7E8` motore | `0x0D` | Vehicle speed | `A` | km/h |
| `0x7E8` motore | `0x0F` | Intake air temperature (IAT) | `A − 40` | °C |
| `0x7E8` motore | `0x10` | MAF air flow rate | `(256 × A + B) / 100` | g/s |
| `0x7E8` motore | `0x13` | Oxygen sensors present (2 banks) | bitmap | — |
| `0x7E8` motore | `0x1C` | OBD standards conformance | enum | — |
| `0x7E8` motore | `0x1F` | Run time since engine start | `256 × A + B` | s |
| `0x7E8` motore | `0x20` | PIDs supported `[0x21–0x40]` | bitmap | — |
| `0x7E8` motore | `0x21` | Distance traveled with MIL on | `256 × A + B` | km |
| `0x7E8` motore | `0x23` | Fuel rail gauge pressure | `10 × (256 × A + B)` | kPa |
| `0x7E8` motore | `0x24` | Oxygen Sensor 1 — wide range (lambda + V) | `λ = 2 × (256·A+B)/65536`, `V = 8 × (256·C+D)/65536` | λ, V |
| `0x7E8` motore | `0x2C` | Commanded EGR | `A × 100 / 255` | % |
| `0x7E8` motore | `0x2D` | EGR error | `A × 100 / 128 − 100` | % |
| `0x7E8` motore | `0x30` | Warm-ups since codes cleared | `A` | conteggio |
| `0x7E8` motore | `0x31` | Distance traveled since codes cleared | `256 × A + B` | km |
| `0x7E8` motore | `0x33` | Absolute barometric pressure | `A` | kPa |
| `0x7E8` motore | `0x40` | PIDs supported `[0x41–0x60]` | bitmap | — |
| `0x7E8` motore | `0x41` | Monitor status this drive cycle | bitmap | — |
| `0x7E8` motore | `0x42` | Control module voltage | `(256 × A + B) / 1000` | V |
| `0x7E8` motore | `0x46` | Ambient air temperature | `A − 40` | °C |
| `0x7E8` motore | `0x49` | Accelerator pedal position D | `A × 100 / 255` | % |
| `0x7E8` motore | `0x4A` | Accelerator pedal position E | `A × 100 / 255` | % |
| `0x7E8` motore | `0x4C` | Commanded throttle actuator | `A × 100 / 255` | % |
| `0x7E8` motore | `0x4F` | Maximum values for lambda / O2 / current / MAP | `A`, `B` (V), `C` (mA), `D × 10` (kPa) | — |

> ℹ️ Non tutti questi PID vengono effettivamente letti a runtime: il round-robin (§8) interroga solo il sottoinsieme strettamente necessario per dashboard e modelli, lasciando gli altri come potenzialmente disponibili per estensioni future.

### 5.2 PID supportati — variante CANbus_conn

> 🚧 **Sezione da completare.** Questa tabella verrà popolata appena disponibile il dump dei PID supportati letti via MCP2515 (file atteso: `DOCS/supported_pid_CAN audi_A5.md`).
>
> In linea teorica, l'insieme dei PID esposti dall'ECU **non dipende dal transport** (CAN nativo vs. ELM327 WiFi): entrambi i percorsi parlano allo stesso modulo motore via OBD2 Mode 01. Possibili differenze attese:
> - eventuali PID che l'ELM327 filtra o non espone correttamente nella sua interfaccia ASCII (e che invece sul CAN nativo arrivano puliti);
> - eventuale jitter o frame mancanti su PID multi-byte ad alta frequenza, che l'ELM327 può mascherare.
>
> Una volta acquisito il file, replicare qui la stessa struttura tabellare di §5.1 e segnalare in nota le eventuali differenze rispetto alla variante WIFI.

| PID | Descrizione | Formula | Unità |
|---|---|---|---|
| _da popolare_ | _da popolare_ | _da popolare_ | _da popolare_ |

### 5.3 Parametri monitorabili in più

Le due sotto-sezioni elencano parametri **non ancora attivi** nel monitor mode che però sono ragionevolmente abilitabili. Si distinguono per origine:

- **§5.3.1** — *nuovi* parametri resi visibili dallo **scan multi-ECU** introdotto al boot (vedi `executeScanMode` e [`pid_descriptions.h`](pid_descriptions.h)): potenzialmente leggibili da PID standard SAE J1979 supportati dall'ECU motore o dal cambio. Sono *potenziali*: la lista effettiva dipende dai PID che lo scan rileva come supportati al boot.
- **§5.3.2** — derivati dai **PID già letti** (lista §5.1) che il firmware potrebbe calcolare senza bisogno di nuove richieste OBD: il dato è già disponibile, manca solo il calcolo/visualizzazione.

#### 5.3.1 Parametri "letti diretti" potenzialmente nuovi (post scan multi-ECU)

> ⚠️ Questa lista è *potenziale*: la modifica ha aggiunto solo lo **scan diagnostico** — il monitor mode legge ancora gli stessi PID di prima. La lista qui sotto è quindi una mappa del *potenziale*: cosa potresti aggiungere al monitor in futuro, ovvero PID standard SAE che la A5 2.7 TDI CGKA potrebbe esporre. La risposta dello scan al boot dirà quali sono effettivamente supportati dall'ECU motore (e dal cambio se risponde).

> 🔌 Salvo dove indicato altrimenti, tutti i PID nei sottoparagrafi seguenti si riferiscono all'**ECU motore (`0x7E8`)**.

##### Motore — termici e lubrificazione

| ECU | PID | Parametro | Note |
|---|---|---|---|
| `0x7E8` motore | `0x5C` | Temperatura olio motore | utile su CGKA, l'olio è il primo a soffrire |
| `0x7E8` motore | `0x6B` | Temperatura EGR | gas che entrano in ricircolo |
| `0x7E8` motore | `0x77` | Temperatura intercooler/CACT | aria sovralimentata dopo lo scambiatore |
| `0x7E8` motore | `0x78` | Temperatura gas di scarico (EGT) Banco 1 | uscita turbina |
| `0x7E8` motore | `0x7C` | Temperatura DPF | cruciale per stato rigenerazione |

##### Iniezione e carburante

| ECU | PID | Parametro | Note |
|---|---|---|---|
| `0x7E8` motore | `0x5D` | Anticipo iniezione | diagnostica strategie ECU sotto carico |
| `0x7E8` motore | `0x5E` | Consumo carburante motore | l/h diretto dall'ECU, più accurato di MAF/AFR |
| `0x7E8` motore | `0x22` | Pressione rail relativa al collettore | oggi leggi 0x23 assoluta |
| `0x7E8` motore | `0x2F` | Livello serbatoio % | sostituisce stima con consumo |

##### Coppia (potenzialmente molto utile su CGKA)

| ECU | PID | Parametro | Note |
|---|---|---|---|
| `0x7E8` motore | `0x61` | Coppia richiesta dal pilota % | — |
| `0x7E8` motore | `0x62` | Coppia effettiva motore % | oggi la **stimi** con `TorqueModel`; con questo è il valore *vero* dell'ECU |
| `0x7E8` motore | `0x63` | Coppia di riferimento motore (Nm) | il "100%" dell'ECU; oggi assumi 400 Nm hardcoded |

##### Turbo e scarico

| ECU | PID | Parametro | Note |
|---|---|---|---|
| `0x7E8` motore | `0x70` | Boost pressure control | target ECU vs reale |
| `0x7E8` motore | `0x71` | Controllo geometria variabile turbo (VGT) | apertura palette |
| `0x7E8` motore | `0x73` | Pressione gas di scarico | a monte/valle DPF se ci sono due sensori |
| `0x7E8` motore | `0x74` | Regime turbocompressore | RPM turbina, raro ma se c'è è oro |

##### Farfalla / aspirazione

| ECU | PID | Parametro | Note |
|---|---|---|---|
| `0x7E8` motore | `0x11` | Posizione farfalla | sui diesel comandata per ricircolo EGR e cut-off |
| `0x7E8` motore | `0x45` | Posizione relativa farfalla | — |
| `0x7E8` motore | `0x47` / `0x48` | Posizione assoluta farfalla B/C | — |

##### Cambio Multitronic (esposti da `0x7E9` se risponde)

| ECU | PID | Parametro | Note |
|---|---|---|---|
| `0x7E9` cambio | `0xA4` | Marcia attuale | su Multitronic = "rapporto" virtuale, da verificare |
| `0x7E9` cambio | `0x05` | Temperatura liquido del cambio | Multitronic CGKA è famigerato per surriscaldamento — se esposto è il PID più importante |

##### Contatori / diagnostica

| ECU | PID | Parametro | Note |
|---|---|---|---|
| `0x7E8` motore | `0x4D` | Tempo motore con MIL accesa | in ore, oggi leggi solo i km |
| `0x7E8` motore | `0x4E` | Tempo dal clear DTC | — |
| `0x7E8` motore | `0x31` | Distanza percorsa dal clear DTC | — |
| `0x7E8` motore | `0x43` | Carico assoluto | carico % normalizzato sul max teorico, diverso da `0x04` |
| `0x7E8` motore | `0xA6` | Contachilometri | odometro totale, raramente esposto |

##### Valori calcolati nuovi (derivati dai PID sopra)

> Solo se i PID sorgente risultano supportati allo scan.

###### Coppia / potenza più accurate

| Calcolo | Formula | Beneficio |
|---|---|---|
| Coppia reale (Nm) | `(0x62 / 100) × 0x63` | sostituisce `TorqueModel` con il valore ECU |
| Potenza reale (kW) | `coppiaNm × RPM / 9549` | oggi calcoli con coppia stimata |
| Δ coppia richiesta vs effettiva | `0x61 − 0x62` | mostra quando l'auto non riesce a soddisfare la domanda (limp mode, boost insufficiente, cut-off) |

###### Salute turbo

| Calcolo | Formula | Beneficio |
|---|---|---|
| Rapporto VGT vs RPM | `0x71 / RPM` | curva di pilotaggio palette (utile per scoprire palette grippate) |
| Pressione differenziale scarico | `0x73 − BARO` | carico sul DPF |
| Backpressure ratio | `0x73 / MAP` | indicatore intasamento scarico |

###### Salute intercooler (più accurato di quello attuale)

Oggi calcoli `intercoolerEff` con `0x33 BARO`, `0x46 ambient`, `0x0F IAT`. Con `0x77 CACT` misuri direttamente la temperatura uscita intercooler invece che usare IAT (che è dopo l'intercooler ma prima del collettore):

| Calcolo | Formula | Beneficio |
|---|---|---|
| Efficienza intercooler reale | `(T_ambient − CACT) / (T_ambient − T_uscita_compressore)` | più accurato (richiede anche EGT) |

###### Stato DPF

| Calcolo | Formula | Beneficio |
|---|---|---|
| Drift temperatura DPF | `d(0x7C)/dt` | durante una rigenerazione attiva sale rapidamente; permette di rilevare *automaticamente* quando l'auto sta rigenerando |
| Temperatura DPF a regime | media mobile di `0x7C` sopra una soglia | indicatore catalizzatore funzionante |

###### Iniezione / efficienza

| Calcolo | Formula | Beneficio |
|---|---|---|
| Consumo l/100km diretto | `0x5E × 100 / (velocità × densità diesel)` | oggi lo derivi da `MAF/lambda`, con `0x5E` è più preciso (specialmente in transitorio) |
| Mappa anticipo vs RPM | `0x5D` graficato contro RPM | curva caratteristica della rimappatura |

###### Cambio Multitronic (se `0x7E9` espone PID)

| ECU sorgenti | Calcolo | Formula | Beneficio |
|---|---|---|---|
| `0x7E9` + `0x7E8` | Temperatura olio cambio vs tempo motore | `0x05@7E9` vs `0x1F@7E8` | pendenza di riscaldamento, utile per individuare surriscaldamento patologico |
| `0x7E8` motore | Slip frizione virtuale | `RPM_motore × π × diam_ruota / (60 × velocità × rapporto_finale)` confrontato col rapporto attuale | indica usura/slittamento (calcolo già parziale presente come `gearRatio`) |

##### Considerazioni pratiche

- I PID `0x60–0x9F` sono "estensioni diesel/HD" SAE J1939/J1979-2: alcuni sono comuni su VAG diesel post-2008, **ma non garantiti**. Lo scan dirà quali sono effettivamente esposti.
- I PID di **coppia reale (`0x62`, `0x63`)** sono i più impattanti se supportati: eliminerebbero l'incertezza dell'attuale `TorqueModel` e permetterebbero anche di rimuovere `BoostModel` per la stima di carico.
- **Cambio Multitronic**: storicamente le centraline VAG pre-2010 espongono *poco* via OBD2 standard sull'ECU cambio (`0x7E9`). I dati ricchi (temperatura olio CVT, slittamento, conta cicli) vivono spesso solo su Mode 22 UDS con label files VAG.
- Alcuni di questi PID (es. `0x70-0x74`) sono "container": i 4 byte di payload vanno decodificati in più sottocampi. Implementare la lettura richiede attenzione al data layout SAE J1979.

---

#### 5.3.2 Parametri/calcoli ottenibili dai PID già letti

Lista filtrata sui soli PID che la A5 espone secondo §5.1. Tutti i dati sono *già in possesso* del firmware (round-robin o letture handler `/data`), manca solo il calcolo/esposizione.

> 🔌 Tutti i PID e i calcoli di questa sotto-sezione si riferiscono all'**ECU motore (`0x7E8`)**, in coerenza con la lista di §5.1.

##### PID supportati ma non ancora interrogati a runtime

| ECU | PID | Parametro | Beneficio |
|---|---|---|---|
| `0x7E8` motore | `0x31` | Distanza percorsa dal clear DTC | odometro parziale dal reset, distinto da `0x21` (km con MIL) |
| `0x7E8` motore | `0x41` | Monitor status drive cycle | bitmap dei monitor di emissioni completati nel ciclo corrente — **utile in pre-revisione** per sapere se l'auto è "Ready" |
| `0x7E8` motore | `0x13` | Sonde lambda presenti (2 banchi) | informazione statica una tantum (configurazione veicolo) |
| `0x7E8` motore | `0x1C` | Standard OBD del veicolo | informazione statica (tipo OBD: EOBD, OBD-II, ecc.) |
| `0x7E8` motore | `0x4F` | Massimi calibrazione (lambda, V/I sonda, MAP) | informazione statica utile per validare letture nel range |

##### Valori calcolati nuovi dai PID già letti

###### Stile d'uso e percorrenze

| Calcolo | Formula | Beneficio |
|---|---|---|
| Km medi per warm-up | `0x31 / 0x30` | distingue uso urbano da lunga percorrenza |
| % distanza con MIL on | `0x21 / 0x31 × 100` | frazione del totale percorso dal reset con guasto attivo |
| Velocità media sessione | `(odometro accumulato) / 0x1F` | trip avg dall'avviamento (richiede integrazione di `0x0D` nel tempo) |
| Carico medio sessione | media mobile di `0x04` | indicatore di stile guida (sportivo vs cruise) |
| Peak hold sessione | max(`boostBar`), max(`RPM`), max(coppia stimata), max(`0x05`) | "watermark" della sessione corrente |

###### Diagnostica passiva (su PID già letti)

| Calcolo | Formula | Beneficio |
|---|---|---|
| Rampa riscaldamento liquido | `d(0x05)/dt` nei primi 5 min dopo avviamento | individua termostato bloccato aperto (rampa lenta < 2 °C/min) |
| Pressione differenziale aspirazione @ idle | `0x33 − 0x0B` con RPM ≈ idle, pedale = 0 | indicatore filtro aria intasato (∆P > soglia tipica) |
| Drift attuatore farfalla EGR | `0x4C − media(0x49, 0x49 + 0x4A)` | quando il pedale chiama coppia ma la farfalla EGR non risponde |
| Drift pedale D vs E (già calcolato) | abs(`0x49 − 0x4A`) | già esposto come `driftPedal` in dashboard |
| Cut-off iniezione (DFCO, già calcolato) | `0x04 < 1.0 %` | già esposto come `dfco` in dashboard |
| Indicatore rigenerazione DPF attiva | EGR ≈ 0% + RPM stabile in cruise + IAT alta + boost stabile elevato | euristica per rilevare cicli di rigenerazione (richiede tracking temporale) |

###### Efficienza e prestazioni (su dati già letti)

| Calcolo | Formula | Beneficio |
|---|---|---|
| BSFC (già calcolato) | `(MAF × 3600) / powerKw` | grammi di gasolio per kWh; già esposto come `bsfc` |
| Efficienza volumetrica (già calcolata) | `MAF × 120 / (cilindrata × RPM × ρ_aria)` | già esposto come `volEff` |
| Potenza istantanea (già calcolata) | `coppia × RPM / 9549` | già esposto come `powerKw`/`powerCv` |
| Consumo l/100km (già calcolato) | `(MAF / (14.5 × λ)) × 3600 / (835 × velocità) × 100` | già esposto come `fuelL100` |

> ℹ️ Molti dei calcoli "Efficienza e prestazioni" sono già implementati in `handleData()` di [`web_dashboard.h`](web_dashboard.h) e visibili in dashboard quando `debug=true`. Sono qui per chiarezza che la lista è esaustiva.

### 5.4 Mode OBD2 e UDS — riferimento

Il **Mode** (o **Service ID**) è il "verbo" del protocollo diagnostico: dice all'ECU *cosa fare*. Il dato specifico (il "complemento oggetto") è un identificatore che si chiama **PID** sotto OBD2 standard e **DID** sotto UDS. Un frame CAN tipico:

```
OBD2:  [ length ] [ Mode ] [ PID ]              esempio: 02 01 0C        (leggi RPM)
UDS:   [ length ] [ Mode ] [ DID hi ] [ DID lo ] esempio: 03 22 11 40    (leggi DID 0x1140)
```

Risposta = `Mode + 0x40`. Errore = `0x7F <Mode> <NRC>` (Negative Response Code).

| Aspetto | OBD2 (Mode `0x01`-`0x0A`) | UDS (Mode `0x10`+) |
|---|---|---|
| **A chi parla** | broadcast `0x7DF`, *chiunque risponde* | indirizzato a *un* ECU specifico (es. `0x7E0` motore, `0x7E1` cambio) |
| **Dati esposti** | solo PID standard SAE J1979 (~150 PID, definizione pubblica) | qualsiasi DID il costruttore decide di esporre (16 bit = 65k, definizioni *segrete* fuori dai label files VCDS/ODIS) |
| **Scopo legale** | obbligo emissioni (revisione/MOT/EOBD) | diagnosi e manutenzione costruttore |
| **Cosa puoi fare** | leggere | leggere, scrivere, comandare attuatori, flashare, configurare |

#### 5.4.1 Mode OBD2 standard (SAE J1979 / ISO 15031-5)

Convenzione **Monitored**:
- ✅ **letto live** — interrogato nel round-robin del monitor o handler `/data`
- 🔍 **scoperto** — rilevato dallo scan al boot ma non letto live
- ➖ **non interrogato** — il firmware nemmeno lo richiede

Convenzione **Standard**:
- ✅ **standard** — definito in SAE J1979 / ISO 14229, decodifica pubblica
- ⚠️ **reverse-engineering** — definito dal costruttore, serve label file VCDS o forum VAG

| Mode | ECU | PID | Nome | Standard | Monitored |
|---|---|---|---|---|---|
| `0x01` | `0x7E8` motore | `0x00` | Bitmask PID supportati 01-20 | ✅ SAE J1979 | 🔍 scoperto |
| `0x01` | `0x7E8` motore | `0x01` | Stato MIL + numero DTC | ✅ SAE J1979 | ✅ letto live (`checkMILStatus`) |
| `0x01` | `0x7E8` motore | `0x04` | Carico motore calcolato | ✅ SAE J1979 | ✅ letto live |
| `0x01` | `0x7E8` motore | `0x05` | Temperatura liquido raffreddamento | ✅ SAE J1979 | ✅ letto live |
| `0x01` | `0x7E8` motore | `0x0B` | Pressione assoluta collettore (MAP) | ✅ SAE J1979 | ✅ letto live |
| `0x01` | `0x7E8` motore | `0x0C` | Regime motore (RPM) | ✅ SAE J1979 | ✅ letto live |
| `0x01` | `0x7E8` motore | `0x0D` | Velocità veicolo | ✅ SAE J1979 | ✅ letto live (debug `/data`) |
| `0x01` | `0x7E8` motore | `0x0F` | Temperatura aria aspirata (IAT) | ✅ SAE J1979 | ✅ letto live |
| `0x01` | `0x7E8` motore | `0x10` | Portata massa aria (MAF) | ✅ SAE J1979 | ✅ letto live |
| `0x01` | `0x7E8` motore | `0x13` | Sonde lambda presenti (2 banchi) | ✅ SAE J1979 | 🔍 scoperto |
| `0x01` | `0x7E8` motore | `0x1C` | Standard OBD del veicolo | ✅ SAE J1979 | 🔍 scoperto |
| `0x01` | `0x7E8` motore | `0x1F` | Tempo motore dall'avviamento | ✅ SAE J1979 | ✅ letto live |
| `0x01` | `0x7E8` motore | `0x20` | Bitmask PID supportati 21-40 | ✅ SAE J1979 | 🔍 scoperto |
| `0x01` | `0x7E8` motore | `0x21` | Distanza percorsa con MIL accesa | ✅ SAE J1979 | ✅ letto live |
| `0x01` | `0x7E8` motore | `0x23` | Pressione rail (gauge) | ✅ SAE J1979 | ✅ letto live |
| `0x01` | `0x7E8` motore | `0x24` | Sonda lambda 1 — wide range (λ + V) | ✅ SAE J1979 | ✅ letto live |
| `0x01` | `0x7E8` motore | `0x2C` | EGR comandato | ✅ SAE J1979 | ✅ letto live |
| `0x01` | `0x7E8` motore | `0x2D` | Errore EGR | ✅ SAE J1979 | ✅ letto live |
| `0x01` | `0x7E8` motore | `0x30` | Warm-up dal clear DTC | ✅ SAE J1979 | ✅ letto live |
| `0x01` | `0x7E8` motore | `0x31` | Distanza dal clear DTC | ✅ SAE J1979 | 🔍 scoperto |
| `0x01` | `0x7E8` motore | `0x33` | Pressione barometrica | ✅ SAE J1979 | ✅ letto live |
| `0x01` | `0x7E8` motore | `0x40` | Bitmask PID supportati 41-60 | ✅ SAE J1979 | 🔍 scoperto |
| `0x01` | `0x7E8` motore | `0x41` | Monitor status drive cycle | ✅ SAE J1979 | 🔍 scoperto |
| `0x01` | `0x7E8` motore | `0x42` | Tensione modulo (batteria) | ✅ SAE J1979 | ✅ letto live |
| `0x01` | `0x7E8` motore | `0x46` | Temperatura aria ambiente | ✅ SAE J1979 | ✅ letto live |
| `0x01` | `0x7E8` motore | `0x49` | Pedale acceleratore D | ✅ SAE J1979 | ✅ letto live |
| `0x01` | `0x7E8` motore | `0x4A` | Pedale acceleratore E | ✅ SAE J1979 | ✅ letto live |
| `0x01` | `0x7E8` motore | `0x4C` | Attuatore farfalla comandato | ✅ SAE J1979 | ✅ letto live |
| `0x01` | `0x7E8` motore | `0x4F` | Massimi calibrazione (λ/O2/MAP) | ✅ SAE J1979 | 🔍 scoperto |
| `0x01` | `0x7E8` motore | `0x5C` | Temperatura olio motore | ✅ SAE J1979 | ➖ non interrogato (potenziale §5.3.1) |
| `0x01` | `0x7E8` motore | `0x5D` | Anticipo iniezione | ✅ SAE J1979 | ➖ non interrogato (potenziale) |
| `0x01` | `0x7E8` motore | `0x5E` | Consumo carburante motore (l/h) | ✅ SAE J1979 | ➖ non interrogato (potenziale) |
| `0x01` | `0x7E8` motore | `0x62` | Coppia effettiva motore (%) | ✅ SAE J1979 | ➖ non interrogato (potenziale) |
| `0x01` | `0x7E8` motore | `0x63` | Coppia di riferimento motore (Nm) | ✅ SAE J1979 | ➖ non interrogato (potenziale) |
| `0x01` | `0x7E8` motore | `0x6B` | Temperatura EGR | ✅ SAE J1979 | ➖ non interrogato (potenziale) |
| `0x01` | `0x7E8` motore | `0x70` | Boost pressure control | ✅ SAE J1979 | ➖ non interrogato (potenziale) |
| `0x01` | `0x7E8` motore | `0x71` | Controllo VGT (geometria turbo) | ✅ SAE J1979 | ➖ non interrogato (potenziale) |
| `0x01` | `0x7E8` motore | `0x77` | Temperatura intercooler (CACT) | ✅ SAE J1979 | ➖ non interrogato (potenziale) |
| `0x01` | `0x7E8` motore | `0x78` | Temperatura gas scarico (EGT) B1 | ✅ SAE J1979 | ➖ non interrogato (potenziale) |
| `0x01` | `0x7E8` motore | `0x7C` | Temperatura DPF | ✅ SAE J1979 | ➖ non interrogato (potenziale) |
| `0x01` | `0x7E9` cambio | qualsiasi | (PID Mode 01 dal cambio Multitronic) | ✅ SAE J1979 | 🔍 scoperto al boot, mai letti live |
| `0x02` | `0x7E8` motore | `0x02 [DTC]` | Freeze frame: snapshot al momento del DTC | ✅ SAE J1979 | ➖ non interrogato |
| `0x03` | `0x7E8` motore | — | Lettura DTC stored (con MIL) | ✅ SAE J1979 | ✅ letto live (`readDTCCodes`, ogni 30 s) |
| `0x04` | `0x7E8` motore | — | Cancellazione DTC + reset readiness | ✅ SAE J1979 | ✅ usato (`clearDTCsViaCAN`, on-demand) |
| `0x05` | n/a | n/a CAN | O2 sensor monitoring (legacy non-CAN) | ✅ SAE J1979 | ➖ non applicabile (A5 è CAN) |
| `0x06` | `0x7E8` motore | TID `0x01..0xFF` | Test results monitoring (catalizzatore, EGR, DPF) | ⚠️ TID definiti dal costruttore | ➖ non interrogato (TID opachi senza label) |
| `0x07` | `0x7E8` motore | — | Pending DTC (in osservazione) | ✅ SAE J1979 | ➖ non interrogato |
| `0x08` | `0x7E8` motore | TID | Control on-board (forza attuatori) | ⚠️ TID definiti dal costruttore | ➖ non interrogato (rischioso) |
| `0x09` | `0x7E8` motore | `0x02` | VIN del veicolo | ✅ SAE J1979 | ➖ non interrogato (valutato non utile) |
| `0x09` | `0x7E8` motore | `0x04` | Calibration ID (versione SW) | ✅ SAE J1979 | ➖ non interrogato (valutato non utile) |
| `0x09` | `0x7E8` motore | `0x06` | Calibration Verification Number (CVN) | ✅ SAE J1979 | ➖ non interrogato (valutato non utile) |
| `0x09` | `0x7E8` motore | `0x0A` | ECU Name | ✅ SAE J1979 | ➖ non interrogato (valutato non utile) |
| `0x09` | `0x7E8` motore | `0x0B` | IPT diesel (counter readiness) | ✅ SAE J1979 | ➖ non interrogato (valutato non utile) |
| `0x0A` | `0x7E8` motore | — | Permanent DTC (non cancellabili) | ✅ SAE J1979 | ➖ non interrogato |

#### 5.4.2 Mode UDS (ISO 14229-1 / ISO 15765-3)

I servizi diagnostici "estesi" usati dai costruttori (VAG VCDS, BMW INPA, Mercedes XENTRY, ecc.). Richiedono comunicazione **indirizzata** verso ID specifici per ECU. Non c'è "PID" ma **DID** (16 bit) o **Sub-funzione** (8 bit) a seconda del Mode.

> ℹ️ Per moduli diversi da motore/cambio (ABS `0x740/0x748`, BCM `0x731/0x73B`, cruscotto `0x714/0x77E`, ecc.) lo stesso schema si replica cambiando solo la coppia di ID. La tabella usa motore (`0x7E0/0x7E8`) come riferimento.

| Mode | ECU (req → resp) | DID / Sub | Nome | Standard | Monitored |
|---|---|---|---|---|---|
| `0x10` | `0x7E0` → `0x7E8` motore | sub `0x01` | Default Diagnostic Session | ✅ ISO 14229 | ➖ non interrogato |
| `0x10` | `0x7E0` → `0x7E8` motore | sub `0x02` | Programming Session (per flash) | ✅ ISO 14229 | ➖ non interrogato |
| `0x10` | `0x7E0` → `0x7E8` motore | sub `0x03` | Extended Diagnostic Session | ✅ ISO 14229 | ➖ non interrogato |
| `0x10` | `0x7E1` → `0x7E9` cambio | sub `0x03` | Extended Session sul Multitronic | ✅ ISO 14229 | ➖ non interrogato |
| `0x11` | `0x7E0` → `0x7E8` motore | sub `0x01` | ECU Reset hard | ✅ ISO 14229 | ➖ non interrogato |
| `0x11` | `0x7E0` → `0x7E8` motore | sub `0x03` | ECU Reset soft | ✅ ISO 14229 | ➖ non interrogato |
| `0x14` | `0x7E0` → `0x7E8` motore | DTC mask | Clear Diagnostic Information (selettivo) | ✅ ISO 14229 | ➖ non interrogato (usiamo Mode 04 OBD2) |
| `0x19` | `0x7E0` → `0x7E8` motore | sub `0x02` | Read DTC by status mask | ✅ ISO 14229 | ➖ non interrogato (usiamo Mode 03 OBD2) |
| `0x19` | `0x7E0` → `0x7E8` motore | sub `0x06` | Read DTC extended data record | ✅ ISO 14229 | ➖ non interrogato |
| `0x22` | `0x7E1` → `0x7E9` cambio | DID `0x1140` | Temperatura olio CVT (DID forum VAG, da verificare) | ⚠️ reverse-engineering VAG | ➖ non interrogato |
| `0x22` | `0x7E0` → `0x7E8` motore | DID `0x028C` | DPF soot mass (esempio noto VAG) | ⚠️ reverse-engineering VAG | ➖ non interrogato |
| `0x22` | `0x7E0` → `0x7E8` motore | DID `0xF190` | VIN via UDS | ✅ ISO 14229 (DID standard) | ➖ non interrogato |
| `0x22` | `0x7E0` → `0x7E8` motore | DID `0xF18C` | ECU Serial Number | ✅ ISO 14229 (DID standard) | ➖ non interrogato |
| `0x22` | `0x7E0` → `0x7E8` motore | DID `0xF187` | ECU Part Number | ✅ ISO 14229 (DID standard) | ➖ non interrogato |
| `0x22` | `0x7E0` → `0x7E8` motore | DID arbitrario | Read Data By Identifier (caso generico) | ⚠️ DID applicativi proprietari | ➖ non interrogato |
| `0x23` | `0x7E0` → `0x7E8` motore | indirizzo + lunghezza | Read Memory By Address (richiede `0x27`) | ✅ ISO 14229 | ➖ non interrogato |
| `0x24` | `0x7E0` → `0x7E8` motore | DID | Read Scaling Data (metadati DID) | ✅ ISO 14229 | ➖ non interrogato |
| `0x27` | `0x7E0` → `0x7E8` motore | sub `0x01` (req seed) | Security Access — request seed | ⚠️ algoritmo seed/key proprietario | ➖ non interrogato |
| `0x27` | `0x7E0` → `0x7E8` motore | sub `0x02` (send key) | Security Access — send key | ⚠️ algoritmo seed/key proprietario | ➖ non interrogato |
| `0x28` | `0x7E0` → `0x7E8` motore | sub `0x00`-`0x03` | Communication Control (silenzia rete) | ✅ ISO 14229 | ➖ non interrogato |
| `0x29` | `0x7E0` → `0x7E8` motore | sub vari | Authentication (UDS 2020+) | ✅ ISO 14229 | ➖ non interrogato (n/a CGKA) |
| `0x2A` | `0x7E0` → `0x7E8` motore | DID + freq | Read Data By Periodic Identifier | ✅ ISO 14229 | ➖ non interrogato |
| `0x2C` | `0x7E0` → `0x7E8` motore | DID dinamico | Dynamically Define DID | ✅ ISO 14229 | ➖ non interrogato |
| `0x2E` | `0x7E0` → `0x7E8` motore | DID + payload | Write Data By Identifier (codifica) | ⚠️ DID applicativi proprietari | ➖ non interrogato |
| `0x2F` | `0x7E0` → `0x7E8` motore | DID + control | Input/Output Control (test attuatore) | ⚠️ DID applicativi proprietari | ➖ non interrogato |
| `0x31` | `0x7E0` → `0x7E8` motore | sub `0x01` + RID | Routine Control: rigenerazione DPF, reset adattamenti | ⚠️ RID proprietari | ➖ non interrogato |
| `0x34` | `0x7E0` → `0x7E8` motore | indirizzo + lunghezza | Request Download (rimappatura) | ✅ ISO 14229 | ➖ non interrogato |
| `0x35` | `0x7E0` → `0x7E8` motore | indirizzo + lunghezza | Request Upload (backup ECU) | ✅ ISO 14229 | ➖ non interrogato |
| `0x36` | `0x7E0` → `0x7E8` motore | counter + dati | Transfer Data (durante 0x34/0x35) | ✅ ISO 14229 | ➖ non interrogato |
| `0x37` | `0x7E0` → `0x7E8` motore | — | Request Transfer Exit | ✅ ISO 14229 | ➖ non interrogato |
| `0x38` | `0x7E0` → `0x7E8` motore | sub + filename | Request File Transfer (UDS 2013+) | ✅ ISO 14229 | ➖ non interrogato |
| `0x3D` | `0x7E0` → `0x7E8` motore | indirizzo + dati | Write Memory By Address (richiede `0x27`) | ✅ ISO 14229 | ➖ non interrogato |
| `0x3E` | `0x7E0` → `0x7E8` motore | sub `0x00` | Tester Present (keep-alive sessione) | ✅ ISO 14229 | ➖ non interrogato |
| `0x83` | `0x7E0` → `0x7E8` motore | sub | Access Timing Parameter | ✅ ISO 14229 | ➖ non interrogato |
| `0x84` | `0x7E0` → `0x7E8` motore | sub | Secured Data Transmission | ✅ ISO 14229 | ➖ non interrogato (n/a CGKA) |
| `0x85` | `0x7E0` → `0x7E8` motore | sub `0x01`/`0x02` | Control DTC Setting (on/off registrazione) | ✅ ISO 14229 | ➖ non interrogato |
| `0x86` | `0x7E0` → `0x7E8` motore | sub + DID | Response On Event (notifiche async) | ✅ ISO 14229 | ➖ non interrogato |
| `0x87` | `0x7E0` → `0x7E8` motore | sub | Link Control (cambio velocità bus) | ✅ ISO 14229 | ➖ non interrogato |

> ℹ️ Risposta a un Mode UDS = `Mode + 0x40` (es. `0x22` → `0x62`). Errore = frame `0x7F <Mode> <NRC>` con NRC notevoli: `0x11` Service Not Supported, `0x22` Conditions Not Correct, `0x31` Request Out Of Range, `0x33` Security Access Denied, `0x78` Response Pending (l'ECU sta lavorando — non considerare timeout).

> ℹ️ **DID UDS sull'A5**: `0xF190` (VIN), `0xF18C` (Serial), `0xF187` (Part Number) sono **standard ISO 14229** e affidabili. I DID applicativi VAG come `0x028C` (DPF soot), `0x1140` (temp CVT) sono **non standard**: derivati da reverse engineering pubblicato sui forum VAG, vanno verificati per la specifica ECU. Senza i label files Ross-Tech la lista è incompleta.

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

Il progetto è strutturato come **due sketch fratelli** in sottocartelle, con header condivisi nella root del repo. Per Arduino IDE / `arduino-cli` ogni sketch deve poter trovare gli `.h` condivisi: il workspace usa `["build.extra_flags", "-I<absolute-project-root>"]` in `arduino.json` (vedi `.vscode/`). Gli `#include` di `EngineConstants.h` in `BoostModel.h` e `TorqueModel.h` sono volutamente **path assoluti** perché Arduino IDE non risolve i path relativi fra header e sketch in cartelle diverse.

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

### `TorqueModel.h` — stima coppia motore

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
| `/serial-data` | GET | `text/plain` | byte da `?since=N` (cursor del client) fino al più recente nel buffer circolare di `WebSerial`; header `X-Seq:<n>` (nuovo cursor) e `X-Dropped:0\|1` (overflow). Polling consigliato a 250 ms dalla tab "Serial monitor" |
| `/scan` (solo CAN) | GET | HTML | pagina di riepilogo scan multi-ECU (mirror della tab "PID supportati") |
| `/scan-data` (solo CAN) | GET | JSON | `{ecus:[{id,totalRaw,pids:[]}…]}` — alimenta la tab "PID supportati" |
| `/update` (CAN) `/ota` (WiFi) | GET / POST | upload firmware | servito da `ESP8266HTTPUpdateServer` |

> ⚠️ Nessuna autenticazione, CORS o rate limiting: il modello di minaccia è la sola rete locale del SoftAP / dell'ELM327.

### 11.3 Layout della dashboard

La pagina è suddivisa in **3 tab** (mobile-first, larghezza max 480 px):

#### Tab 1 — **Dashboard** (default)

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

#### Tab 2 — **PID supportati**

Risultato dello scan multi-ECU: per ogni ECU rilevata sul bus mostra l'identificativo (es. `7E8`, `7E9`), il totale dei PID supportati e l'elenco con descrizioni SAE. Dati da `/scan-data`. Sul build WiFi è single-ECU (ELM327 inoltra solo l'ECU motore).

#### Tab 3 — **Serial monitor**

Specchio web del Serial monitor di Arduino IDE. Mostra tutto ciò che il firmware scrive su `Serial.*`, inclusi i blocchi `=== DIAGNOSTICA COMPLETA ===` periodici quando `debugMode=true`. Cattura tramite il wrapper `WebSerial` (vedi [`serial_logger.h`](serial_logger.h)) che inoltra ogni byte sia alla UART USB sia a un buffer circolare in RAM da 4 KB. Polling client a 250 ms su `/serial-data` con cursor `since=N`; se il buffer va in overflow tra due fetch viene segnalato `X-Dropped:1` e mostrato un banner nella tab. Auto-scroll che si sospende se l'utente scrolla in alto, con stato `[scroll bloccato]` in basso a destra. Pulsanti "A fine" e "Pulisci" (clear locale del `<pre>`, non del buffer firmware).

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
├── web_dashboard.h           server HTTP + JSON + 3 tab, condiviso (gating via #define OBD_CONN_*)
├── serial_logger.h           wrapper WebSerial: cattura Serial.* in buffer circolare per /serial-data
├── BoostModel.h              modello pressione turbo (namespace Audi27TDI140kW::Boost)
├── TorqueModel.h             modello coppia (namespace Audi27TDI140kW::Torque)
├── EngineConstants.h         tutti i parametri motore (namespace Audi27TDI140kW)
├── button_handler.h          debounce + state machine pulsante (BTN_NONE/SHORT/LONG)
├── light_sensor.h            auto-brightness OLED (LDR su A0, state machine sticky+fade)
├── dtc_descriptions.h        tabella DTC PROGMEM + decodeDTC() / getDTCDescription()
├── pid_descriptions.h        tabella PID SAE J1979 PROGMEM + getPIDShortName() / getPIDFullName()
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
