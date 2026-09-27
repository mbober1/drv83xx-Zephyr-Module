/*
 * SPDX-FileCopyrightText: Copyright (c) 2026 Marcin Bober
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT ti_drv83xx

#include <zephyr/drivers/bldc.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/spi.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/util.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(drv83xx, CONFIG_BLDC_LOG_LEVEL);

/* Enable and wake up times of the drv83xx bldc controller family. Only after they have elapsed
 * are controller output signals guaranteed to be valid.
 */
#define DRV83XX_ENABLE_TIME  K_NSEC(50)
#define DRV83XX_WAKE_UP_TIME K_USEC(1000)
#define DRV83XX_SPI_WRITE	(0 << 7)
#define DRV83XX_SPI_READ	(1 << 7)
#define DRV83XX_SPI_OPERATION (SPI_WORD_SET(8) | SPI_TRANSFER_MSB | SPI_MODE_CPHA)
#define DRV8316_REG_CONTROL1	(0x3)
#define DRV8316_REG_CONTROL2	(0x4)
#define DRV8316_REG_CONTROL3	(0x5)
#define DRV8316_REG_CONTROL4	(0x6)
#define DRV8316_REG_CONTROL5	(0x7)
#define DRV8316_REG_CONTROL6	(0x8)
#define DRV8316_REG_CONTROL10	(0xc)
#define DRV8316_REG_LOCK			(0x06)
#define DRV8316_REG_UNLOCK		(0x03)


union drv83xx_bus {
	struct spi_dt_spec spi;
};

struct drv83xx_reading {
	uint32_t data;
};

typedef int (*drv83xx_bus_check_fn)(const union drv83xx_bus *bus);
typedef int (*drv83xx_data_read_fn)(const union drv83xx_bus *bus, struct drv83xx_reading *buffer);


static int drv83xx_spi_read(const struct device *dev, uint8_t addr, uint8_t *data);
static int drv83xx_spi_write(const struct device *dev, uint8_t addr, uint8_t value);

struct drv83xx_bus_io {
	drv83xx_bus_check_fn check;
	drv83xx_data_read_fn read;
};

/**
 * @brief DRV83XX bldc driver configuration data.
 *
 * This structure contains all the devicetree specifications for the pins
 * needed by a given DRV83XX bldc driver.
 */
struct drv83xx_config {
	union drv83xx_bus bus;
	const struct drv83xx_bus_io *bus_io;
	struct gpio_dt_spec enable_gpio;
	uint8_t pwm_mode;
	uint8_t slew_rate;
	uint8_t sdo_mode;
	uint8_t csa_gain;
	uint8_t ocp_mode;
	uint8_t ocp_level;
	uint8_t ocp_retry;
	uint8_t ocp_deglitch;
	uint8_t recirculation_mode;
	uint8_t pwm_100_duty_frequency;
	uint8_t buck_voltage;
	uint8_t buck_current_limit;
	uint8_t delay_target;
	bool overvoltage_enable;
	bool overvoltage_level;
	bool overtemperature_report;
	bool spi_fault_report;
	bool ocp_cycle_by_cycle;
	bool synchronous_rectification;
	bool asynchronous_rectification;
	bool buck_enable;
	bool buck_power_sequencing;
	bool delay_compensation;
};

/**
 * @brief DRV83XX bldc driver data.
 */
struct drv83xx_data {
	const struct device *dev;
	struct gpio_callback fault_cb_data;
	bldc_drv_event_cb_t fault_cb;
	void *fault_cb_user_data;
};

static int drv83xx_enable(const struct device *dev)
{
	return 0;
}

static int drv83xx_disable(const struct device *dev)
{
	return 0;
}

static int drv83xx_set_fault_cb(const struct device *dev, bldc_drv_event_cb_t fault_cb,
				void *user_data)
{
	struct drv83xx_data *data = dev->data;

	data->fault_cb = fault_cb;
	data->fault_cb_user_data = user_data;

	return 0;
}

void fault_event(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
	struct drv83xx_data *data = CONTAINER_OF(cb, struct drv83xx_data, fault_cb_data);

	if (data->fault_cb != NULL) {
		data->fault_cb(data->dev, BLDC_DRV_EVENT_FAULT_DETECTED,
			data->fault_cb_user_data);
	} else {
		LOG_WRN_ONCE("%s: Fault pin triggered but no callback is set", dev->name);
	}
}

