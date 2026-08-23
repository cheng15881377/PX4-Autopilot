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

#include <drivers/drv_sensor.h>
#include <px4_platform_common/getopt.h>
#include <px4_platform_common/module.h>

static constexpr uint8_t YAHBOOM_I2C_ADDRESS{0x23};
static constexpr uint32_t YAHBOOM_I2C_FREQUENCY{100000};

void YahboomIMU::print_usage()
{
	PRINT_MODULE_USAGE_NAME("yahboom_imu", "driver");
	PRINT_MODULE_USAGE_SUBCATEGORY("imu");
	PRINT_MODULE_USAGE_COMMAND("start");
	PRINT_MODULE_USAGE_PARAMS_I2C_SPI_DRIVER(true, false);
	PRINT_MODULE_USAGE_PARAMS_I2C_ADDRESS(YAHBOOM_I2C_ADDRESS);
	PRINT_MODULE_USAGE_PARAM_INT('R', 0, 0, 35, "Rotation", true);
	PRINT_MODULE_USAGE_DEFAULT_COMMANDS();
}

extern "C" __EXPORT int yahboom_imu_main(int argc, char *argv[])
{
	int ch;
	using ThisDriver = YahboomIMU;
	BusCLIArguments cli{true, false};
	cli.default_i2c_frequency = YAHBOOM_I2C_FREQUENCY;
	cli.i2c_address = YAHBOOM_I2C_ADDRESS;

	while ((ch = cli.getOpt(argc, argv, "R:")) != EOF) {
		switch (ch) {
		case 'R':
			cli.rotation = static_cast<enum Rotation>(atoi(cli.optArg()));
			break;
		}
	}

	const char *verb = cli.optArg();

	if (!verb) {
		ThisDriver::print_usage();
		return PX4_ERROR;
	}

	BusInstanceIterator iterator(MODULE_NAME, cli, DRV_IMU_DEVTYPE_YAHBOOM);

	if (!strcmp(verb, "start")) {
		return ThisDriver::module_start(cli, iterator);
	}

	if (!strcmp(verb, "stop")) {
		return ThisDriver::module_stop(iterator);
	}

	if (!strcmp(verb, "status")) {
		return ThisDriver::module_status(iterator);
	}

	ThisDriver::print_usage();
	return PX4_ERROR;
}
