#include "events.hpp"
#include "watchdog.hpp"
#include <vector>
#include <algorithm>
using namespace std;

namespace events
{
    Stream& logger = Serial;

    class ProcessImpl;

    struct ChannelEvent {
        bool valid;
        const BaseChannel* channel;
        unsigned long time;
        function<void(unsigned long)> callbackCaller;
    };
    
    
    
    vector<ChannelEvent>& channel_events() {
        static vector<ChannelEvent>* obj = new vector<ChannelEvent>;
        return *obj;
    }
    
    vector<ProcessImpl*>& processes() {
        static vector<ProcessImpl*>* obj = new vector<ProcessImpl*>;
        return *obj;
    }




    class ProcessImpl : public Process
    {
        const char* name;
        vector<function<void(unsigned long, unsigned long)>> callbacks;
        unsigned long period = 0;
        unsigned long times_run = 0;
        unsigned long time_spent = 0;
        unsigned long last_start_time = 0;
    public:
        ProcessImpl(const char* name) : name(name) 
        {
            processes().push_back(this);
        }
        
        virtual ~ProcessImpl() {
            assert(0);
        }
        
        virtual Process& subscribe(function<void(unsigned long, unsigned long)>&& cb)
        {
            callbacks.push_back(move(cb));
            return *this;
        };
        
        virtual Process& setPeriod(long long period) 
        {
            this->period = period;
            return *this;  
        };
        
        void runIfNeeded(unsigned long loop_time)
        {
            if (loop_time >= this->last_start_time+period) {
                unsigned long start_time = millis();
                for (auto& iter : callbacks) {
                    iter(start_time, start_time-last_start_time);
                }
                unsigned long stop_time = millis();
                time_spent += (stop_time-start_time);
                times_run++;
                last_start_time = start_time;
            }
        }
    };

    Process& makeProcess(const char* name)
    {
        return *new ProcessImpl(name);
    }


    // Due to use of templates, we need to expose some implementation detail here
    void BaseChannel::publishImpl(unsigned long time, function<void(unsigned long)> cbCaller)
    {
        ChannelEvent value = {true, this, time, cbCaller};
        noInterrupts();
        channel_events().emplace_back(value);
        interrupts();
    }

    BaseChannel::BaseChannel(const char* name) : name(name)
    {
    }
    
    BaseChannel::~BaseChannel()
    {
        assert(0);
    }


    void loop()
    {
        unsigned long time = millis();
        
        noInterrupts();

        //for (auto& iter : channel_events()) logger.write(iter.valid?'1':'0');
        //logger.println();
        
        // run events and flag executed ones as invalid
        for (int i = 0; i < (int)channel_events().size(); ) {
            auto& item_ref = channel_events()[i];
            if (item_ref.valid && time >= item_ref.time) {
                auto callbackCaller = item_ref.callbackCaller;
                item_ref.valid = false;
                interrupts(); // item_ref could now get clobbered
                callbackCaller(time);
                noInterrupts();
            } else {
                i++;
            }
        }
        
        //for (auto& iter : channel_events()) logger.write(iter.valid?'1':'0');
        //logger.println();

        // compact the list, removing invalid events
        {
            auto r = channel_events().begin();
            auto w = channel_events().begin();
            while (1) {
                while (r!=channel_events().end() && !r->valid) r++;
                if (r==channel_events().end()) break;
                w++ = r++;
            }
            channel_events().resize(w-channel_events().begin());
            for (auto& iter : channel_events()) assert(iter.valid);
        }

        //for (auto& iter : channel_events()) logger.write(iter.valid?'1':'0');
        //logger.println();

        interrupts();
        //logger.println();

        for (auto& iter : processes()) {
            iter->runIfNeeded(time);
        }

        watchdog::tickle();
    }
}

void loop() {    
    events::loop();
    for (int i=0; i<10; i++) delay(10);
}

