#include "sensors.hpp"
#include "pins.hpp"
#include <vector>
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
	int mag_samples = 0;
	std::array<int64_t, 3> accel_data{};
	std::array<int64_t, 3> gyro_data{};
	std::array<int64_t, 3> mag_data{};
	int64_t accel_mag = 0;
	int64_t gyro_mag = 0;
	int64_t mag_mag = 0;
};
volatile int64_t akku_pre = 0;
volatile int64_t akku_post = 0;
volatile SensorAccumData akku_shared;

void sensorUpdate(const sensors::SensorData &data)
{
	akku_pre++;
	SensorAccumData akku;
	memcpy(&akku, (void*)&::akku_shared, sizeof(SensorAccumData));

	// accumulate inertial data
	if (data.imu_valid)
	{
		akku.imu_samples++;
		// accumulate accelerometer vector
		akku.accel_data[0] += data.accel_data[0];
		akku.accel_data[1] += data.accel_data[1];
		akku.accel_data[2] += data.accel_data[2];
		// accumulate accelerometer magnitude
		int64_t accel_mag_squared = int64_t(data.accel_data[0]) * data.accel_data[0] + int64_t(data.accel_data[1]) * data.accel_data[1] + int64_t(data.accel_data[2]) * data.accel_data[2];
		int64_t accel_mag = sqrt(accel_mag_squared);
		akku.accel_mag += accel_mag;
		// accumulate gyro axis
		akku.gyro_data[0] += data.gyro_data[0];
		akku.gyro_data[1] += data.gyro_data[1];
		akku.gyro_data[2] += data.gyro_data[2];
		// accumulate gyro magnitude
		int64_t gyro_mag_squared = int64_t(data.gyro_data[0]) * data.gyro_data[0] + int64_t(data.gyro_data[1]) * data.gyro_data[1] + int64_t(data.gyro_data[2]) * data.gyro_data[2];
		int64_t gyro_mag = sqrt(gyro_mag_squared);
		akku.gyro_mag += gyro_mag;
	}
	// accumulate magnetometer vector
	if (data.mag_valid)
	{
		akku.mag_samples++;
		akku.mag_data[0] += data.mag_data[0];
		akku.mag_data[1] += data.mag_data[1];
		akku.mag_data[2] += data.mag_data[2];
		// accumulate magnetometer magnitude
		int64_t mag_mag_squared = int64_t(data.mag_data[0]) * data.mag_data[0] + int64_t(data.mag_data[1]) * data.mag_data[1] + int64_t(data.mag_data[2]) * data.mag_data[2];
		int64_t mag_mag = sqrt(mag_mag_squared);
		akku.mag_mag += mag_mag;
	}

	memcpy((void*)&::akku_shared, &akku, sizeof(SensorAccumData));
	akku_post++;
}

enum Modes
{
	MODE_NONE = 0,
	MODE_ROTATION = 1,
	MODE_GYRO_OFFSET = 2
};
Modes mode = MODE_NONE;


float getError(const std::array<float, 3> &a, const std::array<float, 3> &b)
{
	float maxError = 0;
	float tmp0, tmp1, tmp2;
	tmp0 = fabs(a[0] - b[0]);
	tmp1 = fabs(a[1] - b[1]);
	tmp2 = fabs(a[2] - b[2]);
	if (tmp2 > tmp1)
		tmp1 = tmp2;
	if (tmp1 > tmp0)
		tmp0 = tmp1;
	return tmp0;
}

static const float stability_threshold = 0.5;
static const int stability_iterations = 100;
static const int rotation_output_delay = 20;

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
	for (int i = 0; !SerialUSB; i++)
		delay(100);

	Serial.println("Please select what to calibrate");
	Serial.println("1) Mag- and accelerometer (default)");
	Serial.println("2) Gyro offset");
	Serial.setTimeout(10000);
	mode = (Modes)Serial.readStringUntil('\n').toInt();
	if (mode != MODE_ROTATION && mode != MODE_GYRO_OFFSET) 
		mode = MODE_ROTATION;

	// setup sensors
	sensors::setup(sensorUpdate);

	// Clear sensor akku
	noInterrupts();
	if (akku_pre!=akku_post) {
		Serial.println("Race!");
		while(1);
	}
	auto tmp = SensorAccumData{};
	memcpy((void*)&::akku_shared, &tmp, sizeof(SensorAccumData));
	interrupts();
}

