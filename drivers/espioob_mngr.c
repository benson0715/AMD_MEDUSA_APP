/**
* Copyright (c) 2019 Intel Corporation
* Modifications copyright (c) 2023 Advanced Micro Devices, Inc.
*/

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/espi.h>
#include "espi_hub.h"
#include "espioob_mngr.h"
#include "memops.h"
#include "amd_crb_drivers_spiFlash.h"

#if CONFIG_OOB_MSG_MNGR
#include "oob_msg_mngr.h"
#endif

LOG_MODULE_REGISTER(oobmngr, CONFIG_ESPIOOB_MNGR_LOG_LEVEL);

#define ASYNC_MSGQ_MAX_MSGS		(8U)
#define ASYNC_MSGQ_ALIGNMENT    (4U)

#if CONFIG_OOB_MSG_MNGR
#define OOB_MSG_LEN_FROM_BYTE_CNT(x)	((x + OOB_IDX_BYTE_CNT + 1) + 1) // + PEC code
#else
#define OOB_MSG_LEN_FROM_BYTE_CNT(x)	(x + OOB_IDX_BYTE_CNT + 1)
#endif

static uint8_t rx_buf[MAX_OOB_BUF_SIZE];

struct oob_msg {
	struct espi_oob_packet *tx;
	struct espi_oob_packet *rx;
	struct k_mutex txn_lock;
	struct k_sem txn_sync;
};

static struct oob_msg master_fch;

static oob_rx_callback_handler_t fch_msg_hndlr;

static uint8_t oob_rx_tag = 0;
static uint16_t oob_rx_length = 0;
extern void hook_espi_oob_test_buffer(uint8_t tag, uint16_t length, struct espi_oob_packet *rx);

struct async_msb {
	uint8_t buf[MAX_OOB_BUF_SIZE];
	uint16_t len;
	uint8_t from;
	uint8_t tag;
	oob_rx_callback_handler_t fn;
};

K_MSGQ_DEFINE(async_msgq, sizeof(struct async_msb), ASYNC_MSGQ_MAX_MSGS,
	ASYNC_MSGQ_ALIGNMENT);

/**
 * @brief Register OOB handler for a specific master address
 *
 * @param master_addr Master address to register handler for
 * @param fn Callback function to handle OOB messages
 */
void register_oob_hndlr(uint8_t master_addr, oob_rx_callback_handler_t fn)
{
	switch (master_addr) {
	case OOB_MASTER_ADDR_FCH:
		fch_msg_hndlr = fn;
		break;
	default:
		LOG_ERR("No handler support for master address");
	}
}

/**
 * @brief Verify OOB transmit packet validity
 *
 * @param tx Pointer to OOB packet to verify
 * @return 0 on success, negative error code on failure
 */
static int verify_oob_tx_pckt(struct espi_oob_packet *tx)
{
	uint8_t len = OOB_MSG_LEN_FROM_BYTE_CNT(tx->buf[OOB_IDX_BYTE_CNT]);

	if ((len >= MAX_OOB_BUF_SIZE) || (len < OOB_IDX_HDR_SIZE)) {
		LOG_ERR("TX Invalid byte count %d", len);
		return -EINVAL;
	}

	if (len != tx->len) {
		LOG_ERR("Tx len %d & byte count %d mis-match", tx->len, len);
		return -EINVAL;
	}

	switch (tx->buf[OOB_IDX_DEST_SLV_ADDR]) {
	case OOB_DST_ADDR(OOB_MASTER_ADDR_FCH):
		break;
	default:
		LOG_ERR("Invalid Dest slave addr: %x",
			tx->buf[OOB_IDX_DEST_SLV_ADDR]);
		return -EINVAL;
	}

	if (tx->buf[OOB_IDX_SRC_SLV_ADDR] !=
		OOB_SRC_ADDR(OOB_SLAVE_ADDR_EC)) {
		LOG_INF("Invalid Src slave addr. Use %x", OOB_SLAVE_ADDR_EC);
		/* No need to return as error */
	}

	return 0;
}

/**
 * @brief Verify OOB receive packet validity
 *
 * @param rx Pointer to OOB packet to verify
 * @return 0 on success, negative error code on failure
 */
