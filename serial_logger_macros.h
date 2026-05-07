/**
 * Helper di macro per redirigere Serial.* al wrapper TeeSerial.
 *
 * Includere questo header negli sketch e in web_dashboard.h: in questo
 * modo le chiamate Serial.print*(...) vengono dirottate sul wrapper
 * logSerial, che le inoltra alla UART e le accoda al buffer circolare
 * di serial_logger.h.
 *
 * IMPORTANTE: l'#include va posizionato DOPO tutti gli #include di
 * sistema che dichiarano "extern HardwareSerial Serial;" (Arduino.h e
 * tutti gli header che lo tirano dentro). Cosi' la dichiarazione di
 * Serial e' gia' stata fissata al tipo HardwareSerial prima che il
 * #define entri in vigore. Da quel punto in poi, ogni Serial.qualcosa
 * scritto nel firmware viene rimappato a logSerial.qualcosa.
 *
 * NON includere questo header nel file/blocco che istanzia logSerial:
 * il costruttore TeeSerial(Serial) verrebbe rimappato a
 * TeeSerial(logSerial) causando una ricorsione infinita. Negli sketch
 * la definizione e' racchiusa fra "#undef Serial" e
 * "#define Serial logSerial".
 *
 * @since 07/05/26 Mattia Alesi
 */
#ifndef SERIAL_LOGGER_MACROS_H
#define SERIAL_LOGGER_MACROS_H

#include "serial_logger.h"

extern TeeSerial logSerial;

#define Serial logSerial

#endif
