#include <assert.h>
#include <Wire.h>
#include <Uart.h>
#include "wiring_private.h" // pinPeripheral() function

#undef min
#undef max
#include <utility>
#include <vector>


#define DEBUG_PRINT

 



namespace misc 
{
  static const auto VBAT_PIN = A7;
  float readBatteryVoltage()
  {
    const float refVolt = 3.0f;
    return analogRead(VBAT_PIN)*(2.f*refVolt/1024.f);
  }
}


class Logger
{
public:
  void begin() {
  }

  template<typename... Args>
  void print(Args&&... args)
  {
     Serial.print(std::forward<Args>(args)...);
  }

  template<typename... Args>
  void println(Args&&... args)
  {
     Serial.println(std::forward<Args>(args)...);
  }  

  template<typename... Args>
  void write(Args&&... args)
  {
     Serial.write(std::forward<Args>(args)...);
  }

  
} logger;




class SerialReceiver
{  
  std::vector<std::pair<long long, String>> received;
  String curStr;
  long long tick=0;
  static const int timeout = 10;
  Uart& serial;
public:
  SerialReceiver(Uart& serial) : serial(serial) {}
  void update()
  {
    while (serial.available()) {
      char ch = serial.read();
      curStr += ch;
      if (ch == '\n') {
        curStr.trim();
        if (curStr.length()>0) {
          logger.print("<");
          logger.println(curStr);
          received.push_back(std::make_pair(timeout, curStr));
          curStr = "";
        }
      }
    }
    for (auto iter=received.begin(); iter!=received.end(); ) 
    {
      if (iter->first-- == 0) {
        logger.print("Timeout: ");
        logger.println(iter->second);
        iter = received.erase(iter);
      } else {
        iter++;
      }
    }
  }

  bool match(String* retStr, String str, bool anchorStart=true, bool anchorEnd=true)
  {
    for (auto iter=received.begin(); iter!=received.end(); iter++) {
      const String& pStr = iter->second;
      
      bool res;
      if (anchorStart && anchorEnd) {
        res = pStr.equals(str);
      } else if (anchorStart) {
        res = pStr.startsWith(str);
      } else if (anchorEnd) {
        res = pStr.endsWith(str);
      } else {
        for (int i=0; i<pStr.length(); i++) {
          if (pStr.startsWith(str, i)) res = true;
        }
      }
          
      if (res) {
        if (retStr) *retStr = pStr;
        logger.print("Found: ");
        logger.println(pStr);
        received.erase(iter);
        return true;
      }
    }
    return false;
  }
};




namespace gsm {
  static const auto BAUD = 9600;
  static const auto PIN_RX = 3ul; // PA09 // SERCOM2.1 // Their GSM_TX
  static const auto PIN_TX = 4ul; // PA08 // SERCOM2.0 // Their GSM_RX
  static const auto PIN_RTS = 2ul; // PA14 // SERCOM2.2 
  static const auto PIN_CTS = 5ul; // PA15 // SERCOM2.3 
  static const auto PAD_RX = SERCOM_RX_PAD_1; // Use pad 1 for RX
  static const auto PAD_TX = UART_TX_RTS_CTS_PAD_0_2_3; // UART_TX_PAD_0 or UART_TX_RTS_CTS_PAD_0_2_3  

  Uart serial(&sercom2, PIN_RX, PIN_TX, PAD_RX, PAD_TX);
  SerialReceiver receiver(serial);

  void begin() {
    logger.println("GSM?");

    // bring up serial
    serial.begin(BAUD);
    pinPeripheral(PIN_TX, PIO_SERCOM_ALT);
    pinPeripheral(PIN_RX, PIO_SERCOM_ALT);
    pinPeripheral(PIN_RTS, PIO_SERCOM);
    pinPeripheral(PIN_CTS, PIO_SERCOM);
    while (!serial);

    // calibrate baud
    while(1) {
      logger.println(">AT");
      serial.println("AT");
      delay(100); 
      receiver.update();
      if (receiver.match(nullptr, "OK", true, true)) 
        break;
    }

    // wait for "RDY"
    while(1) {
      delay(100); 
      receiver.update();
      if (receiver.match(nullptr, "RDY", true, true)) 
        break;
    }
    
    logger.println("GSM!");
  }
}
void SERCOM2_Handler()
{
  gsm::serial.IrqHandler();
}




class SimCom
{
  static const auto PIN_STATUS = 25ul;
  static const auto PIN_PWRKEY = 38ul;
public:
  bool isOn() {
    return digitalRead(PIN_STATUS);
  }

  void powerOnOff() {
      int startStatus = isOn();
      if (startStatus) {
        logger.println("SimCom is on - turning off");
      } else {
        logger.println("SimCom is off - turning on");
      }
      pinMode(PIN_PWRKEY, OUTPUT);
      digitalWrite(PIN_PWRKEY, LOW);
      delay(1000);
      while(isOn()==startStatus) {
        delay(100);
      }
      digitalWrite(PIN_PWRKEY, HIGH);
      pinMode(PIN_PWRKEY, INPUT);
      delay(100);
      int stopStatus = isOn();
      if (stopStatus) {
        logger.println("SimCom is now on");
      } else {
        logger.println("SimCom is now off");
      }
      assert(startStatus!=stopStatus);
  }
  
  void begin()
  {
    pinMode(PIN_STATUS, INPUT_PULLDOWN);
    pinMode(PIN_PWRKEY, INPUT);
    logger.println("SimCom?");
 
    // turn on SimCom (cycle power if already on)
    if (isOn())
      powerOnOff();
    assert(!isOn());
    powerOnOff();
    assert(isOn());

    gsm::begin();
   
    logger.println("SimCom!");
  }
 
  
} simCom;



void setup() {
  Serial.begin(74880);
  while(!Serial);
  logger.println("Setup?");
  
  logger.begin();
  simCom.begin();
  
  logger.println("Setup!");
}



void loop() {
  logger.print("VBat: " ); 
  logger.println(misc::readBatteryVoltage());
  gsm::receiver.update();
  delay(1000);
}
