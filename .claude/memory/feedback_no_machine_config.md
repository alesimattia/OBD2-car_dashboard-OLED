---
name: Non configurare la macchina di sviluppo
description: Non installare né configurare strumenti sulla workstation Windows; usare solo ciò che è già presente e demandare build/verifiche all'utente
type: feedback
---

Su questa macchina di sviluppo (Windows) non installare e non configurare strumenti per conto dell'utente:
niente `arduino-cli`, niente toolchain, niente modifiche a PATH/env, niente `c_cpp_properties.json` o simili.

**Why:** dichiarato esplicitamente dall'utente — la workstation è una macchina di sviluppo gestita altrove,
non va configurata da Claude. Anche il "verifico se X è disponibile e poi lo installo" è considerato fuori scope.

**Aggiornamento del 25/08/2026:** l'utente ha chiesto uno script di verifica, quindi ora esiste
`A:\obd\build.ps1`, che usa la toolchain portable già presente in `A:\tmp\arduino`
**senza installare niente**: se il core ESP8266 o le librerie mancano si ferma e stampa i comandi.
Il core non è stato installato, sempre per decisione dell'utente. Dettagli in [[reference_build]].

**How to apply:** se uno strumento manca in PATH (es. `arduino-cli`, compilatori, formatter), non proporre
l'installazione né procedere a configurarlo. Limitarsi a segnalare brevemente che il tool non è disponibile e
demandare all'utente la verifica/build manuale. Non modificare file di configurazione VSCode o di sistema
senza richiesta esplicita.
