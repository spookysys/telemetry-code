#include "sensors.hpp"
#include "pins.hpp"
#include "events.hpp"
#include "watchdog.hpp"
#include "regtek.hpp"
#include <Wire.h>
#include <array>

using namespace sensors;

namespace
{
    Stream& logger = Serial;

    // I2C scan function
    void I2CScan()
    {
        // scan for i2c devices
        byte error, address;
        int nDevices;

        logger.println("Scanning...");

        nDevices = 0;
        for (address = 1; address < 127; address++)
        {
            // The i2c_scanner uses the return value of
            // the Write.endTransmisstion to see if
            // a device did acknowledge to the address.
            Wire.beginTransmission(address);
            error = Wire.endTransmission();

            if (error == 0)
            {
                logger.print("I2C device found at address 0x");
                if (address < 16)
                    logger.print("0");
                logger.print(address, HEX);
                logger.println("  !");

                nDevices++;
            }
            else if (error == 4)
            {
                logger.print("Unknow error at address 0x");
                if (address < 16)
                    logger.print("0");
                logger.println(address, HEX);
            }
        }
        if (nDevices == 0)
            logger.println("No I2C devices found\n");
        else
            logger.println("done\n");
    }

    void writeByte(uint8_t address, uint8_t subAddress, uint8_t data)
    {
        Wire.beginTransmission(address); // Initialize the Tx buffer
        Wire.write(subAddress);          // Put slave register address in Tx buffer
        Wire.write(data);                // Put data in Tx buffer
        Wire.endTransmission();          // Send the Tx buffer
    }

    uint8_t readByte(uint8_t address, uint8_t subAddress)
    {
        uint8_t data;                         // `data` will store the register data
        Wire.beginTransmission(address);      // Initialize the Tx buffer
        Wire.write(subAddress);               // Put slave register address in Tx buffer
        Wire.endTransmission(false);          // Send the Tx buffer, but send a restart to keep connection alive
        Wire.requestFrom(address, (size_t)1); // Read one byte from slave register address
        data = Wire.read();                   // Fill Rx buffer with result
        return data;                          // Return data read from slave register
    }

    void readBytes(uint8_t address, uint8_t subAddress, uint8_t count, uint8_t *dest)
    {
        Wire.beginTransmission(address); // Initialize the Tx buffer
        Wire.write(subAddress);          // Put slave register address in Tx buffer
        Wire.endTransmission(false);     // Send the Tx buffer, but send a restart to keep connection alive
        uint8_t i = 0;
        Wire.requestFrom(address, (size_t)count); // Read bytes from slave register address
        while (Wire.available())
        { // Put read results in the Rx buffer
            dest[i++] = Wire.read();
        }
    }

    // accelerometer and gyroscope
    class Imu
    {
        static const uint8_t ADDR = (!pins::MPU_ADO) ? 0x68 : 0x69;
        static const uint8_t WHO_AM_I = 0x75;
        static const uint8_t WHO_AM_I_ANSWER = 0x71;

        static const uint8_t SMPLRT_DIV = 0x19;
        static const uint8_t CONFIG = 0x1A;
        static const uint8_t GYRO_CONFIG = 0x1B;
        static const uint8_t ACCEL_CONFIG = 0x1C;
        static const uint8_t ACCEL_CONFIG2 = 0x1D;

        static const uint8_t FIFO_EN = 0x23;
        static const uint8_t I2C_MST_CTRL = 0x24;

        static const uint8_t INT_PIN_CFG = 0x37;
        static const uint8_t INT_ENABLE = 0x38;

        static const uint8_t INT_STATUS = 0x3A;

        static const uint8_t USER_CTRL = 0x6A;
        static const uint8_t PWR_MGMT_1 = 0x6B;
        static const uint8_t PWR_MGMT_2 = 0x6C;

        static const uint8_t FIFO_COUNTH = 0x72;
        static const uint8_t FIFO_COUNTL = 0x73;
        static const uint8_t FIFO_R_W = 0x74;

