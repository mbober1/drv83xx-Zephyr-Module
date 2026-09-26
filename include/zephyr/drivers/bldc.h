/*
 * SPDX-FileCopyrightText: Copyright (c) 2026 Marcin Bober
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file drivers/bldc.h
 * @ingroup bldc_interface
 * @brief Main header file for bldc driver API.
 */

#ifndef ZEPHYR_INCLUDE_DRIVERS_BLDC_H_
#define ZEPHYR_INCLUDE_DRIVERS_BLDC_H_

/**
 * @brief Interfaces for bldc motor controllers.
 * @defgroup bldc_interface bldc
 * @since 4.0
 * @version 0.8.0
 * @ingroup io_interfaces
 * @{
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <errno.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief bldc Motor direction options
 */
enum bldc_direction {
	/** Negative direction */
	BLDC_DIRECTION_NEGATIVE = 0,
	/** Positive direction */
	BLDC_DIRECTION_POSITIVE = 1,
};

/**
 * @brief bldc Motor run mode options
 */
enum bldc_run_mode {
	/** Torque Mode */
	BLDC_RUN_MODE_TORQUE = 0,
	/** Position Mode */
	BLDC_RUN_MODE_POSITION = 1,
	/** Velocity Mode */
	BLDC_RUN_MODE_VELOCITY = 2,
};

/**
 * @brief bldc Events
 */
enum bldc_event {
	/** Steps set using move_by or move_to have been executed */
	BLDC_EVENT_STEPS_COMPLETED = 0,
	/** Left end switch status changes to pressed */
	BLDC_EVENT_LEFT_END_STOP_DETECTED = 1,
	/** Right end switch status changes to pressed */
	BLDC_EVENT_RIGHT_END_STOP_DETECTED = 2,
	/** bldc has stopped */
	BLDC_EVENT_STOPPED = 3,
};

/**
 * @cond INTERNAL_HIDDEN
 *
 * bldc driver API definition and system call entry points.
 *
 */

/**
 * @brief Set the reference position of the bldc
 *
 * @see bldc_set_actual_position() for details.
 */
typedef int (*bldc_set_reference_position_t)(const struct device *dev, const int32_t value);

/**
 * @brief Get the actual a.k.a reference position of the bldc
 *
 * @see bldc_get_actual_position() for details.
 */
typedef int (*bldc_get_actual_position_t)(const struct device *dev, int32_t *value);

/**
 * @brief Callback function for bldc events
 */
typedef void (*bldc_event_callback_t)(const struct device *dev, const enum bldc_event event,
					 void *user_data);

/**
 * @brief Set the callback function to be called when a bldc event occurs
 *
 * @see bldc_set_event_callback() for details.
 */
typedef int (*bldc_set_event_callback_t)(const struct device *dev,
					    bldc_event_callback_t callback, void *user_data);

/**
 * @brief Move the bldc relatively by a given number of micro-steps.
 *
 * @see bldc_move_by() for details.
 */
typedef int (*bldc_move_by_t)(const struct device *dev, const int32_t micro_steps);

/**
 * @brief Move the bldc to an absolute position in micro-steps.
 *
 * @see bldc_move_to() for details.
 */
typedef int (*bldc_move_to_t)(const struct device *dev, const int32_t micro_steps);

/**
 * @brief Run the bldc with a given step interval in a given direction
 *
 * @see bldc_run() for details.
 */
typedef int (*bldc_run_t)(const struct device *dev, const enum bldc_direction direction);

/**
 * @brief Stop the bldc
 *
 * @see bldc_stop() for details.
 */
typedef int (*bldc_stop_t)(const struct device *dev);

/**
 * @brief Is the target position fo the bldc reached
 *
 * @see bldc_is_moving() for details.
 */
typedef int (*bldc_is_moving_t)(const struct device *dev, bool *is_moving);

/**
 * @brief bldc Driver API
 */
__subsystem struct bldc_driver_api {
	bldc_set_reference_position_t set_reference_position;
	bldc_get_actual_position_t get_actual_position;
	bldc_set_event_callback_t set_event_callback;
	bldc_move_by_t move_by;
	bldc_move_to_t move_to;
	bldc_run_t run;
	bldc_stop_t stop;
	bldc_is_moving_t is_moving;
};

/**
 * @endcond
 */

/**
 * @brief Set the reference position of the bldc
 *
 * @param dev pointer to the device structure for the driver instance.
 * @param value The reference position to set in micro-steps.
 *
 * @retval -EIO General input / output error
 * @retval -ENOSYS If not implemented by device driver
 * @retval 0 Success
 */
__syscall int bldc_set_reference_position(const struct device *dev, int32_t value);

static inline int z_impl_bldc_set_reference_position(const struct device *dev,
							const int32_t value)
{
	__ASSERT_NO_MSG(dev != NULL);
	const struct bldc_driver_api *api = (const struct bldc_driver_api *)dev->api;

	if (api->set_reference_position == NULL) {
		return -ENOSYS;
	}
	return api->set_reference_position(dev, value);
}

