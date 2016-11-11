#include "SimCom.hpp"
#include "Logger.hpp"
#include "MySerial.hpp"
#include <functional>
#include <queue>
#include <memory>

// APN setup
#define APN "data.lyca-mobile.no"
#define APN_USER "lmno"
#define APN_PW "plus"

// OpenWeatherMap key
#define OWM_APIKEY "18143027801bd9493887c2020cb2968e"

namespace simcom {
  class GpsComm {
  public:
    MySerial serial;
    GpsComm() {}
    void begin()
    {
      logger.println("GPS: Opening serial");
      serial.begin("gps", 115200, 31ul/*PB23 SERCOM5.3 RX<-GPS_TX */, 30ul/*PB22 SERCOM5.2 TX->GPS_RX*/, PIO_SERCOM_ALT, PIO_SERCOM_ALT, SERCOM_RX_PAD_3, UART_TX_PAD_2, &sercom5);
    }
    
    void update(unsigned long timestamp, unsigned long delta)
    {
      while (serial.hasString()) {
        String str = serial.popString();
        if (str[0]!='$') {
          logger.println(str);
          logger.println("Not understood");
          continue;
        }
        if (str[1]!='G') {
          logger.println(str);
          logger.println("Unknown talker");
          continue;
        }
        int idx_l = 3;
        int idx_r = str.indexOf(',', idx_l);
        String sss = str.substring(idx_l, idx_r);
        //logger.println(sss);
        if (sss=="GGA") {
          // Global Positioning System Fix Data. Time, Position and fix related data for a GPS receive
        } else if (sss=="RMC") {
          // Time, date, position, course and speed data
        } else if (sss=="GLL") {
          // Geographic Position - Latitude/Longitude
          // Position was calculated based on one or more of the SVs having their states derived from almanac parameters, as opposed to ephemerides.
        } else if (sss=="VTG") {
          // Course and speed information relative to the ground
        } else if (sss=="ACCURACY") {
          // ...
        } else if (sss=="GSA") { // GPS DOP and active satellite
        } else if (sss=="GSV") { // Satellites in view
        } else if (sss=="ZDA") { // Time & Date – UTC, Day, Month, Year and Local Time Zone
        } else {
          logger.println(str);
          logger.println("Unknown sentence identifier");
        }
      }
    }

    void run(String cmd)
    {
      
    }
  };
}

namespace simcom {
  class GsmComm {
    using unsolicited_message_handler_t = std::function<bool(const String& line)>;
    unsolicited_message_handler_t unsolicited_message_handler = nullptr;
  public:
    MySerial serial;
    GsmComm() {}

