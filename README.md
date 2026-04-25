# OBD2-car_dashboard-OLED
Car engine dashboard with ESP8266 wifi/CAN sniffer on i2c OLED display

## Pulsante fisico

Pulsante NA (normalmente aperto) verso GND, gestito con `INPUT_PULLUP` e debounce software (50 ms).

| Sketch | Pin | GPIO |
|---|---|---|
| `CANbus_conn` | **D3** | GPIO0 |
| `WIFI_conn`   | **D5** | GPIO14 (D3/D4 occupati dall'I2C OLED) |

### Funzioni

- **Click breve** → scorre ciclicamente le schermate del display: Monitor → DTC pag.1 → DTC pag.2 → … → Monitor.
- **Pressione lunga (≥ 2.5 s)** → invia OBD2 Service 04 (Clear DTC) e cancella i codici di errore della centralina.

> ⚠️ Cancellare i DTC perde lo storico errori e resetta i monitor di readiness della centralina.

La stessa cancellazione è disponibile dal pulsante **CANCELLA** nel web dashboard (`/dashboard`, endpoint `/clear-dtc`).
