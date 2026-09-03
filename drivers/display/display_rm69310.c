/*
 * Copyright (c) 2026 Muhammad Waleed Badar
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT raydium_rm69310

#include <zephyr/kernel.h>
#include <zephyr/drivers/mipi_dbi.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/sys/util_macro.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(display_rm69310, CONFIG_DISPLAY_LOG_LEVEL);

#define RM69310_MCS_SET_PAGE 0xFE
#define RM69310_MCS_PAGE_UCS 0x00
#define RM69310_MCS_PAGE_1   0x01
#define RM69310_MCS_PAGE_2   0x02
#define RM69310_MCS_PAGE_4   0x04

#define RM69310_MCS_SPI_WRAM_CMD 0xC4
#define RM69310_MCS_SPI_WRAM_BIT BIT(7)

struct rm69310_config {
	const struct device *mipi_dev;
	struct mipi_dbi_config dbi_config;
	uint16_t width;
	uint16_t height;
	int16_t x_offset;
	int16_t y_offset;
	bool inversion;
};

struct rm69310_data {
	enum display_pixel_format pixel_format;
};

static inline int rm69310_dcs_write(const struct device *dev, uint8_t cmd, const uint8_t *data,
				    size_t len)
{
	const struct rm69310_config *cfg = dev->config;

	return mipi_dbi_command_write(cfg->mipi_dev, &cfg->dbi_config, cmd, data, len);
}

static int rm69310_blanking_on(const struct device *dev)
{
	return rm69310_dcs_write(dev, MIPI_DCS_SET_DISPLAY_OFF, NULL, 0);
}

static int rm69310_blanking_off(const struct device *dev)
{
	return rm69310_dcs_write(dev, MIPI_DCS_SET_DISPLAY_ON, NULL, 0);
}

static int rm69310_write(const struct device *dev, const uint16_t x, const uint16_t y,
			 const struct display_buffer_descriptor *desc, const void *buf)
{
	const struct rm69310_config *cfg = dev->config;
	struct rm69310_data *data = dev->data;
	uint8_t cmd[4] = {0};
	int ret;

	__ASSERT(desc->width <= desc->pitch, "pitch must be >= width");

	sys_put_be16((uint16_t)(x + cfg->x_offset), &cmd[0]);
	sys_put_be16((uint16_t)(x + cfg->x_offset + desc->width - 1U), &cmd[2]);
	ret = rm69310_dcs_write(dev, MIPI_DCS_SET_COLUMN_ADDRESS, cmd, sizeof(cmd));
	if (ret < 0) {
		return ret;
	}

	sys_put_be16((uint16_t)(y + cfg->y_offset), &cmd[0]);
	sys_put_be16((uint16_t)(y + cfg->y_offset + desc->height - 1U), &cmd[2]);
	ret = rm69310_dcs_write(dev, MIPI_DCS_SET_PAGE_ADDRESS, cmd, sizeof(cmd));
	if (ret < 0) {
		return ret;
	}

	ret = rm69310_dcs_write(dev, MIPI_DCS_WRITE_MEMORY_START, NULL, 0);
	if (ret < 0) {
		return ret;
	}

	return mipi_dbi_write_display(cfg->mipi_dev, &cfg->dbi_config, buf,
				      (struct display_buffer_descriptor *)desc, data->pixel_format);
}

static void rm69310_get_capabilities(const struct device *dev, struct display_capabilities *caps)
{
	const struct rm69310_config *cfg = dev->config;
	const struct rm69310_data *data = dev->data;

	caps->x_resolution = cfg->width;
	caps->y_resolution = cfg->height;
	caps->supported_pixel_formats = PIXEL_FORMAT_RGB_565 | PIXEL_FORMAT_RGB_888;
	caps->current_pixel_format = data->pixel_format;
	caps->current_orientation = DISPLAY_ORIENTATION_NORMAL;
	caps->screen_info = 0;
}

static int rm69310_set_pixel_format(const struct device *dev, const enum display_pixel_format fmt)
{
	struct rm69310_data *data = dev->data;
	uint8_t pixel_fmt;
	int ret;

	switch (fmt) {
	case PIXEL_FORMAT_RGB_565:
		pixel_fmt = MIPI_DCS_PIXEL_FORMAT_16BIT;
		break;
	case PIXEL_FORMAT_RGB_888:
		pixel_fmt = MIPI_DCS_PIXEL_FORMAT_24BIT;
		break;
	default:
		LOG_ERR("Unsupported pixel format");
		return -ENOTSUP;
	}

	ret = rm69310_dcs_write(dev, MIPI_DCS_SET_PIXEL_FORMAT, &pixel_fmt, sizeof(pixel_fmt));
	if (ret < 0) {
		return ret;
	}

	data->pixel_format = fmt;
	return 0;
}

static int rm69310_configure_panel(const struct device *dev)
{
	const struct rm69310_config *cfg = dev->config;
	struct rm69310_data *data = dev->data;
	uint8_t cmd;
	int ret;

	cmd = RM69310_MCS_PAGE_UCS;
	ret = rm69310_dcs_write(dev, RM69310_MCS_SET_PAGE, &cmd, sizeof(cmd));
	if (ret < 0) {
		return ret;
	}

	cmd = RM69310_MCS_SPI_WRAM_BIT;
	ret = rm69310_dcs_write(dev, RM69310_MCS_SPI_WRAM_CMD, &cmd, sizeof(cmd));
	if (ret < 0) {
		return ret;
	}

	ret = rm69310_dcs_write(dev, MIPI_DCS_EXIT_IDLE_MODE, NULL, 0);
	if (ret < 0) {
		return ret;
	}

	ret = rm69310_dcs_write(dev, MIPI_DCS_SET_DISPLAY_OFF, NULL, 0);
	if (ret < 0) {
		return ret;
	}

	ret = rm69310_set_pixel_format(dev, data->pixel_format);
	if (ret < 0) {
		return ret;
	}

	cmd = 0;
	ret = rm69310_dcs_write(dev, MIPI_DCS_SET_TEAR_ON, &cmd, sizeof(cmd));
	if (ret < 0) {
		return ret;
	}

	cmd = UINT8_MAX;
	ret = rm69310_dcs_write(dev, MIPI_DCS_SET_DISPLAY_BRIGHTNESS, &cmd, sizeof(cmd));
	if (ret < 0) {
		return ret;
	}

	/* Delay 50 ms before exiting sleep mode */
	k_msleep(50);
	ret = rm69310_dcs_write(dev, MIPI_DCS_EXIT_SLEEP_MODE, NULL, 0);
	if (ret < 0) {
		return ret;
	}
	k_msleep(150);

	cmd = cfg->inversion ? MIPI_DCS_ENTER_INVERT_MODE : MIPI_DCS_EXIT_INVERT_MODE;
	ret = rm69310_dcs_write(dev, cmd, NULL, 0);
	if (ret < 0) {
		return ret;
	}

	return rm69310_dcs_write(dev, MIPI_DCS_SET_DISPLAY_ON, NULL, 0);
}

