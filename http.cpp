#include "http.hpp"
#include "gsm.hpp"
#include "logging.hpp"

namespace http
{
  Logger& logger = logging::get("http");

  using namespace gsm;

  class Http
  {
    bool requesting = false;

    int tmp_cnt;
    bool tmp_err;
  public:

    bool isRequesting()
    {
      return requesting;
    }

    void rqGet(const String& url, std::function<void(bool)> done_callback)
    {
      if (!isConnected()) {
        done_callback(false);
        return;
      }

      this->requesting = true;
      
      gsm::runner()->then(
        "AT+HTTPINIT", 1000
      )->then(
        String("AT+HTTPPARA=\"URL\",\"") + url + "\"", 1000
      )->sync(
        [this](bool failed, Runner*) {
          tmp_cnt = 0;
          return NOP;
        }
      )->then(
        "AT+HTTPACTION=0", 
        30000,
        [this](const String& msg, Runner* r) {
          if (msg.startsWith("+HTTPACTION:")) {
            std::array<String, 3> toks;
            tokenize(msg, toks);
            int status = toks[1].toInt();
            int bytes = toks[2].toInt();
            logger.println(String("Status: ") + String(status) + String(" Bytes: ") + toks[2]);
            return (++tmp_cnt==2) ? OK : NOP; // NOTE
          }
          else if (msg=="OK") {
            return (++tmp_cnt==2) ? OK : NOP; // NOTE
          }
          else if (msg=="ERROR") return ERROR;
          return NOP;
        }
      )->then(
        "AT+HTTPREAD", 30000,
        [&](const String& msg, Runner* r) {
          //logger.println(String("message: ") + msg);
          if (msg=="OK") return OK;
          else if (msg=="ERROR") return ERROR;
          return NOP;
        }
      )->sync(
        [this](bool err, Runner* r) { 
          tmp_err = err;
          return OK;
        }
      )->then(
        "AT+HTTPTERM", 
        1000
      )->sync(
        [this, done_callback](bool err, Runner* r) {
          done_callback(tmp_err);
          requesting = false;
          return NOP;
        }
      );
    }
    
  };


  Http http_obj;
  
  void rqGet(const String& str, std::function<void(bool)> done_callback)
  {
    return http_obj.rqGet(str, done_callback);
  }

  bool isRequesting()
  {
    return http_obj.isRequesting();
  }
}

