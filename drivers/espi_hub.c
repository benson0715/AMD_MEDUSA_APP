/*****************************************************************************
 * Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
 ******************************************************************************
 */

#include <zephyr/logging/log.h>
#include <zephyr/drivers/espi.h>
#include "espi_hub.h"
#include "board_config.h"
#include "espioob_mngr.h"
#include "board_config.h"
#include "acpi.h"
#if CONFIG_ESPI_SAF
#include "saf_config.h"
#endif
#if CONFIG_ECDBGI_SUPPORT
#include "ec_debug.h"
#endif
#include "f_nv_options.h"
#include "dbgLogFifo.h"
LOG_MODULE_REGISTER(espihub, CONFIG_ESPIHUB_LOG_LEVEL);

#if CONFIG_ENABLE_ESPI_LTR
/* LTR requirement enable */
#define ESPI_LTR_REQ_ENABLE		1U
/* LTR latency value in unit of scale */
#define ESPI_LTR_LATENCY		2U

static int espihub_send_ltr(void);
#endif

/* vw flags for ECRAM debug */
uint32_t g_u32_vw_SlpMscCounter = 0;
uint32_t g_u32_vw_SocLPCounter = 0;
uint32_t g_u32_vw_SMIOUTnCounter = 0;
uint32_t g_u32_vw_NMIOUTnCounter = 0;

/* true: indicates SoC in Low power state; It doesn't indicate S0i3 */
static bool _s_SocLowPowerFlag = false;

static const struct device *espi_dev;
static struct espihub_context hub;
static espi_warn_handler_t warn_handlers[ESPIHUB_MAX_HANDLER_INDEX];
static espi_state_handler_t state_handler;
static espi_acpi_handler_t acpi_handlers[MAX_ACPI_HANDLERS];
static espi_kbc_handler_t kbc_handler;
static espi_postcode_handler_t postcode_handler;
#if CONFIG_UDC_MANAGEMENT
static espi_get_udc_init_status_handler_t get_udc_init_status_handler;
#endif
#if CONFIG_KSCAN_EC
static espi_S0i3SelfRestore_handler_t S0i3SelfRestore_handler;
#endif /* CONFIG_KSCAN_EC */
extern uint8_t gs_eSpiMmioSpace[256];
static bool _s_SafBootMode = false;

/**
 * @brief Add state handler for eSPI hub
 *
 * @param handler State handler callback function
 * @return 0 on success, -EINVAL if handler already registered
 */
int espihub_add_state_handler(espi_state_handler_t handler)
{
	__ASSERT(handler, "Handler shouldn't be NULL");
	if (state_handler) {
		LOG_ERR("Only 1 state handler supported");
		return -EINVAL;
	}

	state_handler = handler;
	return 0;
}

/**
 * @brief Add warning handler for eSPI hub
 *
 * @param type Handler type index
 * @param handler Warning handler callback function
 * @return 0 on success, -EINVAL if handler already registered
 */
int espihub_add_warn_handler(enum espihub_handler type,
			      espi_warn_handler_t handler)
{
	__ASSERT(handler, "Handler shouldn't be NULL");
	if (warn_handlers[type]) {
		LOG_ERR("Only warn %d handler supported", type);
		return -EINVAL;
	}

	warn_handlers[type] = handler;
	return 0;
}

/**
 * @brief Reset the low power state flag
 *
 * Reset low power state flag. Should be called in power on reset and system reset.
 *
 */
void espihub_onReset(void)
{
	_s_SocLowPowerFlag = false;
}

/**
 * @brief Add ACPI handler for eSPI hub
 *
 * @param type ACPI handler type index
 * @param handler ACPI handler callback function
 * @return 0 on success, -EINVAL if handler already registered
 */
int espihub_add_acpi_handler(enum espihub_acpi_handler type,
			      espi_acpi_handler_t handler)
{
	__ASSERT(handler, "Handler shouldn't be NULL");
	if (acpi_handlers[type]) {
		LOG_ERR("Only 1 ACPI %d handler supported", type);
		return -EINVAL;
	}

	acpi_handlers[type] = handler;
	return 0;
}

