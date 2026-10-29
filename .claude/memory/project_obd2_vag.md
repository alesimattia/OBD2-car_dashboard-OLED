---
name: project_obd2_vag
description: Cosa VAG espone via OBD2 standard e via UDS sulla B9.5, regole UDS VAG (coppie di ID, discovery, flow control, DTC non decodificabili) e confine fra lettura e scrittura
metadata:
  type: project
---

VAG espone via OBD2 standard i PID Mode 01 legati alle emissioni; sulla B9.5 il set del motore è però ampio (coppia
`62`/`63`/`8E`, boost `70`, olio `5C`, portata carburante `5E`, livello `2F`, contachilometri `A6`). Il resto (marcia, BAS e
batteria del mild hybrid, dati del cambio S-tronic) passa da UDS `22`, letto dalla scansione completa solo per i DID con
decodifica OBDb pubblica.

**UDS VAG:**
- Coppie richiesta/risposta: risposta = richiesta + `0x6A`, oppure + 8 per `7E0`-`7E7`; richieste nello spazio `700`-`773`.
  Elenco delle coppie estratto da ODIS in `OBD_ODIS_ECUS` di `obd_scan.h` (github.com/ConnorHowell/vag-uds-ids, senza
  licenza: nel progetto solo le coppie di ID, nomi italiani nostri).
- A 29 bit (`17FC00xx` → `17FE00xx`) risponde solo la batteria alta tensione di PHEV e BEV: sulla A5 non c'è.
- Discovery con `3E 00`, come dtcsweep; per gli ID ODIS muti un tentativo con `22 F197`. SFD non blocca le letture.
- Flow control in UDS all'ID di richiesta: l'ELM327 in modo 0 lo manda a risposta − 8, quindi servono `ATFCSH`, `ATFCSD300005`
  e `ATFCSM1`, in quest'ordine (altrimenti `?`).
- I DTC UDS VAG non seguono la codifica SAE (VCDS li mostra da una tabella VW): si riportano solo i conteggi con `19 01`.

**Mode 03 su CAN:** la risposta ha il byte di conteggio dopo `0x43` (ISO 15765-4).

**Confine:** sola lettura nella default session; nessun servizio di scrittura, sessione o sicurezza, niente CAN comfort.

**How to apply:** a "perchè non posso leggere X?" verificare prima se OBDb ha una decodifica del DID selezionabile per il
2022; se manca, resta un dato da VCDS/OBDeleven. Vedi [[project_scope_obd2]] e [[project_vehicle]].
