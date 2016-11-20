#include "gsm.hpp"
#include "logging.hpp"
#include "MySerial.hpp"
#include "watchdog.hpp"
#include <queue>

// APN setup
#define APN "data.lyca-mobile.no"
//#define APN_USER "lmno"
//#define APN_PW "plus"



namespace gsm
{
  Logger& logger = logging::get("gsm");


  class InitialRunnerImpl : public Runner
  {
    std::function<void(Task*)> enqueue;
    virtual Runner* thenGeneral(Task* task);
  public:
    InitialRunnerImpl(std::function<void(Task*)> enqueue_func) : enqueue(enqueue_func) {}
  };

  class CommandRunnerImpl : public Runner
  {
    Task& task;
    virtual Runner* thenGeneral(Task* task);
  public:
    CommandRunnerImpl(Task& task) : task(task) {}
  };


  class Task
  {
  public:
    CommandRunnerImpl runner;
    Task* next = nullptr;
    
    enum Type {
      TYPE_COMMAND = 1,
      TYPE_SYNC = 2
    } type;

    Task(Type type) 
      : type(type)
      , runner(*this)
      {}
  };
  
  class CommandTask : public Task
  {
    static Result defaultHandlerImpl(const String& msg, Runner* r) 
    {
      if (msg=="OK") return OK;
      else if (msg=="ERROR") return ERROR;
      return NOP;
    }
  public:
    const String cmd;
    unsigned long timeout = 0;
    const std::function<Result(const String&, Runner* runner)> handler = nullptr;
  public:
    CommandTask(const String& cmd, unsigned long timeout, std::function<Result(const String&, Runner*)> handler) 
      : Task(TYPE_COMMAND)
      , cmd(cmd)
      , timeout(timeout)
      , handler(handler ? handler : defaultHandlerImpl) 
      { assert(this->handler); }
  };

  class SyncTask : public Task
  {
  public:
    const std::function<Result(bool, Runner*)> handler;
    SyncTask(std::function<Result(bool, Runner*)> handler)
      : Task(TYPE_SYNC)
      , handler(handler) 
      { 
        assert(handler);
      }
  };

  Runner* InitialRunnerImpl::thenGeneral(Task* next) { 
    assert(next);
    enqueue(next);
    return &next->runner;
  }

  Runner* CommandRunnerImpl::thenGeneral(Task* next) {
    assert(next);
    Task* old_next = task.next;
    task.next = next;
    if (old_next) {
      Task* iter = next;
      while (iter->next) {
        iter = (Task*)iter->next;
      }
      iter->next = old_next;
    }
    return &next->runner;
  }

    

  Task* makeCommandTask(const String& cmd, unsigned long timeout, std::function<Result(const String&, Runner*)> handler) 
  { 
    return new CommandTask(cmd, timeout, handler); 
  }

  Task* makeSyncTask(std::function<Result(bool failed, Runner*)> handler)
  {
    return new SyncTask(handler);
  }


  class GsmLayer0 {
  public:
    MySerial serial = {"gsm", true, false};
  
    void beginL0() 
    {
      logger.println("Opening serial");
      watchdog::tickle();
      serial.begin_hs(19200, 3ul/*PA09 SERCOM2.1 RX<-GSM_TX */, 4ul/*PA08 SERCOM2.0 TX->GSM_RX*/, 2ul /* RTS PA14 SERCOM2.2 */, 5ul /* CTS PA15 SERCOM2.3 */, PIO_SERCOM_ALT, PIO_SERCOM_ALT, PIO_DIGITAL, PIO_DIGITAL, SERCOM_RX_PAD_1, UART_TX_PAD_0, &sercom2);
      watchdog::tickle();
    
      logger.println("Detecting baud");
      serial.setTimeout(100);
      for (int i = 0; i <= 10; i++) {
        serial.println("AT");
        watchdog::tickle();
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
        watchdog::tickle();
        assert(i < 10);
      }
      serial.setTimeout(1000);
      logger.println();
      watchdog::tickle();
    }    

    void updateL0() {}

    void IrqHandler() {
      serial.IrqHandler();
    }    
  };
  


  
  class GsmLayer1 : protected GsmLayer0 
  {
    std::queue<Task*> task_queue;
    CommandTask* current_task = nullptr;



    void startTask(Task* task, bool failed)
    {
      assert(!current_task);
      if (!task) return;
      if (task->type==Task::TYPE_COMMAND && !failed) {
        current_task = (CommandTask*)task;
        watchdog::tickle();
        serial.println(current_task->cmd);
        watchdog::tickle();
      } else if (task->type==Task::TYPE_COMMAND && failed) {
        Task* next = task->next;
        delete task;
        startTask(next, true);
      } else if (task->type==Task::TYPE_SYNC) {
        Result res = ((SyncTask*)task)->handler(failed, &task->runner);
        Task* next = task->next;
        delete task;
        if (res==NOP) startTask(next, failed);
        else if (res==OK) startTask(next, false);
        else if (res==ERROR) startTask(next, true);
        else assert(!"Invalid handler result");
      }
    }
    
    void finishTask(bool failed)
    {
      assert(current_task);
      Task* next = current_task->next;
      delete current_task;
      current_task = nullptr;
      startTask(next, failed);
    }

    void startQueuedTask()
    {
      Task* task = task_queue.front();
      task_queue.pop();
      startTask(task, false);
    }    

    InitialRunnerImpl initial_runner = {[this](Task* task){ 
      task_queue.push(task); 
    }};

    virtual bool unsolicitedMessageHandler(const String& msg) = 0;
  public:
    void beginL1()
    {
      beginL0();
    }
    
