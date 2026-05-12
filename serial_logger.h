/**
 * Logger seriale circolare per ESP8266.
 *
 * Cattura tutto cio' che viene scritto su Serial in un buffer circolare
 * accessibile via HTTP (endpoint /serial-data della dashboard), in modo
 * da poter leggere il monitor seriale dal browser senza connessione USB.
 *
 * Funzionamento:
 * - WebSerial e' un Print che inoltra ogni byte a una HardwareSerial reale
 *   E lo accoda al buffer circolare. Quando il buffer e' pieno, sovrascrive
 *   i byte piu' vecchi (comportamento tipico di un terminale seriale).
 * - Un sequence number monotonico (totale byte scritti dall'avvio) consente
 *   al client web di richiedere solo i byte nuovi tramite ?since=N.
 *
 * Uso:
 * - Includere questo header DOPO tutti gli #include di sistema (Arduino.h e
 *   header che dichiarano "extern HardwareSerial Serial;"). Da quel punto
 *   in poi, ogni Serial.print*(...) viene catturata automaticamente:
 *   non serve modificare le ~400 chiamate esistenti, ne' istanziare nulla
 *   negli .ino. L'istanza unica e' un singleton static-local definito
 *   nell'header tramite funzione inline (singola tra translation unit).
 *
 * Opt-out: definire WEB_SERIAL_NO_REDEFINE prima dell'#include per usare
 * solo la classe senza rimappare Serial (caso d'uso raro, es. test).
 *
 * Buffer:
 * - dimensione configurabile via #define SERIAL_LOG_BUFFER_SIZE (default 4096).
 * - allocazione statica (no heap), inizializzato a zero prima di setup(),
 *   quindi attivo gia' durante l'esecuzione dei costruttori globali.
 *
 * Usato da: web_dashboard.h (handler /serial-data), WIFI_conn.ino, CANbus_conn.ino
 *
 */

#ifndef SERIAL_LOGGER_H
	#define SERIAL_LOGGER_H

	#include <Arduino.h>

	#ifndef SERIAL_LOG_BUFFER_SIZE
		#define SERIAL_LOG_BUFFER_SIZE 4096
	#endif

	class WebSerial : public Print {
	public:
		explicit WebSerial(HardwareSerial& hw) : _hw(hw) {}

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
		// Override di Print: scrive sia sulla UART (Arduino IDE Serial
		// monitor via USB) sia nel buffer circolare letto dalla tab
		// "Serial monitor" della dashboard web. I due canali sono
		// indipendenti: il client web puo' aprire/chiudere la dashboard
		// senza alterare l'output USB.
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

	/**
	 * Singleton del wrapper. Flusso:
	 *
	 *  1. Ogni .ino e ogni .h che include questo file e' una translation
	 *     unit (TU) separata. La parola chiave `inline` su questa funzione
	 *     dice al linker di deduplicare i simboli: anche se l'header e'
	 *     incluso da WIFI_conn.ino e da CANbus_conn.ino (in build distinte)
	 *     o, in una stessa build, da piu' .cpp/.ino, esiste UNA sola
	 *     definizione di __webSerialInstance() nel binario finale, e quindi
	 *     UN solo static-local `s` condiviso. Sarebbe il comportamento di
	 *     una variabile inline C++17, qui ottenuto in modo C++11-compatibile.
	 *
	 *  2. Lo static-local NON e' inizializzato al boot insieme alle altre
	 *     globali: viene costruito in modo lazy alla PRIMA invocazione di
	 *     __webSerialInstance(). La prima invocazione avviene quando un
	 *     qualunque pezzo di codice scrive `Serial.x` (ora rimappato a
	 *     `__webSerialInstance().x`). Per il firmware questo accade al
	 *     piu' tardi dentro setup(), quando il core ESP8266 ha gia'
	 *     completato i costruttori globali — `Serial` (HardwareSerial)
	 *     e' quindi sicuramente costruita e referenziabile.
	 *
	 *  3. La posizione di questa funzione nel file e' critica: deve venire
	 *     PRIMA del `#define Serial __webSerialInstance()` sotto. Il
	 *     preprocessore C lavora in modo testuale e in ordine: se il
	 *     #define fosse gia' attivo qui, la riga `static WebSerial s(Serial)`
	 *     verrebbe espansa in `static WebSerial s(__webSerialInstance())`.
	 *     Risultato: il costruttore chiama se stesso → ricorsione infinita
	 *     → stack overflow al primo Serial.print.
	 */
	inline WebSerial &__webSerialInstance(){
		static WebSerial s(Serial);
		return s;
	}

	/** Da questo punto in poi ogni "Serial.qualcosa" 
	 * nel firmware diventa "__webSerialInstance().qualcosa". 
	 * Le librerie precompilate non sono affette (linkano contro il simbolo Serial reale) */
	#ifndef WEB_SERIAL_NO_REDEFINE
		#define Serial __webSerialInstance()
	#endif

#endif  // SERIAL_LOGGER_H
