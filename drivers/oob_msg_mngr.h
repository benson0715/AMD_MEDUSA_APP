/**
* Copyright (c) 2025 Advanced Micro Devices, Inc.
*/

/**
 * @brief OOB Message Manager APIs.
 *
 */
#ifndef __OOB_MSG_MNGR_H_
#define __OOB_MSG_MNGR_H_

#include <stdint.h>
#include <stdbool.h>
#include <zephyr/toolchain/gcc.h>
#include "oob_rpmc_msg.h"
#include "oob_misc_msg.h"

#define OOB_BYTES_PER_MSG_MAX      (64)

#define SMBUS_EC_SLAVE_ADDRESS     (0x07)
#define SMBUS_SOC_SLAVE_ADDRESS    (0x08)
#define SMBUS_ACCESS_W             (0)
#define SMBUS_ACCESS_R             (1)
#define SMBUS_CMD_MCTP             (0x0F)
#define SMBUS_BYTES                (3)
#define SMBUS_MCTP_BYTES           (5)
#define SMBUS_PEC_BYTES            (1)
#define SMBUS_CTRL_BYTES           (SMBUS_BYTES + SMBUS_MCTP_BYTES + SMBUS_PEC_BYTES)
#define SMBUS_MSG_MAX_BYTES        (OOB_BYTES_PER_MSG_MAX - SMBUS_CTRL_BYTES)

#define MCTP_MSG_MAX_BYTES         (110)
#define MCTP_BYTES_START           (9)
#define MCTP_ENABLED               (1)
#define MCTP_MSG_TAG               (0)
#define MCTP_OWNER_TAG             (1)
#define MCTP_PACKAGE_SEQUENCE_MAX  (3)
#define MCTP_EOM_ENABLED           (1)
#define MCTP_SOM_ENABLED           (1)
#define MCTP_EC_EID                (0x40)
#define MCTP_SOC_EID               (0x50)
#define MCTP_HDR_VERSION           (1)
#define MCTP_RSVD                  (0)

#define MCTP_PACKAGE_MAX           (119) // Caculated based on the 128 length setting.

#define MSG_RPMC_TYPE              (0x7D)
#define MSG_MEMORY_INFO_TYPE       (0x60)
#define MSG_I2C_TUNNE_TYPE         (0x61)

/* Message fragmentation states for MCTP packets */
#define ESOM_NOMORE  0 // Integrated data (single packet)
#define ESOM_START   1 // Broken-up data: The first fragment
#define ESOM_ONGOING 2 // Broken-up data: Continuing fragment
#define ESOM_END     3 // Broken-up data: The last fragment

#define MSG_MNGR_BUFFER_LENGTH 128  // Input message buffer size

/**
 * @brief The SMBus header protocol structure.
 */
struct smbus_header {
	uint8_t access : 1; // Write = 0
	uint8_t dst_addr : 7; // EC SMB Addr = 0x07, SoC SMB Addr = 0x08
	uint8_t cmd; // MCTP = 0x0F

	// If the MCTP packet payload length (starting with byte 9) is 64 bytes,
	// the value in the Byte Count field would be 69. (The count of 69 accounts for 64 bytes of
	// MCTP packet payload plus the five bytes [bytes 4 through 8,
	// inclusive] that comprise the bytes of the SMBus-specific header and
	// MCTP header that follow the Byte Count field.)
	uint8_t bytes;
} __packed;
BUILD_ASSERT(sizeof(struct smbus_header) == 3, "The SMBus header structure is invalid!");

/**
 * @brief The MCTP header protocol structure.
 */
struct mctp_header {
	uint8_t mctp_en : 1; // MCTP over SMBus = 1
	uint8_t src_addr : 7; // EC SMB Addr = 0x07, SoC SMB Addr = 0x08

