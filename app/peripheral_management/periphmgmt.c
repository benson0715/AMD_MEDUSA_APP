/**
 * Copyright (c) 2019 Intel Corporation
 * Modifications copyright (c) 2023 Advanced Micro Devices, Inc.
 */

#include <errno.h>
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <soc.h>
#include <zephyr/logging/log.h>
#include "periphmgmt.h"
#include "app_btn.h"
#include "board_config.h"
#include "smchost.h"
#include "board_init.h"
#include "board_periph.h"
#include "task_handler.h"

#if CONFIG_SLEEP
#include "board_sleep.h"
#endif

LOG_MODULE_DECLARE(periph, CONFIG_PERIPHERAL_LOG_LEVEL);

/* register sem for debounce */
K_SEM_DEFINE(gpio_debounce_lock, 0, 1);

/* ************************** *
 *     static valuable        *
 * ************************** */
static int debouncing_ongoing;

/* ************************** *
 *     global valuable        *
 * ************************** */
uint8_t g_isThrottleApuInDcOnly = 1;
uint8_t g_isThrottleApuInAcOnly = 0;
/* task slp ready */
uint8_t *task_periphSlpReady = NULL;
uint16_t g_InputBufferIoExp0_flag = 0;

/* ************************** *
 *     extern valuable        *
 * ************************** */
extern periph_gpio_callback gpio_cb_list[];
extern struct inputBuffer_info inputBuffer_lst[];
extern uint16_t gpio_cb_list_len, inputBuffer_lst_len;

/**
 * @brief trigger the peripheral thread sem
 */
void periph_setup_sem(void)
{
	task_status_set(task_periphSlpReady, TASK_STASTUS_SLEEP_READY, 0);

	k_sem_give(&gpio_debounce_lock);
}

/**
 * @brief register the previous native gpio callback to gpio interrupt vector
 *
 * @retval 0 if successful.
 */
int periph_register_gpio_cb_vector(void)
{
	struct inputBuffer_info *info;
	int ret = 0;
	uint8_t index = 0;

#if CONFIG_SOC_SERIES_NPCX4
	for (index = 0; index < gpio_cb_list_len; index++) {
		info = &inputBuffer_lst[index];

		ret = gpio_init_callback_pin(info->port_pin, &info->gpio_cb, gpio_cb_list[index]);
		if (ret) {
			LOG_ERR("Failed to init callback for %s", info->name);
			return ret;
		}

		ret = gpio_add_callback_pin(info->port_pin, &info->gpio_cb);
		if (ret) {
			LOG_ERR("Failed to add callback for %s", info->name);
			return ret;
		}

		if (gpio_get_power(info->port_pin) == GPIO_CTRL_PWRG_OFF) {
			ret = gpio_interrupt_configure_pin(info->port_pin, GPIO_INT_EDGE_BOTH);
			ret = gpio_set_power(info->port_pin, GPIO_CTRL_PWRG_OFF);

		} else {
			ret = gpio_interrupt_configure_pin(info->port_pin, GPIO_INT_EDGE_BOTH);
		}

		if (ret) {
			LOG_ERR("Failed to configure isr for %s", info->name);
		}
	}
#else
	for (index = 0; index < gpio_cb_list_len; index++) {
		info = &inputBuffer_lst[index];
		ret = gpio_init_callback_pin(info->port_pin, &info->gpio_cb, gpio_cb_list[index]);
		if (ret) {
			LOG_ERR("Failed to init callback for %s", info->name);
			return ret;
		}		
	}
	gpio_interrupt_configure_all();
#endif
	return ret;
}

/**
 * @brief Allow to register for gpio high/low events.
 *
 * @param port_pin the gpio port and pin identifier.
 * @param handler the function to be called when there is a button event.
 * @retval 0 if successful.
 */
int periph_register_debounce_handler(uint32_t port_pin, pin_handler_t handler)
{
	int i;
	int ret = 0;
	int level;

	for (i = 0; i < inputBuffer_lst_len; i++) {
		if (inputBuffer_lst[i].port_pin == port_pin) {
			break;
		}
	}

	if (i == inputBuffer_lst_len) {
		LOG_ERR("Unknown pin_idx:%d", i);
		return -EINVAL;
	}

	inputBuffer_lst[i].handler = handler;

	/* will use 3<<14 flag to detect if it is IOEXP.
	 * As native gpio high 8bites is from 0 to 6 (MEC1723/MEC1701), still can use it to separate native and ioexp
	 * But for other EC, need to double confirm */
	if (GPIO_isIoExpPin(port_pin)) {
#if CONFIG_IO_EXPANDER
		level = ioexp_get(inputBuffer_lst[i].port_pin);
#else
		level = 0;
		LOG_ERR("Disable IOEXP, but debounce IOEXP pins");
		return level;
#endif
	} else {
		/* Update pin level from gpio pin status */
		level = gpio_read_pin(inputBuffer_lst[i].port_pin);
	}

	if (level < 0) {
		LOG_ERR("Failed to read pin_idx %d", i);
		return level;
	}

	inputBuffer_lst[i].prev_level = level;

	return ret;
}

