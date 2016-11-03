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

void MySerial::begin_hs(const char* name, unsigned long baudrate, uint8_t pinRX, uint8_t pinTX, uint8_t pinRTS, uint8_t pinCTS, _EPioType pinTypeRX, _EPioType pinTypeTX, _EPioType pinTypeRTS, _EPioType pinTypeCTS, SercomRXPad padRX, SercomUartTXPad padTX, SERCOM* sercom)
{
  pinPeripheral(pinRTS, pinTypeRTS);
  pinPeripheral(pinCTS, pinTypeCTS);
  this->begin(name, baudrate, pinRX, pinTX, pinTypeRX, pinTypeTX, padRX, padTX, sercom);
  this->handshakeEnabled = true;
  this->pinRTS = pinRTS;
  this->pinCTS = pinCTS;
  pinMode(pinRTS, OUTPUT);
  pinMode(pinCTS, INPUT);
  this->curRts = false; // false means we can receive
  digitalWrite(pinRTS, this->curRts);
}


void MySerial::updateRts()
{
  if (!handshakeEnabled) return;
  
  bool changed = false;
  if (curRts==false && rxBuffer.available() >= rxBuffer.capacity()*2/3) 
  {
    curRts = true;
    changed = true;
  } 
  else if (curRts==true && rxBuffer.available() <= rxBuffer.capacity()*1/3)
  { 
    curRts = false;
    changed = true;
  }

  if (changed)
    digitalWrite(pinRTS, curRts);
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
    //logger.write(tmp);
    if (rxBuffer.is_full()) {
      assert(!"RX Buffer full!");
    } else {
      rxBuffer.push(tmp);
      updateRts();
    }
  }
  
  if (sercom->isUARTError()) {
    sercom->acknowledgeUARTError();
    logger.print(name);
    logger.println(" Error");
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
  //if (handshakeEnabled && digitalRead(pinCTS)) return false;
  return (sercom->isDataRegisterEmptyUART() ? 1 : 0);
}

int MySerial::peek()
{
  return rxBuffer.peek();
}

int MySerial::read()
{
  if (available()==0) return -1;
  char ch = rxBuffer.pop();
  updateRts();
  return ch;
}

size_t MySerial::write(const uint8_t data)
{
  logger.write(data);
  //while (!availableForWrite());
  sercom->writeDataUART(data);
  return 1;
}




/*
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
*/
