/*****************************************************************************
 * Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.
 ******************************************************************************
 *
 * OPT4048 Sensor Thread Application - Power-Optimized Implementation
 *
 * This module provides power-focused ambient light sensing using one-shot
 * measurement mode with threshold-based hysteresis. The sensor stays in
 * power-down mode and only triggers measurements on-demand, significantly
 * reducing power consumption.
 *
 * Power-Focused Features:
 * - One-shot measurement mode (sensor powered only during measurement)
 * - Threshold-based hysteresis (10% lux change triggers update)
 * - Auto power-down between measurements
 * - On-demand sampling reduces average power to <50µA
 * - Interrupt-driven change detection (when interrupt pin available)
 *
 * Operation:
 * - Thread polls every 1 second in S0 state
 * - Each poll triggers one-shot measurement
 * - Sensor auto-returns to power-down after conversion
 * - Updates ACPI only when lux changes >10%
 * - Typical power: 0.5mA for 100ms, then 0µA for 900ms = 50µA average
 *
 *****************************************************************************/
#include <zephyr/kernel.h>
#include <errno.h>
#include <zephyr/logging/log.h>
#include "app_opt4048_thread.h"
#include "app_pseq.h"
#include "board_config.h"
#include "task_handler.h"
#include "dev_opt4048.h"
#include "app_acpi.h"
#include "acpiplanes.h"
#include "scicodes.h"
LOG_MODULE_REGISTER(opt4048_app, CONFIG_ALS_LOG_LEVEL);

#define ALS_CHANGE_THRESHOLD_PERCENT  10  /* Notify if change > 10% */
/* One-shot mode timing */
#define ONE_SHOT_CONVERSION_TIME_MS   100  /* 100ms conversion time */
#define ONE_SHOT_WAIT_TIME_MS         120  /* Wait 120ms for conversion to complete */

/* ************************** *
 *     global valuable        *
 * ************************** */

/* ************************** *
 *     static valuable        *
 * ************************** */
/* Task control */
static uint8_t *task_opt4048_slp_ready;

/* Sensor state */
static bool sensor_initialized = false;
static bool sensor_enabled = false;

/* Sensor data cache */
static opt4048_color_t current_color_data;
static uint32_t current_lux = 0;
static double current_cct = 0.0;

/* ALS change detection with hysteresis */
static uint32_t last_reported_lux = 0;

/* Interrupt-driven operation control */
static volatile bool interrupt_pending = false;             /* Flag set by interrupt callback */

/* ACPI ALS data - accessible by OS */
static uint16_t acpi_als_lux = 0;      /* Offset 0-1: ALS lux value */
static uint16_t acpi_als_red = 0;      /* Offset 2-3: Red channel */
static uint16_t acpi_als_green = 0;    /* Offset 4-5: Green channel */
static uint16_t acpi_als_blue = 0;     /* Offset 6-7: Blue channel */
static uint16_t acpi_als_white = 0;    /* Offset 8-9: White/IR channel */
static uint16_t acpi_als_cct = 0;      /* Offset 10-11: CCT in Kelvin */

/* als thread sem */
static K_SEM_DEFINE(alsSem, 0, 1);              /* Semaphore to trigger als thread */

/**
 * @brief als trigger measurement timer callback
 *
 * @return None
 */
static void _app_opt4048_tick(struct k_timer *timer)
{
	app_opt4048_giveSem();
}
K_TIMER_DEFINE(als_timer, _app_opt4048_tick, NULL);

/**
 * @brief Initialize OPT4048 sensor for power-optimized one-shot operation
 *
 * @return true if successful, false otherwise
 */