/**
 * @brief Add KBC handler for eSPI hub
 *
 * @param handler KBC handler callback function
 * @return 0 on success, -EINVAL if handler already registered
 */
int espihub_add_kbc_handler(espi_kbc_handler_t handler)
{
	__ASSERT(handler, "Handler shouldn't be NULL");
	if (kbc_handler) {
		LOG_ERR("Only 1 KBC handler supported");
		return -EINVAL;
	}

	kbc_handler = handler;
	return 0;
}

/**
 * @brief Add postcode handler for eSPI hub
 *
 * @param handler Postcode handler callback function
 * @return 0 on success, -EINVAL if handler already registered
 */
int espihub_add_postcode_handler(espi_postcode_handler_t handler)
{
	__ASSERT(handler, "Handler shouldn't be NULL");
	if (postcode_handler) {
		LOG_ERR("Only 1 postcode handler supported");
		return -EINVAL;
	}

	postcode_handler = handler;
	return 0;
}

/* Status */
enum boot_config_mode espihub_boot_mode(void)
{
	return hub.spi_boot_mode;
}

bool espihub_reset_status(void)
{
	return hub.espi_rst_sts;
}

static void host_warn_handler(uint32_t signal, uint32_t status)
{
	LOG_DBG("%s", __func__);
	switch (signal) {
	case ESPI_VWIRE_SIGNAL_PLTRST:
		LOG_INF("PLT_RST changed %d", status);
		if (warn_handlers[ESPIHUB_PLATFORM_RESET]) {
			warn_handlers[ESPIHUB_PLATFORM_RESET](status);
		} else {
			LOG_WRN("No PLT_RST handler registered");
		}
		break;
	case ESPI_VWIRE_SIGNAL_HOST_RST_WARN:
		if (warn_handlers[ESPIHUB_RESET_WARNING]) {
			warn_handlers[ESPIHUB_RESET_WARNING](status);
		} else {
			LOG_WRN("No Host rst handler registered");
		}
		LOG_INF("Send ACK HOST RST %d", status);
		espi_send_vwire(espi_dev, ESPI_VWIRE_SIGNAL_HOST_RST_ACK,
				status);
		break;
	case ESPI_VWIRE_SIGNAL_OOB_RST_WARN:
		LOG_INF("Send OOB_RST_ACK %d", status);
		espi_send_vwire(espi_dev, ESPI_VWIRE_SIGNAL_OOB_RST_ACK,
				status);
		break;
	default:
		LOG_ERR("Host warning unhandled %d", signal);
		break;
	}
}

/* eSPI bus event handler */
static void espi_reset_handler(const struct device *dev,
			       struct espi_callback *cb,
			       struct espi_event event)
{
	LOG_WRN("%s", __func__);
	if (event.evt_type == ESPI_BUS_RESET) {
		hub.espi_rst_sts = event.evt_data;
		LOG_INF("eSPI BUS reset %d", event.evt_data);
		dbgLogFifo_printf("e_rst: %d\n", event.evt_data);
		if (warn_handlers[ESPIHUB_BUS_RESET]) {
			warn_handlers[ESPIHUB_BUS_RESET](event.evt_data);
		} else {
			LOG_WRN("No bus reset handler registered");
		}
	}
}

