/**
 * Copyright (c) 2019 Intel Corporation
 * Modifications copyright (c) 2023 Advanced Micro Devices, Inc.
 */
#include <errno.h>
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <soc.h>
#include <zephyr/logging/log.h>
#include "sci.h"
#include "smc.h"
#include "smchost.h"
#include "app_pseq.h"
#include "espi_hub.h"
#include "acpi.h"
#include "gpio_ec.h"
#include "board_config.h"
#include "app_acpi.h"
LOG_MODULE_REGISTER(sci, CONFIG_SMCHOST_LOG_LEVEL);

struct acpi_state_flags g_acpi_state_flags;

K_MSGQ_DEFINE(sci_msgq, sizeof(uint8_t), SCIQ_SIZE, sizeof(uint8_t));

/**
 * @brief  Initialize the SCI Queue.
 */
void sci_queue_init(void)
{
	g_acpi_state_flags.sci_enabled = 1;
}

/**
 * @brief Flushes all pending SCI notifications.
 *
 * Note: This is particular relevant in eSPI-based platforms where no SCI
 * notifications should be sent after eSPI host indicates warning about ongoing
 * system reset.
 */
void sci_queue_flush(void)
{
	LOG_DBG("%s %d SCI flushed", __func__, k_msgq_num_used_get(&sci_msgq));
	k_msgq_purge(&sci_msgq);
}

/**
 * @brief Generate a System Control Interrupt pulse.
 *
 * System control interrupt are used to notify OS of ACPI events,
 * Do not send SCI when not in acpi mode or system is in Sx.
 * SCI is a pulse so need to send a eSPI virtual wire packet with zero then
 * then another eSPI VW packet with one.
 * eSPI driver should guarantee both virtual are transmitted, a delay between
 * packets should never be added here.
 */
void generate_sci(void)
{
#if CONFIG_SMCHOST_SCI_OVER_ESPI
	int ret;
#endif
		int key = irq_lock();

#if CONFIG_SMCHOST_SCI_OVER_ESPI
		ret = espihub_send_vw(ESPI_VWIRE_SIGNAL_SCI, ESPIHUB_VW_LOW);
		if (ret) {
			LOG_WRN("SCI failed");
		}
		k_busy_wait(100);

		ret = espihub_send_vw(ESPI_VWIRE_SIGNAL_SCI, ESPIHUB_VW_HIGH);
		if (ret) {
			LOG_WRN("SCI failed");
		}
#else
	#if CONFIG_SOC_FAMILY_MEC
		gpio_write_pin(EC_APU_SCI, 1);
		k_busy_wait(6);	//6us
		gpio_write_pin(EC_APU_SCI_N, 0);
	#else
		gpio_write_pin(EC_APU_SCI_N, 0);
		k_busy_wait(6);	//6us
		gpio_write_pin(EC_APU_SCI_N, 1);
	#endif
#endif
		irq_unlock(key);
}

/**
 * @brief Generates an SCI event if there are any pending in the SCI queue.
 *
 */
void check_sci_queue(void)
{

	if (k_msgq_num_used_get(&sci_msgq) == 0) {
		acpi_set_flag(ACPI_EC_0, ACPI_FLAG_SCIEVENT, 0);
	} else {
		LOG_DBG("SCI pending");
		acpi_set_flag(ACPI_EC_0, ACPI_FLAG_SCIEVENT, 1);
		generate_sci();
	}
}

/**
 * @brief Indicates if there are any SCI pending that can be sent.
 *
 * @return true if SCI are pending and can be sent.
 */
bool sci_pending(void)
{
	return ((((g_acpi_state_flags.sci_enabled &&
		g_acpi_state_flags.acpi_mode))||((g_ui8MondernStandbySupport && 
		(app_pseq_systemState() == SYSTEM_S0_R_STATE|| app_pseq_systemState() == SYSTEM_S3_STATE)))) &&
		k_msgq_num_used_get(&sci_msgq) > 0);
}

/**
 * @brief Sends any pending notifications to the operating system.
 *
 * Sends pending notifications to OS handler in response to a query command.
 */
void send_sci_events(void)
{
	int ret;
	uint8_t evt_byte = 0;

	if (!g_acpi_state_flags.acpi_mode) {
		return;
	}

	if (acpi_get_flag(ACPI_EC_0, ACPI_FLAG_OBF)) {
		LOG_WRN("No SCI sent. Some other transaction in progress");
		return;
	}

	ret = k_msgq_get(&sci_msgq, &evt_byte, K_NO_WAIT);
	if (ret < 0) {
		switch (ret) {
		case -ENOMSG:
			LOG_DBG("SCI msgq Empty!");
			break;
		default:
			LOG_ERR("SCI Queue Fetch failure %02x", ret);
			return;
		}
	}

	if (k_msgq_num_used_get(&sci_msgq) == 0) {
		acpi_set_flag(ACPI_EC_0, ACPI_FLAG_SCIEVENT, 0);
	} else {
		acpi_set_flag(ACPI_EC_0, ACPI_FLAG_SCIEVENT, 1);
	}

	acpi_write_odr(ACPI_EC_0, evt_byte);
	if (ret == -ENOMSG) {
		acpi_set_flag(ACPI_EC_0, ACPI_FLAG_SCIEVENT, 0);
	}
	LOG_INF("Sent SCI %02x", evt_byte);
	generate_sci();
}

/**
 * @brief Stores data to send to the operating system in the SCI queue.
 *
 * @param code the byte to push onto queue.
 */
void enqueue_sci(uint8_t code)
{
	int ret;

	LOG_INF("enqueued SCI %02x", code);
	ret = k_msgq_put(&sci_msgq, (void *)&code, K_NO_WAIT);
	if (ret < 0) {
		LOG_ERR("SCI msgq Put failure %02x", ret);
	}

	smchost_signal_request();
}

/**
 * @brief Check if system is in ACPI mode or not.
 *
 * @return 1 - system is in ACPI mode, else 0.
 */
inline bool is_system_in_acpi_mode(void)
{
	return g_acpi_state_flags.acpi_mode;
}