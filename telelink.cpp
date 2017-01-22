#include <functional>
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


	class Module
	{
		std::function<void(bool)> status_cb;

		bool isOn() { 
			return digitalRead(pins::SC_STATUS); 
		}
		
		enum Action
		{
			INITIAL,  // Need to turn on?
			ON_LOW,   // pull PWRKEY low
			ON_HIGH,  // pull PWRKEY high
			ON_POLL,  // poll STATUS high
			ON_UARTS, // open uarts
			ON_SYNC,  // sync gsm uart speed
			OFF_LOW,  // pull PWRKEY low
			OFF_HIGH_POLL, // pull PWRKEY high and poll STATUS
			AWAIT_RESTART  // poll STATUS low, then go to ON_POLL
		};

		events::Channel<Action, int> action_chan = events::Channel<Action, int>::make("telelink_module");

		void doActionIn(unsigned long time, Action action, int retries=0)
		{
			action_chan.postIn(time, action, retries);
		}

		void doAction(Action action, int retries=0)
		{
			static bool sync_got_at = false;
			static bool sync_got_ok = false;
			switch (action) {
			case INITIAL:
				if (isOn()) {
					logger.println("Module already on, go to sync");
					doAction(ON_UARTS);
				} else {
					logger.println("Module off, turn on");
					doAction(ON_LOW);
				}
				break;
			case ON_LOW:
				logger.println("Turning on");
				digitalWrite(pins::SC_PWRKEY, LOW);
				doActionIn(1001, ON_HIGH);
				break;
			case ON_HIGH:
				digitalWrite(pins::SC_PWRKEY, HIGH);
				doAction(ON_POLL, 22);
				break;
			case ON_POLL:
				if (isOn()) {
					logger.println();
					logger.println("SC_STATUS went high");
					doAction(ON_UARTS);
				} else if (retries>0) {
					logger.write('.');
					doActionIn(100, ON_POLL, retries-1);
				} else {
					logger.println();
					logger.println("Failed turning module on. Retrying.");
					doAction(ON_LOW);
				}
				break;
			case ON_UARTS:
				logger.println("Opening UARTS");
				gsm_uart.beginHandshaked();
				logger.println("Synchronizing baud");
				sync_got_at = false;
				sync_got_ok = false;
				doAction(ON_SYNC, 11);
				break;
			case ON_SYNC: {
				bool done = false;
				while (gsm_uart.hasLine()) {
					auto x = gsm_uart.popLine();
					if (x=="AT") sync_got_at = true;
					else if (sync_got_at && x=="OK") {
						sync_got_ok = true;
						break;
					}
				}
				if (sync_got_ok) {
					logger.println("Finished turning on module!");
					status_cb(true);
				} else if (retries>0) {
					gsm_uart.println("AT");
					doActionIn(100, ON_SYNC, retries-1);
				} else {
					logger.println("Could not sync GSM, only option is a hard restart");
					doAction(OFF_LOW);
				}
			} break;
			case OFF_LOW:
				logger.println("Turning module off");
				digitalWrite(pins::SC_PWRKEY, LOW);
				doActionIn(1501, OFF_HIGH_POLL, 11);
				break;
			case OFF_HIGH_POLL:
				digitalWrite(pins::SC_PWRKEY, HIGH);
				if (!isOn()) {
					doActionIn(800, ON_LOW);
				} else if (retries>0) {
					doActionIn(100, OFF_HIGH_POLL, retries-1);
				} else {
					logger.println("Failed turning module off. Retrying.");
					doAction(OFF_LOW);
				}
				break;
			case AWAIT_RESTART:
				if (!isOn()) {
					logger.println();
					logger.println("SC_STATUS went low");
					doActionIn(100, ON_POLL, 30);
				} else if (retries>0) {
					logger.write('.');
					doActionIn(100, AWAIT_RESTART, retries-1);
				} else {
					logger.println();
					logger.println("Module not restarting. Initiating hard restart.");
					doAction(OFF_LOW);
				}
				break;				
			default:
				assert(0);
			}
		}


	public:
		MySerial<1024> gsm_uart;

		Module() : gsm_uart("gsm_uart") {}

		// turn on
		void begin(std::function<void(bool)> status_cb)
		{
			this->status_cb = status_cb;
			action_chan.subscribe([&](unsigned long time, Action action, int retries) {
				this->doAction(action, retries);
			});
			pinMode(pins::SC_STATUS, INPUT);
			pinMode(pins::SC_PWRKEY, OUTPUT);
			digitalWrite(pins::SC_PWRKEY, HIGH);
			doActionIn(100, INITIAL);
		}

		// restart
		void restart() {
			logger.println("Restart initiated");
			status_cb(false);
			gsm_uart.println("AT+CFUN=1,1"); // "AT+CPOWD=1"
			doAction(AWAIT_RESTART, 10);
		}

		void hardRestart() {
			logger.println("Hard restart initiated");
			status_cb(false);
			doAction(OFF_LOW);
		}

	} module;
} // namespace {}

