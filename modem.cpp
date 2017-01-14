#include "modem.hpp"
#include "events.hpp"
#include "pins.hpp"

using namespace modem;

namespace modem 
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
		// Turn on
		pinMode(pins::SC_PWRKEY, OUTPUT);
		digitalWrite(pins::SC_PWRKEY, HIGH);
		if (!isOn()) {
			auto& modem_on = events::makeChannel<int>("modem_on").subscribe([&](unsigned long time, int step) {
				if (step==0) digitalWrite(pins::SC_PWRKEY, LOW);
				if (step==1) digitalWrite(pins::SC_PWRKEY, HIGH);
				if (step==2) assert(isOn());
			});
			modem_on.publishIn( 500, 0);
			modem_on.publishIn(1500, 1);
			modem_on.publishIn(1600, 2);
		}
	}
}
