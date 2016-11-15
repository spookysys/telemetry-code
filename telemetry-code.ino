// NOTE: leftover code from arduino zero is hogging sercom5, which we need. Comment this out from variants.cpp to fix.
#include "common.hpp"
#include "logging.hpp"
#include "simcom.hpp"
#include "gps.hpp"
#include "gsm.hpp"
#include "http.hpp"
#include "watchdog.hpp"


namespace {
  Logger& logger = logging::get("main");
  
  float readBatteryVoltage()
  {
    static const auto VBAT_PIN = A7;
    static const float ref_volt = 3.0f;
    return analogRead(VBAT_PIN) * (ref_volt * 2.f / 1024.f);
  }
}



// data.sparkfun setup
static const String publicKey = "roMd2jR96OTpEAb4jG1y";
static const String privateKey = "jk9NvjPKE6Ug1rq0P6NY";
static const String inputUrl = "http://data.sparkfun.com/input/"+publicKey+"?private_key="+privateKey;

// how long do we accept not having uploaded any telemetry before we reboot
static unsigned long first_send_deadline = 65000;
static unsigned long silence_deadline = 35000;



void setup() {

  pinMode(PIN_LED, OUTPUT);
  digitalWrite(PIN_LED, HIGH);

  watchdog::begin();
  logging::begin();
  logger.println("Hey there!");
  simcom::begin();
}

static unsigned long last_send_timestamp = 0;
static void sendData(unsigned long timestamp)
{
  
  gps::GpsData gps_data = gps::get();

  // generate string
  String url = inputUrl;
  url += "&seconds=" + String(timestamp / 1000);
  url += "&voltage=" + String(readBatteryVoltage()) + "&free_ram=" + String(freeRam());
  url += String("") + "&gps_fix=" + String(gps_data.fix) + "&gps_altitude=" + gps_data.altitude + "&gps_latitude=" + gps_data.latitude + "&gps_longitude=" + gps_data.longitude + "&gps_accuracy=" + gps_data.accuracy;
 
  // upload
  if (gsm::isConnected() && !http::isRequesting()) {
    http::rqGet(
      url, 
      [timestamp](bool err, int status) { 
        if (!err && (status==200 || status==201 || status==202)) {
          logger.println(String("Logged with status ") + String(status) + " at " + String(timestamp/1000) + "s");
          last_send_timestamp = timestamp;
        } else {
          logger.println(String("Logging failed with status ") + String(status) + " at " + String(timestamp/1000) + "s");
        }
      }
    );
  }
    
}


void every_30s(unsigned long timestamp)
{
  gsm::maintainConnection();
}



// called every 10 seconds
void every_10s(unsigned long timestamp)
{
  sendData(timestamp);  
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
  // update everything
  simcom::update(timestamp, delta);

  // keep watchdog timer happy
  watchdog::tickle();

  // reboot if not sending telemetry
  if (last_send_timestamp==0 && timestamp > first_send_deadline) watchdog::reboot();
  if (last_send_timestamp!=0 && timestamp-last_send_timestamp > silence_deadline) watchdog::reboot();

  // thus, in practice every_10th is ~ the same as this
  delay(100); 
}



void loop() {
  static unsigned long last_timestamp = 0;
  unsigned long timestamp = millis();
  if (last_timestamp==0) last_timestamp = timestamp;

  every(timestamp, timestamp-last_timestamp);  
  if (timestamp/100 != last_timestamp/100) every_10th_s(timestamp);
  if ((timestamp-50)/1000 != (last_timestamp-50)/1000) every_1s(timestamp);
  if ((timestamp-525)/10000 != (last_timestamp-525)/10000) every_10s(timestamp);
  if ((timestamp-5575)/30000 != (last_timestamp-5575)/30000) every_30s(timestamp);
  
  last_timestamp = timestamp;
}