    void begin(unsolicited_message_handler_t op)
    {
      this->unsolicited_message_handler = op; 
      
      logger.println("GSM: Opening serial");
      serial.begin_hs("gsm",  115200,  3ul/*PA09 SERCOM2.1 RX<-GSM_TX */,  4ul/*PA08 SERCOM2.0 TX->GSM_RX*/, 2ul /* RTS PA14 SERCOM2.2 */, 5ul /* CTS PA15 SERCOM2.3 */, PIO_SERCOM_ALT, PIO_SERCOM_ALT, PIO_DIGITAL, PIO_DIGITAL, SERCOM_RX_PAD_1, UART_TX_PAD_0, &sercom2);
      logger.println();
  
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

  private:
    struct QueuedCommand
    {
      String cmd;
      unsigned long timeout;
      QueuedCommand(const String& cmd, unsigned long timeout) : cmd(cmd), timeout(timeout) {}
      std::function<void(const String& msg, std::function<void()> resolve, std::function<void()> fail)> msg_handler = nullptr;
      std::function<void(bool)> done_handler = nullptr;
    };
    
    std::queue<QueuedCommand> command_queue;   
  
    std::unique_ptr<QueuedCommand> current_cmd;

    void finish_cmd(bool error) {
      if (error) {
        logger.println(String("FAILED: \"") + current_cmd->cmd + "\"");
      } else {
        //logger.println(String("SUCCEEDED: \"") + current_cmd->cmd + "\"");
      }
      if (current_cmd->done_handler)
        current_cmd->done_handler(error);
      current_cmd.reset();
    }
    
  public:
    static const unsigned long default_timeout = 1000;
  
    void update(unsigned long timestamp, unsigned long delta)
    {
      // fetch message
      while (serial.hasString()) {
        String str = serial.popString();
        if (unsolicited_message_handler(str)) {
          // ok
        } else if (current_cmd && str=="OK") {
          finish_cmd(false);
        } else if (current_cmd && str=="ERROR") {
          finish_cmd(true);
        } else if (current_cmd && current_cmd->msg_handler) {
          current_cmd->msg_handler(str, [&](){ finish_cmd(false); }, [&](){ finish_cmd(true); });
        } else if (current_cmd) {
          logger.println(String("UNHANDLED: \"") + str + "\" while running \"" + current_cmd->cmd + "\"");
          assert(0);
        } else {
          logger.println(String("UNHANDLED: \"") + str + "\"");
          assert(0);
        }
      }
    
      // decrement timeout
      if (current_cmd) {
        if (current_cmd->timeout > delta) {
          current_cmd->timeout -= delta;
        } else {
          logger.println(String("TIMEOUT: \"") + current_cmd->cmd + "\" - stopping.");
          current_cmd.reset();
          assert(0);
        }
      }

      // start a new cmd
      if (!current_cmd && command_queue.size()) {
        current_cmd = std::unique_ptr<QueuedCommand>(new QueuedCommand(std::move(command_queue.front())));
        command_queue.pop();
        serial.println(current_cmd->cmd);
      }
    }

    // full blown version
    void run_h2(const String& cmd, unsigned long timeout, std::function<void(const String&, std::function<void()> resolve, std::function<void()> fail)> msg_handler, std::function<void(bool)> done_handler=nullptr) {
      QueuedCommand tmp(cmd, timeout);
      tmp.msg_handler = msg_handler;
      tmp.done_handler = done_handler;
      command_queue.push(std::move(tmp));
    }

    // full blown minus timeout
    void run_h2(const String& cmd, std::function<void(const String&, std::function<void()> resolve, std::function<void()> fail)> msg_handler, std::function<void(bool)> done_handler=nullptr) {
      run_h2(cmd, default_timeout, msg_handler, done_handler);
    }

    // simple message handler
    void run_h1(const String& cmd, unsigned long timeout, std::function<void(const String&)> simple_msg_handler, std::function<void(bool)> done_handler=nullptr) {
      QueuedCommand tmp(cmd, timeout);
      tmp.msg_handler = [simple_msg_handler](const String& msg, std::function<void()> resolve, std::function<void()> fail){ simple_msg_handler(msg); };
      tmp.done_handler = done_handler;
      command_queue.push(std::move(tmp));
    }

    // simple message handler minus timeout
    void run_h1(const String& cmd, std::function<void(const String&)> simple_msg_handler, std::function<void(bool)> done_handler=nullptr) {
      run_h1(cmd, default_timeout, simple_msg_handler, done_handler);
    }  

    // no message handler
    void run(const String& cmd, unsigned long timeout, std::function<void(bool)> done_handler=nullptr)
    {
      QueuedCommand tmp(cmd, timeout);
      tmp.msg_handler = nullptr;
      tmp.done_handler = done_handler;
      command_queue.push(std::move(tmp));
    }

    // no message handler minus timeout
    void run(const String& cmd, std::function<void(bool)> done_handler=nullptr)
    {
      run(cmd, default_timeout, done_handler);
    }  
  };
}

namespace simcom {
  GpsComm gps;
  GsmComm gsm;
}

void SERCOM5_Handler()
{
  simcom::gps.serial.IrqHandler();
}

void SERCOM2_Handler()
{
  simcom::gsm.serial.IrqHandler();
}


namespace simcom {
  class GsmClient {
    std::function<void(const String& lon, const String& lat, const String& date, const String& time_utc)> gps_priming_callback;

    static const unsigned long connection_maintenance_period  = 60000;
    unsigned long connection_maintenance_timer = 0; // for re-starting maintenance
    bool connection_maintenance_ongoing = false;

