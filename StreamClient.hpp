#pragma once
#include <functional>
#include "AsyncTasks.hpp"

class StreamClient
{
  struct Job : public AsyncTask {
    String cmd;
    std::function<void (const String& msg)> msg_handler;
    std::function<
  };
  
};


