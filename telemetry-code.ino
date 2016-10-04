// NOTE: leftover code from arduino zero is hogging sercom5, which we need. Comment this out from variants.cpp to fix.
#include <Wire.h>
#include <Uart.h>
#include "wiring_private.h" // pinPeripheral() function
#include <TinyGPS868.h>
#undef min
#undef max
#include <utility>
#include <vector>

// assert
void assert_handler(const char* expr, const char* file, int line);
#define assert(expr) do { if (!(expr)) { assert_handler(#expr, __FILE__, __LINE__); } } while(0)
    
// debug mode
#define DEBUG


#define APN "data.lyca-mobile.no"
#define APN_USER "lmno"
#define APN_PW "plus"


#define OPENWEATHERMAP_API_KEY "18143027801bd9493887c2020cb2968e"



// LEDs
#define PIN_GLED 8



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
public:
  bool isInited() 
  {
    return true;
  }

  void begin() {
    #ifdef DEBUG
    Serial.begin(74880);
    while(!Serial);
    #endif
  }

  template<typename... Args>
  void print(Args&&... args)
  {
    #ifdef DEBUG
    Serial.print(std::forward<Args>(args)...);
    #endif
  }

  template<typename... Args>
  void println(Args&&... args)
  {
    #ifdef DEBUG
    Serial.println(std::forward<Args>(args)...);
    #endif
  }  

  template<typename... Args>
  void write(Args&&... args)
  {
    #ifdef DEBUG
    Serial.write(std::forward<Args>(args)...);
    #endif
  }

  void flush()
  {
    #ifdef DEBUG
    Serial.flush();
    #endif
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
  // blink the line number
  pinMode(PIN_GLED, OUTPUT);
  digitalWrite(PIN_GLED, LOW);
  int digits[4];
  digits[3] = (line / 1000) % 10;
  digits[2] = (line / 100) % 10;
  digits[1] = (line / 10) % 10;
  digits[0] = (line / 1) % 10;
  for (int digit_i=3; digit_i>=0; digit_i--)
  {
    delay(500);
    for (int i=0; i<digits[digit_i]; i++) {
      digitalWrite(PIN_GLED, HIGH);
      delay(200);
      digitalWrite(PIN_GLED, LOW);
      delay(200);
    }
  }
  delay(500);
  // abort if in debug mode
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
  bool start_of_line = true;
public:
  SerialComm(Uart& my_serial) : my_serial(my_serial) {}

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




class GsmComm : public SerialComm
{
  static const auto BAUD = 9600*4;
  static const auto PIN_RX = 3ul; // PA09 // SERCOM2.1 // Their GSM_TX
  static const auto PIN_TX = 4ul; // PA08 // SERCOM2.0 // Their GSM_RX
  static const auto PIN_RTS = 2ul; // PA14 // SERCOM2.2 
  static const auto PIN_CTS = 5ul; // PA15 // SERCOM2.3 
  static const auto PAD_RX = SERCOM_RX_PAD_1; // Use pad 1 for RX
  static const auto PAD_TX = UART_TX_RTS_CTS_PAD_0_2_3; // UART_TX_PAD_0 or UART_TX_RTS_CTS_PAD_0_2_3  
public:
  Uart ss;
  GsmComm() : ss(&sercom2, PIN_RX, PIN_TX, PAD_RX, PAD_TX), SerialComm(ss) {}
  
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

    // Disable echo
    purge("OK");
    println("ATE0");
    while(!match(nullptr, "OK")) { delay(100); update(); }

    // Enable hardware flow control
    purge("OK");
    println("AT+IFC=2,2");
    while(!match(nullptr, "OK")) { delay(100); update(); }
    

    getWeatherData(63,10);
    
    logger.println("GSM!");
  }

  void getWeatherData(int lat, int lon) {
    // activate GPRS
    purge("OK");
    println("AT+CGATT=1");
    while(!match(nullptr, "OK")) { delay(100); update(); }

    purge("OK");
    println("AT+SAPBR=3,1,\"Contype\",\"GPRS\"");
    while(!match(nullptr, "OK")) { delay(100); update(); }

    purge("OK");
    print("AT+SAPBR=3,1,\"APN\",\""); print(APN); println("\"");
    while(!match(nullptr, "OK")) { delay(100); update(); }

    purge("OK");
    println("AT+SAPBR=1,1");
    while(!match(nullptr, "OK")) { delay(100); update(); }

    purge("OK");
    println("AT+HTTPINIT");
    while(!match(nullptr, "OK")) { delay(100); update(); }

    purge("OK");
    println(String("")+"AT+HTTPPARA=\"URL\",\"api.openweathermap.org/data/2.5/weather?lat="+lat+"&lon="+lon+"\"");
    while(!match(nullptr, "OK")) { delay(100); update(); }

    purge("OK");
    println("AT+HTTPACTION=0");
    while(!match(nullptr, "OK")) { delay(100); update(); }
    while(!match(nullptr, "+HTTPACTION", false)) { delay(100); update(); }

    println("AT+HTTPREAD");
    //while(!match(nullptr, "OK")) { delay(100); update(); }
    
    // grab what comes
/*    while(1) {
      while (ss.available())
        logger.println(ss.read());
      delay(10);
    }*/
    // +HTTPREAD: 107
    // {"cod":401, "message": "Invalid API key. Pleamap.org/faq#error401 for more info."}
    // NOTE: Need flow control here!

    
  }
  
} gsmComm;

