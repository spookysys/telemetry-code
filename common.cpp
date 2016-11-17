#include "common.hpp"
#include "logging.hpp"
#include "watchdog.hpp"

namespace {
  Logger& logger = logging::get("common");
}

void assert_handler(const char* expr, const char* file, int line)
{
  //noInterrupts();
  
  logger.println(String("Assertion failed: ") + expr + " in " + file + ":" + String(line));
  logger.println();
  logger.flush();

  // blink quickly so you can see that there's an error in the field
  for (int i=0; i<20; i++)
  {
    digitalWrite(PIN_LED, i&1);
    delay(100);
    watchdog::tickle();
  }
 
  //interrupts();
  #ifdef DEBUG
  //abort();
  #endif
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


