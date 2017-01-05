#include "calibrate.hpp"
#include "events.hpp"
#include "sensors.hpp"
#include <cmath>

using namespace std;

namespace calibrate
{
    Stream& logger = SerialUSB;



    class RangeFinder
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
        int32_t offs() {
            return (min+max)/2;
        }
        int32_t size() {
            return max-min;
        }
    };

    std::array<RangeFinder, 3> mag_range;

    // Plot
    static const int num_plot_vals = 9;
    auto& plot_channel = events::makeChannel<const std::array<int32_t, num_plot_vals>&>("plot").subscribe([&](unsigned long time, const std::array<int32_t, num_plot_vals>& v){
        for (auto& iter : v) {
            logger.print( iter * (1.f/float(1<<16)) );
            logger.print("\t");
        }
        logger.println();
    });

    // dump calibration values
    auto& dump_process = events::makeProcess("dump").subscribe([&](unsigned long time, unsigned long delta) {
        logger.println("// Magnetometer calibration values:");
        logger.println(String() + "static const std::array<int32_t, 3> mag_min = {{ " + String(mag_range[0].min) + ", " + String(mag_range[1].min) + ", " + String(mag_range[2].min) + " }};");
        logger.println(String() + "static const std::array<int32_t, 3> mag_max = {{ " + String(mag_range[0].max) + ", " + String(mag_range[1].max) + ", " + String(mag_range[2].max) + " }};");
        logger.println(String() + "static const std::array<int32_t, 3> mag_offs = {{ " + String(mag_range[0].offs()) + ", " + String(mag_range[1].offs()) + ", " + String(mag_range[2].offs()) + " }};");
        logger.println(String() + "static const std::array<int32_t, 3> mag_size = {{ " + String(mag_range[0].size()) + ", " + String(mag_range[1].size()) + ", " + String(mag_range[2].size()) + " }};");
    }).setPeriod(10000);
    

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
            mag_range[0].update(data.mag_data[0]);
            mag_range[1].update(data.mag_data[1]);
            mag_range[2].update(data.mag_data[2]);
            plot_channel.publish(std::array<int32_t, num_plot_vals>{{data.mag_data[0], data.mag_data[1], data.mag_data[2], mag_range[0].min, mag_range[0].max, mag_range[1].min, mag_range[1].max, mag_range[2].min, mag_range[2].max}});
        }
    }

}