/* eSPI logical channels enable/disable event handler */
static void espi_ch_handler(const struct device *dev, struct espi_callback *cb,
			    struct espi_event event)
{
	LOG_DBG("%s", __func__);
	if (event.evt_type == ESPI_BUS_EVENT_CHANNEL_READY) {
		if (event.evt_details == ESPI_CHANNEL_VWIRE) {

#if CONFIG_KSCAN_EC
			S0i3SelfRestore_handler();
#endif /* CONFIG_KSCAN_EC */
			LOG_INF("VW channel ready: %d", event.evt_data);
			dbgLogFifo_printf("e_vw: %d\n", event.evt_data);
			hub.host_vw_ready = event.evt_data;
			/*
			 * send vw_SLAVE_BOOT_LOAD_STATUS and vw_SLAVE_BOOT_LOAD_DONE when
			 * both flash and VW channels are ready
			 */
			espihub_onBothFaVwChReady();
		}

#if CONFIG_ESPI_FLASH_CHANNEL
		if (event.evt_details == ESPI_CHANNEL_FLASH) {
			LOG_WRN("ESPI_SAF_DETECT %d", espihub_get_saf_boot_mode());

			if (espihub_get_saf_boot_mode()) {
					LOG_INF("Enable shared ROM mode: SAFS");
#if CONFIG_ESPI_SAF
					initialize_saf_bridge();
#endif
			} else {
					LOG_WRN("Enable shared ROM mode: MAF");
			}
			
			/* FC status change */
			if (event.evt_data & 0x02) {
				/* Flash channel is ready */
				if (event.evt_data & 0x01) {
					espihub_onBothFaVwChReady();
				} else {
					/* Flash channel is not ready */
				}
				dbgLogFifo_printf("e_fa: %d\n", (event.evt_data & 0x01));
			}
		}
#endif

#if CONFIG_ENABLE_ESPI_LTR
		if (event.evt_details == ESPI_CHANNEL_PERIPHERAL) {
			if (event.evt_data == ESPI_PC_EVT_BUS_MASTER_ENABLE) {
				LOG_DBG("Sending ltr.... ");
				int ltr_status = espihub_send_ltr();
				if (!ltr_status) {
					LOG_DBG("LTR_Send success.... ");
				} else {
					LOG_ERR("LTR_Send failed %d", ltr_status);
				}
			}
		}
#endif
	}
}

/* eSPI vwire received event handler */
static void vwire_handler(const struct device *dev, struct espi_callback *cb,
			  struct espi_event event)
{
	LOG_INF("VWire %d sts: %d", event.evt_details, event.evt_data);
	if (event.evt_type == ESPI_BUS_EVENT_VWIRE_RECEIVED) {
		switch (event.evt_details) {
		case ESPI_VWIRE_SIGNAL_PLTRST:
#if CONFIG_UDC_MANAGEMENT
			espihub_set_udc_port(get_udc_init_status_handler());
#endif
#if CONFIG_ECDBGI_ESPI_PC_BUS_MASTER_TEST
extern void npcx_host_init_shaw4_test_ram_base(uint32_t base);
	uint32_t nvOption;
	GET_NV_OPTIONS(espi_mmio_test_bar, nvOption);
	npcx_host_init_shaw4_test_ram_base(nvOption);
#endif /* CONFIG_ECDBGI_ESPI_PC_BUS_MASTER_TEST */
			host_warn_handler(event.evt_details, event.evt_data);
			break;
		case ESPI_VWIRE_SIGNAL_SLP_S3:
		case ESPI_VWIRE_SIGNAL_SLP_S4:
		case ESPI_VWIRE_SIGNAL_SLP_S5:
			LOG_INF("SLP %d changed %d", event.evt_details,
				event.evt_data);
			if (state_handler) {
				state_handler(event.evt_details,
					      event.evt_data);
			} else {
				LOG_WRN("No state handler registered");
			}
			break;
		case ESPI_VWIRE_SIGNAL_HOST_RST_WARN:
		case ESPI_VWIRE_SIGNAL_OOB_RST_WARN:
			host_warn_handler(event.evt_details, event.evt_data);
			break;
		case ESPI_VWIRE_SIGNAL_SMIOUT:
			LOG_DBG("VW_ISR vw_NMIOUTn %d", event.evt_data);
			g_u32_vw_SMIOUTnCounter++;
			break;
		case ESPI_VWIRE_SIGNAL_NMIOUT:
			LOG_DBG("VW_ISR vw_SMIOUTn %d", event.evt_data);
			g_u32_vw_NMIOUTnCounter++;
			break;
		case ESPI_VWIRE_SIGNAL_FL_ACK:
			break;
		case ESPI_VWIRE_SIGNAL_SLP_MSC:
			LOG_DBG("VW_ISR vw_SLP_MSC %d", event.evt_data);
			g_u32_vw_SlpMscCounter++;
			break;
		case ESPI_VWIRE_SIGNAL_SOC_LP:
			LOG_DBG("VW_ISR vw_soc_lp %d", event.evt_data);
			g_u32_vw_SocLPCounter++;
			/* update SOC low power flag in interrupt context */
			_s_SocLowPowerFlag = !!(event.evt_data);
			break;
		case ESPI_VWIRE_SIGNAL_SUS_STAT:
			LOG_DBG("VW_ISR vw_sus_stat %d", event.evt_data);
			break;
		default:
			LOG_WRN("Unhandled VWire %d", event.evt_details);
			break;
		}
	}
}

