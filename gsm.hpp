#pragma once
#include "common.hpp"
#include <functional>

namespace gsm
{
  using gps_priming_fn_t = std::function<void(const String& lon, const String& lat, const String& date, const String& time_utc)>;

  void begin(gps_priming_fn_t gps_priming_callback);
  void update(unsigned long timestamp, unsigned long delta);

  bool isConnected();
  void connectionFailed();
  

  static const int default_timeout = 1000;

  class Task;
  class Runner;


  Task* makeCommandTask(const String& cmd, unsigned long timeout, std::function<void(const String&)> message_handler, std::function<bool(Runner*)> done_handler);
  Task* makeFinallyTask(std::function<bool(bool err, Runner*)> handler);
  
  class Runner
  {
    virtual Runner* thenGeneral(Task* task) = 0;
  public:
    Runner* then(const String& cmd, unsigned long timeout, std::function<void(const String&)> message_handler=nullptr, std::function<bool(Runner*)> done_handler=nullptr)
    {
      return thenGeneral(makeCommandTask(cmd, timeout, message_handler, done_handler));
    }
    Runner* finally(std::function<bool(bool err, Runner*)> finally_handler)
    {
      return thenGeneral(makeFinallyTask(finally_handler));
    }
  };

  Runner* runner();
};


