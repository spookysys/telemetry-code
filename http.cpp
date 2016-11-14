#include "http.hpp"
#include "gsm.hpp"
#include "logging.hpp"

namespace http
{
  Logger& logger = logging::get("gsm");

  class Http
  {
  public:
    bool rq_in_progress = false;

    bool isRqInProgress()
    {
      return rq_in_progress;
    }

    bool tmp_rq_err;
    bool rqGet(const String& url, std::function<void(bool)> done_callback)
    {
      if (!gsm::isConnected()) return false;

      this->rq_in_progress = true;
      
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
        [this, done_callback](bool err, gsm::Runner* r) { 
          tmp_rq_err = err;
          return false; 
        }
      )->then(
        "AT+HTTPTERM", 
        1000
      )->finally(
        [this, done_callback](bool err, gsm::Runner* r) {
          err = err || tmp_rq_err;
          this->rq_in_progress = false;
          done_callback(err);
          return err;
        }
      );
        
      return true;
    }
    
  };


  Http http_obj;
  
  bool rqGet(const String& str, std::function<void(bool)> done_callback)
  {
    return http_obj.rqGet(str, done_callback);
  }

  int isRqInProgress()
  {
    return http_obj.isRqInProgress();
  }
}