    bool network_status = false;
    bool bearer_status = false;
    bool gps_priming_done = false;

    // check for network signal
    void connection_maintenance_start()
    {
      assert(!connection_maintenance_ongoing);
      connection_maintenance_ongoing = true;

      connection_maintenance_1();
    }

    // check signal strength and whether bearer profile is active
    // and whether IP stack is active
    void connection_maintenance_1() 
    {
      gsm.run_h1("AT+CSQ;+SAPBR=2,1", [this](const String& msg){
        if (msg.startsWith("+CSQ: ")) {
          int lastIndex = msg.lastIndexOf(',');
          int tmp = msg.substring(6, lastIndex).toInt();
          assert(tmp>=0);
          logger.println(String("Signal: ") + tmp);
          if (tmp<=1) updateNetworkStatus(false);
        } else if (msg.startsWith("+SAPBR: ")) {
          int index0 = msg.indexOf(',');
          int index1 = msg.indexOf(',', index0+1);
          int cid = msg.substring(8, index0).toInt();
          int status = msg.substring(index0+1, index1).toInt();
          String ip = msg.substring(index1+2, msg.length()-1);
          assert(cid>=0 && status>=0 && ip.length()>0);
          bool ret = (status==1);
          logger.println(String("Current bearer status: ") + ret);
          updateBearerStatus(status==1);          
        } else {
          assert(!"Unknown message");
        }
      }, [this](bool err) {
        if (!err && network_status) connection_maintenance_2();
        else connection_maintenance_done();
      });
    }

    // configure bearer
    void connection_maintenance_2()
    {
      // skip if already configured 
      if (this->bearer_status) {
        connection_maintenance_3();
        return;
      }
      // do it
      gsm.run("AT+SAPBR=3,1,\"Contype\",\"GPRS\";+SAPBR=3,1,\"APN\",\"" APN "\";+SAPBR=1,1", 10000, [this](bool err){
        if (!err) {
          updateBearerStatus(true);
          connection_maintenance_3();
        } else connection_maintenance_done();
      });
    }

    // query location and time to prime GPS
    void connection_maintenance_3() {
      // skip if already done
      if (gps_priming_done) {
        connection_maintenance_4();
        return;
      }
      // do it
      gsm.run_h1("AT+CIPGSMLOC=1,1", 10000, [this](const String& msg) {
        int index0 = msg.indexOf(',');
        int index1 = msg.indexOf(',', index0+1);
        int index2 = msg.indexOf(',', index1+1);
        int index3 = msg.indexOf(',', index2+1);
        this->gps_priming_callback(msg.substring(index0+1, index1), msg.substring(index1+1, index2), msg.substring(index2+1, index3), msg.substring(index3+1));
        this->gps_priming_done = true;
      }, [this](bool err) {
        if (!err) connection_maintenance_4();
        else connection_maintenance_done();
      });
    }

    void connection_maintenance_4() {
      connection_maintenance_done();
    }

    void connection_maintenance_done() {
      logger.println("connection_maintenance_done");
      if (network_status) {
        logger.println(String("Setting maintenance timer to ") + connection_maintenance_period);
        connection_maintenance_timer = connection_maintenance_period;
      } else {
        logger.println("Not setting maintenance timer, as network is disconnected");
      }
      connection_maintenance_ongoing = false;
    }
  public:

    void updateBearerStatus(bool op) {
      if (!op) {
        if (this->bearer_status) logger.println("Bearer disconnected!");
        this->bearer_status = false;
      } else {
        if (!this->bearer_status) logger.println("Bearer connected!");
        this->bearer_status = true;
      }
    }
  
    void updateNetworkStatus(bool op) {
      if (!op) {
        if (this->network_status) logger.println("Network disconnected!");
        this->network_status = false;
        updateBearerStatus(false);
        connection_maintenance_timer = 0;
      } else {
        if (!this->network_status) logger.println("Network connected!");
        this->network_status = true;
        if (!connection_maintenance_ongoing)
          connection_maintenance_start();
      }
    }

    void begin(decltype(GsmClient::gps_priming_callback) gps_priming_callback) {
      this->gps_priming_callback = gps_priming_callback;
    }
      
