#include "common.hpp"
#include "SD.h"

namespace flashlog
{
  void begin();
  
  File* logFile();
  File* sensorFile();
  File* gpsFile();

  void flush();
}


