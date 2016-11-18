#include "common.hpp"

namespace imu
{
  struct Data {
    float ax, ay, az;
    float gx, gy, gz;
    float mx, my, mz;
    float q0, qx, qy, qz;
    float yaw, pitch, roll;
  };

  const Data& get();

  bool begin();
  void update();
}

