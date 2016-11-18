// NOTE: leftover code from arduino zero is hogging sercom5, which we need. Comment this out from variants.cpp to fix.
#include "common.hpp"
#include "logging.hpp"
#include "simcom.hpp"
#include "gps.hpp"
#include "gsm.hpp"
#include "http.hpp"
#include "watchdog.hpp"
#include "flashlog.hpp"
#include "imu.hpp"
#include <Wire.h>

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
static unsigned long first_send_deadline = 125000;
static unsigned long silence_deadline = 65000;



void setup() {

  pinMode(PIN_LED, OUTPUT);
  digitalWrite(PIN_LED, HIGH);

  logging::begin();
  logger.println("Hey there display!");
  
  flashlog::begin();
  logger.println("Hey there flash too!");

  Wire.begin(); // Start Wire (I2C)
  /*
  sercom3.disableWIRE(); // Disable the I2C bus
  SERCOM3->I2CM.BAUD.bit.BAUD = SystemCoreClock / ( 2 * 400000) - 1; // Set the I2C SCL frequency to 400kHz
  sercom3.enableWIRE(); // Restart the I2C bus
  */

  bool imu_ok = imu::begin();
  
  watchdog::begin();

  if (!imu_ok) {
    logger.println("Could not initialize IMU - rebooting");
    watchdog::reboot();
  }
  
  simcom::begin();
  
  flashlog::gpsFile()->println("logtime,gga_time,fix,latitude,longitude,altitude,accuracy_time,accuracy");
  watchdog::tickle();

  flashlog::sensorFile()->println("ax,ay,az,gx,gy,gz,mx,my,mz,q0,qx,qy,qz");

  logger.println("Initialization done!");
  watchdog::tickle();
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
          logger.println(String("Uploaded telemetry at ") + String(timestamp/1000) + "s");
          last_send_timestamp = timestamp;
        } else if (!err) {
          logger.println(String("Upload failed with status ") + String(status) + " at " + String(timestamp/1000) + "s");
        } else {
          logger.println(String("Upload failed at ") + String(timestamp/1000) + "s");
          gsm::connectionFailed();
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

  watchdog::tickle();
 
  logger.println("Flushing flash..");
  flashlog::flush();
  logger.println("done.");
}


// called every 1 second
void every_1s(unsigned long timestamp)
{
  static bool led_val = false;
  digitalWrite(PIN_LED, led_val);
  led_val = !led_val;

  const gps::GpsData& gps_data = gps::get();
  flashlog::gpsFile()->println(String(timestamp) + "," + String(gps_data.gga_time) + "," + String(gps_data.fix) + "," + gps_data.latitude + "," + gps_data.longitude + "," + gps_data.altitude + "," + gps_data.accuracy_time + "," + gps_data.accuracy);
}


// called every 10th second
void every_10th_s(unsigned long timestamp)
{
  // keep watchdog timer happy
  watchdog::tickle();

  imu::update();

  imu::Data imu_data = imu::get();
  flashlog::sensorFile()->println(String(imu_data.ax) + "," + String(imu_data.ay) + "," + String(imu_data.az) + "," + String(imu_data.gx) + "," + String(imu_data.gy) + "," + String(imu_data.gz) + "," + String(imu_data.mx) + "," + String(imu_data.my) + "," + String(imu_data.mz) + "," + String(imu_data.q0) + "," + String(imu_data.qx) + "," + String(imu_data.qy) + "," + String(imu_data.qz));
}



void every(unsigned long timestamp, unsigned long delta)
{ 
  // update everything
  simcom::update(timestamp, delta);

  // reboot if not sending telemetry
  if (last_send_timestamp==0 && timestamp > first_send_deadline) {
    logger.println("Deadline reached before first successful telemetry upload. Rebooting.");
    watchdog::reboot();
  }
  if (last_send_timestamp!=0 && timestamp-last_send_timestamp > silence_deadline) {
    logger.println("Deadline reached before next successful telemetry upload. Rebooting.");
    watchdog::reboot();
  }

  // short delay
  delay(10); 
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
