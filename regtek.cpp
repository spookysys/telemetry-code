#include "regtek.hpp"
#include "events.hpp"
using namespace std;

namespace regtek
{
    Stream& logger = SerialUSB;

    auto& plot_channel = events::makeChannel<const std::array<int16_t, 3>&>("plot").subscribe([&](unsigned long time, const std::array<int16_t, 3>& mag){
        logger.println(String() + mag[0] + "\t" + mag[1] + "\t" + mag[2]);
        delay(2);
    });


    // note: run from isr
    extern void sensorUpdate(
        bool imu_valid, 
        const std::array<int16_t, 3>& imu_accel,
        const std::array<int16_t, 3>& imu_gyro,
        bool mag_valid,
        bool mag_of,
        const std::array<int16_t, 3>& mag,
        bool alt_valid,
        const uint32_t& alt_p,
        const int32_t& alt_t
    )
    {
        if (mag_valid && !mag_of) {
            plot_channel.publish(mag);
        }
    }

}