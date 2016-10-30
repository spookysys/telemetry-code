#pragma once
#include "GsmSerial.hpp"
#include "Logger.hpp"


// APN setup
#define APN "data.lyca-mobile.no"
#define APN_USER "lmno"
#define APN_PW "plus"
#define OPENWEATHERMAP_API_KEY "18143027801bd9493887c2020cb2968e"


// higher-level object to comm with gsm
class GsmComm
{
public:
  GsmSerial& gsmSerial = ::gsmSerial;
  
  void begin() {
    logger.println("GSM?");

    gsmSerial.begin();
    
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
    gsmSerial.find("Call Ready\r");
    logger.println();
 
    // Disable echo
    gsmSerial.setTimeout(1000);
    gsmSerial.println("ATE0");
    assert(gsmSerial.find("OK\r") != -1);
    logger.println();
    
    // Enable hardware flow control
    gsmSerial.setTimeout(1000);
    gsmSerial.println("AT+IFC=2,2");
    assert(gsmSerial.find("OK\r") != -1);
    logger.println();
    
    //getWeatherData(63,10);
    
    logger.println("GSM!");
  }

  /*
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
    
    // +HTTPREAD: 107
    // {"cod":401, "message": "Invalid API key. Pleamap.org/faq#error401 for more info."}
    // NOTE: Need flow control here!
  }
  */
  
};


