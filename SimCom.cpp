#include "simcom.hpp"
#include "logging.hpp"
#include "gsm.hpp"
#include "gps.hpp"
#include "watchdog.hpp"

namespace simcom
{
  Logger& logger = logging::get("simcom");
  
  const auto PIN_GPS_EN = 26ul;
  const auto PIN_STATUS = 25ul;
  const auto PIN_PWRKEY = 38ul;
  
  bool isOn()
  {
    return digitalRead(PIN_STATUS);
  }
  
  void powerOnOff() {
    int startStatus = isOn();
    if (startStatus) {
      logger.println("SimCom is on - turning off");
    } else {
      logger.println("SimCom is off - turning on");
    }
    pinMode(PIN_PWRKEY, OUTPUT);
    digitalWrite(PIN_PWRKEY, LOW);
    for (int i=0; i<10; i++) {
      delay(100);
      watchdog::tickle();
    }
    for (int i = 0; i <= 100; i++) {
      delay(100);
      watchdog::tickle();
      if (isOn() != startStatus) break;
      assert(i < 100);
    }
    digitalWrite(PIN_PWRKEY, HIGH);
    pinMode(PIN_PWRKEY, INPUT);
    //delay(100);
    int stopStatus = isOn();
    if (stopStatus) {
      logger.println("SimCom is now on");
    } else {
      logger.println("SimCom is now off");
    }
    assert(startStatus != stopStatus);
  }
  
  
  void begin() 
  {
    pinMode(PIN_GPS_EN, INPUT); // high-z
    pinMode(PIN_STATUS, INPUT_PULLDOWN);
    pinMode(PIN_PWRKEY, INPUT_PULLDOWN);
  
    // If module already on, reset it
    if (isOn()) {
      powerOnOff();
      assert(!isOn());
      for (int i=0; i<8; i++) {
        delay(100);
        watchdog::tickle();
      }
      assert(!isOn());
    }
  
    // Turn module on
    assert(!isOn());
    powerOnOff();
    assert(isOn());
  
    // GSM
    gsm::begin(gps::prime);

    // GPS
    gps::begin();
    watchdog::tickle();
  }
  
  void update(unsigned long timestamp, unsigned long delta)
  {
    gsm::update(timestamp, delta);
    gps::update(timestamp, delta);
  }
}



#if 0

void getWeatherData(int lat, int lon) {
  gsmSerial.setTimeout(10000);


  gsmSerial.println("AT+HTTPINIT");
  assert(gsmSerial.find("OK\r") != -1);
  logger.println();

  gsmSerial.println(String("") + "AT+HTTPPARA=\"URL\",\"api.openweathermap.org/data/2.5/weather?lat=" + lat + "&lon=" + lon + "&APPID=" + OWM_APIKEY + "\"");
  assert(gsmSerial.find("OK\r") != -1);
  logger.println();

  gsmSerial.println("AT+HTTPACTION=0");
  assert(gsmSerial.find("OK\r") != -1);
  assert(gsmSerial.find("+HTTPACTION") != -1);
  logger.println();

  gsmSerial.println("AT+HTTPREAD");
  assert(gsmSerial.find("OK\r") != -1);
  logger.println();

  // +HTTPREAD: 107
  // {"cod":401, "message": "Invalid API key. Pleamap.org/faq#error401 for more info."}

}

void httpGet(const String& url, std::function<bool (const string&)> func)
{
  
  
}

#endif




