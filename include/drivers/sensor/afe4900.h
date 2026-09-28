/*
 * Copyright (c) 2026 Muhammad Waleed Badar
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ZSBAND_INCLUDE_DRIVERS_SENSOR_AFE4900_H_
#define ZSBAND_INCLUDE_DRIVERS_SENSOR_AFE4900_H_

#include <zephyr/drivers/sensor.h>

/* PPG phase outputs as signed 24-bit ADC codes */
enum sensor_channel_afe4900 {
	SENSOR_CHAN_AFE4900_LED1 = SENSOR_CHAN_PRIV_START,
	SENSOR_CHAN_AFE4900_LED2,
	SENSOR_CHAN_AFE4900_LED3,
	SENSOR_CHAN_AFE4900_AMBIENT,
};

#endif /* ZSBAND_INCLUDE_DRIVERS_SENSOR_AFE4900_H_ */