    void update(unsigned long timestamp, unsigned long delta)
    {
      if (connection_maintenance_timer > delta) {
        connection_maintenance_timer -= delta;
      } else if (connection_maintenance_timer>0) {
        connection_maintenance_timer = 0;
        connection_maintenance_start();
      }
    }
  };

}

namespace simcom {
  GsmClient gsm_client;
}
  
// on/off
namespace simcom
{
  const auto PIN_GPS_EN = 26ul;
  const auto PIN_STATUS = 25ul;
  const auto PIN_PWRKEY = 38ul;
  
  bool isOn()
  {
    return digitalRead(PIN_STATUS);
  }
  
  void powerOnOff() {
    int startStatus = isOn();
    if (startStatus) {
      logger.println("SimCom is on - turning off");
    } else {
      logger.println("SimCom is off - turning on");
    }
    pinMode(PIN_PWRKEY, OUTPUT);
    digitalWrite(PIN_PWRKEY, LOW);
    delay(1000);
    for (int i = 0; i <= 100; i++) {
      delay(100);
      if (isOn() != startStatus) break;
      assert(i < 100);
    }
    digitalWrite(PIN_PWRKEY, HIGH);
    pinMode(PIN_PWRKEY, INPUT);
    //delay(100);
    int stopStatus = isOn();
    if (stopStatus) {
      logger.println("SimCom is now on");
    } else {
      logger.println("SimCom is now off");
    }
    assert(startStatus != stopStatus);
  }
}

namespace simcom {

  void gpsPrimingCallback(const String& lon, const String& lat, const String& date, const String& time_utc)
  {
    logger.println(lon);
    logger.println(lat);
    logger.println(date);
    logger.println(time_utc);
  }

  // for now, gobble all unsolicited messages
  bool gsmUnsolicitedMessageHandler(const String& str)
  {
    static const char* gobbleList[] = {
      //"ATE0", // may be echoed while turning off echo
      //"RDY", // may be printed during initialization
      //"+CFUN: 1", // may be printed during initialization
      //"+CPIN: READY", // may be printed during initialization
      //"Call Ready", // may be printed during initialization
      //"SMS Ready", // may be printed during initialization

      "+CCWA:",
      "+CLIP:",
      "+CRING:",
      "+CREG:",
      "+CCWV:",
      "+CMTI:", // message
      "+CMT:", // message
      "+CBM:", // broadcast message
      "+CDS:", // sms status report
      "+COLP:",
      "+CSSU:",
      "+CSSI:",
      "+CLCC:",
      "*PSNWID:",
      "*PSUTTZ:",
      "+CTZV:",
      "DST:",
      "+CSMINS:",
      "+CDRIND:",
      "+CHF:",
      "+CENG:",
      "MO RING",
      "MO CONNECTED",
      "+CPIN:", // Indicates whether some password is required
      "+CSQN:",
      "+SIMTONE:",
      "+STTONE:",
      "+CR:",
      "+CUSD:",
      "RING",
      "NORMAL POWER DOWN",
      "+CMTE:",
      "UNDER-VOLTAGE POWER DOWN",
      "UNDER-VOLTAGE WARNING",
      "UNDER-VOLTAGE WARNNING",
      "OVER-VOLTAGE POWER DOWN",
      "OVER-VOLTAGE WARNING",
      "OVER-VOLTAGE WARNNING",
      "CHARGE-ONLY MODE",
      "RDY",
      "Call Ready",
      "SMS Ready",
      "+CFUN:",
      //[<n>,]CONNECT OK
      "CONNECT",
      //[<n>,]CONNECT FAIL
      //[<n>,]ALREADY CONNECT
      //[<n>,]SEND OK 
      //[<n>,]CLOSED
      "RECV FROM:",
      "+IPD,",
      "+RECEIVE,",
      "REMOTE IP:",
      "+CDNSGIP:",
      "+PDP: DEACT",
      //"+SAPBR",
      "+HTTPACTION:",
      "+FTP",
      //"+CGREG:",
      "ALARM RING",
      "+CALV:"
    };

    if (str.startsWith("+CGREG: ")) {
      int status = str.substring(8, 9).toInt();
      assert(status>=0);
      gsm_client.updateNetworkStatus(status==1 || status==5);
      return true;
    }
    
    for (int i=0; i<sizeof(gobbleList)/sizeof(*gobbleList); i++) {
      if (str.startsWith(gobbleList[i])) {
        //logger.print(String("\"") + str + "\" - gobbled by callback");
        return true;
      }
    }
    return false;
  } 

