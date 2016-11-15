#include "gps.hpp"
#include "logging.hpp"
#include "MySerial.hpp"
#include <functional>
#include <cstring>

namespace gps 
{
  Logger& logger = logging::get("gps");


  class GpsLayer0 {
  public:
    MySerial serial = {"gps", true, false};
  
    void beginL0() 
    {
      logger.println("Opening serial");
      serial.begin(115200, 31ul/*PB23 SERCOM5.3 RX<-GPS_TX */, 30ul/*PB22 SERCOM5.2 TX->GPS_RX*/, PIO_SERCOM_ALT, PIO_SERCOM_ALT, SERCOM_RX_PAD_3, UART_TX_PAD_2, &sercom5);
    
    }    
    void updateL0() {}

    void IrqHandler() {
      serial.IrqHandler();
    }    
  };

  class GpsLayer1 : protected GpsLayer0
  {
    GpsData gps_data;
  public:
    void beginL1() 
    {
      beginL0();

      logger.println("Activating aviation mode");
      if (!run(
        "$PMTK886,2",
        [](const String& rsp){ 
           return rsp.startsWith("$PMTK001,886,3"); // *36
        }, 
        1000
      )) logger.println("Warning: Failed to activate aviation mode");
      logger.println();

      logger.println("Enabling fast TTF when out of tunnel mode");
      if (!run(
        "$PMTK257,1",
        [](const String& rsp){ 
           return rsp.startsWith("$PMTK001,257"); // *36
        }, 
        1000
      )) logger.println("Warning: Failed to activate fast TTG when out of tunnel mode");
      logger.println();
    }

    const GpsData& get() 
    { 
      return gps_data; 
    }

    bool run(const String& command, std::function<bool(const String&)> rsp_handler, unsigned long timeout=1000, int attempts=4)
    {
      int checksum = 0;
      for (int i=0; i<command.length(); i++) {
        auto ch = command[i];
        if (i==0 && ch=='$') continue;
        checksum ^= ch;
      }
      for (int attempt_i = 0; attempt_i<attempts; attempt_i++)
      {
        serial.println(command + "*" + String(checksum, HEX));
        for (int i=0; i<timeout/100; i++) {
          delay(100);
          while (serial.hasString()) {
            String rsp = serial.popString();
            if (rsp_handler(rsp)) {
              logger.println("OK");
              return true;
            } else if (unsolicitedMessageHandler(rsp)) {
              // it was an unrelated message; go on
            } else {
              logger.println(String("Unknown response: \"") + rsp + "\"");
            }
          }
        }
      }
      return false;
    }

    template<typename T>
    static void tokenizeNmea(const String& str, T& toks)
    {
      int r_idx = -1;
      bool err = false;
      for (auto& iter : toks) {
        int l_idx = r_idx+1;
        r_idx = str.indexOf(',', l_idx);
        if (r_idx<0) r_idx = str.indexOf('*', l_idx);
        if (l_idx<0 || r_idx<0) return;
        else iter = str.substring(l_idx, r_idx);
      }
    }
    
    
    static const String parseNmeaCoord(const String& coord, const String& dir)
    {
      int dot_pos = coord.indexOf('.');
      if (dot_pos<0) return "NaN";
      
      bool neg = 0;
      if (dir=="N" ) neg = false;
      else if (dir=="S") neg = true;
      else if (dir=="E") neg = false;
      else if (dir=="W") neg = true;
      else return dir;
    
      unsigned long degrees = coord.substring(0, dot_pos-2).toInt();
      unsigned long minutes_int  = coord.substring(dot_pos-2, dot_pos).toInt();
      unsigned long minutes_frac = coord.substring(dot_pos+1, dot_pos+5).toInt();
    
      unsigned long decimal_contribution = minutes_int*(100005/6) + (minutes_frac*10)/6;
    
      int part_decimal = (decimal_contribution % 1000000);
      int part_integer = degrees + (decimal_contribution / 1000000);
      if (neg) part_integer = -part_integer;
    
      char buff[7] = "000000";
      String part_decimal_s(part_decimal);
      int len = part_decimal_s.length();
      std::strncpy(buff + 6 - len, part_decimal_s.c_str(), len);
    
      return String(part_integer) + "." + String(buff);
    }