/**
 * @brief Get the actual step count for a given bldc.
 * @note This function does not guarantee that the returned position is the exact current
 * position. For precise positioning, encoders should be used in addition to the bldc driver.
 *
 * @param dev pointer to the device structure for the driver instance.
 * @param value The actual position to get in micro-steps
 *
 * @retval -EIO General input / output error
 * @retval -ENOSYS If not implemented by device driver
 * @retval 0 Success
 */
__syscall int bldc_get_actual_position(const struct device *dev, int32_t *value);

static inline int z_impl_bldc_get_actual_position(const struct device *dev, int32_t *value)
{
	__ASSERT_NO_MSG(dev != NULL);
	__ASSERT_NO_MSG(value != NULL);
	const struct bldc_driver_api *api = (const struct bldc_driver_api *)dev->api;

	if (api->get_actual_position == NULL) {
		return -ENOSYS;
	}
	return api->get_actual_position(dev, value);
}

/**
 * @brief Set the callback function to be called when a bldc event occurs
 *
 * @param dev pointer to the device structure for the driver instance.
 * @param callback Callback function to be called when a bldc event occurs
 * passing NULL will disable the callback
 * @param user_data User data to be passed to the callback function
 *
 * @retval -ENOSYS If not implemented by device driver
 * @retval 0 Success
 */
__syscall int bldc_set_event_callback(const struct device *dev,
					 bldc_event_callback_t callback, void *user_data);

static inline int z_impl_bldc_set_event_callback(const struct device *dev,
						    bldc_event_callback_t callback,
						    void *user_data)
{
	__ASSERT_NO_MSG(dev != NULL);
	const struct bldc_driver_api *api = (const struct bldc_driver_api *)dev->api;

	if (api->set_event_callback == NULL) {
		return -ENOSYS;
	}
	return api->set_event_callback(dev, callback, user_data);
}


/**
 * @brief Set the micro-steps to be moved from the current position i.e. relative movement
 *
 * @note The bldc will move by the given number of micro-steps from the current position.
 * This function is non-blocking.
 *
 * @param dev pointer to the device structure for the driver instance.
 * @param micro_steps target micro-steps to be moved from the current position
 *
 * @retval -EIO General input / output error
 * @retval -EINVAL If the timing for steps is incorrectly configured
 * @retval 0 Success
 */
__syscall int bldc_move_by(const struct device *dev, int32_t micro_steps);

static inline int z_impl_bldc_move_by(const struct device *dev, const int32_t micro_steps)
{
	__ASSERT_NO_MSG(dev != NULL);
	const struct bldc_driver_api *api = (const struct bldc_driver_api *)dev->api;

	return api->move_by(dev, micro_steps);
}

/**
 * @brief Set the absolute target position of the bldc
 *
 * @note The bldc will move to the given micro-steps position from the reference position.
 * This function is non-blocking.
 *
 * @param dev pointer to the device structure for the driver instance.
 * @param micro_steps target position to set in micro-steps
 *
 * @retval -EIO General input / output error
 * @retval -EINVAL If the timing for steps is incorrectly configured
 * @retval 0 Success
 */
__syscall int bldc_move_to(const struct device *dev, int32_t micro_steps);

static inline int z_impl_bldc_move_to(const struct device *dev, const int32_t micro_steps)
{
	__ASSERT_NO_MSG(dev != NULL);
	const struct bldc_driver_api *api = (const struct bldc_driver_api *)dev->api;

	return api->move_to(dev, micro_steps);
}

/**
 * @brief Run the bldc with a given step interval in a given direction
 *
 * @note The bldc shall be set into motion and run continuously until
 * stalled or stopped using some other command, for instance, bldc_stop(). This
 * function is non-blocking.
 *
 * @param dev pointer to the device structure for the driver instance.
 * @param direction The direction to set
 *
 * @retval -EIO General input / output error
 * @retval -EINVAL If the timing for steps is incorrectly configured
 * @retval -ENOSYS If not implemented by device driver
 * @retval 0 Success
 */
__syscall int bldc_run(const struct device *dev, enum bldc_direction direction);

static inline int z_impl_bldc_run(const struct device *dev,
				     const enum bldc_direction direction)
{
	__ASSERT_NO_MSG(dev != NULL);
	const struct bldc_driver_api *api = (const struct bldc_driver_api *)dev->api;

	if (api->run == NULL) {
		return -ENOSYS;
	}
	return api->run(dev, direction);
}

/**
 * @brief Stop the bldc
 * @note Cancel all active movements.
 *
 * @param dev pointer to the device structure for the driver instance.
 *
 * @retval -EIO General input / output error
 * @retval -ENOSYS If not implemented by device driver
 * @retval 0 Success
 */
__syscall int bldc_stop(const struct device *dev);

