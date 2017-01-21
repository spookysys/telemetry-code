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
		String name;
		vector<function<void(unsigned long, unsigned long)>> callbacks;
		unsigned long period = 0;
		unsigned long times_run = 0;
		unsigned long time_spent = 0;
		unsigned long last_start_time = 0;
	public:
		ProcessImpl(const String& name) : name(name) 
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
		
		virtual Process& setPeriod(unsigned long period) 
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

	Process& Process::make(const String& name)
	{
		auto tmp = new ProcessImpl(name);
		return *tmp;
	}


	// Due to use of templates, we need to expose some implementation detail here
	void BaseChannel::postImpl(unsigned long time, function<void(unsigned long)> cbCaller)
	{
		ChannelEvent value = {true, this, time, cbCaller};
		noInterrupts();
		channel_events().emplace_back(value);
		interrupts();
	}

	BaseChannel::BaseChannel(const String& name) : name(name)
	{
	}
	
	BaseChannel::~BaseChannel()
	{
		assert(0);
	}


	void loop()
	{
		unsigned long time = millis();
		
		// Stop interrupts for channel selection
		noInterrupts();
		
		// run events and flag executed ones as invalid
		for (int i = 0; i < (int)channel_events().size(); ) {
			auto& item_ref = channel_events()[i];
			if (item_ref.valid && time >= item_ref.time) {
				item_ref.valid = false;
				// Enable interrupts while running the channel
				auto callbackCaller = item_ref.callbackCaller;
				interrupts(); // item_ref could now get clobbered
				callbackCaller(time);
				noInterrupts();
			} else {
				i++;
			}
		}
		
		// compact the list, removing invalid events
		{
			int num_valid = 0;
			for (const auto& iter : channel_events()) if (iter.valid) num_valid++;
			auto r = channel_events().begin();
			auto w = channel_events().begin();
			while (1) {
				while (r!=channel_events().end() && !r->valid) r++;
				if (r==channel_events().end()) break;
				*w++ = *r++;
			}
			channel_events().resize(w-channel_events().begin());
			for (const auto& iter : channel_events()) assert(iter.valid);
			assert(channel_events().size()==num_valid);
		}

		// Continue interrupts for channel selection
		interrupts();

		// Run processes
		for (auto& iter : processes()) {
			iter->runIfNeeded(time);
		}

		// Ticke watchdog
		watchdog::tickle();
	}
}

// Main arduino loop!
void loop() {    
	events::loop();
	delay(100);
}

