#include "sensors.hpp"
#include "pins.hpp"
#include <Wire.h>

namespace
{
    void wireKhz(int wire_khz)
    {
        sercom3.disableWIRE();
        SERCOM3->I2CM.BAUD.bit.BAUD = SystemCoreClock / (2000 * wire_khz) - 1;
        sercom3.enableWIRE();
    }

    struct SensorAccumData
    {
        int imu_samples = 0;
        std::array<int64_t, 3> accel_data{};
        std::array<int64_t, 3> gyro_data{};
        int64_t gyro_mag;
        int mag_samples = 0;
        std::array<int64_t, 3> mag_data{};
    } akku;


    void sensorUpdate(const sensors::SensorData &data)
    {
        // accumulate inertial data
        if (data.imu_valid) {
            akku.imu_samples++;
            // accumulate accelerometer vector
            akku.accel_data[0] += data.accel_data[0];
            akku.accel_data[1] += data.accel_data[1];
            akku.accel_data[2] += data.accel_data[2];
            // accumulate gyro axis
            akku.gyro_data[0] += data.gyro_data[0];
            akku.gyro_data[1] += data.gyro_data[1];
            akku.gyro_data[2] += data.gyro_data[2];
            // accumulate gyro magnitude
            int64_t mag_squared = int64_t(data.gyro_data[0])*data.gyro_data[0] 
                                + int64_t(data.gyro_data[1])*data.gyro_data[1] 
                                + int64_t(data.gyro_data[2])*data.gyro_data[2];
            int64_t mag = sqrt(mag_squared);
            akku.gyro_mag += mag;
            // gyro axis and magnitude are accumulated separately to overcome shortening of vector when averaging
            // this is not done for accelerometer and magnetometer, since for them I'm mostly interested in direction
        }
        // accumulate magnetometer vector
        if (data.mag_valid) {
            akku.mag_samples++;
            akku.mag_data[0] += data.mag_data[0];
            akku.mag_data[1] += data.mag_data[1];
            akku.mag_data[2] += data.mag_data[2];
        }
    }
}

void setup()
{
    // light LED while booting
    pinMode(pins::LED, OUTPUT);
    digitalWrite(pins::LED, true);
      
    // Init i2c
    Wire.begin();
    wireKhz(400);

    // Init serial
    SerialUSB.begin(9600);
    int last_i=0;
    for (int i=0; !SerialUSB; i++) {
        delay(100);
        last_i = i*100;
    }
    SerialUSB.println(String("Yo! ") + last_i);
    
    // setup sensors
    sensors::setup(sensorUpdate);
}

void loop()
{
    static bool led_on = false;
    led_on = !led_on;
    digitalWrite(pins::LED, led_on);
    
    static const int json_sample_delay = 200;
    delay(json_sample_delay);
    Serial.println(String("Delay: ") + json_sample_delay);
    Serial.println(String("sample_hz: ") + 1000/json_sample_delay);

    noInterrupts();
    {
        static unsigned long tick = 0;

        static const float point_precision = (1<<8);
        float imu_scaler = point_precision / akku.imu_samples;
        float mag_scaler = point_precision / akku.mag_samples;

        Serial.println("{");
        Serial.println(String("  \"imu_samples\":") + akku.imu_samples + ",");
        Serial.println(String("  \"mag_samples\":") + akku.mag_samples + ",");
        Serial.println(String("  \"tick\":") + tick + ",");
        Serial.println(String("  \"gyro\":[") + akku.gyro_data[0]*imu_scaler + "," + akku.gyro_data[1]*imu_scaler + "," + akku.gyro_data[2]*imu_scaler + "], \"gyro_mag\":" + akku.gyro_mag*imu_scaler + ",");
        Serial.println(String("  \"accel\":[") + akku.accel_data[0]*imu_scaler + "," + akku.accel_data[1]*imu_scaler + "," + akku.accel_data[2]*imu_scaler + "],");
        Serial.println(String("  \"mag\":[") + akku.mag_data[0]*mag_scaler + "," + akku.mag_data[1]*mag_scaler + "," + akku.mag_data[2]*mag_scaler + "]" );
        Serial.println("},");

        // just average forever
        //akku = SensorAccumData{};

        tick++;
    }
    interrupts();

}
