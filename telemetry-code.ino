#include "common.hpp"
#include "pins.hpp"
#include "sensors.hpp"
#include "telelink.hpp"
#include "events.hpp"
#include "regtek.hpp"
#include "watchdog.hpp"
#include <Wire.h>



namespace {
	Stream& logger = Serial;

	float readBatteryVoltage()
	{
		static const auto VBAT_PIN = A7;
		static const float ref_volt = 3.0f;
		return analogRead(VBAT_PIN) * (ref_volt * 2.f / 1024.f);
	}

	extern "C" char *sbrk(int i);
	inline unsigned long freeRam ()
	{
		char stack_dummy = 0;
		return &stack_dummy - sbrk(0);
	}

	void wireKhz(int wire_khz)
	{
		sercom3.disableWIRE();
		SERCOM3->I2CM.BAUD.bit.BAUD = SystemCoreClock / (2000 * wire_khz) - 1;
		sercom3.enableWIRE();        
	}


	events::Process& blink_process = events::makeProcess("blink").subscribe([&](unsigned long time, unsigned long delta) {
		static bool val = false;
		pinMode(pins::LED, OUTPUT);
		digitalWrite(pins::LED, val);    
		val = !val;
	});



	events::Process& stats_process = events::makeProcess("stats").subscribe([&](unsigned long time, unsigned long delta) {
		//logger.println(String() + "Free RAM: " + freeRam() + ", Voltage: " + readBatteryVoltage());
	}).setPeriod(10000);
}


void setup() {
	watchdog::setup();
	
	// light LED while booting
	pinMode(pins::LED, OUTPUT);
	digitalWrite(pins::LED, true);
	
	// Init i2c
	Wire.begin();
	wireKhz(400);
	
	// Init serial
	SerialUSB.begin(9600);
	int last_i=0;
	for (int i=0; i<200 && !SerialUSB; i++) {
		delay(100);
		last_i = i*100;
		watchdog::tickle();
	}
	SerialUSB.println(String("Yo! ") + last_i);
		
	// Init modules
	sensors::setup(regtek::sensorUpdate);
	telelink::setup(nullptr, nullptr);

	
	// Indicate correct or errorenous operation by blinking
	common::assert_channel.subscribe([&](unsigned long time, const char* msg) {
		blink_process.setPeriod(100);
	});
	blink_process.setPeriod(1000);
}