    private:
        int gyro_scale = 0;  // 250 dps
        int accel_scale = 0; // 2 g

    public:
        // Howto interrupt: https://github.com/kriswiner/MPU-9250/issues/57

        void sendReset()
        {
            writeByte(ADDR, PWR_MGMT_1, 0x80);
        }

        void setBypass()
        {
            writeByte(ADDR, INT_PIN_CFG, 0x02); // allows access to magnetometer (I think)
        }

        bool setup()
        {
            bool ok = true;
            
            // Identify
            uint8_t c = readByte(ADDR, WHO_AM_I);
            if (c != WHO_AM_I_ANSWER)
            {
                logger.println(String("MPU failed to identify: ") + String(c, HEX));
                ok = false;
            }

            // Setup clock
            writeByte(ADDR, PWR_MGMT_1, 0x01);
            delay(200);

            // SMPLRT_DIV
            // Data output (fifo) sample rate
            // Set to 0 for the maximum 1 kHz (= internal sample rate)
            writeByte(ADDR, SMPLRT_DIV, 0x04); // 200 Hz
            //writeByte(ADDR, SMPLRT_DIV, 0x20); // 30Hz (for debugging)

            // CONFIG
            // [6] Fifo behaviour on overflow - 0:drop oldest data, 1:drop new data
            // [5:3] fsync mode - 0: disabled
            // [2:0] DLPF_CFG - Gyro/Temperature filter bandwidth - 0:250Hz, 1:184Hz, 2:92Hz, 3:41Hz..., 5:5Hz and 7:3600Hz
            writeByte(ADDR, CONFIG, 0x43); // 41Hz, 5.9ms delay

            // GYRO_CONFIG
            // [7:5] Gyro Self-Test [XYZ] - 0:disabled
            // [4:3] Gyro Scale - 0: 250dps, 1:500dps, 2:1000dps, 3:2000dps
            // [1:0] Fchoice_b - Gyro/Temperature filter enable - 0:enabled
            assert(!(gyro_scale % 4));
            writeByte(ADDR, GYRO_CONFIG, gyro_scale << 3);

            // ACCEL_CONFIG2
            // [3] fchoice_b - Filter enable - 0:enabled
            // [2:0] DLPF_CFG - Filter bandwidth - (complicated)
            writeByte(ADDR, ACCEL_CONFIG2, 0x03); // 41Hz

            // ACCEL_CONFIG
            // [2:0] Accelerometer Self-Test [XYZ] - 0:disabled
            // [4:3] Accelerometer Scale - 0:2g, 1:4g, 2:8g, 3:16g
            assert(!(accel_scale % 4));
            writeByte(ADDR, ACCEL_CONFIG, accel_scale << 3);

            // Interrupt pin config
            // [7] int pin is active 0:high, 1:low
            // [6] int pin is 0:push-pull 1:open-drain
            // [5] int pin is held until 0:50us 1:status is cleared
            // [4] int status is cleared by 0:reading int status register 1:any read operation
            // [3] fsync as an interrupt is active 0:high 1:low
            // [2] fsync as an interrupt is 0:disabled 1:enabled
            // [1] bypass_en, affects i2c master pins
            //writeByte(ADDR, INT_PIN_CFG, 0x22); // INT is high until status register is read
            //writeByte(ADDR, INT_PIN_CFG, 0x12);  // INT is 50 microsecond pulse and any read to clear
            writeByte(ADDR, INT_PIN_CFG, 0x02); // INT is 50ms pulse or until status register is read

            // Reset fifo and signal paths
            writeByte(ADDR, USER_CTRL, 0x05);
            delay(25); // Delay a while to let the device stabilize

            // USER_CTRL
            // Bit 7 enable DMP, bit 3 reset DMP (secret)
            // [6] FIFO_EN 1:enable
            // [2] FIFO_RST 1:reset
            // [0] SIG_COND_RST: 1:reset signal paths and sensor registers
            writeByte(ADDR, USER_CTRL, 0x40); // enable fifo

            // FIFO_EN
            // [6:4] gyro xyz
            // [3] accel
            writeByte(ADDR, FIFO_EN, 0x78); // gyro and accel

            // Enable Raw Sensor Data Ready interrupt to propagate to interrupt pin
            writeByte(ADDR, INT_ENABLE, 0x01); // Enable data ready (bit 0) interrupt

            return ok;
        }