static int verify_oob_rx_pckt(struct espi_oob_packet *rx)
{
	uint8_t len = OOB_MSG_LEN_FROM_BYTE_CNT(rx->buf[OOB_IDX_BYTE_CNT]);

	if ((len >= MAX_OOB_BUF_SIZE) || (len < OOB_IDX_HDR_SIZE)) {
		LOG_ERR("RX Invalid byte count %d", len);
		return -EINVAL;
	}

	LOG_HEXDUMP_DBG(rx->buf, rx->len, "oob_rx:");
	if (len != rx->len) {
		LOG_ERR("rx len & byte count mis-match %d %d", len, rx->len);
		return -EINVAL;
	}

	if (rx->buf[OOB_IDX_DEST_SLV_ADDR] !=
		OOB_DST_ADDR(OOB_SLAVE_ADDR_EC)) {
		/*
		 * Commented out due to PMC bug - PMC sends invalid addr.
		LOG_ERR("Invalid Dest slave addr in OOB Rx");
		return -EINVAL;
		*/
	}

	switch (rx->buf[OOB_IDX_SRC_SLV_ADDR]) {
	case OOB_SRC_ADDR(OOB_MASTER_ADDR_FCH):
		break;
	default:
		LOG_ERR("Invalid Src slave addr %x in OOB Rx",
			rx->buf[OOB_IDX_SRC_SLV_ADDR]);
		return -EINVAL;
	}

	return 0;
}

/**
 * @brief Get OOB master structure from address byte
 *
 * @param addr_byte Address byte containing master address
 * @return Pointer to oob_msg structure for the master
 */
static inline struct oob_msg *get_oob_master(uint8_t addr_byte)
{
	/* Decode 7bit master address from 8bit address value */
	uint8_t master_addr = OOB_7BIT_ADDR(addr_byte);

	switch (master_addr) {
	case OOB_MASTER_ADDR_FCH:
		return &master_fch;
	default:
		return &master_fch;
	}
}

/**
 * @brief Send OOB message synchronously and wait for response
 *
 * @param req Pointer to request OOB packet
 * @param resp Pointer to response OOB packet buffer
 * @param timeout Timeout in milliseconds
 * @return 0 on success, negative error code on failure
 */
int oob_send_sync(struct espi_oob_packet *req, struct espi_oob_packet *resp,
		  int timeout)
{
	int ret = 0;
	struct oob_msg *master;
	int wait_time = MAX(MIN(timeout, MAX_WAIT_TIME_FOR_OOB_IN_MS),
		MIN_WAIT_TIME_FOR_OOB_IN_MS);

#ifndef CONFIG_OOBMNGR_SUPPORT
	return -ENOTSUP;
#endif

	if ((req == NULL) || (resp == NULL)) {
		return -ENODATA;
	}

	ret = verify_oob_tx_pckt(req);
	if (ret) {
		LOG_ERR("OOB Tx packet verification failed %d", ret);
		return ret;
	}

	master = get_oob_master(req->buf[OOB_IDX_DEST_SLV_ADDR]);

	if (master == NULL) {
		return -EINVAL;
	}

	if (k_mutex_lock(&master->txn_lock,
			 K_MSEC(MIN_WAIT_TIME_FOR_OOB_IN_MS))) {
		LOG_ERR("OOB tx lock timeout");
		return -EBUSY;
	}

	master->tx = req;
	master->rx = resp;
	k_sem_reset(&master->txn_sync);

	ret = espihub_send_oob(master->tx);
	if (ret) {
		LOG_ERR("Error sending OOB %d", ret);
		k_mutex_unlock(&master->txn_lock);
		k_sem_give(&master->txn_sync);
		return -EIO;
	}

	LOG_DBG("OOB Tx Successful");

	/* Wait till OOB response, txn_sync semaphore released by rx handler */
	ret = k_sem_take(&master->txn_sync, K_MSEC(wait_time));

	if (ret) {
		LOG_ERR("OOB Rx sem timeout");
		ret = -ETIMEDOUT;
	} else {
		if (master->rx->len) {
			LOG_DBG("OOB Rx Successful");
		} else {
			LOG_ERR("OOB Rx received, but buffer space not enough");
			ret = -ENOBUFS;
		}
	}

	k_mutex_unlock(&master->txn_lock);
	k_sem_give(&master->txn_sync);

	return ret;
}

