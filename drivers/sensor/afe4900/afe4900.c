/*
 * Copyright (c) 2026 Muhammad Waleed Badar
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT ti_afe4900

#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/sys/util.h>

#include <drivers/sensor/afe4900.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(afe4900, CONFIG_SENSOR_LOG_LEVEL);

#define AFE4900_REG_CONTROL0      0x00
#define AFE4900_REG_PRPCT         0x1D
#define AFE4900_REG_TIMEREN       0x1E
#define AFE4900_REG_TIA_GAIN      0x21
#define AFE4900_REG_LED_CURRENT   0x22
#define AFE4900_REG_CONTROL1      0x23
#define AFE4900_REG_LED2VAL       0x2A
#define AFE4900_REG_ALED2VAL      0x2B
#define AFE4900_REG_LED1VAL       0x2C
#define AFE4900_REG_ALED1VAL      0x2D
#define AFE4900_REG_CLKDIV        0x39
#define AFE4900_REG_DEEP_SLP_ENDC 0x6B

#define AFE4900_CONTROL0_ENABLE_ULP BIT(5)
#define AFE4900_CONTROL0_SW_RESET   BIT(3)

#define AFE4900_TIMEREN   BIT(8)
#define AFE4900_NUMAV_TWO 1

#define AFE4900_TIA_GAIN_MSB BIT(6)
#define AFE4900_TIA_GAIN_LSB GENMASK(2, 0)

/* Bits that must always be set, plus the internal 128 kHz oscillator */
#define AFE4900_CONTROL1_DYN_TX   BIT(20)
#define AFE4900_CONTROL1_DYN_BIAS BIT(14)
#define AFE4900_CONTROL1_OSC_EN   BIT(9)
#define AFE4900_CONTROL1_DYN_TIA  BIT(4)
#define AFE4900_CONTROL1_DYN_ADC  BIT(3)
#define AFE4900_CONTROL1                                                                           \
	(AFE4900_CONTROL1_DYN_TX | AFE4900_CONTROL1_DYN_BIAS | AFE4900_CONTROL1_OSC_EN |           \
	 AFE4900_CONTROL1_DYN_TIA | AFE4900_CONTROL1_DYN_ADC)

/* Divide the 128 kHz oscillator by 4 to clock the timing engine at 32 kHz */
#define AFE4900_CLKDIV_TE_DIV4 5
#define AFE4900_TE_CLK_HZ      32000

/* DEEP_SLEEP_ENDC is set relative to PRPCT, as in the datasheet examples */
#define AFE4900_DEEP_SLP_END_OFFSET 7

#define AFE4900_ILED_MAX_UA 50000
#define AFE4900_ILED_MAX    255

/* Datasheet Table 142: t4 reset pulse, t5 reset to I2C, t8 power-down exit to reset */
#define AFE4900_RESET_PULSE_US    30
#define AFE4900_RESET_WAIT_MS     2
#define AFE4900_PWDN_EXIT_WAIT_MS 10

#define AFE4900_NUM_OUTPUTS 4

struct afe4900_reg {
	uint8_t addr;
	uint16_t val;
};

/*
 * Sample timing for a 32 kHz timing engine clock (datasheet Table 49). LED4 is
 * left at zero current, so the ALED1 phase measures ambient light.
 */
static const struct afe4900_reg afe4900_timing[] = {
	{0x01, 0x0B}, {0x02, 0x0D}, {0x03, 0x14}, {0x04, 0x17}, {0x05, 0x10}, {0x06, 0x12},
	{0x07, 0x15}, {0x08, 0x17}, {0x09, 0x0A}, {0x0A, 0x0D}, {0x0B, 0x1A}, {0x0C, 0x1C},
	{0x0D, 0x0F}, {0x0E, 0x12}, {0x0F, 0x14}, {0x10, 0x17}, {0x11, 0x19}, {0x12, 0x1C},
	{0x13, 0x1E}, {0x14, 0x21}, {0x36, 0x0F}, {0x37, 0x12}, {0x43, 0x19}, {0x44, 0x1C},
	{0x52, 0x27}, {0x53, 0x27}, {0x64, 0x00}, {0x65, 0x23}, {0x66, 0x00}, {0x67, 0x23},
	{0x68, 0x00}, {0x69, 0x23}, {0x6A, 0x2E},
};

/* Output registers in the order of the private sensor channels */
static const uint8_t afe4900_outputs[AFE4900_NUM_OUTPUTS] = {
	AFE4900_REG_LED1VAL,
	AFE4900_REG_LED2VAL,
	AFE4900_REG_ALED2VAL,
	AFE4900_REG_ALED1VAL,
};

struct afe4900_config {
	struct i2c_dt_spec i2c;
	struct gpio_dt_spec reset;
	uint16_t prpct;
	uint8_t tia_gain;
	uint32_t led_current;
};

