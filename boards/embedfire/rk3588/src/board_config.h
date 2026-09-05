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
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 ****************************************************************************/

/**
 * @file board_config.h
 *
 * EmbedFire LubanCat RK3588 prototype board definitions.
 */

#pragma once

// Local prototype identity. Request a dedicated architecture ID before
// submitting this board upstream.
#define BOARD_OVERRIDE_UUID "RK3588PX4PROTO00" // Must be exactly 16 characters.
#define PX4_SOC_ARCH_ID     PX4_SOC_ARCH_ID_UNUSED

// Linux exposes the buses through /dev/i2c-* and /dev/spidev*.
#define CONFIG_I2C 1
#define PX4_NUMBER_I2C_BUSES 3

#define CONFIG_SPI 1

// No board ADC is wired in the initial prototype. Keep the channels invalid so
// battery_status can be built and a real power monitor can be added later.
#define ADC_BATTERY_VOLTAGE_CHANNEL (-1)
#define ADC_BATTERY_CURRENT_CHANNEL (-1)
#define ADC_AIRSPEED_VOLTAGE_CHANNEL (-1)
#define ADC_DP_V_DIV 1.0f

#include <system_config.h>
#include <px4_platform_common/board_common.h>
