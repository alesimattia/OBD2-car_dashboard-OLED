---
name: Come si compila su pc-030
description: build.ps1 di A:\obd - verifica di compilazione con la toolchain portable condivisa, board ESP8266 e opzioni, precondizioni (core e librerie) e cosa manca oggi su questa macchina
type: reference
---

**Su `pc-030` (Windows) esiste `A:\obd\build.ps1`**, creato il 25/08/2026 su richiesta
dell'utente, sullo stesso modello degli script di `A:\epd` e `A:\rd`.

**Serve solo a verificare che il codice sia corretto**: non esporta nessun binario e non
carica niente sulla board. È la stessa regola applicata agli altri due progetti, dove
`--output-dir` è stato tolto e la cartella `A:\tmp\arduino\out` rimossa. Gli oggetti
intermedi restano in `build\` dentro la cartella dello sketch e servono all'incrementale.

## Uso

```
A:\obd\build.ps1                      CANbus_conn (predefinito)
A:\obd\build.ps1 -Sketch WIFI_conn    l'altro sketch principale
A:\obd\build.ps1 -Sketch can_sniffer  oppure -Sketch canbox_sniffer, i tool
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
- **Toolchain**: `arduino-cli` portable in `A:\tmp\arduino`, con il config **non** nel
  percorso di default, quindi va sempre passato `--config-file A:/tmp/arduino/arduino-cli.yaml`.
  È la stessa installazione che usano `A:\epd` e `A:\rd`.

## Precondizioni non ancora soddisfatte su questa macchina

Lo script le controlla e si ferma stampando i comandi esatti, invece di lasciar fallire la
compilazione con errori poco parlanti:

1. **Il core ESP8266 non è installato.** Qui c'è solo l'ESP32 3.3.11. Servono circa 400 MB in
   `C:\xz\arduino\data` e l'URL aggiuntivo
   `http://arduino.esp8266.com/stable/package_esp8266com_index.json`, perchè il config locale
   ha un index filtrato per il solo ESP32. La versione dell'altra macchina è la **3.1.2**.
2. **Mancano `mcp_can` e `U8g2`**, che i due sketch principali includono. Sull'altra macchina
   U8g2 è la **2.35.30**. I tool in `tools\` non le usano.

L'utente ha deciso il 25/08/2026 di **non installarle ora**: lo script è quindi scritto e
verificato nella sintassi e nel controllo delle precondizioni, ma la compilazione vera non è
mai stata eseguita su questa macchina.

## Modifica fatta per renderlo possibile

`BoostModel.h` e `TorqueModel.h` includevano `EngineConstants.h` con un **path assoluto del
Mac** (`/Users/alesimattia/Documents/OBD2-car_dashboard-OLED/...`), che su Windows non esiste:
la compilazione sarebbe fallita sempre, qualunque flag. Ora è `#include "EngineConstants.h"`,
che il `-I<radice>` già presente risolve identico su entrambe le macchine. Toccata solo la riga
di include: nessuna formula, costante o soglia, quindi il vincolo di
[[feedback_helpers_readonly]] resta rispettato.
