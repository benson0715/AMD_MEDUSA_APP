/*****************************************************************************
 * Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
 ******************************************************************************
 */
/**
 * @brief eSPI hub APIs.
 *
 */

#ifndef __ESPI_HUB_H__
#define __ESPI_HUB_H__

#include <zephyr/drivers/espi.h>
#include "system.h"

#if defined(CONFIG_SOC_SERIES_RTS5918) || defined(CONFIG_SOC_FAMILY_REALTEK_EC)
#include "realtek_espi.h"
#else
#include "nuvoton_espi.h"
#endif

#define ESPI_WAIT_TRUE            1u

#define WAIT_TIMEOUT_FOREVER      0xFFFFu

/* Minimum and maximum eSPI frequencies */
#define MIN_ESPI_FREQ             20u
#define MAX_ESPI_FREQ             66u

#define MAX_ACPI_HANDLERS         2u
#define MAX_PERIPH_HANDLERS       2u

/* TODO: Check if we can replace these macros */
#define E8042_ISR_DATA_POS	8U
#define E8042_ISR_CMD_DATA_POS	0U
#define KBC_IBF_DATA(x)           (((x) >> E8042_ISR_DATA_POS) & 0xFFU)
#define KBC_CMD_DATA(x)           ((x) & 0xFU)

/* TODO: Replace these macros with Zephyr byte order */
#define ESPI_PERIPHERAL_TYPE(x)   ((x) & 0x0000FFFF)
#define ESPI_PERIPHERAL_INDEX(x)  (((x) & 0xFFFF0000) >> 16)

/* Map port to OCB index */
#define ESPI_VW_SIGNAL_OCB_USBC_INDEX(x)	(ESPI_VWIRE_SIGNAL_OCB_0 + x)

typedef void (*espi_acpi_handler_t)(void);
typedef void (*espi_warn_handler_t)(uint8_t status);
typedef void (*espi_state_handler_t)(uint32_t signal, uint32_t status);
typedef void (*espi_kbc_handler_t)(uint8_t data, uint8_t status);
typedef void (*espi_postcode_handler_t)(uint8_t port_index, uint8_t code);


#define	ESPIHUB_VW_LOW	0
#define	ESPIHUB_VW_HIGH	1

enum espihub_handler {
	ESPIHUB_RESET_WARNING,
	ESPIHUB_PLATFORM_RESET,
	ESPIHUB_BUS_RESET,

	ESPIHUB_MAX_HANDLER_INDEX,
};

enum espihub_acpi_handler {
	ESPIHUB_ACPI_PUBLIC,
	ESPIHUB_ACPI_PRIVATE_0,
	ESPIHUB_ACPI_PRIVATE_1,
	ESPIHUB_ACPI_PRIVATE_2,
};

/**
 * @brief Helds context for all espi operations.
 *
 */
struct espihub_context {
	struct espi_callback espi_bus_cb;
	struct espi_callback vw_rdy_cb;
	struct espi_callback vw_cb;
	struct espi_callback p80_cb;
#if CONFIG_ESPI_PERIPHERAL_8042_KBC
	struct espi_callback kbc_cb;
#endif
#if CONFIG_ESPI_OOB_CHANNEL_RX_ASYNC
	struct espi_callback oob_cb;
#endif
	uint8_t espi_rst_sts;
	bool host_vw_ready;
	/* SPI flash sharing config detection */
	enum boot_config_mode spi_boot_mode;
	bool boot_config_detected;
};

/**
 * @brief Perform eSPI controllower initialization.
 *
 * @retval -EIO General input / output error, failed to configure device.
 * @retval -EINVAL invalid capabilities, failed to configure device.
 * @retval -ENOTSUP capability not supported by eSPI slave.
 * @retval 0 if success.
 */
int espihub_init(void);

/**
 * @brief Return boot configuration mode.
 *
 * @retval see enum boot_config_mode.
 */
enum boot_config_mode espihub_boot_mode(void);

/**
 * @brief Return current status of eSPI reset.
 *
 * @retval true if eSPI reset is de-asserted, false otherwise.
 */
bool espihub_reset_status(void);

/**
 * @brief Poll eSPI reset status until is asserted/de-asserted.
 *
 * @param exp_sts the expected status.
 * @param timeout value expressed in multiple of 100us.
 *
 * @retval -ETIMEDOUT or 0 if success.
 */
int espihub_wait_for_espi_reset(uint8_t exp_sts, uint16_t timeout);

/**
 * @brief Poll eSPI virtual wire until asserted/de-asserted.
 *
 * @param signal the virtual wire to monitor.
 * @param timeout value expressed in multiple of 100us.
 * @param exp_level the expected level of the virtual wire.
 * @param ack_required indicate if acknowledge when received.
 *
 * @retval -ETIMEDOUT or success.
 */
