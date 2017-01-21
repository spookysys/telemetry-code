#include "common.hpp"
#include <array>




namespace sensors {
	struct SensorData;
}

namespace sensor_analyzer {

	// note: run from isr
	extern void sensorUpdate(const sensors::SensorData& data);
  
}