  void begin()
  {
    logger.println("SimCom?");
    pinMode(PIN_GPS_EN, INPUT); // high-z
    pinMode(PIN_STATUS, INPUT_PULLDOWN);
    pinMode(PIN_PWRKEY, INPUT_PULLDOWN);
  
    // If module already on, reset it
    if (isOn()) {
      powerOnOff();
      assert(!isOn());
      delay(800);
      assert(!isOn());
    }
  
    // Turn module on
    assert(!isOn());
    powerOnOff();
    assert(isOn());
  
    // GSM
    gsm.begin(gsmUnsolicitedMessageHandler);


    // Turn off echo and enable flow control
    gsm.run_h1("ATE0+IFC=2,2", [](const String& msg) {
      assert(msg.startsWith("ATE0")); // gobble echo
    });

    // Enable unsolicited reporting of connection status
    gsm.run("AT+CGREG=1");

    // GPS
    gps.begin();

    // gsm-client
    gsm_client.begin(gpsPrimingCallback);
  
    logger.println("SimCom!");
  }

  void update(unsigned long timestamp, unsigned long delta)
  {
    gps.update(timestamp, delta);
    gsm.update(timestamp, delta);
    gsm_client.update(timestamp, delta);
    
    while (Serial.available()) {
      gsm.serial.write(Serial.read());
    }
  }
}  


    



// old junk
#if 0


const char* state_table[] = {
  0, // idle
  "AT+CSQ", // signal strength (abort if too low)
  0, // response
  "AT+SAPBR=3,1,\"Contype\",\"GPRS\"", // Set bearer parameter
  "AT+SAPBR=3,1,\"APN\",\"" APN "\"", // Set bearer context
  "AT+SAPBR=1,1", // Activate bearer context
  "AT+SAPBR=2,1", // Read bearer context - our IP
  0,
  "AT+CIPGSMLOC=1,1", // Read time and location for seeding GPS, but only do this once
  0
};

// AT+CSQ
// AT+SAPBR=3,1,"Contype","GPRS";+SAPBR=3,1,"APN","data.lyca-mobile.no"
// AT+SAPBR=1,1
// AT+SAPBR=2,1;+CIPGSMLOC=1,1

// GSM: Wait for stuff to come up
gsmSerial.setTimeout(10000);
gsmSerial.find("SMS Ready\r");
logger.println();
delay(1000);


void gsmGetTimeAndLocation()
{
  gsmSerial.println("AT+CIPGSMLOC=1,1");
  assert(gsmSerial.find("+CIPGSMLOC:") != -1);
  delay(100);
}


void getWeatherData(int lat, int lon) {
  gsmSerial.setTimeout(10000);


  gsmSerial.println("AT+HTTPINIT");
  assert(gsmSerial.find("OK\r") != -1);
  logger.println();

  gsmSerial.println(String("") + "AT+HTTPPARA=\"URL\",\"api.openweathermap.org/data/2.5/weather?lat=" + lat + "&lon=" + lon + "&APPID=" + OWM_APIKEY + "\"");
  assert(gsmSerial.find("OK\r") != -1);
  logger.println();

  gsmSerial.println("AT+HTTPACTION=0");
  assert(gsmSerial.find("OK\r") != -1);
  assert(gsmSerial.find("+HTTPACTION") != -1);
  logger.println();

  gsmSerial.println("AT+HTTPREAD");
  assert(gsmSerial.find("OK\r") != -1);
  logger.println();

  // +HTTPREAD: 107
  // {"cod":401, "message": "Invalid API key. Pleamap.org/faq#error401 for more info."}

}

void httpGet(const String& url, std::function<bool (const string&)> func)
{
  
  
}

#endif




