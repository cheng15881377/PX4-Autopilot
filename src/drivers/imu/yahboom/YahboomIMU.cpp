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

#include "YahboomIMU.hpp"

#include <cmath>
#include <cstring>

#include <lib/geo/geo.h>
#include <mathlib/mathlib.h>
#include <px4_platform_common/posix.h>

YahboomIMU::YahboomIMU(const I2CSPIDriverConfig &config) :
	I2C(config),
	I2CSPIDriver(config),
	_px4_accel(get_device_id(), config.rotation),
	_px4_gyro(get_device_id(), config.rotation),
	_px4_mag(get_device_id(), config.rotation),
	_px4_baro(get_device_id())
{
	_retries = 1;
	_px4_accel.set_range(16.f * CONSTANTS_ONE_G);
	_px4_gyro.set_range(math::radians(2000.f));
}

YahboomIMU::~YahboomIMU()
{
	ScheduleClear();
	perf_free(_sample_perf);
	perf_free(_comms_errors);
	perf_free(_bad_data);
}

int YahboomIMU::init()
{
	const int ret = I2C::init();

	if (ret != PX4_OK) {
		PX4_ERR("I2C init failed (%d)", ret);
		return ret;
	}

	ScheduleOnInterval(SAMPLE_INTERVAL_US, SAMPLE_INTERVAL_US);
	return PX4_OK;
}

void YahboomIMU::exit_and_cleanup()
{
	ScheduleClear();
	I2CSPIDriverBase::exit_and_cleanup();
}

int YahboomIMU::probe()
{
	if (ReadRegister(REG_VERSION, _firmware_version, sizeof(_firmware_version)) != PX4_OK) {
		return PX4_ERROR;
	}

	const bool all_zero = (_firmware_version[0] == 0) && (_firmware_version[1] == 0) && (_firmware_version[2] == 0);
	const bool all_ff = (_firmware_version[0] == 0xff) && (_firmware_version[1] == 0xff)
			    && (_firmware_version[2] == 0xff);

	if (all_zero || all_ff) {
		DEVICE_DEBUG("invalid firmware version %u.%u.%u", _firmware_version[0], _firmware_version[1],
			     _firmware_version[2]);
		return PX4_ERROR;
	}

	return PX4_OK;
}

int YahboomIMU::ReadRegister(uint8_t reg, uint8_t *data, unsigned length, hrt_abstime *timestamp_sample)
{
	// The module is an MCU implementing command-style data blocks rather than
	// a normal auto-incrementing sensor register map. Its vendor library waits
	// 1 ms after every command. Keep the same gap here, otherwise adjacent
	// reads can return zero or partially updated blocks.
	if (_last_transfer_end != 0) {
		const hrt_abstime elapsed = hrt_elapsed_time(&_last_transfer_end);

		if (elapsed < COMMAND_GAP_US) {
			px4_usleep(COMMAND_GAP_US - elapsed);
		}
	}

	const hrt_abstime transfer_begin = hrt_absolute_time();
	const int ret = transfer(&reg, 1, data, length);
	const hrt_abstime transfer_end = hrt_absolute_time();

	if (timestamp_sample != nullptr) {
		*timestamp_sample = transfer_begin + (transfer_end - transfer_begin) / 2;
	}

	_last_transfer_end = transfer_end;
	return ret;
}

int16_t YahboomIMU::Int16LE(const uint8_t *data)
{
	const uint16_t value = static_cast<uint16_t>(data[0]) | (static_cast<uint16_t>(data[1]) << 8);
	return static_cast<int16_t>(value);
}

float YahboomIMU::FloatLE(const uint8_t *data)
{
	const uint32_t value = static_cast<uint32_t>(data[0])
			       | (static_cast<uint32_t>(data[1]) << 8)
			       | (static_cast<uint32_t>(data[2]) << 16)
			       | (static_cast<uint32_t>(data[3]) << 24);
	float result{};
	static_assert(sizeof(result) == sizeof(value), "unexpected float size");
	memcpy(&result, &value, sizeof(result));
	return result;
}

