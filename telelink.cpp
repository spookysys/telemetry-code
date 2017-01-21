#include "telelink.hpp"
#include "events.hpp"
#include "pins.hpp"
#include "wiring_private.h" // pinPeripheral() function

using namespace telelink;

// APN setup
#define APN "data.lyca-mobile.no"
//#define APN_USER "lmno"
//#define APN_PW "plus"


namespace {
	Stream& logger = Serial;


	// Logic for communicating over gsm
	namespace gsm {

		// Subscribe for a notification when num_newlines goes from 0 to 1
		auto& rx_chan = events::Channel<>::make("gsm_rx");

		// GSM Serial Link
		class Serial
		{
			static constexpr unsigned long fifo_depth = 1024; // must be pow-2
			static constexpr unsigned long baudrate = 9600;//19200;
			static constexpr uint8_t pin_rx = 3;  /*PA09 SERCOM2.1 RX<-GSM_TX*/
			static constexpr uint8_t pin_tx = 4;  /*PA08 SERCOM2.0 TX->GSM_RX*/
			static constexpr uint8_t pin_rts = 2; /*PA14 SERCOM2.2 RTS*/
			static constexpr uint8_t pin_cts = 5; /*PA15 SERCOM2.3 CTS*/
			static constexpr _EPioType pin_type_rx = PIO_SERCOM_ALT;
			static constexpr _EPioType pin_type_tx = PIO_SERCOM_ALT;
			static constexpr _EPioType pin_type_rts = PIO_DIGITAL;
			static constexpr _EPioType pin_type_cts = PIO_DIGITAL;
			static constexpr SercomRXPad pad_rx = SERCOM_RX_PAD_1;
			static constexpr SercomUartTXPad pad_tx = UART_TX_PAD_0;
			static constexpr SERCOM* sercom = &sercom2;

			std::array<char, fifo_depth> rx_fifo;
			int rx_push_idx = 0;
			int rx_pop_idx = 0;
			volatile bool rx_full = false;
			volatile uint16_t num_newlines = 0;
		public:

			// Open the link
			void begin()
			{
				pinPeripheral(pin_rx, pin_type_rx);
				pinPeripheral(pin_tx, pin_type_tx);
				pinPeripheral(pin_rts, pin_type_rts);
				pinPeripheral(pin_cts, pin_type_cts);
				sercom->initUART(UART_INT_CLOCK, SAMPLE_RATE_x16, baudrate);
				sercom->initFrame(UART_CHAR_SIZE_8_BITS, LSB_FIRST, SERCOM_NO_PARITY, SERCOM_STOP_BIT_1);
				sercom->initPads(pad_tx, pad_rx);
				sercom->enableUART();
				pinMode(pin_cts, INPUT);
				pinMode(pin_rts, OUTPUT);
				digitalWrite(pin_rts, 0);
			}
			
			// Handle bytes on the rx
			void irqHandler()
			{
				while (sercom->availableDataUART()) {
					char x = sercom->readDataUART();
					if (!rx_full) {
						rx_fifo[rx_push_idx] = x;
						rx_push_idx = (rx_push_idx + 1) & (fifo_depth - 1);
						if (rx_push_idx == fifo_depth) rx_push_idx = 0;
						rx_full = (rx_push_idx == rx_pop_idx);
						if (x=='\n' && num_newlines==0) rx_chan.post();
						if (x=='\n') num_newlines++;
					}
				}
				
				if (sercom->isUARTError()) {
					assert(!"gsm rx uart error");
					sercom->acknowledgeUARTError();
					sercom->clearStatusUART();
				}
			}

			void dump()
			{
				logger.println("Dump fifo");
				for (int i=0; i<fifo_depth; i++) {
					if (isprint(rx_fifo[i])) logger.write(rx_fifo[i]);
					else logger.print(String("\\") + String(rx_fifo[i], HEX));
				}
				logger.println(String() + "num_newlines: " + num_newlines);
				logger.println();
			}

			events::Process& dumper_proc = events::Process::make("gsm_rx_dumper").subscribe([this](unsigned long time, unsigned long delta) {
				//dump();
			}).setPeriod(10000);


			bool hasLine() 
			{
				return num_newlines > 0;
			}