        bool read(std::array<int16_t, 3> &accel, std::array<int16_t, 3> &gyro)
        {
            // Number of bytes in fifo
            uint16_t fifo_bytes;
            readBytes(ADDR, FIFO_COUNTH, 2, (uint8_t *)&fifo_bytes);
            fifo_bytes = misc::swapEndianness(fifo_bytes);

            // How many complete packets?
            static const int packet_size = 12;
            uint16_t packet_count = fifo_bytes / packet_size;

            // Empty the fifo and return the last entry
            if (packet_count)
            {
                std::array<int16_t, packet_size / 2> packet_data;
                for (int i = 0; i < packet_count; i++)
                {
                    readBytes(ADDR, FIFO_R_W, packet_size, (uint8_t *)&packet_data[0]);
                }
                accel[0] = misc::swapEndianness(packet_data[0]);
                accel[1] = misc::swapEndianness(packet_data[1]);
                accel[2] = misc::swapEndianness(packet_data[2]);
                gyro[0] = misc::swapEndianness(packet_data[3]);
                gyro[1] = misc::swapEndianness(packet_data[4]);
                gyro[2] = misc::swapEndianness(packet_data[5]);
                return true;
            }
            else
            {
                return false;
            }
        }

        uint8_t readInterruptStatus()
        {
            return readByte(ADDR, INT_STATUS);
        }
    };

    class Magnetometer
    {
        static const uint8_t ADDR = 0x0C;

        static const uint8_t WHO_AM_I = 0x00;
        static const uint8_t WHO_AM_I_ANSWER = 0x48;

        static const uint8_t INFO = 0x01;
        static const uint8_t ST1 = 0x02;    // data ready status bit 0
        static const uint8_t XOUT_L = 0x03; // data
        static const uint8_t XOUT_H = 0x04;
        static const uint8_t YOUT_L = 0x05;
        static const uint8_t YOUT_H = 0x06;
        static const uint8_t ZOUT_L = 0x07;
        static const uint8_t ZOUT_H = 0x08;
        static const uint8_t ST2 = 0x09;    // Data overflow bit 3 and data read error status bit 2
        static const uint8_t CNTL = 0x0A;   // Power down (0000), single-measurement (0001), self-test (1000) and Fuse ROM (1111) modes on bits 3:0
        static const uint8_t ASTC = 0x0C;   // Self test control
        static const uint8_t I2CDIS = 0x0F; // I2C disable
        static const uint8_t ASAX = 0x10;   // Fuse ROM x-axis sensitivity adjustment value
        static const uint8_t ASAY = 0x11;   // Fuse ROM y-axis sensitivity adjustment value
        static const uint8_t ASAZ = 0x12;   // Fuse ROM z-axis sensitivity adjustment value

    private:
        // Scale adjustment values 
        // adjust_f[0] = float(int(adjust[0]) - 128) / 256.f + 1.f;
        // adjust_f[1] = float(int(adjust[1]) - 128) / 256.f + 1.f;
        // adjust_f[2] = float(int(adjust[2]) - 128) / 256.f + 1.f;
        // Fixed point i:8
        // adjust_x[0] = int(adjust[0]) - 128 + 256;
        // adjust_x[1] = int(adjust[1]) - 128 + 256;
        // adjust_x[2] = int(adjust[2]) - 128 + 256;
        std::array<uint8_t, 3> adjust;

