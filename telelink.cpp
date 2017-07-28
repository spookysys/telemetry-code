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

#define SERVER_IP "telemetry-app-156617.appspot.com"
#define SERVER_PORT 8080

namespace {
	Stream& logger = Serial;

	// Encapsulate power-on/-off and other low-level stuff for sim868 module
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
			ON_SYNC,  // sync gsm baud
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
					auto tmp = gsm_uart.popLine();
					if (tmp=="AT") sync_got_at = true;
					else if (sync_got_at && tmp=="OK") {
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
					assert(0);
					logger.println("Could not sync GSM, only option is a hard restart");
					hardRestart();
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
					assert(0);
					logger.println("Failed turning module off. Retrying.");
					hardRestart();
				}
				break;
			case AWAIT_RESTART:
				while (gsm_uart.hasLine()) gsm_uart.popLine();
				if (!isOn()) {
					logger.println();
					logger.println("SC_STATUS went low, closing UARTs");
					gsm_uart.end();
					doActionIn(100, ON_POLL, 30);
				} else if (retries>0) {
					logger.write('.');
					doActionIn(100, AWAIT_RESTART, retries-1);
				} else {
					assert(0);
					logger.println();
					logger.println("Module not restarting. Initiating hard restart.");
					hardRestart();
				}
				break;				
			default:
				assert(0);
				logger.println("Invalid action");
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

		// Restart
		void restart() {
			logger.println("Restart initiated");
			status_cb(false);
			gsm_uart.println("AT+CFUN=1,1"); // "AT+CPOWD=1"
			doAction(AWAIT_RESTART, 10);
		}

		// Hard restart
		void hardRestart() {
			logger.println("Hard restart initiated");
			status_cb(false);
			gsm_uart.end();
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
	class GsmConnector 
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
			CONNECT_6,
			CONNECT_7,
			CONNECT_8,
			CONNECT_9,
			CONNECT_10,
			CONNECTED
		};

		bool alse;
		bool online = false;
		State state = INVALID;


		// set to 2000 for "debug mode"
		static const unsigned long debug_delay = 0;

		void setState(State new_state)
		{
			if (debug_delay) setStateIn(0, new_state);
			else {
				logger.println(String("[")+new_state+"]");
				state = new_state;
			}
		}

		void setStateIn(unsigned long time, State new_state)
		{
			time += debug_delay;
			static auto chan = events::Channel<State>::make("gsm_state").subscribe([this](unsigned long time, State new_state) {
				logger.println(String("[")+new_state+"]");
				state = new_state;
			});
			chan.postIn(time, new_state);
			state = IGNORE;
			logger.println("[]");
		}

		void sendCommand(const String& command, State new_state, unsigned long timeout=10000)
		{
			if (debug_delay) sendCommandIn(0, command, new_state, timeout);
			else {
				logger.println(String("[")+new_state+"]");
				state = new_state;
				module.gsm_uart.println(command);
			}
		}

		void sendCommandIn(unsigned long time, const String& command, State new_state, unsigned long timeout=10000)
		{
			time += debug_delay;
			static auto chan = events::Channel<String, State>::make("gsm_command").subscribe([this](unsigned long time, String command, State new_state) {
				logger.println(String("[")+new_state+"]");
				state = new_state;
				module.gsm_uart.println(command);
			});
			chan.postIn(time, command, new_state);
			state = IGNORE;
			logger.println("[]");
		}

		void simulateReply(const String& reply, State new_state)
		{
			if (debug_delay) simulateReplyIn(0, reply, new_state);
			else {
				logger.println(String("[")+new_state+"]");
				logger.println(String("|")+reply);
				state = new_state;
				processLine(reply);
			}
		}

		void simulateReplyIn(unsigned long time, const String& reply, State new_state)
		{
			time += debug_delay;
			static auto chan = events::Channel<String, State>::make("gsm_command").subscribe([this](unsigned long time, String reply, State new_state) {
				logger.println(String("[")+new_state+"]");
				logger.println(String("|")+reply);
				state = new_state;
				processLine(reply);
			});
			chan.postIn(time, reply, new_state);
			state = IGNORE;
			logger.println("[]");
		}

		void powerOnBootstrap()
		{
			// Fails with ERROR unless I give the module some seconds to start up
			sendCommandIn(3000, "AT+IFC=2,2", CONNECT_1);
		}

