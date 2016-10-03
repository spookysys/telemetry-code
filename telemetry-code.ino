// NOTE: leftover code from arduino zero is hogging sercom5, which we need. Comment this out from variants.cpp to fix.
#include <TinyGPS++.h>
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



// Allow out-of-order processing of received messages and stuff
class SerialComm
{  
  std::vector<std::pair<long long, String>> received;
  String curStr;
  long long tick=0;
  static const int timeout = 10;
  Uart& my_serial;
  bool start_of_line = true;
public:
  SerialComm(Uart& my_serial) : my_serial(my_serial) {}

  void IrqHandler() {
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
        logger.print("Purged: ");
        logger.println(iter->second);
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
        logger.print("Match: ");
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
    if (start_of_line) logger.print(">");
    logger.print(std::forward<Args>(args)...);
    my_serial.print(std::forward<Args>(args)...);
    start_of_line = false;
  }

  template<typename... Args>
  void println(Args&&... args)
  {
    if (start_of_line) logger.print(">");
    logger.println(std::forward<Args>(args)...);
    my_serial.println(std::forward<Args>(args)...);
    start_of_line = true;
  }  

  template<typename... Args>
  void write(Args&&... args)
  {
    if (start_of_line) logger.print(">");
    logger.write(std::forward<Args>(args)...);
    my_serial.write(std::forward<Args>(args)...);
    start_of_line = false;
  }

};




class Gsm : public SerialComm
{
  static const auto BAUD = 9600;
  static const auto PIN_RX = 3ul; // PA09 // SERCOM2.1 // Their GSM_TX
  static const auto PIN_TX = 4ul; // PA08 // SERCOM2.0 // Their GSM_RX
  static const auto PIN_RTS = 2ul; // PA14 // SERCOM2.2 
  static const auto PIN_CTS = 5ul; // PA15 // SERCOM2.3 
  static const auto PAD_RX = SERCOM_RX_PAD_1; // Use pad 1 for RX
  static const auto PAD_TX = UART_TX_RTS_CTS_PAD_0_2_3; // UART_TX_PAD_0 or UART_TX_RTS_CTS_PAD_0_2_3  
  Uart ss;
public:
  Gsm() : ss(&sercom2, PIN_RX, PIN_TX, PAD_RX, PAD_TX), SerialComm(ss) {}
  
  void begin() {
    logger.println("GSM?");

    // bring up serial
    ss.begin(BAUD);
    pinPeripheral(PIN_TX, PIO_SERCOM_ALT);
    pinPeripheral(PIN_RX, PIO_SERCOM_ALT);
    pinPeripheral(PIN_RTS, PIO_SERCOM);
    pinPeripheral(PIN_CTS, PIO_SERCOM);
    while (!ss);

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
} gsm;

void SERCOM2_Handler()
{
  gsm.IrqHandler();
}




class Gps : public TinyGPSPlus
{
  static const auto BAUD = 115200;
  static const auto PIN_EN = 26; // PA27
  static const auto PIN_RX = 31; // PB23 // SERCOM5.3 // Their GPS_TX
  static const auto PIN_TX = 30; // PB22 // SERCOM5.2 // Their GPS_RX
  static const auto PAD_RX = SERCOM_RX_PAD_3;
  static const auto PAD_TX = UART_TX_PAD_2;
  Uart ss;
public:
  Gps() : TinyGPSPlus(), ss(&sercom5, PIN_RX, PIN_TX, PAD_RX, PAD_TX){}

  void IrqHandler() {
    ss.IrqHandler();
  }

  void begin() {
    logger.println("GPS?");

    // Enable GNSS
    pinMode(PIN_EN, INPUT_PULLUP);
    delay(1000);
    pinMode(PIN_EN, INPUT_PULLDOWN);
    delay(1000);
    pinMode(PIN_EN, INPUT_PULLUP);
    delay(1000);
    
    // Bring up serial
    ss.begin(BAUD);
    pinPeripheral(PIN_TX, PIO_SERCOM_ALT);
    pinPeripheral(PIN_RX, PIO_SERCOM_ALT);
    while (!ss);

    // Enable GNSS
    pinMode(PIN_EN, INPUT_PULLUP);
    delay(1000);
    pinMode(PIN_EN, INPUT_PULLDOWN);
    delay(1000);
    pinMode(PIN_EN, INPUT_PULLUP);
    delay(1000);
    
    logger.println("GPS!");
  }

  void update() {
    while (ss.available()) {
      char ch = ss.read();
      logger.print(ss);
      encode(ch);
    }
  }

  void dumpAll()
  {
    logger.println(location.lat(), 6); // Latitude in degrees (double)
    logger.println(location.lng(), 6); // Longitude in degrees (double)
    logger.print(location.rawLat().negative ? "-" : "+");
    logger.println(location.rawLat().deg); // Raw latitude in whole degrees
    logger.println(location.rawLat().billionths);// ... and billionths (u16/u32)
    logger.print(location.rawLng().negative ? "-" : "+");
    logger.println(location.rawLng().deg); // Raw longitude in whole degrees
    logger.println(location.rawLng().billionths);// ... and billionths (u16/u32)
    logger.println(date.value()); // Raw date in DDMMYY format (u32)
    logger.println(date.year()); // Year (2000+) (u16)
    logger.println(date.month()); // Month (1-12) (u8)
    logger.println(date.day()); // Day (1-31) (u8)
    logger.println(time.value()); // Raw time in HHMMSSCC format (u32)
    logger.println(time.hour()); // Hour (0-23) (u8)
    logger.println(time.minute()); // Minute (0-59) (u8)
    logger.println(time.second()); // Second (0-59) (u8)
    logger.println(time.centisecond()); // 100ths of a second (0-99) (u8)
    logger.println(speed.value()); // Raw speed in 100ths of a knot (i32)
    logger.println(speed.knots()); // Speed in knots (double)
    logger.println(speed.mph()); // Speed in miles per hour (double)
    logger.println(speed.mps()); // Speed in meters per second (double)
    logger.println(speed.kmph()); // Speed in kilometers per hour (double)
    logger.println(course.value()); // Raw course in 100ths of a degree (i32)
    logger.println(course.deg()); // Course in degrees (double)
    logger.println(altitude.value()); // Raw altitude in centimeters (i32)
    logger.println(altitude.meters()); // Altitude in meters (double)
    logger.println(altitude.miles()); // Altitude in miles (double)
    logger.println(altitude.kilometers()); // Altitude in kilometers (double)
    logger.println(altitude.feet()); // Altitude in feet (double)
    logger.println(satellites.value()); // Number of satellites in use (u32)
    logger.println(hdop.value()); // Horizontal Dim. of Precision (100ths-i32)
  }

} gps;

void SERCOM5_Handler()
{
  gps.IrqHandler();
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
   
    logger.println("SimCom!");
  }
 
  
} simCom;



void setup() {
  Serial.begin(74880);
  while(!Serial);
  logger.println("Setup?");
  
  logger.begin();
  simCom.begin();
  gps.begin();
  gsm.begin();

  
  logger.println("Setup!");
}



void loop() {
  gsm.update();
  gps.update();
  //gps.dumpAll();
  logger.print("VBat: " ); 
  logger.println(misc::readBatteryVoltage());
  logger.print("Ram: ");
  logger.println(misc::freeRam());
  delay(1000);
}