			String popLine()
			{
				if (!hasLine()) {
					assert(0);
					return String();
				}

				// Interrupts off
				noInterrupts();

				// Remove string from fifo and find start/stop
				int start_idx = rx_pop_idx;
				while (rx_fifo[rx_pop_idx] != '\n' && rx_fifo[rx_pop_idx] != '\r') {
					rx_pop_idx = (rx_pop_idx + 1) & (fifo_depth - 1);
				}
				int stop_idx = rx_pop_idx;

				// Calculate length of string
				int length = (stop_idx - start_idx) & (fifo_depth - 1);
				if (rx_full) length = fifo_depth;
				
				// Copy and condition string to stack
				std::array<char, fifo_depth+1> buff;
				buff[length] = 0;
				if (start_idx < stop_idx) {
					memcpy(buff.data(), rx_fifo.data()+start_idx, length);
				} else if (length) {
					memcpy(buff.data(), rx_fifo.data()+start_idx, fifo_depth-start_idx);
					memcpy(buff.data()+fifo_depth-start_idx, rx_fifo.data(), stop_idx);
				}
				
				// Skip end-of-line-markers and decrement num_newlines
				if (rx_fifo[rx_pop_idx] == '\r') {
					rx_pop_idx = (rx_pop_idx + 1) & (fifo_depth - 1);
				}
				if (rx_fifo[rx_pop_idx] == '\n') {
					rx_pop_idx = (rx_pop_idx + 1) & (fifo_depth - 1);
					num_newlines--;
				} else assert(0);

				// fifo no longer full (if it was)
				if (rx_pop_idx != start_idx) rx_full = false;
				else assert(0);

				// Interrupts back on and return
				interrupts();
				return String(buff.data());
			}

			void waitForDataRegister()
			{
				for (int i=0; i<100; i++) {
					delay(1);
					if (sercom->isDataRegisterEmptyUART()) return;
				}
				assert(!"isDataRegisterEmptyUART stuck at high");
			}

			void println(const char* x=nullptr)
			{
				if (x) {
					logger.println(String("gsm_tx>") + x);
					for (; *x; x++) {
						waitForDataRegister();
						sercom->writeDataUART(*x);
					}
				} else logger.println("gsm_tx>");
				waitForDataRegister();
				sercom->writeDataUART('\n');
			}

			void println(const String& x)
			{
				println(x.c_str());
			}

		} serial;

		

		// current state
		enum State 
		{
			CLOSED = 0,
			SYNCING,
			CONNECT_1,
			CONNECT_2,
			CONNECT_3,
			CONNECT_4,
			CONNECT_5,
			IDLE,
			IGNORE
		};
		State state = CLOSED;
		bool online = false;


		static const unsigned long command_delay = 2000;

		void setState(State new_state)
		{
			static auto chan = events::Channel<State>::make("gsm_state").subscribe([&](unsigned long time, State new_state) {
				logger.println(String(new_state));
				state = new_state;
			});
			chan.postIn(command_delay, new_state);
			state = IGNORE;
			logger.println("[]");
		}

		void sendCommand(const char* command, State new_state, unsigned long timeout=10000)
		{
			static auto chan = events::Channel<const char*, State>::make("gsm_command").subscribe([&](unsigned long time, const char* command, State new_state) {
				logger.println(String(new_state));
				state = new_state;
				serial.println(command);
			});
			chan.postIn(command_delay, command, new_state);
			state = IGNORE;
			logger.println("[]");
		}


		void callback(unsigned long time, const String& line)
		{
			// online/offline
			if (line.startsWith("+CGREG:")) {
				// Interpret status
				int status = String(line[line.length()-1]).toInt();
				if (status==1 || status==5) {
					online = true;
					logger.println("Going online!");
				} else if (status==0 || status==2) {
					online = false;
					logger.println("Going offline!");
				} else assert(!"Unexpected network status");

				// Unsolicited? return!
				if (line.length() == 9) return;
				else assert(line.length()==11);
			}

			// Signal strength
			if (line.startsWith("+CSQ:")) {
				int signal_strength = line.substring(6, line.indexOf(',')).toInt();
				logger.println(String("Signal strength: ") + signal_strength);
				return;
			}

			// ERROR? Reboot
			if (line=="ERROR") {
				//sendCommand("AT+CPOWD=0", CLOSED);
				sendCommand("AT+CFUN=1,1", CLOSED);
			}
			static int response_counter = 0;
			switch (state) {
				case CLOSED: {
					logger.println("CLOSED");
				} break;
				case SYNCING: {
					if (line=="OK")	sendCommand("AT+IFC=2,2;+CGREG=0", CONNECT_1);
				} break;
				case CONNECT_1: {
					if (line=="OK") {
						sendCommand("AT+SAPBR=2,1", CONNECT_2); // AT+CSQ;+SAPBR=2,1
						response_counter = 0;
					}
				} break;
				case CONNECT_2: {
					static bool has_bearer_profile = false;
					if (line.startsWith("+SAPBR:")) { // Problem: this is coming after "OK"
						std::array<String, 2> toks;
						misc::tokenize(line, toks);
						has_bearer_profile = toks[1]=="1";
						logger.println(String("has_bearer_profile: ") + has_bearer_profile);
						response_counter++;
					} else if (line=="OK") {
						response_counter++;
					}
					if (response_counter==2) {
						if (has_bearer_profile) {
							sendCommand("AT+CGREG?;+CGREG=1", CONNECT_4);
							response_counter = 0;
						} else {
							sendCommand("AT+SAPBR=3,1,\"Contype\",\"GPRS\";+SAPBR=3,1,\"APN\",\"" APN "\";+SAPBR=1,1", CONNECT_3);
						}
					}
				} break;
				case CONNECT_3: {
					if (line=="OK") sendCommand("AT+CGREG?;+CGREG=1", CONNECT_4);
					response_counter = 0;
				} break;
				case CONNECT_4: {
					if (line=="OK" || line.startsWith("+CGREG:")) response_counter++;
					if (response_counter==2) sendCommand("AT+CFUN=1,1", SYNCING);
				} break;
				case IDLE: {
					logger.println("IDLE");
					
				} break;
				case IGNORE: {
					logger.println("IGNORE");
				} break;
				default: {
					assert(!"Message received on gsm-rx while in an invalid state!");
					setState(CONNECT_1);
				}
			}
		}

