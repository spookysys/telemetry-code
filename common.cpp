#include "common.hpp"
#include "logging.hpp"
#include "watchdog.hpp"
#include <Wire.h>

namespace {
  Logger& logger = logging::get("common");
}

void assert_handler(const char* expr, const char* file, int line)
{
  watchdog::tickle();
  logger.println(String("Assertion failed: ") + expr + " in " + file + ":" + String(line));
  logger.println();
  watchdog::tickle();
  logger.flush();
  watchdog::tickle();

  // blink quickly so you can see that there's an error in the field
  for (int i=0; i<20; i++)
  {
    watchdog::tickle();
    digitalWrite(PIN_LED, i&1);
    delay(100);
  }
  watchdog::tickle();
 
  //interrupts();
  #ifdef DEBUG
  //abort();
  #endif
}  

// I2C scan function
void I2Cscan()
{
  // scan for i2c devices
  byte error, address;
  int nDevices;

  logger.println("Scanning...");

  nDevices = 0;
  for (address = 1; address < 127; address++ )
  {
    // The i2c_scanner uses the return value of
    // the Write.endTransmisstion to see if
    // a device did acknowledge to the address.
    Wire.beginTransmission(address);
    error = Wire.endTransmission();

    if (error == 0)
    {
      logger.print("I2C device found at address 0x");
      if (address < 16)
        logger.print("0");
      logger.print(address, HEX);
      logger.println("  !");

      nDevices++;
    }
    else if (error == 4)
    {
      logger.print("Unknow error at address 0x");
      if (address < 16)
        logger.print("0");
      logger.println(address, HEX);
    }
  }
  if (nDevices == 0)
    logger.println("No I2C devices found\n");
  else
    logger.println("done\n");

}



// hack for basic STL
// https://forum.pjrc.com/threads/23467-Using-std-vector?p=69787&viewfull=1#post69787
namespace std {
  void __throw_bad_alloc()
  {
    assert(!"Unable to allocate memory");
  }

  void __throw_bad_function_call()
  {
    assert(!"Bad function call");
  }
    
  void __throw_length_error( char const*e )
  {
    assert(!(String("Length Error :")+e));
  }
}


