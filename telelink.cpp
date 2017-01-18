#include "telelink.hpp"
#include "events.hpp"
#include "pins.hpp"
#include "wiring_private.h" // pinPeripheral() function

using namespace telelink;


namespace {
	Stream& logger = SerialUSB;

	class GsmSerial : public HardwareSerial
	{
		static constexpr unsigned long fifo_depth = 1024;
		static constexpr unsigned long baudrate = 19200;
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
		bool rx_full = false;
	
	public:

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
		
	};



	namespace connect {

		enum State
		{
			INIT = 0,
			PWRKEY_LOW = 1,
			PWRKEY_HIGH = 2,
			AWAIT_STATUS = 3,
			TURNED_ON = 4
		};

		void callback(unsigned long time, State state, unsigned long timeout);

		auto& connect_channel = events::Channel<State, unsigned long>::make("telelink_connect").subscribe(callback);


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
				assert(!"Lost power on SimCom module");
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
					logger.println("TURNED_ON - done for now!");
				} break;
				default:
					assert(!"Something went wrong!");
					postIn(0, INIT);
			}
		}
	} // namespace connect {}
} // namespace {}


namespace telelink 
{


	void setup(
		void (*gpsPps)(), 
		void (*gpsData)(float latitude, float longitude, float elevation)
	)
	{	
		logger.println("Connecting");
		connect::postIn(0, connect::INIT);
	}
}
