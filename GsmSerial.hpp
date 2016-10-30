#pragma once
#include "common.hpp"
#include <HardwareSerial.h>

class GsmSerial : public HardwareSerial
{
public:
  void begin(unsigned long) { assert(0); }
  void begin(unsigned long baudrate, uint16_t config) { assert(0); }
  void begin();
  void end();
  void flush();
  void IrqHandler();
  int available();
  int availableForWrite();
  int peek();
  int read();
  size_t write(const uint8_t data);
  using Print::write; // pull in write(str) and write(buf, size) from Print
  operator bool() { return true; }
  int findMulti(const char* strings[], const int tNum);
};

extern GsmSerial gsmSerial;


