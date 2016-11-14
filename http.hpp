#pragma once
#include "common.hpp"
#include <functional>

namespace http
{

  void rqGet(const String& url, std::function<void(bool)> done_callback=nullptr);
  bool isRequesting();
}