void SERCOM2_Handler()
{
  gsmComm.IrqHandler();
}


TinyGPSPlus gps;

class GpsComm
{
  static const auto BAUD = 115200;
  static const auto PIN_RX = 31ul; // PB23 // SERCOM5.3 // Their GPS_TX
  static const auto PIN_TX = 30ul; // PB22 // SERCOM5.2 // Their GPS_RX
  static const auto PAD_RX = SERCOM_RX_PAD_3;
  static const auto PAD_TX = UART_TX_PAD_2;
public:
  Uart ss;

  GpsComm() : ss(&sercom5, PIN_RX, PIN_TX, PAD_RX, PAD_TX) {}

  void IrqHandler()
  {
    ss.IrqHandler();
  }
    
  void begin() {
    logger.println("GPS?");

    // Bring up serial
    ss.begin(BAUD);
    pinPeripheral(PIN_TX, PIO_SERCOM_ALT);
    pinPeripheral(PIN_RX, PIO_SERCOM_ALT);
    while (!ss);

    logger.println("GPS!");
  }

  void dumpAll()
  {
    logger.print("Characters processed: ");
    logger.println(gps.charsProcessed());
    logger.print("Sentences with fix: ");
    logger.println(gps.sentencesWithFix());
    logger.print("Failed checksums: ");
    logger.println(gps.failedChecksum());
    logger.print("Passed checksums: ");
    logger.println(gps.passedChecksum());
    if (gps.location.isUpdated()) {
      logger.println("Location:");
      logger.print("lat "); logger.println(gps.location.lat(), 6); // Latitude in degrees (double)
      logger.print("lng "); logger.println(gps.location.lng(), 6); // Longitude in degrees (double)
      logger.print("rawlat "); logger.print(gps.location.rawLat().negative ? "-" : "+");
      logger.println(gps.location.rawLat().deg); // Raw latitude in whole degrees
      logger.println(gps.location.rawLat().billionths);// ... and billionths (u16/u32)
      logger.print("rawlng "); logger.print(gps.location.rawLng().negative ? "-" : "+");
      logger.println(gps.location.rawLng().deg); // Raw longitude in whole degrees
      logger.println(gps.location.rawLng().billionths);// ... and billionths (u16/u32)
    }
    if (gps.date.isUpdated()) {
      logger.println("Date:");
      logger.print("ddmmyy "); logger.println(gps.date.value()); // Raw date in DDMMYY format (u32)
      logger.print("year "); logger.println(gps.date.year()); // Year (2000+) (u16)
      logger.print("month "); logger.println(gps.date.month()); // Month (1-12) (u8)
      logger.print("day "); logger.println(gps.date.day()); // Day (1-31) (u8)
    }
    if (gps.time.isUpdated()) {
      logger.println("Time:");
      logger.print("hhmmsscc "); logger.println(gps.time.value()); // Raw time in HHMMSSCC format (u32)
      logger.print("hour "); logger.println(gps.time.hour()); // Hour (0-23) (u8)
      logger.print("minute "); logger.println(gps.time.minute()); // Minute (0-59) (u8)
      logger.print("second "); logger.println(gps.time.second()); // Second (0-59) (u8)
      logger.print("centisecond "); logger.println(gps.time.centisecond()); // 100ths of a second (0-99) (u8)
    }
    if (gps.speed.isUpdated()) {
      logger.println("Speed:");
      logger.print("100ths knot "); logger.println(gps.speed.value()); // Raw speed in 100ths of a knot (i32)
      logger.print("knots "); logger.println(gps.speed.knots()); // Speed in knots (double)
      logger.print("mph "); logger.println(gps.speed.mph()); // Speed in miles per hour (double)
      logger.print("mps "); logger.println(gps.speed.mps()); // Speed in meters per second (double)
      logger.print("kmph "); logger.println(gps.speed.kmph()); // Speed in kilometers per hour (double)
    }
    if (gps.course.isUpdated()) {
      logger.println("Course:");
      logger.print("100ths of degree "); logger.println(gps.course.value()); // Raw course in 100ths of a degree (i32)
      logger.print("degrees "); logger.println(gps.course.deg()); // Course in degrees (double)
    }
    if (gps.altitude.isUpdated()) {
      logger.println("Altitude:");
      logger.print("centimeters "); logger.println(gps.altitude.value()); // Raw altitude in centimeters (i32)
      logger.print("meters "); logger.println(gps.altitude.meters()); // Altitude in meters (double)
      logger.print("miles "); logger.println(gps.altitude.miles()); // Altitude in miles (double)
      logger.print("km "); logger.println(gps.altitude.kilometers()); // Altitude in kilometers (double)
      logger.print("feet "); logger.println(gps.altitude.feet()); // Altitude in feet (double)
    }
    if (gps.satellites.isUpdated()) {
      logger.println("Satellites:");
      logger.print("number "); logger.println(gps.satellites.value()); // Number of satellites in use (u32)
    }
    if (gps.hdop.isUpdated()) {
      logger.println("Hdop:");
      logger.print("horiz dim of prec 100ths "); logger.println(gps.hdop.value()); // Horizontal Dim. of Precision (100ths-i32)    
    }
  }

