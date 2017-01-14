#pragma once

// Create streams that output data to server, PC via usb, and to files on the SD drive
namespace logging
{
	void setup();

	enum Level
	{
		ALERT=1,  // Primary information, should be read by operator
		INFO=2,   // Status information, might be read by operator
		LOG=3,    // Suitable for human consumption at a later date
		WEB=4,    // Too fast for human, slow enough for internet
		FLASH=5,  // Too fast for internet, store on flash
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
	};

	Logger& makeLogger(const char* name, Level baseLevel);
}

