#include "common.hpp"
#include <array>

namespace regtek {

    // note: run from isr
    extern void sensorUpdate(
        bool imu_valid, 
        const std::array<int16_t, 3>& imu_accel,
        const std::array<int16_t, 3>& imu_gyro,
        bool mag_valid,
        bool mag_of,
        const std::array<int32_t, 3>& mag,
        bool alt_valid,
        const uint32_t& alt_p,
        const int32_t& alt_t
    );
  
}

