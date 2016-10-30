#pragma once
#include "common.hpp"


class Logger : public Print
{
  Serial_* serial = nullptr;
public:
  Logger()
  {}
  
  void begin(Serial_* serial) {
    this->serial = serial;
  }

  virtual size_t write(uint8_t op)
  {
    if (serial)
      serial->write(op);
  }
  virtual size_t write(const uint8_t *buffer, size_t size)
  {
    if (serial)
      serial->write(buffer, size);
  }
  virtual void flush() 
  {
    if (serial)
      serial->flush();
  }

  using Print::write; // pull in write(str) and write(buf, size) from Print


  /*  
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
  */
};

extern Logger logger;



