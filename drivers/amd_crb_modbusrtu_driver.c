/*****************************************************************************
 * Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
 ******************************************************************************
 */

#include <stdint.h>
#include <stdbool.h>
#include <errno.h>
#include <string.h>
#include <assert.h>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/uart.h>
#include "board_config.h"
#include "system.h"
#include "amd_crb_modbusrtu_driver.h"

LOG_MODULE_REGISTER(modbusrtu, CONFIG_MODBUSRTU_LOG_LEVEL);

/* Initialize device structure */
struct modbusrtu_device modbus_dev = {
	.rx_ready = false,
	.tx_done = false,
	.slave_addr = 0,
	.slave_reg_addr = 0,
	.slave_data_len = 0,
	.last_error = 0,
	.rx_enabled = false,
	.rx_buf_pool = {
		{.in_use = false},
		{.in_use = false}
	},
	.frame_state = {
		.frame_len = 0,
		.frame_started = false,
		.frame_complete = false,
		.expected_len = 0
	}
};


/* CRC16 lookup tables */
const uint8_t CRC16Low[256]={
	0x00,0xc0,0xc1,0x01,0xc3,0x03,0x02,0xc2,0xc6,0x06,0x07,0xc7,0x05,0xc5,0xc4,0x04,
	0xcc,0x0c,0x0d,0xcd,0x0f,0xcf,0xce,0x0e,0x0a,0xca,0xcb,0x0b,0xc9,0x09,0x08,0xc8,
	0xd8,0x18,0x19,0xd9,0x1b,0xdb,0xda,0x1a,0x1e,0xde,0xdf,0x1f,0xdd,0x1d,0x1c,0xdc,
	0x14,0xd4,0xd5,0x15,0xd7,0x17,0x16,0xd6,0xd2,0x12,0x13,0xd3,0x11,0xd1,0xd0,0x10,
	0xf0,0x30,0x31,0xf1,0x33,0xf3,0xf2,0x32,0x36,0xf6,0xf7,0x37,0xf5,0x35,0x34,0xf4,
	0x3c,0xfc,0xfd,0x3d,0xff,0x3f,0x3e,0xfe,0xfa,0x3a,0x3b,0xfb,0x39,0xf9,0xf8,0x38,
	0x28,0xe8,0xe9,0x29,0xeb,0x2b,0x2a,0xea,0xee,0x2e,0x2f,0xef,0x2d,0xed,0xec,0x2c,
	0xe4,0x24,0x25,0xe5,0x27,0xe7,0xe6,0x26,0x22,0xe2,0xe3,0x23,0xe1,0x21,0x20,0xe0,
	0xa0,0x60,0x61,0xa1,0x63,0xa3,0xa2,0x62,0x66,0xa6,0xa7,0x67,0xa5,0x65,0x64,0xa4,
	0x6c,0xac,0xad,0x6d,0xaf,0x6f,0x6e,0xae,0xaa,0x6a,0x6b,0xab,0x69,0xa9,0xa8,0x68,
	0x78,0xb8,0xb9,0x79,0xbb,0x7b,0x7a,0xba,0xbe,0x7e,0x7f,0xbf,0x7d,0xbd,0xbc,0x7c,
	0xb4,0x74,0x75,0xb5,0x77,0xb7,0xb6,0x76,0x72,0xb2,0xb3,0x73,0xb1,0x71,0x70,0xb0,
	0x50,0x90,0x91,0x51,0x93,0x53,0x52,0x92,0x96,0x56,0x57,0x97,0x55,0x95,0x94,0x54,
	0x9c,0x5c,0x5d,0x9d,0x5f,0x9f,0x9e,0x5e,0x5a,0x9a,0x9b,0x5b,0x99,0x59,0x58,0x98,
	0x88,0x48,0x49,0x89,0x4b,0x8b,0x8a,0x4a,0x4e,0x8e,0x8f,0x4f,0x8d,0x4d,0x4c,0x8c,
	0x44,0x84,0x85,0x45,0x87,0x47,0x46,0x86,0x82,0x42,0x43,0x83,0x41,0x81,0x80,0x40
};
const uint8_t CRC16High[256]={
	0x00,0xC1,0x81,0x40,0x01,0xc0,0x80,0x41,0x01,0xc0,0x80,0x41,0x00,0xc1,0x81,0x40,
	0x01,0xc0,0x80,0x41,0x00,0xc1,0x81,0x40,0x00,0xc1,0x81,0x40,0x01,0xc0,0x80,0x41,
	0x01,0xc0,0x80,0x41,0x00,0xc1,0x81,0x40,0x00,0xc1,0x81,0x40,0x01,0xc0,0x80,0x41,
	0x00,0xc1,0x81,0x40,0x01,0xc0,0x80,0x41,0x01,0xc0,0x80,0x41,0x00,0xc1,0x81,0x40,
	0x01,0xc0,0x80,0x41,0x00,0xc1,0x81,0x40,0x00,0xc1,0x81,0x40,0x01,0xc0,0x80,0x41,
	0x00,0xc1,0x81,0x40,0x01,0xc0,0x80,0x41,0x01,0xc0,0x80,0x41,0x00,0xc1,0x81,0x40,
	0x00,0xc1,0x81,0x40,0x01,0xc0,0x80,0x41,0x01,0xc0,0x80,0x41,0x00,0xc1,0x81,0x40,
	0x01,0xc0,0x80,0x41,0x00,0xc1,0x81,0x40,0x00,0xc1,0x81,0x40,0x01,0xc0,0x80,0x41,
	0x01,0xc0,0x80,0x41,0x00,0xc1,0x81,0x40,0x00,0xc1,0x81,0x40,0x01,0xc0,0x80,0x41,
	0x00,0xc1,0x81,0x40,0x01,0xc0,0x80,0x41,0x01,0xc0,0x80,0x41,0x00,0xc1,0x81,0x40,
	0x00,0xc1,0x81,0x40,0x01,0xc0,0x80,0x41,0x01,0xc0,0x80,0x41,0x00,0xc1,0x81,0x40,
	0x01,0xc0,0x80,0x41,0x00,0xc1,0x81,0x40,0x00,0xc1,0x81,0x40,0x01,0xc0,0x80,0x41,
	0x00,0xc1,0x81,0x40,0x01,0xc0,0x80,0x41,0x01,0xc0,0x80,0x41,0x00,0xc1,0x81,0x40,
	0x01,0xc0,0x80,0x41,0x00,0xc1,0x81,0x40,0x00,0xc1,0x81,0x40,0x01,0xc0,0x80,0x41,
	0x01,0xc0,0x80,0x41,0x00,0xc1,0x81,0x40,0x00,0xc1,0x81,0x40,0x01,0xc0,0x80,0x41,
	0x00,0xc1,0x81,0x40,0x01,0xc0,0x80,0x41,0x01,0xc0,0x80,0x41,0x00,0xc1,0x81,0x40
};

