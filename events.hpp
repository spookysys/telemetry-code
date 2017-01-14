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
	};



	extern Process& makeProcess(const char* name);


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
		
		Channel<Params...>& subscribe(void (*callback)(unsigned long, Params...));
		
		Channel<Params...>& publish(long long time, Params... params);
		Channel<Params...>& publish(Params... params);
	};


	template<typename ... Params>
	Channel<Params...>& Channel<Params...>::subscribe(void (*callback)(unsigned long, Params...))
	{
		callbacks.push_back(callback);
		return *this;
	}
	
	template<typename ... Params>
	Channel<Params...>& Channel<Params...>::publish(long long time, Params... params)
	{
		publishImpl(time, std::bind(&Channel::callCallbacks, this, std::placeholders::_1, params...));
		return *this;
	}
	
	template<typename ... Params>
	Channel<Params...>& Channel<Params...>::publish(Params... params)
	{
		publishImpl(0, std::bind(&Channel::callCallbacks, this, std::placeholders::_1, params...));
		return *this;
	}


	template<typename ... Params>
	static Channel<Params...>& makeChannel(const char* name) 
	{
		return *new Channel<Params...>(name);
	}

}
