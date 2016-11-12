#pragma once
#include "common.hpp"
#include <functional>

namespace gsm
{
  void begin();
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
  };

  TaskKnob& run_task(Task* task); 
  template<typename... Args> inline TaskKnob& run(Args... args) { return run_task(make_task(args...)); }
      
};


