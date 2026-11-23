---
name: reference_obd2_docs
description: Dove vivono le tabelle di verità su PID, Mode, DID, centraline e calcoli (README in tre parti con §8 sezione unica dei PID/DID reali per Mode, tools/obdb, obd_proto.h, obd_scan.h, pid_descriptions.h, DOCS) e regole di contenuto e di forma del README
metadata:
  type: reference
---

Per "quali PID espone l'A5", "cosa fa un Mode", "come si decodifica o si calcola una grandezza" consultare in ordine:

1. **README**, in tre parti. Uso (§1-6): avvio rapido, funzioni e varianti, hardware, build e configurazione, uso
   quotidiano (display, pulsante, dashboard con soglie e SoftAP, DTC, luminosità), scansione completa dal firmware (§6.1),
   da PC (§6.2) e profilo vettura (§6.3). Vettura (§7-8): §7 dati con fonti, rapporti, mild hybrid, centraline OBD (§7.4)
   e UDS con indirizzi e DTC VCDS (§7.5); **§8 PID e DID disponibili sulla vettura**, sezione unica per Mode (8.1 Mode 01
   con formule, valori al minimo e uso nel firmware, 8.2 Mode 02, 8.3 DTC OBD, 8.4 Mode 06, 8.5 Mode 09, 8.6-8.8 UDS `22`
   identificazione, `F4xx` e DID VAG con i valori incoerenti marcati ⚠️, 8.9 UDS `19`, 8.10 calcoli possibili).
   Riferimento tecnico (§9-16): architettura, lettura live e DTC (§10), calcoli derivati con il campo JSON (§11),
   scansione in dettaglio (§12: fasi, ricerca UDS, letture, dati interpretabili §12.4, failsafe, ELM327/STN, report, OBFCM,
   servizi e NRC §12.9), endpoint e schemi JSON (§13), auto-brightness, limiti, licenze e fonti (§16).
2. **`tools/obdb/`**: copie dei signalset `SAEJ1979.json` e `Audi-A5.json` e del supporto per anno delle catture A5 ai
   commit fissati in `tools/obdb_select.ps1` (CC-BY-SA-4.0), `selection.txt` con ogni segnale incluso o escluso e il
   motivo, `selection.json`. Le risposte reali con i valori attesi stanno nei casi di test di `OBDb/Audi-A5`
   (`tests/test_cases/<anno>`), da scaricare nello scratchpad quando serve verificare un interprete.
3. **`obd_proto.h`**: lunghezze SAE dei PID Mode 01 (`obdPidDataLen`), tabella UASID del Mode 06 (da python-OBD), decoder.
4. **`obd_scan.h`**: elenco ODIS delle coppie UDS VAG con i nomi italiani, nomi dei monitor Mode 06, TID, contatori IPT e DID
   di identificazione.
5. **`pid_descriptions.h`**: nomi italiani brevi (dashboard) ed estesi (Serial) dei PID Mode 01, 256 voci indicizzate per PID.
6. **`DOCS/`**: scheda eTD Audi, SSP 683 (`DMSA_engine.pdf`), autoscan VCDS, `references.lnk` con le fonti web (le pagine web
   si citano col link, senza copiarne il testo nel repo pubblico); i report della scansione completa, del firmware o di `pid_scan.ps1`,
   vanno salvati qui. Le bitmap dei Mode 02, 06 e 09 non sono nel corpo del report: si decodificano dalla traccia TX/RX in coda.

**Regole del README (scelte dell'utente):**
- contenuto: solo PID e DID che la vettura ha realmente restituito, mai cataloghi teorici SAE/ISO; i PID reali stanno nella
  sola §8, divisa per Mode, e le altre sezioni vi rimandano invece di ripetere elenchi;
- forma: un solo file (GitHub non supporta l'inclusione di un `.md` in un altro), ordinato per percorso di lettura in tre
  parti (uso, vettura, riferimento tecnico); avvisi GitHub `> [!NOTE]`/`[!TIP]`/`[!IMPORTANT]`/`[!WARNING]`/`[!CAUTION]`,
  tabelle lunghe e schemi di consultazione dentro `<details>`, diagrammi Mermaid, paragrafi brevi o elenchi, emoji solo sui
  titoli delle parti;
- a ogni rinumerazione vanno aggiornati i rimandi esterni: `obd_live.h`, commenti di `OTA_PASS` negli sketch,
  `tools/obdb_select.md` e le memorie.

Fonti esterne affidabili usate: signalset e decoder OBDb su GitHub (`OBDb/SAEJ1979`, `Audi-A5`, `Audi-A4`, `Audi-Q5`,
`.schemas`), python-OBD per UASID e OBDMID, `ConnorHowell/vag-uds-ids` per le coppie ODIS, dtcsweep per la discovery UDS VAG,
datasheet ELM327 (Elm Electronics) e STN1100, tabella Wikipedia "OBD-II PIDs", scheda eTD Audi, SSP 683, cartella stampa
Audi A5 2020 su audi.com.
