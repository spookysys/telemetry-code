#pragma once
#include "common.hpp"
#include <SD.h>

class Logger {
public:
  String id;
  Logger(const String& id) : id(id) {}
  virtual void println() = 0;
  virtual void println(const String& str) = 0;
  virtual void print(const String& str) = 0;  
  virtual void write(char ch) = 0;
  virtual void flush() = 0;
};

namespace logging
{
  void begin();
  Logger& get(const String& id);
  void setLogfile(File* file);
}


