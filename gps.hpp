#pragma once
#include "common.hpp"

namespace gps
{
  struct GpsData
  {
    int    fix = 0;
    String lat;
    String lon;
    String altitude;
    String accuracy;
  };
  
  void begin();
  void update(unsigned long timestamp, unsigned long delta);
  void prime(const String& lon, const String& lat, const String& date, const String& time_utc);
  const GpsData& get();
}