/**
 * @brief Send OOB test message synchronously and wait for response
 *
 * @param req Pointer to request OOB packet
 * @param resp Pointer to response OOB packet buffer
 * @param timeout Timeout in milliseconds
 * @return 0 on success, negative error code on failure
 */
int oob_test_send_sync(struct espi_oob_packet *req, struct espi_oob_packet *resp,
		  int timeout)
{
	int ret = 0;
	struct oob_msg *master;
	int wait_time = MAX(MIN(timeout, MAX_WAIT_TIME_FOR_OOB_IN_MS),
		MIN_WAIT_TIME_FOR_OOB_IN_MS);

#ifndef CONFIG_OOBMNGR_SUPPORT
	return -ENOTSUP;
#endif

	if ((req == NULL) || (resp == NULL)) {
		return -ENODATA;
	}

	master = get_oob_master(req->buf[OOB_IDX_DEST_SLV_ADDR]);

	if (master == NULL) {
		return -EINVAL;
	}
	if (k_mutex_lock(&master->txn_lock,
			 K_MSEC(MIN_WAIT_TIME_FOR_OOB_IN_MS))) {
		LOG_ERR("OOB tx lock timeout");
		return -EBUSY;
	}

	master->tx = req;
	master->rx = resp;
	k_sem_reset(&master->txn_sync);

	ret = espihub_send_oob(master->tx);
	if (ret) {
		LOG_ERR("Error sending OOB %d", ret);
		k_mutex_unlock(&master->txn_lock);
		k_sem_give(&master->txn_sync);
		return -EIO;
	}

	LOG_DBG("OOB Test Tx Successful");

	/* Wait till OOB response, txn_sync semaphore released by rx handler */
	ret = k_sem_take(&master->txn_sync, K_MSEC(wait_time));

	if (ret) {
		LOG_ERR("OOB Test Rx sem timeout");
		ret = -ETIMEDOUT;
	} else {
		if (master->rx->len) {
			LOG_DBG("OOB Test Rx Successful");
		} else {
			LOG_ERR("OOB Test Rx received, but buffer space not enough");
			ret = -ENOBUFS;
		}
	}

	k_mutex_unlock(&master->txn_lock);
	k_sem_give(&master->txn_sync);

	return ret;
}

/**
 * @brief Send OOB message asynchronously
 *
 * @param req Pointer to request OOB packet
 * @param cb Callback function to invoke on response
 * @return 0 on success, negative error code on failure
 */
int oob_send_async(struct espi_oob_packet *req, oob_rx_callback_handler_t cb)
{
	int ret;
	struct async_msb msg;

#ifndef CONFIG_OOBMNGR_SUPPORT
	return -ENOTSUP;
#endif

	if (req == NULL) {
		return -ENODATA;
	}

	ret = verify_oob_tx_pckt(req);
	if (ret) {
		LOG_ERR("OOB Tx packet verification failed %d", ret);
		return ret;
	}

	msg.len = req->len;
	memcpys(msg.buf, req->buf, req->len);
	msg.fn = cb;
	msg.from = OOB_SLAVE_ADDR_EC;

	ret = k_msgq_put(&async_msgq, &msg, K_NO_WAIT);
	if (ret) {
		LOG_ERR("Async msg request enque failed %d", ret);
		return -ENOBUFS;
	}

	return 0;
}

/**
 * @brief Send OOB test message asynchronously
 *
 * @param req Pointer to request OOB packet
 * @param cb Callback function to invoke on response
 * @return 0 on success, negative error code on failure
 */
int oob_test_send_async(struct espi_oob_packet *req, oob_rx_callback_handler_t cb)
{
	int ret;
	struct async_msb msg;

#ifndef CONFIG_OOBMNGR_SUPPORT
	return -ENOTSUP;
#endif

	if (req == NULL) {
		return -ENODATA;
	}
	msg.len = req->len;
	memcpys(msg.buf, req->buf, req->len);
	msg.tag = req->tag;
	msg.fn = cb;
	msg.from = OOB_SLAVE_ADDR_EC;

	ret = k_msgq_put(&async_msgq, &msg, K_NO_WAIT);
	if (ret) {
		LOG_ERR("Test Async msg request enque failed %d", ret);
		return -ENOBUFS;
	}

	return 0;
}