struct afe4900_data {
	int32_t sample[AFE4900_NUM_OUTPUTS];
};

static int afe4900_write(const struct device *dev, uint8_t reg, uint32_t val)
{
	const struct afe4900_config *cfg = dev->config;
	uint8_t buf[4] = {reg};

	sys_put_be24(val, &buf[1]);

	return i2c_write_dt(&cfg->i2c, buf, sizeof(buf));
}

static int afe4900_read(const struct device *dev, uint8_t reg, uint32_t *val)
{
	const struct afe4900_config *cfg = dev->config;
	uint8_t buf[3];
	int ret;

	ret = i2c_write_read_dt(&cfg->i2c, &reg, sizeof(reg), buf, sizeof(buf));
	if (ret < 0) {
		return ret;
	}

	*val = sys_get_be24(buf);

	return 0;
}

static int afe4900_sample_fetch(const struct device *dev, enum sensor_channel chan)
{
	struct afe4900_data *data = dev->data;
	uint32_t val;
	int ret;

	if (chan != SENSOR_CHAN_ALL) {
		return -ENOTSUP;
	}

	for (int i = 0; i < AFE4900_NUM_OUTPUTS; i++) {
		ret = afe4900_read(dev, afe4900_outputs[i], &val);
		if (ret < 0) {
			return ret;
		}

		data->sample[i] = sign_extend(val, 23);
	}

	return 0;
}

static int afe4900_channel_get(const struct device *dev, enum sensor_channel chan,
			       struct sensor_value *val)
{
	struct afe4900_data *data = dev->data;

	switch ((int)chan) {
	case SENSOR_CHAN_AFE4900_LED1 ... SENSOR_CHAN_AFE4900_AMBIENT:
		val->val1 = data->sample[chan - SENSOR_CHAN_AFE4900_LED1];
		val->val2 = 0;
		return 0;
	default:
		return -ENOTSUP;
	}
}

static DEVICE_API(sensor, afe4900_api) = {
	.sample_fetch = afe4900_sample_fetch,
	.channel_get = afe4900_channel_get,
};

static int afe4900_reset(const struct device *dev)
{
	const struct afe4900_config *cfg = dev->config;
	int ret;

	if (cfg->reset.port != NULL) {
		if (!gpio_is_ready_dt(&cfg->reset)) {
			LOG_ERR("Reset GPIO not ready");
			return -ENODEV;
		}

		/* Release RESETZ in case it held the device in power-down */
		ret = gpio_pin_configure_dt(&cfg->reset, GPIO_OUTPUT_INACTIVE);
		if (ret < 0) {
			return ret;
		}
		k_msleep(AFE4900_PWDN_EXIT_WAIT_MS);

		/* A pulse of 25-50 us resets the device, longer powers it down */
		ret = gpio_pin_set_dt(&cfg->reset, 1);
		if (ret < 0) {
			return ret;
		}
		k_busy_wait(AFE4900_RESET_PULSE_US);
		ret = gpio_pin_set_dt(&cfg->reset, 0);
	} else {
		ret = afe4900_write(dev, AFE4900_REG_CONTROL0, AFE4900_CONTROL0_SW_RESET);
	}

	k_msleep(AFE4900_RESET_WAIT_MS);

	return ret;
}

static int afe4900_init(const struct device *dev)
{
	const struct afe4900_config *cfg = dev->config;
	uint32_t prpct;
	int ret;

	if (!i2c_is_ready_dt(&cfg->i2c)) {
		LOG_ERR("I2C bus not ready");
		return -ENODEV;
	}

	ret = afe4900_reset(dev);
	if (ret < 0) {
		LOG_ERR("Reset failed: %d", ret);
		return ret;
	}

	/* ENABLE_ULP must be set before writing registers at 23h and above */
	ret = afe4900_write(dev, AFE4900_REG_CONTROL0, AFE4900_CONTROL0_ENABLE_ULP);
	if (ret < 0) {
		return ret;
	}

	ret = afe4900_write(dev, AFE4900_REG_CONTROL1, AFE4900_CONTROL1);
	if (ret < 0) {
		return ret;
	}

	ret = afe4900_write(dev, AFE4900_REG_CLKDIV, AFE4900_CLKDIV_TE_DIV4);
	if (ret < 0) {
		return ret;
	}

	for (size_t i = 0; i < ARRAY_SIZE(afe4900_timing); i++) {
		ret = afe4900_write(dev, afe4900_timing[i].addr, afe4900_timing[i].val);
		if (ret < 0) {
			return ret;
		}
	}

	ret = afe4900_write(dev, AFE4900_REG_PRPCT, cfg->prpct);
	if (ret < 0) {
		return ret;
	}

	ret = afe4900_write(dev, AFE4900_REG_DEEP_SLP_ENDC,
			    cfg->prpct - AFE4900_DEEP_SLP_END_OFFSET);
	if (ret < 0) {
		return ret;
	}

	ret = afe4900_write(dev, AFE4900_REG_TIA_GAIN,
			    FIELD_PREP(AFE4900_TIA_GAIN_MSB, cfg->tia_gain >> 3) |
				    FIELD_PREP(AFE4900_TIA_GAIN_LSB, cfg->tia_gain));
	if (ret < 0) {
		return ret;
	}

	ret = afe4900_write(dev, AFE4900_REG_LED_CURRENT, cfg->led_current);
	if (ret < 0) {
		return ret;
	}

	/* Check that the device is present by reading back a register */
	ret = afe4900_read(dev, AFE4900_REG_PRPCT, &prpct);
	if (ret < 0) {
		LOG_ERR("Failed to read back PRPCT: %d", ret);
		return ret;
	}

	if (prpct != cfg->prpct) {
		LOG_ERR("PRPCT read back 0x%06x, expected 0x%04x", prpct, cfg->prpct);
		return -ENODEV;
	}

	return afe4900_write(dev, AFE4900_REG_TIMEREN, AFE4900_TIMEREN | AFE4900_NUMAV_TWO);
}

