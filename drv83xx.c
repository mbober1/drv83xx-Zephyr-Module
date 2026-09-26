/*
 * SPDX-FileCopyrightText: Copyright (c) 2026 Marcin Bober
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT ti_drv83xx

#include <zephyr/drivers/bldc.h>
#include <zephyr/drivers/spi.h>
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


union drv83xx_bus {
	struct spi_dt_spec spi;
};


typedef int (*drv83xx_bus_check_fn)(const union drv83xx_bus *bus);
typedef int (*drv83xx_data_read_fn)(const union drv83xx_bus *bus, struct drv83xx_reading *buffer);


struct drv83xx_bus_io {
	drv83xx_bus_check_fn check;
	drv83xx_data_read_fn read;
};

struct drv83xx_reading {
	uint32_t data;
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
};

/**
 * @brief DRV83XX bldc driver data.
 */
struct drv83xx_data {
	const struct device *dev;
	// struct drv83xx_pin_states pin_states;
	struct gpio_callback fault_cb_data;
	bldc_drv_event_cb_t fault_cb;
	void *fault_cb_user_data;
};

static int drv83xx_enable(const struct device *dev)
{
	const struct drv83xx_config *config = dev->config;
	struct drv83xx_data *data = dev->data;
	int ret;




	return ret;
}

static int drv83xx_disable(const struct device *dev)
{
	const struct drv83xx_config *config = dev->config;
	struct drv83xx_data *data = dev->data;
	int ret;


	return ret;
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
	uint8_t buffer_tx[2] = { (addr<<1) | DRV83XX_SPI_READ, 0 };
	// buffer_tx[0] |= get_parity(buffer_tx[0], buffer_tx[1]);
	uint8_t status;

	const struct spi_buf tx_buf = {
			.buf = buffer_tx,
			.len = 2,
	};
	const struct spi_buf_set tx = {
		.buffers = &tx_buf,
		.count = 1
	};
	const struct spi_buf rx_buf[2] = {
		{
			.buf = &status,
			.len = 1,
		},
		{
			.buf = data,
			.len = 1,
		}
	};
	const struct spi_buf_set rx = {
		.buffers = rx_buf,
		.count = 2
	};

	if (spi_transceive_dt(&config->bus.spi, &tx, &rx)) {
		return -EIO;
	}

	return 0;
}

const struct drv83xx_bus_io drv83xx_bus_io_spi = {
	.check = drv83xx_bus_check_spi,
	.read = drv83xx_data_read_spi
};

static int drv83xx_init(const struct device *dev)
{
	const struct drv83xx_config *const config = dev->config;
	const union drv83xx_bus *bus = &(config->bus);
	int ret;

	uint8_t data;
	ret = drv83xx_spi_read(dev, 0x0, &data);

	// setPWMMode
	// setSlew
	// setCurrentSenseGain
	// setOCPMode
	// setBuckVoltage

	return 0;
}

#define DRV83XX_CONFIG_SPI(inst)				\
	{						\
		.bus.spi = SPI_DT_SPEC_INST_GET(inst, DRV83XX_SPI_OPERATION),	\
		.bus_io = &drv83xx_bus_io_spi,		\
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
