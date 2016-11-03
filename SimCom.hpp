#pragma once
#include "common.hpp"

namespace simcom
{
  void begin();
  bool isOn();
  void powerOnOff();
  void update(unsigned long timestamp, unsigned long delta);
}




