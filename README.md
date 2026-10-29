# OBD2 Car Dashboard OLED

Dashboard motore in tempo reale per **Audi A5 F5 (B9.5) 40 TFSI 150 kW mild hybrid S-tronic**, basata su ESP8266 e display OLED SH1106 1.3" (128×64). Il progetto offre **due sketch alternativi** che condividono dashboard, modelli fisici e web server, e si differenziano solo per il transport verso l'ECU.

> ℹ️ **Stato**: firmware e anteprime sono allineati alla A5 F5 40 TFSI; l'elenco dei PID realmente leggibili arriverà dallo scan eseguito sull'auto (§5.1, §5.5).

> 🖥️ **Anteprime interattive** nel browser, senza clonare la repo: [display OLED](https://raw.githack.com/alesimattia/OBD2-car_dashboard-OLED/main/ui_preview.html) (§3.1) e [dashboard web](https://raw.githack.com/alesimattia/OBD2-car_dashboard-OLED/main/monitor_preview.html) (§11.5).

---

## 📋 Indice

1. [Panoramica](#1-panoramica)
2. [Le due varianti a confronto](#2-le-due-varianti-a-confronto)
3. [Architettura](#3-architettura)
   - [3.1 Anteprima del display OLED](#31-anteprima-del-display-oled)
4. [Hardware e cablaggio](#4-hardware-e-cablaggio)
   - [4.1 Componenti comuni](#41-componenti-comuni)
   - [4.2 Variante CANbus_conn](#42-variante-canbus_conn)
   - [4.3 Variante WIFI_conn](#43-variante-wifi_conn)
5. [Veicolo target](#5-veicolo-target)
   - [5.1 Discovery delle centraline e dei PID](#51-discovery-delle-centraline-e-dei-pid)
   - [5.2 PID letti dal firmware e formule](#52-pid-letti-dal-firmware-e-formule)
   - [5.3 Parametri monitorabili in più](#53-parametri-monitorabili-in-più)
   - [5.4 Riferimento Mode OBD2 e UDS](#54-riferimento-mode-obd2-e-uds)
   - [5.5 Scan da PC con tools/pid_scan.ps1](#55-scan-da-pc-con-toolspid_scanps1)
6. [Software e build](#6-software-e-build)
7. [Configurazione](#7-configurazione)
8. [Lettura live multi-PID](#8-lettura-live-multi-pid)
9. [Modelli fisici e calcoli derivati](#9-modelli-fisici-e-calcoli-derivati)
10. [Pulsante fisico](#10-pulsante-fisico)
11. [Interfaccia web /dashboard](#11-interfaccia-web-dashboard)
12. [Auto-brightness OLED](#12-auto-brightness-oled)
13. [DTC, codici di errore](#13-dtc-codici-di-errore)
14. [Note operative e limiti](#14-note-operative-e-limiti)
15. [Licenza](#15-licenza)

---

## 1. Panoramica

Entrambi gli sketch:

- Al boot scoprono **tutte le centraline OBD** che rispondono (motore, cambio e altre) e i PID Mode 01 e le InfoType Mode 09 dichiarati da ciascuna, con report su Serial e nella pagina `/scan`; dal web si avvia una **scansione approfondita** che verifica quali PID rispondono davvero.
- Mostrano sull'OLED 4 metriche live (**BOOST**, **TEMP**, **COPPIA**, **BATT**) con aggiornamento parziale del framebuffer; boost e coppia vengono dalla centralina o, in sua assenza, dai modelli fisici (§8).
- Leggono i valori principali con **una richiesta multi-PID per ciclo** (fino a 6 PID), con ripiego automatico a un PID per ciclo.
- Espongono via SoftAP un **web dashboard** (`/dashboard`) con diagnostica completa e refresh JSON ogni 500 ms.
- Leggono e cancellano i **DTC di tutte le centraline** (Mode 03 / Mode 04), mostrandoli paginati sul display con la centralina di provenienza.
- Supportano **OTA** firmware via browser su `http://192.168.4.1`.
- Sono pilotabili da un **pulsante fisico** per ciclare le schermate e cancellare i DTC.
- Regolano automaticamente la **luminosità dell'OLED** in base alla luce ambientale (LDR su ADC, opzionale), con override manuale dal web dashboard; senza LDR il contrasto resta al 50%.

---

## 2. Le due varianti a confronto

| Caratteristica | [`CANbus_conn/`](CANbus_conn/) | [`WIFI_conn/`](WIFI_conn/) |
|---|---|---|
| **Transport ECU** | CAN nativo via MCP2515/TJA1050 (SPI) | TCP via adattatore ELM327 WiFi |
| **Hardware extra** | Modulo MCP2515 + TJA1050 (8 o 16 MHz) | Adattatore ELM327 WiFi commerciale |
| **Refresh OBD** | **~25-50 cicli/s** stimati, tutti i valori principali per ciclo | ~5-8 cicli/s stimati |
| **Latenza tipica per richiesta** | ~10-25 ms (multi-frame compreso) | ~120-200 ms |
| **Distinzione delle centraline** | ID CAN di ogni frame | header CAN attivi (`ATH1`); senza, una sola centralina |
| **Cablaggio** | SPI + I²C + OBD2 connector | Solo I²C + alimentazione |
| **Costo aggiuntivo** | ~5 € (MCP2515) | ~10-15 € (ELM327 WiFi) |
| **Affidabilità segnale** | Alta (CAN nativo) | Dipende da qualità ELM327 |
| **WiFi mode** | `WIFI_AP` (solo SoftAP per OTA/dashboard) | `WIFI_AP_STA` (STA verso ELM327 + AP per OTA/dashboard) |
| **Endpoint OTA** | `/update` | `/ota` |
| **Compatibile ESP-01** | ❌ No (mancano pin SPI) | ✅ Sì, senza LDR (§4.3) |

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
                   │  │  Lettura live multi-PID    │  │
                   │  │ (boost o MAP, carico, RPM, │  │
                   │  │  MAF, coppia + 1 lento)    │  │
                   │  └────┬──────────────────┬────┘  │
                   │       │ cache PID        │       │
                   │  ┌────▼─────┐  ┌─────────▼────┐  │
                   │  │ Boost:   │  │ Coppia:      │  │
                   │  │ 0x70 o   │  │ 0x62 o       │  │
                   │  │ modello  │  │ modello      │  │
                   │  └──────────┘  └──────────────┘  │
                   │                                  │
                   │  ┌─── HTTP server (port 80) ───┐ │
                   │  │ /dashboard /data /scan      │ │
                   │  │ /scan-data /probe /debug    │ │
                   │  │ /clear-dtc /brightness      │ │
                   │  │ /update (CAN) /ota (WiFi)   │ │
                   │  └──────────────────────────────┘ │
                   └──────────────────────────────────┘
                              ▲ SoftAP "OBD2_UPDATE"
                              ▼
                          smartphone
```

Nello sketch **WIFI_conn** il blocco MCP2515 è sostituito da un `WiFiClient` TCP verso `192.168.0.10:35000` (porta dell'ELM327); l'ESP8266 lavora in `WIFI_AP_STA` (STA verso l'ELM327 + AP per l'OTA/dashboard).

Il codice OBD è condiviso dalle due varianti in tre header della root: [`obd_proto.h`](obd_proto.h) contiene le primitive di protocollo senza dipendenze da Arduino (bitmap dei PID, riassemblaggio ISO-TP, righe ELM327 con header, decodifica di DTC, risposte negative, multi-PID, boost `0x70`, OBFCM), [`obd_scan.h`](obd_scan.h) la discovery, la scansione approfondita, il report e il JSON di `/scan-data`, [`obd_live.h`](obd_live.h) la lettura live, i DTC e l'istantanea diagnostica. Ogni sketch fornisce solo il trasporto, la funzione `obdTransact()`: su CAN nativo invia la richiesta e riassembla i frame inviando il flow control all'ID fisico della centralina, sull'ELM327 imposta l'header (`ATSH`) e legge le righe con gli header CAN attivi.

### 3.1 Anteprima del display OLED

[`ui_preview.html`](ui_preview.html) mostra nel browser, senza ESP, tutte le schermate del display (avvio, connessione, scansione, monitor, DTC, cancellazione, errori) con valori realistici. L'immagine ne riporta quattro; cliccandola si apre la versione live.

<p align="center">
  <a href="https://raw.githack.com/alesimattia/OBD2-car_dashboard-OLED/main/ui_preview.html">
    <img src="DOCS/ui_preview.png" width="700" alt="Schermate del display OLED: monitor, veleggio mild hybrid, DTC e cancellazione rifiutata">
  </a>
</p>

---

## 4. Hardware e cablaggio

### 4.1 Componenti comuni

| Componente | Note |
|---|---|
| ESP8266 **D1 Mini** (clone) | 4 MB flash, partizione `4M (no FS)` |
| **OLED SH1106 1.3" I²C** | 128×64 monocromo, indirizzo `0x3C` |
| Pulsante NA verso GND | Debounce software 50 ms |
| Fotoresistore GL5528 + 10 kΩ pull-down (opzionale, solo D1 Mini) | Voltage divider su `A0` per auto-brightness; senza, contrasto fisso al 50% (§12) |

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

### 4.3 Variante `WIFI_conn`

Componenti aggiuntivi:
- Adattatore **ELM327 WiFi** generico (rete `WiFi_OBDII` aperta, IP `192.168.0.10:35000`)

Lo sketch gira su **LOLIN D1 Mini** o su **ESP-01** e sceglie il profilo dalla board selezionata nell'IDE: "Generic ESP8266 Module" attiva il profilo ESP-01, ogni altra board quello del D1 Mini. L'OLED usa GPIO0/GPIO2 su entrambe, gli unici pin liberi dell'ESP-01 oltre a RX e TX.

Cablaggio su D1 Mini (display + pulsante + LDR, niente CAN):

| Segnale | D1 Mini | Periferica |
|---|---|---|
| OLED VCC | 3.3 V | VCC |
| OLED GND | GND | GND |
| OLED SDA | **GPIO0** (D3) | SDA |
| OLED SCL | **GPIO2** (D4) | SCL |
| Pulsante | **D5** (GPIO14) | altro lato → GND |
| LDR (lato luce) | 3.3 V | LDR |
| LDR (giunzione) | **A0** | LDR ↔ resistore 10 kΩ ↔ GND |

> ⚠️ Il pulsante è su **D5 (GPIO14)**: D3/D4 sono l'I²C dell'OLED, D0 non ha pull-up interno, D8 deve restare basso all'avvio e RX/TX sono del convertitore USB. D1, D2, D6 e D7 sarebbero equivalenti.

Cablaggio su ESP-01 (display + pulsante, niente LDR):

| Segnale | ESP-01 | Periferica |
|---|---|---|
| OLED VCC | 3.3 V | VCC |
| OLED GND | GND | GND |
| OLED SDA | **GPIO0** | SDA |
| OLED SCL | **GPIO2** | SCL |
| Pulsante | **GPIO3 (RX)** tramite 1 kΩ in serie | altro lato → GND |
| CH_PD (EN) | 3.3 V | abilitazione del chip |

> ⚠️ Sull'ESP-01 il pulsante usa RX, quindi la seriale lavora in sola trasmissione e i log restano su TX. La resistenza da 1 kΩ in serie protegge l'uscita TX dell'adattatore USB-seriale se il pulsante viene premuto mentre si carica il firmware; GPIO3 non decide la modalità di avvio, quindi premerlo all'accensione non ha effetti.

> L'ESP-01 non porta fuori l'ingresso analogico: il profilo ESP-01 disattiva l'LDR (`LDR_ENABLED` = 0) e il contrasto resta a `defaultBrightness`, regolabile dallo slider della dashboard (§12).

---

## 5. Veicolo target

| Parametro | Valore |
|---|---|
| Modello | Audi A5 F5 (B9.5, restyling), produzione da 07/2020 |
| Motore | **DMSA**, 2.0 TFSI 4 cilindri in linea benzina della famiglia EA888, montato longitudinalmente |
| Cilindrata / alesaggio × corsa / compressione | 1984 cm³ / 82.5 × 92.8 mm / **12.2:1** |
| Distribuzione | bilancieri a rullo, variazione continua della fasatura su aspirazione e scarico, punterie idrauliche, 4 valvole per cilindro |
| Alimentazione | iniezione diretta, regolazione lambda, controllo detonazione, turbocompressore con intercooler; benzina 95 ottani EN 228 |
| Potenza / Coppia | **150 kW (204 CV)** a 4475-6000 rpm / **320 Nm** a 1450-4475 rpm |
| Cambio | **S-tronic 7 rapporti** DL382, codice `0CK`: doppia frizione in bagno d'olio a comando idraulico, rapporti nella tabella più sotto |
| Trazione | anteriore |
| Ibridazione | **Mild hybrid 12 V**: belt alternator starter (BAS) collegato all'albero motore, recupero in rilascio fino a 5 kW, batteria agli ioni di litio 12 V 10 Ah separata nella zona posteriore |
| Impianto elettrico | 12 V, potenza elettrica massima 3.1 kW |
| Post-trattamento | catalizzatore, sonda lambda, filtro antiparticolato benzina (GPF) |
| Emissioni | Euro 6d o Euro 6e secondo l'anno di produzione (la scheda Audi 06/2024 riporta Euro 6e) |
| Prestazioni | 210 km/h autolimitata, 0-100 km/h in 7.2 s |
| Pneumatici di serie | 225/50 R17 su cerchio 7.5J × 17 |
| Capacità | olio motore 5.2 L con filtro, circuito di raffreddamento 8 L, serbatoio 54 L |
| Massa | 1515 kg a vuoto senza conducente, 2085 kg complessiva |
| Manutenzione | tagliando ogni 30000 km o 2 anni |
| Bus diagnostica | **CAN 11 bit a 500 kbps** (ISO 15765-4) |

> ℹ️ I dati vengono dalla scheda tecnica Audi (eTD) della A5 Sportback 40 TFSI S-tronic 150 kW MHEV a trazione anteriore. Su Coupè e Cabriolet cambiano masse e prestazioni: per la Coupè ADAC riporta 1560 kg e 0-100 km/h in 7.1 s. Le costanti che il firmware usa per questo motore sono in [`EngineConstants.h`](EngineConstants.h).

> ⚠️ VAG espone solo i PID Mode 01 richiesti per le emissioni: su un benzina Euro 6d compaiono tipicamente correzioni carburante, sonda lambda a valle del catalizzatore e temperatura catalizzatore. L'elenco effettivo va rilevato con lo scan al boot (§5.1).

#### Rapporti del cambio S-tronic `0CK`

Coppia finale **4.234**, retromarcia 2.750. Le velocità sono calcolate con la circonferenza di rotolamento di 2.00 m del pneumatico di serie (`TYRE_ROLLING_CIRCUMFERENCE_M`), quindi sono indicative: con cerchi diversi o pneumatici usurati variano di qualche punto percentuale.

| Marcia | Rapporto cambio | Rapporto totale | km/h ogni 1000 rpm | rpm a 100 km/h | km/h a 6000 rpm |
|---|---|---|---|---|---|
| 1ª | 3.188 | 13.50 | 8.9 | fuori giri | 53 |
| 2ª | 2.190 | 9.27 | 12.9 | fuori giri | 78 |
| 3ª | 1.517 | 6.42 | 18.7 | 5350 | 112 |
| 4ª | 1.057 | 4.48 | 26.8 | 3730 | 161 |
| 5ª | 0.738 | 3.12 | 38.4 | 2600 | 230 |
| 6ª | 0.557 | 2.36 | 50.9 | 1970 | 305 |
| 7ª | 0.433 | 1.83 | 65.5 | 1530 | 393 |

La gamma complessiva dei rapporti è 7.36. La velocità massima di 210 km/h è autolimitata e in 7ª corrisponde a circa 3200 rpm, ben sotto il regime di potenza massima. Il campo `gearRatio` della dashboard calcola il rapporto del solo cambio, al netto della coppia finale: a marcia innestata e frizione chiusa coincide con la colonna "Rapporto cambio", quindi permette di riconoscere la marcia senza leggere la centralina del cambio; durante i cambi marcia o con frizione aperta non corrisponde a nessun rapporto.

#### Logiche mild hybrid viste dall'OBD

Il belt alternator starter sostituisce alternatore e motorino nelle ripartenze, recupera energia in rilascio e la accumula nella batteria agli ioni di litio. Sono due le logiche che si vedono nei dati OBD:

- **Veleggio a motore spento** fra 55 e 160 km/h quando si rilascia l'acceleratore: il cambio apre le frizioni, il motore si spegne e l'impianto resta alimentato dalla batteria agli ioni di litio.
- **Start-stop esteso**: il motore si spegne già sotto i 22 km/h in fase di arresto.

In entrambi i casi la centralina motore resta raggiungibile sul CAN e riporta RPM, carico e MAF a zero con il quadro acceso, mentre il MAP si porta alla pressione barometrica: boost, coppia e potenza valgono zero e i calcoli che dividono per RPM o potenza mostrano `N/D`, senza che si tratti di un errore di lettura. Il BAS e la batteria agli ioni di litio non hanno PID standard: il sistema si osserva solo indirettamente da queste condizioni.

#### Convenzione ECU ID OBD2

Sul bus CAN 11-bit le richieste OBD2 vengono inviate al **broadcast** `0x7DF`; ogni controllore presente risponde con il proprio **ID di risposta** nell'intervallo `0x7E8`-`0x7EF`. Sull'A5 F5 i moduli rilevanti sono:

| Response ID | Modulo | Ruolo |
|---|---|---|
| `0x7E8` | **ECU motore** (J623, 2.0 TFSI) | espone i PID standard Mode 01 di motore, emissioni, turbo, sonde lambda e catalizzatore |
| `0x7E9` | **ECU cambio** (J217, meccatronica S-tronic DL382) | sulle B9 catturate risponde con pochi PID: `01 04 05 0C 0D 41 42 49` |
| `0x7EC` | centralina di funzione non identificata (richiesta su `0x7E4`) | sulle B9 catturate risponde con `01 04 05 0C 0D 41 42 46 49`; presente anche su vetture senza mild hybrid |
| altri `0x7EA`-`0x7EF` | nessuna sulle B9 termiche catturate | `0x7ED`-`0x7EF` compaiono solo sulle ibride plug-in (batteria ad alta tensione, motori elettrici) |

Nelle tabelle che seguono ogni PID è etichettato con l'ECU di provenienza. Le centraline e i PID di `0x7E9` e `0x7EC` vengono da catture reali di Audi A4/A5/Q5 B9 2019-2024 raccolte dal progetto OBDb (stessa piattaforma, nessuna con certezza motore DMSA): sulle stesse vetture la centralina motore espone fra l'altro `0x5C` olio, `0x61`/`0x62`/`0x63` coppia (`0x63` = 320 Nm sulle 40 TFSI), `0x8E` attrito, `0x70` boost (sensore A assoluto), `0x2F`, `0x5E`, `0x9D`, `0x9E` e `0xA6`, mentre `0x5B`, `0x9A`, `0xA4` e `0x7A`-`0x7C` non compaiono mai. Lo scan al boot (§5.1) dice quali centraline e PID sono attivi sulla vettura specifica.

### 5.1 Discovery delle centraline e dei PID

La discovery è identica nelle due varianti ([`obd_scan.h`](obd_scan.h)) e parte al boot, prima del monitor (per rilevare centraline e PID da un PC, senza ESP, vedi §5.5):

1. **Centraline**: richiesta `0100` in broadcast su `0x7DF` ripetuta due volte, con unione delle risposte; fino a 8 centraline (`0x7E8`-`0x7EF`), finestra di 150 ms. Ogni risposta fornisce già il primo range della bitmap Mode 01.
2. **Indirizzamento fisico**: per ogni centralina una richiesta `0100` all'ID fisico (risposta − 8, per esempio `0x7E0` per il motore). Le letture successive verso una sola centralina usano l'indirizzo fisico quando risponde, altrimenti il broadcast con attesa della sola risposta attesa.
3. **Catena Mode 01** (`0120` ... `01E0`) e **catena Mode 09** (`0900`, `0920`): un broadcast per range raccoglie tutte le centraline insieme; chi non risponde riceve un secondo broadcast e poi la richiesta fisica. Una centralina muta interrompe solo la propria catena.
4. **Nome della centralina** (InfoType `0x0A`, es. `ECM-EngineControl`) e **contatori OBFCM** (InfoType `0x17`, vedi sotto) quando dichiarati.
5. **Capacità del motore**: coppia di riferimento `0x63` e prova di una richiesta con due PID, che abilita la lettura multi-PID (§8).

Se nessuna centralina risponde lo scan si ripete dopo 10 s; se manca il motore `0x7E8`, da cui dipende la lettura live, viene stampato il report delle centraline trovate e lo scan si ripete. Sulla variante WiFi l'ELM327 lavora con gli header CAN attivi (`ATH1`), quindi ogni riga di risposta porta l'ID della centralina; un clone che ignora `ATH1` viene riconosciuto e la discovery si limita a una centralina, mentre un clone che rifiuta `ATSH` lavora solo in broadcast. Entrambi i casi sono dichiarati nel report.

**Report dello scan**: su Serial (USB o tab "Serial monitor" della dashboard) compare `=== REPORT SCAN OBD2 ===` con, per ogni centralina, ID, nome, risposta all'indirizzo fisico, PID Mode 01 e InfoType Mode 09 con codice e descrizione, poi OBFCM con i byte grezzi e le capacità del motore. La tab "PID supportati" e la pagina `/scan` mostrano gli stessi elenchi per descrizione.

**Scansione approfondita**: dal pulsante "Avvia scansione approfondita" della pagina `/scan` (o della tab) il firmware interroga, un passo per ogni giro del loop, `0100` fisico a `0x7E0`-`0x7E7`, ogni PID Mode 01 da `0x01` a `0xFF` e ogni InfoType Mode 09 da `0x01` a `0x1F` in broadcast. Su Serial stampa una riga per ogni risposta con byte grezzi, lunghezza diversa da quella SAE e PID non dichiarati, poi il riepilogo per centralina; la pagina mostra i PID dichiarati ma muti e quelli che rispondono senza essere dichiarati. Dura circa 30 s su CAN e 60-80 s su WiFi; nel frattempo il monitor è sospeso e la dashboard non interroga il bus. Il VIN non viene riportato. Il report di questa scansione eseguita sull'auto è il dump da conservare in [`DOCS/`](DOCS/).

**Consumo reale OBFCM** (Mode 09, InfoType `0x17`): le vetture omologate Euro 6d-ISC-FCM ed Euro 6e devono registrare a bordo il consumo reale (On-Board Fuel Consumption Monitoring, regolamento UE 2018/1832) e renderlo leggibile dalla presa OBD secondo SAE J1979-DA, con quattro contatori a 32 bit di distanza (0.1 km) e carburante (0.01 L), recenti e di vita; il valore `FFFFFFFF` significa non disponibile. Il firmware legge `0917` allo scan dalla centralina che lo dichiara (motore per primo) e mostra nella pagina `/scan` distanza e litri di vita e il consumo medio reale dall'immatricolazione, misurato dall'ECU e non stimato. VCDS legge "Mode 9, Type 17" su varie vetture del gruppo (Golf 8, Q5, A6), ma il layout dei byte viene da un'implementazione open source verificata su una sola vettura non VAG, perchè il testo SAE J1979-DA è a pagamento: per questo il report Serial riporta sempre i byte grezzi e la pagina segnala un consumo medio fuori da 3-30 L/100 km come layout da verificare.

### 5.2 PID letti dal firmware e formule

Il firmware interroga solo i PID che il motore ha dichiarato. Le formule sono quelle della tabella SAE J1979 pubblica e non dipendono dal veicolo.

| PID | Descrizione | Formula | Unità | Uso |
|---|---|---|---|---|
| `0x01` | Stato MIL e numero DTC confermati | bit 7 di A = MIL, A & 0x7F = numero | — | DTC di tutte le centraline (§13) |
| `0x04` | Carico motore calcolato | `A × 100 / 255` | % | lettura live, modelli |
| `0x05` | Temperatura liquido | `A − 40` | °C | casella TEMP |
| `0x0B` | Pressione assoluta collettore (MAP) | `A` | kPa | boost stimato, efficienza volumetrica |
| `0x0C` | Regime motore | `(256 × A + B) / 4` | rpm | lettura live |
| `0x0D` | Velocità | `A` | km/h | diagnostica |
| `0x0F` | Temperatura aria aspirata | `A − 40` | °C | modelli, diagnostica |
| `0x10` | Portata massa aria (MAF) | `(256 × A + B) / 100` | g/s | modelli, consumo stimato |
| `0x1F` / `0x21` / `0x30` | Tempo motore, km con MIL, riscaldamenti dal clear | `256 × A + B`, `A` | s, km, conteggio | diagnostica |
| `0x23` | Pressione rail (iniezione diretta) | `10 × (256 × A + B)` | kPa | diagnostica |
| `0x24` / `0x34` | Sonda lambda 1 wide range | `λ = (256 × A + B) / 32768`; `0x24`: `V = (256 × C + D) / 8192`; `0x34`: `I = (256 × C + D) / 256 − 128` mA | λ | diagnostica, AFR |
| `0x33` | Pressione barometrica | `A` | kPa | riferimento del boost |
| `0x42` | Tensione modulo | `(256 × A + B) / 1000` | V | diagnostica |
| `0x44` | Lambda comandata | `(256 × A + B) / 32768` | λ | diagnostica; confrontata con `0x24` mostra gli arricchimenti di protezione a pieno carico |
| `0x46` / `0x49` / `0x4A` / `0x4C` | Temperatura esterna, pedale D/E, farfalla comandata | `A − 40`, `A × 100 / 255` | °C, % | diagnostica |
| `0x5B` | Carica residua batteria ibrida | `A × 100 / 255` | % | casella BATT |
| `0x5C` | Temperatura olio motore | `A − 40` | °C | diagnostica; sul TFSI sovralimentato è il riferimento per decidere quando sfruttare tutta la coppia |
| `0x5E` | Portata carburante motore | `(256 × A + B) / 20` | L/h | consumo misurato, al posto della stima da MAF e lambda |
| `0x62` / `0x8E` | Coppia effettiva / di attrito | `A − 125` | % di `0x63` | casella COPPIA: `0x62` è la coppia indicata, comprensiva dell'attrito `0x8E` che viene sottratto |
| `0x63` | Coppia di riferimento | `256 × A + B` | Nm | letta una volta allo scan: è il "100%" della centralina |
| `0x70` | Boost pressure control, sensore A | presente se il bit 1 di A è 1; `(256 × D + E) / 32` | kPa assoluti | casella BOOST; il valore comandato contenuto nello stesso PID non viene usato |

Caratteristiche del 2.0 TFSI di cui tengono conto i calcoli:

- `0x2C` / `0x2D` (EGR) non vengono letti: l'EA888 realizza il ricircolo dei gas internamente con la fasatura variabile.
- `0x23` (rail) è la pressione dell'iniezione diretta benzina: 70-100 bar al minimo e fino a 350 bar.
- `0x24` (lambda) resta intorno a λ = 1 nel funzionamento stechiometrico, con arricchimenti sotto 1 a pieno carico. Sulle B9 catturate la sonda è esposta come `0x34`, che il firmware usa in alternativa.
- `0x4C` (farfalla) è l'organo principale di regolazione dell'aria, non una farfalla di supporto all'EGR.
- `0x5B` è definito da SAE per le batterie di trazione ad alta tensione e sulle B9 termiche catturate non compare mai, quindi la casella BATT mostrerà quasi certamente `N/D`.

### 5.3 Parametri monitorabili in più

Parametri che il firmware non legge e non calcola ancora. §5.3.1 elenca i PID standard SAE J1979 che un 2.0 TFSI Euro 6d può esporre: sono *potenziali*, e quali siano davvero supportati lo dicono lo scan al boot e la scansione approfondita (§5.1). §5.3.2 elenca le grandezze ricavabili dai PID già letti (§5.2) senza nuove richieste OBD; quelle già calcolate sono in §9.

#### 5.3.1 PID diretti potenziali

##### Motore, termici e lubrificazione

| ECU | PID | Parametro | Note |
|---|---|---|---|
| `0x7E8` motore | `0x3C` | Temperatura catalizzatore banco 1 sensore 1 | spesso calcolata dall'ECU; mostra il light-off dopo l'avviamento e il raffreddamento durante il veleggio a motore spento |
| `0x7E8` motore | `0x3E` | Temperatura catalizzatore banco 1 sensore 2 | a valle, utile insieme al GPF |
| `0x7E8` motore | `0x77` | Temperatura aria di sovralimentazione (CACT) | aria all'uscita dell'intercooler |
| `0x7E8` motore | `0x78` | Temperatura gas di scarico (EGT) banco 1 | se esposta |

##### Iniezione, accensione e miscela

| ECU | PID | Parametro | Note |
|---|---|---|---|
| `0x7E8` motore | `0x06` | Correzione carburante a breve termine banco 1 (STFT) | regolazione in anello chiuso istante per istante |
| `0x7E8` motore | `0x07` | Correzione carburante a lungo termine banco 1 (LTFT) | adattamento memorizzato: una deriva persistente indica infiltrazioni d'aria o iniettori fuori norma |
| `0x7E8` motore | `0x0E` | Anticipo di accensione | i ritardi sotto carico segnalano detonazione o carburante di qualità scarsa |
| `0x7E8` motore | `0x15` | Sonda banco 1 sensore 2, a valle del catalizzatore | tensione della sonda post-catalizzatore, base del controllo di efficienza |
| `0x7E8` motore | `0x59` | Pressione rail assoluta | alternativa a `0x23` |
| `0x7E8` motore | `0x2F` | Livello serbatoio % | presente sulle B9 catturate |
| `0x7E8` motore | `0x51` | Tipo di carburante | informazione statica |

##### Coppia e aspirazione

| ECU | PID | Parametro | Note |
|---|---|---|---|
| `0x7E8` motore | `0x61` | Coppia richiesta dal pilota % | confrontata con `0x62` (calcoli più sotto) |
| `0x7E8` motore | `0x11` | Posizione farfalla | sul benzina è l'organo principale di regolazione dell'aria |
| `0x7E8` motore | `0x45` | Posizione relativa farfalla | — |
| `0x7E8` motore | `0x47` / `0x48` | Posizione assoluta farfalla B/C | — |

##### Turbo e scarico

| ECU | PID | Parametro | Note |
|---|---|---|---|
| `0x7E8` motore | `0x74` | Regime turbocompressore | raro |
| `0x7E8` motore | `0x7A` / `0x7B` / `0x7C` | Pressione differenziale e temperatura del filtro antiparticolato | SAE li definisce per il DPF diesel e non compaiono sulle B9 catturate; da verificare se l'ECU li usa per il GPF |

##### Cambio S-tronic (esposti da `0x7E9` se risponde)

| ECU | PID | Parametro | Note |
|---|---|---|---|
| `0x7E9` cambio | `0xA4` | Marcia attuale | sul DL382 è una marcia reale da 1 a 7 e sostituisce la stima del rapporto; non compare sulle B9 catturate |

##### Contatori e diagnostica

| ECU | PID | Parametro | Note |
|---|---|---|---|
| `0x7E8` motore | `0x31` | Distanza percorsa dal clear DTC | odometro parziale dal reset, distinto da `0x21` (km con MIL) |
| `0x7E8` motore | `0x41` | Monitor status drive cycle | bitmap dei monitor di emissioni completati nel ciclo corrente, **utile in pre-revisione** per sapere se l'auto è "Ready" |
| `0x7E8` motore | `0x4D` | Tempo motore con MIL accesa | in ore; il firmware legge solo i km con MIL (`0x21`) |
| `0x7E8` motore | `0x4E` | Tempo dal clear DTC | — |
| `0x7E8` motore | `0x43` | Carico assoluto | carico % normalizzato sul massimo teorico, diverso da `0x04` |
| `0x7E8` motore | `0xA6` | Contachilometri | odometro totale, presente sulle B9 catturate |
| `0x7E8` motore | `0x13` | Sonde lambda presenti (2 banchi) | informazione statica (configurazione del veicolo) |
| `0x7E8` motore | `0x1C` | Standard OBD del veicolo | informazione statica (EOBD, OBD-II, ecc.) |
| `0x7E8` motore | `0x4F` | Massimi calibrazione (lambda, V/I sonda, MAP) | informazione statica, utile per validare le letture nel range |

##### Valori calcolabili con questi PID

| Calcolo | Formula | Beneficio |
|---|---|---|
| Δ coppia richiesta vs effettiva | `0x61 − 0x62` | mostra quando l'auto non riesce a soddisfare la domanda (limp mode, boost insufficiente, cut-off) |
| Correzione carburante totale | `0x06 + 0x07` | valori stabilmente oltre ±10% indicano infiltrazioni d'aria, pressione carburante o iniettori fuori norma |
| Attività della sonda a valle | ampiezza dell'oscillazione di `0x15` confrontata con `0x24` | una sonda a valle che oscilla come quella a monte indica un catalizzatore poco efficiente |
| Drift temperatura GPF | `d(0x7C)/dt` | evidenzia le fasi di rigenerazione |
| Contropressione GPF | `0x7A` nel tempo a parità di portata | indicatore di intasamento del filtro |
| Rapporto totale per marcia (`0x7E9` + `0x7E8`) | `RPM / velocità` registrato per ciascun valore di `0xA4` | un rapporto instabile a marcia fissa indica slittamento della frizione |

##### Considerazioni pratiche

- I PID `0x60-0x9F` sono estensioni delle revisioni più recenti di SAE J1979, **non garantite**.
- Alcuni PID (es. `0x70`, `0x7A`) sono "container": i byte di payload vanno decodificati in più sottocampi secondo il data layout SAE J1979.
- **Cambio S-tronic**: i dati ricchi (temperatura olio di frizioni e meccatronica, usura frizioni, adattamenti) vivono solo su Mode 22 UDS con label files VAG.

#### 5.3.2 Calcoli dai PID già letti

Grandezze che il firmware può ricavare dai PID di §5.2, che sono già in cache (lettura live o istantanea diagnostica): manca solo il calcolo. Tutti i PID sono dell'ECU motore `0x7E8`.

##### Stile d'uso e percorrenze

| Calcolo | Formula | Beneficio |
|---|---|---|
| Km medi per warm-up | `0x31 / 0x30` | distingue uso urbano da lunga percorrenza; richiede anche `0x31` (§5.3.1) |
| % distanza con MIL on | `0x21 / 0x31 × 100` | frazione del totale percorso dal reset con guasto attivo; richiede anche `0x31` |
| Velocità media sessione | `(odometro accumulato) / 0x1F` | media dall'avviamento (richiede integrazione di `0x0D` nel tempo) |
| Carico medio sessione | media mobile di `0x04` | indicatore di stile guida (sportivo vs cruise) |
| Peak hold sessione | max(`boostBar`), max(`RPM`), max(coppia), max(`0x05`) | "watermark" della sessione corrente |

##### Diagnostica passiva

| Calcolo | Formula | Beneficio |
|---|---|---|
| Rampa riscaldamento liquido | `d(0x05)/dt` nei primi 5 min dopo avviamento | individua termostato bloccato aperto (rampa lenta < 2 °C/min) |
| Arricchimento di protezione | `0x44 < 1` con carico elevato | quantifica quanto l'ECU arricchisce per proteggere turbina e catalizzatore |
| Errore regolazione boost | pressione comandata − pressione reale, entrambe contenute in `0x70` | individua wastegate che non regola o perdite nel circuito di sovralimentazione |
| Veleggio a motore spento (MHEV) | `0x0C = 0` con `0x0D` fra 55 e 160 km/h | conta durata e chilometri percorsi a motore spento, e distingue il veleggio da uno spegnimento reale |
| Intervento start-stop | `0x0C = 0` con `0x0D` sotto 22 km/h o fermo | numero e durata delle soste a motore spento nella sessione |

### 5.4 Riferimento Mode OBD2 e UDS

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

Convenzione **Monitored**:
- ✅ **letto live**: interrogato dalla lettura live, dall'istantanea diagnostica o allo scan, se dichiarato
- 🔍 **scoperto**: rilevato dallo scan al boot o dalla scansione approfondita, ma non letto
- ➖ **non interrogato**: il firmware non lo richiede

Convenzione **Standard**:
- ✅ **standard**: definito in SAE J1979 / ISO 14229, decodifica pubblica
- ⚠️ **reverse-engineering**: definito dal costruttore, serve label file VCDS o forum VAG

#### 5.4.1 Mode OBD2 standard (SAE J1979 / ISO 15031-5)

| Mode | ECU | PID | Nome | Standard | Monitored |
|---|---|---|---|---|---|
| `0x01` | `0x7E8` motore | PID di §5.2 | Dati live del motore | ✅ SAE J1979 | ✅ letto live, allo scan o nell'istantanea diagnostica |
| `0x01` | `0x7E8` motore | `0x00` / `0x20` / `0x40` ... | Bitmask PID supportati | ✅ SAE J1979 | 🔍 scoperto |
| `0x01` | `0x7E8` motore | `0x13`, `0x1C`, `0x31`, `0x41`, `0x4F` | PID statici e contatori (§5.3.1) | ✅ SAE J1979 | 🔍 scoperto |
| `0x01` | `0x7E8` motore | altri PID di §5.3.1 | PID potenziali | ✅ SAE J1979 | ➖ non interrogato |
| `0x01` | tutte | `0x01` | Stato MIL + numero DTC | ✅ SAE J1979 | ✅ letto ogni 30 s da tutte le centraline (`obdLiveCheckDtcs`) |
| `0x01` | `0x7E9` cambio, `0x7EC` | qualsiasi | PID Mode 01 delle altre centraline | ✅ SAE J1979 | 🔍 scoperto al boot e nella scansione approfondita, mai letti live |
| `0x02` | `0x7E8` motore | `0x02 [DTC]` | Freeze frame: snapshot al momento del DTC | ✅ SAE J1979 | ➖ non interrogato |
| `0x03` | tutte | — | Lettura DTC confermati | ✅ SAE J1979 | ✅ letto da ogni centralina con DTC, anche a MIL spenta (`obdLiveCheckDtcs`, ogni 30 s) |
| `0x04` | tutte | — | Cancellazione DTC + reset readiness | ✅ SAE J1979 | ✅ usato in broadcast con esito per centralina (`obdLiveClearDtcs`, on-demand) |
| `0x05` | n/a | n/a CAN | O2 sensor monitoring (legacy non-CAN) | ✅ SAE J1979 | ➖ non applicabile (A5 è CAN) |
| `0x06` | `0x7E8` motore | TID `0x01..0xFF` | Test results monitoring (catalizzatore, sonde lambda, GPF) | ⚠️ TID definiti dal costruttore | ➖ non interrogato (TID opachi senza label) |
| `0x07` | `0x7E8` motore | — | Pending DTC (in osservazione) | ✅ SAE J1979 | ➖ non interrogato |
| `0x08` | `0x7E8` motore | TID | Control on-board (forza attuatori) | ⚠️ TID definiti dal costruttore | ➖ non interrogato (rischioso) |
| `0x09` | tutte | `0x00` / `0x20` | Bitmap InfoType supportate | ✅ SAE J1979 | 🔍 scoperto al boot |
| `0x09` | tutte | `0x02` | VIN del veicolo | ✅ SAE J1979 | 🔍 solo scansione approfondita, valore non riportato |
| `0x09` | tutte | `0x04` / `0x06` / `0x08` | Calibration ID, CVN, IPT benzina | ✅ SAE J1979 | 🔍 solo scansione approfondita (byte grezzi su Serial) |
| `0x09` | tutte | `0x0A` | ECU Name | ✅ SAE J1979 | ✅ letto allo scan, nome della centralina in report e pagina `/scan` |
| `0x09` | motore, poi altre | `0x17` | OBFCM: distanza e carburante recenti e di vita | ✅ SAE J1979-DA (layout da verificare) | ✅ letto allo scan (§5.1) |
| `0x0A` | `0x7E8` motore | — | Permanent DTC (non cancellabili) | ✅ SAE J1979 | ➖ non interrogato |

#### 5.4.2 Mode UDS (ISO 14229-1 / ISO 15765-3)

I servizi diagnostici "estesi" usati dai costruttori (VAG VCDS, BMW INPA, Mercedes XENTRY, ecc.). Richiedono comunicazione **indirizzata** verso ID specifici per ECU. Non c'è "PID" ma **DID** (16 bit) o **Sub-funzione** (8 bit) a seconda del Mode.

> ℹ️ Per moduli diversi da motore/cambio (ABS `0x740/0x748`, BCM `0x731/0x73B`, cruscotto `0x714/0x77E`, ecc.) lo stesso schema si replica cambiando solo la coppia di ID. La tabella usa motore (`0x7E0/0x7E8`) come riferimento.

| Mode | ECU (req → resp) | DID / Sub | Nome | Standard | Monitored |
|---|---|---|---|---|---|
| `0x10` | `0x7E0` → `0x7E8` motore | sub `0x01` | Default Diagnostic Session | ✅ ISO 14229 | ➖ non interrogato |
| `0x10` | `0x7E0` → `0x7E8` motore | sub `0x02` | Programming Session (per flash) | ✅ ISO 14229 | ➖ non interrogato |
| `0x10` | `0x7E0` → `0x7E8` motore | sub `0x03` | Extended Diagnostic Session | ✅ ISO 14229 | ➖ non interrogato |
| `0x10` | `0x7E1` → `0x7E9` cambio | sub `0x03` | Extended Session sul cambio S-tronic | ✅ ISO 14229 | ➖ non interrogato |
| `0x11` | `0x7E0` → `0x7E8` motore | sub `0x01` | ECU Reset hard | ✅ ISO 14229 | ➖ non interrogato |
| `0x11` | `0x7E0` → `0x7E8` motore | sub `0x03` | ECU Reset soft | ✅ ISO 14229 | ➖ non interrogato |
| `0x14` | `0x7E0` → `0x7E8` motore | DTC mask | Clear Diagnostic Information (selettivo) | ✅ ISO 14229 | ➖ non interrogato (usiamo Mode 04 OBD2) |
| `0x19` | `0x7E0` → `0x7E8` motore | sub `0x02` | Read DTC by status mask | ✅ ISO 14229 | ➖ non interrogato (usiamo Mode 03 OBD2) |
| `0x19` | `0x7E0` → `0x7E8` motore | sub `0x06` | Read DTC extended data record | ✅ ISO 14229 | ➖ non interrogato |
| `0x22` | `0x7E0` → `0x7E8` motore | DID `0xF190` | VIN via UDS | ✅ ISO 14229 (DID standard) | ➖ non interrogato |
| `0x22` | `0x7E0` → `0x7E8` motore | DID `0xF18C` | ECU Serial Number | ✅ ISO 14229 (DID standard) | ➖ non interrogato |
| `0x22` | `0x7E0` → `0x7E8` motore | DID `0xF187` | ECU Part Number | ✅ ISO 14229 (DID standard) | ➖ non interrogato |
| `0x22` | `0x7E0` → `0x7E8` motore | DID arbitrario | Read Data By Identifier (caso generico) | ⚠️ DID applicativi proprietari | ➖ non interrogato |
| `0x23` | `0x7E0` → `0x7E8` motore | indirizzo + lunghezza | Read Memory By Address (richiede `0x27`) | ✅ ISO 14229 | ➖ non interrogato |
| `0x24` | `0x7E0` → `0x7E8` motore | DID | Read Scaling Data (metadati DID) | ✅ ISO 14229 | ➖ non interrogato |
| `0x27` | `0x7E0` → `0x7E8` motore | sub `0x01` (req seed) | Security Access: request seed | ⚠️ algoritmo seed/key proprietario | ➖ non interrogato |
| `0x27` | `0x7E0` → `0x7E8` motore | sub `0x02` (send key) | Security Access: send key | ⚠️ algoritmo seed/key proprietario | ➖ non interrogato |
| `0x28` | `0x7E0` → `0x7E8` motore | sub `0x00`-`0x03` | Communication Control (silenzia rete) | ✅ ISO 14229 | ➖ non interrogato |
| `0x29` | `0x7E0` → `0x7E8` motore | sub vari | Authentication (UDS 2020+) | ✅ ISO 14229 | ➖ non interrogato (supporto sulla A5 F5 da verificare) |
| `0x2A` | `0x7E0` → `0x7E8` motore | DID + freq | Read Data By Periodic Identifier | ✅ ISO 14229 | ➖ non interrogato |
| `0x2C` | `0x7E0` → `0x7E8` motore | DID dinamico | Dynamically Define DID | ✅ ISO 14229 | ➖ non interrogato |
| `0x2E` | `0x7E0` → `0x7E8` motore | DID + payload | Write Data By Identifier (codifica) | ⚠️ DID applicativi proprietari | ➖ non interrogato |
| `0x2F` | `0x7E0` → `0x7E8` motore | DID + control | Input/Output Control (test attuatore) | ⚠️ DID applicativi proprietari | ➖ non interrogato |
| `0x31` | `0x7E0` → `0x7E8` motore | sub `0x01` + RID | Routine Control: rigenerazione GPF, reset adattamenti | ⚠️ RID proprietari | ➖ non interrogato |
| `0x34` | `0x7E0` → `0x7E8` motore | indirizzo + lunghezza | Request Download (rimappatura) | ✅ ISO 14229 | ➖ non interrogato |
| `0x35` | `0x7E0` → `0x7E8` motore | indirizzo + lunghezza | Request Upload (backup ECU) | ✅ ISO 14229 | ➖ non interrogato |
| `0x36` | `0x7E0` → `0x7E8` motore | counter + dati | Transfer Data (durante 0x34/0x35) | ✅ ISO 14229 | ➖ non interrogato |
| `0x37` | `0x7E0` → `0x7E8` motore | — | Request Transfer Exit | ✅ ISO 14229 | ➖ non interrogato |
| `0x38` | `0x7E0` → `0x7E8` motore | sub + filename | Request File Transfer (UDS 2013+) | ✅ ISO 14229 | ➖ non interrogato |
| `0x3D` | `0x7E0` → `0x7E8` motore | indirizzo + dati | Write Memory By Address (richiede `0x27`) | ✅ ISO 14229 | ➖ non interrogato |
| `0x3E` | `0x7E0` → `0x7E8` motore | sub `0x00` | Tester Present (keep-alive sessione) | ✅ ISO 14229 | ➖ non interrogato |
| `0x83` | `0x7E0` → `0x7E8` motore | sub | Access Timing Parameter | ✅ ISO 14229 | ➖ non interrogato |
| `0x84` | `0x7E0` → `0x7E8` motore | sub | Secured Data Transmission | ✅ ISO 14229 | ➖ non interrogato (supporto sulla A5 F5 da verificare) |
| `0x85` | `0x7E0` → `0x7E8` motore | sub `0x01`/`0x02` | Control DTC Setting (on/off registrazione) | ✅ ISO 14229 | ➖ non interrogato |
| `0x86` | `0x7E0` → `0x7E8` motore | sub + DID | Response On Event (notifiche async) | ✅ ISO 14229 | ➖ non interrogato |
| `0x87` | `0x7E0` → `0x7E8` motore | sub | Link Control (cambio velocità bus) | ✅ ISO 14229 | ➖ non interrogato |

> ℹ️ Risposta a un Mode UDS = `Mode + 0x40` (es. `0x22` → `0x62`). Errore = frame `0x7F <Mode> <NRC>` con NRC notevoli: `0x11` Service Not Supported, `0x22` Conditions Not Correct, `0x31` Request Out Of Range, `0x33` Security Access Denied, `0x78` Response Pending (l'ECU sta lavorando, da non considerare timeout).

> ℹ️ **DID UDS sull'A5**: `0xF190` (VIN), `0xF18C` (Serial), `0xF187` (Part Number) sono **standard ISO 14229** e affidabili. I DID applicativi VAG, per esempio carico di particolato del GPF o temperatura olio del cambio S-tronic, sono **non standard**: si ricavano da reverse engineering pubblicato sui forum VAG e vanno verificati per la specifica ECU. Senza i label files Ross-Tech la lista è incompleta.

### 5.5 Scan da PC con `tools/pid_scan.ps1`

[`tools/pid_scan.ps1`](tools/pid_scan.ps1) rileva da un PC Windows, senza ESP, le centraline e i PID supportati dalla vettura tramite un adattatore ELM327 USB su porta COM, come il Vgate vLinker FS USB (bridge FTDI, 115200 baud di fabbrica). Usa solo servizi di lettura: nessun comando cancella i DTC o modifica lo stato delle centraline. Gira con Windows PowerShell 5.1, senza dipendenze.

Per ogni centralina che risponde sul CAN 11 bit legge le bitmap dei supportati dei Mode 01, 02 (freeze frame 0), 06 e 09 e, se dichiarato, il nome (Mode 09 InfoType `0x0A`). È indipendente dal firmware: rispetto alla discovery di §5.1 aggiunge i supportati dei Mode 02 e 06, ma non verifica le risposte PID per PID come la scansione approfondita e non legge OBFCM.

Si lancia a quadro acceso e con FORScan e VCDS chiusi, perchè la porta COM è esclusiva:

```powershell
.\tools\pid_scan.ps1 -Port COM5                    # porta del vLinker in Gestione dispositivi
.\tools\pid_scan.ps1                               # se c'è una sola porta COM
.\tools\pid_scan.ps1 -Port COM5 -BaudRate 115200   # baud fisso invece della ricerca
.\tools\pid_scan.ps1 -Port COM5 -Verbose           # anche il traffico TX/RX a console
```

Se la policy di esecuzione blocca gli script locali: `powershell -ExecutionPolicy Bypass -File .\tools\pid_scan.ps1 -Port COM5`.

Senza `-BaudRate` lo script prova 115200, 38400, 230400 e 500000 finchè `ATI` risponde ELM327; senza `-Port` usa l'unica porta presente e, se ce ne sono più di una, si ferma elencandole. L'adattatore lavora con gli header CAN attivi (`ATH1`) e il protocollo automatico (`ATSP0`): la prima `0100` avvia la ricerca del protocollo ed è ripetuta fino a tre volte, e lo script si ferma se il veicolo non usa il CAN 11 bit (protocolli 6 e 8).

Il report `pid_scan_<data>_<ora>.txt` viene scritto nella cartella dello script anche dopo un errore o un Ctrl+C. Contiene adattatore, protocollo e tensione, un blocco per centralina con i PID supportati di ogni Mode e le bitmap grezze, e in coda la traccia TX/RX completa. Come il report del firmware, va conservato in [`DOCS/`](DOCS/).

> ℹ️ Le unità vLinker FS prodotte prima del 7/2/2025 hanno problemi su alcune porte USB 3.0 e vanno collegate tramite un hub USB 2.0. L'interfaccia Ross-Tech HEX-V2 di VCDS non è utilizzabile: si presenta come dispositivo HID, senza porta COM nè comandi ELM327.

---

## 6. Software e build

### Librerie Arduino

Comuni a entrambi gli sketch:
- `ESP8266WiFi` (core ESP8266 ≥ 3.1.2)
- `ESP8266WebServer`
- `ESP8266HTTPUpdateServer`
- `U8g2` (di olikraus): driver OLED
- `Wire` (core)

Solo per `CANbus_conn`:
- `mcp_can` (di coryjfowler): driver MCP2515
- `SPI` (core)

### Toolchain

- **Arduino CLI** + core `esp8266:esp8266 @ 3.1.2`
- VSCode + estensione **Arduino Community Edition** (`vscode-arduino.vscode-arduino-community`)
- FQBN: `esp8266:esp8266:d1_mini_clone` per il D1 Mini; per l'ESP-01 (solo `WIFI_conn`) `esp8266:esp8266:generic` con Flash size "1MB (FS:none OTA:~502KB)" (`eesz=1M`) e Flash mode DOUT (`FlashMode=dout`)

### Build & flash

Il progetto è strutturato come **due sketch fratelli** in sottocartelle, con header condivisi nella root del repo. Ogni sketch trova gli `.h` condivisi grazie al flag `-I<root del progetto>` passato in `build.extra_flags` (`arduino.json` in `.vscode/`); anche `BoostModel.h` e `TorqueModel.h` includono `EngineConstants.h` per nome secco, quindi la build non dipende dal sistema operativo. Su Windows [`build.ps1`](build.ps1) passa lo stesso flag e verifica solo che il codice compili, senza esportare binari nè caricarli sulla board.

Comando indicativo con `arduino-cli`, lanciato dalla root del repo (variante CAN):

```bash
arduino-cli compile \
  --fqbn esp8266:esp8266:d1_mini_clone \
  --build-property "build.extra_flags=-I$PWD" \
  CANbus_conn

arduino-cli upload -p /dev/cu.usbserial-XXXX \
  --fqbn esp8266:esp8266:d1_mini_clone \
  CANbus_conn
```

Sostituire `CANbus_conn` con `WIFI_conn` per l'altra variante.

> ⚠️ Il budget IRAM è quasi esaurito: evitare di aggiungere funzioni `IRAM_ATTR`. Su ESP-01 il binario di `WIFI_conn` deve inoltre restare sotto i circa 502 KB che la flash da 1 MB riserva all'OTA.

---

## 7. Configurazione

Tutte le costanti modificabili sono in cima a ciascuno sketch.

### `CANbus_conn/CANbus_conn.ino`

| `#define` | Default | Significato |
|---|---|---|
| `MCP2515_CRYSTAL` | `MCP_8MHZ` | `MCP_8MHZ` o `MCP_16MHZ`, dipende dal modulo |
| `CAN_CS_PIN` | `15` | D8 |
| `CAN_SPEED` | `CAN_500KBPS` | velocità bus OBD2 |
| `ISOTP_CONSECUTIVE_TIMEOUT_MS` | `150` | attesa massima fra due consecutive frame (N_Cr di ISO 15765-4) |
| `RESPONSE_PENDING_TIMEOUT_MS` | `5000` | attesa estesa dopo una risposta `7F xx 78` (P2*CAN) |
| `SILENT_CYCLES_BEFORE_RECOVERY` | `10` | cicli senza risposta dal motore prima di reinizializzare il MCP2515 (bus-off) o ripetere lo scan |
| `OTA_SSID` / `OTA_PASS` | `OBD2_UPDATE` / `alesimattia` | SoftAP per OTA |
| `OTA_WINDOW_MS` | `180000` | finestra OTA: 3 min dal boot (estesa se un client si connette) |
| `DIAG_PRINT_INTERVAL_MS` | `5000` | intervallo minimo fra due stampe della diagnostica seriale |
| `DTC_PER_PAGE` | `2` | DTC per pagina display |

ID OBD, finestre di ascolto (150 ms allo scan, 100 ms a regime) e numero massimo di DTC (`OBD_MAX_DTC` = 12) sono costanti condivise in [`obd_proto.h`](obd_proto.h), [`obd_scan.h`](obd_scan.h) e [`obd_live.h`](obd_live.h).

### `WIFI_conn/WIFI_conn.ino`

| `#define` | Default | Significato |
|---|---|---|
| `ELM327_SSID` / `ELM327_PASS` | `WiFi_OBDII` / `""` | rete dell'adattatore |
| `ELM327_IP` | `192.168.0.10` | IP TCP dell'ELM327 |
| `ELM327_PORT` | `35000` | porta TCP dell'ELM327 |
| `ELM327_TIMEOUT` | `2000` | timeout di lettura della risposta (ms); l'adattatore risponde `NO DATA` dopo circa 200 ms |
| `ELM327_FIRST_TIMEOUT` | `5000` | timeout sulla prima richiesta OBD dopo l'inizializzazione |
| `ELM327_PROTOCOL` | `ATSP6` | CAN 11 bit 500 kbps (gruppo VAG); usare `ATSP0` per auto |
| `SILENT_CYCLES_BEFORE_RECOVERY` | `10` | cicli senza risposta dal motore prima della riconnessione all'ELM327 |
| `DIAG_PRINT_INTERVAL_MS` | `5000` | intervallo minimo fra due stampe della diagnostica seriale |
| `OTA_SSID` / `OTA_PASS` | `OBD2_UPDATE` / `alesimattia` | SoftAP per OTA |
| `BUTTON_PIN` | `D5` (D1 Mini), `3` (ESP-01) | pin del pulsante, scelto dal profilo della board (§4.3) |
| `LDR_ENABLED` | `0` solo su ESP-01 | lettura dell'LDR, disattivata dove manca l'ingresso analogico (§12) |

---

## 8. Lettura live multi-PID

SAE J1979 impone alle centraline di accettare fino a 6 PID in una sola richiesta Mode 01, con un'unica risposta che contiene solo i PID supportati. A ogni ciclo del monitor il firmware ([`obd_live.h`](obd_live.h), `obdLiveStep()`) invia quindi una sola richiesta al motore con:

| Gruppo | PID | Frequenza |
|---|---|---|
| Veloce | boost `0x70` (oppure MAP `0x0B` se `0x70` manca), carico `0x04`, RPM `0x0C`, MAF `0x10`, coppia `0x62` | a ogni ciclo |
| Lento | liquido `0x05`, IAT `0x0F`, BARO `0x33`, rail `0x23`, tensione `0x42`, batteria ibrida `0x5B`, attrito `0x8E`, MAP `0x0B` se il boost viene da `0x70` | un PID per ciclo, a rotazione |

Entrano solo i PID che il motore ha dichiarato. La risposta, tipicamente di 3-4 frame, viene separata con le lunghezze SAE di ogni PID (`obdPidDataLen()` in [`obd_proto.h`](obd_proto.h)). Lo scan verifica che il motore accetti una richiesta con due PID; se non la accetta, o se a regime arrivano 3 risposte multi-PID malformate di fila, il firmware passa a un PID per ciclo con la stessa priorità (gruppo veloce tre volte, gruppo lento una).

Dopo ogni ciclo `obdLiveRecalc()` aggiorna i valori mostrati, con l'indicazione "misurato" o "stimato" in dashboard:

- **Boost**: `(pressione 0x70 − BARO) / 100` bar, misurato prima della farfalla e senza la saturazione del MAP; senza BARO fa da riferimento il MAP letto a motore fermo. Senza `0x70` resta la stima di `BoostModel` (§9).
- **Coppia**: `(0x62 − 0x8E) / 100 × 0x63` Nm all'albero; senza `0x8E` solo `0x62`, senza `0x63` i 320 Nm di `TORQUE_MAX_NM`. Senza `0x62` resta la stima di `TorqueModel` (§9), e se mancano i suoi ingressi la casella mostra `N/D`.

**Stale detection**: dopo 3 letture fallite consecutive dello stesso PID il valore in cache torna `NAN` o non disponibile, così display e dashboard mostrano `N/D` invece di un valore congelato. Dopo 10 cicli senza alcuna risposta dal motore: sul ramo CAN il MCP2515 viene reinizializzato se l'`EFLG` segnala bus-off, altrimenti si ripete lo scan; sul ramo WiFi la connessione TCP all'ELM327 viene chiusa e ristabilita.

---

## 9. Modelli fisici e calcoli derivati

Tutti i parametri specifici del motore vivono in [`EngineConstants.h`](EngineConstants.h) sotto il namespace `Audi40TFSI150kW`: per adattare il firmware a un motore differente si modifica solo lì.

| Costante | Valore | Origine |
|---|---|---|
| `ENGINE_DISPLACEMENT_M3` | 0.001984 | cilindrata 1984 cm³ |
| `TORQUE_MAX_NM` / `POWER_MAX_KW` | 320 / 150 | dati di targa Audi |
| `TORQUE_PLATEAU_START_RPM` / `_END_RPM` | 1450 / 4475 | intervallo di coppia massima Audi |
| `K_AIR_GPS_PER_KW` | 1.02 | BSFC benzina di circa 250 g/kWh × 14.7 / 3600 |
| `IDLE_MIN_RPM` | 550 | soglia sotto il minimo del motore caldo |
| `MAX_VALID_RPM` | 6500 | potenza massima fino a 6000 rpm, più margine |
| `FUEL_STOICHIOMETRIC_AFR` / `FUEL_DENSITY_G_PER_L` | 14.7 / 745 | benzina EN 228 |
| `FINAL_DRIVE_RATIO` / `TYRE_ROLLING_CIRCUMFERENCE_M` | 4.234 / 2.00 | S-tronic `0CK`, pneumatico 225/50 R17 |

I modelli intervengono solo quando la centralina non espone boost (`0x70`) o coppia (`0x62`), §8. `BoostModel.h` e `TorqueModel.h` sono forniti e mantenuti dall'autore dei modelli fisici, quindi la ritaratura delle formule va fatta lì.

> ⚠️ Le costanti contengono i valori della DMSA, ma alcune formule dei modelli non sono ancora adeguate al benzina:
>
> | Punto | Dove | Effetto sul benzina |
> |---|---|---|
> | Correzione rail del modello di coppia | `railPressureCorrection()` in [`TorqueModel.h`](TorqueModel.h) | si aspetta almeno 250-950 bar secondo il carico: con il rail benzina il correttore resta al minimo 0.88 e la coppia stimata risulta circa il 12% più bassa |
> | Correzione VE del modello di coppia | `veCorrection()` in [`TorqueModel.h`](TorqueModel.h) | correttore limitato a 0.85-1.05 attorno a un'efficienza volumetrica di riferimento di 0.90, non verificata sul 2.0 TFSI |
> | Modello di boost | [`BoostModel.h`](BoostModel.h) | perdita di carico fra compressore e collettore con coefficienti non verificati sul circuito aria del 2.0 TFSI |

### `BoostModel.h`: stima pressione turbo

Output: pressione turbo **relativa in bar** (può essere negativa = depressione).

```
boost = (MAP - BARO) + ΔP_charge_path
```

dove `ΔP_charge_path` stima la perdita di carico fra compressore e collettore in funzione di `MAF`, `RPM`, `IAT`, `LOAD` e dell'efficienza volumetrica apparente. Filtro EMA opzionale su MAP/BARO/MAF/LOAD (`BOOST_EMA_ALPHA_*`). Quando il motore è in vera depressione a basso carico, la perdita di carico aggiunta viene proporzionalmente ridotta per non falsare il segno.

### `TorqueModel.h`: stima coppia motore

Output: **coppia in Nm** (clamp `0..420`).

```
T = T_nom(rpm) · loadFactor · cMAF · cVE · cRail · cVolts · cBARO
```

- `T_nom(rpm)` è costruita dalle costanti: smoothstep da 550 rpm al plateau, plateau pieno (320 Nm) fra 1450 e 4475 rpm, sopra il plateau limite imposto dalla potenza (`9550 · 150 / rpm`, circa 239 Nm a 6000 rpm).
- `loadFactor = LOAD/100`, clampato leggermente sopra 1 per non perdere transitori.
- `cMAF`, `cVE` sono correzioni vere derivate da MAF/MAP/IAT con clamp stretti per evitare overshoot.
- `cRail`, `cVolts`, `cBARO` sono correzioni *deboli* opzionali (NAN ⇒ `1.0`) usate solo per non sovrastimare in undervoltage o in alta quota.

Filtro EMA opzionale su LOAD/MAF/MAP. Quando viene chiamato dalla lettura live il filtro è disattivato (`useFilter=false`) perchè lo smoothing è già implicito nella cache dei PID.

### Calcoli derivati

Calcolati una sola volta da `obdDiagCompute()` in [`obd_live.h`](obd_live.h) e usati sia dalla dashboard (oggetto `diag` di `/data`, §11.4) sia dalla diagnostica seriale. Ogni grandezza vale `null` quando mancano i PID sorgente.

| Grandezza | Formula | Campo `diag` |
|---|---|---|
| Potenza | `coppia × RPM / 9549`, con la coppia letta quando disponibile (§8); i CV li calcola la pagina | `powerKw` |
| Rapporto aria/carburante | `λ × 14.7` | `afr` |
| Portata carburante | `0x5E`, altrimenti `MAF / (14.7 × λ)` convertita con 745 g/L | `fuelLh`, fonte in `fuelSrc` |
| Consumo | `portata L/h × 100 / velocità`, sopra 3 km/h | `fuelL100` |
| Consumo specifico (BSFC) | `portata g/s × 3600 / potenza kW` | `bsfc` |
| Altitudine ISA | `44330 × (1 − (BARO / 101.325)^0.1903)` | `altitude` |
| Densità dell'aria nel collettore | `MAP × 1000 / (R_aria × (IAT + 273.15))` | `airDensity` |
| Rapporto di compressione | `p / BARO`, con `p` dal sensore boost `0x70` o in sua assenza dal MAP | `pressureRatio` |
| Efficienza intercooler | `(T_teorica − IAT) / (T_teorica − T_esterna)`, con `T_teorica` della compressione isoentropica dell'aria esterna al rapporto di compressione | `intercoolerEff` |
| Efficienza volumetrica | `MAF × 12000 / (cilindrata_L × RPM × ρ_aria)` | `volEff` |
| Drift pedale D/E | `abs(0x49 − 0x4A)` | `driftPedal` |
| Δ farfalla-pedale | `0x4C − 0x49` | `deltaThrottlePedal` |
| Taglio iniezione (DFCO) | `0x04 < 1 %` a motore acceso | `dfco` |
| Motore fermo (veleggio o start-stop) | `0x0C < 1` rpm | `engineStopped` |
| Rapporto cambio | `RPM × 60 × 2.00 / (velocità × 1000 × 4.234)`, sopra 5 km/h; riconosce la marcia con la tabella di §5 | `gearRatio` |
| Accelerazione, variazione boost | differenza fra due letture valide | `accel`, `boostRate` |

> ℹ️ AFR e, senza `0x5E`, il consumo usano `FUEL_STOICHIOMETRIC_AFR` = 14.7 e `FUEL_DENSITY_G_PER_L` = 745. Con la benzina E5 distribuita in Italia lo stechiometrico reale è circa 14.4, quindi il consumo stimato risulta inferiore al reale di circa il 2%.

---

## 10. Pulsante fisico

Pulsante NA verso GND (pin in §4), gestito con `INPUT_PULLUP` e debounce software 50 ms ([`button_handler.h`](button_handler.h)). Nessuna ISR, nessun `delay()`. La macchina a stati genera tre eventi: `BTN_NONE`, `BTN_SHORT`, `BTN_LONG`.

| Azione | Effetto |
|---|---|
| **Click breve** | Scorre ciclicamente le schermate del display: Monitor → DTC pag.1 → DTC pag.2 → … → Monitor. Avanza manualmente la pagina e posticipa l'auto-cycle dei 5 s. |
| **Pressione lunga** (≥ 2.5 s) | 🛠️ **Cancella i codici di errore DTC** di tutte le centraline con Mode 04 (§13) e mostra l'esito sul display per 1.2 s. |

---

## 11. Interfaccia web `/dashboard`

Tutta la logica HTTP vive in [`web_dashboard.h`](web_dashboard.h), abilitata via `#define OBD_CONN_CAN` o `#define OBD_CONN_WIFI` nello sketch (entrambi i casi sono già configurati).

### 11.1 SoftAP e accesso

Entrambi gli sketch aprono il SoftAP `OTA_SSID` con la password `OTA_PASS` (§7); la dashboard è su `http://192.168.4.1/dashboard`. L'AP resta attivo per `OTA_WINDOW_MS` (3 min) dal boot: se un client si collega il timeout viene **sospeso** e alla disconnessione riparte da capo. Scaduta la finestra il server HTTP viene fermato e, su `CANbus_conn`, la radio WiFi viene spenta per risparmiare circa 75 mA; per aggiornare di nuovo il firmware serve un power cycle, scelta deliberata per non tenere la radio accesa durante la guida.

### 11.2 Endpoint HTTP

| Path | Metodo | Risposta | Note |
|---|---|---|---|
| `/dashboard` | GET | HTML | dashboard mobile-first, polling `/data` ogni 500 ms |
| `/scan` | GET | HTML | la stessa pagina della dashboard, aperta sulla tab "PID supportati" |
| `/data` | GET | JSON | valori principali + DTC + LDR; con `debug=true` anche l'istantanea diagnostica (§11.4) |
| `/debug` | GET | `{"debug":bool}` | toggle runtime modalità debug, query `?on` / `?off` |
| `/brightness` | GET | `{"ldr":N\|null,"contrast":N,"mode":"auto\|manual\|fading\|fixed","ldrAtOverride":N}` | stato luminosità; `?set=N` (0-255) attiva override manuale, `?auto` rientra in automatico con fade; finchè la presenza dell'LDR non è provata `ldr` vale `null` e il modo automatico è `fixed` |
| `/clear-dtc` | GET | `{"ok":bool,"msg":"..."}` | Mode 04 a tutte le centraline con esito leggibile; rifiutata durante scan e scansione approfondita |
| `/serial-data` | GET | `text/plain` | byte da `?since=N` (cursor del client) fino al più recente nel buffer circolare di `WebSerial`; header `X-Seq:<n>` (nuovo cursor) e `X-Dropped:0\|1` (overflow). Polling consigliato a 250 ms dalla tab "Serial monitor" |
| `/scan-data` | GET | JSON | risultato dello scan, in chunked encoding: `{probe, adapter, engine, obfcm, ecus:[{name, detail, physical, probed, pids[], undoc, mode09[], undoc09, silent[], extra[], extra09[], silentUndoc, extraUndoc}]}`; elenchi per descrizione, senza codici esadecimali |
| `/probe` | GET | JSON | `?start` avvia la scansione approfondita (solo dopo uno scan riuscito); risponde `{started, status}` |
| `/probe-status` | GET | JSON | `{state:"idle\|running\|done", percent, seconds}` |
| `/update` (CAN) `/ota` (WiFi) | GET / POST | upload firmware | servito da `ESP8266HTTPUpdateServer` |

> ⚠️ Nessuna autenticazione, CORS o rate limiting: il modello di minaccia è la sola rete locale del SoftAP / dell'ELM327.

### 11.3 Layout della dashboard

La pagina è suddivisa in **3 tab** (mobile-first, larghezza max 480 px).

#### Tab 1, Dashboard (default)

1. **Toggle "Diagnostica Serial"**: attiva o disattiva la modalità debug (vedi sotto).
2. **Dati principali**: sempre visibili, BOOST e COPPIA con l'indicazione "misurato" o "stimato", TEMP, BATTERIA IBRIDA (4 box grandi, valori a colori).
3. **Sezioni avanzate**, visibili solo con `debug=true`:
   - 🚙 Motore (carico, RPM, MAF, rail, lambda, lambda comandata, AFR, temp liquido, temp olio, IAT)
   - 🚗 Veicolo (velocità, farfalla, rapporto cambio, accelerazione, pedale D/E)
   - ⛽ Consumi (L/100km, portata carburante con la fonte, cons. specifico g/kWh, potenza kW/CV)
   - 🌪️ Turbo (boost con la fonte, pressione del sensore boost, rapporto compressione, eff. intercooler, variazione boost)
   - 🔧 Diagnostica (batteria, motore acceso o fermo, drift pedale, taglio iniezione, eff. volumetrica, Δ farfalla)
   - 🌍 Ambiente (temp esterna, baro, altitudine, densità aria)
   - ⏱️ Contatori (tempo motore, km con MIL, riscaldamenti dal clear)
4. **Box DTC** (visibile solo se `dtcCount > 0`): riquadro rosso con badge MIL (solo se accesa) e lista di codice, centralina e descrizione.
5. **Card "Luce ambiente"**: valore LDR live (N/D senza LDR), contrasto applicato, modalità (`auto`/`manual`/`fading`, `fixed` senza LDR), slider 0-255 e bottone "Reset auto".
6. **Bottone "Cancella DTC"**: invia `/clear-dtc` (con conferma).

**Modalità debug**: il toggle chiama `/debug?on` o `/debug?off` e controlla la variabile `debugMode`, che parte `false` al boot e resta in RAM fino al riavvio; lo stato torna anche nel JSON di `/data`, così il toggle resta allineato se la pagina viene ricaricata. Con debug attivo ogni 5 s `obdPrintDiagnostics()` stampa su Serial (115200 baud) l'istantanea diagnostica con i riferimenti tipici dell'EA888, e ogni richiesta `/data` legge circa 4-5 gruppi multi-PID: il refresh dell'OLED rallenta, quindi è uno strumento di tuning e non una modalità d'esercizio.

I valori vengono colorati di **rosso** (allarme) o **giallo** (warning) quando fuori range:

| Parametro | 🟡 Warning | 🔴 Allarme |
|---|---|---|
| Batteria 12 V | < 12.2 V o > 15.0 V | < 11.8 V o > 15.3 V (misurati sulle B9: 11.9-14.9 V) |
| Drift pedale D/E | 3-5 % | > 5 % |
| Lambda | — | < 0.75, oppure > 1.15 fuori dal taglio iniezione e a motore acceso |
| Temp. liquido | > 110 °C | > 115 °C (normale fino a 107 °C a carico parziale, termostato d'emergenza a 113 °C) |
| Press. rail | > 360 bar (massimo dell'evo4: 350) | < 30 bar a motore acceso (misurati 70-100 al minimo) |
| Eff. intercooler | — | < 40 % |
| Cons. specifico | — | > 400 g/kWh con carico > 50 % |
| Eff. volumetrica | — | < 60 % |
| Boost | > 1.3 bar | > 1.5 bar |

> ℹ️ Le soglie vengono da dati EA888 Gen3/evo4 (formazione Audi, comunicato VW) e dalle catture B9, non da dati specifici DMSA: vanno confermate con i log della vettura.

#### Tab 2, PID supportati

Risultato dello scan, uguale nelle due varianti: fonte di coppia e boost e modalità di lettura del motore, consumo reale di vita OBFCM, pulsante e avanzamento della scansione approfondita, poi per ogni centralina il nome (es. "Motore" con `ECM-EngineControl`), i PID dichiarati per descrizione, le InfoType Mode 09 e, dopo la scansione approfondita, i PID dichiarati ma muti e quelli che rispondono senza essere dichiarati. Dati da `/scan-data`; la pagina `/scan` apre direttamente questa tab.

#### Tab 3, Serial monitor

Specchio web del Serial monitor: mostra tutto ciò che il firmware scrive su `Serial.*`, catturato dal wrapper `WebSerial` ([`serial_logger.h`](serial_logger.h)) in un buffer circolare di 4 KB e letto con polling di 250 ms su `/serial-data`; un overflow fra due letture viene segnalato da un banner. L'auto-scroll si sospende quando si scorre verso l'alto, e "Pulisci" svuota solo la vista locale, non il buffer del firmware.

### 11.4 Schema JSON `/data`

La risposta è inviata in chunked encoding; i valori non disponibili valgono `null`.

**Sempre presenti** (nessuna interrogazione del bus, solo i valori della lettura live):
```json
{
  "debug": false, "busy": false, "mil": false, "dtcCount": 1,
  "boost": 0.85, "coolant": 92, "torque": 211, "hybBatt": null,
  "boostSrc": "ECU", "torqueSrc": "ECU",
  "lightLevel": 740, "contrast": 180, "brightnessMode": "auto",
  "dtc": [{"code": "P0420", "ecu": "Motore", "desc": "Efficienza catalizz."}]
}
```

`busy` è vero durante connessione, scan e scansione approfondita: in quel caso la diagnostica non viene letta. `lightLevel` vale `null` finchè la presenza dell'LDR non è provata (§12), e allora `brightnessMode` vale `fixed` finchè non si usa lo slider.

**Con `debug=true`** si aggiunge l'oggetto `diag`, letto al momento della richiesta in gruppi multi-PID con i soli PID dichiarati; i campi calcolati sono descritti in §9: `engineStopped`, `load`, `coolant`, `oilTemp`, `map`, `boostKpa`, `rpm`, `speed`, `iat`, `maf`, `railBar`, `lambda`, `lambdaCmd`, `o2v`, `o2ma`, `baro`, `volts`, `ambient`, `pedalD`, `pedalE`, `throttle`, `runtime`, `kmMil`, `starts`, `boost`, `boostSrc`, `fuelSrc`, `torque`, `powerKw`, `afr`, `fuelLh`, `fuelL100`, `bsfc`, `altitude`, `airDensity`, `intercoolerEff`, `pressureRatio`, `volEff`, `driftPedal`, `deltaThrottlePedal`, `gearRatio`, `accel`, `boostRate`, `dfco`.

### 11.5 Anteprima offline

[`monitor_preview.html`](monitor_preview.html) contiene la stessa pagina di `DASHBOARD_HTML` in [`web_dashboard.h`](web_dashboard.h), più uno script che sostituisce `fetch()` e simula gli endpoint del firmware con valori realistici per la A5 F5 40 TFSI: guida mista con fasi di veleggio a motore spento, tre centraline con OBFCM, scansione approfondita, DTC e rifiuto della cancellazione a motore acceso. Cliccando l'immagine si apre la versione live.

<p align="center">
  <a href="https://raw.githack.com/alesimattia/OBD2-car_dashboard-OLED/main/monitor_preview.html">
    <img src="DOCS/monitor_preview.png" width="320" alt="Dashboard web con la diagnostica attiva">
  </a>
</p>

Se cambia `DASHBOARD_HTML` l'anteprima va rigenerata copiando la pagina e mantenendo lo script di simulazione; quando cambiano le anteprime vanno ricatturati anche gli screenshot `DOCS/ui_preview.png` e `DOCS/monitor_preview.png`.

---

## 12. Auto-brightness OLED

Il contrasto del display è regolato in funzione della luce ambientale letta da un fotoresistore (LDR) sull'ADC `A0`. La logica vive in [`light_sensor.h`](light_sensor.h) ed è chiamata polled da `loop()` (`updateBrightness()`); nessun `IRAM_ATTR`, nessun ISR. L'LDR è opzionale (vedi "LDR assente" più sotto).

**Schema cablaggio** (D1 Mini, identico nelle due varianti):

```
3.3V ──[ LDR ]── A0 ──[ 10 kΩ ]── GND
```

Più luce → R_LDR ↓ → V_A0 ↑ → ADC ↑ → contrasto ↑. Il D1 Mini integra già un partitore interno che riporta A0 al range 0-3.3 V.

**State machine**, tre stati:

| Stato | Comportamento |
|---|---|
| `auto` | Contrasto target = `adcToContrast(LDR filtrato)`, applicato con isteresi 3 unità per evitare flicker. |
| `manual` | L'utente ha mosso lo slider del web dashboard. Il contrasto resta al valore manuale finchè la luce ambientale non varia di più di `LDR_SCENE_DELTA` (default 150 conteggi ADC) rispetto al valore al momento dell'override. |
| `fading` | Transizione graduale verso il nuovo target automatico (`BR_FADE_STEP` = 4 unità ogni `BR_FADE_TICK_MS` = 30 ms → ~1.9 s per range pieno). Si entra qui da `manual` quando la scena cambia troppo, oppure dal bottone "Reset auto" del dashboard. |

**Filtraggio ADC**: mediana mobile su 5 campioni seguita da un'EMA con α = 0.15. La mediana elimina fino a 2 campioni anomali consecutivi di qualunque ampiezza (disturbi della SPI del MCP2515 o del WiFi) e lascia passare in 3 campioni un cambio di luce persistente, quindi la media non resta mai bloccata, nemmeno all'uscita da una galleria. Il settling al 95% di una variazione netta di luce vale `t_95% ≈ ln(0.05) / (LDR_SAMPLE_HZ · ln(1 − α)) ≈ 18.4 / LDR_SAMPLE_HZ` secondi più i 2 campioni di ritardo della mediana: con i 10 Hz di default il contrasto segue la nuova luce in circa 2.0 s, a 4 Hz in circa 5 s.

**LDR assente o buio totale**: senza LDR il partitore interno del D1 Mini tiene A0 a massa, e un LDR al buio totale (oltre circa 2.5 MΩ) dà la stessa lettura, quindi i due casi non si distinguono elettricamente. Una lettura alta invece prova che l'LDR c'è, perchè un pin a massa non la produce mai. Il firmware sfrutta questa asimmetria:
- **finchè la presenza non è provata** il contrasto resta a `defaultBrightness` = 128, il 50% del registro di contrasto dell'SH1106, e le letture basse non entrano nella media nè nella calibrazione;
- **alla prima lettura filtrata di almeno 12 conteggi** (`LDR_PRESENT_MIN_ADC`) la presenza è provata fino al riavvio e la regolazione automatica parte dal valore corrente;
- **da lì in poi** una lettura vicina a 0 è buio e porta il contrasto al minimo, come qualsiasi luce scarsa: il display non diventa mai più luminoso quando l'ambiente si scurisce.

Prima della prova di presenza la dashboard mostra LDR `N/D` e modo `fixed`, lo slider funziona e "Reset auto" riporta a 128. Il profilo ESP-01 di `WIFI_conn` non legge mai l'ADC (`LDR_ENABLED` = 0).

**Mapping ADC → contrasto**: gamma 2.0 sul range utile calibrato dinamicamente (vedi sotto), output limitato a `BR_CONTRAST_MIN..BR_CONTRAST_MAX` (5..240 di default) per evitare di spegnere il pannello al buio assoluto e per non saturare il driver in pieno sole.

**Auto-calibrazione live del range LDR**: i limiti ADC usati da `adcToContrast()` (`ldrMinObserved` / `ldrMaxObserved` nello stato `BrightnessState`) si adattano alla luce effettivamente osservata in sessione, senza taratura manuale: il sistema parte con un range fallback `30..950` e durante l'uso registra gli estremi del valore EMA filtrato. Un decay attivo (~40 unità/minuto, agisce solo se un bound è > 20 conteggi oltre l'EMA corrente) permette di "dimenticare" rapidamente outlier accidentali (riflessi, flash di sole nel parabrezza). Calibrazione e override manuale vivono solo in RAM e ripartono da zero a ogni boot, per non consumare i cicli di scrittura della EEPROM.

**Comportamento atteso nel tempo**:

| Fase | Stato bounds osservati | Mappatura usata | Tempi |
|---|---|---|---|
| Boot → +2s (warm-up) | vuoti (min=1023, max=0) | fallback 30..950 | 2 s |
| +2s → primi sample | espansione veloce | fallback finchè range < 100 | tipicamente < 10 s |
| Sessione stabilizzata | range > 100 sull'ambiente reale | bounds osservati | regime |
| Outlier isolato (flash sole) | max espanso, poi decay | bounds che si correggono | ~2-3 min per outlier di 100 unità |
| Reboot | reset a vuoti | fallback, poi ricalibra | 2 s + qualche secondo |

**Costanti tunabili** in [`light_sensor.h`](light_sensor.h):

| `#define` | Default | Significato |
|---|---|---|
| `LDR_ENABLED` | `1` | Lettura dell'LDR; gli sketch senza ingresso analogico la disattivano definendola a `0` prima dell'include |
| `defaultBrightness` | `128` | Contrasto senza LDR: 50% del registro di contrasto dell'SH1106 (0-255) |
| `LDR_SAMPLE_HZ` | `10` | Frequenza di campionamento LDR (Hz), da cui `LDR_SAMPLE_MS = 1000 / LDR_SAMPLE_HZ` |
| `LDR_MEDIAN_WINDOW` | `5` | Campioni della mediana mobile davanti all'EMA |
| `LDR_PRESENT_MIN_ADC` | `12` | Lettura filtrata che prova la presenza dell'LDR fino al riavvio; un pin a massa non la raggiunge |
| `LDR_SCENE_DELTA` | `150` | Variazione di luce, rispetto al valore al momento dell'override, che riporta da `manual` ad `auto` passando per `fading`. Circa il 16% della scala: distingue l'ingresso in galleria dalle fluttuazioni normali; più alto rende lo slider più persistente |
| `BR_HYST_DELTA` | `3` | Differenza minima, su scala 0-255, per riapplicare `setContrast` in `auto`: assorbe l'oscillazione di ±1 dell'EMA a luce stabile ed evita scritture I²C inutili |
| `BR_FADE_STEP` | `4` | Δ contrasto per tick di fade: più alto accorcia il fade ma rende gli scalini percepibili |
| `BR_FADE_TICK_MS` | `30` | Periodo del tick di fade; un fade dura `(\|target − current\| / BR_FADE_STEP) · BR_FADE_TICK_MS`, da 5 a 240 circa 1.76 s |
| `BR_CONTRAST_MIN` | `5` | Floor: un minimo non nullo tiene leggibili le scritte anche al buio assoluto; aumentarlo se di notte il display è invisibile, ridurlo se abbaglia |
| `BR_CONTRAST_MAX` | `240` | Ceiling: oltre, la luminosità non cresce in modo apprezzabile e il pannello si usura prima (burn-in); aumentarlo solo se in pieno sole il display è poco leggibile |
| `LDR_CALIBRATION_FALLBACK_MIN` | `30` | Bound minimo del fallback ADC (sessione appena iniziata) |
| `LDR_CALIBRATION_FALLBACK_MAX` | `950` | Bound massimo del fallback ADC |
| `LDR_CALIBRATION_MIN_RANGE` | `100` | Span minimo per usare i bound osservati |
| `LDR_CALIBRATION_WARMUP_MS` | `2000` | Warm-up post-boot prima di calibrare |
| `LDR_CALIBRATION_DECAY_MS` | `3000` | Periodo del tick di decay anti-outlier |
| `LDR_CALIBRATION_DECAY_STEP` | `2` | Unità di decay per tick (2/3s ≈ 40/min) |
| `LDR_CALIBRATION_DECAY_MARGIN` | `20` | Distanza dall'EMA oltre la quale parte il decay |

---

## 13. DTC, codici di errore

- **Lettura** (`obdLiveCheckDtcs()` in [`obd_live.h`](obd_live.h), all'ingresso nel monitor e poi ogni 30 s): `0101` in broadcast raccoglie stato MIL e numero di DTC confermati di **ogni centralina**; la MIL è accesa se lo è per almeno una. A ogni centralina con almeno un DTC, anche a MIL spenta, viene inviata una **Mode 03**, in indirizzamento fisico quando risponde.
  - La risposta su CAN contiene dopo `0x43` il **numero di DTC** (ISO 15765-4), poi due byte per codice; il riassemblaggio ISO-TP controlla sequenza e lunghezza dichiarata e ignora il riempimento.
  - Su CAN nativo il flow control va all'ID fisico della centralina; con l'ELM327 lo invia l'adattatore.
  - Se nessuna centralina risponde a `0101` resta lo stato precedente, così i codici non spariscono per un timeout.
- **Capienza**: fino a `OBD_MAX_DTC` = 12 codici, ognuno con la centralina che lo ha memorizzato.
- **Decodifica**: la categoria (P/C/B/U) e i 4 nibble del codice vengono ricostruiti in stringa "P0301".
- **Descrizione breve**: ricerca binaria in [`dtc_descriptions.h`](dtc_descriptions.h): 73 codici in PROGMEM, italiano, ≤ 21 caratteri, con quelli tipici del benzina (fasatura, sonde lambda, miscela, accensione e bobine, catalizzatore, EVAP, pompa carburante, filtro antiparticolato) e di rete (`U0100`, `U0101`). Per i codici assenti viene mostrato solo il codice.
- **Display**: alternanza automatica fra schermata dati e pagine DTC ogni `SCREEN_SWITCH_MS` (5 s), con codice e centralina sulla prima riga e descrizione sulla seconda. Il click breve sul pulsante avanza manualmente la pagina e posticipa l'auto-cycle (§10). La lettura OBD continua anche sulle pagine DTC, che si ridisegnano solo quando cambiano.
- **Cancellazione**: Mode 04 in broadcast su `0x7DF` (`obdLiveClearDtcs()`), dal pulsante con la pressione lunga (§10) o dal web dashboard (`/clear-dtc`). Il firmware raccoglie l'esito di ogni centralina, risposta positiva `0x44` oppure negativa `7F 04 xx`, e se almeno una ha accettato rilegge subito i DTC. Il display mostra:

| Esito | Testo OLED |
|---|---|
| tutte le centraline hanno accettato | `DTC cancellati` |
| alcune hanno accettato, altre rifiutato | `Cancellati in parte` |
| rifiuto `7F 04 22` (condizioni non corrette, tipicamente motore acceso) | `Spegnere il motore` |
| altro rifiuto | `Rifiutato` |
| nessuna risposta | `Nessuna risposta` |

> ⚠️ Cancellare i DTC perde lo storico errori e **resetta i monitor di readiness** della centralina. Dopo un reset i monitor possono richiedere diversi cicli di guida (anche 100+ km in condizioni miste) prima di tornare tutti in stato "Ready", lo stato richiesto per superare la revisione/MOT.

---

## 14. Note operative e limiti

- **Motore rimappato**: il modello di coppia assume un plateau piatto a 320 Nm fra 1450 e 4475 rpm con caduta a potenza costante oltre, fino ai 150 kW. Con una rimappatura vanno riallineati `TORQUE_MAX_NM`, `POWER_MAX_KW` e i limiti del plateau in [`EngineConstants.h`](EngineConstants.h).
- **MAP saturato**: il PID `0x0B` è 1 byte (0-255 kPa). Sopra ~2.5 bar assoluti il valore satura, quindi il boost stimato dal modello è limitato a ~1.5 bar relativi; il boost letto da `0x70` non ha questo limite.
- **Mild hybrid**: a motore spento nel veleggio e nello start-stop boost, coppia e potenza valgono zero senza che si tratti di un guasto di lettura (§5).
- **Adattatori ELM327 clone**: le versioni "v1.5" non sono prodotti Elm Electronics e possono rifiutare `ATH1`, `ATSH` o il suffisso del numero di risposte; il firmware lo verifica all'avvio e lo dichiara nel report dello scan.
- **Filo dell'LDR interrotto durante la guida**: dopo la prova di presenza il firmware non distingue un filo interrotto dal buio totale, quindi il display resta al contrasto minimo fino al riavvio successivo, che riparte da `defaultBrightness` (§12).
- **Refresh display**: l'aggiornamento parziale (`u8g2.updateDisplayArea`) trasmette solo le tile rows modificate, riducendo il traffico I²C da ~1024 byte a ~256-384 byte per frame.

---

## 15. Licenza

GNU GPL v3, vedi [`LICENSE`](LICENSE).
