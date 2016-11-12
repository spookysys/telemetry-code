#include "MySerial.hpp"
#include "logging.hpp"
#include "wiring_private.h" // pinPeripheral() function

MySerial::MySerial(const char* id, bool echo_tx, bool echo_rx) : logger(logging::get(String(id)+"-tx")), logger_rx(logging::get(String(id)+"-rx")), echo_tx(echo_tx), echo_rx(echo_rx) {}


void MySerial::begin(unsigned long baudrate, uint8_t pinRX, uint8_t pinTX, _EPioType pinTypeRX, _EPioType pinTypeTX, SercomRXPad padRX, SercomUartTXPad padTX, SERCOM* sercom)
{
  this->sercom = sercom;
  pinPeripheral(pinRX, pinTypeRX);
  pinPeripheral(pinTX, pinTypeTX);
  sercom->initUART(UART_INT_CLOCK, SAMPLE_RATE_x16, baudrate);
  sercom->initFrame(UART_CHAR_SIZE_8_BITS, LSB_FIRST, SERCOM_NO_PARITY, SERCOM_STOP_BIT_1);
  sercom->initPads(padTX, padRX);
  sercom->enableUART();
}

void MySerial::begin_hs(unsigned long baudrate, uint8_t pinRX, uint8_t pinTX, uint8_t pinRTS, uint8_t pinCTS, _EPioType pinTypeRX, _EPioType pinTypeTX, _EPioType pinTypeRTS, _EPioType pinTypeCTS, SercomRXPad padRX, SercomUartTXPad padTX, SERCOM* sercom)
{
  pinPeripheral(pinRTS, pinTypeRTS);
  pinPeripheral(pinCTS, pinTypeCTS);
  this->begin(baudrate, pinRX, pinTX, pinTypeRX, pinTypeTX, padRX, padTX, sercom);
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
  if (curRts==false && rx_buffer.available() >= rts_rx_stop) 
  {
    if (echo_rx) logger_rx.write('!');
    curRts = true;
    changed = true;
  } 
  else if (curRts==true && rx_buffer.available() <= rts_rx_cont)
  { 
    if (echo_rx) logger_rx.write('-');
    curRts = false;
    changed = true;
  }

  if (changed)
    digitalWrite(pinRTS, curRts);
}

void MySerial::end()
{
  sercom->resetUART();
  rx_buffer.clear();
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
    if (echo_rx) logger_rx.write(tmp);
    if (rx_buffer.is_full()) {
      if (echo_rx) logger.write('§');
      //logger.print("_rxOF_");
    } else {
      rx_buffer.push(tmp);
      updateRts();
    }
  }
  
  if (sercom->isUARTError()) {
    sercom->acknowledgeUARTError();
    if (echo_rx) logger.write('%');
    //logger.print("_rxER_");
    // TODO: if (sercom->isBufferOverflowErrorUART()) ....
    // TODO: if (sercom->isFrameErrorUART()) ....
    // TODO: if (sercom->isParityErrorUART()) ....
    sercom->clearStatusUART();
  }

}

int MySerial::available()
{
  return rx_buffer.available(); 
}

int MySerial::availableForWrite()
{
  //if (handshakeEnabled && digitalRead(pinCTS)) return false;
  return (sercom->isDataRegisterEmptyUART() ? 1 : 0);
}

int MySerial::peek()
{
  return rx_buffer.peek();
}

int MySerial::read()
{
  if (available()==0) return -1;
  char ch = rx_buffer.pop();
  updateRts();
  return ch;
}

size_t MySerial::write(const uint8_t data)
{
  if (echo_tx) logger.write(data);
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
