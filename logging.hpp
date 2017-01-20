#pragma once

// Create streams that output data to server, PC via usb, and to files on the SD drive
namespace logging
{
	void setup();

	enum Level
	{
		ALERT=1,    // Priority information, should be read by operator
		HUMAN=2,    // Suitable for human consumption (low bandwidth)
		SERVER=3,   // Send to server over TCP (medium bandwidth)
		REALTIME=4  // Too fast for internet, store on flash (maybe send over UDP) (high bandwidth)
	};

	struct LoggerCommon
	{
		void pushLevel(Level op);
		void popLevel();
	};

	struct LoggerArray : public LoggerCommon
	{
		void endArray();
		void null();
		void boolean(bool val);
		void number(int8_t val);
		void number(uint8_t val);
		void number(int16_t val);
		void number(uint16_t val);
		void number(int32_t val);
		void number(uint32_t val);
		void number(int64_t val);
		void number(uint64_t val);
		void number(float val);
		void number(double val);
		void string(const char* val);
		void string(const String& val);
		LoggerDict& beginDict();
		LoggerArray& beginArray();
	};

	struct LoggerDict : public LoggerCommon {
		void endDict();
		void null(const char* name);
		void boolean(const char* name, bool val);
		void number(const char* name, int8_t val);
		void number(const char* name, uint8_t val);
		void number(const char* name, int16_t val);
		void number(const char* name, uint16_t val);
		void number(const char* name, int32_t val);
		void number(const char* name, uint32_t val);
		void number(const char* name, int64_t val);
		void number(const char* name, uint64_t val);
		void number(const char* name, float val);
		void number(const char* name, double val);
		void string(const char* name, const char* val);
		void string(const char* name, const String& val);
		LoggerArray& beginArray(const char* name);
		LoggerDict& beginDict(const char* name);
	};

	class Logger : public LoggerArray
	{
	public:

		static Logger& make(const char* name, Level baseLevel);
	};

}