static int drv83xx_bus_check_spi(const union drv83xx_bus *bus)
{
	return spi_is_ready_dt(&bus->spi) ? 0 : -ENODEV;
}


static inline int drv83xx_data_read_spi(const union drv83xx_bus *bus, 
                      struct drv83xx_reading *out)
{
	uint8_t buffer[3];
	const struct spi_buf rx_buf = {
		.buf = buffer,
		.len = ARRAY_SIZE(buffer),
	};

	const struct spi_buf_set rx_bufs = {
		.buffers = &rx_buf,
		.count = 1U
	};

	int ret = spi_read_dt(&bus->spi, &rx_bufs);

	uint32_t value = 	((uint32_t)buffer[0] << 16) |
										((uint32_t)buffer[1] << 8)  |
										((uint32_t)buffer[2]);
	
	out->data = 	value;

	if (ret < 0) {
		LOG_ERR("SPI read failed: %d", ret);
	}
	else
	{
		LOG_DBG("SPI read succeeded: 0x%06X", out->data);
	}

	return ret;
}

uint8_t get_parity(uint8_t b1, uint8_t b2)
{
	uint8_t par = __builtin_popcount(b1) + __builtin_popcount(b2);
	uint8_t result = (par&0x01)==0x01;
	return result & 0x01;
}

static int drv83xx_spi_read(const struct device *dev, uint8_t addr, uint8_t *data)
{
	const struct drv83xx_config *config = dev->config;
	uint8_t tx_data[2] = {(uint8_t)((addr << 1) | DRV83XX_SPI_READ), 0};
	uint8_t rx_data[2];
	struct spi_buf tx_buf = {.buf = tx_data, .len = sizeof(tx_data)};
	struct spi_buf rx_buf = {.buf = rx_data, .len = sizeof(rx_data)};
	struct spi_buf_set tx = {.buffers = &tx_buf, .count = 1};
	struct spi_buf_set rx = {.buffers = &rx_buf, .count = 1};
	uint16_t frame = ((uint16_t)tx_data[0] << 8) | tx_data[1];
	int ret;

	if ((__builtin_popcount((unsigned int)frame) & 1U) != 0U) {
		tx_data[0] |= 1U;
	}
	ret = spi_transceive_dt(&config->bus.spi, &tx, &rx);
	if (ret == 0) {
		*data = rx_data[1];
	}

	if (ret < 0) {
		LOG_ERR("SPI read failed: %d", ret);
	}
	else
	{
		LOG_DBG("SPI read succeeded: 0x%02X", *data);
	}

	return ret;
}

static int drv83xx_spi_write(const struct device *dev, uint8_t addr, uint8_t value)
{
	const struct drv83xx_config *config = dev->config;
	uint8_t tx_data[2] = {(uint8_t)(addr << 1), value};
	uint8_t rx_data[2];
	uint8_t actual;
	struct spi_buf tx_buf = {.buf = tx_data, .len = sizeof(tx_data)};
	struct spi_buf rx_buf = {.buf = rx_data, .len = sizeof(rx_data)};
	struct spi_buf_set tx = {.buffers = &tx_buf, .count = 1};
	struct spi_buf_set rx = {.buffers = &rx_buf, .count = 1};
	uint16_t frame = ((uint16_t)tx_data[0] << 8) | tx_data[1];

	if ((__builtin_popcount((unsigned int)frame) & 1U) != 0U) {
		tx_data[0] |= 1U;
	}

	int ret = spi_transceive_dt(&config->bus.spi, &tx, &rx);

	#if CONFIG_DRV83XX_READBACK
	ret = drv83xx_spi_read(dev, addr, &actual);

	if (ret >= 0 && (actual != value)) {
		LOG_ERR("Register 0x%02x mismatch: expected 0x%02x, got 0x%02x",
			addr, value, actual);
	}
	else
	{
		LOG_DBG("Register 0x%02x verified successfully", addr);
	}
	#endif

	if (ret < 0) {
		LOG_ERR("SPI write to register 0x%02x failed: %d", addr, ret);
	}
	else
	{
		LOG_DBG("SPI write reg 0x%02x = 0x%02x, response 0x%02x%02x",
			addr, value, rx_data[0], rx_data[1]);
	}
	return ret;
}