bool YahboomIMU::ReadAccelerometer()
{
	uint8_t data[ACCEL_LENGTH] {};
	hrt_abstime timestamp_sample{};
	const int ret = ReadRegister(REG_ACCEL, data, sizeof(data), &timestamp_sample);

	if (ret != PX4_OK) {
		perf_count(_comms_errors);
		return false;
	}

	const float accel_scale = (16.f / 32767.f) * CONSTANTS_ONE_G;

	_px4_accel.set_error_count(perf_event_count(_comms_errors));
	_px4_accel.update(timestamp_sample,
			  Int16LE(&data[0]) * accel_scale,
			  Int16LE(&data[2]) * accel_scale,
			  Int16LE(&data[4]) * accel_scale);
	return true;
}

bool YahboomIMU::ReadGyroscope()
{
	uint8_t data[GYRO_LENGTH] {};
	hrt_abstime timestamp_sample{};
	const int ret = ReadRegister(REG_GYRO, data, sizeof(data), &timestamp_sample);

	if (ret != PX4_OK) {
		perf_count(_comms_errors);
		return false;
	}

	const float gyro_scale = math::radians(2000.f / 32767.f);

	_px4_gyro.set_error_count(perf_event_count(_comms_errors));
	_px4_gyro.update(timestamp_sample,
			 Int16LE(&data[0]) * gyro_scale,
			 Int16LE(&data[2]) * gyro_scale,
			 Int16LE(&data[4]) * gyro_scale);

	return true;
}

bool YahboomIMU::ReadMagnetometer()
{
	uint8_t data[MAG_LENGTH] {};
	hrt_abstime timestamp_sample{};
	const int ret = ReadRegister(REG_MAG, data, sizeof(data), &timestamp_sample);

	if (ret != PX4_OK) {
		perf_count(_comms_errors);
		return false;
	}

	const float mag_scale_gauss = (800.f / 32767.f) * 0.01f; // module units are microtesla

	_px4_mag.set_error_count(perf_event_count(_comms_errors));
	_px4_mag.update(timestamp_sample,
			Int16LE(&data[0]) * mag_scale_gauss,
			Int16LE(&data[2]) * mag_scale_gauss,
			Int16LE(&data[4]) * mag_scale_gauss);

	return true;
}

bool YahboomIMU::ReadBarometer()
{
	uint8_t data[BARO_LENGTH] {};
	hrt_abstime timestamp_sample{};
	const int ret = ReadRegister(REG_BARO, data, sizeof(data), &timestamp_sample);

	if (ret != PX4_OK) {
		perf_count(_comms_errors);
		return false;
	}

	const float temperature = FloatLE(&data[4]);
	const float pressure_pa = FloatLE(&data[8]);

	if (!PX4_ISFINITE(temperature) || !PX4_ISFINITE(pressure_pa)
	    || temperature < -50.f || temperature > 100.f
	    || pressure_pa < 10000.f || pressure_pa > 120000.f) {
		perf_count(_bad_data);
		return false;
	}

	_px4_baro.set_error_count(perf_event_count(_comms_errors) + perf_event_count(_bad_data));
	_px4_baro.set_temperature(temperature);
	_px4_baro.update(timestamp_sample, pressure_pa);
	return true;
}

void YahboomIMU::RunImpl()
{
	perf_begin(_sample_perf);
	ReadAccelerometer();
	ReadGyroscope();

	// Stagger slower transfers so a 100 kHz bus does not have to carry the
	// magnetometer and barometer transactions in the same 4 ms cycle.
	_cycle = (_cycle + 1) % 20;

	if ((_cycle == 2) || (_cycle == 7) || (_cycle == 12) || (_cycle == 17)) {
		ReadMagnetometer(); // 40 Hz

	} else if (_cycle == 0) {
		ReadBarometer(); // 10 Hz
	}

	perf_end(_sample_perf);
}

void YahboomIMU::print_status()
{
	I2CSPIDriverBase::print_status();
	PX4_INFO("firmware: %u.%u.%u", _firmware_version[0], _firmware_version[1], _firmware_version[2]);
	PX4_INFO("rates: accel/gyro 200 Hz, mag 40 Hz, baro 10 Hz");
	perf_print_counter(_sample_perf);
	perf_print_counter(_comms_errors);
	perf_print_counter(_bad_data);
}