/* Forward declaration */
static uint16_t Modbus_CRC16(const uint8_t *buff_addr, uint8_t buf_length);
static rx_buf_t *get_free_rx_buf(void);
static void release_rx_buf(rx_buf_t *buf);
static int re_enable_rx_dma(void);

static bool parse_frame_header(uint8_t *data, uint16_t len, uint16_t *expected_len);
static void reset_frame_state(void);

/**
 * @brief Parse Modbus frame header to derive expected frame length
 *
 * @param data: Pointer to frame data (must contain at least first 3 bytes: addr+func+data_len)
 * @param len: Length of received data
 * @param expected_len: Output parameter, expected complete frame length
 *
 * @return bool: true if parsing succeeds, false otherwise
 */
static bool parse_frame_header(uint8_t *data, uint16_t len, uint16_t *expected_len)
{
	if (len < 3) {
		LOG_DBG("Frame header incomplete (len=%d < 3)", len);
		return false;
	}

	uint8_t func_code = data[1];

	/* Handle function code 04 or 03 response (normal response: addr+func+data_len+data+CRC) */
	if (func_code == 0x04||func_code == 0x03) {
		uint8_t data_len = data[2]; /* Data length field (in bytes) */
		*expected_len = 3 + data_len + 2; /* 3(addr+func+data_len) + data + 2(CRC) */
		LOG_DBG("04 func response: expected len=%d (data_len=%d)", *expected_len, data_len);
		return true;
	}

	/* Handle function code 04 or 03 exception response (addr+func(0x84)+exception_code+CRC) */
	if (func_code == 0x84||func_code == 0x83) {
		*expected_len = 5; /* Fixed length: 4 bytes(addr+func+exception_code) + 2 bytes(CRC) = 5 */
		LOG_DBG("04 func exception response: expected len=%d", *expected_len);
		return true;
	}

	/* Handle function code 0x06 (Write Single Register) normal response */
	/* Response structure: addr(1) + func(1) + reg_addr_high(1) + reg_addr_low(1) + reg_value_high(1) + reg_value_low(1) + CRC(2) */
	if (func_code == 0x06) {
		  *expected_len = 8; /* Fixed length for 0x06 normal response (6 bytes payload + 2 bytes CRC) */
		  LOG_DBG("06 func normal response: expected len=%d", *expected_len);
		  return true;
	  }
	
	  /* Handle function code 0x06 exception response (addr+func(0x86)+exception_code+CRC) */
	if (func_code == 0x86) {
		  *expected_len = 5; /* Fixed length: 4 bytes(addr+func+exception_code) + 2 bytes(CRC) = 5 */
		  LOG_DBG("06 func exception response: expected len=%d", *expected_len);
		  return true;
	}

	/* Handle function code 0x10 (Write Multiple Registers) normal response */
	/* Response structure: addr(1) + func(1) + start_reg_high(1) + start_reg_low(1) + reg_count_high(1) + reg_count_low(1) + CRC(2) */
	if (func_code == 0x10) {
		*expected_len = 8; /* Fixed length for 0x10 normal response (6 bytes payload + 2 bytes CRC) */
		LOG_DBG("10 func normal response: expected len=%d", *expected_len);
		return true;
	}

	/* Handle function code 0x10 exception response (addr+func(0x90)+exception_code+CRC) */
	if (func_code == 0x90) {
		*expected_len = 5; /* Fixed length: 4 bytes(addr+func+exception_code) + 2 bytes(CRC) = 5 */
		LOG_DBG("10 func exception response: expected len=%d", *expected_len);
		return true;
	}

	/* Extend for other function codes (e.g., 0x03, 0x10) as needed */
	LOG_WRN("Unsupported function code for header parse: 0x%02x", func_code);
	return false;
}

/**
 * @brief Reset frame reception state
 */
static void reset_frame_state(void)
{
	struct modbusrtu_device *mdev = &modbus_dev;
	k_mutex_lock(&mdev->lock, K_FOREVER);
	mdev->frame_state.frame_len = 0;
	mdev->frame_state.frame_started = false;
	mdev->frame_state.frame_complete = false;
	mdev->frame_state.expected_len = 0;
	memset(mdev->frame_state.frame_buf, 0, MODBUS_MAX_FRAME_SIZE);
	k_mutex_unlock(&mdev->lock);
	LOG_DBG("Frame state reset");
}

/**
 * @brief UART event callback handler (supports cross-buffer data concatenation)
 *
 * @param dev: UART device pointer
 * @param evt: UART event structure pointer
 * @param user_data: User data pointer (unused)
 */
