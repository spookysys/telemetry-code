#pragma once
#include "MySerial.hpp"
#include <functional>
#include <queue>
#include <memory>

// something like javascript promises, to sequence asynchronously chains of commands and response-handlers

class GsmBoss
{
  MySerial& serial;

public:
  using ResolveFunc = std::function<void(const String&)>;
  using FailFunc = std::function<void(const String&)>;

private:
  using ActionFunc = std::function<String(const String&)>;
  using ResponseFunc = std::function<void(const String& message, ResolveFunc resolve, FailFunc fail)>;
  using FailedFunc = std::function<void(const String& message, ResolveFunc resolve, FailFunc fail)>;
  using SuccessFunc = std::function<void(const String&)>;
  class Continuator
  {
  private:
    friend GsmBoss;
    unsigned long timeout = 0;
    ActionFunc action_func = nullptr;
    FailedFunc failed_func = nullptr;
    ResponseFunc response_func = nullptr;
    SuccessFunc success_func = nullptr;
    std::unique_ptr<Continuator> then_continuator = nullptr;
    Continuator() {}
    Continuator(ActionFunc action_func, int timeout) : action_func(action_func), timeout(timeout) {}
    Continuator(Continuator&& op) = default;
    Continuator& operator=(Continuator&& op) = default;
  public:
    Continuator& then(ActionFunc action, unsigned long timeout = 1000) 
    {
      assert(!this->then_continuator);
      assert(!this->success_func);
      this->then_continuator = std::unique_ptr<Continuator>(new Continuator(action, timeout));
      return *this->then_continuator;
    }
/*
    Continuator& then(std::function<String()> action, unsigned long timeout = 1000) 
    {
      return this->then([=](const String& op)->String{ return action(); }, timeout);
    }
*/
    Continuator& then(const String& command, unsigned long timeout = 1000)
    {
      return this->then([=](const String& op)->String{ return command; }, timeout);
    }
    
    Continuator& success(SuccessFunc success_func)
    {
      assert(!this->then_continuator);
      assert(!this->success_func);
      this->success_func = success_func;
      return *this;
    }
    Continuator& failure(FailedFunc failed_func)
    {
      assert(!this->failed_func);
      this->failed_func = failed_func;
      return *this;
    }
    Continuator& response(ResponseFunc response_func)
    {
      assert(!this->response_func);
      this->response_func = response_func;
      return *this;
    }
  };

  unsigned long countdown = 0;
  std::unique_ptr<Continuator> current = nullptr;
  String command; // just for error messages
  std::queue<std::unique_ptr<Continuator>> todo;
public:
  GsmBoss(MySerial& serial) : serial(serial) {}

  Continuator& first(std::function<String()> action_func_noparam, unsigned long timeout=1000)
  {
    todo.push(std::unique_ptr<Continuator>(new Continuator([=](const String&)->String { return action_func_noparam(); }, timeout)));
    return *todo.back();
  }
  
  Continuator& first(const String& command, unsigned long timeout=1000)
  {
    todo.push(std::unique_ptr<Continuator>(new Continuator([=](const String&)->String { return command; }, timeout)));
    return *todo.back();
  }

  bool (*callback)(const String& str) = nullptr;

  void update(unsigned long timestamp, unsigned int delta) 
  {
    String str;
    bool hasString = serial.hasString();
    if (hasString) str = serial.popString();

    // try the callback
    if (hasString && callback) {
      bool consumed = callback(str);
      if (consumed) {
        hasString = false;
      }
    }

    // handle responses and timeouts to currently running action
    if (current) {
      bool done = false;
      bool failed = false;
      String done_str;
      String failed_str;

      // try to consume string
      if (hasString) {        
        if (str=="OK") {
          done = true;
          done_str = str;
          hasString = false;
        } else if (str=="ERROR") {
          failed = true;
          failed_str = str;
          hasString = false;
        } else if (current->response_func) {
          current->response_func( 
            str, 
            [&](const String& msg) {
              done = true;
              done_str = msg;
            },
            [&](const String& err) {
              failed = true;
              failed_str = err;
            }
          );
          hasString = false;
        }
      }

      // implements timeout
      if (!done && !failed) {
        assert(this->countdown>0);
        this->countdown -= delta;
        if (this->countdown<=0) {
          failed = true;
          failed_str = "TIMEOUT";
          this->countdown = 0;
        }
      }

      // respond to "done" or "failed"
      assert(!(done && failed)); // can't be both
      if (failed) this->countdown = 0; 
      while (failed && this->current) {
        if (this->current->failed_func) {
          failed = false;
          this->current->failed_func( 
            failed_str, 
            [&](const String& msg) {
              // we're actually good and can go to "then"
              done = true;
              done_str = msg;
            },
            [&](const String& err) {
              // throw new failure to later handler
              std::unique_ptr<Continuator> tmp = std::move(this->current);
              this->current = std::move(tmp->then_continuator);
              tmp.reset();
              failed = true;
              failed_str = err;
            }
          );
          if (!failed && !done) { // we're finished - kill the whole remaining range of continuators
            while (this->current) {
              std::unique_ptr<Continuator> tmp = std::move(this->current);
              this->current = std::move(tmp->then_continuator);
              tmp.reset();
            }
          }
        } else {
          std::unique_ptr<Continuator> tmp = std::move(this->current);
          this->current = std::move(tmp->then_continuator);
          tmp.reset();
        }
        if (!this->current && failed) {
          logger.println(String("GsmBoss: Unhandled failure \"") + failed_str + "\" while executing \"" + this->command + "\"");
          assert(0);           
        }
      }
           
      if (done) {
        if (this->current->then_continuator) {
          std::unique_ptr<Continuator> tmp = std::move(this->current->then_continuator);
          this->current.reset();
          this->current = std::move(tmp);
          this->command = this->current->action_func(done_str);
          serial.println(this->command);
          this->countdown = this->current->timeout;
        } else {
          if (this->current->success_func)
            this->current->success_func(done_str);
          this->current.reset();
          this->countdown = 0;
        }
      }

    }

    // start new action from queue
    if (!current && todo.size()) {
      this->current = std::move(todo.front());
      todo.pop();
      String nullstr;
      this->command = this->current->action_func(nullstr);
      serial.println(this->command);
      this->countdown = this->current->timeout;
    }

    // is string still not consumed? then it is an error
    if (hasString) {
      if (this->current)
        logger.println(String("GsmBoss: Unexpected message \"")+str+"\" while executing \"" + this->command + "\"");
      else
        logger.println(String("GsmBoss: Unexpected message \"")+str+"\"");
      
      assert(0);
    }
  }
  

  
};