/* eSPI peripheral channel notifications handler */
static void periph_handler(const struct device *dev, struct espi_callback *cb,
			   struct espi_event event)
{
	uint8_t periph_type;
	uint8_t periph_index;

	periph_type = ESPI_PERIPHERAL_TYPE(event.evt_details);
	periph_index = ESPI_PERIPHERAL_INDEX(event.evt_details);

	switch (periph_type) {
	case ESPI_PERIPHERAL_DEBUG_PORT80:
		if (postcode_handler) {
			postcode_handler(periph_index, event.evt_data);
		} else {
			LOG_WRN("No postcode handler registered");
		}
		break;
	case ESPI_PERIPHERAL_HOST_IO:
		//printk("acpihandler\r\n");
		if (acpi_handlers[ESPIHUB_ACPI_PUBLIC]) {
			acpi_handlers[ESPIHUB_ACPI_PUBLIC]();
		} else {
			LOG_WRN("No ACPI handler registered");
		}
		break;
#if CONFIG_ESPI_PERIPHERAL_8042_KBC
	case ESPI_PERIPHERAL_8042_KBC:
		/* The second lowest byte contains the data and the lowest
		 * byte indicates if the information received was command
		 * or data
		 */
		if (kbc_handler) {
			kbc_handler(KBC_IBF_DATA(event.evt_data),
				    KBC_CMD_DATA(event.evt_data));
		} else {
			LOG_WRN("No KBC handler registered");
		}
		break;
#endif
#if defined(CONFIG_ESPI_PERIPHERAL_EC_HOST_CMD) || defined(CONFIG_ESPI_PERIPHERAL_HOST_IO_PVT)
#if defined(CONFIG_ESPI_PERIPHERAL_EC_HOST_CMD)
	case ESPI_PERIPHERAL_EC_HOST_CMD:
#endif /* CONFIG_ESPI_PERIPHERAL_EC_HOST_CMD */
	case ESPI_PERIPHERAL_HOST_IO_PVT:
		if (acpi_handlers[ESPIHUB_ACPI_PRIVATE_0]) {
			acpi_handlers[ESPIHUB_ACPI_PRIVATE_0]();
		} else {
			LOG_WRN("No PVT ACPI handler registered");
		}
		break;
#endif
	default:
		LOG_INF("%s periph 0x%x [%x]", __func__,
			periph_type, event.evt_data);
		break;
	}
}

#if CONFIG_ESPI_OOB_CHANNEL_RX_ASYNC
/* eSPI bus OOB event handler */
static void espi_oob_rx_handler(const struct device *dev,
			       struct espi_callback *cb,
			       struct espi_event event)
{
	if (event.evt_type == ESPI_BUS_EVENT_OOB_RECEIVED) {
		oob_rx_cb_handler(event.evt_details, event.evt_data);
	}
}
#endif

void detect_boot_mode(void)
{
	hub.spi_boot_mode = FLASH_BOOT_MODE_G3_SHARING;

	hub.boot_config_detected = true;
}

/**
 * @brief Initialize eSPI hub
 *
 * @return 0 on success, negative error code on failure
 */
