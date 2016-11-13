#include "http.hpp"
#include "gsm.hpp"

class Http
{
public:

  bool get(const String& url)
  {
    if (!gsm::isConnected()) return false;
    
    gsm::runner()->then("HTTPINIT", 1000
    )->then(String("AT+HTTPPARA=\"URL\",\"") + url + "\"", 1000
    )->then(
      "HTTPACTION=0", 10000,
      [&](const String& msg) {
        
      }
    )->then(
      "AT+HTTPREAD", 10000,
      [&](const String& msg) {
        
      }
    )->finally(
      [&](bool err, Runner* r) {
        r->then("HTTPTERM");
        return true;
      }
    );
      
    return true;
  }
  
};

namespace http
{
  Http http_obj;
  
  bool get(const String& str)
  {
    return http_obj.get(str);
  }
}

