#include "SimCom.hpp"
#include "Logger.hpp"
#include "MySerial.hpp"
#include "CommandManager.hpp"

// APN setup
#define APN "data.lyca-mobile.no"
#define APN_USER "lmno"
#define APN_PW "plus"

// OpenWeatherMap key
#define OWM_APIKEY "18143027801bd9493887c2020cb2968e"



// serials and irq handlers
namespace simcom
{
  MySerial gsmSerial;
  MySerial gpsSerial;
}

void SERCOM2_Handler()
{
  simcom::gsmSerial.IrqHandler();
}

void SERCOM5_Handler()
{
  simcom::gpsSerial.IrqHandler();
}



namespace simcom
{
  const auto PIN_GPS_EN = 26ul;
  const auto PIN_STATUS = 25ul;
  const auto PIN_PWRKEY = 38ul;
  
  bool isOn()
  {
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
    for (int i = 0; i <= 20; i++) {
      delay(100);
      if (isOn() != startStatus) break;
      assert(i < 20);
    }
    digitalWrite(PIN_PWRKEY, HIGH);
    pinMode(PIN_PWRKEY, INPUT);
    //delay(100);
    int stopStatus = isOn();
    if (stopStatus) {
      logger.println("SimCom is now on");
    } else {
      logger.println("SimCom is now off");
    }
    assert(startStatus != stopStatus);
  }
  
  
  
  bool GsmLineCallback(const String& str);
  void OpenGsmSerial()
  {
    logger.println("GSM: Opening serial");
    gsmSerial.begin_hs("gsm",  115200,  3ul/*PA09 SERCOM2.1 RX<-GSM_TX */,  4ul/*PA08 SERCOM2.0 TX->GSM_RX*/, 2ul /* RTS PA14 SERCOM2.2 */, 5ul /* CTS PA15 SERCOM2.3 */, PIO_SERCOM_ALT, PIO_SERCOM_ALT, PIO_DIGITAL, PIO_DIGITAL, SERCOM_RX_PAD_1, UART_TX_PAD_0, &sercom2);
    gsmSerial.callback = GsmLineCallback;
    logger.println();
  
    logger.println("GSM: Detecting baud");
    for (int i = 0; i <= 10; i++) {
      gsmSerial.println("AT");
      if (gsmSerial.readln_wait(100)=="OK") break;
      assert(i < 10);
    }
    logger.println();
  
  
    String ret;
  
    logger.println("GSM: Disabling echo");
    gsmSerial.setTimeout(1000);
    gsmSerial.println("ATE0");
    assert(gsmSerial.readln_wait()=="OK");
    logger.println();
    
    logger.println("GSM: Enabling flow control");
    gsmSerial.println("AT+IFC=2,2");
    assert(gsmSerial.readln_wait()=="OK");
  }

  bool GpsLineCallback(const String& str);
  void OpenGpsSerial()
  {
    logger.println("GPS: Opening serial");
    gpsSerial.begin("gps", 115200, 31ul/*PB23 SERCOM5.3 RX<-GPS_TX */, 30ul/*PB22 SERCOM5.2 TX->GPS_RX*/, PIO_SERCOM_ALT, PIO_SERCOM_ALT, SERCOM_RX_PAD_3, UART_TX_PAD_2, &sercom5);
    gpsSerial.callback = GpsLineCallback;
  }
  

  
  // net state
  enum NetState {
    MS_DISCONNECTED,
    MS_CONNECTED,
    MS_CONNECT_IN_PROGRESS,
    MS_REQUEST_IN_PROGRESS,
  };
  NetState net_state = MS_DISCONNECTED;


  
  void begin()
  {
    logger.println("SimCom?");
    pinMode(PIN_GPS_EN, INPUT); // high-z
    pinMode(PIN_STATUS, INPUT_PULLDOWN);
    pinMode(PIN_PWRKEY, INPUT_PULLDOWN);
  
    // If module already on, reset it
    if (isOn()) {
      powerOnOff();
      assert(!isOn());
      delay(800);
      assert(!isOn());
    }
  
    // Turn module on
    assert(!isOn());
    powerOnOff();
    assert(isOn());
  
  
    // GSM
    OpenGsmSerial();
  
  
    // GPS
    // OpenGpsSerial();
  
    logger.println("SimCom!");
  }




