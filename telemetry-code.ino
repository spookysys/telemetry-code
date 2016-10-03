// NOTE: leftover code from arduino zero is hogging sercom5, which we need. Comment this out from variants.cpp to fix.
#include <Wire.h>
#include <Uart.h>
#include "wiring_private.h" // pinPeripheral() function

#undef min
#undef max
#include <utility>
#include <vector>

// assert
void assert_handler(const char* expr, const char* file, int line);
#define assert(expr) do { if (!(expr)) { assert_handler(#expr, __FILE__, __LINE__); } } while(0)
    
// debug mode
#define DEBUG

 



extern "C" char *sbrk(int i);
namespace misc 
{
  float readBatteryVoltage()
  {
    static const auto VBAT_PIN = A7;
    static const float ref_volt = 3.0f;
    return analogRead(VBAT_PIN)*(ref_volt*2.f/1024.f);
  }

  int freeRam () 
  {  
    char stack_dummy = 0;  
    return &stack_dummy - sbrk(0); 
  }

}



// TODO: Log to SD and internet
class Logger
{
  bool inited = false;
public:
  bool isInited() 
  {
    return inited;
  }

  void begin() {
    inited = true;
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

  void flush()
  {
    Serial.flush();
  }
} logger;



void assert_handler(const char* expr, const char* file, int line)
{
  if (logger.isInited()) {
    logger.print("Assertion failed: ");
    logger.println(expr);
    logger.print(" in ");
    logger.print(file);
    logger.print(":");
    logger.print(line);
  }
  #ifdef DEBUG
  abort();
  #endif
}





// Allow out-of-order processing of received messages and stuff
class SerialComm
{  
  std::vector<std::pair<long long, String>> received;
  String curStr;
  long long tick=0;
  static const int timeout = 10;
  Uart& my_serial;
  const char* n;
  bool start_of_line = true;
public:
  SerialComm(const char* n, Uart& my_serial) : n(n), my_serial(my_serial) {}

  void IrqHandler()
  {
    my_serial.IrqHandler();
  }
  
