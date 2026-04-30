---
name: Hardware setup
description: Differenze hardware tra i due sketch, vincoli pin ESP-01, scelte cablaggio e flash partition
type: project
---

Il progetto ha due sketch paralleli per due hardware diversi:

**WIFI_conn/WIFI_conn.ino → ESP-01 (1MB flash)**
- Solo GPIO0 e GPIO2 disponibili → usati per I2C OLED (SDA=GPIO0, SCL=GPIO2)
- Wire.begin(0, 2) — NON i pin standard D1/D2 di un D1 Mini
- Niente SPI possibile (mancano i pin) → impossibile usare MCP2515 su ESP-01
- Connessione OBD2 via WiFi a un adattatore ELM327
- Flash partition consigliata: 1M (no FS) per massimizzare lo spazio sketch + OTA

**CANbus_conn/CANbus_conn.ino → LOLIN D1 Mini (4MB flash)**
- Lettura OBD2 diretta via CAN bus con modulo MCP2515+TJA1050 integrato (8MHz)
- Pin SPI: CS=D8, MOSI=D7, MISO=D6, SCK=D5
- I2C OLED: SDA=GPIO4(D2), SCL=GPIO5(D1)
- WiFi usato SOLO per SoftAP OTA/dashboard (non c'è STA)
- Flash partition: 4M (no FS)

**Why:** L'ESP-01 era già disponibile e basta per la versione WiFi; il D1 Mini serve perché solo lui espone i pin SPI necessari al MCP2515.

**How to apply:** Quando l'utente fa modifiche al display o al cablaggio, ricorda che i pin I2C sono diversi tra i due sketch. Quando si parla di vincoli flash/spazio sketch, i limiti dell'ESP-01 sono molto più stretti del D1 Mini.
