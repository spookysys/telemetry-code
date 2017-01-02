#include "common.hpp"
#include "pins.hpp"
#include "sensors.hpp"
#include "telelink.hpp"
#include "events.hpp"
#include "watchdog.hpp"
#include <Wire.h>

namespace {
  Stream& logger = Serial;
  
  events::Process& blink_process = events::makeProcess("blink").subscribe([&](unsigned long time, unsigned long delta) {
    static bool val = false;
    pinMode(pins::LED, OUTPUT);
    digitalWrite(pins::LED, val);    
    val = !val;
  });

  /*
  events::Process& stats_process = events::makeProcess("stats").subscribe([&](unsigned long time, unsigned long delta) {
    logger.println("Stats go here..");
    // assert(!"Just testing assertions");
  }).setPeriod(10000);
  */

}


void setup() {
    watchdog::setup();
    
    // light LED while booting
    pinMode(pins::LED, OUTPUT);
    digitalWrite(pins::LED, true);
    
    // Init i2c
    Wire.begin();
    sercom3.disableWIRE();
    SERCOM3->I2CM.BAUD.bit.BAUD = SystemCoreClock / (2 * 400000) - 1; // 400khz
    sercom3.enableWIRE();    
    
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
    bool sensors_ok = sensors::setup();
    bool telelink_ok = telelink::setup();
    assert(sensors_ok);
    assert(telelink_ok);
    
    // Indicate correct or errorenous operation by blinking
    common::assert_channel.subscribe([&](unsigned long time, const char* msg) {
        blink_process.setPeriod(100);
    });
    blink_process.setPeriod(1000);
    
}