    public:
        bool setup()
        {
            bool ok = true;

            uint8_t c = readByte(ADDR, WHO_AM_I);
            if (c != WHO_AM_I_ANSWER)
            {
                logger.println(String("Magnetometer failed to identify: ") + String(c, HEX));
                ok = false;
            }

            // First extract the factory calibration for each magnetometer axis
            writeByte(ADDR, CNTL, 0x00); // Power down magnetometer
            delay(10);
            writeByte(ADDR, CNTL, 0x0F); // Enter Fuse ROM access mode
            delay(10);
            readBytes(ADDR, ASAX, 3, &adjust[0]); // Read the x-, y-, and z-axis calibration values
            writeByte(ADDR, CNTL, 0x00);          // Power down magnetometer
            delay(10);
            for (auto& iter : adjust) logger.println(iter);
            // Configure the magnetometer for continuous read and highest resolution
            // set Mscale bit 4 to 1 (0) to enable 16 (14) bit resolution in CNTL register,
            // and enable continuous mode data acquisition Mmode (bits [3:0]), 0010 for 8 Hz and 0110 for 100 Hz sample rates
            writeByte(ADDR, CNTL, 0x16); // 16 bit, 100Hz acquisition
            delay(10);

            return ok;
        }

        bool newData()
        {
            return readByte(ADDR, ST1) & 0x01;
        }

        bool read(std::array<int32_t, 3> &res, bool &overflow)
        {
            // return false if no new data
            if (!newData()) return false;

            // read out values
            std::array<uint8_t, 7> raw;
            readBytes(ADDR, XOUT_L, 7, &raw[0]); // Read the six raw data and ST2 registers sequentially into data array

            // combine bytes
            std::array<int32_t, 3> tmp;
            tmp[0] = int16_t((uint16_t(raw[1]) << 8) | raw[0]); // Turn the MSB and LSB into a signed 16-bit value
            tmp[1] = int16_t((uint16_t(raw[3]) << 8) | raw[2]); // Data stored as little Endian
            tmp[2] = int16_t((uint16_t(raw[5]) << 8) | raw[4]);

            // scale by factory-provided values (see MPU-9250 Register Map under "Sensitivity Adjustment")
            tmp[0] *= int(adjust[0]) + 128;
            tmp[1] *= int(adjust[1]) + 128;
            tmp[2] *= int(adjust[2]) + 128;
            
            // flip axes to match gyro/accel (see MPU-9250 Product Specification under "Orientation of Axes")
            std::swap(tmp[0], tmp[1]);
            tmp[2] = -tmp[2];

            // write result
            res[0] = tmp[0];
            res[1] = tmp[1];
            res[2] = tmp[2];
            overflow = raw[6] & 0x08;
            return true;
        }
    };

    class Altimeter
    {
        static const uint8_t ADDR = 0x76;
        static const uint8_t WHO_AM_I = 0xD0;
        static const uint8_t WHO_AM_I_ANSWER = 0x58;

        static const uint8_t CALIBRATION_DATA = 0x88;

        static const uint8_t VERSION = 0xD1;
        static const uint8_t RESET = 0xE0;

        static const uint8_t STATUS = 0xF3;
        static const uint8_t CONTROL = 0xF4;
        static const uint8_t CONFIG = 0xF5;
        
        static const uint8_t PRESSUREDATA = 0xF7;
        static const uint8_t TEMPDATA = 0xFA;

        struct CalibrationData
        {
            uint16_t t1;
            int16_t t2, t3;
            uint16_t p1;
            int16_t p2, p3, p4, p5, p6, p7, p8, p9;
            uint16_t resv;
        };

        union {
            CalibrationData dig;
            std::array<int16_t, 13> calibration_data;
        };

    public:
        static float getAltitude(float pressurePa, float seaLevelhPa=1013.25f) {
          return 44330 * (1.0 - pow(pressurePa / (seaLevelhPa*100), 0.1903));
        }

