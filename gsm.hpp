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
  void maintainConnection();



  enum Result
  {
    NOP = 0,
    OK = 1,
    ERROR = 2
  };
  
  class Task;
  class Runner;

  Task* makeCommandTask(const String& cmd, unsigned long timeout, std::function<Result(const String&, Runner*)> handler);
  Task* makeSyncTask(std::function<Result(bool failed, Runner*)> handler);

  
  class Runner
  {
    virtual Runner* thenGeneral(Task* task) = 0;
  public:
    Runner* then(const String& cmd, unsigned long timeout, std::function<Result(const String&, Runner*)> handler=nullptr)
    {
      return thenGeneral(makeCommandTask(cmd, timeout, handler));
    }
    Runner* sync(std::function<Result(bool failed, Runner*)> handler)
    {
      return thenGeneral(makeSyncTask(handler));
    }
  };

  Runner* runner();
};