/**
 * @brief Send OOB response to master
 *
 * @param tx Pointer to OOB packet to send
 * @return 0 on success, negative error code on failure
 */
int oob_respond_master(struct espi_oob_packet *tx)
{
	int ret;
	struct oob_msg *master;

#ifndef CONFIG_OOBMNGR_SUPPORT
	return -ENOTSUP;
#endif

	if (tx == NULL) {
		return -ENODATA;
	}

	/*
	ret = verify_oob_tx_pckt(tx);
	if (ret) {
		LOG_ERR("OOB Tx packet verification failed %d", ret);
		return ret;
	}
	*/
	master = get_oob_master(tx->buf[OOB_IDX_DEST_SLV_ADDR]);

	if (master == NULL) {
		return -EINVAL;
	}

	if (k_mutex_lock(&master->txn_lock, K_NO_WAIT)) {
		LOG_ERR("OOB tx lock timeout");
		return -EBUSY;
	}

	master->tx = tx;

	ret = espihub_send_oob(master->tx);
	if (ret) {
		LOG_ERR("Error sending OOB %d", ret);
		ret = -EIO;
	} else {
		LOG_DBG("OOB Tx Successful");
	}

	k_mutex_unlock(&master->txn_lock);
	return ret;
}

/**
 * @brief Legacy handler for FCH OOB receive messages
 *
 * @param rx Pointer to received OOB packet
 * @param err Error code from receive operation
 */
void espi_fch_oob_rx_legacy_handler(struct espi_oob_packet *rx, int err)
{
	int ret;
	struct oob_msg *master;

	LOG_DBG("%s", __func__);

	if (rx == NULL) {
		return;
	}

	master = get_oob_master(rx->buf[OOB_IDX_DEST_SLV_ADDR]);

	if (master == NULL) {
		LOG_ERR("Msg from Unknown master - Discard");
		return;
	}

	if (k_mutex_lock(&master->txn_lock, K_NO_WAIT)) {
		LOG_ERR("OOB tx lock timeout");
		return;
	}

	// rx->buf[0] = SMBus Target Address
	// rx->buf[1] = SMBus Command
	// rx->buf[2] = SMBus Byte Count
	// rx->buf[3] = SMBus Data Byte 0
	// rx->buf[4] = SMBus Data Byte 1
	
	hook_espi_oob_test_buffer(oob_rx_tag, oob_rx_length, rx);

	master->tx = rx;
	ret = espihub_send_oob(master->tx);
	if (ret) {
		LOG_ERR("Error sending OOB %d", ret);
	} else {
		LOG_DBG("OOB Tx Successful");
	}

	k_mutex_unlock(&master->txn_lock);
	return;
}

#if CONFIG_OOB_MSG_MNGR
static uint8_t tx_buf[MAX_OOB_BUF_SIZE];

/**
 * @brief Handler for FCH OOB receive messages with message manager
 *
 * @param rx Pointer to received OOB packet
 * @param err Error code from receive operation
 */