static bool _app_opt4048_init_sensor(void)
{
	int ret;

	if (sensor_initialized) {
		return true;
	}

	LOG_INF("Initializing OPT4048 sensor (power-optimized one-shot mode)");

	ret = dev_opt4048_init();
	if (ret != 0) {
		LOG_ERR("Failed to initialize OPT4048: %d", ret);
		return false;
	}

	/* Configure for one-shot mode with optimal power settings */
	/* Set conversion time to 100ms for good accuracy vs power tradeoff */
	ret = dev_opt4048_set_conversion_time(OPT4048_CONVERSION_TIME_100MS);
	if (ret != 0) {
		LOG_WRN("Failed to set conversion time: %d", ret);
	}

	/* Set auto-ranging for best dynamic range */
	ret = dev_opt4048_set_range(OPT4048_RANGE_AUTO);
	if (ret != 0) {
		LOG_WRN("Failed to set auto range: %d", ret);
	}

	/* Configure threshold on Channel 3 (white/lux) for hysteresis detection */
	ret = dev_opt4048_set_threshold_channel(OPT4048_THRESHOLD_CH_3);
	if (ret != 0) {
		LOG_WRN("Failed to set threshold channel: %d", ret);
	}

	/* Set fault count to 1 (immediate response to changes) */
	ret = dev_opt4048_set_fault_count(OPT4048_FAULT_COUNT_1);
	if (ret != 0) {
		LOG_WRN("Failed to set fault count: %d", ret);
	}

	/* Enable interrupt-driven operation */
	/* Configure interrupt to trigger when data is ready on all channels */
	ret = dev_opt4048_set_int_cfg(OPT4048_INT_CFG_DR_ALL_CHANNELS);
	if (ret != 0) {
		LOG_WRN("Failed to set interrupt config: %d", ret);
	} else {
		LOG_INF("OPT4048 interrupt configured for data-ready mode");
	}

	/* Enable interrupt latching for reliable edge detection */
	ret = dev_opt4048_set_latch(true);
	if (ret != 0) {
		LOG_WRN("Failed to enable interrupt latch: %d", ret);
	}

	/* Ensure sensor starts in power-down mode */
	ret = dev_opt4048_set_operation_mode(OPT4048_OPERATION_MODE_POWER_DOWN);
	if (ret != 0) {
		LOG_WRN("Failed to set power-down mode: %d", ret);
	}

	sensor_initialized = true;
	LOG_INF("OPT4048 initialized in interrupt-driven one-shot mode");

	return true;
}

/**
 * @brief Poll OPT4048 sensor using one-shot measurement
 *
 * Power-optimized: Sensor stays in power-down, triggers one measurement,
 * automatically returns to power-down after conversion completes.
 *
 * @return 0 on success, negative error code on failure
 */
static int _app_opt4048_poll_sensor(void)
{
	int ret;

	if (!sensor_initialized) {
		return -ENODEV;
	}

	/* Trigger one-shot measurement (sensor auto-powers up) */
	ret = dev_opt4048_trigger_one_shot();
	if (ret != 0) {
		LOG_ERR("Failed to trigger one-shot measurement: %d", ret);
		return ret;
	}

	return 0;
}

/**
 * @brief Process existing sensor data without triggering new measurement
 *
 * Used when interrupt indicates data is ready. Reads and processes
 * sensor data without triggering a new one-shot measurement.
 *
 * @return 0 on success, negative error code on failure
 */