int espihub_wait_for_vwire(enum espi_vwire_signal signal, uint16_t timeout,
		   uint8_t exp_level, bool ack_required);

/**
 * @brief Poll signal while monitoring eSPI virtual wire.
 *
 * Note: This is used to detect glitches or when VW indicate abort.
 *
 * @param port_pin a EC GPIO. See @ec_gpio.h.
 * @param exp_sts the expected pin value.
 * @param timeout value expressed in multiple of 100us.
 * @param signal the virtual wire to monitor.
 * @param abort_sts the virtual wire status that indicates to abort the wait.
 *
 * @retval -ETIMEDOUT or 0 if success.
 */
int wait_for_pin_monitor_vwire(uint32_t port_pin, uint32_t exp_sts,
			       uint16_t timeout,
			       enum espi_vwire_signal signal,
			       uint8_t abort_sts);

/**
 * @brief Add a system state handler.
 * States are tracked via eSPI host virtual wire notifications for each
 * system transition.
 *
 * @param handler module handler called when system state is received.
 *
 * @retval -EINVAL if a handler already registered or 0 if success.
 */
int espihub_add_state_handler(espi_state_handler_t handler);

/**
 * @brief Detect SPI configuration boot mode.
 *
 * Uses eSPI flash channel status to distinguish between MAFS and SAF/G3
 * If channel is ready, this will indicate ROM bootloader booted in MAFS mode
 * and that eSPI channels negotiation was already performed.
 *
 * To distinguish SAF/G3 when supported HW strap is used.
 */
void detect_boot_mode(void);

/**
 * @brief Add a host warning handler.
 *
 * Host can send different warnings about system transitions or capabilities
 * temporarily unavailable.
 *
 * @param handler_type the type of the handler been registered.
 * @param handler module handler for host warnings received via eSPI bus.
 *
 * @retval -EINVAL if a handler already registered or 0 if success.
 */
int espihub_add_warn_handler(enum espihub_handler handler_type,
			     espi_warn_handler_t handler);

/**
 * @brief Add a host warning handler.
 *
 * Host can send different warnings about system transitions or capabilities
 * temporarily unavailable.
 *
 * @param handler_type the type of the handler been registered.
 * @param handler module handler for host warnings received via eSPI bus.
 *
 * @retval -EINVAL if a handler already registered or 0 if success.
 */
int espihub_add_acpi_handler(enum espihub_acpi_handler type,
			     espi_acpi_handler_t handler);

/**
 * @brief Add a keyboard subsystem handler.
 *
 * Host can send different traffic over designated ports for keyboards.
 *
 * @param handler module handler for host warnings received via eSPI bus.
 *
 * @retval -EINVAL if a handler already registered or  0 if success.
 */
int espihub_add_kbc_handler(espi_kbc_handler_t handler);

/**
 * @brief Add a post-code update handler.
 *
 * @param handler module handler for updating post-codes.
 *
 * @retval -EINVAL if a handler already registered or  0 if success.
 */
int espihub_add_postcode_handler(espi_postcode_handler_t handler);

/**
 * @brief Retrieve a virtual wire ensuring the eSPI channel is ready.
 *
 * @param signal the virtual wire to monitor.
 * @param level the current value of the virtual wire.
 *
 * @retval -EINVAL if eSPI channel is not ready or 0 if success.
 */
int espihub_retrieve_vw(enum espi_vwire_signal signal,
			uint8_t *level);

/**
 * @brief Send a virtual wire ensuring the eSPI channel is ready.
 *
 * @param signal the virtual wire to monitor.
 * @param level the desired value of the virtual wire.
 *
 * @retval -EINVAL if eSPI channel is not ready or 0 if success.
 */
int espihub_send_vw(enum espi_vwire_signal signal,
		    uint8_t level);

/**
 * @brief Retrieve an OOB packet ensuring the eSPI OOB channel is ready.
 *
 * @param pckt representation of OOB response.
 *
 * @retval -EINVAL if eSPI channel is not ready or 0 if success.
 */
int espihub_retrieve_oob(struct espi_oob_packet *pckt);

/**
 * @brief Send an OOB packet ensuring the eSPI OOB channel is ready.
 *
 * @param pckt representation of OOB request.
 *
 * @retval -EINVAL if eSPI channel is not ready or 0 if success.
 */
int espihub_send_oob(struct espi_oob_packet *pckt);

/**
 * @brief Perform keyboard write operation over eSPI.
 *
 * @param cmd to be sent to keyboard over the bus.
 * @param payload for the command for the keyboard.
 *
 * @retval 0 if success, error otherwise.
 */