void espi_fch_oob_rx_handler(struct espi_oob_packet *rx, int err)
{
	int ret;
	struct oob_msg *master;
	struct espi_oob_packet tx = {
		.buf = tx_buf,
		.len = sizeof(tx_buf)
	};

	LOG_DBG("%s", __func__);

	if (rx == NULL) {
		return;
	}

	master = get_oob_master(rx->buf[OOB_IDX_DEST_SLV_ADDR]);

	if (master == NULL) {
		LOG_ERR("Msg from Unknown master - Discard");
		return;
	}

	if (k_mutex_lock(&master->txn_lock, K_NO_WAIT)) {
		LOG_ERR("OOB tx lock timeout");
		return;
	}

	ret = oob_msg_mngr_proc(rx->buf, rx->len, tx.buf, &tx.len);
	if (ret) {
		LOG_ERR("Error occurs in oob message manager process %d", ret);
		goto unlock_and_return;
	}

	if (!tx.len) {
		LOG_WRN("Received a broken-up message");
		goto unlock_and_return;
	}

	// Send initial message
	tx.tag = master->rx->tag;
	master->tx = &tx;
	LOG_HEXDUMP_DBG(tx.buf, tx.len, "oob_tx:");
	ret = espihub_send_oob(master->tx);
	if (ret) {
		LOG_ERR("Error sending OOB %d", ret);
		goto unlock_and_return;
	}
	LOG_DBG("OOB Tx Successful");

	// Send remaining fragments
	while (1) {
		ret = oob_msg_mngr_proc_remainning(tx.buf, &tx.len);
		if (ret) {
			LOG_ERR("Error processing OOB remaining %d", ret);
			break;
		}

		if (!tx.len) {
			break;
		}

		LOG_HEXDUMP_DBG(tx.buf, tx.len, "oob_tx_remaining:");
		ret = espihub_send_oob(master->tx);
		if (ret) {
			LOG_ERR("Error sending OOB remaining %d", ret);
			break;
		}
		LOG_DBG("OOB Tx remaining Successful");
	}

	oob_msg_mngr_postpne_proc();

unlock_and_return:

	k_mutex_unlock(&master->txn_lock);
	return;
}
#endif

/**
 * @brief Internal OOB receive handler - intended to be handled as in ISR
 *
 * @param rx Pointer to received OOB packet
 * @param evt_details Event details from eSPI
 * @param evt_data Event data from eSPI
 */
static void oob_rx_handler(struct espi_oob_packet *rx, uint32_t evt_details, uint32_t evt_data)
{
	int ret;
	struct oob_msg *master;
	struct async_msb msg;

	/* Validate OOB message */
	ret = verify_oob_rx_pckt(rx);
#if CONFIG_OOB_MSG_MNGR
	if (ret) {
		LOG_ERR("Invalid Rx packet");
		return;
	}
#endif

	/* Find the OOB Rx master */
	master = get_oob_master(rx->buf[OOB_IDX_SRC_SLV_ADDR]);

	if (master == NULL) {
		LOG_ERR("Msg from Unknown master - Discard");

		/* Clear Rx buffer for next downstream oob */
		memsets(rx->buf, 0, rx->len);
		rx->len = sizeof(rx_buf);
		return;
	}

	/*
	 * Route the OOB Rx to appropriate Rx buffer.
	 * If OOB rx received when semaphore not acquired by EC i.e. sem_count
	 * is a non-zero value, then the downstream OOB message is treated as
	 * master initiated OOB request.
	 * If OOB rx received when semaphore is already acquired by EC for the
	 * same master i.e. sem_count = 0, then the downstream OOB message is
	 * tied as a response to the EC initiated OOB request message. Semaphore
	 * must be released then for the waiting task to catch the response.
	 */
	if (k_sem_count_get(&master->txn_sync)) {
		/*
		 * Incoming messages from FCH can be handled here.
		 *
		 * Also OOB responses received post timeout land up here.
		 * No action needed, they can be discarded. Warning log is
		 * enough. When that happens, MIN_WAIT_TIME should be tweaked.
		 */

		if (rx->len < sizeof(msg.buf)) {
			memcpys(msg.buf, rx->buf, rx->len);
			msg.fn = NULL;
			msg.len = rx->len;
			msg.from = OOB_7BIT_ADDR(rx->buf[OOB_IDX_SRC_SLV_ADDR]);
			msg.tag = (uint8_t)evt_data;
				
			/* save the rx tag in the evt data */
			oob_rx_tag = (uint8_t)evt_data;
			oob_rx_length = (uint16_t) evt_details & 0x1FFF;

			if (k_msgq_put(&async_msgq, &msg, K_NO_WAIT)) {
				LOG_ERR("Rx msg enque failed %d", ret);
			}
		} else {
			LOG_ERR("Rx Message size too big.");
		}

	} else {

		if (master->rx->len >= rx->len) {
			memcpys(master->rx->buf, rx->buf, rx->len);
			master->rx->len = rx->len;
		} else {
			master->rx->len = 0;
			LOG_WRN("Rx Buf space too small");
		}

		k_sem_give(&master->txn_sync);
	}


	/* Clear Rx buffer for next downstream oob */
	memsets(rx->buf, 0, rx->len);
	rx->len = sizeof(rx_buf);
}