static inline int z_impl_bldc_stop(const struct device *dev)
{
	__ASSERT_NO_MSG(dev != NULL);
	const struct bldc_driver_api *api = (const struct bldc_driver_api *)dev->api;

	if (api->stop == NULL) {
		return -ENOSYS;
	}
	return api->stop(dev);
}

/**
 * @brief Check if the bldc is currently moving
 *
 * @param dev pointer to the device structure for the driver instance.
 * @param is_moving Pointer to a boolean to store the moving status of the bldc
 *
 * @retval -EIO General input / output error
 * @retval -ENOSYS If not implemented by device driver
 * @retval 0 Success
 */
__syscall int bldc_is_moving(const struct device *dev, bool *is_moving);

static inline int z_impl_bldc_is_moving(const struct device *dev, bool *is_moving)
{
	__ASSERT_NO_MSG(dev != NULL);
	__ASSERT_NO_MSG(is_moving != NULL);
	const struct bldc_driver_api *api = (const struct bldc_driver_api *)dev->api;

	if (api->is_moving == NULL) {
		return -ENOSYS;
	}
	return api->is_moving(dev, is_moving);
}


enum bldc_drv_event {
	/** bldc driver stall detected */
	BLDC_DRV_EVENT_STALL_DETECTED = 0,
	/** bldc driver fault detected */
	BLDC_DRV_EVENT_FAULT_DETECTED = 1,
};

/**
 * @cond INTERNAL_HIDDEN
 *
 * bldc Drv driver API definition and system call entry points.
 *
 */

/**
 * @brief Enable the bldc driver
 *
 * @see bldc_drv_enable() for details.
 */
typedef int (*bldc_drv_enable_t)(const struct device *dev);

/**
 * @brief Disable the bldc driver
 *
 * @see bldc_drv_disable() for details.
 */
typedef int (*bldc_drv_disable_t)(const struct device *dev);

/**
 * @brief Callback function for bldc driver events
 */
typedef void (*bldc_drv_event_cb_t)(const struct device *dev, const enum bldc_drv_event event,
				       void *user_data);

/**
 * @brief Set the callback function to be called when a bldc_drv_event occurs
 *
 * @see bldc_drv_set_event_callback() for details.
 */
typedef int (*bldc_drv_set_event_callback_t)(const struct device *dev,
						bldc_drv_event_cb_t callback, void *user_data);

/**
 * @brief bldc DRV Driver API
 */
__subsystem struct bldc_drv_driver_api {
	bldc_drv_enable_t enable;
	bldc_drv_disable_t disable;
	bldc_drv_set_event_callback_t set_event_cb;
};

/**
 * @endcond
 */

/**
 * @brief Enable bldc driver
 *
 * @details Enabling the driver shall switch on the power stage and energize the coils.
 *
 * @param dev pointer to the device structure for the driver instance.
 *
 * @retval -EIO Error during Enabling
 * @retval 0 Success
 */
__syscall int bldc_drv_enable(const struct device *dev);

static inline int z_impl_bldc_drv_enable(const struct device *dev)
{
	__ASSERT_NO_MSG(dev != NULL);
	const struct bldc_drv_driver_api *api = (const struct bldc_drv_driver_api *)dev->api;

	return api->enable(dev);
}

/**
 * @brief Disable bldc driver
 *
 * @details Disabling the driver shall switch off the power stage and de-energize the coils.
 *
 * @param dev pointer to the device structure for the driver instance.
 *
 * @retval  -ENOTSUP Disabling of driver is not supported.
 * @retval -EIO Error during Disabling
 * @retval 0 Success
 */
__syscall int bldc_drv_disable(const struct device *dev);

static inline int z_impl_bldc_drv_disable(const struct device *dev)
{
	__ASSERT_NO_MSG(dev != NULL);
	const struct bldc_drv_driver_api *api = (const struct bldc_drv_driver_api *)dev->api;

	return api->disable(dev);
}

/**
 * @brief Set the callback function to be called when a bldc_drv_event occurs
 *
 * @param dev pointer to the device structure for the driver instance.
 * @param callback Callback function to be called when a bldc_drv_event occurs
 * passing NULL will disable the callback
 * @param user_data User data to be passed to the callback function
 *
 * @retval -ENOSYS If not implemented by device driver
 * @retval 0 Success
 */
__syscall int bldc_drv_set_event_cb(const struct device *dev, bldc_drv_event_cb_t callback,
				       void *user_data);

static inline int z_impl_bldc_drv_set_event_cb(const struct device *dev,
						  bldc_drv_event_cb_t cb, void *user_data)
{
	__ASSERT_NO_MSG(dev != NULL);
	const struct bldc_drv_driver_api *api = (const struct bldc_drv_driver_api *)dev->api;

	if (api->set_event_cb == NULL) {
		return -ENOSYS;
	}

	return api->set_event_cb(dev, cb, user_data);
}

/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#include <zephyr/syscalls/bldc.h>

#endif /* ZEPHYR_INCLUDE_DRIVERS_BLDC_H_ */
