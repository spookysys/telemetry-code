// NOTE: leftover code from arduino zero is hogging sercom5, which we need. Comment this out from variants.cpp to fix.
#include "common.hpp"
#include "logging.hpp"
#include "simcom.hpp"

extern "C" char *sbrk(int i);

namespace {
  Logger& logger = logging::get("main");
  
  
  float readBatteryVoltage()
  {
    static const auto VBAT_PIN = A7;
    static const float ref_volt = 3.0f;
    return analogRead(VBAT_PIN) * (ref_volt * 2.f / 1024.f);
  }
  
  int freeRam ()
  {
    char stack_dummy = 0;
    return &stack_dummy - sbrk(0);
  }

}


void setup() {

  pinMode(PIN_LED, OUTPUT);
  digitalWrite(PIN_LED, HIGH);

  logging::begin();
  logger.println("Hey there!");
  simcom::begin();

  logger.println("Setup done!");
}



void loop() {
  static unsigned long timestamp = 0; 
  unsigned long delta;
  {
    unsigned long new_timestamp = millis();
    if (timestamp==0) delta = 0;
    else delta = new_timestamp-timestamp;
    timestamp = new_timestamp;
  }


  
  simcom::update(timestamp, delta);

  //logger.print("VBat: " );
  //logger.println(misc::readBatteryVoltage());
  //logger.print("Ram: ");
  //logger.println(freeRam());

  //gsmComm.update();
  //gpsComm.update();
  //gpsComm.dumpAll();


  //  while (Serial.available()) {
  //    char ch = Serial.read();
  //    gsmSerial.write(ch);
  //  }
  //  while (gsmSerial.available()) {
  //    Serial.write((char)gsmSerial.read());
  //  }
  /*
    static int teller=0;
    static int toggler=0;
    teller++;

    if (teller==100) {
      teller=0;
      switch (toggler) {
        case 0:
        logger.println("INPUT_PULLDOWN");
        pinMode(PIN_GPS_EN, INPUT_PULLDOWN);
        toggler=1;
        break;
        case 1:
        logger.println("INPUT_PULLUP");
        pinMode(PIN_GPS_EN, INPUT_PULLUP);
        toggler=2;
        break;
        case 2:
        logger.println("LOW");
        pinMode(PIN_GPS_EN, OUTPUT);
        digitalWrite(PIN_GPS_EN, LOW);
        toggler=3;
        break;
        case 3:
        logger.println("HIGH");
        pinMode(PIN_GPS_EN, OUTPUT);
        digitalWrite(PIN_GPS_EN, HIGH);
        toggler=0;
        break;
      }
    }
  */
  // blink the mid-led
  pinMode(PIN_LED, OUTPUT);
  digitalWrite(PIN_LED, HIGH);
  delay(100);
  digitalWrite(PIN_LED, LOW);
  delay(100);
  
}
