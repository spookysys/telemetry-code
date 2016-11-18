#include "common.hpp"

namespace imu
{
  struct Data {
    float ax, ay, az;
    float gx, gy, gz;
    float mx, my, mz;
  };

  const Data& get();

  bool begin();
  void update();
}

