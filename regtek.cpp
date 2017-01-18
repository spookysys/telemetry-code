#include "regtek.hpp"
#include "events.hpp"
#include "sensors.hpp"
#include <cmath>

using namespace std;

namespace regtek
{
	Stream& logger = SerialUSB;


	float GyroMeasError = PI * (4.0f / 180.0f);   // gyroscope measurement error in rads/s (start at 40 deg/s)
	float GyroMeasDrift = PI * (0.0f  / 180.0f);   // gyroscope measurement drift in rad/s/s (start at 0.0 deg/s/s)


	static const int num_vals = 3;

	auto& plot_channel = events::Channel<const std::array<int32_t, num_vals>&>::make("plot").subscribe([&](unsigned long time, const std::array<int32_t, num_vals>& v){
		for (auto& iter : v) {
			logger.print( iter * (1.f/float(1<<8)) );
			logger.print("\t");
		}
		logger.println();
	});


	// note: run from isr
	extern void sensorUpdate(const sensors::SensorData& data)
	{
		static std::array<int32_t, 3> state{};

		if (data.imu_valid) {
			state[0] += data.gyro_data[0];
			state[1] += data.gyro_data[1];
			state[2] += data.gyro_data[2];

			state[0] = (state[0] + (360<<16)) % (360<<16);
			state[1] = (state[1] + (360<<16)) % (360<<16);
			state[2] = (state[2] + (360<<16)) % (360<<16);

			//plot_channel.post(state);
		}

		/*
		if (data.mag_valid) {
			plot_channel.post(data.mag_data);
		}
		*/
	}

}