    void updateL1(unsigned long timestamp, unsigned long delta)
    {   
      updateL0();

      if (current_task && current_task->timeout > delta) current_task->timeout -= delta;
      else if (current_task) current_task->timeout = 0;
      
      while (serial.hasString())
      {
        String str = serial.popString();
        if (str.length()==0) {
          // ignore
        } else if (unsolicitedMessageHandler(str)) {
          // ok
        } else if (current_task) {
          Result res = current_task->handler(str, &current_task->runner);
          if (res==NOP) { /*keep running */ }
          else if (res==OK) finishTask(false);
          else if (res==ERROR) finishTask(true);
          else assert(!"Invalid handler result");
        } else if (current_task) {
          logger.println(String("Unhandled: \"") + str + "\" running \"" + current_task->cmd + "\"");
        } else {
          logger.println(String("Unhandled: \"") + str + "\"");
        }
      }

      if (current_task && current_task->timeout == 0) {
        logger.println(String("Timeout running \"") + current_task->cmd + "\"");
        finishTask(true);
      }

      if (!current_task && task_queue.size()) {
        logger.println("Starting queued task");
        startQueuedTask();
      }
    }

    Runner* runner() {
      return &initial_runner;
    }
  };
  
  
  class GsmLayer2 : protected GsmLayer1
  {
  private:
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
        /*"+HTTPACTION:",*/ "+FTP", /*"+CGREG:",*/ "ALARM RING", "+CALV:"
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
        if (msg.startsWith(gobbleList[i])) return true;
      }
      return false;
    }
  
  private:
    gsm::gps_priming_fn_t gps_priming_callback = nullptr;

    bool connected = false;
    bool gprs_status = false;

    bool maintainConnectionRunning = false;
    long signal_strength = 0;
  public:

    bool isMaintainConnectionRunning() {
      return maintainConnectionRunning;
    }
  
    void maintainConnection()
    {
      logger.println("Connection Maintenance");
      maintainConnectionRunning = true;
      
      runner()->then( // check signal and whether we need to set up bearer profile
        "AT+CSQ;+SAPBR=2,1", 10000, 
        [this](const String& msg, Runner* r) {
          if (msg.startsWith("+CSQ:")) { // remember signal strength until we receive "OK"
            signal_strength = msg.substring(6, msg.indexOf(',')).toInt();
          }
          else if (msg.startsWith("+SAPBR:")) {
            std::array<String, 2> toks;
            tokenize(msg, toks);
            if (toks[1]!="1") { // we need to set up bearer profile, so attach this task
              r->then("AT+SAPBR=3,1,\"Contype\",\"GPRS\";+SAPBR=3,1,\"APN\",\"" APN "\";+SAPBR=1,1", 20000);
            }
          }
          else if (msg=="OK" && signal_strength==0) {
            logger.println("No signal");
            return ERROR;
          }
          else if (msg=="OK") return OK;
          else if (msg=="ERROR") return ERROR;
          return NOP;
        }
      )->sync( // conclude on whether we managed to connect
        [this](bool failed, Runner* r) {
          maintainConnectionRunning = false;
          if (failed) {
            logger.println("Failed to connect.");
            connectionFailed();
            return ERROR;
          } else {
            logger.println("Connected.");
            gprs_status = true;
            connected = true;
            return OK;
          }
        }
      )->sync( // prime GPS on successful connection
        [this](bool failed, Runner* r) {
          if (!failed && gps_priming_callback) {
            logger.println("Requesting GSM location and time");
            r->then(
              "AT+CIPGSMLOC=1,1", 20000,
              [this](const String& msg, Runner* r) {
                if (msg.startsWith("+CIPGSMLOC")) {
                  std::array<String, 5> toks;
                  tokenize(msg, toks);
                  gps_priming_callback(toks[1], toks[2], toks[3], toks[4]);
                  gps_priming_callback = nullptr;
                } 
                else if (msg=="OK") return OK;
                else if (msg=="ERROR") return ERROR;
                return NOP;
              }
            );
          }
          return NOP;            
        }
      );
    }
    
    void connectionFailed()
    {
      if (connected) logger.println("Disconnected");
      connected = false;
    }
      
    void beginL2(gps_priming_fn_t gps_priming_callback) 
    {
      this->beginL1();
      this->gps_priming_callback = gps_priming_callback;
    }

    bool isConnected()
    {
      return connected;
    }
    
  };

  
  class GsmFacade : protected GsmLayer2
  {
  public:
    void begin(gps_priming_fn_t gps_priming_callback) 
    {
      this->beginL2(gps_priming_callback);
    }

    void update(unsigned long timestamp, unsigned long delta)
    {
      updateL1(timestamp, delta);

      while (Serial.available()) {
        serial.write(Serial.read());
      }
    }

    using GsmLayer1::runner;
    using GsmLayer0::IrqHandler;
    using GsmLayer2::isConnected;
    using GsmLayer2::connectionFailed;
    using GsmLayer2::maintainConnection;
    using GsmLayer2::isMaintainConnectionRunning;
  };


  
}

namespace gsm
{
  GsmFacade gsm_obj;
  
  void begin(gps_priming_fn_t gps_priming_callback)
  {
    gsm_obj.begin(gps_priming_callback);
  }

  void update(unsigned long timestamp, unsigned long delta)
  {
    gsm_obj.update(timestamp, delta);
  }

  bool isConnected()
  {
    return gsm_obj.isConnected();
  }

  void connectionFailed()
  {
    gsm_obj.connectionFailed();
  }

  void maintainConnection()
  {
    if (!gsm_obj.isMaintainConnectionRunning()) gsm_obj.maintainConnection();
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



