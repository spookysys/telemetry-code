#include "common.hpp"
#include <array>

namespace sensors {
    struct SensorData;
}

namespace regtek {

    // note: run from isr
    extern void sensorUpdate(const sensors::SensorData& data);
  
}