static void uart_callback(const struct device *dev, struct uart_event *evt, void *user_data)
{
	struct modbusrtu_device *mdev = &modbus_dev;
	int i;
	rx_buf_t *used_buf = NULL;

	switch (evt->type) {
	case UART_TX_DONE:
		LOG_DBG("UART TX Complete: buf=%p len=%d", (void *)evt->data.tx.buf, evt->data.tx.len);
		mdev->tx_done = true;
		k_sem_give(&mdev->tx_sem);
		break;

	case UART_TX_ABORTED:
		LOG_ERR("UART_TX_ABORTED: buf=%p len=%d", (void *)evt->data.tx.buf, evt->data.tx.len);
		mdev->tx_done = false;
		mdev->last_error = -EIO;
		k_sem_give(&mdev->tx_sem);
		break;

	case UART_RX_RDY:
		LOG_DBG("UART_RX_RDY: buf=%p len=%d offset=%d",
				(void *)evt->data.rx.buf, evt->data.rx.len, evt->data.rx.offset);

		/* Skip empty data (timeout) */
		if (evt->data.rx.len == 0) {
			LOG_DBG("RX empty data (timeout)");
			reset_frame_state();
			mdev->last_error = -ETIMEDOUT;
			/* Release current buffer */
			for (int idx = 0; idx < RX_BUF_POOL_SIZE; idx++) {
				if (evt->data.rx.buf == mdev->rx_buf_pool[idx].data) {
					release_rx_buf(&mdev->rx_buf_pool[idx]);
					break;
				}
			}
			re_enable_rx_dma();
			k_sem_give(&mdev->rx_sem);
			break;
		}

		/* Find the currently used buffer */
		for (int idx = 0; idx < RX_BUF_POOL_SIZE; idx++) {
			if (evt->data.rx.buf == mdev->rx_buf_pool[idx].data) {
				used_buf = &mdev->rx_buf_pool[idx];
				break;
			}
		}
		if (used_buf == NULL) {
			LOG_ERR("Unknown RX buffer: %p", (void *)evt->data.rx.buf);
			mdev->last_error = -EINVAL;
			k_sem_give(&mdev->rx_sem);
			break;
		}

		/* Check for frame buffer overflow */
		if (mdev->frame_state.frame_len + evt->data.rx.len > MODBUS_MAX_FRAME_SIZE) {
			LOG_ERR("Frame buffer overflow (current=%d, new=%d, max=%d)",
					mdev->frame_state.frame_len, evt->data.rx.len, MODBUS_MAX_FRAME_SIZE);
			reset_frame_state();
			mdev->last_error = -EOVERFLOW;
			k_mutex_unlock(&mdev->lock);
			release_rx_buf(used_buf);
			k_sem_give(&mdev->rx_sem);
			break;
		}

		/* Append new data to frame buffer */
		memcpy(&mdev->frame_state.frame_buf[mdev->frame_state.frame_len],
			   &evt->data.rx.buf[evt->data.rx.offset],
			   evt->data.rx.len);
		mdev->frame_state.frame_len += evt->data.rx.len;
		mdev->frame_state.frame_started = true;

		LOG_DBG("Frame buffer updated: total len=%d", mdev->frame_state.frame_len);
		LOG_HEXDUMP_DBG(&mdev->frame_state.frame_buf[mdev->frame_state.frame_len - evt->data.rx.len],
						evt->data.rx.len, "New RX data");

		/* Parse frame header to derive expected length (only if not derived and data is sufficient) */
		if (mdev->frame_state.expected_len == 0) {
			parse_frame_header(mdev->frame_state.frame_buf, mdev->frame_state.frame_len,
							  &mdev->frame_state.expected_len);
		}

		/* Check if frame is complete */
		bool frame_complete = false;
		if (mdev->frame_state.expected_len > 0) {
			/* Judge by expected length */
			frame_complete = (mdev->frame_state.frame_len >= mdev->frame_state.expected_len);
		} else {
			/* Fallback to minimum frame length + timeout if expected length not derived (avoid infinite waiting) */
			frame_complete = (mdev->frame_state.frame_len >= MODBUS_MIN_FRAME_SIZE);
		}

		if (frame_complete) {
			mdev->frame_state.frame_complete = true;
			LOG_DBG("Frame complete: total len=%d (expected=%d)",
					mdev->frame_state.frame_len, mdev->frame_state.expected_len);
		}

		release_rx_buf(used_buf);

		/* Trigger application layer processing when frame is complete */
		if (mdev->frame_state.frame_complete) {
			mdev->rx_ready = true;
			const uint8_t *frame_data = mdev->frame_state.frame_buf;
			uint16_t frame_len = mdev->frame_state.frame_len;

			/* Log complete frame */
			LOG_INF("Complete frame received: %d bytes", frame_len);
			for (i = 0; i < frame_len; i++) {
				LOG_INF("  [%d]: 0x%02x", i, frame_data[i]);
			}

			/* Verify minimum frame length (addr + func + CRC = 4 bytes minimum) */
			if (frame_len < 4) {
				LOG_ERR("Frame too short: %d < 4", frame_len);
				mdev->last_error = -EINVAL;
				reset_frame_state();
				k_sem_give(&mdev->rx_sem);
				break;
			}

			/* CRC verification */
			uint16_t received_crc = (frame_data[frame_len - 1] << 8) | frame_data[frame_len - 2];
			uint16_t calculated_crc = Modbus_CRC16(frame_data, frame_len - 2);
			if (received_crc != calculated_crc) {
				LOG_ERR("CRC mismatch: received=0x%04x calculated=0x%04x",
						received_crc, calculated_crc);
				mdev->last_error = -EBADMSG;
				reset_frame_state();
				k_sem_give(&mdev->rx_sem);
				break;
			}


			/* Process function code 0x03 (Read Holding Registers) response */
			if (frame_data[1] == 0x03) {
				mdev->slave_data_len = frame_data[2];

				/* Verify data length consistency */
				if (mdev->slave_data_len != (frame_len - 5)) {
					LOG_ERR("Data length mismatch: header=%d actual=%d",
							mdev->slave_data_len, frame_len - 5);
					mdev->last_error = -EINVAL;
					reset_frame_state();
					k_sem_give(&mdev->rx_sem);
					break;
				}

				/* Register address boundary check */
				if (mdev->slave_reg_addr + (mdev->slave_data_len / 2) > MODBUS_MAX_REGISTERS) {
					LOG_ERR("Register out of bounds: 0x%04x + %d",
							mdev->slave_reg_addr, mdev->slave_data_len / 2);
					mdev->last_error = -ERANGE;
					reset_frame_state();
					k_sem_give(&mdev->rx_sem);
					break;
				}

				/* Store register data (big-endian to host byte order) */
				for (int m = 0; m < mdev->slave_data_len / 2; m++) {
					mdev->registers[mdev->slave_reg_addr + m] =
						(frame_data[3 + m * 2] << 8) | frame_data[4 + m * 2];
					LOG_INF("Register[0x%04x] = 0x%04x",
							mdev->slave_reg_addr + m,
							mdev->registers[mdev->slave_reg_addr + m]);
				}
				mdev->last_error = 0;
				LOG_INF("03 func read completed successfully");
			}
			/* Process function code 0x04 (Read Input Registers) response */
			else if (frame_data[1] == 0x04) {
				mdev->slave_data_len = frame_data[2];

				/* Verify data length consistency */
				if (mdev->slave_data_len != (frame_len - 5)) {
					LOG_ERR("Data length mismatch: header=%d actual=%d",
							mdev->slave_data_len, frame_len - 5);
					mdev->last_error = -EINVAL;
					reset_frame_state();
					k_sem_give(&mdev->rx_sem);
					break;
				}

				/* Register address boundary check */
				if (mdev->slave_reg_addr + (mdev->slave_data_len / 2) > MODBUS_MAX_REGISTERS) {
					LOG_ERR("Register out of bounds: 0x%04x + %d",
							mdev->slave_reg_addr, mdev->slave_data_len / 2);
					mdev->last_error = -ERANGE;
					reset_frame_state();
					k_sem_give(&mdev->rx_sem);
					break;
				}

				/* Store register data (big-endian to host byte order) */
				for (int m = 0; m < mdev->slave_data_len / 2; m++) {
					mdev->registers[mdev->slave_reg_addr + m] =
						(frame_data[3 + m * 2] << 8) | frame_data[4 + m * 2];
					LOG_ERR("Register[0x%04x] = 0x%04x",
							mdev->slave_reg_addr + m,
							mdev->registers[mdev->slave_reg_addr + m]);
				}
				mdev->last_error = 0;
				LOG_INF("04 func read completed successfully");
			}
			else if (frame_data[1] == 0x83) {
				/* Process function code 0x03 exception response */
				uint8_t exception_code = frame_data[2];
				LOG_ERR("03 func exception: 0x%02x", exception_code);
				mdev->last_error = -EIO;
			}
			else if (frame_data[1] == 0x84) {
				/* Process function code 0x04 exception response */
				uint8_t exception_code = frame_data[2];
				LOG_ERR("04 func exception: 0x%02x", exception_code);
				mdev->last_error = -EIO;
			} 
			else if (frame_data[1] == 0x10) {
				/* Process function code 10 exception response */
				LOG_INF("10 func completed successfully");
				mdev->last_error = 0;
			} else {
				LOG_WRN("Unsupported function code: 0x%02x", frame_data[1]);
				mdev->last_error = -ENOTSUP;
			}

			/* Reset frame state for next reception */
			reset_frame_state();
			k_sem_give(&mdev->rx_sem);
		}
		break;

	case UART_RX_BUF_REQUEST:
		LOG_DBG("UART_RX_BUF_REQUEST: need new buffer");
		rx_buf_t *free_buf = get_free_rx_buf();
		if (free_buf != NULL) {
			LOG_DBG("Respond with buffer: %p (len=%d)", (void *)free_buf->data, RX_BUF_LEN);
			free_buf->in_use = true;
			int ret = uart_rx_buf_rsp(dev, free_buf->data, RX_BUF_LEN);
			if (ret != 0) {
				LOG_ERR("rx_buf_rsp failed: %d", ret);
				release_rx_buf(free_buf);
			}
		} else {
			LOG_ERR("No free buffer!");
			uart_rx_buf_rsp(dev, NULL, 0);
		}
		break;

	case UART_RX_BUF_RELEASED:
		LOG_DBG("UART_RX_BUF_RELEASED: buf=%p", (void *)evt->data.rx_buf.buf);
		for (int idx = 0; idx < RX_BUF_POOL_SIZE; idx++) {
			if (evt->data.rx_buf.buf == mdev->rx_buf_pool[idx].data) {
				release_rx_buf(&mdev->rx_buf_pool[idx]);
				memset(mdev->rx_buf_pool[idx].data, 0, RX_BUF_LEN);
				break;
			}
		}
		break;

	case UART_RX_DISABLED:
		LOG_DBG("UART_RX_DISABLED");
		mdev->rx_enabled = false;
		reset_frame_state();
		for (int idx = 0; idx < RX_BUF_POOL_SIZE; idx++) {
			release_rx_buf(&mdev->rx_buf_pool[idx]);
		}
		break;

	case UART_RX_STOPPED:
		LOG_ERR("UART_RX_STOPPED: reason=%d", evt->data.rx_stop.reason);
		if (mdev->rx_enabled) {
			re_enable_rx_dma();
		}
		break;
	}
}



