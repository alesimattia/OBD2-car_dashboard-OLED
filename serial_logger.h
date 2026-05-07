/**
 * Logger seriale circolare per ESP8266.
 *
 * Cattura tutto cio' che viene scritto su Serial in un buffer circolare
 * accessibile via HTTP (endpoint /serial-data della dashboard), in modo
 * da poter leggere il monitor seriale dal browser senza connessione USB.
 *
 * Funzionamento:
 * - TeeSerial e' un Print che inoltra ogni byte a una HardwareSerial reale
 *   E lo accoda al buffer circolare. Quando il buffer e' pieno, sovrascrive
 *   i byte piu' vecchi (comportamento tipico di un terminale seriale).
 * - Un sequence number monotonico (totale byte scritti dall'avvio) consente
 *   al client web di richiedere solo i byte nuovi tramite ?since=N.
 *
 * Va usato in combinazione con serial_logger_macros.h che fa
 * "#define Serial logSerial": cosi' le ~400 chiamate Serial.print* esistenti
 * non richiedono modifiche.
 *
 * Buffer:
 * - dimensione configurabile via #define SERIAL_LOG_BUFFER_SIZE (default 4096).
 * - allocazione statica (no heap), inizializzato a zero prima di setup(),
 *   quindi attivo gia' durante l'esecuzione dei costruttori globali.
 *
 * Usato da: web_dashboard.h (handler /serial-data), WIFI_conn.ino, CANbus_conn.ino
 *
 * @since 07/05/26 Mattia Alesi
 */

#ifndef SERIAL_LOGGER_H
#define SERIAL_LOGGER_H

#include <Arduino.h>

#ifndef SERIAL_LOG_BUFFER_SIZE
#define SERIAL_LOG_BUFFER_SIZE 4096
#endif

class TeeSerial : public Print {
public:
  explicit TeeSerial(HardwareSerial& hw) : _hw(hw) {}

  // Forward dei metodi di HardwareSerial usati dallo sketch
  void begin(unsigned long baud) { _hw.begin(baud); }
  void begin(unsigned long baud, SerialConfig cfg) { _hw.begin(baud, cfg); }
  void end() { _hw.end(); }
  int available() { return _hw.available(); }
  int read() { return _hw.read(); }
  int peek() { return _hw.peek(); }
  void flush() override { _hw.flush(); }
  void setDebugOutput(bool en) { _hw.setDebugOutput(en); }
  operator bool() const { return (bool)_hw; }

  // Override di Print: scrive sia sulla UART sia nel buffer circolare
  size_t write(uint8_t c) override {
    _append(c);
    return _hw.write(c);
  }
  size_t write(const uint8_t* buf, size_t n) override {
    for (size_t i = 0; i < n; i++) _append(buf[i]);
    return _hw.write(buf, n);
  }

  // ---- API per la dashboard ----

  // Numero totale di byte scritti su Serial dall'avvio (sequence number).
  static uint32_t logHeadSeq() { return _seq(); }

  // Capacita' del buffer circolare.
  static size_t logBufferSize() { return SERIAL_LOG_BUFFER_SIZE; }

  /**
   * Legge dal buffer i byte da @p sinceSeq fino al piu' recente disponibile.
   * Se @p sinceSeq e' troppo vecchio (gia' sovrascritto), parte dal piu'
   * vecchio byte ancora in buffer e segnala overflow tramite @p outDropped.
   *
   * @param sinceSeq    cursor del client (totale byte gia' visti)
   * @param dst         buffer di destinazione
   * @param maxBytes    capacita' massima di dst
   * @param outNextSeq  out: nuovo cursor da usare al prossimo fetch
   * @param outDropped  out: true se il client ha perso byte rispetto al ring
   * @return numero di byte copiati in dst
   *
   * @since 07/05/26 Mattia Alesi
   */
  static size_t logReadFrom(uint32_t sinceSeq, uint8_t* dst, size_t maxBytes,
                            uint32_t* outNextSeq, bool* outDropped) {
    // Snapshot atomico di head e copia: disabilita interrupt brevemente
    // per evitare che _append() modifichi _seq durante la lettura.
    noInterrupts();
    uint32_t head = _seq();
    interrupts();

    uint32_t oldest = (head > SERIAL_LOG_BUFFER_SIZE)
                        ? head - SERIAL_LOG_BUFFER_SIZE : 0;
    bool dropped = false;
    if (sinceSeq < oldest) { sinceSeq = oldest; dropped = true; }
    if (sinceSeq > head)   { sinceSeq = head; }

    size_t avail = (size_t)(head - sinceSeq);
    size_t toCopy = avail < maxBytes ? avail : maxBytes;

    uint8_t* b = _buffer();
    for (size_t i = 0; i < toCopy; i++) {
      // L'indice di un byte con sequenza S e' sempre (S % BUFSIZE):
      // _append scrive proprio in quella posizione e sovrascrive il
      // byte piu' vecchio con la stessa modulo-classe, mantenendo
      // l'invariante anche dopo il wrap.
      dst[i] = b[(sinceSeq + i) % SERIAL_LOG_BUFFER_SIZE];
    }

    if (outNextSeq) *outNextSeq = sinceSeq + toCopy;
    if (outDropped) *outDropped = dropped;
    return toCopy;
  }

private:
  HardwareSerial& _hw;

  // Storage statico tramite static-locals di funzioni inline:
  // garantisce singola istanza tra translation unit anche su C++11
  // (i .cpp di Arduino non offrono inline-variable C++17).
  static uint8_t* _buffer() {
    static uint8_t b[SERIAL_LOG_BUFFER_SIZE];
    return b;
  }
  static uint32_t& _seq() {
    static uint32_t s = 0;
    return s;
  }

  static inline void _append(uint8_t c) {
    uint32_t& s = _seq();
    _buffer()[s % SERIAL_LOG_BUFFER_SIZE] = c;
    s++;
  }
};

#endif  // SERIAL_LOGGER_H