static int rm69310_init(const struct device *dev)
{
	const struct rm69310_config *cfg = dev->config;
	int ret;

	if (!device_is_ready(cfg->mipi_dev)) {
		LOG_ERR("MIPI DBI host not ready");
		return -ENODEV;
	}

	ret = mipi_dbi_reset(cfg->mipi_dev, 30);
	if (ret < 0) {
		LOG_ERR("MIPI DBI reset failed: %d", ret);
		return ret;
	}

	ret = rm69310_configure_panel(dev);
	if (ret < 0) {
		LOG_ERR("Display configuration failed: %d", ret);
		return ret;
	}

	LOG_DBG("RM69310 ready (%ux%u%s)", cfg->width, cfg->height,
		cfg->inversion ? ", inverted" : "");

	return 0;
}

static const struct display_driver_api rm69310_api = {
	.blanking_on = rm69310_blanking_on,
	.blanking_off = rm69310_blanking_off,
	.write = rm69310_write,
	.get_capabilities = rm69310_get_capabilities,
	.set_pixel_format = rm69310_set_pixel_format,
};

#define RM69310_WORD_SIZE(n)                                                                       \
	((DT_INST_STRING_UPPER_TOKEN(n, mipi_mode) == MIPI_DBI_MODE_SPI_4WIRE) ? SPI_WORD_SET(8)   \
									       : SPI_WORD_SET(9))

#define RM69310_INIT(n)                                                                            \
	static struct rm69310_data rm69310_data_##n = {                                            \
		.pixel_format = DT_INST_PROP(n, pixel_format),                                     \
	};                                                                                         \
	                                                                                           \
	static const struct rm69310_config rm69310_config_##n = {                                  \
		.mipi_dev = DEVICE_DT_GET(DT_INST_PARENT(n)),                                      \
		.dbi_config = MIPI_DBI_CONFIG_DT_INST(                                             \
			n, RM69310_WORD_SIZE(n) | SPI_OP_MODE_CONTROLLER, 0),                      \
		.width = DT_INST_PROP(n, width),                                                   \
		.height = DT_INST_PROP(n, height),                                                 \
		.x_offset = DT_INST_PROP(n, x_offset),                                             \
		.y_offset = DT_INST_PROP(n, y_offset),                                             \
		.inversion = DT_INST_PROP(n, inversion),                                           \
	};                                                                                         \
	                                                                                           \
	DEVICE_DT_INST_DEFINE(n, rm69310_init, NULL, &rm69310_data_##n, &rm69310_config_##n,       \
			      POST_KERNEL, CONFIG_DISPLAY_INIT_PRIORITY, &rm69310_api);

DT_INST_FOREACH_STATUS_OKAY(RM69310_INIT)