int espihub_init(void)
{
	int ret;

	/* Indicate to eSPI master simplest configuration: Single line,
	 * 20MHz frequency and only logical channel 0 and 1 are supported
	 */
	struct espi_cfg cfg = {
#if CONFIG_POWER_SEQUENCE_MINIMUM_ESPI_CAPS
		.io_caps = ESPI_IO_MODE_SINGLE_LINE,
		.channel_caps = ESPI_CHANNEL_VWIRE | ESPI_CHANNEL_PERIPHERAL |
				ESPI_CHANNEL_OOB | ESPI_CHANNEL_FLASH,
		.max_freq = MIN_ESPI_FREQ,
#else
		.io_caps = ESPI_IO_MODE_SINGLE_LINE | ESPI_IO_MODE_DUAL_LINES |
					ESPI_IO_MODE_QUAD_LINES,
		.channel_caps = ESPI_CHANNEL_VWIRE | ESPI_CHANNEL_PERIPHERAL |
				ESPI_CHANNEL_OOB | ESPI_CHANNEL_FLASH,
		.max_freq = MAX_ESPI_FREQ,
#endif
	};

	espi_dev = DEVICE_DT_GET(ESPI_0);
	if (!device_is_ready(espi_dev)) {
		LOG_ERR("ESPI Device not ready");
		return -ENODEV;
	}

	LOG_DBG("About to configure eSPI device %s", __func__);
	ret = espi_config(espi_dev, &cfg);
	if (ret) {
		LOG_ERR("eSPI slave configured failed");
		return ret;
	}

	espi_init_callback(&hub.espi_bus_cb, espi_reset_handler,
			   ESPI_BUS_RESET);
	espi_init_callback(&hub.vw_rdy_cb, espi_ch_handler,
			   ESPI_BUS_EVENT_CHANNEL_READY);
	espi_init_callback(&hub.vw_cb, vwire_handler,
			   ESPI_BUS_EVENT_VWIRE_RECEIVED);
	espi_init_callback(&hub.p80_cb, periph_handler,
			   ESPI_BUS_PERIPHERAL_NOTIFICATION);

	espi_add_callback(espi_dev, &hub.espi_bus_cb);
	espi_add_callback(espi_dev, &hub.vw_rdy_cb);
	espi_add_callback(espi_dev, &hub.vw_cb);
	espi_add_callback(espi_dev, &hub.p80_cb);

#if CONFIG_ESPI_PERIPHERAL_8042_KBC

	espi_init_callback(&hub.kbc_cb, periph_handler,
				ESPI_BUS_PERIPHERAL_NOTIFICATION);
	espi_add_callback(espi_dev, &hub.kbc_cb);

#endif

#if CONFIG_ESPI_OOB_CHANNEL_RX_ASYNC
	espi_init_callback(&hub.oob_cb, espi_oob_rx_handler,
			   ESPI_BUS_EVENT_OOB_RECEIVED);
	espi_add_callback(espi_dev, &hub.oob_cb);
#endif

	hub.host_vw_ready = espi_get_channel_status(espi_dev,
						    ESPI_CHANNEL_VWIRE);
	LOG_DBG("%s hub.host_vw_ready: %d", __func__, hub.host_vw_ready);

#if CONFIG_ECDBGI_SUPPORT
	/* Init eSPI Validation application layer */
	espi_validation_init();
#endif

	return ret;
}

static int handle_vw_ack(enum espi_vwire_signal signal, uint8_t value)
{
	int ret;

	LOG_DBG("%s", __func__);
	switch (signal) {
	case ESPI_VWIRE_SIGNAL_SUS_WARN:
		ret = espi_send_vwire(espi_dev, ESPI_VWIRE_SIGNAL_SUS_ACK,
				      value);
		break;
	default:
		LOG_ERR("Ack for %d not supported", signal);
		return -EINVAL;
	}

	return ret;
}

/**
 * @brief Wait for virtual wire signal to reach expected level
 *
 * @param signal Virtual wire signal to wait for
 * @param timeout Timeout in 100us units
 * @param exp_level Expected signal level
 * @param ack_required Whether acknowledgment is required
 * @return 0 on success, -EIO on read failure, -ETIMEDOUT on timeout
 */
