#include "telelink.hpp"
#include "events.hpp"
#include "pins.hpp"
#include "MySerial.hpp"
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

		MySerial<1024> serial("gsm");

		// current state
		enum State 
		{
			CLOSED = 0,
			SYNCING_1,
			SYNCING_2,
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
			}

			// ERROR? Reboot
			if (line=="ERROR") {
				//sendCommand("AT+CPOWD=0", CLOSED);
				logger.println("Restarting SIM868 module");
				sendCommand("AT+CFUN=1,1", SYNCING_1);
				return;
			}

			static int response_counter = 0;
			switch (state) {
				case CLOSED: {
					logger.println("CLOSED");
				} break;
				case SYNCING_1: {
					if (line=="OK")	sendCommand("AT", SYNCING_2);
				} break;
				case SYNCING_2: {
					if (line=="OK")	sendCommand("AT+CGREG=1", CONNECT_1); // "AT+IFC=2,2;+CGREG=1"
				} break;
				case CONNECT_1: {
					if (line=="OK") {
						sendCommand("AT+SAPBR=2,1", CONNECT_2); // AT+CSQ;+SAPBR=2,1
						response_counter = 0;
					}
				} break;
				case CONNECT_2: {
					static bool has_bearer_profile = false;
					if (line.startsWith("+SAPBR:")) {
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
							sendCommand("AT+CGREG?", CONNECT_4);
							response_counter=0;
						} else {
							sendCommand("AT+SAPBR=3,1,\"Contype\",\"GPRS\";+SAPBR=3,1,\"APN\",\"" APN "\";+SAPBR=1,1", CONNECT_3);
						}
					}
				} break;
				case CONNECT_3: {
					if (line=="OK") {
						sendCommand("AT+CGREG?", CONNECT_4);
						response_counter=0;
					}
				} break;
				case CONNECT_4: {
					if (line=="OK" || line.startsWith("+CGREG:")) response_counter++;
					if (response_counter==2) sendCommand("AT+IFC=2,2", CONNECT_5);
				} break;
				case CONNECT_5: {
					if (line=="OK") setState(IDLE);
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
		static auto& sync_chan = events::Channel<>::make("gsm_sync");

		// called once at init
		void setup()
		{
			serial.rxLineChan().subscribe([&](unsigned long time) {
				while (serial.hasLine()) {
					auto tmp = serial.popLine();
					logger.println(String("gsm_rx>") + tmp);
					callback(time, tmp);
				}
			});
			sync_chan.subscribe([&](unsigned long time){
				if (state==SYNCING_1) {
					serial.println("AT");
					sync_chan.postAt(time+200);
				}
			});
		}

		void begin()
		{
			serial.beginHandshaked();
			state = SYNCING_1;
			sync_chan.post();
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

	void send(char* data, unsigned long num_bytes)
	{
	}
	
}
