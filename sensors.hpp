#pragma once
#include "common.hpp"
#include <array>

namespace sensors
{
    struct SensorData {
        bool imu_valid = false;
        std::array<int16_t, 3> imu_accel{};
        std::array<int16_t, 3> imu_gyro{};
        bool mag_valid = false;
        bool mag_of = false;
        std::array<int32_t, 3> mag_data{};
        bool alt_valid = false;
        uint32_t alt_p = false;
        int32_t alt_t = false;
    };

    extern bool setup(void (*isrCallback)(const SensorData&));
   
}
