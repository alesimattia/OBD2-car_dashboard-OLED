---
name: feedback_no_machine_config
description: Non installare nè configurare strumenti sulla workstation Windows; usare solo ciò che è presente e demandare build e verifiche all'utente
metadata:
  type: feedback
---

Sulla workstation Windows di sviluppo non installare e non configurare strumenti per conto dell'utente: niente
`arduino-cli` in PATH, toolchain, modifiche a PATH o variabili d'ambiente, `c_cpp_properties.json` o simili.

**Why:** richiesta esplicita dell'utente: la macchina è gestita altrove e non va configurata da Claude. Anche "verifico se X
c'è e poi lo installo" è fuori ambito.

**Eccezione concordata:** `A:\obd\build.ps1` usa la toolchain portable in `C:\tmp\arduino` senza installare niente e, se core
o librerie mancano, si ferma stampando i comandi. Il core esp8266 3.1.2 è installato lì su richiesta dell'utente; le librerie
`mcp_can` e `U8g2` non lo sono, per sua decisione. Dettagli in [[reference_build]].

**How to apply:** se uno strumento manca, segnalarlo in breve e demandare all'utente la verifica o la build; non modificare
configurazioni VSCode o di sistema senza richiesta esplicita.
