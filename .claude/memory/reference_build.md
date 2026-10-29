---
name: reference_build
description: build.ps1 di A:\obd verifica la compilazione con la toolchain portable in C:\tmp\arduino; board e opzioni ESP8266, -I sulla radice, librerie non installate e verifica con librerie stub
metadata:
  type: reference
---

`A:\obd\build.ps1` (su `pc-030`, Windows) serve solo a verificare che il codice compili: non esporta binari e non carica
niente sulla board, come gli script analoghi di `A:\epd` e `A:\rd`.

**Uso:** senza parametri compila `CANbus_conn`; `-Sketch WIFI_conn` l'altro sketch, `-Clean` da zero, `-Dettagli` output
completo del compilatore, `-Avvisi` con `--warnings all`. Niente junction: ogni sketch sta già in una cartella con il nome
del suo `.ino`, come vuole arduino-cli.

**Cosa passa ad arduino-cli:**
- board `esp8266:esp8266:d1_mini_clone` con la stringa di opzioni di `.vscode\arduino.json` (xtal=160, mmu=3232, eesz=4M,
  FlashMode=dout e il resto), la stessa usata sul Mac;
- `build.extra_flags=-IA:\obd`, senza il quale nessuno sketch trova gli header condivisi nella radice; anche `BoostModel.h`
  e `TorqueModel.h` includono `EngineConstants.h` per nome e si affidano a questo `-I`;
- `--config-file C:/tmp/arduino/arduino-cli.yaml`, obbligatorio perchè il config della toolchain portable non sta nel
  percorso di default; è la stessa installazione di `A:\epd` e `A:\rd`;
- `--build-path A:\tmp\arduino-build\<sketch>` per l'incrementale, così gli oggetti restano su `A:` e non nella cache di
  `C:\tmp\arduino`.

**Core e librerie:** lo script controlla entrambi e si ferma stampando i comandi esatti. Il core ESP8266 3.1.2, la stessa
versione del Mac, è installato da un index filtrato con `xtensa-lx106-elf-gcc` e `python3` (memoria
`build_toolchain_arduino` di `A:\epd`). `mcp_can` (per `CANbus_conn`) e `U8g2` (per entrambi gli sketch, usata anche da
`light_sensor.h`; sul Mac la 2.35.30) non sono installate per decisione dell'utente, quindi `build.ps1` si ferma al
controllo delle librerie.

**Verifica senza librerie:** `C:\tmp\arduino\bin\arduino-cli.exe` con le board options di `build.ps1`, `--build-path` nello
scratchpad e `--library` verso due stub nello scratchpad (`mcp_can/src/mcp_can.h` e `U8g2/src/U8g2lib.h`, ciascuno con un
`library.properties`) che dichiarano solo ciò che il codice usa: `MCP_CAN`, `U8G2_SH1106_128X64_NONAME_F_HW_I2C`, i font
`u8g2_font_6x10_tr` e `u8g2_font_7x14B_tr`. Verifica tutto il progetto tranne le librerie e stampa l'occupazione di RAM, IRAM
e flash. Non c'è un compilatore host: il C++ da eseguire su PC (per esempio test di `obd_proto.h`) qui si controlla solo con
`xtensa-lx106-elf-g++ -std=gnu++17 -fsyntax-only` e si esegue sul Mac.
