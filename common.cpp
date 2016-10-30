#include "common.hpp"
#include "Logger.hpp"

void assert_handler(const char* expr, const char* file, int line)
{
  logger.print("Assertion failed: ");
  logger.println(expr);
  logger.print(" in ");
  logger.print(file);
  logger.print(":");
  logger.print(line);
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
  #ifdef DEBUG
  abort();
  #endif
}  