void loop()
{

	// Blink led to show working
	static bool led_on = false;
	led_on = !led_on;
	digitalWrite(pins::LED, led_on);

	// One of two modes
	if (mode == MODE_GYRO_OFFSET)
	{
		static std::vector<std::array<float, 3>> results;

		// Sample
		Serial.println("Sampling:");
		std::array<float, 3> interm_res;
		long num_stable_results = 0;
		noInterrupts();
		if (akku_pre!=akku_post) {
			Serial.println("Race!");
			while(1);
		}
		auto tmp = SensorAccumData{};
		memcpy((void*)&::akku_shared, &tmp, sizeof(SensorAccumData));
		interrupts();
		do
		{
			// Wait and accumulate sensor data
			delay(100);

			// Read data
			noInterrupts();
			if (akku_pre!=akku_post) {
				Serial.println("Race!");
				while(1);
			}
			SensorAccumData akku;
			memcpy(&akku, (void*)&::akku_shared, sizeof(SensorAccumData));
			interrupts();

			// Average gyro data from beginning of 'do'
			double imu_scaler = 1.0 / akku.imu_samples;
			interm_res[0] = akku.gyro_data[0] * imu_scaler;
			interm_res[1] = akku.gyro_data[1] * imu_scaler;
			interm_res[2] = akku.gyro_data[2] * imu_scaler;

			// Print intermediate result
			Serial.print(String("[") + interm_res[0] + "," + interm_res[1] + "," + interm_res[2] + "]");

			// Try to detect when result has stabilized
			static std::array<float, 3> stable_candidate = interm_res;
			float error = getError(stable_candidate, interm_res);
			if (error < stability_threshold)
			{
				num_stable_results++;
				Serial.print(String() + " - " + num_stable_results + "/" + stability_iterations);
			}
			else
			{
				num_stable_results = 0;
				stable_candidate = interm_res;
			}

			Serial.println();

		} while (num_stable_results < stability_iterations);

		// Remember result
		results.push_back(interm_res);

		// Print results so far
		Serial.println();
		Serial.println("Results so far:");
		Serial.println("[");
		for (const auto &res : results)
			Serial.println(String("  [") + res[0] + "," + res[1] + "," + res[2] + "],");
		Serial.println("]");
		Serial.println();

		// Wait for keypress
		Serial.println("Shake the unit, put it down, and press enter (or wait 10 seconds)");
		while (Serial.available())
			Serial.read();
		Serial.setTimeout(10000);
		Serial.readStringUntil('\n');
	}
	else if (mode == MODE_ROTATION)
	{
		static unsigned long tick = 0;
		static unsigned long start_time = millis();
		unsigned long time = millis() - start_time;

		// Wait and accumulate sensor data
		long ventetid = ((tick + 1) * rotation_output_delay) - time;
		if (ventetid > 0)
			delay(ventetid);

		// Read out and reset accumulated sensor data
		noInterrupts();
		if (akku_pre!=akku_post) {
			Serial.println("Race!");
			while(1);
		}
		SensorAccumData akku;
		memcpy(&akku, (void*)&::akku_shared, sizeof(SensorAccumData)); 
		auto tmp = SensorAccumData{};
		memcpy((void*)&::akku_shared, &tmp, sizeof(SensorAccumData));        
		interrupts();

		// Output data
		if (tick==0) Serial.println(String("\"sample_hz\":") + 1000 / rotation_output_delay + ",");
		double imu_scaler = 1.0 / akku.imu_samples;
		double mag_scaler = 1.0 / akku.mag_samples;
		Serial.println("{");
		Serial.println(String("  \"tick\":") + tick + ",");
		//Serial.println(String("  \"time\":") + time + ",");
		//Serial.println(String("  \"imu_samples\":") + akku.imu_samples + ",");
		//Serial.println(String("  \"mag_samples\":") + akku.mag_samples + ",");
		Serial.println(String("  \"gyro\":[") + akku.gyro_data[0] * imu_scaler + "," + akku.gyro_data[1] * imu_scaler + "," + akku.gyro_data[2] * imu_scaler + "], \"gyro_mag\":" + akku.gyro_mag * imu_scaler + ",");
		Serial.println(String("  \"accel\":[") + akku.accel_data[0] * imu_scaler + "," + akku.accel_data[1] * imu_scaler + "," + akku.accel_data[2] * imu_scaler + "], \"accel_mag\":" + akku.accel_mag * imu_scaler + ",");
		Serial.println(String("  \"mag\":[") + akku.mag_data[0] * mag_scaler + "," + akku.mag_data[1] * mag_scaler + "," + akku.mag_data[2] * mag_scaler + "], \"mag_mag\":" + akku.mag_mag * mag_scaler);
		Serial.println("},");

		tick++;
	}
}
