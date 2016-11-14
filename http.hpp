#pragma once
#include "common.hpp"
#include <functional>

namespace http
{

  bool get(const String& url, std::function<void(bool)> done_callback=nullptr);
  int getNumRqsInProgress();
}