/**
 * @brief Get a free buffer from RX pool (thread-safe)
 *
 * @return rx_buf_t*: Pointer to free buffer, NULL if pool exhausted
 */
static rx_buf_t *get_free_rx_buf(void)
{
	struct modbusrtu_device *mdev = &modbus_dev;
	k_mutex_lock(&mdev->lock, K_FOREVER);

	/* Find first free buffer (FIFO order) */
	for (int idx = 0; idx < RX_BUF_POOL_SIZE; idx++) {
		if (!mdev->rx_buf_pool[idx].in_use) {
			k_mutex_unlock(&mdev->lock);
			return &mdev->rx_buf_pool[idx];
		}
	}

	k_mutex_unlock(&mdev->lock);
	LOG_WRN("RX buffer pool exhausted (size=%d)", RX_BUF_POOL_SIZE);
	return NULL;
}

/**
 * @brief Release a buffer (mark as free, thread-safe)
 *
 * @param buf: Pointer to buffer to release
 */
static void release_rx_buf(rx_buf_t *buf)
{
	if (buf == NULL) {
		LOG_WRN("Attempt to release NULL buffer");
		return;
	}

	struct modbusrtu_device *mdev = &modbus_dev;
	k_mutex_lock(&mdev->lock, K_FOREVER);
	buf->in_use = false;
	k_mutex_unlock(&mdev->lock);
	LOG_DBG("Buffer released: %p", (void *)buf->data);
}

/**
 * @brief Re-enable RX DMA (adapted to NPCX driver flow)
 *
 * @return int: 0 if success, negative error code otherwise
 */
