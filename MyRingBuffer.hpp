#pragma once
#include "common.hpp"
#include <array>

// ring buffer for serials
template<int S>
class MyRingBuffer
{
private:
  std::array<char, S> buff;
  int push_idx;
  int pop_idx;
  static bool is_term(char ch)
  {
    return ch==0 || ch=='\n' || ch=='\r';
  }
  static int next_idx(int index)
  {
    return (uint32_t)(index + 1) % S;
  }
public:
  
  MyRingBuffer()
  {
    memset(buff.data(), 0, S);
    clear();
  }

  void clear()
  {
    push_idx = 0;
    pop_idx = 0;
  }
  
  int capacity() 
  {
    return S-1;
  }

  int available()
  {
    int delta = push_idx - pop_idx;  
    if(delta < 0)
      return S + delta;
    else
      return delta;
  }

  bool is_full()
  {
    return (next_idx(push_idx) == pop_idx);
  }

  void push(char c)
  {
    assert(next_idx(push_idx) != pop_idx);
    buff[push_idx] = c;
    push_idx = next_idx(push_idx);
  }

  int pop()
  {
    assert(pop_idx != push_idx);
    auto value = buff[pop_idx];
    pop_idx = next_idx(pop_idx);
    return value;
  }

  int peek()
  {
    assert(pop_idx != push_idx);
    return buff[pop_idx];
  }

  bool has_string() {
    for (int i=pop_idx; i!=push_idx; i=next_idx(i))
      if (!is_term(buff[i])) return true;
    return false;
  }

  String pop_string() {
    String ret;
    int i = pop_idx;
    for (; i!=push_idx &&  is_term(buff[i]); i=next_idx(i)); // skip leading terminators
    for (; i!=push_idx && !is_term(buff[i]); i=next_idx(i)) ret += buff[i];
    for (; i!=push_idx &&  is_term(buff[i]); i=next_idx(i)); // skip trailing terminators
    pop_idx = i;
    return ret;
  }

};

