/*
 * Copyright (c) 2026 Muhammad Waleed Badar
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT azoteq_iqs620a

#include <zephyr/device.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/byteorder.h>

#include <drivers/sensor/iqs620a.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(iqs620a, CONFIG_SENSOR_LOG_LEVEL);

#define IQS620A_REG_PROD_NUM     0x00
#define IQS620A_REG_SW_NUM       0x01
#define IQS620A_REG_HW_NUM       0x02
#define IQS620A_REG_SYS_FLAGS    0x10
#define IQS620A_REG_COUNTS       0x20
#define IQS620A_REG_SYS_SETTINGS 0xD0

#define IQS620A_PROD_NUM 0x41

#define IQS620A_SYS_FLAGS_SHOW_RESET BIT(7)
#define IQS620A_SYS_FLAGS_IN_ATI     BIT(2)

#define IQS620A_SYS_SETTINGS_ACK_RESET BIT(6)
#define IQS620A_SYS_SETTINGS_REDO_ATI  BIT(1)

#define IQS620A_PROX_FLAGS_PROX_MASK GENMASK(2, 0)

#define IQS620A_NUM_CHANNELS   6
#define IQS620A_ATI_POLL_MS    10
#define IQS620A_ATI_TIMEOUT_MS 1000

/* Offsets within the flags and UI data block starting at 0x10 */
enum iqs620a_flags_offset {
	IQS620A_SYS_FLAGS,
	IQS620A_GLOBAL_EVENTS,
	IQS620A_PROX_FLAGS,
	IQS620A_SAR_FLAGS,
	IQS620A_HYST_OUT_L,
	IQS620A_HYST_OUT_H,
	IQS620A_HALL_FLAGS,
	IQS620A_HALL_OUT_L,
	IQS620A_HALL_OUT_H,
	IQS620A_TEMP_FLAGS,
	IQS620A_TEMP_OUT_L,
	IQS620A_TEMP_OUT_H,
	IQS620A_FLAGS_LEN,
};

struct iqs620a_config {
	struct i2c_dt_spec i2c;
	const uint8_t *init_regs;
	size_t init_regs_len;
};

struct iqs620a_data {
	uint8_t flags[IQS620A_FLAGS_LEN];
	uint16_t counts[IQS620A_NUM_CHANNELS];
};

static int iqs620a_wait_ati(const struct device *dev)
{
	const struct iqs620a_config *cfg = dev->config;
	uint8_t flags;
	int ret;

	for (int elapsed = 0; elapsed < IQS620A_ATI_TIMEOUT_MS; elapsed += IQS620A_ATI_POLL_MS) {
		ret = i2c_reg_read_byte_dt(&cfg->i2c, IQS620A_REG_SYS_FLAGS, &flags);
		if (ret < 0) {
			return ret;
		}

		if (!(flags & IQS620A_SYS_FLAGS_IN_ATI)) {
			return 0;
		}

		k_msleep(IQS620A_ATI_POLL_MS);
	}

	return -ETIMEDOUT;
}

/* Apply settings, acknowledge the reset and re-run ATI */
static int iqs620a_setup(const struct device *dev)
{
	const struct iqs620a_config *cfg = dev->config;
	int ret;

	for (size_t i = 0; i + 1 < cfg->init_regs_len; i += 2) {
		ret = i2c_reg_write_byte_dt(&cfg->i2c, cfg->init_regs[i], cfg->init_regs[i + 1]);
		if (ret < 0) {
			return ret;
		}
	}

	ret = i2c_reg_update_byte_dt(&cfg->i2c, IQS620A_REG_SYS_SETTINGS,
				     IQS620A_SYS_SETTINGS_ACK_RESET | IQS620A_SYS_SETTINGS_REDO_ATI,
				     IQS620A_SYS_SETTINGS_ACK_RESET |
					     IQS620A_SYS_SETTINGS_REDO_ATI);
	if (ret < 0) {
		return ret;
	}

	return iqs620a_wait_ati(dev);
}

static int iqs620a_sample_fetch(const struct device *dev, enum sensor_channel chan)
{
	const struct iqs620a_config *cfg = dev->config;
	struct iqs620a_data *data = dev->data;
	uint8_t counts[IQS620A_NUM_CHANNELS * 2];
	int ret;

	if (chan != SENSOR_CHAN_ALL) {
		return -ENOTSUP;
	}

	ret = i2c_burst_read_dt(&cfg->i2c, IQS620A_REG_SYS_FLAGS, data->flags, sizeof(data->flags));
	if (ret < 0) {
		return ret;
	}

	if (data->flags[IQS620A_SYS_FLAGS] & IQS620A_SYS_FLAGS_SHOW_RESET) {
		LOG_WRN("Unexpected device reset");
		ret = iqs620a_setup(dev);
		if (ret < 0) {
			return ret;
		}

		ret = i2c_burst_read_dt(&cfg->i2c, IQS620A_REG_SYS_FLAGS, data->flags,
					sizeof(data->flags));
		if (ret < 0) {
			return ret;
		}
	}

	ret = i2c_burst_read_dt(&cfg->i2c, IQS620A_REG_COUNTS, counts, sizeof(counts));
	if (ret < 0) {
		return ret;
	}

	for (int i = 0; i < IQS620A_NUM_CHANNELS; i++) {
		data->counts[i] = sys_get_le16(&counts[i * 2]);
	}

	return 0;
}

