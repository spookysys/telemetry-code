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

	// Returns:
	// True if number of tokens in string matches elements in "toks" collection
	template<typename T>
	static bool tokenize(const String& str, T& toks, char separator=',')
	{
		int l_idx = 0, r_idx = 0;
		auto tok = toks.begin();
		while (r_idx>=0 && tok!=toks.end()) {
			r_idx = str.indexOf(separator, l_idx);
			if (r_idx<0) *tok = str.substring(l_idx);
			else *tok = str.substring(l_idx, r_idx);
			l_idx = r_idx + 1;
			tok++;
		}
		return (tok == toks.end()) && (r_idx == -1);
	}	
}

// define assert handler
#define assert(expr) do { if (!(expr)) { common::assertionHandler(#expr, __FILE__, __LINE__); } } while(0)

