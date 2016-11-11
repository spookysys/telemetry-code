#pragma once
#include "common.hpp"
#include <functional>

namespace gsm
{
  void begin(std::function<bool(const String& line)> umh);
  void update(unsigned long timestamp, unsigned long delta);


  class Task;
  class TaskKnob;
  static const long default_timeout = 1000;
  Task* make_task(const String& cmd, std::function<void(const String&)> message_handler, std::function<void(bool err, TaskKnob&)> done_handler, unsigned long timeout=default_timeout);
  Task* make_task(const String& cmd, std::function<void(const String&)> message_handler, std::function<void(bool)>                done_handler, unsigned long timeout=default_timeout);
  Task* make_task(const String& cmd,                                                     std::function<void(bool, TaskKnob&)>     done_handler, unsigned long timeout=default_timeout);
  Task* make_task(const String& cmd,                                                     std::function<void(bool)>                done_handler, unsigned long timeout=default_timeout);

 

  class TaskKnob
  {
    Task* task;
  public:
    TaskKnob(Task* task) : task(task) {}
    TaskKnob& then(Task* task);
    template<typename... Args> TaskKnob& then(Args... args) { this->then(make_task(args...)); }  
  };

  TaskKnob& run(Task* task); 
  template<typename... Args> TaskKnob& run(Args... args) { run(make_task(args...)); }
      
};


