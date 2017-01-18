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
		virtual Process& setPeriod(long long period) = 0;
		
		static Process& make(const char* name);
	};





	// Due to use of templates, we need to expose some implementation detail here
	class BaseChannel {
	protected:
		const char* name;
		void publishImpl(unsigned long time, std::function<void(unsigned long)> cbCaller);
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

		Channel<Params...>& publish(Params... params)
		{
			publishImpl(0, std::bind(&Channel::callCallbacks, this, std::placeholders::_1, params...));
			return *this;
		}

		Channel<Params...>& publishAt(long long time, Params... params)
		{
			publishImpl(time, std::bind(&Channel::callCallbacks, this, std::placeholders::_1, params...));
			return *this;
		}

		Channel<Params...>& publishIn(long long in_time, Params... params)
		{
			if (in_time==0) publish(params...);
			else publishAt(millis()+in_time, params...);
		}


		static Channel<Params...>& make(const char* name) 
		{
			return *new Channel<Params...>(name);
		}
	};



}