static int re_enable_rx_dma(void)
{
	struct modbusrtu_device *mdev = &modbus_dev;
	if (!mdev->rx_enabled || mdev->uart_dev == NULL) {
		LOG_ERR("RX not initialized or UART device invalid");
		return -EINVAL;
	}

	/* Get first free buffer for initial enable */
	rx_buf_t *free_buf = get_free_rx_buf();
	if (free_buf == NULL) {
		LOG_ERR("No free buffer to re-enable RX");
		return -ENOMEM;
	}

	free_buf->in_use = true;
	int ret = uart_rx_enable(mdev->uart_dev, free_buf->data, RX_BUF_LEN,MODBUS_RX_TIMEOUT_MS);
	if (ret != 0) {
		LOG_ERR("Failed to re-enable RX DMA: %d", ret);
		release_rx_buf(free_buf);
		mdev->rx_enabled = false;
		return ret;
	}

	LOG_DBG("RX DMA re-enabled with buffer: %p", (void *)free_buf->data);
	return 0;
}

/**
 * @brief Initialize Modbus RTU driver (adapted to NPCX UART DMA driver)
 *
 * @return int: 0 if success, negative error code otherwise
 */
int amd_crb_modbusrtu_init(void)
{
	struct modbusrtu_device *mdev = &modbus_dev;

	/* Get UART device */
	mdev->uart_dev = DEVICE_DT_GET(UART_NODE);
	if (!device_is_ready(mdev->uart_dev)) {
		LOG_ERR("UART device not ready");
		return -ENODEV;
	}
	/* Initialize synchronization primitives */
	k_mutex_init(&mdev->lock);
	k_sem_init(&mdev->rx_sem, 0, 1);
	k_sem_init(&mdev->tx_sem, 0, 1);
	k_sem_init(&mdev->transaction_sem, 1, 1);

	/* Register callback with NPCX UART driver */
	int ret = uart_callback_set(mdev->uart_dev, uart_callback, NULL);
	if (ret != 0) {
		LOG_ERR("Failed to set UART callback: %d", ret);
		return ret;
	}

	/* Clear buffers and registers */
	for (int idx = 0; idx < RX_BUF_POOL_SIZE; idx++) {
		memset(mdev->rx_buf_pool[idx].data, 0, RX_BUF_LEN);
		mdev->rx_buf_pool[idx].in_use = false;
	}
	memset(mdev->tx_buf, 0, sizeof(mdev->tx_buf));
	memset(mdev->registers, 0, sizeof(mdev->registers));

	/* Enable RX DMA (initial enable: provides first buffer) */
	mdev->rx_enabled = true;
	ret = re_enable_rx_dma();
	if (ret != 0) {
		LOG_ERR("Failed to enable RX DMA during initialization: %d", ret);
		mdev->rx_enabled = false;
		return ret;
	}

	LOG_INF("Modbus RTU driver initialized (UART: %s, DMA mode, buf pool size: %d)",
			mdev->uart_dev->name, RX_BUF_POOL_SIZE);
	return 0;
}

/**
 * @brief Calculate Modbus RTU CRC16
 *
 * @param buff_addr: Data buffer pointer
 * @param buf_length: Data length in bytes
 *
 * @return uint16_t: Calculated CRC16 (little-endian)
 */
static uint16_t Modbus_CRC16(const uint8_t *buff_addr, uint8_t buf_length)
{
	uint16_t uIndex;
	uint8_t uchCRCHi = 0xFF;
	uint8_t uchCRCLow = 0xFF;

	while (buf_length--) {
		uIndex = uchCRCLow ^ *buff_addr++;
		uchCRCLow = uchCRCHi ^ CRC16High[uIndex];
		uchCRCHi = CRC16Low[uIndex];
	}
	return (uchCRCHi << 8 | uchCRCLow);
}

/**
 * @brief Read holding registers
 *
 * @param dev_addr: Modbus device address (1-247)
 * @param reg_addr: Starting register address
 * @param values: Pointer to buffer to store register values (can be NULL to only read into cache)
 * @param length: Number of registers to read
 *
 * @return 0 if successful, negative error code otherwise
 *
 * @note Implements Modbus function code 0x03 (Read Holding Registers)
 *       This function reads from device and optionally copies values to user buffer.
 *       If values is NULL, data is only stored in internal cache.
 */
