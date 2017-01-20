#pragma once
#include <Arduino.h>
#undef min
#undef max
#undef PIN_LED


namespace events 
{
	template<typename ... Params> class Channel;
}

namespace common 
{
	extern events::Channel<const char*>& assert_channel;
	void assertionHandler(const char* expr, const char* file, int line);
	extern unsigned long hasAsserted();
}


namespace misc
{
	template <typename T>
	static T swapEndianness(T u)
	{
		union {
			T u;
			uint8_t u8[sizeof(T)];
		} source, dest;
	
		source.u = u;
	
		for (size_t k = 0; k < sizeof(T); k++)
			dest.u8[k] = source.u8[sizeof(T) - k - 1];
	
		return dest.u;
	}

	template<typename T>
	static void tokenize(const String& str, T& toks, char separator=',')
	{
		int r_idx = -1;
		bool err = false;
		for (auto& iter : toks) {
			int l_idx = r_idx+1;
			r_idx = str.indexOf(separator, l_idx);
			if (l_idx<0) return;
			else if (r_idx<0) iter = str.substring(l_idx);
			else iter = str.substring(l_idx, r_idx);
		}
	}	
}

// define assert handler
#define assert(expr) do { if (!(expr)) { common::assertionHandler(#expr, __FILE__, __LINE__); } } while(0)