int espihub_wait_for_vwire(enum espi_vwire_signal signal, uint16_t timeout,
		   uint8_t exp_level, bool ack_required)
{
	int ret;
	uint8_t level;
	int loop_cnt = timeout;

	do {
		ret = espi_receive_vwire(espi_dev, signal, &level);
		if (ret) {
			LOG_ERR("Failed to read %x %d", signal, ret);
			return -EIO;
		}

		if (exp_level == level) {
			break;
		}

		k_usleep(100);
		loop_cnt--;
	} while (loop_cnt > 0 || (timeout == WAIT_TIMEOUT_FOREVER));

	if (loop_cnt == 0) {
		LOG_DBG("VWIRE %d is %x", signal, level);
		return -ETIMEDOUT;
	}

	if (ack_required) {
		handle_vw_ack(signal, level);
	}

	return 0;
}

/**
 * @brief Wait for eSPI reset to reach expected status
 *
 * @param exp_sts Expected reset status
 * @param timeout Timeout in 100us units
 * @return 0 on success, -ETIMEDOUT on timeout
 */
int espihub_wait_for_espi_reset(uint8_t exp_sts, uint16_t timeout)
{
	int loop_cnt = timeout;

	do {
		if (exp_sts == hub.espi_rst_sts) {
			break;
		}
		k_usleep(100);
		loop_cnt--;
	} while (loop_cnt > 0 || (timeout == WAIT_TIMEOUT_FOREVER));

	if (loop_cnt == 0) {
		return -ETIMEDOUT;
	}

	return 0;
}

/**
 * @brief Retrieve virtual wire signal level
 *
 * @param signal Virtual wire signal to retrieve
 * @param level Pointer to store signal level
 * @return 0 on success, negative error code on failure
 */
int espihub_retrieve_vw(enum espi_vwire_signal signal,
			uint8_t *level)
{
	if (!hub.host_vw_ready) {
		LOG_ERR("eSPI host not ready to receive VWs");
	}

	return espi_receive_vwire(espi_dev, signal, level);
}

/**
 * @brief Send virtual wire signal
 *
 * @param signal Virtual wire signal to send
 * @param level Signal level to send
 * @return 0 on success, -EINVAL if host not ready
 */
int espihub_send_vw(enum espi_vwire_signal signal, uint8_t level)
{
	/* Per SoC spec need to ensure that eSPI host is ready */
	if (!hub.host_vw_ready) {
		LOG_ERR("eSPI host not ready to receive VWs");
		return -EINVAL;
	}

	return espi_send_vwire(espi_dev, signal, level);
}

/**
 * @brief Retrieve OOB packet
 *
 * @param resp_pckt Pointer to OOB packet structure
 * @return 0 on success, negative error code on failure
 */
int espihub_retrieve_oob(struct espi_oob_packet *resp_pckt)
{
	return espi_receive_oob(espi_dev, resp_pckt);
}

/**
 * @brief Send OOB packet
 *
 * @param req_pckt Pointer to OOB packet structure
 * @return 0 on success, negative error code on failure
 */
int espihub_send_oob(struct espi_oob_packet *req_pckt)
{
	return espi_send_oob(espi_dev, req_pckt);
}

/**
 * @brief Write to KBC
 *
 * @param cmd LPC peripheral opcode
 * @param data Data to write
 * @return 0 on success, negative error code on failure
 */
int espihub_kbc_write(enum lpc_peripheral_opcode cmd, uint32_t data)
{
	uint32_t ldata = data;

	return espi_write_lpc_request(espi_dev, cmd, &ldata);
}

/**
 * @brief Read from KBC
 *
 * @param cmd LPC peripheral opcode
 * @param data Pointer to store read data
 * @return 0 on success, negative error code on failure
 */
int espihub_kbc_read(enum lpc_peripheral_opcode cmd, uint32_t *data)
{
	return espi_read_lpc_request(espi_dev, cmd, data);
}

#if CONFIG_ENABLE_ESPI_LTR
/**
 * @brief Send LTR (Latency Tolerance Reporting) message
 *
 * @return 0 on success, negative error code on failure
 */