const struct drv83xx_bus_io drv83xx_bus_io_spi = {
	.check = drv83xx_bus_check_spi,
	.read = drv83xx_data_read_spi
};

static int drv83xx_configure_registers(const struct device *dev)
{
	const struct drv83xx_config *config = dev->config;
	uint8_t value;
	int ret;

	ret = drv83xx_spi_write(dev, DRV8316_REG_CONTROL1, DRV8316_REG_UNLOCK);
	if (ret < 0) {
		return ret;
	}

	ret = drv83xx_spi_read(dev, DRV8316_REG_CONTROL2, &value);
	if (ret < 0) {
		goto lock_registers;
	}
	value = (value & ~0x3fU) | (config->pwm_mode << 1) |
		(config->slew_rate << 3) | (config->sdo_mode << 5);
	ret = drv83xx_spi_write(dev, DRV8316_REG_CONTROL2, value);
	if (ret < 0) {
		goto lock_registers;
	}

	ret = drv83xx_spi_read(dev, DRV8316_REG_CONTROL3, &value);
	if (ret < 0) {
		goto lock_registers;
	}
	value &= ~0x1fU;
	value |= (config->overtemperature_report ? BIT(0) : 0U) |
		 (config->spi_fault_report ? 0U : BIT(1)) |
		 (config->overvoltage_enable ? BIT(2) : 0U) |
		 (config->overvoltage_level ? BIT(3) : 0U) |
		 (config->pwm_100_duty_frequency ? BIT(4) : 0U);
	ret = drv83xx_spi_write(dev, DRV8316_REG_CONTROL3, value);
	if (ret < 0) {
		goto lock_registers;
	}

	ret = drv83xx_spi_read(dev, DRV8316_REG_CONTROL4, &value);
	if (ret < 0) {
		goto lock_registers;
	}
	value = (value & ~0x7fU) | config->ocp_mode | (config->ocp_level << 2) |
		(config->ocp_retry << 3) | (config->ocp_deglitch << 4) |
		(config->ocp_cycle_by_cycle ? BIT(6) : 0U);
	ret = drv83xx_spi_write(dev, DRV8316_REG_CONTROL4, value);
	if (ret < 0) {
		goto lock_registers;
	}

	ret = drv83xx_spi_read(dev, DRV8316_REG_CONTROL5, &value);
	if (ret < 0) {
		goto lock_registers;
	}
	value = (value & ~0x4fU) | config->csa_gain |
		(config->synchronous_rectification ? BIT(2) : 0U) |
		(config->asynchronous_rectification ? BIT(3) : 0U) |
		(config->recirculation_mode << 6);
	ret = drv83xx_spi_write(dev, DRV8316_REG_CONTROL5, value);
	if (ret < 0) {
		goto lock_registers;
	}

	ret = drv83xx_spi_read(dev, DRV8316_REG_CONTROL6, &value);
	if (ret < 0) {
		goto lock_registers;
	}
	value = (value & ~0x1fU) | (config->buck_enable ? 0U : BIT(0)) |
		(config->buck_voltage << 1) | (config->buck_current_limit << 3) |
		(config->buck_power_sequencing ? 0U : BIT(4));
	ret = drv83xx_spi_write(dev, DRV8316_REG_CONTROL6, value);
	if (ret < 0) {
		goto lock_registers;
	}

	ret = drv83xx_spi_read(dev, DRV8316_REG_CONTROL10, &value);
	if (ret < 0) {
		goto lock_registers;
	}
	value = (value & ~0x1fU) | config->delay_target |
		(config->delay_compensation ? BIT(4) : 0U);
	ret = drv83xx_spi_write(dev, DRV8316_REG_CONTROL10, value);

lock_registers:
	{
		int lock_ret = drv83xx_spi_write(dev, DRV8316_REG_CONTROL1,
						 DRV8316_REG_LOCK);

		if (ret < 0) {
			return ret;
		}
		if (lock_ret < 0) {
			return lock_ret;
		}
	}

	return 0;
}

