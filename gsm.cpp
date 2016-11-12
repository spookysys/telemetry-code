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
    void fail() 
    { 
      assert(0); 
    }
  };

  class CommandTask;
  class CommandRunnerImpl : public Runner
  {
    CommandTask& task;
    virtual Runner* thenGeneral(Task* task);
  public:
    CommandRunnerImpl(CommandTask& task) : task(task) {}
    void fail() 
    { 
      logger.println("Runner.fail() not implemented"); 
    }
  };

  class Task
  {
  public:
    enum Type {
      TYPE_COMMAND = 1,
      TYPE_FINALLY = 2
    } type;

    Task(Type type) 
      : type(type)
      {}
  };
  
  class CommandTask : public Task
  {
  public:
    CommandRunnerImpl runner;
    Task* next = nullptr;
  public:
    const String cmd;
    unsigned long timeout = 0;
    const std::function<void(const String&)> message_handler = nullptr;
    const std::function<bool(Runner*)> done_handler = nullptr;
  public:
    CommandTask(const String& cmd, unsigned long timeout, std::function<void(const String&)> message_handler, std::function<bool(Runner*)> done_handler) 
      : Task(TYPE_COMMAND)
      , runner(*this)
      , cmd(cmd)
      , timeout(timeout)
      , message_handler(message_handler)
      , done_handler(done_handler) 
      {}
  };

  class FinallyTask : public Task
  {
  public:
    const std::function<void(bool)> finally_handler;
    FinallyTask(std::function<void(bool)> finally_handler) 
      : Task(TYPE_FINALLY)
      , finally_handler(finally_handler) 
      {}
  };

  Runner* InitialRunnerImpl::thenGeneral(Task* next) { 
    assert(next);
    assert(next->type==Task::TYPE_COMMAND);
    enqueue((CommandTask*)next); 
    return &((CommandTask*)next)->runner;
  }

  Runner* CommandRunnerImpl::thenGeneral(Task* next) {
    assert(next);
    
    Task* old_next = task.next;
    task.next = next;

    if (old_next) {
      assert(next->type==Task::TYPE_COMMAND);
      CommandTask* iter = (CommandTask*)next;
      while (iter->next) {
        assert(iter->next->type==Task::TYPE_COMMAND);
        iter = (CommandTask*)iter->next;
      }
      iter->next = old_next;
    }
    
    if (next->type==Task::TYPE_COMMAND) return &((CommandTask*)next)->runner;
    else return nullptr;
  }

    

  Task* makeCommandTask(const String& cmd, unsigned long timeout, std::function<void(const String&)> message_handler, std::function<bool(Runner*)> done_handler) 
  { 
    return new CommandTask(cmd, timeout, message_handler, done_handler); 
  }

  Task* makeFinallyTask(std::function<void(bool err)> handler)
  {
    return new FinallyTask(handler);
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
    std::queue<CommandTask*> task_queue;
    CommandTask* current_task = nullptr;

    void startTask(Task* task)
    {
      assert(task && !current_task);
      if (task->type==Task::TYPE_COMMAND) {
        current_task = (CommandTask*)task;
        serial.println(((CommandTask*)task)->cmd);
      } else if (task->type==Task::TYPE_FINALLY) {
        ((FinallyTask*)task)->finally_handler(false);
      }
    }

    void failCurrentTask()
    {
      assert(current_task);
      while (current_task) {
        Task* tmp = current_task->next;
        delete current_task;
        current_task = nullptr;
        if (tmp->type==Task::TYPE_COMMAND) {
          current_task = (CommandTask*)tmp;
        } else if (tmp->type==Task::TYPE_FINALLY) {
          ((FinallyTask*)tmp)->finally_handler(true);
          delete tmp;
          break;
        }
      }
    }

    void resolveCurrentTask()
    {
      assert(current_task);
      Task* next = current_task->next;
      delete current_task;
      current_task = nullptr;
      startTask(next);
    }

    InitialRunnerImpl initial_runner = {[this](Task* task){ 
      assert(task->type==Task::TYPE_COMMAND);
      task_queue.push((CommandTask*)task); 
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
          bool ok = true;
          if (current_task->done_handler) ok = current_task->done_handler(&current_task->runner);
          if (ok) resolveCurrentTask();
          else failCurrentTask();
        } else if (current_task && str=="ERROR") {
          logger.println(String("Error while running \"") + current_task->cmd + "\"");
          failCurrentTask();
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
        failCurrentTask();
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

    bool connected = false;
    bool gprs_status = false;

    unsigned long connect_countdown = 0;

    
    void scheduleReconnect() 
    {
      assert(gprs_status);
      logger.println("Scheduling connection maintenance in some seconds");
      connect_countdown = 15000;
    }
      

    void connectionFailed()
    {
      if (connected) logger.println("Disconnected");
      connected = false;
      connect_countdown = 0;
      if (gprs_status) scheduleReconnect();
    }

    long tmp_signal_strength = 0;
    bool tmp_bearer_status = false;
    bool tmp_msg_error = false;
    void connect()
    {
      logger.println("Connecting");
      assert(gprs_status);
      assert(connect_countdown==0);
      tmp_signal_strength = 0;
      tmp_bearer_status = false;
      tmp_msg_error = false;
      
      runner()->then(
        "AT+CSQ;+SAPBR=2,1", 
        10000, 
        [&](const String& msg) {
          if (msg.startsWith("+CSQ:")) {
            tmp_signal_strength = msg.substring(6, msg.indexOf(',')).toInt();
          } else if (msg.startsWith("+SAPBR:")) {
            int index0 = msg.indexOf(',');
            int index1 = msg.indexOf(',', index0+1);
            tmp_msg_error = (index0<0 || index1<0);
            if (!tmp_msg_error) {
              int status = msg.substring(index0+1, index1).toInt();
              tmp_bearer_status = (status==1);
            }
          }
        }, 
        [&](Runner* r) {
          if (tmp_msg_error || tmp_signal_strength<=1) return false; // fail
          
          if (!tmp_bearer_status) {
            r = r->then("AT+SAPBR=3,1,\"Contype\",\"GPRS\";+SAPBR=3,1,\"APN\",\"" APN "\";+SAPBR=1,1", 90000, nullptr, nullptr);
          }
  
          if (gps_priming_callback) {
            r = r->then(
              "AT+CIPGSMLOC=1,1", 61000,
              [&](const String& msg) {
                int index0 = msg.indexOf(',');
                int index1 = msg.indexOf(',', index0+1);
                int index2 = msg.indexOf(',', index1+1);
                int index3 = msg.indexOf(',', index2+1);
                tmp_msg_error = (index0<0 || index1<0 || index2<0 || index3<0);
                if (!tmp_msg_error) {
                  gps_priming_callback(msg.substring(index0+1, index1), msg.substring(index1+1, index2), msg.substring(index2+1, index3), msg.substring(index3+1));
                  gps_priming_callback = nullptr;
                }
              },
              [&](Runner* r) {
                return !tmp_msg_error;
              }
            );
          }
          
          return true;
        }
      )->finally(
        [&](bool err) {
          if (err || !gprs_status) {
            logger.println("Failed to connect.");
            connectionFailed();
          } else {
            logger.println("Connected.");
            connected = true;
            scheduleReconnect();
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
        if (!old_status && gprs_status) connect();
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
      
      if (connect_countdown > delta) {
        connect_countdown -= delta;
      } else if (connect_countdown > 0) {
        connect_countdown = 0;
        assert(gprs_status);
        if (gprs_status) connect();
      }
    }

    bool isConnected()
    {
      return connected;
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
    using GsmLayer2::isConnected;
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

  bool isConnected()
  {
    return gsm_obj.isConnected();
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



