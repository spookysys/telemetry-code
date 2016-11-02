#pragma once
#include "common.hpp"
#include <array>

// ring buffer for serials
template<int S, typename T>
class MyRingBuffer
{
private:
  std::array<T, S> _aucBuffer;
  int _iHead ;
  int _iTail ;
public:
  MyRingBuffer()
  {
    memset( _aucBuffer.data(), 0, S);
    clear();
  }
  int capacity() 
  {
    return _aucBuffer.size();
  }
  void store_char(T c)
  {
    int i = nextIndex(_iHead);
    //assert(i != _iTail);
    _aucBuffer[_iHead] = c ;
    _iHead = i ;
  }
  void clear()
  {
    _iHead = 0;
    _iTail = 0;
  }

  int read_char()
  {
    if(_iTail == _iHead)
      return -1;
  
    T value = _aucBuffer[_iTail];
    _iTail = nextIndex(_iTail);
  
    return value;
  }
  
  int available()
  {
    int delta = _iHead - _iTail;  
    if(delta < 0)
      return S + delta;
    else
      return delta;
  }
  int peek()
  {
    if(_iTail == _iHead)
      return -1;
  
    return _aucBuffer[_iTail];
  }
  bool isFull()
  {
    return (nextIndex(_iHead) == _iTail);
  }
  bool contains(char ch)
  {
    for (int i=_iTail; i!=_iHead; i=nextIndex(i)) {
      if (ch==_aucBuffer[i]) return true;
    }
    return false;
  }
private:
  int nextIndex(int index)
  {
    return (uint32_t)(index + 1) % S;
  }
};

