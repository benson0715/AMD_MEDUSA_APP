/*****************************************************************************
 * Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
 ******************************************************************************
 */

#ifndef AMD_CRB_MODBUSRTU_DRIVER_H_
#define AMD_CRB_MODBUSRTU_DRIVER_H_

#include <stdint.h>

/* Modbus RTU configuration constants */
#define UART_NODE UART_1
// Define necessary macros if not defined in header file
#ifndef MODBUS_MIN_FRAME_SIZE
#define MODBUS_MIN_FRAME_SIZE 8  // Minimum Modbus RTU frame length (addr+func+data+CRC)
#endif

#ifndef MODBUS_MAX_FRAME_SIZE
#define MODBUS_MAX_FRAME_SIZE 2560// Maximum Modbus RTU frame length (for 04 func: 1+1+252+2=256)
#endif

#ifndef MODBUS_MSG_SIZE
#define MODBUS_MSG_SIZE 2560      // Maximum Modbus RTU frame length
#endif

#ifndef MODBUS_MAX_REGISTERS
#define MODBUS_MAX_REGISTERS 4096// Maximum number of registers
#endif

#ifndef MODBUS_RX_TIMEOUT_MS
#define MODBUS_RX_TIMEOUT_MS 100 // RX timeout in milliseconds
#endif

#ifndef MODBUS_TX_TIMEOUT_MS
#define MODBUS_TX_TIMEOUT_MS 1000  // TX timeout in milliseconds
#endif

#ifndef MODBUS_INTER_FRAME_DELAY_MS
#define MODBUS_INTER_FRAME_DELAY_MS 2 // Inter-frame delay (3.5 character times)
#endif

// Buffer pool configuration (double buffer for DMA RX)
#define RX_BUF_POOL_SIZE 2       // Number of RX buffers (2 = double buffer)
#define RX_BUF_LEN MODBUS_MSG_SIZE// Length of each RX buffer

// RX buffer structure (for buffer pool management)
typedef struct {
	uint8_t data[RX_BUF_LEN];   // Buffer data
	bool in_use;                // Buffer status: true = in use by DMA, false = free
} rx_buf_t;

// Frame reception state (for cross-buffer concatenation)
typedef struct {
	uint8_t frame_buf[MODBUS_MAX_FRAME_SIZE]; // Complete frame buffer
	uint16_t frame_len;                       // Current received frame length
	bool frame_started;                       // Flag indicating if a frame reception has started
	bool frame_complete;                      // Flag indicating if the frame is complete
	uint16_t expected_len;                    // Expected frame length (derived from protocol fields)
} frame_rx_state_t;

// Modbus RTU device structure
struct modbusrtu_device {
	const struct device *uart_dev;
	struct k_mutex lock;
	struct k_sem rx_sem;
	struct k_sem tx_sem;
	struct k_sem transaction_sem;
	bool rx_ready;                             // Flag indicating RX data is ready
	bool tx_done;                              // Flag indicating TX is completed
	uint8_t slave_addr;                        // Current slave address for communication (frame filtering)
	uint16_t slave_reg_addr;                   // Current requested register address
	uint16_t request_len;
	uint8_t slave_data_len;                    // Data length of slave response
	int last_error;                            // Last error code
	uint8_t tx_buf[MODBUS_MAX_FRAME_SIZE];     // TX data buffer
	uint16_t registers[MODBUS_MAX_REGISTERS];  // Register storage array
	// RX buffer pool (for DMA continuous reception)
	rx_buf_t rx_buf_pool[RX_BUF_POOL_SIZE];    // RX buffer pool
	bool rx_enabled;                           // Flag indicating RX is enabled
	// Frame reception state (for cross-buffer concatenation)
	frame_rx_state_t frame_state;              // Frame reception state management

};



extern struct modbusrtu_device modbus_dev;
/**
 * @brief Initialize the Modbus RTU driver
 *
 * @return 0 if successful, negative error code otherwise
 *         -ENODEV: UART device not ready
 *         -EINVAL: Failed to set callback
 */
int amd_crb_modbusrtu_init(void);

/**
 * @brief Write multiple holding registers
 *
 * @param dev_addr Modbus device address (1-247)
 * @param reg_addr Starting register address
 * @param value    Pointer to data buffer
 * @param len      Data length in bytes
 *
 * @return 0 if successful, negative error code otherwise
 *         -EINVAL: Invalid parameters (NULL pointer, invalid address/length)
 *         -ERANGE: Register address overflow
 */
int amd_crb_modbusrtu_write(uint16_t dev_addr, uint16_t reg_addr,
			    uint8_t *value, uint32_t len);

/**
 * @brief Read holding registers (Function Code 0x04 - Read Input Registers)
 *
 * @param dev_addr Modbus device address (1-247)
 * @param reg_addr Starting register address
 * @param values   Pointer to buffer to store register values (can be NULL)
 * @param length   Number of registers to read (1-125)
 *
 * @return 0 if successful, negative error code otherwise
 *         -EINVAL: Invalid parameters (invalid address/length)
 *         -ERANGE: Register address overflow
 *         -ETIMEDOUT: Response timeout
 *         -EBADMSG: CRC mismatch
 *         -EIO: Modbus exception response
 *
 * @note This function blocks until response is received or timeout occurs.
 *       Current implementation uses function code 0x04 (Read Input Registers).
 */
int amd_crb_modbusrtu_read(uint16_t dev_addr, uint16_t reg_addr, uint16_t *values, uint16_t length);

/**
 * @brief Read holding registers (Function Code 0x03 - Read Holding Registers)
 *
 * @param dev_addr Modbus device address (1-247)
 * @param reg_addr Starting register address
 * @param values   Pointer to buffer to store register values (can be NULL)
 * @param length   Number of registers to read (1-125)
 *
 * @return 0 if successful, negative error code otherwise
 *         -EINVAL: Invalid parameters (invalid address/length)
 *         -ERANGE: Register address overflow
 *         -ETIMEDOUT: Response timeout
 *         -EBADMSG: CRC mismatch
 *         -EIO: Modbus exception response
 *
 * @note This function blocks until response is received or timeout occurs.
 *       Uses Modbus function code 0x03 (Read Holding Registers).
 */
int amd_crb_modbusrtu_read_holding_registers(uint16_t dev_addr, uint16_t reg_addr, uint16_t *values, uint16_t length);

#endif /* AMD_CRB_MODBUSRTU_DRIVER_H_ */

