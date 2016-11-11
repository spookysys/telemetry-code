#pragma once
#include "common.hpp"
#include <list>


class Scheduler;


class Task
{
  Scheduler& scheduler;
  int num_prevs = 0;
  std::list<std::pair<Task*, std::function<bool()>> nexts;
public:
  class Interface {
    Task& task;
  private:
    friend class Task;
    TaskInterface(Task& task) : task(task) {}
  public:
    TaskInterface& then(Task* next) 
    {
      task.nexts.push_back( std::pair<Task*, std::function<bool()>(next, []{return true;}) );
      next->num_prevs++;
    }
    TaskInterface& if(std::function<bool()> condition, Task* then_, Task* else_=nullptr)
    {
      task.nexts.push_back( std::pair<Task*, std::function<bool()>(then_, []{return  condition();}) );
      task.nexts.push_back( std::pair<Task*, std::function<bool()>(else_, []{return !condition();}) );
      then_->num_prevs++;
      else_->num_prevs++;
    }
    template<typename T, int case_num>
    TaskInterface& select(std::function<T()> expression, const std::array<std::pair<T, Task*>, case_num>& cases)
    {
      for (int i=0; i<case_num; i++) {
        task.nexts.push_back( std::pair<Task*, std::function<bool()>(cases[i]->second, []{return expression()==cases[i]->first;}) );
        cases[i]->second->num_prevs++;
      }
    }
  };
private:
  friend class Scheduler;
  TaskInterface interface;
  virtual void start() = 0;
protected:
  Task(Scheduler& scheduler) : scheduler(scheduler), interface(*this) {}
  void finished();
};


class TaskInterface
{
public:
  template<typename T, int case_num>
  TaskInterface& select(std::function<T()>, const std::array<Task*, case_num>& cases);
};


class Scheduler
{
  std::list<unique_ptr<Task>> task_pool;
  Task* begin = nullptr;
  Task* end   = nullptr;
};


