#pragma once
#include "GsmSerial.hpp"
#include "Logger.hpp"


// APN setup
#define APN "data.lyca-mobile.no"
#define APN_USER "lmno"
#define APN_PW "plus"
#define OWM_APIKEY "18143027801bd9493887c2020cb2968e"


// higher-level object to comm with gsm
class GsmComm
{
public:
  GsmSerial& gsmSerial = ::gsmSerial;
  
  void begin() {
    logger.println("GSM?");

    //gsmSerial.begin();
    
    // calibrate baud
    gsmSerial.setTimeout(100);
    for (int i=0; ; i++) {
      gsmSerial.println("AT");
      if (gsmSerial.find("OK\r")) break;      
      assert(i<10);
    }
    logger.println();
    
    // wait for stuff to come up
    gsmSerial.setTimeout(10000);
    gsmSerial.find("SMS Ready\r");
    logger.println();

    delay(1000);
 
    // Disable echo
    gsmSerial.setTimeout(1000);
    gsmSerial.println("ATE0");
    assert(gsmSerial.find("OK\r") != -1);
    logger.println();
    
    // Enable hardware flow control
    /*
    gsmSerial.setTimeout(1000);
    gsmSerial.println("AT+IFC=2,2");
    assert(gsmSerial.find("OK\r") != -1);
    logger.println();
    */
    
    //getWeatherData(63,10);
    
    logger.println("GSM!");


    getWeatherData(0,0);

    getTime();
  }


  void getTime()
  {
    gsmSerial.println("AT+CIPGSMLOC=1,1");
    assert(gsmSerial.find("+CIPGSMLOC:") != -1);
    delay(100);
  }

  
  void getWeatherData(int lat, int lon) {
    gsmSerial.setTimeout(10000);

    // activate GPRS
    gsmSerial.println("AT+CGATT=1");
    assert(gsmSerial.find("OK\r") != -1);
    logger.println();

    gsmSerial.println("AT+SAPBR=3,1,\"Contype\",\"GPRS\"");
    assert(gsmSerial.find("OK\r") != -1);
    logger.println();
    
    gsmSerial.print("AT+SAPBR=3,1,\"APN\",\""); gsmSerial.print(APN); gsmSerial.println("\"");
    assert(gsmSerial.find("OK\r") != -1);
    logger.println();

    gsmSerial.println("AT+SAPBR=1,1");
    assert(gsmSerial.find("OK\r") != -1);
    logger.println();

    gsmSerial.println("AT+HTTPINIT");
    assert(gsmSerial.find("OK\r") != -1);
    logger.println();
  
    gsmSerial.println(String("")+"AT+HTTPPARA=\"URL\",\"api.openweathermap.org/data/2.5/weather?lat="+lat+"&lon="+lon+"&APPID=" + OWM_APIKEY + "\"");
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
  
  
};

extern GsmComm gsmComm;