static int _app_opt4048_process_data(void)
{
	int ret;
	uint32_t lux_delta;
	bool significant_change = false;

	if (!sensor_initialized) {
		return -ENODEV;
	}

	/* Check if conversion is ready */
	if (!dev_opt4048_get_conv_ready_flag()) {
		LOG_WRN("Conversion not ready, no data to process");
		return -EAGAIN;
	}

	/* Read all color channels */
	ret = dev_opt4048_get_all_channels(&current_color_data);
	if (ret != 0) {
		LOG_ERR("Failed to read OPT4048 channels: %d", ret);
		return ret;
	}

	/* Calculate lux value from green channel (CIE Y coefficient = 0.00215) */
	current_lux = (uint32_t)(current_color_data.green * 0.00215);

	/* Calculate CCT */
	current_cct = dev_opt4048_get_cct();

	/* Update ACPI variables for OS consumption */
	acpi_als_lux = (uint16_t)(current_lux > 0xFFFF ? 0xFFFF : current_lux);
	acpi_als_red = (uint16_t)(current_color_data.red >> 4);
	acpi_als_green = (uint16_t)(current_color_data.green >> 4);
	acpi_als_blue = (uint16_t)(current_color_data.blue >> 4);
	acpi_als_white = (uint16_t)(current_color_data.white >> 4);
	acpi_als_cct = (uint16_t)(current_cct > 0xFFFF ? 0xFFFF : (uint16_t)current_cct);

	/* Hysteresis-based change detection (10% threshold) */
	if (last_reported_lux == 0) {
		/* First reading */
		significant_change = true;
	} else {
		/* Calculate percentage change */
		if (current_lux > last_reported_lux) {
			lux_delta = current_lux - last_reported_lux;
		} else {
			lux_delta = last_reported_lux - current_lux;
		}

		/* Check if change exceeds threshold */
		if ((lux_delta * 100) > (last_reported_lux * ALS_CHANGE_THRESHOLD_PERCENT)) {
			significant_change = true;
		}
	}

	/* Notify ACPI/OS if significant change detected */
	if (significant_change) {
		LOG_INF("ALS change (interrupt): %u -> %u lux (>10%%), notifying OS",
		        last_reported_lux, current_lux);
		/* Send ACPI notification for ALS change (SCI event) */
		ACPI_NotifyHost(ACPI_SCI_LUXR);
		last_reported_lux = current_lux;
	}

	LOG_DBG("OPT4048 data processed: Lux=%u, CCT=%.0fK, R=%u, G=%u, B=%u, W=%u",
	        current_lux, current_cct,
	        current_color_data.red,
	        current_color_data.green,
	        current_color_data.blue,
	        current_color_data.white);

	return 0;
}

/**
 * @brief Enable/disable sensor operation
 *
 * In one-shot mode, "enable" just marks sensor as ready.
 * Actual power control is handled by one-shot trigger.
 *
 * @param enable true to enable, false to disable
 */
static void _app_opt4048_enable(bool enable)
{
	if (enable && !sensor_enabled) {
		LOG_INF("Enabling OPT4048 one-shot operation");
		sensor_enabled = true;
		/* Sensor stays in power-down until one-shot is triggered */
		dev_opt4048_set_operation_mode(OPT4048_OPERATION_MODE_POWER_DOWN);
	} else if (!enable && sensor_enabled) {
		LOG_INF("Disabling OPT4048 operation");
		sensor_enabled = false;
		/* Ensure power-down mode */
		dev_opt4048_set_operation_mode(OPT4048_OPERATION_MODE_POWER_DOWN);
	}
}

/**
 * @brief Get ACPI ALS data for OS consumption
 *
 * This function is called by the ACPI handler in app_acpi.c to retrieve
 * the current ALS sensor data for exposing to the OS via ACPI plane 0xB2.
 *
 * @param lux    Pointer to store lux value
 * @param red    Pointer to store red channel value
 * @param green  Pointer to store green channel value
 * @param blue   Pointer to store blue channel value
 * @param white  Pointer to store white/IR channel value
 * @param cct    Pointer to store CCT value
 */
void app_opt4048_get_acpi_data(uint16_t *lux, uint16_t *red, uint16_t *green,
                                uint16_t *blue, uint16_t *white, uint16_t *cct)
{
	if (lux)   *lux = acpi_als_lux;
	if (red)   *red = acpi_als_red;
	if (green) *green = acpi_als_green;
	if (blue)  *blue = acpi_als_blue;
	if (white) *white = acpi_als_white;
	if (cct)   *cct = acpi_als_cct;
}

/**
 * @brief OPT4048 application initialization
 *
 * @return true if successful, false otherwise
 */
bool app_opt4048_init(void)
{
	LOG_INF("Initializing OPT4048 application");

	/* Initialize sensor hardware */
	if (!_app_opt4048_init_sensor()) {
		LOG_ERR("Failed to initialize OPT4048 sensor hardware");
		/* If init failed, no need to enable the timer to poll the sensor */
		return false;
	}

	/* set the timer to trigger measurement as 2s interval */
	k_timer_start(&als_timer, K_MSEC(2000), K_MSEC(2000));

	/* ACPI handler registration is done in app_acpi.c ACPI_planeInit() */

	return true;
}