    bool unsolicitedMessageHandler(const String& msg)
    {
      if (msg[0]!='$') return false; // not a command
      if (msg[1]!='G') return false; // unknown talker
      int idx_l = 3;
      int idx_r = msg.indexOf(',', idx_l);
      String sss = msg.substring(idx_l, idx_r);
      if (sss=="GGA") { // Global Positioning System Fix Data. Time, Position and fix related data for a GPS receive
        std::array<String, 11> toks;
        tokenizeNmea(msg, toks);
        gps_data.fix = (toks[6].length()>0) ? toks[6].toInt() : 0;
        gps_data.latitude = parseNmeaCoord(toks[2], toks[3]);
        gps_data.longitude = parseNmeaCoord(toks[4], toks[5]);
        gps_data.altitude = (toks[10]=="M" && toks[9].length()>0) ? toks[9] : "NaN";
      } else if (sss=="ACCURACY") { // Accuracy
        std::array<String, 2> toks;
        tokenizeNmea(msg, toks);
        gps_data.accuracy = toks[1];
      } else if (sss=="RMC") { // Time, date, position, course and speed data
      } else if (sss=="VTG") { // Course and speed information relative to the ground
      } else if (sss=="GLL") { // Geographic Position - Latitude/Longitude. Position was calculated based on one or more of the SVs having their states derived from almanac parameters, as opposed to ephemerides.
      } else if (sss=="GSA") { // GPS DOP and active satellite
      } else if (sss=="GSV") { // Satellites in view
      } else if (sss=="ZDA") { // Time & Date – UTC, Day, Month, Year and Local Time Zone
      } else {
        return false; // Unknown sentence identifier
      }
      /*
      gps> Unhandled message: "$PMTK011,MTKGPS*08"
      gps> Unhandled message: "$PMTK010,001*2E"
      gps> Unhandled message: "$PMTK011,MTKGPS*08"
      gps> Unhandled message: "$PMTK010,002*2D"
      gps> Unhandled message: "$GLGV,1,1,00*65"
      */
      return true;
    }
    
    void updateL1(unsigned long timestamp, unsigned long delta)
    {
      updateL0();
      
      while (serial.hasString()) {
        String str = serial.popString();
        if (!unsolicitedMessageHandler(str)) {
          logger.println(String("Unhandled message: \"") + str + "\"");
        }
      }
    }      

    
    void prime(const String& lon /*10.418731*/, const String& lat /*63.415344*/, String date /*2016/11/13*/, String time_utc /*16:56:23*/)
    {
      //$PMTK741,Lat,Long,Alt,YYYY,MM,DD,hh,mm,ss *CS<CR><LF>

      int default_altitude = 50;
      
      // replace separators with ','
      for (int i=0; i<date.length(); i++) {
        auto& ch = date[i];
        if (ch=='/') ch = ',';
      }
      for (int i=0; i<time_utc.length(); i++) {
        auto& ch = time_utc[i];
        if (ch==':') ch = ',';
      }

      // prime time
      if (!run(
        String("$PMTK740,") + date + "," + time_utc,
        [](const String& rsp){ 
           return rsp.startsWith("$PMTK001,740"); 
        }, 
        1000
      )) logger.println("Warning: Failed to prime GPS with time");

      // prime location
      if (!run(
        String("$PMTK741,") + lat + "," + lon + "," + String(default_altitude) + "," + date + "," + time_utc,
        [](const String& rsp){ 
           return rsp.startsWith("$PMTK001,741"); 
        }, 
        1000
      )) logger.println("Warning: Failed to prime GPS with location");
      logger.println("Priming GPS done");
    }
  };


  class GpsFacade : protected GpsLayer1
  {
  public:
    void update(unsigned long timestamp, unsigned long delta)
    {
      updateL1(timestamp, delta);
    }
    
    void begin()
    {
      beginL1();
    }

    using GpsLayer0::IrqHandler;
    using GpsLayer1::prime;
    using GpsLayer1::get;
  };
  

  


}




namespace gps
{
  GpsFacade gps_obj;
  
  void begin()
  {
    gps_obj.begin();
  }

  void update(unsigned long timestamp, unsigned long delta)
  {
    gps_obj.update(timestamp, delta);
  }

  void prime(const String& lon, const String& lat, const String& date, const String& time_utc)
  {
    gps_obj.prime(lon, lat, date, time_utc);
  }

  const GpsData& get()
  {
    return gps_obj.get();
  }
}


void SERCOM5_Handler()
{
  gps::gps_obj.IrqHandler();
}