		// used for syncing with "AT" commands
		static auto& at_chan = events::Channel<>::make("gsm_at");

		// called once at init
		void setup()
		{
			rx_chan.subscribe([&](unsigned long time) {
				while (serial.hasLine()) {
					auto tmp = serial.popLine();
					logger.println(String("gsm_rx>") + tmp);
					callback(time, tmp);
				}
			});
			at_chan.subscribe([&](unsigned long time){
				if (state==SYNCING) {
					serial.println("AT");
					at_chan.postAt(time+300);
				}
			});
		}

		void begin()
		{
			serial.begin();
			state = SYNCING;
			at_chan.post();
		}


	} // namespace gsm {}

	// Logic for poweron, reboot, etc
	namespace life {

		enum State
		{
			INIT = 0,
			PWRKEY_LOW = 1,
			PWRKEY_HIGH = 2,
			AWAIT_STATUS = 3,
			TURNED_ON = 4
		};

		void callback(unsigned long time, State state, unsigned long timeout);

		auto& connect_channel = events::Channel<State, unsigned long>::make("telelink_life").subscribe(callback);

		void postIn(unsigned long in_time, State state, unsigned long timeout=0)
		{
			connect_channel.postIn(in_time, state, timeout);
		}

		bool isOn() 
		{
			return digitalRead(pins::SC_STATUS);
		}

		void callback(unsigned long time, State state, unsigned long timeout)
		{
			if (state>=TURNED_ON && !isOn()) {
				assert(!"No power on SimCom module");
				postIn(0, INIT);
				return;
			}

			switch(state) {
				case INIT: {
					logger.println("INIT: Initializing PWRKEY high");
					pinMode(pins::SC_STATUS, INPUT);
					pinMode(pins::SC_PWRKEY, OUTPUT);
					digitalWrite(pins::SC_PWRKEY, HIGH);
					postIn(500, PWRKEY_LOW);
				} break;
				case PWRKEY_LOW: {
					logger.println("PWRKEY_LOW");
					if (isOn()) {
						logger.println("Already turned on, skipping to state TURNED_ON");
						postIn(0, TURNED_ON);
					} else {
						digitalWrite(pins::SC_PWRKEY, LOW);
						postIn(1000, PWRKEY_HIGH);
					}
				} break;
				case PWRKEY_HIGH: {
					logger.println("PWRKEY_HIGH");
					digitalWrite(pins::SC_PWRKEY, HIGH);
					postIn(0, AWAIT_STATUS, time+2200);
				} break;
				case AWAIT_STATUS: {
					if (time < timeout && !isOn()) {
						logger.println("AWAIT_STATUS");
						postIn(100, AWAIT_STATUS, timeout);
					} else {
						postIn(0, TURNED_ON);
					}
				} break;
				case TURNED_ON: {
					logger.println("TURNED_ON - good to go!");
					gsm::begin();
				} break;
				default: {
					assert(0);
					postIn(0, INIT);
				}
			}
		}
	} // namespace life {}
} // namespace {}


void SERCOM2_Handler()
{
	gsm::serial.irqHandler();
}



namespace telelink 
{


	void setup(
		void (*gpsPps)(), 
		void (*gpsData)(float latitude, float longitude, float elevation)
	)
	{	
		logger.println("Connecting");
		gsm::setup();
		life::postIn(0, life::INIT);
	}
}
