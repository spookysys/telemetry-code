#pragma once
#include "common.hpp"

// Note: this modem has built-in gps!
namespace modem 
{
	void setup(
		void (*gpsPps)(), 
		void (*gpsData)(float latitude, float longitude, float elevation)
	);

	bool isOn();

}