/**
 * @brief Initialize OOB manager
 */
static void oobmngr_init(void)
{
	k_mutex_init(&master_fch.txn_lock);

	k_sem_init(&master_fch.txn_sync, 1, 1);

#if CONFIG_OOB_MSG_MNGR
	oob_msg_mngr_init_proc();
#endif
}

/**
 * @brief OOB manager thread entry point
 *
 * @param p1 Unused parameter
 * @param p2 Unused parameter
 * @param p3 Unused parameter
 */
void oobmngr_thread(void *p1, void *p2, void *p3)
{
	int ret;
	struct async_msb msg;

	oobmngr_init();

	while (1) {
		k_msgq_get(&async_msgq, &msg, K_FOREVER);

		if (msg.from == OOB_SLAVE_ADDR_EC) {
			/* OOB message from EC to master */
			struct espi_oob_packet req = {
				.buf = msg.buf, .len = msg.len, .tag = msg.tag};
			struct espi_oob_packet resp = {
				.buf = msg.buf, .len = sizeof(msg.buf)};

			ret = oob_test_send_sync(&req, &resp,
					    OOB_MSG_SYNC_WAIT_TIME_DFLT);

			LOG_DBG("Async msg processed, status: %d", ret);

			if (msg.fn != NULL) {
				msg.fn(&resp, ret);
			}
		} else {
			/* Master initiated OOB message */
			switch (msg.from) {
			case OOB_MASTER_ADDR_FCH:
				msg.fn = fch_msg_hndlr;
				break;
			default:
				LOG_ERR("Unsupported 0%x", msg.from);
				break;
			}

			if (msg.fn != NULL) {
				struct espi_oob_packet mstr_msg = {
					.buf = msg.buf, .len = msg.len, .tag = msg.tag};
				msg.fn(&mstr_msg, 0);
			}
		}
	}
}

/**
 * @brief OOB receive callback handler
 *
 * @param evt_details Event details from eSPI
 * @param evt_data Event data from eSPI
 */
void oob_rx_cb_handler(uint32_t evt_details, uint32_t evt_data)
{
#ifndef CONFIG_OOBMNGR_SUPPORT
	return;
#endif
	struct espi_oob_packet rx = {.buf = rx_buf, .len = sizeof(rx_buf)};
#if 0 // Disable it for rpmc over espi feature enablement
	uint8_t saf_tx_buf[22];
	struct espi_oob_packet tx = {.buf = &saf_tx_buf[0], .len = 22};
#endif
	if (espihub_get_saf_boot_mode() == true) {
#if 0 // Disable it for rpmc over espi feature enablement
		saf_tx_buf[0] = 0x00;
		saf_tx_buf[1] = 0x26;
		saf_tx_buf[2] = 0x00;
		amd_crb_drivers_sFlashJedecId(0, &saf_tx_buf[3]);
		LOG_WRN("Jedec ID %x %x %x", saf_tx_buf[3], saf_tx_buf[4], saf_tx_buf[5]);
		amd_crb_drivers_sUniqId(0, 0, 16, &saf_tx_buf[6]);
		LOG_WRN("Unique ID %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x ", saf_tx_buf[6],
			saf_tx_buf[7], saf_tx_buf[8], saf_tx_buf[9], saf_tx_buf[10], saf_tx_buf[11],
			saf_tx_buf[12], saf_tx_buf[13], saf_tx_buf[14], saf_tx_buf[15],
			saf_tx_buf[16], saf_tx_buf[17], saf_tx_buf[18], saf_tx_buf[19],
			saf_tx_buf[20], saf_tx_buf[21]);
#endif
	}

	if (espihub_retrieve_oob(&rx) == 0) {
		if (espihub_get_saf_boot_mode() == true) {
#if 0 // Disable it for rpmc over espi feature enablement
			espihub_send_oob(&tx);
#else
			oob_rx_handler(&rx, evt_details, evt_data);
#endif
		} else {
			oob_rx_handler(&rx, evt_details, evt_data);
		}
	}
}