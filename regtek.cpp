#include "regtek.hpp"
#include "events.hpp"
#include "sensors.hpp"
using namespace std;

namespace regtek
{
    Stream& logger = SerialUSB;

    static const int num_vals = 3;

    auto& plot_channel = events::makeChannel<const std::array<int32_t, num_vals>&>("plot").subscribe([&](unsigned long time, const std::array<int32_t, num_vals>& v){
        for (auto& iter : v) {
            logger.print(iter);
            logger.print("\t");
        }
        logger.println();
    });


    // note: run from isr
    extern void sensorUpdate(const sensors::SensorData& data)
    {
        if (data.mag_valid && !data.mag_of) {
            plot_channel.publish(data.mag_data);
        }
    }

}