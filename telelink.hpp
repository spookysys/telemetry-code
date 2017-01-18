#pragma once
#include "common.hpp"

// Note: this telelink has built-in gps!
namespace telelink 
{
	void setup(
		void (*gpsPps)(), 
		void (*gpsData)(float latitude, float longitude, float elevation)
	);

	bool isOn();

}