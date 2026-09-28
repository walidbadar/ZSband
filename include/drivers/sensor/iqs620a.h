/*
 * Copyright (c) 2026 Muhammad Waleed Badar
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ZSBAND_INCLUDE_DRIVERS_SENSOR_IQS620A_H_
#define ZSBAND_INCLUDE_DRIVERS_SENSOR_IQS620A_H_

#include <zephyr/drivers/sensor.h>

enum sensor_channel_iqs620a {
	/** ProxFusion UI flags: proximity Ch0-2 in bits 0-2, touch Ch0-2 in bits 4-6 */
	SENSOR_CHAN_IQS620A_PROX_FLAGS = SENSOR_CHAN_PRIV_START,
	/** Hall-effect UI output */
	SENSOR_CHAN_IQS620A_HALL,
	/** Hall-effect UI flags: N/S in bit 0, proximity in bit 1, touch in bit 2 */
	SENSOR_CHAN_IQS620A_HALL_FLAGS,
	/** Temperature UI output (uncalibrated) */
	SENSOR_CHAN_IQS620A_TEMP,
	/** Raw counts of channels 0 to 5 */
	SENSOR_CHAN_IQS620A_COUNTS_CH0,
	SENSOR_CHAN_IQS620A_COUNTS_CH1,
	SENSOR_CHAN_IQS620A_COUNTS_CH2,
	SENSOR_CHAN_IQS620A_COUNTS_CH3,
	SENSOR_CHAN_IQS620A_COUNTS_CH4,
	SENSOR_CHAN_IQS620A_COUNTS_CH5,
};

#endif /* ZSBAND_INCLUDE_DRIVERS_SENSOR_IQS620A_H_ */
