---
name: feedback_data_must_be_interpretable
description: Leggere e proporre solo dati con decodifica pubblica e verificata (regole OBDb della selezione), niente valori grezzi; DTC UDS VAG solo come conteggi; in dashboard descrizioni testuali, mai indirizzi hex
metadata:
  type: feedback
---

Si leggono e si propongono solo dati con decodifica e significato fisico da fonte pubblica e verificata: SAE J1979,
ISO 14229, signalset OBDb validati sulle catture reali. Mai byte grezzi nei report, nè valori di sensori dichiarati non
supportati.

**Why:** richiesta ripetuta dell'utente ("solo pid realmente interpretabili"); esclusi per questo il brute force dei DID,
i DID VAG senza decodifica pubblica, i TID Mode 06 non standard e gli indirizzi dei PID in dashboard.

**Regole della selezione OBDb** (applicate da `tools/obdb_select.ps1`, firmware e `pid_scan.ps1` usano la stessa):
- il segnale ha un'unità fisica, una mappa di valori testuali o è booleano; non `unknown`, `hex`, `normal`, nè con "raw";
- uno `scalar` VAG passa solo se il nome contiene come parola intera count, counter, number, misfire, ratio, factor, lambda,
  gear o satellite, e non status, mode o state;
- il comando non richiede `din` e vale per il 2022, non in debug (`dbg` o `dbgfilter` che include il 2022);
- un valore si mostra solo se il suo bit di supporto vale sì; i monitor C/D dei PID `01`/`41` solo con accensione comandata;
- i PID con scala da `4F`/`50` si escludono se quei PID dichiarano massimi non predefiniti.
- Polarità dei booleani: la prima parola dell'unità OBDb (`noyes`, `yesno`, `offon`) è il valore del bit a 0.

**DTC UDS VAG:** non si decodificano dai byte, perchè VCDS li mostra da una tabella VW proprietaria: solo conteggi per stato.
**Mode 06:** nome del monitor ed esito per i MID standard; valore e limiti solo per i TID `01`-`0C`.

**How to apply:** prima di proporre un PID, DID o TID verificare formula, unità e significato pubblici; se è interpretabile
solo in parte dichiararlo e aspettarsi un rifiuto. In dashboard descrizioni italiane da `pid_descriptions.h` e nomi delle
centraline, mai indirizzi esadecimali. Vedi [[project_scope_obd2]].
