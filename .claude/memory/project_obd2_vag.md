---
name: Limitazioni OBD2 VAG
description: Cosa VAG espone via OBD2 standard sulla B9.5 e cosa resta solo UDS; scelte dell'utente sui protocolli proprietari
metadata:
  type: project
---

VAG espone via OBD2 standard solo i PID Mode 01 legati alle emissioni; tutto il resto passa da UDS con label file VCDS/ODIS.
Sulla B9.5 il set standard del motore è però ampio (coppia `62`/`63`/`8E`, boost `70`, olio `5C`, portata carburante `5E`,
livello `2F`, contachilometri `A6`), mentre restano solo UDS: marcia (`22 210F` su `0x7E1`), batteria e BAS del mild hybrid,
temperature e adattamenti del cambio S tronic.

**Mode 03 su CAN:** la risposta ha il byte di conteggio dopo `0x43` (ISO 15765-4); multi-frame con flow control all'ID fisico
della centralina (risposta − 8).

**Scelta dell'utente:** nessuna comunicazione UDS proprietaria o sul CAN comfort; si usano solo PID e InfoType standard SAE.

**How to apply:** a "perchè non posso leggere X?" la risposta è quasi sempre "solo UDS, serve VCDS/OBDeleven"; non proporre
soluzioni UDS proprietarie. Vedi [[project_scope_obd2]] e [[project_vehicle]].
