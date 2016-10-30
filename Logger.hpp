#pragma once
#include "common.hpp"


class Logger
{
  Serial_* serial = nullptr;
public:
  Logger()
  {}
  
  void begin(Serial_* serial) {
    this->serial = serial;
    if (serial)
      while(!*serial);
  }

  template<typename... Args>
  void print(Args&&... args)
  {
    if (serial)
      serial->print(std::forward<Args>(args)...);
  }

  template<typename... Args>
  void println(Args&&... args)
  {
    if (serial)
      serial->println(std::forward<Args>(args)...);
  }  

  template<typename... Args>
  void write(Args&&... args)
  {
    if (serial)
      serial->write(std::forward<Args>(args)...);
  }

  void flush()
  {
    if (serial)
      serial->flush();
  }
};

extern Logger logger;