  // gsm-commands that will be run
  // push to front for immediate action, to back for queuing up
  CommandManager command_manager;

    
  const char* state_table[] = {
    0, // idle
    "AT+CSQ", // signal strength (abort if too low)
    0, // response
    "AT+SAPBR=3,1,\"Contype\",\"GPRS\"", // Set bearer parameter
    "AT+SAPBR=3,1,\"APN\",\"" APN "\"", // Set bearer context
    "AT+SAPBR=1,1", // Activate bearer context
    "AT+SAPBR=2,1", // Read bearer context - our IP
    0,
    "AT+CIPGSMLOC=1,1", // Read time and location for seeding GPS, but only do this once
    0
  };


  


  
  void update(unsigned long timestamp, unsigned long delta)
  {
    // send commands from serial to GSM port
    while (Serial.available()) {
      char ch = Serial.read();
      gsmSerial.write(ch);
    }

    String str = gsmSerial.readln_imm();
    if (str.length()) {
      logger.println("Got something!");
      command_manager.process_response(gsmSerial, str);
    }

    static bool har_gjort = false;
    if (timestamp > 10000 && !har_gjort) {
      har_gjort = true;
      command_manager.enqueue(gsmSerial, "AT+CSQ", [=](const String& rsp, CommandRunnerFn run) {
        logger.print("Fikk ");
        logger.print(rsp);
        logger.println(" i min lambda");
      });
    }

    command_manager.update(timestamp, delta);
    
  }
  



  bool GpsLineCallback(const String& str)
  {

    return false;
  }

  

  bool GsmLineCallback(const String& str)
  {
    static const char* gobbleList[] = {
      "AT", // may be echoed during baud-detection
      "ATE0", // may be echoed while turning off echo
      "RDY", // may be printed during initialization
      "+CFUN: 1", // may be printed during initialization
      "+CPIN: READY", // may be printed during initialization
      "Call Ready", // may be printed during initialization
      "SMS Ready", // may be printed during initialization
    };
  
    for (int i=0; i<sizeof(gobbleList)/sizeof(*gobbleList); i++) {
      //logger.println(i);
      if (str == gobbleList[i]) {
        logger.print("\"");
        logger.print(str);
        logger.println("\" - gobbled by callback");
        return true;
      }
    }

    // location/time response
    /*
    if (str.startsWith("+CIPGSMLOC:")) {
      // decode and feed to gps is not yet initialized
      return true;
    }

    // signal strength
    if (str.startsWith("+CSQ:")) {
      // log it
      return true;
    }
    */
    
    
    return false;
  } 
} // namespace simcom



// old junk
#if 0


// GSM: Wait for stuff to come up
gsmSerial.setTimeout(10000);
gsmSerial.find("SMS Ready\r");
logger.println();
delay(1000);


void gsmGetTimeAndLocation()
{
  gsmSerial.println("AT+CIPGSMLOC=1,1");
  assert(gsmSerial.find("+CIPGSMLOC:") != -1);
  delay(100);
}


void getWeatherData(int lat, int lon) {
  gsmSerial.setTimeout(10000);


  gsmSerial.println("AT+HTTPINIT");
  assert(gsmSerial.find("OK\r") != -1);
  logger.println();

  gsmSerial.println(String("") + "AT+HTTPPARA=\"URL\",\"api.openweathermap.org/data/2.5/weather?lat=" + lat + "&lon=" + lon + "&APPID=" + OWM_APIKEY + "\"");
  assert(gsmSerial.find("OK\r") != -1);
  logger.println();

  gsmSerial.println("AT+HTTPACTION=0");
  assert(gsmSerial.find("OK\r") != -1);
  assert(gsmSerial.find("+HTTPACTION") != -1);
  logger.println();

  gsmSerial.println("AT+HTTPREAD");
  assert(gsmSerial.find("OK\r") != -1);
  logger.println();

  // +HTTPREAD: 107
  // {"cod":401, "message": "Invalid API key. Pleamap.org/faq#error401 for more info."}

}

void httpGet(const String& url, std::function<bool (const string&)> func)
{
  
  
}

#endif