static int iqs620a_channel_get(const struct device *dev, enum sensor_channel chan,
			       struct sensor_value *val)
{
	struct iqs620a_data *data = dev->data;

	val->val2 = 0;

	switch ((int)chan) {
	case SENSOR_CHAN_PROX:
		val->val1 = !!(data->flags[IQS620A_PROX_FLAGS] & IQS620A_PROX_FLAGS_PROX_MASK);
		break;
	case SENSOR_CHAN_IQS620A_PROX_FLAGS:
		val->val1 = data->flags[IQS620A_PROX_FLAGS];
		break;
	case SENSOR_CHAN_IQS620A_HALL:
		val->val1 = sys_get_le16(&data->flags[IQS620A_HALL_OUT_L]);
		break;
	case SENSOR_CHAN_IQS620A_HALL_FLAGS:
		val->val1 = data->flags[IQS620A_HALL_FLAGS];
		break;
	case SENSOR_CHAN_IQS620A_TEMP:
		val->val1 = sys_get_le16(&data->flags[IQS620A_TEMP_OUT_L]);
		break;
	case SENSOR_CHAN_IQS620A_COUNTS_CH0 ... SENSOR_CHAN_IQS620A_COUNTS_CH5:
		val->val1 = data->counts[chan - SENSOR_CHAN_IQS620A_COUNTS_CH0];
		break;
	default:
		return -ENOTSUP;
	}

	return 0;
}

static DEVICE_API(sensor, iqs620a_api) = {
	.sample_fetch = iqs620a_sample_fetch,
	.channel_get = iqs620a_channel_get,
};

static int iqs620a_init(const struct device *dev)
{
	const struct iqs620a_config *cfg = dev->config;
	uint8_t id[3];
	int ret;

	if (!i2c_is_ready_dt(&cfg->i2c)) {
		LOG_ERR("I2C bus not ready");
		return -ENODEV;
	}

	ret = i2c_burst_read_dt(&cfg->i2c, IQS620A_REG_PROD_NUM, id, sizeof(id));
	if (ret < 0) {
		LOG_ERR("Failed to read device ID: %d", ret);
		return ret;
	}

	if (id[IQS620A_REG_PROD_NUM] != IQS620A_PROD_NUM) {
		LOG_ERR("Invalid product number 0x%02x", id[IQS620A_REG_PROD_NUM]);
		return -ENODEV;
	}

	LOG_DBG("Software number 0x%02x, hardware number 0x%02x", id[IQS620A_REG_SW_NUM],
		id[IQS620A_REG_HW_NUM]);

	ret = iqs620a_setup(dev);
	if (ret < 0) {
		LOG_ERR("Setup failed: %d", ret);
		return ret;
	}

	return 0;
}

#define IQS620A_DEFINE(inst)                                                                       \
	BUILD_ASSERT(DT_INST_PROP_LEN_OR(inst, init_regs, 0) % 2 == 0,                             \
		     "init-regs must contain address/value pairs");                                \
                                                                                                   \
	static const uint8_t iqs620a_init_regs_##inst[] = DT_INST_PROP_OR(inst, init_regs, {0});   \
                                                                                                   \
	static struct iqs620a_data iqs620a_data_##inst;                                            \
                                                                                                   \
	static const struct iqs620a_config iqs620a_config_##inst = {                               \
		.i2c = I2C_DT_SPEC_INST_GET(inst),                                                 \
		.init_regs = iqs620a_init_regs_##inst,                                             \
		.init_regs_len = DT_INST_PROP_LEN_OR(inst, init_regs, 0),                          \
	};                                                                                         \
                                                                                                   \
	SENSOR_DEVICE_DT_INST_DEFINE(inst, iqs620a_init, NULL, &iqs620a_data_##inst,               \
				     &iqs620a_config_##inst, POST_KERNEL,                          \
				     CONFIG_SENSOR_INIT_PRIORITY, &iqs620a_api);

DT_INST_FOREACH_STATUS_OKAY(IQS620A_DEFINE)