  void update()
  {
    bool noe = false;
    while(ss.available()>0) {
      char ch = ss.read();
      logger.print(ch);
      gps.encode(ch);
      noe = true;
    }
    if (noe) logger.println();
  }
} gpsComm;


void SERCOM5_Handler()
{
  gpsComm.IrqHandler();
}





static const auto PIN_GPS_EN = 26ul; // PA27  
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
    pinMode(PIN_GPS_EN, INPUT); // high-z
    //digitalWrite(PIN_GLED, LOW);
    pinMode(PIN_STATUS, INPUT_PULLDOWN);
    pinMode(PIN_PWRKEY, INPUT_PULLDOWN);
    logger.println("SimCom?");

    // turn on SimCom (cycle power if already on)
    if (isOn()) {
      powerOnOff();
      assert(!isOn());
      delay(800);
    }
    assert(!isOn());
    powerOnOff();
    assert(isOn());

    // activate GPS too
    //digitalWrite(PIN_GLED, HIGH);
   
    logger.println("SimCom!");
  }
 
  
} simCom;



void setup() {

  pinMode(PIN_GLED, OUTPUT);
  digitalWrite(PIN_GLED, HIGH); 
  
  logger.begin();
  simCom.begin();
  gpsComm.begin();
  gsmComm.begin();

  
  logger.println("Setup done!");
}



void loop() {
  //logger.print("VBat: " ); 
  //logger.println(misc::readBatteryVoltage());
  //logger.print("Ram: ");
  //logger.println(misc::freeRam());
  //gsmComm.update();
  //gpsComm.update();
  //gpsComm.dumpAll();


  while (Serial.available()) {
    char ch = Serial.read();
    Serial.write(ch);
    gsmComm.ss.write(ch);
  }
  while (gsmComm.ss.available()) {
    Serial.write((char)gsmComm.ss.read());
  }
/*
  static int teller=0;
  static int toggler=0;
  teller++;
  
  if (teller==100) {
    teller=0;
    switch (toggler) {
      case 0:
      logger.println("INPUT_PULLDOWN");
      pinMode(PIN_GPS_EN, INPUT_PULLDOWN);
      toggler=1;
      break;
      case 1:
      logger.println("INPUT_PULLUP");
      pinMode(PIN_GPS_EN, INPUT_PULLUP);
      toggler=2;
      break;
      case 2:
      logger.println("LOW");
      pinMode(PIN_GPS_EN, OUTPUT);
      digitalWrite(PIN_GPS_EN, LOW);
      toggler=3;
      break;
      case 3:
      logger.println("HIGH");
      pinMode(PIN_GPS_EN, OUTPUT);
      digitalWrite(PIN_GPS_EN, HIGH);
      toggler=0;
      break;
    }
  }
 */
  // blink the mid-led
  pinMode(PIN_GLED, OUTPUT);
  digitalWrite(PIN_GLED, HIGH); 
  delay(100);
  digitalWrite(PIN_GLED, LOW);
  delay(100);
}