/**
 * @brief Light sensor interrupt callback
 *
 * Called by GPIO interrupt handler when sensor interrupt pin asserts.
 * This function signals the thread to process sensor data.
 *
 * NOTE: This runs in interrupt context - keep it fast!
 */
void app_opt4048_interrupt_callback(void)
{
	/* Set interrupt pending flag */
	interrupt_pending = true;
	/* Signal semaphore to wake up thread */
	k_sem_give(&alsSem);
}

/**
 * @brief Light sensor trigger thread
 *
 * Called by task if it want to trigger light sensor thread run one cycle
 *
 * NOTE: This runs in interrupt context - keep it fast!
 */
void app_opt4048_giveSem(void)
{
	/* Signal semaphore to wake up thread */
	k_sem_give(&alsSem);
}

/**
 * @brief OPT4048 sensor thread
 *
 * Handles periodic sensor polling and updates based on system power state.
 *
 * @param p1 Thread run period (milliseconds)
 * @param p2 Thread sleep ready flag pointer
 * @param p3 Reserved
 */
void opt4048_thread(void *p1, void *p2, void *p3)
{
	uint32_t period = *(uint32_t *)p1;
	task_opt4048_slp_ready = (uint8_t *)p2;
	static enum system_power_state last_pwr_state = SYSTEM_INIT_STATE;
	enum system_power_state pwr_state;
	LOG_INF("OPT4048 thread started, poll period: %u ms", period);
	while (1) {
		/**
		* wait sem for available jobs
		*/
		k_sem_take(&alsSem, K_FOREVER);

		/* Not allow enter sleep */
		task_status_set(task_opt4048_slp_ready, TASK_STASTUS_SLEEP_READY, 0);

		/* Get current power state */
		pwr_state = app_pseq_systemState();
		/* Handle power state transitions */
		if (last_pwr_state != pwr_state) {
			LOG_DBG("Power state changed: %d -> %d", last_pwr_state, pwr_state);
			if (pwr_state == SYSTEM_S0_STATE) {
				/* Entering S0 - enable sensor */
				_app_opt4048_enable(true);
			} else {
				/* Leaving S0 - disable sensor to save power */
				_app_opt4048_enable(false);
			}
			last_pwr_state = pwr_state;
		}

		/* Only process ALS when sysetem in S0 and sensor enabled */
		if (pwr_state == SYSTEM_S0_STATE && sensor_enabled) {
			/* interupt fired */
			if (interrupt_pending) {
				interrupt_pending = false;
				/* Interrupt mode: just process existing data */
				LOG_DBG("Interrupt: processing existing data");
				_app_opt4048_process_data();

			} else {
				/* Normal polling mode when no interrupt pending */
				_app_opt4048_poll_sensor();
			}
		}
		task_status_set(task_opt4048_slp_ready, TASK_STASTUS_SLEEP_READY, 1);
	}
}

/**
 * @brief Get current lux value
 *
 * @return Current lux value
 */
uint32_t app_opt4048_get_lux(void)
{
	return current_lux;
}

/**
 * @brief Get current CCT value
 *
 * @return Current CCT in Kelvin
 */
double app_opt4048_get_cct(void)
{
	return current_cct;
}

/**
 * @brief Get current color data
 *
 * @param color Pointer to color structure to fill
 * @return 0 on success, -EINVAL if pointer is NULL
 */
int app_opt4048_get_color(opt4048_color_t *color)
{
	if (color == NULL) {
		return -EINVAL;
	}
	*color = current_color_data;
	return 0;
}

/**
 * @brief Check if sensor is initialized
 *
 * @return true if sensor is initialized, false otherwise
 */
bool app_opt4048_is_initialized(void)
{
	return sensor_initialized;
}

/**
 * @brief Check if sensor is enabled
 *
 * @return true if sensor is enabled, false otherwise
 */
bool app_opt4048_is_enabled(void)
{
	return sensor_enabled;
}