static int drv83xx_init(const struct device *dev)
{
	const struct drv83xx_config *const config = dev->config;
	int ret;

	if (!device_is_ready(config->enable_gpio.port)) {
		LOG_ERR("Enable GPIO controller is not ready");
		return -ENODEV;
	}
	ret = gpio_pin_configure_dt(&config->enable_gpio, GPIO_OUTPUT_INACTIVE);
	if (ret < 0) {
		LOG_ERR("Could not configure enable GPIO (%d)", ret);
		return ret;
	}
	ret = gpio_pin_set_dt(&config->enable_gpio, 1);
	if (ret < 0) {
		LOG_ERR("Could not wake DRV8316C (%d)", ret);
		return ret;
	}
	k_sleep(DRV83XX_WAKE_UP_TIME);

	ret = config->bus_io->check(&config->bus);

	if (ret < 0) {
		return ret;
	}

	ret = drv83xx_configure_registers(dev);
	if (ret < 0) {
		LOG_ERR("DRV8316C register configuration failed (%d)", ret);
	}

	return ret;
}

#define DRV83XX_CONFIG_SPI(inst)				\
	{						\
		.bus.spi = SPI_DT_SPEC_INST_GET(inst, DRV83XX_SPI_OPERATION),	\
		.bus_io = &drv83xx_bus_io_spi,		\
		.enable_gpio = GPIO_DT_SPEC_INST_GET(inst, enable_gpios),\
		.pwm_mode = DT_INST_PROP(inst, pwm_mode),	\
		.slew_rate = DT_INST_PROP(inst, slew_rate),	\
		.sdo_mode = DT_INST_PROP(inst, sdo_mode),	\
		.csa_gain = DT_INST_PROP(inst, csa_gain),	\
		.ocp_mode = DT_INST_PROP(inst, ocp_mode),	\
		.ocp_level = DT_INST_PROP(inst, ocp_level),	\
		.ocp_retry = DT_INST_PROP(inst, ocp_retry),	\
		.ocp_deglitch = DT_INST_PROP(inst, ocp_deglitch),	\
		.recirculation_mode = DT_INST_PROP(inst, recirculation_mode),\
		.pwm_100_duty_frequency = DT_INST_PROP(inst, pwm_100_duty_frequency),\
		.buck_voltage = DT_INST_PROP(inst, buck_voltage),	\
		.buck_current_limit = DT_INST_PROP(inst, buck_current_limit),\
		.delay_target = DT_INST_PROP(inst, delay_target),	\
		.overvoltage_enable = DT_INST_PROP(inst, overvoltage_enable),\
		.overvoltage_level = DT_INST_PROP(inst, overvoltage_level),	\
		.overtemperature_report = DT_INST_PROP(inst, overtemperature_report),\
		.spi_fault_report = DT_INST_PROP(inst, spi_fault_report),\
		.ocp_cycle_by_cycle = DT_INST_PROP(inst, ocp_cycle_by_cycle),\
		.synchronous_rectification = DT_INST_PROP(inst, synchronous_rectification),\
		.asynchronous_rectification = DT_INST_PROP(inst, asynchronous_rectification),\
		.buck_enable = DT_INST_PROP(inst, buck_enable),	\
		.buck_power_sequencing = DT_INST_PROP(inst, buck_power_sequencing),\
		.delay_compensation = DT_INST_PROP(inst, delay_compensation),\
	};


static DEVICE_API(bldc_drv, drv83xx_bldc_api) = {
	.enable = drv83xx_enable,
	.disable = drv83xx_disable,
	.set_event_cb = drv83xx_set_fault_cb,
};


#define DRV83XX_DEVICE(inst)                                                                       \
	static const struct drv83xx_config drv83xx_config_##inst =	\
		COND_CODE_1(DT_INST_ON_BUS(inst, spi),			\
			(DRV83XX_CONFIG_SPI(inst)),			\
			());			\
                                                                                                   \
	static struct drv83xx_data drv83xx_data_##inst = {                                         \
		.dev = DEVICE_DT_INST_GET(inst),                                                   \
	};                                                                                         \
                                                                                                   \
	DEVICE_DT_INST_DEFINE(inst, &drv83xx_init, NULL, &drv83xx_data_##inst,                     \
			      &drv83xx_config_##inst, POST_KERNEL, CONFIG_BLDC_INIT_PRIORITY,   \
			      &drv83xx_bldc_api);

DT_INST_FOREACH_STATUS_OKAY(DRV83XX_DEVICE)
