#pragma once
#include "common.hpp"
#include <functional>

namespace gsm
{
  using gps_priming_fn_t = std::function<void(const String& lon, const String& lat, const String& date, const String& time_utc)>;

  void begin(gps_priming_fn_t gps_priming_callback);

  void update(unsigned long timestamp, unsigned long delta);

  static const int default_timeout = 1000;

  class Task;
  class Runner;

  Task* makeTask(const String& cmd, unsigned long timeout, std::function<void(const String&)> message_handler, std::function<void(Runner*)> done_handler);

  class Runner
  {
    virtual Runner* thenGeneral(Task* t) = 0; 
  public:
    template<typename... Args> Runner* then(Args... args) { return this->thenGeneral(makeTask(args...)); } 
    virtual void fail() = 0;
    virtual void finally(std::function<void(bool err)> finally_handler) = 0;
  };

  Runner* runner();
};


