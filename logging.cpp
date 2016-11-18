#include "logging.hpp"
#include <SD.h>
#include <algorithm>
#include <vector>
#include <memory>
#include "watchdog.hpp"

namespace logging
{
  File* logfile = nullptr;
  bool serial_open = false;

  void begin()
  {
    // try bring up serial for 4 sec
    Serial.begin(115200);
    for (int i = 0; i<40 && !Serial; i++) {
      delay(100);
      watchdog::tickle();
    }
    serial_open = Serial;
  }

  

  int indexOfNewline(const String& str, int from)
  {
      int idx0 = str.indexOf('\n');
      int idx1 = str.indexOf('\r');
      if (idx0>=0 && idx1>=0) return std::min(idx0, idx1);
      return idx0>=0 ? idx0 : idx1;
  }

  class LoggerImpl : public Logger
  {
    static const int buff_len = 100;
    char buff[buff_len+1];
    int  buff_idx = 0;
  public:
    LoggerImpl(const String& id) : Logger(id) { buff[0] = 0; }

    void wrap() 
    {
      if (serial_open) {
        Serial.print(id);
        Serial.print("> ");
        Serial.print(buff);
        Serial.println("\\");
      }
      if (logfile) {
        logfile->print(id);
        logfile->print("> ");
        logfile->print(buff);
        logfile->println("\\");
      }      
      buff_idx = 0;
      buff[0] = 0;
    }

    void linebreak()
    {
      if (serial_open) {
        Serial.print(id);
        Serial.print("> ");
        Serial.println(buff);
      }
      if (logfile) {
        logfile->print(id);
        logfile->print("> ");
        logfile->println(buff);
      }
      buff_idx = 0;
      buff[0] = 0;
    }

    char prev_nl = 0;
    size_t write(uint8_t ch)
    {
      if (ch=='\n' || ch=='\r') {
        if (!(ch=='\n' && prev_nl=='\r')) linebreak();
        prev_nl = ch;
      } else {
        buff[buff_idx++] = ch;
        buff[buff_idx] = 0;
        prev_nl = 0;
        if (buff_idx==buff_len) wrap();
      }
      return 1;
    }


    size_t write(const uint8_t *buffer, size_t size)
    {
      for (int i=0; i<size; i++) write(buffer[i]);
      return size;
    }


    void flush()
    {
      if (buff_idx) println();
      if (serial_open) Serial.flush();
      if (logfile) logfile->flush();
    }


  };

  std::vector<std::unique_ptr<LoggerImpl>> logger_pool;
  Logger& get(const String& id) {
    for (auto iter = logger_pool.begin(); iter!=logger_pool.end(); iter++)
      if ((*iter)->id == id) return **iter;
    logger_pool.emplace_back(std::unique_ptr<LoggerImpl>(new LoggerImpl(id)));
    return *logger_pool.back();
  }


  void setLogfile(File* file)
  {
    logfile = file;
    logfile->println("Logger attached");
  }
}

