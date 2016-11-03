#pragma once
#include "MySerial.hpp"
#include <functional>
#include <queue>

// something like javascript promises, to sequence asynchronously chains of commands and response-handlers


struct CommandResponder;

using CommandRunnerFn = std::function<void (MySerial& serial, const String& command, const CommandResponder& responder)>;
using ResponseProcessorFn = std::function<void(const String&, CommandRunnerFn)>;
//using TimeoutProcessorFn = std::function<bool(const String&, CommandRunnerFn)>;
  
struct CommandResponder
{
  ResponseProcessorFn response_processor = nullptr;
  //TimeoutProcessorFn timeout_processor = nullptr;
 // unsigned long timeout = 1000;
};


class CommandManager
{
  struct Command {
    Command(){}
    //Command(MySerial* serial, const String& command, ResponseProcessorFn response_processor) :serial(serial),command(command),responder(CommandResponder(response_processor)) {}
    Command(MySerial* serial, const String& command, const CommandResponder& responder) :serial(serial),command(command),responder(responder){}
    MySerial* serial = nullptr;
    String command;
    CommandResponder responder;
  };
  using OptionalCommand = std::pair<Command, bool>;
  
  Command current_command;
  bool current_command_present = false;
 
  std::queue<Command> command_queue;
   
public:
  void enqueue(MySerial& serial, const String& command, const CommandResponder& responder) {
    command_queue.push({&serial, command, responder});
  }

  void enqueue(MySerial& serial, const String& command, ResponseProcessorFn response_processor) {
    enqueue(serial, command, { response_processor } );
  }

  bool process_response(MySerial& serial, const String& response)
  {
    if (current_command_present && (&serial == current_command.serial)) {
      current_command_present = false;
      current_command.responder.response_processor(response, [this, &serial](MySerial& serial_, const String& command_, const CommandResponder& responder_) {
        assert(!this->current_command_present);
        this->current_command = Command(&serial_, command_, responder_);
        if (command_.length()==0) {
          assert(&serial == &serial_);
        } else {
          serial_.println(command_);
        }
        this->current_command_present = true;
      });
    }
  }
  
  void update(unsigned long timestamp, unsigned long delta)
  {
    if (current_command_present) {
      // process timeouts
    } else if (command_queue.size()) {
      current_command = command_queue.front();
      command_queue.pop();
      current_command_present = true;
      current_command.serial->println(current_command.command);
    }
  }
};

