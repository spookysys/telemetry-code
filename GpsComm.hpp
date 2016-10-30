#pragma once
#include "common.hpp"
#include "TinyGPS.h"

class GpsComm
{
public:
  void begin();
  TinyGPSPlus& state();
  const TinyGPSPlus& state() const;
};

extern GpsComm gpsComm;

