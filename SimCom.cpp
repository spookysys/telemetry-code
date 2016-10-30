#include "SimCom.hpp"
#include "Logger.hpp"

namespace {
  const auto PIN_GPS_EN = 26ul;
  const auto PIN_STATUS = 25ul;
  const auto PIN_PWRKEY = 38ul;
}

bool SimCom::isOn() {
  return digitalRead(PIN_STATUS);
}

void SimCom::powerOnOff() {
    int startStatus = isOn();
    if (startStatus) {
      logger.println("SimCom is on - turning off");
    } else {
      logger.println("SimCom is off - turning on");
    }
    pinMode(PIN_PWRKEY, OUTPUT);
    digitalWrite(PIN_PWRKEY, LOW);
    delay(1000);
    while(isOn()==startStatus) {
      delay(100);
    }
    digitalWrite(PIN_PWRKEY, HIGH);
    pinMode(PIN_PWRKEY, INPUT);
    delay(100);
    int stopStatus = isOn();
    if (stopStatus) {
      logger.println("SimCom is now on");
    } else {
      logger.println("SimCom is now off");
    }
    assert(startStatus!=stopStatus);
}
  
void SimCom::begin()
{
  pinMode(PIN_GPS_EN, INPUT); // high-z
  //digitalWrite(PIN_LED, LOW);
  pinMode(PIN_STATUS, INPUT_PULLDOWN);
  pinMode(PIN_PWRKEY, INPUT_PULLDOWN);
  logger.println("SimCom?");

  // turn on SimCom (cycle power if already on)
  if (isOn()) {
    powerOnOff();
    assert(!isOn());
    delay(800);
  }
  assert(!isOn());
  powerOnOff();
  assert(isOn());

  // activate GPS too
  //digitalWrite(PIN_LED, HIGH);
 
  logger.println("SimCom!");
}

  

SimCom simCom;