int espihub_send_ltr(void)
{
	struct ltr_cfg_pkt req;

	req.ltr_req = ESPI_LTR_REQ_ENABLE;
	req.ltr_scale = ESPI_LTR_SCALE_1MSEC;
	req.latency = ESPI_LTR_LATENCY;

	return espi_send_ltr(espi_dev, &req);
}
#endif

/**
 * @brief Write to flash via eSPI
 *
 * @param pckt Pointer to flash packet structure
 * @return 0 on success, negative error code on failure
 */
int espihub_write_flash(struct espi_flash_packet *pckt)
{
	return espi_write_flash(espi_dev, pckt);
}

/**
 * @brief Read from flash via eSPI
 *
 * @param pckt Pointer to flash packet structure
 * @return 0 on success, negative error code on failure
 */
int espihub_read_flash(struct espi_flash_packet *pckt)
{
	return espi_read_flash(espi_dev, pckt);
}

/**
 * @brief Erase flash via eSPI
 *
 * @param pckt Pointer to flash packet structure
 * @return 0 on success, negative error code on failure
 */
int espihub_erase_flash(struct espi_flash_packet *pckt)
{
	return espi_flash_erase(espi_dev, pckt);
}

#if CONFIG_UDC_MANAGEMENT
/**
 * @brief Set UDC port status
 *
 * @param status UDC status to set
 * @return 0 on success, negative error code on failure
 */
int espihub_set_udc_port(espi_udc_status status)
{

	return espi_xec_set_udc_port(espi_dev, status);
}

/**
 * @brief Mimic TX buffer empty condition
 *
 * @return 0 on success, negative error code on failure
 */
int espihub_mimic_tx_buffer_empty(void)
{
	return espi_xec_mimic_tx_buffer_empty(espi_dev);
}

/**
 * @brief Add handler to get UDC initialization status
 *
 * @param handler Handler callback function
 * @return true on success
 */
int espihub_add_get_udc_init_status_handler(espi_get_udc_init_status_handler_t handler)
{
	get_udc_init_status_handler = handler;
	return true;
}
#endif /* CONFIG_UDC_MANAGEMENT */

#if CONFIG_KSCAN_EC
/**
 * @brief Add S0i3 self restore handler
 *
 * @param handler Handler callback function
 * @return true on success
 */
int espihub_add_S0i3SelfRestore_handler(espi_S0i3SelfRestore_handler_t handler)
{
	S0i3SelfRestore_handler = handler;
	return true;
}
#endif /* CONFIG_KSCAN_EC */

/**
 * @brief Check if SoC is in low power state
 *
 * @return true if SoC is in low power state, false otherwise
 */
bool espihub_socInLP(void)
{
	return _s_SocLowPowerFlag;
}

/**
 * @brief Handle both flash and VW channels ready event
 *
 * Sends boot status and boot done virtual wires when both channels are ready.
 */
void espihub_onBothFaVwChReady(void)
{
	/*
	 * Sent by EC or BMC upon completion of Slave Boot Load from the
	 * master attached flash
	 *
	 * 1 - The boot code load was successful and that the integrity of the
	 *     image is intact, or the boot code load from master attached flash
	 *     is not required.
	 */

	/*
	 * Sent when EC or BMC has completed its boot process as indication
	 * to eSPI master to continue with the G3 to S0 exit.
	 *
	 * 1 - Active high.
	 */
	espi_send_vwire(espi_dev, ESPI_VWIRE_SIGNAL_TARGET_BOOT_STS, 1);

	LOG_INF("Set vw_SLAVE_BOOT_LOAD_STATUS and vw_SLAVE_BOOT_LOAD_DONE to HIGH");
	dbgLogFifo_printf("e_vw_done\n");
}

/**
 * @brief Get SAF boot mode status
 *
 * @return true if SAF boot mode is enabled, false otherwise
 */
bool espihub_get_saf_boot_mode(void)
{
	return _s_SafBootMode;
}

/**
 * @brief Set SAF boot mode
 *
 * @param mode SAF boot mode to set
 */
void espihub_set_saf_boot_mode(bool mode)
{
	_s_SafBootMode = mode;
}