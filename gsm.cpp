#include "gsm.hpp"
#include "logging.hpp"
#include "MySerial.hpp"
#include <queue>

// APN setup
#define APN "data.lyca-mobile.no"
#define APN_USER "lmno"
#define APN_PW "plus"


namespace gsm { MySerial serial("gsm", true, true); }

void SERCOM2_Handler()
{
  gsm::serial.IrqHandler();
}




namespace gsm {
  Logger& logger = logging::get("gsm");
  
  bool unsolicitedMessageHandler(const String& op);


  bool gprs_status = false;
  long gprs_signal_strength = 0;
  bool gprs_bearer_status = false;
  unsigned long gprs_reconnect_in = 0;
  
  gps_priming_fn_t gps_priming_callback = nullptr;



  
  void gprs_connect()
  {
    assert(gprs_status==true);
    run("AT+CSQ;+SAPBR=2,1", [&](const String& msg) {
      if (msg.startsWith("+CSQ:")) {
        gprs_signal_strength = msg.substring(6, msg.indexOf(',')).toInt();
      } else if (msg.startsWith("+SAPBR:")) {
        int index0 = msg.indexOf(',');
        int index1 = msg.indexOf(',', index0+1);
        int status = msg.substring(index0+1, index1).toInt();
        gprs_bearer_status = (status==1);
      }
    }, [&](bool err, TaskKnob& k) {
      if (err || gprs_signal_strength<=1) {
        abort();
        gprs_reconnect_in = 10000;
      } else {
        if (gps_priming_callback) {
          k.then("AT+CIPGSMLOC=1,1", [&](const String& msg) {
            int index0 = msg.indexOf(',');
            int index1 = msg.indexOf(',', index0+1);
            int index2 = msg.indexOf(',', index1+1);
            int index3 = msg.indexOf(',', index2+1);
            gps_priming_callback(msg.substring(index0+1, index1), msg.substring(index1+1, index2), msg.substring(index2+1, index3), msg.substring(index3+1));
            gps_priming_callback = nullptr;
          }, [&](bool err, TaskKnob& k) {
            if (err) {
              abort();
              gprs_reconnect_in = 10000;
            }
          }, 10000);
        }
        if (!gprs_bearer_status) {
          k.then("AT+SAPBR=3,1,\"Contype\",\"GPRS\";+SAPBR=3,1,\"APN\",\"" APN "\";+SAPBR=1,1", [&](bool err, TaskKnob& k) {
            if (err) {
              abort();
              gprs_reconnect_in = 10000;
            }
          }, 10000);
        }
      }
    });
  }
  
  void gprs_disconnect()
  {
    assert(gprs_status==false);
    gprs_signal_strength = 0;
    gprs_bearer_status = false;
    gprs_reconnect_in = 0;
  }

  
  void begin(gps_priming_fn_t gps_priming_callback)
  {
    gsm::gps_priming_callback = gps_priming_callback;
    
    logger.println("Opening serial");
    serial.begin_hs(57600,  3ul/*PA09 SERCOM2.1 RX<-GSM_TX */,  4ul/*PA08 SERCOM2.0 TX->GSM_RX*/, 2ul /* RTS PA14 SERCOM2.2 */, 5ul /* CTS PA15 SERCOM2.3 */, PIO_SERCOM_ALT, PIO_SERCOM_ALT, PIO_DIGITAL, PIO_DIGITAL, SERCOM_RX_PAD_1, UART_TX_PAD_0, &sercom2);
  
    logger.println("Detecting baud");
    serial.setTimeout(100);
    for (int i = 0; i <= 10; i++) {
      serial.println("AT");
      if (serial.find("OK\r")) break;
      assert(i < 10);
    }
    serial.setTimeout(1000);
    logger.println();

    // Turn on flow control, disable echo, enable network status messages
    logger.println("Setup");
    serial.setTimeout(100);
    for (int i = 0; i <= 20; i++) {
      serial.println("AT+IFC=2,2;E0;+CGREG=1");
      if (serial.find("OK\r")) break;
      assert(i < 10);
    }
    serial.setTimeout(1000);
    logger.println();

    logger.println("Setup done!");
  }



  class Task
  {
  public:
    bool  prev = false;
    Task* next = nullptr;
    void  insert(Task* new_next);
    TaskKnob knob;
    const String cmd;
    const std::function<void(bool err, TaskKnob&)> done_handler;
    const std::function<void(const String&)> message_handler = nullptr;
    unsigned long timeout = 0;
    Task(const String& cmd, std::function<void(const String&)> message_handler, std::function<void(bool err, TaskKnob&)> done_handler, unsigned long timeout) : knob(this), cmd(cmd), message_handler(message_handler), done_handler(done_handler), timeout(timeout) {}
  };


