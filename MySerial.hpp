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
  Logger& logger_rx;
  bool echo_tx;
  bool echo_rx;
  
  SERCOM* sercom = nullptr;

  // rx_buffer
  MyRingBuffer<1024> rx_buffer;
  const int rts_rx_stop = rx_buffer.capacity() - 10;
  const int rts_rx_cont = rx_buffer.capacity() - 20;

  // handshaking
  bool handshakeEnabled = false;
  uint8_t pinRTS, pinCTS;
  bool curRts;
  void updateRts();

public:
  MySerial(const char* id, bool echo_tx, bool echo_rx);

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
    return rx_buffer.has_string();
  }

  String popString() {
    auto tmp = rx_buffer.pop_string();
    updateRts();
    return tmp;
  }

};