/**
 * @brief Call the debouncer handler after debounce
 *
 * @param pin_idx the gpio port index.
 */
static void notify_debounce_handlers(uint8_t pin_idx)
{
	int level;

	/* will use 3<<14 flag to detect if it is IOEXP.
	 * As native gpio high 8bites is from 0 to 6 (MEC1723/MEC1701), still can use it to separate native and ioexp
	 * But for other EC, need to double confirm */
	if (GPIO_isIoExpPin(inputBuffer_lst[pin_idx].port_pin)) {
#if CONFIG_IO_EXPANDER
		level = ioexp_get(inputBuffer_lst[pin_idx].port_pin);
#else
		level = 0;
		LOG_ERR("Disable IOEXP, but debounce IOEXP pins");
		return;
#endif
	} else {
		level = gpio_read_pin(inputBuffer_lst[pin_idx].port_pin);
	}

	if (level < 0) {
		LOG_ERR("Failed to read  pin_idx%d", pin_idx);
		return;
	}

	LOG_DBG("Pin[%s] valid change: %x", inputBuffer_lst[pin_idx].name, level);

	/* Ignore the button events if the debounce timings are not satisfied.
	 * This will address cases when input signal is noisy.
	 */
	if (inputBuffer_lst[pin_idx].handler) {
		if (inputBuffer_lst[pin_idx].prev_level != level) {
			LOG_DBG("calling handler");
			inputBuffer_lst[pin_idx].handler(level);
			inputBuffer_lst[pin_idx].prev_level = level;
		}
	}
}

/**
 * @brief ongoing the pin debounce
 */
static void debounce_pins(void)
{
	debouncing_ongoing = 0;

	for (int i = 0; i < inputBuffer_lst_len; i++) {
		if (inputBuffer_lst[i].debouncing) {
			inputBuffer_lst[i].deb_cnt--;

			if (inputBuffer_lst[i].deb_cnt == 0) {
				inputBuffer_lst[i].debouncing = false;
				/* set a default debounce cnt */
				inputBuffer_lst[i].deb_cnt = 0;
				notify_debounce_handlers(i);
			}
		}

		if (inputBuffer_lst[i].debouncing) {
			debouncing_ongoing++;
		}
	}
}

/**
 * @brief if debounce mechanism is ongoing
 *
 * @retval True if during debounce.
 */
bool is_debouncing(void)
{
	return (debouncing_ongoing > 0);
}

/**
 * @brief Handles the power button and determine the next power state.
 *
 * This routines doesn't perform debouncing but tracks short and long press
 * cases. It also sends notifications to Host.
 *
 * If the power button is down for the de-bounce period then set
 * flags to transition to a new power state. If it stays down
 * for 4 seconds power off the system.
 *
 * @param p1 pointer to additional task-specific data.
 * @param p2 pointer to additional task-specific data.
 * @param p3 pointer to additional task-specific data.
 *
 */
void periph_thread(void *p1, void *p2, void *p3)
{
	uint32_t period = *(uint32_t *)p1;
	int ret;

	task_periphSlpReady = (uint8_t *)p2;

	/* Can enter sleep before first circle */
	task_status_set(task_periphSlpReady, TASK_STASTUS_SLEEP_READY, 1);

	ret = board_periph_init();
	if (ret) {
		LOG_ERR("Periph init failed");
	}

	ret = app_btn_init();
	if (ret) {
		LOG_ERR("power button init failed");
	}

	while (true) {
		/* Wait until ISR occurs to start debouncing */
		k_sem_take(&gpio_debounce_lock, K_FOREVER);

		if (task_periphSlpReady != NULL)
			*task_periphSlpReady &= ~BIT(0);
		#if 0 //RTK_TODO
			/* Handle IO expender */
			if (g_InputBufferIoExp0_flag) {
				board_periph_IoExp_IntDispatcher();
			}
		#endif

		do {
			/* Perform debounce for all buttons */
			k_msleep(period);
			debounce_pins();
		} while (is_debouncing());

		task_status_set(task_periphSlpReady, TASK_STASTUS_SLEEP_READY, 1);
	}
}