#include "logging.hpp"
#include <algorithm>
#include <vector>
#include <memory>

namespace logging
{
  bool serial_open = false;

  void begin()
  {
    // try bring up serial for 4 sec
    Serial.begin(74880);
    for (int i = 0; i<40 && !Serial; i++) {
      delay(100);
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
    String buff="";
  public:
    LoggerImpl(const String& id) : Logger(id) {}


    void println()
    {
      if (serial_open) {
        Serial.print(id);
        Serial.print("> ");
        Serial.println(buff);
      }
      buff = "";
    }

    char prev_nl = 0;
    void write(char ch)
    {
      if (ch=='\n' || ch=='\r') {
        if (!(ch=='\n' && prev_nl=='\r')) println();
        prev_nl = ch;
      } else {
        buff += ch;
        prev_nl = 0;
      }
    }

    void print(const String& op)
    {
      #if 0
      int idx_l = 0;
      int idx_r = indexOfNewline(op, idx_l);
      while (idx_r>=0) {
        buff += op.substring(idx_l, idx_r);
        idx_l = idx_r;
        while (idx_l<op.length() && (op[idx_l]=='\n' ||  op[idx_l]=='\r')) {
          write(op[idx_l]);
        }
        idx_r = indexOfNewline(op, idx_l);
      }
      if (idx_l<op.length()) {
        buff += op.substring(idx_l);
      }
      #else
      for (int i=0; i<op.length(); i++) write((char)op[i]);
      #endif
    }

    void println(const String& op) {
      print(op);
      println();
    }

    void flush()
    {
      if (buff.length()) println();
      if (serial_open) Serial.flush();
    }


  };

  std::vector<std::unique_ptr<LoggerImpl>> logger_pool;
  Logger& get(const String& id) {
    for (auto iter = logger_pool.begin(); iter!=logger_pool.end(); iter++)
      if ((*iter)->id == id) return **iter;
    logger_pool.emplace_back(std::unique_ptr<LoggerImpl>(new LoggerImpl(id)));
    return *logger_pool.back();
  }
}

