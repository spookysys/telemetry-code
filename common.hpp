#ifndef COMMON_HPP
#define COMMON_HPP
#include <Arduino.h>
#undef min
#undef max


// general config
#define DEBUG
#define PIN_LED 8


// define assert handler
#define assert(expr) do { if (!(expr)) { assert_handler(#expr, __FILE__, __LINE__); } } while(0)
void assert_handler(const char* expr, const char* file, int line);


extern "C" char *sbrk(int i);
inline unsigned long freeRam ()
{
  char stack_dummy = 0;
  return &stack_dummy - sbrk(0);
}

template<typename T>
static void tokenize(const String& str, T& toks, char separator=',')
{
  int r_idx = -1;
  bool err = false;
  for (auto& iter : toks) {
    int l_idx = r_idx+1;
    r_idx = str.indexOf(separator, l_idx);
    if (l_idx<0) return;
    else if (r_idx<0) iter = str.substring(l_idx);
    else iter = str.substring(l_idx, r_idx);
  }
}



#endif
