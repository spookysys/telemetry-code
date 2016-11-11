#pragma once
#include "common.hpp"

namespace gps
{
  void begin();
  void update(unsigned long timestamp, unsigned long delta);
  void prime(const String& lon, const String& lat, const String& date, const String& time_utc);
}