        void sendReset()
        {
            writeByte(ADDR, RESET, 0xB6);          
        }
        
        bool setup()
        {
            bool ok = true;

            // Read ID
            uint8_t c = readByte(ADDR, WHO_AM_I);
            if (c != WHO_AM_I_ANSWER)
            {
                logger.println(String("Altimeter failed to identify: ") + String(c, HEX));
                ok = false;
            }

            // Read calibration data
            assert(sizeof(CalibrationData) == 13 * 2);
            readBytes(ADDR, CALIBRATION_DATA, sizeof(CalibrationData), (uint8_t*)&calibration_data[0]);

            // For ~50 Hz, set pressure to x8, temperature to x1 and Ts to 0.5ms
            // [7:5] Oversampling of temperature - 001:x1
            // [4:2] Oversampling of pressure - 100:x8
            // [1:0] Mode - 11:normal
            writeByte(ADDR, CONTROL, 0x33);

            // [7:5] Ts - 0:0.5ms
            // [4:2] iir - 0:off (i hope)
            // [0] SPI enable - 0:use_i2c
            writeByte(ADDR, CONFIG, 0);
            return ok;
        }

        bool isMeasuring()
        {
            return readByte(ADDR, STATUS) & 0x4;
        }

        void read(int32_t &temperature, uint32_t &pressure)
        {
            std::array<uint8_t, 6> data;
            readBytes(ADDR, PRESSUREDATA, 6, &data[0]);
            int32_t adc_P = (int32_t(data[0]) << 12) | (int32_t(data[1]) << 4) | (int32_t(data[2])>>4);
            int32_t adc_T = (int32_t(data[3]) << 12) | (int32_t(data[4]) << 4) | (int32_t(data[5])>>4);
            temperature = bmp280_compensate_T_int32(adc_T);
            pressure = bmp280_compensate_P_int64(adc_P);
        }

    private: // Routines straight from BMP280 datasheet
        using BMP280_S32_t = int32_t;
        using BMP280_U32_t = uint32_t;
        using BMP280_S64_t = int64_t;

        // Returns temperature in DegC, resolution is 0.01 DegC. Output value of “5123” equals 51.23 DegC.
        // t_fine carries fine temperature as global value
        BMP280_S32_t t_fine;
        BMP280_S32_t bmp280_compensate_T_int32(BMP280_S32_t adc_T)
        {
            BMP280_S32_t var1, var2, T;
            var1 = ((((adc_T >> 3) - ((BMP280_S32_t)dig.t1 << 1))) * ((BMP280_S32_t)dig.t2)) >> 11;
            var2 = (((((adc_T >> 4) - ((BMP280_S32_t)dig.t1)) * ((adc_T >> 4) - ((BMP280_S32_t)dig.t1))) >> 12) * ((BMP280_S32_t)dig.t3)) >> 14;
            t_fine = var1 + var2;
            T = (t_fine * 5 + 128) >> 8;
            return T;
        }

        // Returns pressure in Pa as unsigned 32 bit integer in Q24.8 format (24 integer bits and 8 fractional bits).
        // Output value of “24674867” represents 24674867/256 = 96386.2 Pa = 963.862 hPa
        BMP280_U32_t bmp280_compensate_P_int64(BMP280_S32_t adc_P)
        {
            BMP280_S64_t var1, var2, p;
            var1 = ((BMP280_S64_t)t_fine) - 128000;
            var2 = var1 * var1 * (BMP280_S64_t)dig.p6;
            var2 = var2 + ((var1 * (BMP280_S64_t)dig.p5) << 17);
            var2 = var2 + (((BMP280_S64_t)dig.p4) << 35);
            var1 = ((var1 * var1 * (BMP280_S64_t)dig.p3) >> 8) + ((var1 * (BMP280_S64_t)dig.p2) << 12);
            var1 = (((((BMP280_S64_t)1) << 47) + var1)) * ((BMP280_S64_t)dig.p1) >> 33;
            if (var1 == 0) {
                return 0; // avoid exception caused by division by zero
            }
            p = 1048576 - adc_P;
            p = (((p << 31) - var2) * 3125) / var1;
            var1 = (((BMP280_S64_t)dig.p9) * (p >> 13) * (p >> 13)) >> 25;
            var2 = (((BMP280_S64_t)dig.p8) * p) >> 19;
            p = ((p + var1 + var2) >> 8) + (((BMP280_S64_t)dig.p7) << 4);
            return (BMP280_U32_t)p;
        }
    };

