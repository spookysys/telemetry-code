#pragma once
#include "common.hpp"
#include "MyRingBuffer.hpp"
#include <HardwareSerial.h>
#include <deque>


class Logger;
class SERCOM;
class MySerial : public HardwareSerial
{
  Logger& logger;
  
  SERCOM* sercom = nullptr;

  // rxBuffer
  MyRingBuffer<1024> rxBuffer;
  static const int rts_rx_margin = 10;

  // handshaking
  bool handshakeEnabled = false;
  uint8_t pinRTS, pinCTS;
  bool curRts;
  void updateRts();

public:
  MySerial(const char* id);

  void begin(unsigned long) {
    assert(0);
  }
  
  void begin(unsigned long baudrate, uint16_t config) {
    assert(0);
  }

  void begin(unsigned long baudrate, uint8_t pinRX, uint8_t pinTX, _EPioType pinTypeRX, _EPioType pinTypeTX, SercomRXPad padRX, SercomUartTXPad padTX, SERCOM* sercom);
  void begin_hs(unsigned long baudrate, uint8_t pinRX, uint8_t pinTX, uint8_t pinRTS, uint8_t pinCTS, _EPioType pinTypeRX, _EPioType pinTypeTX, _EPioType pinTypeRTS, _EPioType pinTypeCTS, SercomRXPad padRX, SercomUartTXPad padTX, SERCOM* sercom);
  
  void end();

  void flush();

  void IrqHandler();

  int available();
  int availableForWrite();
  int peek();
  int read();
  size_t write(const uint8_t data);
  using Print::write; // pull in write(str) and write(buf, size) from Print
  
  operator bool() {
    return true;
  }

  bool hasString() {
    return rxBuffer.has_string();
  }

  String popString() {
    auto tmp = rxBuffer.pop_string();
    updateRts();
    return tmp;
  }

};



