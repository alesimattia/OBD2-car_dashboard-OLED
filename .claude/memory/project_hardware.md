---
name: project_hardware
description: CANbus_conn solo su LOLIN D1 Mini; WIFI_conn su D1 Mini o ESP-01 con profilo scelto dalla board (pulsante su RX e niente LDR sull'ESP-01); criteri di scelta dei pin e partizioni flash
metadata:
  type: project
---

**CANbus_conn:** solo LOLIN D1 Mini (FQBN `esp8266:esp8266:d1_mini_clone`, 4 MB, `4M (no FS)`), per scelta dell'utente e
perchè l'ESP-01 non espone l'SPI del MCP2515. MCP2515/TJA1050 a 8 MHz su CS=D8, MOSI=D7, MISO=D6, SCK=D5; OLED SDA=D2,
SCL=D1; pulsante su D3 (GPIO0, pin di boot); LDR opzionale su A0. L'utente non vuole un `#error` che blocchi altre board.

**WIFI_conn:** gira su LOLIN D1 Mini e su ESP-01, con il profilo scelto dalla macro della board selezionata nell'IDE.
- `ARDUINO_ESP8266_GENERIC` ("Generic ESP8266 Module", `eesz=1M`, `FlashMode=dout`) attiva il profilo ESP-01: pulsante su
  GPIO3 (RX) con la seriale `SERIAL_TX_ONLY`, possibile perchè il firmware non legge mai la seriale; `LDR_ENABLED` a 0,
  perchè l'ESP-01 non porta fuori l'ADC e il pin TOUT fluttuerebbe. Sul pulsante va una resistenza da 1 kΩ in serie, che
  protegge l'adattatore USB-seriale se viene premuto durante il flash.
- Ogni altra board usa il profilo D1 Mini: pulsante su D5 (GPIO14), LDR su A0.
- OLED su GPIO0/GPIO2 (`Wire.begin(0, 2)`) su entrambe le board. La variante `generic` del core non definisce i nomi `D0`-`D8`.

**Scelta del pulsante sul D1 Mini:** esclusi D0 (GPIO16 senza pull-up interno), D3/D4 (I²C e strapping di boot), D8 (GPIO15,
deve stare basso al boot, pull-down sulla scheda), RX/TX (convertitore USB), A0 (solo analogico). D1, D2, D5, D6 e D7 sono
equivalenti; resta D5 perchè già cablato.

**Flash ESP-01:** il binario di WIFI_conn deve restare sotto 512.000 byte per l'OTA: nel core 3.1.2 con `eesz=1M`
`FS_start` è a 0xFB000 e l'Updater vuole firmware vecchio e nuovo arrotondati a 4 KB dentro quello spazio (l'etichetta "OTA
~502KB" del menu sono 502 KiB). Si misura con le librerie stub di [[reference_build]] compilando per `generic`; U8g2 reale
aggiunge una quota stimata di 15-30 KB, da sommare. Le tabelle OBDb della scansione completa occupano circa 80 KB di PROGMEM.

**How to apply:** pin del pulsante e I²C differiscono fra gli sketch e fra i due profili di WIFI_conn; nuove periferiche su
ESP-01 possono andare solo sul bus I²C. Gestione dell'LDR assente in [[project_architecture]].
