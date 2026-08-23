/****************************************************************************
 *
 *   Copyright (c) 2026 PX4 Development Team. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name PX4 nor the names of its contributors may be
 *    used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 ****************************************************************************/

#pragma once

#include <drivers/drv_hrt.h>
#include <lib/drivers/accelerometer/PX4Accelerometer.hpp>
#include <lib/drivers/barometer/PX4Barometer.hpp>
#include <lib/drivers/device/i2c.h>
#include <lib/drivers/gyroscope/PX4Gyroscope.hpp>
#include <lib/drivers/magnetometer/PX4Magnetometer.hpp>
#include <lib/perf/perf_counter.h>
#include <px4_platform_common/i2c_spi_buses.h>

class YahboomIMU : public device::I2C, public I2CSPIDriver<YahboomIMU>
{
public:
	explicit YahboomIMU(const I2CSPIDriverConfig &config);
	~YahboomIMU() override;

	static void print_usage();

	int init() override;
	void print_status() override;
	void RunImpl();

private:
	void exit_and_cleanup() override;
	int probe() override;

	int ReadRegister(uint8_t reg, uint8_t *data, unsigned length, hrt_abstime *timestamp_sample = nullptr);
	bool ReadAccelerometer();
	bool ReadGyroscope();
	bool ReadMagnetometer();
	bool ReadBarometer();

	static int16_t Int16LE(const uint8_t *data);
	static float FloatLE(const uint8_t *data);

	static constexpr uint8_t REG_VERSION{0x01};
	static constexpr uint8_t REG_ACCEL{0x04};
	static constexpr uint8_t REG_GYRO{0x0A};
	static constexpr uint8_t REG_MAG{0x10};
	static constexpr uint8_t REG_BARO{0x32};

	// Two command-style IMU reads plus the required inter-command gap take
	// more than 4 ms on the RK3588 I2C controller. Use a stable 200 Hz cycle
	// instead of continuously overrunning a nominal 250 Hz schedule.
	static constexpr uint32_t SAMPLE_INTERVAL_US{5000};
	static constexpr uint32_t COMMAND_GAP_US{1000}; // required by the vendor access sequence
	static constexpr uint8_t ACCEL_LENGTH{6};
	static constexpr uint8_t GYRO_LENGTH{6};
	static constexpr uint8_t MAG_LENGTH{6};
	static constexpr uint8_t BARO_LENGTH{16};

	PX4Accelerometer _px4_accel;
	PX4Gyroscope _px4_gyro;
	PX4Magnetometer _px4_mag;
	PX4Barometer _px4_baro;

	perf_counter_t _sample_perf{perf_alloc(PC_ELAPSED, MODULE_NAME ": sample")};
	perf_counter_t _comms_errors{perf_alloc(PC_COUNT, MODULE_NAME ": communication errors")};
	perf_counter_t _bad_data{perf_alloc(PC_COUNT, MODULE_NAME ": invalid data")};

	uint8_t _firmware_version[3] {};
	uint8_t _cycle{0}; // 20 cycles = 100 ms
	hrt_abstime _last_transfer_end{0};
};
