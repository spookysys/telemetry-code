#include "MySerial.hpp"
#include "Logger.hpp"
#include "wiring_private.h" // pinPeripheral() function



void MySerial::begin(const char* name, unsigned long baudrate, uint8_t pinRX, uint8_t pinTX, _EPioType pinTypeRX, _EPioType pinTypeTX, SercomRXPad padRX, SercomUartTXPad padTX, SERCOM* sercom)
{
  this->name = name;
  this->sercom = sercom;
  pinPeripheral(pinRX, pinTypeRX);
  pinPeripheral(pinTX, pinTypeTX);
  sercom->initUART(UART_INT_CLOCK, SAMPLE_RATE_x16, baudrate);
  sercom->initFrame(UART_CHAR_SIZE_8_BITS, LSB_FIRST, SERCOM_NO_PARITY, SERCOM_STOP_BIT_1);
  sercom->initPads(padTX, padRX);
  sercom->enableUART();
}

void MySerial::enableHandshaking(uint8_t pinRTS, uint8_t pinCTS)
{
  this->handshakeEnabled = true;
  this->pinRTS = pinRTS;
  this->pinCTS = pinCTS;
  pinMode(pinCTS, INPUT);
  pinMode(pinRTS, OUTPUT);
  this->curRts = false; // false means we can receive
  digitalWrite(pinRTS, this->curRts);
}

void MySerial::updateRts()
{
  if (!handshakeEnabled) return;
  if (curRts==false && rxBuffer.available() >= rxBuffer.capacity()*2/3) 
  {
    //logger.println();
    //logger.println("RTS=1");
    curRts = true;
    digitalWrite(pinRTS, true);
  } 
  else if (curRts==true && rxBuffer.available() <= rxBuffer.capacity()*1/3)
  { 
    //logger.println();
    //logger.println("RTS=0");
    curRts = false;
    digitalWrite(pinRTS, false);
  }
}

void MySerial::end()
{
  sercom->resetUART();
  rxBuffer.clear();
  updateRts();
}

void MySerial::flush()
{
  sercom->flushUART();    
}

void MySerial::IrqHandler()
{
  if (!sercom) return; // not yet inited?
  if (sercom->availableDataUART()) {
    auto tmp = sercom->readDataUART();
    logger.write(tmp);
    if (rxBuffer.isFull()) {
      logger.print(name);
      logger.println(" RX Buffer full!");
    } else {
      rxBuffer.store_char(tmp);
      updateRts();
    }
  }
  if (sercom->isUARTError()) {
    sercom->acknowledgeUARTError();
    //logger.print(name);
    //logger.println(" Error");
    // TODO: if (sercom->isBufferOverflowErrorUART()) ....
    // TODO: if (sercom->isFrameErrorUART()) ....
    // TODO: if (sercom->isParityErrorUART()) ....
    sercom->clearStatusUART();
  }
}

int MySerial::available()
{
  return rxBuffer.available();
}

int MySerial::availableForWrite()
{
  if (handshakeEnabled && digitalRead(pinCTS)) return false;
  return (sercom->isDataRegisterEmptyUART() ? 1 : 0);
}

int MySerial::peek()
{
  return rxBuffer.peek();
}

int MySerial::read()
{
  auto tmp = rxBuffer.read_char();
  updateRts();
  return tmp;
}

size_t MySerial::write(const uint8_t data)
{
  logger.write(data);
  sercom->writeDataUART(data);
  return 1;
}

bool MySerial::contains(char ch)
{
  return rxBuffer.contains('\n');
}

int MySerial::findEither(const char* strings[]) {
  int tNum=0;
  while (strings[tNum]) tNum++;
  Stream::MultiTarget targets[tNum];
  for (int i=0; i<tNum; i++) {
    targets[i].str = strings[i];
    targets[i].len = strlen(strings[i]);
    targets[i].index = 0;
  }
  return Stream::findMulti(targets, tNum);
}
  
int MySerial::findAll(const char* strings[]) {
  assert(!"Not implemented");
}