int espihub_kbc_write(enum lpc_peripheral_opcode cmd, uint32_t payload);

/**
 * @brief Perform keyboard write operation over eSPI.
 *
 * @param cmd to be sent to keyboard over the bus.
 * @param data returned by the keyboard.
 *
 * @retval 0 if success, error otherwise.
 */
int espihub_kbc_read(enum lpc_peripheral_opcode cmd, uint32_t *data);

#ifdef ENABLE_ESPI_LTR
/**
 * @brief Send LTR packet after BME is enabled.
 *
 * @retval 0 if success, error otherwise.
 */
int espihub_send_ltr(void);
#endif

/**
 * @brief Sends a write request packet for shared flash.
 *
 * This routine provides an interface to send a request to write to the flash
 * components shared between the eSPI master and eSPI slaves.
 *
 * @param pckt Address of the representation of write flash transaction.
 *
 * @retval -ENOTSUP eSPI flash logical channel transactions not supported.
 * @retval -EBUSY eSPI flash channel is not ready or disabled by master.
 * @retval -EIO General input / output error, failed request to master.
 */
int espihub_write_flash(struct espi_flash_packet *pckt);

/**
 * @brief Sends a read request packet for shared flash.
 *
 * This routine provides an interface to send a request to read the flash
 * component shared between the eSPI master and eSPI slaves.
 *
 * @param pckt Adddress of the representation of read flash transaction.
 *
 * @retval -ENOTSUP eSPI flash logical channel transactions not supported.
 * @retval -EBUSY eSPI flash channel is not ready or disabled by master.
 * @retval -EIO General input / output error, failed request to master.
 */
int espihub_read_flash(struct espi_flash_packet *pckt);

/**
 * @brief Sends an erase request packet for shared flash.
 *
 * This routine provides an interface to send a request to erase a page in
 * the flash component shared between the eSPI master and eSPI slaves.
 *
 * @param pckt Adddress of the representation of erase flash transaction.
 *
 * @retval -ENOTSUP eSPI flash logical channel transactions not supported.
 * @retval -EBUSY eSPI flash channel is not ready or disabled by master.
 * @retval -EIO General input / output error, failed request to master.
 */
int espihub_erase_flash(struct espi_flash_packet *pckt);

/**
 * @brief return if system in the Soc low power state.
 *
 * This routine provides an interface to return Soc low power state
 *
 * @retval -true: system in Zx status
 */
bool espihub_socInLP(void);

/**
 * @brief Reset the low power state flag
 *
 * Reset low power state flag. Should be called in power on reset and system reset.
 *
 */
void espihub_onReset(void);

/**
 * @brief assert two vw after FA/FW Ready and PLTRST high
 *
 * assert ESPI_VWIRE_SIGNAL_HOST_RST_ACK and ESPI_VWIRE_SIGNAL_SLV_BOOT_DONE VW high
 *
 */
void espihub_onBothFaVwChReady(void);

#if CONFIG_UDC_MANAGEMENT
typedef uint8_t (*espi_get_udc_init_status_handler_t)(void);
/**
 * @brief Add handler for getting udc init status. 
 *
 * @param handler handler for getting udc init status.
 *
 * @retval true if success.
 */
int espihub_add_get_udc_init_status_handler(espi_get_udc_init_status_handler_t handler);

/**
 * @brief Set udc port mode. 
 *
 * @param status udc mode .
 *
 * @retval true if success.
 */
int espihub_set_udc_port(espi_udc_status status);

/**
 * @brief Empty mail box tx buffer. 
 *
 * @retval true if success.
 */
int espihub_mimic_tx_buffer_empty(void);

/**
 * @brief Set udc init status. 
 *
 * @param status udc mode.
 *
 * @retval true if success.
 */
int espihub_set_udc_init_status(espi_udc_status status);
#endif/* CONFIG_UDC_MANAGEMENT */

#if CONFIG_KSCAN_EC
typedef uint8_t (*espi_S0i3SelfRestore_handler_t)(void);
/**
 * @brief Add handler for s0i3 self restore. 
 *
 * @param handler handler for s0i3 self restore.
 *
 * @retval true if success.
 */
int espihub_add_S0i3SelfRestore_handler(espi_S0i3SelfRestore_handler_t handler);
#endif/* CONFIG_KSCAN_EC */

/**
 * @brief Get saf boot mode. 
 *
 * @retval true if saf boot mode.
 */
bool espihub_get_saf_boot_mode(void);

/**
 * @brief Set saf boot mode. 
 *
 * @param mode saf boot mode.
 */
void espihub_set_saf_boot_mode(bool mode);

#endif /* __ESPI_HUB_H__ */
