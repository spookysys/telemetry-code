#include "gsm.hpp"
#include "logging.hpp"
#include "MySerial.hpp"
#include <queue>

// APN setup
#define APN "data.lyca-mobile.no"
#define APN_USER "lmno"
#define APN_PW "plus"


namespace gsm
{
  Logger& logger = logging::get("gsm");

  
  class InitialRunnerImpl : public Runner
  {
    std::function<void(Task*)> enqueue;
    virtual Runner* thenGeneral(Task* then_task);
  public:
    InitialRunnerImpl(std::function<void(Task*)> enqueue_func) : enqueue(enqueue_func) {}
    void fail() { assert(0); }
    void finally(std::function<void(bool err)> finally_handler) { assert(0); }
  };


  class RunnerImpl : public Runner
  {
    Task& task;
    virtual Runner* thenGeneral(Task* task);
  public:
    RunnerImpl(Task& task) : task(task) {}
    void fail() { logger.println("Runner.fail() not implemented"); }
    void finally(std::function<void(bool err)> finally_handler) { logger.println("Runner.finally not implemented"); }
  };

  
  class Task
  {
  public:
    Task* next = nullptr;
    static void insert(Task* task, Task* next);
    static void cancel(Task* task, int num);
    static void cancelAll(Task* task) { cancel(task, 0x7FFF); }
  public:
    RunnerImpl runner;
    const String cmd;
    unsigned long timeout = 0;
    const std::function<void(Runner*)> done_handler = nullptr;
    const std::function<void(const String&)> message_handler = nullptr;
    Task(const String& cmd, unsigned long timeout, std::function<void(const String&)> message_handler, std::function<void(Runner*)> done_handler) : runner(*this), cmd(cmd), timeout(timeout), message_handler(message_handler), done_handler(done_handler) {}
  };

  Runner* RunnerImpl::thenGeneral(Task* then_task) {
    assert(then_task);
    Task::insert(&this->task, then_task);
    return &then_task->runner;
  }

  Runner* InitialRunnerImpl::thenGeneral(Task* then_task) { 
    assert(then_task);
    enqueue(then_task); 
    return &then_task->runner;
  }
    
  void Task::insert(Task* task, Task* next)
  {
    assert(task && next);
    Task* old_next = task->next;
    task->next = next;
    Task* iter = next;
    while (iter->next) iter = iter->next;
    iter->next = old_next;
  }

  void Task::cancel(Task* task, int num)
  {
    Task* iter = task->next;
    for (int i=0; i<num && iter; i++) {
      Task* next = iter->next;
      delete iter;
      iter = next;
    }
    task->next = iter;
  }

  Task* makeTask(const String& cmd, unsigned long timeout, std::function<void(const String&)> message_handler, std::function<void(Runner*)> done_handler) { 
    return new Task(cmd, timeout, message_handler, done_handler); 
  }


  class GsmLayer0 {
  public:
    MySerial serial = {"gsm", true, true};
  
    void beginL0() 
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
      logger.println("Setup");
      serial.setTimeout(100);
      for (int i = 0; i <= 20; i++) {
        serial.println("AT+IFC=2,2;E0;+CGREG=1");
        if (serial.find("OK\r")) break;
        assert(i < 10);
      }
      serial.setTimeout(1000);
      logger.println();      
    }    

    void updateL0() {}

