---
name: project_obd2_vag
description: Cosa VAG espone via OBD2 standard e via UDS sulla B9.5, regole UDS VAG (coppie di ID, discovery, flow control, DTC non decodificabili) e confine fra lettura e scrittura
metadata:
  type: project
---

VAG espone via OBD2 standard i PID Mode 01 legati alle emissioni; sulla B9.5 il set del motore è però ampio (coppia
`62`/`63`/`8E`, boost `70`, portata carburante `5E`/`9D`, marcia `A4`, livello `2F`, contachilometri `A6`; niente olio `5C`
nè MAF `10`). Il resto (BAS e batteria del mild hybrid, dati del cambio S-tronic) passa da UDS `22`, letto dalla scansione completa solo per i DID con
decodifica OBDb pubblica.

**UDS VAG:**
- Coppie richiesta/risposta: risposta = richiesta + `0x6A`, oppure + 8 per `7E0`-`7E7`; candidati della discovery `70A`-`773`
  e `7E0`-`7E7` (`700`-`709` non hanno coppie ODIS e a `700` rispondono 18 centraline insieme). Elenco delle coppie estratto da
  ODIS in `OBD_ODIS_ECUS` di `obd_scan.h` (github.com/ConnorHowell/vag-uds-ids, senza licenza: solo le coppie di ID, nomi
  italiani nostri), più `7E4/7EC` aggiunta perchè lo scan la conferma come la centralina freni di `713/77D` (89 righe).
- A 29 bit (`17FC00xx` → `17FE00xx`) sulla vettura rispondono 5 centraline: BCM2, fari LMS sx/dx, ricarica wireless, MQ-BMS
  (batteria 12 V Li-ion); OBDb non ha DID per nessuna, quindi solo identificazione.
- Discovery con `3E 00`, come dtcsweep; per gli ID ODIS muti un tentativo con `22 F197`. SFD non blocca le letture.
- Flow control in UDS all'ID di richiesta: l'ELM327 in modo 0 lo manda a risposta − 8, quindi servono `ATFCSH`, `ATFCSD300005`
  e `ATFCSM1`, in quest'ordine (altrimenti `?`).
- I DTC UDS VAG sono in formato ISO 14229-1 (`59 01 .. 01` sull'infotainment): 3 byte di numero del costruttore, non un codice
  SAE; VCDS ne mostra il numero in decimale, il byte di stato in binario fra parentesi e descrizione e codice SAE da una
  tabella VW. Sulla vettura `19 01` lo accetta solo l'infotainment `773`, le altre rispondono NRC `12`. Il firmware riporta
  solo i conteggi con una sola `19 02 0D`, contati dai byte di stato (`obdParseDtcStatusCounts`); ripieghi: lista oltre il
  buffer da 72 byte (più di 17 DTC) → una `19 02` per maschera contata dalla lunghezza dichiarata (`obdParseDtcListCount`),
  `19 02` rifiutato (NRC 12/31) → `19 01` per maschera. `pid_scan.ps1` usa una sola `19 02 AF` con numeri e stati
  ([[project_scope_obd2]]). I DTC OBD
  (Mode 03/07) coprono solo le emissioni: quelli di rete e carrozzeria che VCDS trova stanno solo in UDS.

**Mode 03 su CAN:** la risposta ha il byte di conteggio dopo `0x43` (ISO 15765-4).

**Confine:** sola lettura; il firmware resta nella default session, `pid_scan.ps1` apre anche la sessione estesa `10 03`
fuori dalle centraline di sicurezza; nessun servizio di scrittura, reset o sicurezza, niente CAN comfort.

**How to apply:** a "perchè non posso leggere X?" verificare prima se OBDb ha una decodifica del DID selezionabile per il
2022; se manca, resta un dato da VCDS/OBDeleven. Vedi [[project_scope_obd2]] e [[project_vehicle]].