	uint32_t version : 4; // Ver = 0x1
	uint32_t rsvd_4 : 4; // Reserved = 0
	uint32_t dst_eid : 8; // EC EID = 0x40, SoC EID = 0x50
	uint32_t src_eid : 8; // EC EID = 0x40, SoC EID = 0x50
	uint32_t msg_tag : 3; // Message Tag = 0
	uint32_t owner_tag : 1; // Tag Owner = 1
	uint32_t pkt_seq : 2; // Packet sequence number
	uint32_t eom : 1; // End of Message
	uint32_t som : 1; // Start of Message
} __packed;
BUILD_ASSERT(sizeof(struct mctp_header) == 5, "The MCTP header structure is invalid!");

/**
 * @brief The message header protocol structure.
 */
struct msg_header {
	struct smbus_header smbus;
	struct mctp_header mctp;
} __packed;
BUILD_ASSERT(sizeof(struct msg_header) == 8, "The combination message header structure is invalid!");

typedef uint8_t smbus_pec_t; // End of the mctp data format
BUILD_ASSERT(sizeof(smbus_pec_t) == 1, "The SMBus PEC symbol length is invalid!");

struct msg_info {
	uint8_t type : 7; // Message Type 0x7D	rpmc_header_t header;
	uint8_t ic_en : 1; // No integrity check = 0
} __packed;
BUILD_ASSERT(sizeof(struct msg_info) == 1, "The Message command length is invalid!");

/**
 * @brief The receive message structure.
 */
struct msg_in {
	struct msg_info info;

	union {
		struct rc_msg_req rpmc;
		struct misc_msg_req misc;
	};
} __packed;
typedef struct msg_in msg_in_t;
BUILD_ASSERT(sizeof(struct msg_in) <= MCTP_MSG_MAX_BYTES, "The receive message length is invalid!");

/**
 * @brief The transmit message structure.
 */
struct msg_out {
	struct msg_info info;

	union {
		struct rc_msg_rsp rpmc;
		struct misc_msg_rsp misc;
	};
} __packed;
typedef struct msg_out msg_out_t;
BUILD_ASSERT(sizeof(struct msg_out) <= MCTP_MSG_MAX_BYTES, "The subsmit message length is invalid!");

/**
 * @brief The OOB message buffer structure.
 */
struct oob_msg_buf {
	struct msg_header head;

	union {
		uint8_t trans_buf[SMBUS_MSG_MAX_BYTES];
		struct msg_in in;
		struct msg_out out;
	};
} __packed;
typedef struct oob_msg_buf oob_msg_buf_t;
BUILD_ASSERT(sizeof(struct oob_msg_buf) <= MSG_MNGR_BUFFER_LENGTH, "The oob message length is invalid!");

/*****************************************************************************
 * Function name:   oob_msg_mngr_proc
 * Description:     Main OOB message processing function with fragmentation support
 * @param           pInData  - Input data buffer containing OOB message
 * @param           InLen    - Input data buffer size
 * @param           pOutData - Output data buffer for response
 * @param           pOutLen  - Pointer to output data buffer size
 * @return          0 on success, negative on error
 * @note            Handles MCTP fragmentation and routes to service processors
 *****************************************************************************/
int oob_msg_mngr_proc(void *pInData, uint8_t InLen, void *pOutData, uint16_t *pOutLen);

/*****************************************************************************
 * Function name:   oob_msg_mngr_proc_remainning
 * Description:     Process remaining fragmented OOB message packets
 * @param           pOutBuf - Output data buffer for remaining message fragment
 * @param           pOutLen - Pointer to output data buffer size
 * @return          0 on success, negative on error
 * @note            Handles subsequent MCTP packet fragments after initial message
 *****************************************************************************/
int oob_msg_mngr_proc_remainning(void *pOutBuf, uint16_t *pOutLen);

/*****************************************************************************
 * Function name:   oob_msg_mngr_postpne_proc
 * Description:     Handle postponed OOB message operations
 * @param           None
 * @return          None
 * @note            Processes deferred RPMC ROM write operations
 *****************************************************************************/
void oob_msg_mngr_postpne_proc(void);

/*****************************************************************************
 * Function name:   oob_msg_mngr_init_proc
 * Description:     Initialize OOB message manager and related services
 * @param           None
 * @return          None
 * @note            Sets up RPMC services and underlying hardware
 *****************************************************************************/
void oob_msg_mngr_init_proc(void);


#endif
