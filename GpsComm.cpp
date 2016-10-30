#include "GpsComm.hpp"
#include "Logger.hpp"
#include "wiring_private.h" // pinPeripheral() function

namespace 
{
  TinyGPSPlus tinyGps;
}


class GpsSerial : public HardwareSerial
{
  static const auto baudrate = 115200;
  static const auto pinRX = 31ul; // PB23 // SERCOM5.3 // Their GPS_TX
  static const auto pinTX = 30ul; // PB22 // SERCOM5.2 // Their GPS_RX
  static const auto pinTypeRX = PIO_SERCOM_ALT;
  static const auto pinTypeTX = PIO_SERCOM_ALT;
  static const auto padRX = SERCOM_RX_PAD_3;
  static const auto padTX = UART_TX_PAD_2;
  SERCOM* sercom = &sercom5;
public:
  void begin(unsigned long) { assert(0); }
  void begin(unsigned long baudrate, uint16_t config) { assert(0); }
  void begin()
  {
    pinPeripheral(pinRX, pinTypeRX);
    pinPeripheral(pinTX, pinTypeTX); 
    sercom->initUART(UART_INT_CLOCK, SAMPLE_RATE_x16, baudrate);
    sercom->initFrame(UART_CHAR_SIZE_8_BITS, LSB_FIRST, SERCOM_NO_PARITY, SERCOM_STOP_BIT_1);
    sercom->initPads(padTX, padRX);
    sercom->enableUART();
  }
  void end()
  {
    sercom->resetUART();
    //rxBuffer.clear();
  }
  void flush()
  {
    sercom->flushUART();    
  }
  void IrqHandler()
  {
    if (sercom->availableDataUART()) {
      auto tmp = sercom->readDataUART();
      logger.write(tmp);
      tinyGps.encode(tmp);
    }
    if (sercom->isUARTError()) {
      sercom->acknowledgeUARTError();
      logger.println();
      logger.print("GPS-ERR: ");
      if (sercom->isBufferOverflowErrorUART()) logger.print("bufferoverflow");
      if (sercom->isFrameErrorUART()) logger.print("frame");
      if (sercom->isParityErrorUART()) logger.print("parity");
      logger.println();
      sercom->clearStatusUART();
    }
  }
  int available()
  {
    return 0;//return rxBuffer.available();
  }
  int availableForWrite()
  {
    return (sercom->isDataRegisterEmptyUART() ? 1 : 0);
  }
  int peek()
  {
    return -1;//return rxBuffer.peek();
  }
  int read()
  {
    return -1;//return rxBuffer.read_char();
  }
  size_t write(const uint8_t data)
  {
    logger.write(data);
    sercom->writeDataUART(data);
    return 1;
  }
  using Print::write; // pull in write(str) and write(buf, size) from Print

  operator bool() { return true; }

} gpsSerial;

void SERCOM5_Handler()
{
  gpsSerial.IrqHandler();
}


void GpsComm::begin()
{
  gpsSerial.begin();
}

TinyGPSPlus& GpsComm::state()
{
  return tinyGps;
}

const TinyGPSPlus& GpsComm::state() const
{
  return tinyGps;
}

GpsComm gpsComm;


