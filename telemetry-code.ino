// NOTE: leftover code from arduino zero is hogging sercom5, which we need. Comment this out from variants.cpp to fix.
#include "common.hpp"
#include "logging.hpp"
#include "simcom.hpp"
#include "gps.hpp"
#include "gsm.hpp"
#include "http.hpp"


// data.sparkfun setup
static const String publicKey = "roMd2jR96OTpEAb4jG1y";
static const String privateKey = "jk9NvjPKE6Ug1rq0P6NY";
static const String inputUrl = "http://data.sparkfun.com/input/"+publicKey+"?privateKey="+privateKey;


namespace {
  Logger& logger = logging::get("main");
  
  
  float readBatteryVoltage()
  {
    static const auto VBAT_PIN = A7;
    static const float ref_volt = 3.0f;
    return analogRead(VBAT_PIN) * (ref_volt * 2.f / 1024.f);
  }
  

}


void setup() {

  pinMode(PIN_LED, OUTPUT);
  digitalWrite(PIN_LED, HIGH);
  
  logging::begin();
  logger.println("Hey there!");
  simcom::begin();
}



// called every 10 seconds
void every_10s(unsigned long timestamp)
{
  logger.println("10s");

  gps::GpsData gps_data = gps::get();

  String url = inputUrl + "&voltage=" + String(readBatteryVoltage()) + "&free_ram=" + String(freeRam());
  if (gps_data.fix!=0) url += String("") + "&fix=" + String(gps_data.fix) + "&altitude=" + gps_data.altitude + "&latitude=" + gps_data.latitude + "&longitude=" + gps_data.longitude + "&accuracy=" + gps_data.accuracy;
  if (gsm::isConnected() && !http::isRqInProgress()) {
    http::rqGet(
      url, 
      [](bool err) { 
        logger.println(String("get returned with ") + err);
      }
    );
  }
  
}


// called every 1 second
void every_1s(unsigned long timestamp)
{
  static bool led_val = false;
  digitalWrite(PIN_LED, led_val);
  led_val = !led_val;
}


// called every 10th second
void every_10th_s(unsigned long timestamp)
{
  
}


void every(unsigned long timestamp, unsigned long delta)
{  
  simcom::update(timestamp, delta);

  delay(100); // thus, in practice every_10th is ~ the same as this
}



void loop() {
  static unsigned long last_timestamp = 0;
  unsigned long timestamp = millis();
  if (last_timestamp==0) last_timestamp = timestamp;

  every(timestamp, timestamp-last_timestamp);  
  if (timestamp/100 != last_timestamp/100) every_10th_s(timestamp);
  if ((timestamp+33)/1000 != (last_timestamp+33)/1000) every_1s(timestamp);
  if ((timestamp+466)/10000 != (last_timestamp+466)/10000) every_10s(timestamp);
  
  last_timestamp = timestamp;
}
