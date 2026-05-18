/**
* Copyright (c) 2025 Advanced Micro Devices, Inc.
*/
#ifndef __OOB_MISC_MSG_H__
#define __OOB_MISC_MSG_H__

#include <stdint.h>
#include <stdbool.h>
#include <zephyr/toolchain/gcc.h>
#include "board_config.h"

#define MEM_INFO_CMD_REQUEST       (0)

#define MEM_INFO_TYPE_UNKNOWN      (0)
#define MEM_INFO_TYPE_DDR5         (1)
#define MEM_INFO_TYPE_LPDDR5       (2)

#define I2C_TUNNEL_CMD_WRITE       (0)
#define I2C_TUNNEL_CMD_READ        (1)
#define I2C_TUNNEL_DATA_MAX_LENGTH (32)

struct mem_info_status {
	union {
		uint8_t value;
		struct {
			// Bit 0 shall be 0 on successful completion of a command operation
			// set to 1 when EC cannot get the correct memory information for LPDDR5 or DDR5.
			uint8_t err_occur : 1;

			// Bit 1 - 7 shall be reserved for future use.
			uint8_t rsvd : 7;
		} bit;
	};
} __packed;
typedef struct mem_info_status mem_info_status_t;
BUILD_ASSERT(sizeof(mem_info_status_t) == 1, "The memory info status structure length is invalid!");

struct mem_info_req {
	uint8_t cmd_code;
	uint8_t cmd_input;
} __packed;
BUILD_ASSERT(sizeof(struct mem_info_req) == 2, "The memory info request structure length is invalid!");

struct mem_info_rsp {
	mem_info_status_t rsp_ext_status;
	uint8_t rsp_data;
} __packed;
BUILD_ASSERT(sizeof(struct mem_info_rsp) == 2, "The memory info response structure length is invalid!");

struct i2c_tunnel_status {
	union {
		uint8_t value;
		struct {
			// Bit 0 shall be 0 on successful completion of a command operation and set to 1 when: Host sends read or write with length 0
			uint8_t invalid_transport : 1;

			// Bit 1 shall be 0 on successful completion of a command operation and set to 1 when: Encounter I2C bus busy
			uint8_t bus_busy : 1;

			// Bit 2 shall be 0 on successful completion of a command operation and set to 1 when: I2C command transfer incomplete
			uint8_t incomplete : 1;

			// Bit 0 shall be 0 on successful completion of a command operation and set to 1 when: I2C device address NAK
			uint8_t addr_nak : 1;

			// Bit 4 shall be 0 on successful completion of a command operation and set to 1 when:I2C data NAK
			uint8_t data_nak : 1;

			// Bit 5 shall be 0 on successful completion of a command operation and set to 1 when:I2C clock timeout
			uint8_t clock_timeout : 1;

			// Bit 6 shall be 0 on successful completion of a command operation and set to 1 when:I2C data overflow
			uint8_t overflow : 1;

			uint8_t rsvd : 1;
		} bit;
	};
} __packed;
typedef struct i2c_tunnel_status i2c_tunnel_status_t;
BUILD_ASSERT(sizeof(i2c_tunnel_status_t) == 1, "The I2C tunnel status structure length is invalid!");

struct i2c_tunnel_req {
	uint8_t cmd_code;
	uint8_t rw : 1;
	uint8_t address : 7;
	uint8_t reg;
	uint8_t length;
	uint8_t data[I2C_TUNNEL_DATA_MAX_LENGTH];
} __packed;
BUILD_ASSERT(sizeof(struct i2c_tunnel_req) == 36, "The I2C tunnel request structure length is invalid!");

struct i2c_tunnel_rsp {
	i2c_tunnel_status_t rsp_ext_status;
	uint8_t data[I2C_TUNNEL_DATA_MAX_LENGTH];
} __packed;
BUILD_ASSERT(sizeof(struct i2c_tunnel_rsp) == 33, "The I2C tunnel response structure length is invalid!");

struct misc_msg_req {
	union {
		struct mem_info_req mem_info;
		struct i2c_tunnel_req i2c_tunnel;
	};
} __packed;
BUILD_ASSERT(sizeof(struct misc_msg_req) <= 36, "The MISC message request structure length is invalid!");

struct misc_msg_rsp {
	union {
		struct mem_info_rsp mem_info;
		struct i2c_tunnel_rsp i2c_tunnel;
	};
} __packed;
BUILD_ASSERT(sizeof(struct misc_msg_rsp) <= 36, "The MISC message response structure length is invalid!");

/*****************************************************************************
 * Function name:   oob_misc_memory_info_proc
 * Description:     Process memory info request
 * @param           req  - Input memory info request
 * @param           rsp  - Output memory info response
 * @param           len - Pointer to output message length
 * @return          0 on success, negative on error
 *****************************************************************************/
 int oob_misc_memory_info_proc(struct misc_msg_req *req, struct misc_msg_rsp *rsp, uint16_t *len);

#if CONFIG_ACPI_DAC_PWR
/*****************************************************************************
 * Function name:   oob_misc_i2c_tunnel_proc
 * Description:     Process I2C tunnel request
 * @param           req  - Input I2C tunnel request
 * @param           rsp  - Output I2C tunnel response
 * @param           len - Pointer to output message length
 * @return          0 on success, negative on error
 *****************************************************************************/
 int oob_misc_i2c_tunnel_proc(struct misc_msg_req *req, struct misc_msg_rsp *rsp, uint16_t *len);
#endif

 /*****************************************************************************
 * Function name:   oob_misc_mem_info_type_set
 * Description:     Set the memory info type
 * @param           type - Memory info type (MEM_INFO_TYPE_DDR5 or MEM_INFO_TYPE_LPDDR5)
 * @return          0 successfully, negative on error
 *****************************************************************************/
 int oob_misc_mem_info_type_set(uint8_t type);

#endif

