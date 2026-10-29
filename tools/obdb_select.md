# `obdb_select.ps1`

Genera le tabelle dei segnali che la scansione completa sa decodificare. Le usano il firmware e `pid_scan.ps1`.

## A cosa serve

Lo script parte dai signalset [OBDb](https://github.com/OBDb), le definizioni pubbliche per decodificare i PID OBD2
(`OBDb/SAEJ1979`) e i DID UDS dell'Audi A5 (`OBDb/Audi-A5`). Tiene solo i segnali realmente interpretabili per l'anno
modello scelto e scarta quelli opachi, in debug o non validi per quell'anno. I criteri sono nel README, §5.1.

Produce tre file:

| File | Chi lo usa |
|---|---|
| `obdb_signals.h` (radice del progetto) | il firmware, compilato insieme agli sketch |
| `tools/obdb/selection.json` | `pid_scan.ps1`, che lo legge all'avvio |
| `tools/obdb/selection.txt` | tu: elenco dei segnali inclusi ed esclusi, con il motivo |

I file sono già generati e presenti nel progetto. Non va lanciato prima di compilare il firmware nè prima di
`pid_scan.ps1`, che non lo richiama.

## Quando lanciarlo

Solo se cambia qualcosa nella selezione:

- vuoi un altro anno modello;
- hai cambiato le regole di selezione nello script, per esempio per includere un segnale escluso;
- vuoi aggiornare i dati OBDb a una versione più recente, cambiando i commit fissati in testa allo script.

## Come si usa

Da PowerShell, nella radice del progetto:

```powershell
.\tools\obdb_select.ps1                    # rigenera dai file già scaricati in tools\obdb, anno 2022
.\tools\obdb_select.ps1 -ModelYear 2023    # rigenera per un altro anno modello
.\tools\obdb_select.ps1 -Download          # riscarica i dati OBDb ai commit fissati, poi rigenera
```

Se la policy di esecuzione blocca gli script locali:
`powershell -ExecutionPolicy Bypass -File .\tools\obdb_select.ps1`.

`-Download` richiede la connessione a internet. Senza `-Download` lo script lavora offline sui file di `tools\obdb`.

Dopo l'esecuzione:

1. rileggi `tools/obdb/selection.txt` per controllare cosa è entrato e cosa no;
2. ricompila e carica il firmware;
3. `pid_scan.ps1` usa la nuova selezione da solo, al lancio successivo.

`obdb_signals.h` non va mai modificato a mano: ogni modifica si perde alla rigenerazione successiva.

## Licenza dei dati

I dati OBDb sono distribuiti con licenza CC-BY-SA-4.0: l'attribuzione è in `tools/obdb/README.md`.
