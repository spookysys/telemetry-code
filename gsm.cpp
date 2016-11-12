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

  void begin()
  {
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
    logger.println("Essential config");
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
    if (current_task->done_handler) current_task->done_handler(err, current_task->knob);
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
        logger.println(String("Unhandled message: \"") + str + "\" while running \"" + current_task->cmd + "\"");
        assert(0);
      } else {
        logger.println(String("Unhandled message: \"") + str);
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
      bool status_bool = status==1 || status==5;
      logger.println(String("GPRS status: ") + status_bool);
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

