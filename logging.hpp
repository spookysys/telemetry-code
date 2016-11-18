#pragma once
#include "common.hpp"
#include <SD.h>

class Logger : public Print {
public:
  String id;
  Logger(const String& id) : id(id) {}
  virtual size_t write(uint8_t ch) = 0;
  virtual size_t write(const uint8_t *buffer, size_t size) = 0;
  virtual void flush() = 0;
};

namespace logging
{
  void begin();
  Logger& get(const String& id);
  void setLogfile(File* file);
}