// ISR
void SERCOM2_Handler()
{
	module.gsm_uart.irqHandler();
}



namespace 
{

	// Logic for communicating over gsm
	class GsmClient 
	{

		// current state
		enum State 
		{
			INVALID = -1,
			IGNORE = 0,
			CONNECT_1,
			CONNECT_2,
			CONNECT_3,
			CONNECT_4,
			CONNECT_5,
			IDLE
		};

		bool module_power = false;
		bool online = false;
		State state = INVALID;

		static const unsigned long command_delay = 2000;

		void setState(State new_state)
		{
			static auto chan = events::Channel<State>::make("gsm_state").subscribe([this](unsigned long time, State new_state) {
				logger.println(String(new_state));
				state = new_state;
			});
			chan.postIn(command_delay, new_state);
			state = IGNORE;
			logger.println("[]");
		}

		void sendCommand(const String& command, State new_state, unsigned long timeout=10000)
		{
			static auto chan = events::Channel<String, State>::make("gsm_command").subscribe([this](unsigned long time, String command, State new_state) {
				logger.println(String(new_state));
				state = new_state;
				module.gsm_uart.println(command);
			});
			chan.postIn(command_delay, command, new_state);
			state = IGNORE;
			logger.println("[]");
		}

		void simulateReply(const String& reply, State new_state)
		{
			static auto chan = events::Channel<String, State>::make("gsm_command").subscribe([this](unsigned long time, String reply, State new_state) {
				logger.println(String(new_state));
				state = new_state;
				processLine(reply);
			});
			chan.postIn(command_delay, reply, new_state);
			state = IGNORE;
			logger.println("[]");
		}

		void powerOnBootstrap()
		{
			sendCommand("AT+IFC=2,2", CONNECT_1);
		}

		void processLine(const String& line)
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

			// ERROR? Restart module
			if (line=="ERROR") {
				logger.println("Error detected, restarting module");
				module.restart();
				return;
			}

			static int response_counter = 0;
			switch (state) {
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
						if (!has_bearer_profile) sendCommand("AT+SAPBR=3,1,\"Contype\",\"GPRS\";+SAPBR=3,1,\"APN\",\"" APN "\"", CONNECT_3);
						else simulateReply("OK", CONNECT_3);
					}
				} break;
				case CONNECT_3: {
					if (line=="OK") sendCommand("AT+SAPBR=1,1", CONNECT_4);
				} break;
				case CONNECT_4: {
					if (line=="OK") {
						sendCommand("AT+CGREG?;+CGATT?", CONNECT_5);
						response_counter=0;
					}
				} break;
				case CONNECT_5: {
					if (line=="OK" || line.startsWith("+CGREG:") || line.startsWith("+CGATT:")) response_counter++;
					if (response_counter==3) setState(IDLE);
				} break;
				case IDLE: {
					logger.println("IDLE");
				} break;
				case IGNORE: {
					logger.println("IGNORE");
				} break;
				case INVALID:
				default: {
					assert(!"Message received on gsm-rx while in an invalid state!");
					setState(CONNECT_1);
				}
			}
		}

	public:

		void setup()
		{
			module.gsm_uart.rxLineChan().subscribe([&](unsigned long time) {
				while (module_power && module.gsm_uart.hasLine()) {
					auto line = module.gsm_uart.popLine();
					processLine(line);
				}
			});
		}

		void modulePower(bool power)
		{
			this->module_power = power;
			if (power) powerOnBootstrap();
		}

	} gsm_client;

} // namespace {}




namespace telelink 
{
	void setup(
		void (*gpsPps)(), 
		void (*gpsData)(float latitude, float longitude, float elevation)
	)
	{	
		gsm_client.setup();

		module.begin(
			[&](bool power) { 
				gsm_client.modulePower(power);
			}
		);
	}

	void send(char* data, unsigned long num_bytes)
	{
	}
	
}
