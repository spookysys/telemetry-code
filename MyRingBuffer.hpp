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
  int num_leading_terms = 0;
  int num_nonterms = 0;
  int where = 0; // leading_terms
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
  
  constexpr int capacity() 
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
    switch(where) {
      case 0: {
        if (is_term(c)) num_leading_terms++;
        else {
          where = 1;
          num_nonterms = 1;
        }
      } break;
      case 1: {
        if (!is_term(c)) num_nonterms++;
        else where = 2;
      } break;
      default:;
    }
  }

  int pop()
  {
    assert(pop_idx != push_idx);
    auto value = buff[pop_idx];
    pop_idx = next_idx(pop_idx);
    update_counters();
    return value;
  }

  int peek()
  {
    assert(pop_idx != push_idx);
    return buff[pop_idx];
  }

  bool has_string() {
    /*
    int i = pop_idx;
    for (; i!=push_idx &&  is_term(buff[i]); i=next_idx(i)); // skip leading terminators
    if (i==push_idx) return false; // no string
    for (; i!=push_idx && !is_term(buff[i]); i=next_idx(i)); // skip the string
    if (i==push_idx) return false; // no terminal to end the string
    return true; // goodie!
    */
    return (where >= 2);
  }

  void update_counters()
  {
    num_leading_terms = 0;
    num_nonterms = 0;
    int i = pop_idx;
    where = 0;
    for (; i!=push_idx &&  is_term(buff[i]); i=next_idx(i), num_leading_terms++); // skip leading terminators
    if (i==push_idx) return;
    where = 1;
    for (; i!=push_idx && !is_term(buff[i]); i=next_idx(i), num_nonterms++); // skip the string
    if (i==push_idx) return;
    where = 2;
  }

  String pop_string() {
    String ret;
    int i = pop_idx;
    for (; i!=push_idx &&  is_term(buff[i]); i=next_idx(i)); // skip leading terminators
    for (; i!=push_idx && !is_term(buff[i]); i=next_idx(i)) ret += buff[i];
    if (i!=push_idx && is_term(buff[i])) i=next_idx(i); // skip one trailing terminator (so we don't start processing binary data which happens to follow the string)
    pop_idx = i;
    update_counters();
    return ret;
  }

};