    Imu imu;
    Magnetometer mag;
    Altimeter alt;

    SensorData data;

    struct SensorStats
    {
        int isr_calls = 0;
        int mag_valids = 0;
        int imu_valids = 0;
        int alt_valids = 0;
        int mag_ofs = 0;
    };
    volatile SensorStats stats;

    static void (*sensorDataCallback)(const SensorData&);

    void imuIsr() 
    {
        // Read IMU (accel/gyro)
        data.imu_valid = imu.read(data.imu_accel, data.imu_gyro);

        // Read Magnetometer
        data.mag_valid = mag.read(data.mag_data, data.mag_of);

        // Determine alt_valid
        static int alt_counter = 0;
        data.alt_valid = (alt_counter<=0);
        if (data.alt_valid) alt_counter = 3;
        else alt_counter--;

        // Read Altimeter
        if (data.alt_valid) alt.read(data.alt_t, data.alt_p);

        // Collect Stats
        stats.isr_calls++;
        if (data.imu_valid) stats.imu_valids++;
        if (data.mag_valid) stats.mag_valids++;
        if (data.alt_valid) stats.alt_valids++;
        if (data.mag_valid && data.mag_of) stats.mag_ofs++;
        
        // callback with sensor data
        sensorDataCallback(data);

        // no need, since interrupt line pulses for 50 ms
        //imu.readInterruptStatus();
    }

    auto &isr_stats_process = events::makeProcess("isr_stats").setPeriod(1000).subscribe([&](unsigned long time, unsigned long delta) {
        //logger.println(String("imuIsr called at ") + (isr_calls*1000)/delta + " Hz, isr_calls: " + isr_calls + ", imu_valids: " + imu_valids + " Hz, mag_valids: " + mag_valids + " Hz, alt_valids: " + alt_valids + ", mag_ofs: " + mag_ofs);
        //stats = SensorStats{};

        float altitude = Altimeter::getAltitude(data.alt_p / 256.f);
        //logger.println(String() + "Temperature: " + (alt_t * 0.01f) + " degC, pressure: " + (alt_p / 256.f) + " Pa, altitude: " + altitude + " m");

    });

    /*
    auto& imu_proc = events::makeProcess("imu").setPeriod(10).subscribe([&](unsigned long time, unsigned long delta) {
    imu.update();
    //logger.println(digitalRead(pins::MPU_INT));
    //imu.readInterruptStatus();
    //isr();
    });
    */

} // anon



namespace sensors
{

    bool setup(void (*isrCallback)(const SensorData&))
    {
        // set callback
        ::sensorDataCallback = isrCallback;

        // soft resets
        imu.sendReset();
        alt.sendReset();
        delay(25);
       
        // setup all my sensors
        bool imu_ok = imu.setup(); // call first - sets pass-thru req for magnetometer etc
        assert(imu_ok);
        bool mag_ok = mag.setup();
        assert(mag_ok);
        bool alt_ok = alt.setup();
        assert(alt_ok);

        // perform scan
        I2CScan();

        // I use the MPU interrupt to drive realtime update
        pinMode(pins::MPU_INT, INPUT);
        attachInterrupt(pins::MPU_INT, imuIsr, RISING);

        // return success
        return imu_ok && mag_ok && alt_ok;
    }
}
