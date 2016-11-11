#pragma once
#include "common.hpp"
#include <functional>

namespace gsm
{
  void begin(std::function<bool(const String& line)> umh);
  void update(unsigned long timestamp, unsigned long delta);
};


