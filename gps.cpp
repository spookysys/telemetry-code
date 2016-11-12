#include "gps.hpp"
#include "logging.hpp"
#include "MySerial.hpp"

namespace gps { MySerial serial("gps", true, false); }

void SERCOM5_Handler()
{
  gps::serial.IrqHandler();
}



namespace gps 
{
  Logger& logger = logging::get("gps");
  
  void begin()
  {
    logger.println("Opening serial");
    serial.begin(115200, 31ul/*PB23 SERCOM5.3 RX<-GPS_TX */, 30ul/*PB22 SERCOM5.2 TX->GPS_RX*/, PIO_SERCOM_ALT, PIO_SERCOM_ALT, SERCOM_RX_PAD_3, UART_TX_PAD_2, &sercom5);
    logger.println("Setup done!");
  }

  void update(unsigned long timestamp, unsigned long delta)
  {
    while (serial.hasString()) {
      String str = serial.popString();
      if (str[0]!='$') {
        logger.println(str);
        logger.println("Not understood");
        continue;
      }
      if (str[1]!='G') {
        logger.println(str);
        logger.println("Unknown talker");
        continue;
      }
      int idx_l = 3;
      int idx_r = str.indexOf(',', idx_l);
      String sss = str.substring(idx_l, idx_r);
      //logger.println(sss);
      if (sss=="GGA") {
        // Global Positioning System Fix Data. Time, Position and fix related data for a GPS receive
      } else if (sss=="RMC") {
        // Time, date, position, course and speed data
      } else if (sss=="GLL") {
        // Geographic Position - Latitude/Longitude
        // Position was calculated based on one or more of the SVs having their states derived from almanac parameters, as opposed to ephemerides.
      } else if (sss=="VTG") {
        // Course and speed information relative to the ground
      } else if (sss=="ACCURACY") {
        // ...
      } else if (sss=="GSA") { // GPS DOP and active satellite
      } else if (sss=="GSV") { // Satellites in view
      } else if (sss=="ZDA") { // Time & Date – UTC, Day, Month, Year and Local Time Zone
      } else {
        logger.println(str);
        logger.println("Unknown sentence identifier");
      }
    }
  }

  void prime(const String& lon, const String& lat, const String& date, const String& time_utc)
  {
    logger.println("gps::prime");
  }
}




