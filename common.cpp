#include "common.hpp"
#include "pins.hpp"
#include "events.hpp"


namespace 
{
	Stream& logger = Serial;
	unsigned long has_asserted = 0;
}

namespace common
{
	events::Channel<const char*>& assert_channel = events::Channel<const char*>::make("assert");

	unsigned long hasAsserted()
	{
		return has_asserted;
	}

	void assertionHandler(const char* expr, const char* file, int line)
	{
		unsigned long time = millis();
		
		// for hasAsserted()
		if (!has_asserted) {
			has_asserted = time;
		}
		
		// make error
		const String msg = String("Assertion failed at ") +  String(time) + ": " + expr + " in " + file + ":" + String(line);

		// log error
		logger.println(msg);
		logger.flush();

		// Note: Gets posted only after setup() finished
		assert_channel.post(msg.c_str());
	}  
}
// hack for basic STL
// https://forum.pjrc.com/threads/23467-Using-std-vector?p=69787&viewfull=1#post69787

namespace std {
	void __throw_bad_alloc()
	{
		assert(!"Unable to allocate memory");
		while(1);
	}

	void __throw_bad_function_call()
	{
		assert(!"Bad function call");
		while(1);
	}

	void __throw_length_error( char const*e )
	{
		assert(!(String("Length Error :")+e));
		while(1);
	}
}

