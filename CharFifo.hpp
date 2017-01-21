#pragma once
#include "common.hpp"
#include "events.hpp"


// Note: Depth must be power of two
template<int depth, bool isrSafe>
class CharFifo
{
	std::array<char, depth> data;
	int push_idx = 0;
	int pop_idx = 0;
	volatile bool full = false;
	volatile uint16_t num_newlines = 0;

	int wrap(int x)
	{
		return x & (depth-1);
	}

	String name;
	events::Channel<> line_chan;
	Stream& logger = Serial;
	bool using_line_chan = false;
public:

	CharFifo(const String& name) : name(name), line_chan(name)
	{
		assert(!(depth & (depth-1))); // Check that depth is power of 2
	}

	events::Channel<>& getLineChan()
	{
		using_line_chan = true;
		return line_chan;
	}

	void dump()
	{
		logger.println("Dump:");
		logger.println(String() + "num_newlines: " + num_newlines);
		bool go=full;
		for (int i=pop_idx; i!=push_idx || go; i=wrap(i+1), go=false) {
			if (isprint(data[i])) logger.write(data[i]);
			else logger.print(String("\\") + String(data[push_idx], HEX) + String("\\"));
		}
		logger.println();
	}

	void startDumping(unsigned long period)
	{
		events::Process::make(name).subscribe([&](unsigned long time, unsigned long delta) {
			dump();
		}).setPeriod(period);
	}
	
	bool isFull()
	{
		return full;
	}

	void push(char x)
	{
		if (!full) {
			data[push_idx] = x;
			push_idx = wrap(push_idx + 1);
			if (push_idx == depth) push_idx = 0;
			full = (push_idx == pop_idx);
			if (x=='\n' && num_newlines==0 && using_line_chan) {
				line_chan.post();
			}
			if (x=='\n') num_newlines++;
		}
	}


	bool hasLine() 
	{
		return num_newlines > 0;
	}

	String popLine()
	{
		if (!hasLine()) {
			assert(0);
			return String();
		}

		if (isrSafe) noInterrupts();

		// Remove string from fifo and find start/stop
		int start_idx = pop_idx;
		while (data[pop_idx] != '\n' && data[pop_idx] != '\r') {
			pop_idx = wrap(pop_idx + 1);
		}
		int stop_idx = pop_idx;

		// Calculate length of string
		int length = wrap(stop_idx - start_idx);
		if (full) length = depth;
		
		// Copy and condition string to stack
		std::array<char, depth+1> buff;
		buff[length] = 0;
		if (start_idx < stop_idx) {
			memcpy(buff.data(), data.data()+start_idx, length);
		} else if (length) {
			memcpy(buff.data(), data.data()+start_idx, depth-start_idx);
			memcpy(buff.data()+depth-start_idx, data.data(), stop_idx);
		}
		
		// Skip end-of-line-markers and decrement num_newlines
		if (data[pop_idx] == '\r') {
			pop_idx = wrap(pop_idx + 1);
		}
		if (data[pop_idx] == '\n') {
			pop_idx = wrap(pop_idx + 1);
			num_newlines--;
		} else assert(0);

		// fifo no longer full (if it was)
		if (pop_idx != start_idx) full = false;
		else assert(0);

		// Interrupts back on and return
		if (isrSafe) interrupts();
		return String(buff.data());
	}
	

};
