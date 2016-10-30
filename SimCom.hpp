#ifndef SIMCOM_HPP
#define SIMCOM_HPP

#include "common.hpp"

class SimCom
{
public:
  bool isOn();
  void powerOnOff();
  void begin();
};

extern SimCom simCom;

#endif

