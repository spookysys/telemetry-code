#include "common.hpp"
#include "Logger.hpp"

void assert_handler(const char* expr, const char* file, int line)
{
  //noInterrupts();
  
  logger.print("Assertion failed: ");
  logger.println(expr);
  logger.print(" in ");
  logger.print(file);
  logger.print(":");
  logger.print(line);
  logger.println();
  logger.println();
  logger.flush();
 
  // blink the line number
  pinMode(PIN_LED, OUTPUT);
  digitalWrite(PIN_LED, LOW);
  int digits[4];
  digits[3] = (line / 1000) % 10;
  digits[2] = (line / 100) % 10;
  digits[1] = (line / 10) % 10;
  digits[0] = (line / 1) % 10;
  for (int digit_i=3; digit_i>=0; digit_i--)
  {
    delay(500);
    for (int i=0; i<digits[digit_i]; i++) {
      digitalWrite(PIN_LED, HIGH);
      delay(200);
      digitalWrite(PIN_LED, LOW);
      delay(200);
    }
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


