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

    auto& plot_channel = events::makeChannel<const std::array<int32_t, num_vals>&>("plot").subscribe([&](unsigned long time, const std::array<int32_t, num_vals>& v){
        for (auto& iter : v) {
            logger.print( iter * (1.f/float(1<<16)) );
            logger.print("\t");
        }
        logger.println();
    });

    class Ranger
    {
    public:
        int warmup = 0;
        int32_t min{};
        int32_t max{};
        int32_t sum{};
        int32_t samples{};
        void update(int32_t val)
        {
            if (warmup++<1000) return;
            min = std::min(min, val);
            max = std::max(max, val);
            sum += val;
            samples++;
            if (samples > 1<<30) {
                samples >>= 1;
                sum >>= 1;
            }
        }
        int32_t avg() {
            return sum/samples;
        }
    };


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

            //plot_channel.publish(state);
        }

        if (data.mag_valid) {
            static std::array<Ranger, 3> rangers;
            rangers[0].update(data.mag_data[0]);
            rangers[1].update(data.mag_data[1]);
            rangers[2].update(data.mag_data[2]);
            plot_channel.publish(std::array<int32_t, num_vals>{{rangers[0].min, rangers[0].avg(), rangers[0].max}});
        }
    }

}