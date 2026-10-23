---
name: Come si compila su pc-030
description: build.ps1 di A:\obd - verifica di compilazione con la toolchain portable condivisa in C:\tmp\arduino, board ESP8266 e opzioni, core esp8266 3.1.2 filtrato installato, librerie mcp_can e U8g2 mancanti
type: reference
---

**Su `pc-030` (Windows) esiste `A:\obd\build.ps1`**, creato il 25/08/2026 su richiesta
dell'utente, sullo stesso modello degli script di `A:\epd` e `A:\rd`.

**Serve solo a verificare che il codice sia corretto**: non esporta nessun binario e non
carica niente sulla board, come gli script degli altri due progetti, che non passano
`--output-dir`.

## Uso

```
A:\obd\build.ps1                      CANbus_conn (predefinito)
A:\obd\build.ps1 -Sketch WIFI_conn    l'altro sketch principale
A:\obd\build.ps1 -Clean               da zero
A:\obd\build.ps1 -Dettagli            output completo del compilatore
A:\obd\build.ps1 -Avvisi              con --warnings all
```

Niente junction, a differenza degli altri due progetti: qui ogni sketch sta già in una
cartella che si chiama come il suo `.ino`, che è ciò che arduino-cli pretende.

## Cosa passa allo strumento

- **Board**: `esp8266:esp8266:d1_mini_clone` più la stringa di opzioni presa da
  `.vscode\arduino.json`, quella con cui il progetto viene compilato sull'altra macchina
  (xtal=160, mmu=3232, eesz=4M, FlashMode=dout e il resto).
- **`build.extra_flags=-IA:\obd`**: gli header condivisi stanno nella radice del progetto e
  non accanto agli sketch, quindi senza questo `-I` nessuno dei due sketch principali compila.
- **Toolchain**: `arduino-cli` portable in `C:\tmp\arduino`, con il config **non** nel
  percorso di default, quindi va sempre passato `--config-file C:/tmp/arduino/arduino-cli.yaml`.
  È la stessa installazione che usano `A:\epd` e `A:\rd`.
- **`--build-path A:\tmp\arduino-build\<sketch>`**: gli oggetti intermedi stanno lì e servono
  all'incrementale. Senza, arduino-cli li metterebbe nella build cache su `C:\tmp\arduino\cache`,
  fuori dal vincolo "output su `A:`".

## Core e librerie

Lo script controlla core e librerie e si ferma stampando i comandi esatti, invece di lasciar
fallire la compilazione con errori poco parlanti.

1. **Core ESP8266 3.1.2 installato**, la stessa versione dell'altra macchina, da un index
   filtrato che tiene la sola 3.1.2 con `xtensa-lx106-elf-gcc` e `python3`: dettaglio nella
   memoria `build_toolchain_arduino` di `A:\epd`.
2. **Mancano `mcp_can` e `U8g2`**, non installate per decisione dell'utente. Lo script tiene la
   tabella delle librerie per sketch: `mcp_can` per `CANbus_conn`, `U8g2` per i due sketch, dove
   arriva anche da `light_sensor.h`. Sull'altra macchina U8g2 è la **2.35.30**. Finchè mancano,
   `CANbus_conn` e `WIFI_conn` si fermano sul controllo e non si compilano con `build.ps1`.

## Verifica di compilazione senza le librerie

Funziona invocando direttamente `C:\tmp\arduino\bin\arduino-cli.exe` con le stesse board options
di `build.ps1`, `--build-path` nello scratchpad e `--library` verso due librerie stub create nello
scratchpad (`mcp_can/src/mcp_can.h` e `U8g2/src/U8g2lib.h`, ciascuna con un `library.properties`),
che dichiarano solo le classi e i metodi usati dal codice (`MCP_CAN`, `U8G2_SH1106_128X64_NONAME_F_HW_I2C`,
font `u8g2_font_6x10_tr` e `u8g2_font_7x14B_tr`). Nessuna installazione: verifica tutto il codice del
progetto, librerie escluse, e stampa l'occupazione di RAM, IRAM e flash. Sulla macchina non c'è un
compilatore host: codice C++ da eseguire su PC (per esempio test di `obd_proto.h`) qui si controlla
solo con `xtensa-lx106-elf-g++ -std=gnu++17 -fsyntax-only` e si esegue sul Mac.

## Modifica fatta per renderlo possibile

`BoostModel.h` e `TorqueModel.h` includevano `EngineConstants.h` con un **path assoluto del
Mac** (`/Users/alesimattia/Documents/OBD2-car_dashboard-OLED/...`), che su Windows non esiste:
la compilazione sarebbe fallita sempre, qualunque flag. Ora è `#include "EngineConstants.h"`,
che il `-I<radice>` già presente risolve identico su entrambe le macchine. Toccata solo la riga
di include: nessuna formula, costante o soglia, quindi il vincolo di
[[feedback_helpers_readonly]] resta rispettato.
