#include "http.hpp"
#include "gsm.hpp"
#include "logging.hpp"

namespace http
{
  Logger& logger = logging::get("gsm");

  
  class Http
  {
  public:
    int rqs_in_progress = 0;

    int getNumRqsInProgress()
    {
      return rqs_in_progress;
    }

    bool get(const String& url, std::function<void(bool)> done_callback)
    {
      if (!gsm::isConnected()) return false;

      rqs_in_progress++;
      
      gsm::runner()->then(
        "AT+HTTPINIT", 
        1000
      )->then(
        String("AT+HTTPPARA=\"URL\",\"") + url + "\"", 
        1000
      )->then(
        "AT+HTTPACTION=0", 
        10000,
        [&](const String& msg) {
          logger.println(String("message: ") + msg);
        }
      )->then(
        "AT+HTTPREAD", 10000,
        [&](const String& msg) {
          logger.println(String("message: ") + msg);
        }
      )->finally(
        [this, done_callback](bool main_err, gsm::Runner* r) {
          r->then(
            "AT+HTTPTERM", 
            1000
          )->finally(
            [this, done_callback, main_err](bool err, gsm::Runner* r) {
              err = err || main_err;
              if (err) gsm::connectionFailed();
              if (done_callback) done_callback(main_err);
              rqs_in_progress--;
              return main_err;
            }
          );
          return false;
        }
      );
        
      return true;
    }
    
  };


  Http http_obj;
  
  bool get(const String& str, std::function<void(bool)> done_callback)
  {
    return http_obj.get(str, done_callback);
  }

  int getNumRqsInProgress()
  {
    return http_obj.getNumRqsInProgress();
  }
}