int amd_crb_modbusrtu_read(uint16_t dev_addr, uint16_t reg_addr, uint16_t *values, uint16_t length)
{
	struct modbusrtu_device *mdev = &modbus_dev;
	uint16_t crc_value = 0;
	int ret = 0;

	/* Parameter validation */
	if (dev_addr == 0 || dev_addr > 247) {
		LOG_ERR("Invalid device address: %d", dev_addr);
		return -EINVAL;
	}

	if (length == 0 || length > 125) {  /* Modbus spec: max 125 registers per request */
		LOG_ERR("Invalid register length: %d", length);
		return -EINVAL;
	}

	if (reg_addr + length > 0xFFFF) {
		LOG_ERR("Register address overflow");
		return -ERANGE;
	}
	
	if (reg_addr + length > MODBUS_MAX_REGISTERS) {
		LOG_ERR("Register range out of bounds");
		return -ERANGE;
	}

	/* Acquire transaction semaphore to serialize all Modbus operations */
	ret = k_sem_take(&mdev->transaction_sem, K_MSEC(2000));
	if (ret) {
		LOG_ERR("Failed to acquire transaction lock");
		return -EBUSY;
	}

	/* Lock for the entire transaction to prevent concurrent access */
	k_mutex_lock(&mdev->lock, K_FOREVER);

	/* Store request info for callback */
	mdev->slave_reg_addr = reg_addr;
	mdev->rx_ready = false;
	mdev->tx_done = false;
	mdev->last_error = 0;

	/* Build request packet */
	mdev->tx_buf[0] = (uint8_t)dev_addr;
	mdev->tx_buf[1] = 0x04;  /* Function code: Read Holding Registers */
	mdev->tx_buf[2] = (uint8_t)((reg_addr >> 8) & 0xFF);
	mdev->tx_buf[3] = (uint8_t)(reg_addr & 0xFF);
	mdev->tx_buf[4] = (uint8_t)((length >> 8) & 0xFF);
	mdev->tx_buf[5] = (uint8_t)(length & 0xFF);

	/* Calculate and append CRC */
	crc_value = Modbus_CRC16(mdev->tx_buf, 6);
	mdev->tx_buf[6] = (uint8_t)(crc_value & 0xFF);
	mdev->tx_buf[7] = (uint8_t)((crc_value >> 8) & 0xFF);

	/* Log transmit data */
	LOG_DBG("Modbus TX (8 bytes):");
	for (int i = 0; i < 8; i++) {
		LOG_DBG("  TX[%d]: 0x%02x", i, mdev->tx_buf[i]);
	}

	/* Expected response length: addr(1) + func(1) + length(1) + data(length*2) + crc(2) */
	mdev->request_len = length * 2 + 5;

	/* Clear receive buffer */
	memset(mdev->rx_buf_pool[0].data, 0, MODBUS_MSG_SIZE);

	/* Reset semaphores */
	k_sem_reset(&mdev->rx_sem);
	k_sem_reset(&mdev->tx_sem);

	/* Ensure UART is clean - disable any pending operations */
	uart_rx_disable(mdev->uart_dev);

	/* Transmit request - TX MUST happen before enabling RX */
	ret = uart_tx(mdev->uart_dev, mdev->tx_buf, 8, SYS_FOREVER_MS);
	if (ret) {
		LOG_ERR("Failed to transmit: %d", ret);
		k_mutex_unlock(&mdev->lock);
		goto cleanup;
	}
	
	/* Now enable RX to catch the response */
	ret = uart_rx_enable(mdev->uart_dev, mdev->rx_buf_pool[0].data, MODBUS_MSG_SIZE, MODBUS_RX_TIMEOUT_MS);
	if (ret) {
		LOG_ERR("Failed to enable RX: %d", ret);
		k_mutex_unlock(&mdev->lock);
		goto cleanup;
	}

	/* Temporarily unlock to allow callback to execute */
	k_mutex_unlock(&mdev->lock);

	/* Wait for TX complete */
	ret = k_sem_take(&mdev->tx_sem, K_MSEC(MODBUS_TX_TIMEOUT_MS));
	if (ret) {
		LOG_ERR("TX timeout waiting for completion");
		uart_rx_disable(mdev->uart_dev);
		ret = -ETIMEDOUT;
		goto cleanup;
	}

	/* Check if TX was successful */
	if (!mdev->tx_done) {
		LOG_ERR("TX was aborted");
		uart_rx_disable(mdev->uart_dev);
		ret = -EIO;
		goto cleanup;
	}

	/* Wait for response with timeout */
	LOG_DBG("Waiting for read response...");
	ret = k_sem_take(&mdev->rx_sem, K_MSEC(MODBUS_RX_TIMEOUT_MS));
	LOG_DBG("k_sem_take returned: %d", ret);
	
	/* Reacquire lock for cleanup */
	k_mutex_lock(&mdev->lock, K_FOREVER);
	
	if (ret) {
		LOG_ERR("Response timeout");
		/* Disable RX on timeout */
		uart_rx_disable(mdev->uart_dev);
		k_mutex_unlock(&mdev->lock);
		ret = -ETIMEDOUT;
		goto cleanup;
	}

	/* Disable RX after successful response */
	uart_rx_disable(mdev->uart_dev);
	
	/* Check for errors from callback */
	if (mdev->last_error) {
		LOG_ERR("Read operation failed: %d", mdev->last_error);
		ret = mdev->last_error;
		k_mutex_unlock(&mdev->lock);
		goto cleanup;
	}

	/* Copy values to user buffer if requested */
	if (values != NULL) {
		memcpy(values, &mdev->registers[reg_addr], length * sizeof(uint16_t));
	}
	
	k_mutex_unlock(&mdev->lock);

	/* Inter-frame delay (3.5 character times) - increased for stability */
	k_msleep(MODBUS_INTER_FRAME_DELAY_MS);

	ret = 0;

cleanup:
	/* Ensure UART is fully idle before next transaction */
	if (ret != 0) {
		/* On error, ensure RX is disabled */
		uart_rx_disable(mdev->uart_dev);
	}
	
	/* Additional settling time to prevent EBUSY on next call */
	k_msleep(5);
	
	/* Release transaction semaphore */
	k_sem_give(&mdev->transaction_sem);
	return ret;
}

/**
 * @brief Read holding registers using function code 0x03
 *
 * @param dev_addr: Modbus device address (1-247)
 * @param reg_addr: Starting register address
 * @param values: Pointer to buffer to store register values (can be NULL to only read into cache)
 * @param length: Number of registers to read
 *
 * @return 0 if successful, negative error code otherwise
 *
 * @note Implements Modbus function code 0x03 (Read Holding Registers)
 *       This function reads from device and optionally copies values to user buffer.
 *       If values is NULL, data is only stored in internal cache.
 */
