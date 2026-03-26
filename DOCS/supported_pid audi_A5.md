# PID OBD-II supportati

Questi PID appartengono alla modalità **01** di OBD-II (*Show current data*).  
Di seguito trovi la descrizione di ciascun PID supportato dalla tua ECU, con formula di conversione e unità quando applicabili.

## Elenco PID

### `0x01` — Monitor status since DTCs cleared
Restituisce uno stato codificato a bit. Indica:
- se la spia **MIL** è accesa;
- quanti **DTC** sono presenti;
- lo stato dei monitor diagnostici da quando gli errori sono stati cancellati.

> Non è una misura diretta: va decodificato bit per bit.

### `0x04` — Calculated engine load
Carico motore calcolato.

- **Formula:** `A × 100 / 255`
- **Unità:** `%`

### `0x05` — Engine coolant temperature
Temperatura del liquido di raffreddamento motore.

- **Formula:** `A - 40`
- **Unità:** `°C`

### `0x0B` — Intake manifold absolute pressure
Pressione assoluta nel collettore di aspirazione (**MAP**).

- **Formula:** `A`
- **Unità:** `kPa`

### `0x0C` — Engine speed
Regime motore.

- **Formula:** `(256 × A + B) / 4`
- **Unità:** `rpm`

### `0x0D` — Vehicle speed
Velocità veicolo.

- **Formula:** `A`
- **Unità:** `km/h`

### `0x0F` — Intake air temperature
Temperatura aria aspirata.

- **Formula:** `A - 40`
- **Unità:** `°C`

### `0x10` — MAF air flow rate
Portata aria misurata dal sensore **MAF**.

- **Formula:** `(256 × A + B) / 100`
- **Unità:** `g/s`

### `0x13` — Oxygen sensors present (in 2 banks)
Indica quali sensori ossigeno/lambda sono presenti nei banchi 1 e 2.

> È un valore codificato a bit: ogni bit segnala la presenza di un sensore.

### `0x1C` — OBD standards this vehicle conforms to
Indica lo standard OBD a cui il veicolo è conforme.

> È un valore enumerato, ad esempio può identificare OBD-II, EOBD, JOBD, ecc.

### `0x1F` — Run time since engine start
Tempo di funzionamento del motore dall’avviamento.

- **Formula:** `256 × A + B`
- **Unità:** `s`

### `0x20` — PIDs supported [0x21–0x40]
Bitmap dei PID supportati nel blocco successivo, cioè da `0x21` a `0x40`.

> Non è una misura fisica.

### `0x21` — Distance traveled with MIL on
Distanza percorsa con spia **MIL** accesa.

- **Formula:** `256 × A + B`
- **Unità:** `km`

### `0x23` — Fuel rail gauge pressure
Pressione rail carburante relativa/di linea.

- **Formula:** `10 × (256 × A + B)`
- **Unità:** `kPa`

### `0x24` — Oxygen Sensor 1 (wide range)
Dato del sensore lambda 1 in formato wide range. Restituisce:
- rapporto aria/carburante equivalente (**lambda**);
- tensione del sensore.

- **Formula lambda:** `2 × (256 × A + B) / 65536`
- **Formula tensione:** `8 × (256 × C + D) / 65536`
- **Unità:** `lambda`, `V`

> Questo PID usa 4 byte e contiene due informazioni.

### `0x2C` — Commanded EGR
Comando EGR richiesto dalla ECU.

- **Formula:** `A × 100 / 255`
- **Unità:** `%`

### `0x2D` — EGR Error
Errore EGR, cioè differenza tra valore atteso e reale.

- **Formula:** `A × 100 / 128 - 100`
- **Unità:** `%`

### `0x30` — Warm-ups since codes cleared
Numero di cicli di riscaldamento da quando i codici errore sono stati cancellati.

- **Formula:** `A`

### `0x31` — Distance traveled since codes cleared
Distanza percorsa da quando sono stati cancellati i DTC.

- **Formula:** `256 × A + B`
- **Unità:** `km`

### `0x33` — Absolute barometric pressure
Pressione barometrica assoluta.

- **Formula:** `A`
- **Unità:** `kPa`

### `0x40` — PIDs supported [0x41–0x60]
Bitmap dei PID supportati nel blocco `0x41–0x60`.

> Non è una misura fisica.

### `0x41` — Monitor status this drive cycle
Stato dei monitor diagnostici nel ciclo di guida corrente.

> Come `0x01`, è un valore codificato a bit.

### `0x42` — Control module voltage
Tensione della centralina/modulo di controllo.

- **Formula:** `(256 × A + B) / 1000`
- **Unità:** `V`

### `0x46` — Ambient air temperature
Temperatura aria ambiente esterna.

- **Formula:** `A - 40`
- **Unità:** `°C`

### `0x49` — Accelerator pedal position D
Posizione pedale acceleratore, canale **D**.

- **Formula:** `A × 100 / 255`
- **Unità:** `%`

### `0x4A` — Accelerator pedal position E
Posizione pedale acceleratore, canale **E**.

- **Formula:** `A × 100 / 255`
- **Unità:** `%`

### `0x4C` — Commanded throttle actuator
Comando attuatore farfalla.

- **Formula:** `A × 100 / 255`
- **Unità:** `%`

### `0x4F` — Maximum values for lambda / O2 / current / MAP
PID speciale che fornisce i valori massimi rappresentabili/usati per alcune misure:

- `A` = massimo lambda
- `B` = massima tensione sensore O2 (`V`)
- `C` = massima corrente sensore O2 (`mA`)
- `D × 10` = massima pressione MAP (`kPa`)

> Non descrive lo stato istantaneo del motore, ma i limiti/scaling di alcune grandezze.

## Note finali

Dal tuo elenco si vede che la ECU espone:
- parametri base motore;
- parametri emissioni;
- alcuni parametri avanzati come pressione rail, tensione ECU, posizione acceleratore e farfalla elettronica.

## Riferimento
- SAE J1979 / elenco PID OBD-II standard
- https://en.wikipedia.org/wiki/OBD-II_PIDs