    void IrqHandler() {
      serial.IrqHandler();
    }    
  };
  


  
  class GsmLayer1 : protected GsmLayer0 
  {
    std::queue<Task*> task_queue;
    Task* current_task = nullptr;

    void startTask(Task* task)
    {
      assert(task && !current_task);
      current_task = task;
      serial.println(current_task->cmd);
    }
  
    void finishTask(bool err) {
      assert(current_task);
      if (!err) {
        if (current_task->done_handler) current_task->done_handler(&current_task->runner);
        Task* next = current_task->next;
        delete current_task;
        current_task = nullptr;
        if (next) startTask(next);
      } else {
        Task::cancelAll(current_task);
        delete current_task;
        current_task = nullptr;
      }
    }

    InitialRunnerImpl initial_runner = { [this](Task* task){ task_queue.push(task); } };

    virtual bool unsolicitedMessageHandler(const String& msg) = 0;
  public:
    void beginL1()
    {
      beginL0();
    }
    
    void updateL1(unsigned long timestamp, unsigned long delta)
    {   
      updateL0();
      
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
          finishTask(false);
        } else if (current_task && str=="ERROR") {
          logger.println(String("Error while running \"") + current_task->cmd + "\"");
          finishTask(true);
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
        finishTask(true);
      }

      if (!current_task && task_queue.size()) {
        logger.println("Popping task_queue");
        Task* tmp = task_queue.front();
        task_queue.pop();
        startTask(tmp);
      }
    }

    Runner* runner() {
      return &initial_runner;
    }
  };
  
  
  class GsmLayer2 : protected GsmLayer1
  {
  private:
    gsm::gps_priming_fn_t gps_priming_callback = nullptr;
    
    bool gprs_status = false;

    unsigned long maintain_countdown = 0;

    
    void scheduleMaintenance() 
    {
      logger.println("scheduleMaintenance()");
      assert(gprs_status);
      maintain_countdown = 30000;
    }
      

    void connectionFailed()
    {
      logger.println("connectionFailed()");
      maintain_countdown = 0;
      if (gprs_status) scheduleMaintenance();
    }

    long tmp_signal_strength = 0;
    bool tmp_bearer_status = false;
    void maintainConnection()
    {
      logger.println("maintainConnection()");
      assert(gprs_status);
      assert(maintain_countdown==0);
      tmp_signal_strength = 0;
      tmp_bearer_status = false;
      
      runner()->then(
        "AT+CSQ;+SAPBR=2,1", 
        10000, 
        [&](const String& msg) {
          if (msg.startsWith("+CSQ:")) {
            tmp_signal_strength = msg.substring(6, msg.indexOf(',')).toInt();
          } else if (msg.startsWith("+SAPBR:")) {
            int index0 = msg.indexOf(',');
            int index1 = msg.indexOf(',', index0+1);
            int status = msg.substring(index0+1, index1).toInt();
            tmp_bearer_status = (status==1);
          }
        }, 
        [&](Runner* r) {
          if (tmp_signal_strength<=1) {
            r->fail();
            return;
          }
          
          if (!tmp_bearer_status) {
            r = r->then("AT+SAPBR=3,1,\"Contype\",\"GPRS\";+SAPBR=3,1,\"APN\",\"" APN "\";+SAPBR=1,1", 4000, nullptr, nullptr);
          }
  
          if (gps_priming_callback) {
            r = r->then(
              "AT+CIPGSMLOC=1,1", 10000,
              [&](const String& msg) {
                int index0 = msg.indexOf(',');
                int index1 = msg.indexOf(',', index0+1);
                int index2 = msg.indexOf(',', index1+1);
                int index3 = msg.indexOf(',', index2+1);
                gps_priming_callback(msg.substring(index0+1, index1), msg.substring(index1+1, index2), msg.substring(index2+1, index3), msg.substring(index3+1));
                gps_priming_callback = nullptr;
              },
              nullptr
            );
          }
        }
      )->finally(
        [&](bool err) {
          if (err) {
            connectionFailed();
            scheduleMaintenance();
          }
        }
      );
    }
    

  
  
    bool unsolicitedMessageHandler(const String& msg)
    {
      static const char* gobbleList[] = {
        "+CCWA:", "+CLIP:", "+CRING:", "+CREG:", "+CCWV:", "+CMTI:", "+CMT:", "+CBM:", "+CDS:", "+COLP:",
        "+CSSU:", "+CSSI:", "+CLCC:", "*PSNWID:", "*PSUTTZ:", "+CTZV:", "DST:", "+CSMINS:", "+CDRIND:",
        "+CHF:", "+CENG:", "MO RING", "MO CONNECTED", "+CPIN: READY", "+CSQN:", "+SIMTONE:", "+STTONE:", 
        "+CR:", "+CUSD:", "RING", "NORMAL POWER DOWN", "+CMTE:", "UNDER-VOLTAGE POWER DOWN", "UNDER-VOLTAGE WARNING", 
        "UNDER-VOLTAGE WARNNING", "OVER-VOLTAGE POWER DOWN", "OVER-VOLTAGE WARNING", "OVER-VOLTAGE WARNNING",
        "CHARGE-ONLY MODE", "RDY", "Call Ready", "SMS Ready", "+CFUN:", /*[<n>,]CONNECT OK*/ "CONNECT",
        /*[<n>,]CONNECT FAIL*/ /*[<n>,]ALREADY CONNECT*/ /*[<n>,]SEND OK*/ /*[<n>,]CLOSED*/ "RECV FROM:",
        "RECV FROM:", "+IPD,", "+RECEIVE,", "REMOTE IP:", "+CDNSGIP:", "+PDP: DEACT", /*"+SAPBR",*/
        "+HTTPACTION:", "+FTP", /*"+CGREG:",*/ "ALARM RING", "+CALV:"
      };
      
      if (msg.startsWith("+CGREG: ")) {
        int status = msg.substring(8, 9).toInt();
        assert(status>=0);
        //gsm_client.updateNetworkStatus(status==1 || status==5);
        bool old_status = gprs_status;
        this->gprs_status = (status==1 || status==5);
        if (!old_status && gprs_status) maintainConnection();
        else if (!gprs_status) connectionFailed();
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

      
  public:
  
    void beginL2(gps_priming_fn_t gps_priming_callback) 
    {
      this->beginL1();
      this->gps_priming_callback = gps_priming_callback;
    }

    void updateL2(unsigned long timestamp, unsigned long delta)
    {
      updateL1(timestamp, delta);
      
      if (maintain_countdown > delta) {
        maintain_countdown -= delta;
      } else if (maintain_countdown > 0) {
        maintain_countdown = 0;
        assert(gprs_status);
        if (gprs_status) maintainConnection();
      }
    }
  
  };
  
  
  class GsmLayer3 : protected GsmLayer2
  {
  public:
    void begin(gps_priming_fn_t gps_priming_callback) 
    {
      this->beginL2(gps_priming_callback);
      logger.println("inited!");
    }

    void update(unsigned long timestamp, unsigned long delta)
    {
      updateL2(timestamp, delta);
    }

    using GsmLayer1::runner;

    using GsmLayer0::IrqHandler;
  };


  
}

namespace gsm
{
  GsmLayer3 gsm_obj;
  
  void begin(gps_priming_fn_t gps_priming_callback)
  {
    gsm_obj.begin(gps_priming_callback);
  }

  void update(unsigned long timestamp, unsigned long delta)
  {
    gsm_obj.update(timestamp, delta);
  }


  Runner* runner()
  {
    return gsm_obj.runner();
  }
}


void SERCOM2_Handler()
{
  gsm::gsm_obj.IrqHandler();
}



