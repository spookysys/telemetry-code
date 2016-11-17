#include "flashlog.hpp"
#include "logging.hpp"
#include "watchdog.hpp"
#include <SD.h>


// Set the pins used
#define PIN_CS 7


namespace flashlog
{
  Logger& logger = logging::get("flashlog");

  File logfile;
  File sensorfile;
  File gpsfile;

  void begin()
  {
    pinMode(PIN_CS, OUTPUT);
    if (!SD.begin(PIN_CS)) {
      logger.println("Card init. failed!");
      assert(0);
    }
    // generate log-file that doesn't already exist
    
    for (uint8_t i=0; i<100; i++) {
      watchdog::tickle();
      {
        String filename = String("log")+String(i)+String(".txt");
        if (SD.exists(filename)) continue;
        logfile = SD.open(filename, FILE_WRITE);
        if (!logfile) {
          logger.println(String("Could not create file ") + filename);
          assert(0);
        }
        logging::setLogfile(&logfile);
      }
      watchdog::tickle();
      {
        String filename = String("sen")+String(i)+String(".csv");
        sensorfile = SD.open(filename, FILE_WRITE);
        if (!sensorfile) {
          logger.println(String("Could not create file ") + filename);
          assert(0);
        }
      }
      watchdog::tickle();
      {
        String filename = String("gps")+String(i)+String(".csv");
        gpsfile = SD.open(filename, FILE_WRITE);
        if (!gpsfile) {
          logger.println(String("Could not create file ") + filename);
          assert(0);
        }
      }
      watchdog::tickle();
      break;
    }
  }
  
  File* sensorFile()
  {
    return &sensorfile;
  }
  
  File* gpsFile()
  {
    return &gpsfile;
  }

  void flush()
  {
    watchdog::tickle();
    logfile.flush();
    watchdog::tickle();
    sensorfile.flush();
    watchdog::tickle();
    gpsfile.flush();
    watchdog::tickle();
  }
}

