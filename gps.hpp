#pragma once
#include "common.hpp"

namespace gps
{
  struct GpsData
  {
    unsigned long gga_time = 0;
    int    fix = 0;
    String latitude;
    String longitude;
    String altitude;
    
    unsigned long accuracy_time = 0;
    String accuracy;
  };
  
  void begin();
  void update(unsigned long timestamp, unsigned long delta);
  void prime(const String& lon, const String& lat, const String& date, const String& time_utc);
  const GpsData& get();
}

