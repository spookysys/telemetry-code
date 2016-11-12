#pragma once
#include "common.hpp"
#include <functional>

namespace gsm
{
  using gps_priming_fn_t = std::function<void(const String& lon, const String& lat, const String& date, const String& time_utc)>;

  void begin(gps_priming_fn_t gps_priming_callback);
  void update(unsigned long timestamp, unsigned long delta);


  class Task;
  class TaskKnob;
  static const long default_timeout = 1000;
  Task* make_task(const String& cmd, std::function<void(const String&)> message_handler, std::function<void(bool err, TaskKnob&)> done_handler, unsigned long timeout=default_timeout);
  Task* make_task(const String& cmd, std::function<void(const String&)> message_handler, unsigned long timeout=default_timeout);
  Task* make_task(const String& cmd, std::function<void(bool, TaskKnob&)> done_handler, unsigned long timeout=default_timeout);
  Task* make_task(const String& cmd, unsigned long timeout=default_timeout);

 

  class TaskKnob
  {
    Task* task;
    TaskKnob& then_task(Task* task);
  public:
    TaskKnob(Task* task) : task(task) {}
    template<typename... Args> TaskKnob& then(Args... args) { return this->then_task(make_task(args...)); } 
    TaskKnob& abort(int num=0x7FFFFFFF);
  };

  TaskKnob& run_task(Task* task); 
  template<typename... Args> inline TaskKnob& run(Args... args) { return run_task(make_task(args...)); }
      
};