int amd_crb_modbusrtu_read_holding_registers(uint16_t dev_addr, uint16_t reg_addr, uint16_t *values, uint16_t length)
{
	struct modbusrtu_device *mdev = &modbus_dev;
	uint16_t crc_value = 0;
	int ret = 0;

	/* Parameter validation */
	if (dev_addr == 0 || dev_addr > 247) {
		LOG_ERR("Invalid device address: %d", dev_addr);
		return -EINVAL;
	}

	if (length == 0 || length > 125) {  /* Modbus spec: max 125 registers per request */
		LOG_ERR("Invalid register length: %d", length);
		return -EINVAL;
	}

	if (reg_addr + length > 0xFFFF) {
		LOG_ERR("Register address overflow");
		return -ERANGE;
	}
	
	if (reg_addr + length > MODBUS_MAX_REGISTERS) {
		LOG_ERR("Register range out of bounds");
		return -ERANGE;
	}

	/* Acquire transaction semaphore to serialize all Modbus operations */
	ret = k_sem_take(&mdev->transaction_sem, K_MSEC(2000));
	if (ret) {
		LOG_ERR("Failed to acquire transaction lock");
		return -EBUSY;
	}

	/* Lock for the entire transaction to prevent concurrent access */
	k_mutex_lock(&mdev->lock, K_FOREVER);

	/* Store request info for callback */
	mdev->slave_reg_addr = reg_addr;
	mdev->rx_ready = false;
	mdev->tx_done = false;
	mdev->last_error = 0;

	/* Build request packet */
	mdev->tx_buf[0] = (uint8_t)dev_addr;
	mdev->tx_buf[1] = 0x03;  /* Function code: Read Holding Registers */
	mdev->tx_buf[2] = (uint8_t)((reg_addr >> 8) & 0xFF);
	mdev->tx_buf[3] = (uint8_t)(reg_addr & 0xFF);
	mdev->tx_buf[4] = (uint8_t)((length >> 8) & 0xFF);
	mdev->tx_buf[5] = (uint8_t)(length & 0xFF);

	/* Calculate and append CRC */
	crc_value = Modbus_CRC16(mdev->tx_buf, 6);
	mdev->tx_buf[6] = (uint8_t)(crc_value & 0xFF);
	mdev->tx_buf[7] = (uint8_t)((crc_value >> 8) & 0xFF);

	/* Log transmit data */
	LOG_DBG("Modbus TX (8 bytes) - Read Holding Registers:");
	for (int i = 0; i < 8; i++) {
		LOG_DBG("  TX[%d]: 0x%02x", i, mdev->tx_buf[i]);
	}

	/* Expected response length: addr(1) + func(1) + length(1) + data(length*2) + crc(2) */
	mdev->request_len = length * 2 + 5;

	/* Clear receive buffer */
	memset(mdev->rx_buf_pool[0].data, 0, MODBUS_MSG_SIZE);

	/* Reset semaphores */
	k_sem_reset(&mdev->rx_sem);
	k_sem_reset(&mdev->tx_sem);

	/* Ensure UART is clean - disable any pending operations */
	uart_rx_disable(mdev->uart_dev);

	/* Transmit request - TX MUST happen before enabling RX */
	ret = uart_tx(mdev->uart_dev, mdev->tx_buf, 8, SYS_FOREVER_MS);
	if (ret) {
		LOG_ERR("Failed to transmit: %d", ret);
		k_mutex_unlock(&mdev->lock);
		goto cleanup;
	}
	
	/* Now enable RX to catch the response */
	ret = uart_rx_enable(mdev->uart_dev, mdev->rx_buf_pool[0].data, MODBUS_MSG_SIZE, MODBUS_RX_TIMEOUT_MS);
	if (ret) {
		LOG_ERR("Failed to enable RX: %d", ret);
		k_mutex_unlock(&mdev->lock);
		goto cleanup;
	}

	/* Temporarily unlock to allow callback to execute */
	k_mutex_unlock(&mdev->lock);

	/* Wait for TX complete */
	ret = k_sem_take(&mdev->tx_sem, K_MSEC(MODBUS_TX_TIMEOUT_MS));
	if (ret) {
		LOG_ERR("TX timeout waiting for completion");
		uart_rx_disable(mdev->uart_dev);
		ret = -ETIMEDOUT;
		goto cleanup;
	}

	/* Check if TX was successful */
	if (!mdev->tx_done) {
		LOG_ERR("TX was aborted");
		uart_rx_disable(mdev->uart_dev);
		ret = -EIO;
		goto cleanup;
	}

	/* Wait for response with timeout */
	LOG_DBG("Waiting for holding register read response...");
	ret = k_sem_take(&mdev->rx_sem, K_MSEC(MODBUS_RX_TIMEOUT_MS));
	LOG_DBG("k_sem_take returned: %d", ret);
	
	/* Reacquire lock for cleanup */
	k_mutex_lock(&mdev->lock, K_FOREVER);
	
	if (ret) {
		LOG_ERR("Response timeout");
		/* Disable RX on timeout */
		uart_rx_disable(mdev->uart_dev);
		k_mutex_unlock(&mdev->lock);
		ret = -ETIMEDOUT;
		goto cleanup;
	}

	/* Disable RX after successful response */
	uart_rx_disable(mdev->uart_dev);
	
	/* Check for errors from callback */
	if (mdev->last_error) {
		LOG_ERR("Read holding register operation failed: %d", mdev->last_error);
		ret = mdev->last_error;
		k_mutex_unlock(&mdev->lock);
		goto cleanup;
	}

	/* Copy values to user buffer if requested */
	if (values != NULL) {
		memcpy(values, &mdev->registers[reg_addr], length * sizeof(uint16_t));
	}
	
	k_mutex_unlock(&mdev->lock);

	/* Inter-frame delay (3.5 character times) - increased for stability */
	k_msleep(MODBUS_INTER_FRAME_DELAY_MS);

	ret = 0;

cleanup:
	/* Ensure UART is fully idle before next transaction */
	if (ret != 0) {
		/* On error, ensure RX is disabled */
		uart_rx_disable(mdev->uart_dev);
	}
	
	/* Additional settling time to prevent EBUSY on next call */
	k_msleep(5);
	
	/* Release transaction semaphore */
	k_sem_give(&mdev->transaction_sem);
	return ret;
}

/**
 * @brief Write multiple holding registers
 *
 * @param dev_addr: Modbus device address (1-247)
 * @param reg_addr: Starting register address
 * @param value: Data buffer pointer
 * @param len: Data length in bytes
 *
 * @return 0 if successful, negative error code otherwise
 *
 * @note Implements Modbus function code 0x10 (Write Multiple Registers)
 */