/* RF values in ohms indexed by TIA gain register code (datasheet Table 4) */
#define AFE4900_TIA_GAIN_CODE(ohms)                                                                \
	((ohms) == 500000    ? 0                                                                   \
	 : (ohms) == 250000  ? 1                                                                   \
	 : (ohms) == 100000  ? 2                                                                   \
	 : (ohms) == 50000   ? 3                                                                   \
	 : (ohms) == 25000   ? 4                                                                   \
	 : (ohms) == 10000   ? 5                                                                   \
	 : (ohms) == 1000000 ? 6                                                                   \
	 : (ohms) == 2000000 ? 7                                                                   \
			     : 8)

#define AFE4900_ILED_CODE(inst, idx)                                                               \
	DIV_ROUND_CLOSEST(DT_INST_PROP_BY_IDX(inst, led_current_microamp, idx) * AFE4900_ILED_MAX, \
			  AFE4900_ILED_MAX_UA)

/* Pack the 8-bit LED1-3 current codes into register 22h (datasheet Table 78) */
#define AFE4900_LED_CURRENT(inst)                                                                  \
	((AFE4900_ILED_CODE(inst, 0) >> 2) | ((AFE4900_ILED_CODE(inst, 1) >> 2) << 6) |            \
	 ((AFE4900_ILED_CODE(inst, 2) >> 2) << 12) | ((AFE4900_ILED_CODE(inst, 0) & 0x3) << 18) |  \
	 ((AFE4900_ILED_CODE(inst, 1) & 0x3) << 20) | ((AFE4900_ILED_CODE(inst, 2) & 0x3) << 22))

#define AFE4900_DEFINE(inst)                                                                       \
	BUILD_ASSERT(DT_INST_PROP(inst, sample_rate_hz) >= 1 &&                                    \
			     DT_INST_PROP(inst, sample_rate_hz) <= 500,                            \
		     "sample-rate-hz must be between 1 and 500");                                  \
	BUILD_ASSERT(DT_INST_PROP_LEN(inst, led_current_microamp) == 3,                            \
		     "led-current-microamp must have 3 entries");                                  \
	BUILD_ASSERT(DT_INST_PROP_BY_IDX(inst, led_current_microamp, 0) <= AFE4900_ILED_MAX_UA &&  \
			     DT_INST_PROP_BY_IDX(inst, led_current_microamp, 1) <=                 \
				     AFE4900_ILED_MAX_UA &&                                        \
			     DT_INST_PROP_BY_IDX(inst, led_current_microamp, 2) <=                 \
				     AFE4900_ILED_MAX_UA,                                          \
		     "led-current-microamp entries must not exceed 50000");                        \
                                                                                                   \
	static struct afe4900_data afe4900_data_##inst;                                            \
                                                                                                   \
	static const struct afe4900_config afe4900_config_##inst = {                               \
		.i2c = I2C_DT_SPEC_INST_GET(inst),                                                 \
		.reset = GPIO_DT_SPEC_INST_GET_OR(inst, reset_gpios, {0}),                         \
		.prpct = AFE4900_TE_CLK_HZ / DT_INST_PROP(inst, sample_rate_hz) - 1,               \
		.tia_gain = AFE4900_TIA_GAIN_CODE(DT_INST_PROP(inst, tia_gain_ohms)),              \
		.led_current = AFE4900_LED_CURRENT(inst),                                          \
	};                                                                                         \
                                                                                                   \
	SENSOR_DEVICE_DT_INST_DEFINE(inst, afe4900_init, NULL, &afe4900_data_##inst,               \
				     &afe4900_config_##inst, POST_KERNEL,                          \
				     CONFIG_SENSOR_INIT_PRIORITY, &afe4900_api);

DT_INST_FOREACH_STATUS_OKAY(AFE4900_DEFINE)
