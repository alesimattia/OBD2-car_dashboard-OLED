---
name: reference_build
description: build.ps1 di A:\obd verifica la compilazione con la toolchain portable in C:\tmp\arduino; board e opzioni ESP8266, -I sulla radice, librerie non installate, verifica con librerie stub; test C++ su PC con MSVC e test PowerShell
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
e flash. Lo stub di `mcp_can` deve dichiarare `sendMsgBuf` con 3 e 4 argomenti, `readMsgBuf`, `checkReceive` e `getError`
con `INT32U` = unsigned long; per l'ESP-01 la board è `generic` con `eesz=1M` ([[project_hardware]]).

**Test su PC:** su `pc-030` c'è MSVC di Visual Studio 2022 Community, senza `vswhere` nel PATH: per compilare ed eseguire
C++ di verifica (per esempio prove di `obd_proto.h`, che compila senza Arduino) un `.bat` nello scratchpad chiama
`C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat` e poi `cl /std:c++17 /utf-8 /W4
/EHsc` (`/utf-8` serve per gli accenti dei sorgenti). Gli script PowerShell di verifica girano con `powershell -NoProfile
-ExecutionPolicy Bypass -File`. Tutti i test di verifica, C++ e PowerShell, restano nello scratchpad: l'utente non li esegue
e nel repository vuole solo file usati da lui o dal firmware.
