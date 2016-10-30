#include "GsmSerial.hpp"
#include "Logger.hpp"
#include "wiring_private.h" // pinPeripheral() function
#undef min
#undef max
#include <array>

// ring buffer for serials
template<int S=64, typename T=uint8_t>
class MyRingBuffer
{
private:
  std::array<T, S> _aucBuffer;
  int _iHead ;
  int _iTail ;
public:
  MyRingBuffer()
  {
    memset( _aucBuffer.data(), 0, S);
    clear();
  }
  void store_char(T c)
  {
    int i = nextIndex(_iHead);
    //assert(i != _iTail);
    _aucBuffer[_iHead] = c ;
    _iHead = i ;
  }
  void clear()
  {
    _iHead = 0;
    _iTail = 0;
  }

  int read_char()
  {
    if(_iTail == _iHead)
      return -1;
  
    uint8_t value = _aucBuffer[_iTail];
    _iTail = nextIndex(_iTail);
  
    return value;
  }
  
  int available()
  {
    int delta = _iHead - _iTail;  
    if(delta < 0)
      return S + delta;
    else
      return delta;
  }
  int peek()
  {
    if(_iTail == _iHead)
      return -1;
  
    return _aucBuffer[_iTail];
  }
  bool isFull()
  {
    return (nextIndex(_iHead) == _iTail);
  }
private:
  int nextIndex(int index)
  {
    return (uint32_t)(index + 1) % S;
  }
};


namespace {
  const auto baudrate = 9600;
  const uint8_t pinRX = 3ul; // PA09 // SERCOM2.1 // Their GSM_TX
  const uint8_t pinTX  = 4ul; // PA08 // SERCOM2.0 // Their GSM_RX
  const auto pinTypeRX = PIO_SERCOM_ALT;
  const auto pinTypeTX = PIO_SERCOM_ALT;
  //const uint8_t pinRTS = 2ul; // PA14 // SERCOM2.2 
  //const uint8_t pinCTS = 5ul; // PA15 // SERCOM2.3 
  //const auto pinTypeRTS = PIO_SERCOM;
  //const auto pinTypeCTS = PIO_SERCOM;
  const SercomRXPad padRX = SERCOM_RX_PAD_1; // Use pad 1 for RX
  const SercomUartTXPad padTX = UART_TX_PAD_0; // UART_TX_PAD_0 or UART_TX_RTS_CTS_PAD_0_2_3  
  SERCOM* sercom = &sercom2;
  MyRingBuffer<512> rxBuffer;
}

void GsmSerial::begin()
{
  pinPeripheral(pinRX, pinTypeRX);
  pinPeripheral(pinTX, pinTypeTX);
  //pinPeripheral(pinRTS, pinTypeRTS);
  //pinPeripheral(pinCTS, pinTypeCTS);  
  sercom->initUART(UART_INT_CLOCK, SAMPLE_RATE_x16, baudrate);
  sercom->initFrame(UART_CHAR_SIZE_8_BITS, LSB_FIRST, SERCOM_NO_PARITY, SERCOM_STOP_BIT_1);
  sercom->initPads(padTX, padRX);
  sercom->enableUART();
}

void GsmSerial::end()
{
  sercom->resetUART();
  rxBuffer.clear();
}

void GsmSerial::flush()
{
  sercom->flushUART();    
}

void GsmSerial::IrqHandler()
{
  if (sercom->availableDataUART()) {
    auto tmp = sercom->readDataUART();
    logger.write(tmp);
    if (rxBuffer.isFull()) 
      logger.println("GSM RX Buffer full!");
    else
      rxBuffer.store_char(tmp);
  }
  if (sercom->isUARTError()) {
    sercom->acknowledgeUARTError();
    logger.println("GsmSerial Error Detected");
    // TODO: if (sercom->isBufferOverflowErrorUART()) ....
    // TODO: if (sercom->isFrameErrorUART()) ....
    // TODO: if (sercom->isParityErrorUART()) ....
    sercom->clearStatusUART();
  }
}

int GsmSerial::available()
{
  return rxBuffer.available();
}

int GsmSerial::availableForWrite()
{
  return (sercom->isDataRegisterEmptyUART() ? 1 : 0);
}

int GsmSerial::peek()
{
  return rxBuffer.peek();
}

int GsmSerial::read()
{
  return rxBuffer.read_char();
}

size_t GsmSerial::write(const uint8_t data)
{
  logger.write(data);
  sercom->writeDataUART(data);
  return 1;
}

int GsmSerial::findMulti(const char* strings[], const int tNum) {
  Stream::MultiTarget targets[tNum];
  for (int i=0; i<tNum; i++) {
    targets[i].str = strings[i];
    targets[i].len = strlen(strings[i]);
    targets[i].index = 0;
  }
  return Stream::findMulti(targets, tNum);
}
  


GsmSerial gsmSerial;

void SERCOM2_Handler()
{
  gsmSerial.IrqHandler();
}