		void processLine(String line)
		{

			static int response_counter = 0;
			switch (state) {
				case CONNECT_1: {
					if (line=="ERROR") module.restart();
					else if (line=="OK") {
						sendCommand("AT+SAPBR=2,1;+CGREG=1;+CGREG?;+CGATT?;+CSQ", CONNECT_2);
						response_counter = 0;
					}
				} break;
				case CONNECT_2: {
					// Restart on error
					if (line=="ERROR") module.restart();

					// Registered on network?
					if (line.startsWith("+CGREG:")) {
						// Interpret status
						int status = String(line[line.length()-1]).toInt();
						if (status==1 || status==5) {
							online = true;
							logger.println("Registered on network");
						} else if (status==0 || status==2 || status==3) {
							online = false;
							logger.println("Not registered on network");
						} else assert(!"Unexpected network status");

						// Unsolicited is 9, solicited is 11
						assert(line.length()==9 || line.length()==11);
						if (line.length()==11) response_counter++;
					}

					// GPRS connected?
					if (line.startsWith("+CGATT:")) {
						int status = String(line[line.length()-1]).toInt();
						if (status) logger.println("Attached to GPRS Service");
						else logger.println("Not attached to GPRS Service");
						response_counter++;
					}

					// Signal strength
					if (line.startsWith("+CSQ:")) {
						int signal_strength = line.substring(6, line.indexOf(',')).toInt();
						logger.println(String("Signal strength: ") + signal_strength);
						response_counter++;
					}

					// Bearer profile query
					if (line.startsWith("+SAPBR:")) {
						std::array<String, 2> toks;
						misc::tokenize(line, toks);
						bool has_bearer_profile = toks[1]=="0" || toks[1]=="1";
						if (has_bearer_profile) logger.println("Has bearer profile");
						else logger.println("Does not have bearer profile");
						response_counter++;
					}

					// Final OK
					if (line=="OK") response_counter++;

					// Trigger when all expected responses received and we are on line
					if (response_counter==5 && online) {
						sendCommand("AT+CGREG=0;+SAPBR=3,1,\"Contype\",\"GPRS\";+SAPBR=3,1,\"APN\",\"" APN "\"", CONNECT_3);
					}
				} break;
				case CONNECT_3: {
					if (line=="ERROR" || line=="OK") sendCommand("AT+SAPBR=1,1", CONNECT_4);
				} break;
				case CONNECT_4: {
					if (line=="ERROR" || line=="OK") sendCommand("AT+CIPMODE=1", CONNECT_5);
				} break;
				case CONNECT_5: {
					if (line=="ERROR" || line=="OK") sendCommand("AT+CSTT=" APN, CONNECT_6);
				} break;
				case CONNECT_6: {
					if (line=="ERROR" || line=="OK") sendCommand("AT+CIICR", CONNECT_7);
				} break;
				case CONNECT_7: {
					if (line=="ERROR" || line=="OK") sendCommand("AT+CIFSR", CONNECT_8);
				} break;
				case CONNECT_8: {
					std::array<String, 4> local_ip;
					bool is_ip = misc::tokenize(line, local_ip, '.');
					if (line=="ERROR" || is_ip) {
						sendCommand("AT+CIPCLOSE", CONNECT_9);
						response_counter = 0;
					}
				} break;
				case CONNECT_9: {
					if (line=="ERROR" || line=="CLOSE OK") sendCommand(String() + "AT+CIPSTART=\"TCP\",\"" + SERVER_IP + "\",\"" + SERVER_PORT + "\"", CONNECT_10);
				} break;
				case CONNECT_10: {
					if (line=="ERROR") module.restart();
					else if (line=="OK" || line=="CONNECT OK" || line=="CONNECT") response_counter++; // last time it was "CONNECT" but I am sure I have seen "CONNECT OK"
					if (response_counter==2) {
						module.gsm_uart.println("Hello!");
						setState(CONNECTED);
					}
				} break;
				case CONNECTED: {
					if (line=="ERROR") {
						module.restart();
					} else if (line=="CLOSED") {
						setState(IGNORE);
						module.restart();
					} else {
						logger.println("meh");
					}
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

		bool module_on = false;
	public:

		void setup()
		{
			module.gsm_uart.rxLineChan().subscribe([&](unsigned long time) {
				while (module_on && module.gsm_uart.hasLine()) {
					processLine(module.gsm_uart.popLine());
				}
			});
		}

		void modulePower(bool power)
		{
			this->online = false;
			this->module_on = power;
			state = IGNORE;
			if (power) powerOnBootstrap();
		}

		bool isConnected()
		{
			return module_on && (state == CONNECTED);
		}

	} gsm_link;

} // namespace {}




namespace telelink 
{
	void setup(
		void (*gpsPps)(), 
		void (*gpsData)(float latitude, float longitude, float elevation)
	)
	{	
		gsm_link.setup();
		module.begin([&](bool power) { gsm_link.modulePower(power);	});
	}

	void send(char* data, unsigned long num_bytes)
	{
		if (gsm_link.isConnected()) {
			//module.gsm_uart.println("Hello!");
		}
	}
	
}