  void update()
  {
    while (my_serial.available()) {
      char ch = my_serial.read();
      curStr += ch;
      if (ch == '\n') {
        curStr.trim();
        if (curStr.length()>0) {
          logger.print(n);
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

  void purge(String str, bool anchorEnd=true)
  {
    for (auto iter=received.begin(); iter!=received.end(); ) {
      bool match;
      if (anchorEnd) {
        match = iter->second.equals(str);
      } else {
        match = iter->second.startsWith(str);
      }
      if (match) {
        iter = received.erase(iter);
      } else {
        iter++;
      }
    }
  }

  bool match(String* retStr, String str, bool anchorEnd=true)
  {
    bool found = false;
    for (auto iter=received.begin(); iter!=received.end(); ) {
      bool match;
      if (anchorEnd) {
        match = iter->second.equals(str);
      } else {
        match = iter->second.startsWith(str);
      }

      if (match) {
        if (retStr) *retStr = iter->second;
        logger.print(n);
        logger.print("=");
        logger.println(iter->second);
        if (found) logger.println("Warning: Multiple matches!");
        iter = received.erase(iter);
        found = true;
      } else {
        iter++;
      }
    }
    return found;
  }


  template<typename... Args>
  void print(Args&&... args)
  {
    if (start_of_line) {
      logger.print(n);
      logger.print(">");
    }
    logger.print(std::forward<Args>(args)...);
    my_serial.print(std::forward<Args>(args)...);
    start_of_line = false;
  }

  template<typename... Args>
  void println(Args&&... args)
  {
    if (start_of_line) {
      logger.print(n);
      logger.print(">");
    }
    logger.println(std::forward<Args>(args)...);
    my_serial.println(std::forward<Args>(args)...);
    start_of_line = true;
  }  

  template<typename... Args>
  void write(Args&&... args)
  {
    if (start_of_line) {
      logger.print(n);
      logger.print(">");
    }
    logger.write(std::forward<Args>(args)...);
    my_serial.write(std::forward<Args>(args)...);
    start_of_line = false;
  }

  void flush()
  {
    logger.print(n);
    logger.println(".flush()");
    my_serial.flush();
  }
};




class GsmComm : public SerialComm
{
  static const auto BAUD = 9600;
  static const auto PIN_RX = 3ul; // PA09 // SERCOM2.1 // Their GSM_TX
  static const auto PIN_TX = 4ul; // PA08 // SERCOM2.0 // Their GSM_RX
  static const auto PIN_RTS = 2ul; // PA14 // SERCOM2.2 
  static const auto PIN_CTS = 5ul; // PA15 // SERCOM2.3 
  static const auto PAD_RX = SERCOM_RX_PAD_1; // Use pad 1 for RX
  static const auto PAD_TX = UART_TX_RTS_CTS_PAD_0_2_3; // UART_TX_PAD_0 or UART_TX_RTS_CTS_PAD_0_2_3  
  Uart serial;
public:
  GsmComm() : serial(&sercom2, PIN_RX, PIN_TX, PAD_RX, PAD_TX), SerialComm("gsm", serial) {}
  
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
      println("AT");
      delay(100); 
      update();
      if (match(nullptr, "OK")) 
        break;
    }
    purge("AT");

    // wait for stuff to come up
    int stuffToComeUp = 2;
    while(stuffToComeUp) {
      delay(100); 
      update();
      //if (match(nullptr, "RDY")) stuffToComeUp--;
      //if (match(nullptr, "+CPIN: READY")) stuffToComeUp--;
      //if (match(nullptr, "+CFUN: 1")) stuffToComeUp--;
      if (match(nullptr, "SMS Ready")) stuffToComeUp--;
      if (match(nullptr, "Call Ready")) stuffToComeUp--;
    }
    
    logger.println("GSM!");
  }
} gsmComm;

void SERCOM2_Handler()
{
  gsmComm.IrqHandler();
}




class GpsComm : public SerialComm
{
  static const auto BAUD = 115200;
  static const auto PIN_EN = 26; // PA27
  static const auto PIN_RX = 31; // PB23 // SERCOM5.3 // Their GPS_TX
  static const auto PIN_TX = 30; // PB22 // SERCOM5.2 // Their GPS_RX
  static const auto PAD_RX = SERCOM_RX_PAD_3;
  static const auto PAD_TX = UART_TX_PAD_2;
  Uart serial;
public:
  GpsComm() : serial(&sercom5, PIN_RX, PIN_TX, PAD_RX, PAD_TX), SerialComm("gps", serial) {}
  
  void begin() {
    logger.println("GPS?");

    // Enable GNSS
//    pinMode(PIN_EN, INPUT_PULLDOWN);
//    delay(1000);
//    pinMode(PIN_EN, INPUT_PULLUP);
//    delay(1000);

    // Bring up serial
    serial.begin(BAUD);
    pinPeripheral(PIN_TX, PIO_SERCOM_ALT);
    pinPeripheral(PIN_RX, PIO_SERCOM_ALT);
    while (!serial);


    // Enable GNSS
    delay(1000);
    pinMode(PIN_EN, INPUT_PULLDOWN);
    delay(1000);
    pinMode(PIN_EN, INPUT_PULLUP);
    delay(1000);
          
    // calibrate baud
    /*
    while(1) {
      println("AT");
      delay(100); 
      update();
      if (match(nullptr, "OK")) 
        break;
    }
    purge("AT");
    */

    logger.println("GPS!");
  }
} gpsComm;

void SERCOM5_Handler()
{
  gpsComm.IrqHandler();
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
    delay(800);
    powerOnOff();
    assert(isOn());

    gpsComm.begin();
    gsmComm.begin();
   
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
  logger.print("Ram: ");
  logger.println(misc::freeRam());
  gsmComm.update();
  gpsComm.update();
  //gpsComm.println("AT+CGNSPWR=?");
  delay(1000);
}
