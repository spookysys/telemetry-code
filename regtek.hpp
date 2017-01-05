#include "common.hpp"
#include <array>




namespace sensors {
    struct SensorData;
}

namespace regtek {

    // Magnetometer calibration values:
    static const std::array<int32_t, 3> mag_min = {{ -48944, -104492, -35793 }};
    static const std::array<int32_t, 3> mag_max = {{ 124944, 69460, 139971 }};
    static const std::array<int32_t, 3> mag_offs = {{ 38000, -17516, 52089 }};
    static const std::array<int32_t, 3> mag_size = {{ 173888, 173952, 175764 }};

    // note: run from isr
    extern void sensorUpdate(const sensors::SensorData& data);
  
}

