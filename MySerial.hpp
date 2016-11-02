#pragma once
#include "common.hpp"
#include "Logger.hpp"
#include "MyRingBuffer.hpp"
#include <HardwareSerial.h>
#include <deque>



class SERCOM;
class MySerial : public HardwareSerial
{
    SERCOM* sercom = nullptr;
    const char* name = nullptr;

    // rxBuffer
    MyRingBuffer<1024> rxBuffer;

    // handshaking
    bool handshakeEnabled = false;
    uint8_t pinRTS, pinCTS;
    bool curRts;
    void updateRts();

  public:
    void begin(unsigned long) {
      assert(0);
    }
    
    void begin(unsigned long baudrate, uint16_t config) {
      assert(0);
    }
    
    void begin(const char* name, unsigned long baudrate, uint8_t pinRX, uint8_t pinTX, _EPioType pinTypeRX, _EPioType pinTypeTX, SercomRXPad padRX, SercomUartTXPad padTX, SERCOM* sercom);
    void begin_hs(const char* name, unsigned long baudrate, uint8_t pinRX, uint8_t pinTX, uint8_t pinRTS, uint8_t pinCTS, _EPioType pinTypeRX, _EPioType pinTypeTX, SercomRXPad padRX, SercomUartTXPad padTX, SERCOM* sercom);
    
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


public:
    // Functions for dealing with callbacks

    bool (*callback)(const String& str) = nullptr;

    // wait for string, return 0 if timed out
    String readln(int timeout=1000)
    {
      logger.println("I am in readln");
      int delta = (timeout+16)>>4;
      timeout = delta<<4;
      for (int i=timeout-1; i>=0; i-=delta) {
        while (hasString()) {
          String str = popString();
          logger.print("Popped: ");
          logger.println(str);
          if (!callback(str)) {
            logger.println("(returned)");
            return str;
          }
        }
        if (i>0) {
          logger.print("Waiting ");
          logger.println(delta);
          delay(delta);
        }
      }
      return "";
    }

    // process callbacks, ignoring unexpected strings
    void processCallbacks()
    {
      while (hasString()) {
        String str = popString();
        if (!callback(str)) {
          logger.print(name);
          logger.print(" unexpected: \"");
          logger.print(str);
          logger.println("\"");
        }
      }
    }
};



