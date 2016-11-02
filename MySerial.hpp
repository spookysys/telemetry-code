#pragma once
#include "common.hpp"
#include "MyRingBuffer.hpp"
#include <HardwareSerial.h>
#include <deque>

class SERCOM;
class MySerial : public HardwareSerial
{
  MyRingBuffer<1024, uint8_t> rxBuffer;
  SERCOM* sercom = nullptr;
  const char* name = nullptr;
  bool handshakeEnabled = false;
  uint8_t pinRTS, pinCTS;
  bool curRts;
  void updateRts();
public:
  void begin(unsigned long) { assert(0); }
  void begin(unsigned long baudrate, uint16_t config) { assert(0); }
  void begin(const char* name, unsigned long baudrate, uint8_t pinRX, uint8_t pinTX, _EPioType pinTypeRX, _EPioType pinTypeTX, SercomRXPad padRX, SercomUartTXPad padTX, SERCOM* sercom);
  void enableHandshaking(uint8_t pinRTS, uint8_t pinCTS);
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

  bool contains(char ch);
  int findEither(const char* strings[]);
  int findAll(const char* strings[]);
};



