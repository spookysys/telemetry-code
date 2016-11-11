#include "gsm.hpp"
#include "logging.hpp"
#include "MySerial.hpp"

// APN setup
#define APN "data.lyca-mobile.no"
#define APN_USER "lmno"
#define APN_PW "plus"


namespace gsm { MySerial serial("gsm-ser"); }

void SERCOM2_Handler()
{
  gsm::serial.IrqHandler();
}




namespace gsm {
  Logger& logger = logging::get("gsm");
  
  std::function<bool(const String& line)> umh = nullptr;

  void begin(std::function<bool(const String& line)> umh)
  {
    ::gsm::umh = umh; 
    
    logger.println("Opening serial");
    serial.begin_hs(115200,  3ul/*PA09 SERCOM2.1 RX<-GSM_TX */,  4ul/*PA08 SERCOM2.0 TX->GSM_RX*/, 2ul /* RTS PA14 SERCOM2.2 */, 5ul /* CTS PA15 SERCOM2.3 */, PIO_SERCOM_ALT, PIO_SERCOM_ALT, PIO_DIGITAL, PIO_DIGITAL, SERCOM_RX_PAD_1, UART_TX_PAD_0, &sercom2);
  
    logger.println("GSM: Detecting baud");
    serial.setTimeout(100);
    for (int i = 0; i <= 10; i++) {
      serial.println("AT");
      if (serial.find("OK\r")) break;
      assert(i < 10);
    }
    serial.setTimeout(1000);
    logger.println();
  }

  void update(unsigned long timestamp, unsigned long delta)
  {
    while (serial.hasString())
      serial.popString();
  }


}