int amd_crb_modbusrtu_write(uint16_t dev_addr, uint16_t reg_addr,
			    uint8_t *value, uint32_t len)
{
	struct modbusrtu_device *mdev = &modbus_dev;
	bool is_odd;
	uint32_t num = 0;
	uint16_t crc_value = 0;
	int ret = 0;
	uint32_t actual_len = len;

	/* Parameter validation */
	if (dev_addr == 0 || dev_addr > 247) {
		LOG_ERR("Invalid device address: %d", dev_addr);
		return -EINVAL;
	}

	if (value == NULL) {
		LOG_ERR("NULL value pointer");
		return -EINVAL;
	}

	if (len == 0 || len > (MODBUS_MSG_SIZE - 9)) {
		LOG_ERR("Invalid data length: %d", len);
		return -EINVAL;
	}

	if (reg_addr + (len / 2) > 0xFFFF) {
		LOG_ERR("Register address overflow");
		return -ERANGE;
	}

	/* Acquire transaction semaphore to serialize all Modbus operations */
	ret = k_sem_take(&mdev->transaction_sem, K_MSEC(2000));
	if (ret) {
		LOG_ERR("Failed to acquire transaction lock");
		return -EBUSY;
	}

	/* Check if length is odd */
	is_odd = (len % 2) != 0;

	k_mutex_lock(&mdev->lock, K_FOREVER);

	/* Build packet header */
	mdev->tx_buf[0] = (uint8_t)dev_addr;
	mdev->tx_buf[1] = 0x10;  /* Function code: Write Multiple Registers */
	mdev->tx_buf[2] = (uint8_t)((reg_addr >> 8) & 0xFF);
	mdev->tx_buf[3] = (uint8_t)(reg_addr & 0xFF);
	mdev->tx_buf[4] = 0x00;

	/* Adjust length based on odd or even */
	if (is_odd) {
		mdev->tx_buf[5] = (uint8_t)((len + 1) / 2);  /* Register length */
		mdev->tx_buf[6] = (uint8_t)(len + 1);        /* Byte length */
		actual_len = len + 1;
	} else {
		mdev->tx_buf[5] = (uint8_t)(len / 2);
		mdev->tx_buf[6] = (uint8_t)len;
	}

	/* Fill data region with byte-swapped values */
	if (is_odd) {
		for (num = 0; num < (len - 1); num += 2) {
			mdev->tx_buf[7 + num] = value[num + 1];
			mdev->tx_buf[8 + num] = value[num];
		}
		/* Handle the last odd byte */
		mdev->tx_buf[7 + num] = 0x00;
		mdev->tx_buf[8 + num] = value[num];
		num += 2;
	} else {
		for (num = 0; num < len; num += 2) {
			mdev->tx_buf[7 + num] = value[num + 1];
			mdev->tx_buf[8 + num] = value[num];
		}
	}

	/* Calculate and append CRC */
	crc_value = Modbus_CRC16(mdev->tx_buf, 7 + actual_len);
	mdev->tx_buf[7 + actual_len] = (uint8_t)(crc_value & 0xFF);
	mdev->tx_buf[8 + actual_len] = (uint8_t)((crc_value >> 8) & 0xFF);

	/* Log transmit data */
	LOG_DBG("Modbus TX (%d bytes):", 9 + actual_len);
	for (uint32_t i = 0; i < (9 + actual_len); i++) {
		LOG_DBG("  TX[%d]: 0x%02x", i, mdev->tx_buf[i]);
	}

	/* Store request info for callback verification */
	mdev->slave_reg_addr = reg_addr;
	mdev->rx_ready = false;
	mdev->tx_done = false;
	mdev->last_error = 0;

	/* Clear receive buffer */
	memset(mdev->rx_buf_pool[0].data, 0, MODBUS_MSG_SIZE);

	/* Reset semaphores */
	k_sem_reset(&mdev->rx_sem);
	k_sem_reset(&mdev->tx_sem);

	/* Ensure UART is clean - disable any pending operations */
	uart_rx_disable(mdev->uart_dev);
	k_msleep(10);  /* Increased delay to ensure UART state is fully clear */

	/* Transmit packet - TX MUST happen before enabling RX */
	LOG_DBG("Starting UART TX: %d bytes", 9 + actual_len);
	ret = uart_tx(mdev->uart_dev, mdev->tx_buf, 9 + actual_len, SYS_FOREVER_MS);
	if (ret) {
		LOG_ERR("Failed to start transmit: %d", ret);
		k_mutex_unlock(&mdev->lock);
		goto cleanup;
	}
	
	/* Now enable RX to catch the response */
	ret = uart_rx_enable(mdev->uart_dev, mdev->rx_buf_pool[0].data, MODBUS_MSG_SIZE, MODBUS_RX_TIMEOUT_MS);
	if (ret) {
		LOG_ERR("Failed to enable RX: %d", ret);
		k_mutex_unlock(&mdev->lock);
		goto cleanup;
	}

	k_mutex_unlock(&mdev->lock);

	/* Wait for TX complete */
	ret = k_sem_take(&mdev->tx_sem, K_MSEC(MODBUS_TX_TIMEOUT_MS));
	if (ret) {
		LOG_ERR("TX timeout waiting for completion");
		uart_rx_disable(mdev->uart_dev);
		ret = -ETIMEDOUT;
		goto cleanup;
	}

	/* Check if TX was successful */
	if (!mdev->tx_done) {
		LOG_ERR("TX was aborted");
		uart_rx_disable(mdev->uart_dev);
		ret = -EIO;
		goto cleanup;
	}
	LOG_DBG("UART TX completed successfully");

	/* Wait for response with timeout */
	LOG_DBG("Waiting for write response...");
	ret = k_sem_take(&mdev->rx_sem, K_MSEC(MODBUS_RX_TIMEOUT_MS));
	LOG_DBG("k_sem_take returned: %d", ret);
	
	if (ret) {
		LOG_ERR("Write response timeout (sem_take returned %d)", ret);
		/* Disable RX on timeout */
		uart_rx_disable(mdev->uart_dev);
		ret = -ETIMEDOUT;
		goto cleanup;
	}
	
	/* Disable RX after successful response */
	uart_rx_disable(mdev->uart_dev);
	LOG_DBG("Write response received successfully");

	/* Check for errors from callback */
	if (mdev->last_error) {
		LOG_ERR("Write operation failed: %d", mdev->last_error);
		ret = mdev->last_error;
		goto cleanup;
	}

	/* Inter-frame delay (3.5 character times) - increased for stability */
	k_msleep(MODBUS_INTER_FRAME_DELAY_MS);

	ret = 0;

cleanup:
	/* Ensure UART is fully idle before next transaction */
	if (ret != 0) {
		/* On error, ensure RX is disabled */
		uart_rx_disable(mdev->uart_dev);
	}
	
	/* Additional settling time to prevent EBUSY on next call */
	k_msleep(5);
	
	/* Release transaction semaphore */
	k_sem_give(&mdev->transaction_sem);
	return ret;
}