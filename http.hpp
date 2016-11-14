#pragma once
#include "common.hpp"
#include <functional>

namespace http
{

  bool rqGet(const String& url, std::function<void(bool)> done_callback=nullptr);
  int  isRqInProgress();
}


