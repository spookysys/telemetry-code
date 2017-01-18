#include "telelink.hpp"
#include "events.hpp"
#include "pins.hpp"
#include "wiring_private.h" // pinPeripheral() function

using namespace telelink;


namespace {
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
}


namespace telelink 
{
	bool isOn() 
	{
		return digitalRead(pins::SC_STATUS);
	}

	void setup(
		void (*gpsPps)(), 
		void (*gpsData)(float latitude, float longitude, float elevation)
	)
	{
	
		auto& event_turn_on = events::makeChannel<int>("telelink_turn_on");
		auto& event_connect = events::makeChannel<int>("telelink_connect");

		event_turn_on.subscribe([&](unsigned long time, int step) {
			if (step>0 && isOn()) {
				event_connect.publish(0);
				return;
			}
			switch(step) {
				case 0:
					pinMode(pins::SC_PWRKEY, OUTPUT);
					digitalWrite(pins::SC_PWRKEY, HIGH);
					event_turn_on.publishAt(time+500, step+1);
					break;
				case 1:
					digitalWrite(pins::SC_PWRKEY, LOW);
					event_turn_on.publishAt(time+1000, step+1);
					break;
				case 2:
					digitalWrite(pins::SC_PWRKEY, HIGH);
					event_turn_on.publishAt(time+100, step+1);
					break;
				default:
					assert(step>2 && step<2+22);
					event_turn_on.publishAt(time+100, step+1);
			}
		});

		event_connect.subscribe([&](unsigned long time, int step) {
			switch (step) {
				case 0:
					break;
			}
		});

	}
}