  std::queue<Task*> task_queue;
  Task* current_task = nullptr;

  void start_task(Task* task)
  {
    assert(!current_task);
    current_task = task;
    serial.println(current_task->cmd);
  }

  void finish_task(bool err) {
    assert(current_task);
    // run done-handler
    if (current_task->done_handler) {
      current_task->done_handler(err, current_task->knob);
    } else if (err) {
      current_task->knob.abort();
    }
    // delete this task and start next task
    Task* next = current_task->next;
    delete current_task;
    current_task = nullptr;
    start_task(next);
  }

  void update(unsigned long timestamp, unsigned long delta)
  {   
    if (current_task) {
      if (current_task->timeout > delta) current_task->timeout -= delta;
      else current_task->timeout = 0;
    }
    
    while (serial.hasString())
    {
      String str = serial.popString();
      if (str.length()==0) {
        // ignore
      } else if (unsolicitedMessageHandler(str)) {
        // ok
      } else if (current_task && str=="OK") {
        finish_task(false);
      } else if (current_task && str=="ERROR") {
        logger.println(String("Error while running \"") + current_task->cmd + "\"");
        finish_task(true);
      } else if (current_task && current_task->message_handler) {
        current_task->message_handler(str);
      } else if (current_task) {
        logger.println(String("Unhandled: \"") + str + "\" running \"" + current_task->cmd + "\"");
        assert(0);
      } else {
        logger.println(String("Unhandled: \"") + str);
        assert(0);
      }
    }

    if (current_task && current_task->timeout == 0) {
      logger.println(String("Timeout while running \"") + current_task->cmd + "\"");
      finish_task(true);
    }

    if (!current_task && task_queue.size()) {
      Task* tmp = task_queue.front();
      task_queue.pop();
      start_task(tmp);
    }

    if (gprs_reconnect_in > delta) {
      gprs_reconnect_in -= delta;
    } else if (gprs_reconnect_in > 0) {
      gprs_reconnect_in = 0;
      if (gprs_status) gprs_connect();
    }
  }



  Task* make_task(const String& cmd, std::function<void(const String&)> message_handler, std::function<void(bool err, TaskKnob&)> done_handler, unsigned long timeout) { 
    return new Task(cmd, message_handler, done_handler, timeout); 
  }
  Task* make_task(const String& cmd, std::function<void(const String&)> message_handler, unsigned long timeout) { 
    return new Task(cmd, message_handler, nullptr, timeout); 
  }
  Task* make_task(const String& cmd, std::function<void(bool, TaskKnob&)> done_handler, unsigned long timeout) { 
    return new Task(cmd, nullptr, done_handler, timeout); 
  }
  Task* make_task(const String& cmd, unsigned long timeout) { 
    return new Task(cmd, nullptr, nullptr, timeout); 
  }


  void Task::insert(Task* new_next)
  {
    assert(new_next);
    assert(!new_next->prev);
    Task* old_next = this->next;
    this->next = new_next;
    new_next->prev = true;
    Task* tmp = new_next;
    while (tmp->next) tmp = tmp->next;
    tmp->next = old_next;
  }

  TaskKnob& TaskKnob::abort(int num) 
  {
    assert(task);
    Task* iter = task->next;
    for (int i=0; i<num && iter; i++) {
      Task* tmp = iter;
      iter = iter->next;
      delete tmp;
    }
    task->next = iter;
  }

  TaskKnob& TaskKnob::then_task(Task* new_next)
  {
    task->insert(new_next);
    return new_next->knob;
  }

  TaskKnob& run_task(Task* task)
  {
    task_queue.push(task);
    return task->knob;
  }


  
  bool unsolicitedMessageHandler(const String& msg)
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
    
    if (msg.startsWith("+CGREG: ")) {
      int status = msg.substring(8, 9).toInt();
      assert(status>=0);
      //gsm_client.updateNetworkStatus(status==1 || status==5);
      bool old_status = gprs_status;
      gprs_status = status==1 || status==5;
      if (!old_status && gprs_status) gprs_connect();
      else if (!gprs_status) gprs_disconnect();
      return true;
    }

    for (int i=0; i<sizeof(gobbleList)/sizeof(*gobbleList); i++) {
      if (msg.startsWith(gobbleList[i])) {
        //logger.print(String("\"") + msg + "\" - gobbled by callback");
        return true;
      }
    }
    return false;
  }

}

