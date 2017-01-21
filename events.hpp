#pragma once
#include "common.hpp"
#include <functional>
#include <vector>
#include <memory>

namespace events
{

	class Process
	{
	public:
		virtual Process& subscribe(std::function<void(unsigned long, unsigned long)>&& callback) = 0;
		virtual Process& setPeriod(unsigned long period) = 0;
		
		static Process& make(const char* name);
	};





	// Due to use of templates, we need to expose some implementation detail here
	class BaseChannel {
	protected:
		const char* name;
		void postImpl(unsigned long time, std::function<void(unsigned long)> cbCaller);
	public:
		BaseChannel(const char* name);
		virtual ~BaseChannel();
	};
	
	template<typename ... Params>
	class Channel : public BaseChannel
	{
		std::vector<std::function<void(unsigned long, Params...)>> callbacks;
		
		void callCallbacks(unsigned long time, Params... params) {
			for (auto& iter : callbacks) {
				iter(time, params...);
			}
		}
		
	public:
		Channel(const char* name) : BaseChannel(name) {}
		
		template<typename CallbackType>
		Channel<Params...>& subscribe(CallbackType callback) 
		{
			callbacks.push_back(callback);
			return *this;
		}

		Channel<Params...>& post(Params... params)
		{
			postImpl(0, std::bind(&Channel::callCallbacks, this, std::placeholders::_1, params...));
			return *this;
		}

		Channel<Params...>& postAt(unsigned long time, Params... params)
		{
			postImpl(time, std::bind(&Channel::callCallbacks, this, std::placeholders::_1, params...));
			return *this;
		}

		Channel<Params...>& postIn(unsigned long in_time, Params... params)
		{
			if (in_time==0) post(params...);
			else postAt(millis()+in_time, params...);
		}


		static Channel<Params...>& make(const char* name) 
		{
			return *new Channel<Params...>(name);
		}
	};



}
