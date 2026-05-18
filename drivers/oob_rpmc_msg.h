/**
* Copyright (c) 2025 Advanced Micro Devices, Inc.
*/

#ifndef __OOB_RPMC_MSG_H__
#define __OOB_RPMC_MSG_H__

#include <stdint.h>
#include <stdbool.h>
#include <zephyr/toolchain/gcc.h>
#include "board_config.h"

#ifndef RPMC_ROM_ACCESS_POSTPONE
#define RPMC_ROM_ACCESS_POSTPONE (0)
#endif

#define MSG_INFO_RPMC_IC_EN  (1u)
#define MSG_INFO_RPMC_IC_DIS (0u)
#define MSG_INFO_RPMC_TYPE   (0x7Du)

#define RPMC_PARAM_CMD         (0x04u)
#define RPMC_PARAM_VERSION     (0u)
#define RPMC_PARAM_MC_ADDR_NUM (RPMC_ROM_MC_NUM)
#define RPMC_PARAM_SUPPORTED   (1u)
#define RPMC_PARAM_UNSUPPORTED (0u)
#define RPMC_PARAM_FAR_2V0_EN  (1u)
#define RPMC_PARAM_FAR_2V0_DIS (0u)

#define RPMC_PARAM_FAR_2V0_MC_MINI_NUM (8u)

#define RPMC_PARAM_RESPONSE_TIMEOUT_10_MS   (0u)
#define RPMC_PARAM_RESPONSE_TIMEOUT_20_MS   (1u)
#define RPMC_PARAM_RESPONSE_TIMEOUT_50_MS   (2u)
#define RPMC_PARAM_RESPONSE_TIMEOUT_100_MS  (3u)
#define RPMC_PARAM_RESPONSE_TIMEOUT_200_MS  (4u)
#define RPMC_PARAM_RESPONSE_TIMEOUT_500_MS  (5u)
#define RPMC_PARAM_RESPONSE_TIMEOUT_1000_MS (6u)
#define RPMC_PARAM_RESPONSE_TIMEOUT_2000_MS (7u)

#define RPMC_SHA256_MC_ALGID   (0)
#define RPMC_SHA384_MC_ALGID   (1)
#define RPMC_SHA3_256_MC_ALGID (2)
#define RPMC_SHA3_384_MC_ALGID (3)
#define RPMC_SM3_256_MC_ALGID  (4)
#define RPMC_AIG_ID_NUM        (5)

#define RPMC_AIG_ID_REVERSE(s)               ((RPMC_AIG_ID_NUM - 1)-(s))
#define RPMC_AIG_ID_BIT(s)                   (BIT(RPMC_AIG_ID_REVERSE(s)))

#define RPMC_SHA256_MC_ALGID_BIT              RPMC_AIG_ID_BIT(RPMC_SHA256_MC_ALGID)
#define RPMC_SHA384_MC_ALGID_BIT              RPMC_AIG_ID_BIT(RPMC_SHA384_MC_ALGID)
#define RPMC_SHA3_256_MC_ALGID_BIT            RPMC_AIG_ID_BIT(RPMC_SHA3_256_MC_ALGID)
#define RPMC_SHA3_384_MC_ALGID_BIT            RPMC_AIG_ID_BIT(RPMC_SHA3_384_MC_ALGID)
#define RPMC_SM3_256_MC_ALGID_BIT             RPMC_AIG_ID_BIT(RPMC_SM3_256_MC_ALGID)

#define RPMC_PARAM_ALGORITHM_SHA256_SUPPORT   (RPMC_SHA256_MC_ALGID_BIT)
#define RPMC_PARAM_ALGORITHM_SHA384_SUPPORT   (RPMC_SHA384_MC_ALGID_BIT)
#define RPMC_PARAM_ALGORITHM_SHA3_256_SUPPORT (RPMC_SHA3_256_MC_ALGID_BIT)
#define RPMC_PARAM_ALGORITHM_SHA3_384_SUPPORT (RPMC_SHA3_384_MC_ALGID_BIT)
#define RPMC_PARAM_ALGORITHM_SM3_256_SUPPORT  (RPMC_SM3_256_MC_ALGID_BIT)

#define RPMC_ECAIG_DRIVER_SUPPORT_BITS        (RPMC_SHA256_MC_ALGID_BIT | RPMC_SHA3_384_MC_ALGID_BIT)

/* Rate of update = 5 x (2 ^ RPMC_UpdateRate) seconds  */
#define RPMC_PARAM_UPDATE_RATE_5_SEC  (0u)
#define RPMC_PARAM_UPDATE_RATE_10_SEC (1u)
#define RPMC_PARAM_UPDATE_RATE_20_SEC (2u)
#define RPMC_PARAM_UPDATE_RATE_40_SEC (3u)
#define RPMC_PARAM_UPDATE_RATE_80_SEC (4u)

#define RPMC_CMD_READ_PARAMETER_CODE  (0x04u)
#define RPMC_CMD_WRITE_ROOT_KEY_CODE  (0x0u)
#define RPMC_CMD_UPDATE_HMAC_KEY_CODE (0x01u)
#define RPMC_CMD_INCREASE_MC_CODE     (0x02u)
#define RPMC_CMD_REQUEST_MC_CODE      (0x03u)

#define RPMC_SHA256_LENGTH            (32u)
#define RPMC_SHA384_LENGTH            (48u)
#define RPMC_SHA_LENGTH               (RPMC_SHA384_LENGTH)

#define RPMC_ROOTKEY_SHA256_LENGTH    (RPMC_SHA256_LENGTH)
#define RPMC_HMACKEY_SHA256_LENGTH    (RPMC_SHA256_LENGTH)
#define RPMC_ROOTKEY_SHA384_LENGTH    (RPMC_SHA384_LENGTH)
#define RPMC_HMACKEY_SHA384_LENGTH    (RPMC_SHA384_LENGTH)

#define RPMC_SIG_LENGTH(s)            ((s) == RPMC_SHA256_LENGTH ? RPMC_SHA256_LENGTH : RPMC_SHA384_LENGTH)

#define KDV_ARR_256B                                    \
	{                                                   \
		0x47, 0x8b, 0xe2, 0xbf, 0x97, 0x06, 0x08, 0x78, \
		0x1d, 0x14, 0x52, 0x41, 0x6c, 0xef, 0x90, 0x5d, \
		0x52, 0xdf, 0x0f, 0x7f, 0x42, 0x6d, 0x6b, 0xfc, \
		0xa3, 0xf4, 0x12, 0xc0, 0x20, 0xa2, 0x01, 0xea  \
	}
#define KDV_ARR_384B                                    \
	{                                                   \
		0xea, 0xdb, 0x27, 0xc8, 0x27, 0xc5, 0x17, 0x4c, \
		0x3b, 0xa5, 0x17, 0xde, 0x09, 0x00, 0xa5, 0xb0, \
		0x2b, 0xfe, 0x13, 0x8a, 0x5a, 0x08, 0x4b, 0x87, \
		0xac, 0x7f, 0xbb, 0x1a, 0x1b, 0xae, 0x4e, 0x71, \
		0x6e, 0xe1, 0xa5, 0xeb, 0x39, 0xde, 0x19, 0x7a, \
		0x16, 0x0f, 0x99, 0x7e, 0x4c, 0x43, 0xa8, 0xa9  \
	}

#define RPMC_KDV_ROOTKEY_AIGID_0_ARR KDV_ARR_256B
#define RPMC_KDV_ROOTKEY_AIGID_1_ARR KDV_ARR_384B
#define RPMC_KDV_ROOTKEY_AIGID_2_ARR KDV_ARR_256B
#define RPMC_KDV_ROOTKEY_AIGID_3_ARR KDV_ARR_384B
#define RPMC_KDV_ROOTKEY_AIGID_4_ARR KDV_ARR_256B

#define RPMC_UDID_SIZE       (32u)
#define RPMC_MC_TAG_SIZE     (12u)
#define RPMC_MC_VALUE_SIZE   (4u)
#define RPMC_MC_KEYDATA_SIZE (4u)
#define RPMC_KDV_KEYDATA_ARR   \
	{                          \
		0xf3, 0xa9, 0x33, 0x1b \
	}

#define RC_ST_UN           (0u) // Uninitialized
#define RC_ST_IN           (1u) // Initialized
#define RC_ST_DEACT        (2u) // Deactivated
#define RC_ST_RSVD         (3u) // Reserved
	
#define RC_DATA_UPDATED_RK  (1u)
#define RC_DATA_UPDATED_CT  (2u)

struct rc_hsha_api {
	uint8_t type;

	/**
	 * @brief Initialize the HMAC engine.
	 * @return 0 on success, negative on error.
	 */
	 int (*hmac_init)(void);

	 /**
	  * @brief Free the HMAC engine.
	  * @return 0 on success, negative on error.
	  */
	 int (*hmac_deinit)(void);
 
	 /**
	  * @brief Calculate HMAC for a message.
	  * @param msg Pointer to input message buffer.
	  * @param msg_len Length of the input message.
	  * @param key Pointer to HMAC key buffer.
	  * @param key_len Length of the HMAC key.
	  * @param out Pointer to output buffer for the HMAC result.
	  * @param out_len Length of the output buffer (should match expected HMAC size).
	  * @return 0 on success, negative on error.
	  */
	 int (*hmac_cal)(const uint8_t *msg, uint8_t msg_len, const uint8_t *key, uint8_t key_len, uint8_t *out, uint8_t out_len);
 
	 /**
	  * @brief Calculate SHA256 hash for a message.
	  * @param msg Pointer to input message buffer.
	  * @param msg_len Length of the input message.
	  * @param out Pointer to output buffer for the SHA256 result.
	  * @param out_len Length of the output buffer (should match expected SHA256 size).
	  * @return 0 on success, negative on error.
	  */
	 int (*sha_cal)(uint8_t *msg, uint8_t msg_len, uint8_t *out, uint8_t out_len);
};

struct rc_api {
	/**
	 * @brief Initialize the HMAC engine.
	 * @return 0 on success, negative on error.
	 */
	int (*hmac_init)(void);

	/**
	 * @brief Free the HMAC engine.
	 * @return 0 on success, negative on error.
	 */
	int (*hmac_deinit)(void);

	/**
	 * @brief Calculate HMAC for a message.
	 * @param msg Pointer to input message buffer.
	 * @param msg_len Length of the input message.
	 * @param key Pointer to HMAC key buffer.
	 * @param key_len Length of the HMAC key.
	 * @param out Pointer to output buffer for the HMAC result.
	 * @param out_len Length of the output buffer (should match expected HMAC size).
	 * @return 0 on success, negative on error.
	 */
	int (*hmac_cal)(const uint8_t *msg, uint8_t msg_len, const uint8_t *key, uint8_t key_len, uint8_t *out, uint8_t out_len);

	/**
	 * @brief Calculate SHA256 hash for a message.
	 * @param msg Pointer to input message buffer.
	 * @param msg_len Length of the input message.
	 * @param out Pointer to output buffer for the SHA256 result.
	 * @param out_len Length of the output buffer (should match expected SHA256 size).
	 * @return 0 on success, negative on error.
	 */
	int (*sha_cal)(uint8_t *msg, uint8_t msg_len, uint8_t *out, uint8_t out_len);

	/**
	 * @brief Initialize ROM for RPMC data storage.
	 * @param mc_num Number of monotonic counters.
	 * @return 0 on success, negative on error.
	 */
	int (*rom_init)(uint8_t mc_num);

	/**
	 * @brief Access (read/write) the root key in ROM.
	 * @param mc_addr Monotonic counter address.
	 * @param data Pointer to data buffer for root key.
	 * @param len Length of the data buffer.
	 * @param opcode Operation code (true for write, false for read).
	 * @param bus Bus type (e.g., SPI).
	 * @return 0 on success, negative on error.
	 */
	int (*rom_root_key)(uint8_t mc_addr, uint8_t *data, uint8_t len, bool opcode, uint8_t bus);

	/**
	 * @brief Access (read/write) the monotonic counter value in ROM.
	 * @param mc_addr Monotonic counter address.
	 * @param data Pointer to data buffer for counter value.
	 * @param len Length of the data buffer.
	 * @param opcode Operation code (true for write, false for read).
	 * @param bus Bus type (e.g., SPI).
	 * @return 0 on success, negative on error.
	 */
	int (*rom_counter_value)(uint8_t mc_addr, uint8_t *data, uint8_t len, bool opcode, uint8_t bus);

	/**
	 * @brief Access (read/write) the unique ID in ROM.
	 * @param mc_addr Monotonic counter address.
	 * @param pData Pointer to data buffer for unique ID.
	 * @param len Length of the data buffer.
	 * @return 0 on success, negative on error.
	 */
	int (*unique_id_get)(uint8_t mc_addr, uint8_t* pData, uint8_t len);
};

struct rc_status {
	union {
		uint8_t value;
		struct {
			// Bit 0 shall be 0 on successful completion of a command operation and set to 1 when:
			// Incorrect command size received from transport layer handler. Expected command size is calculated based on Key size and Signature size associated to CMD_MC_AlgID.
			uint8_t err_size : 1;

			// Bit 1 shall be 0 on successful completion of a command operation and set to 1 when:
			// 1. CMD_OpCode, CMD_Type, CMD_MC_Addr or CMD_MC_AlgID is out of supported range.
			// 2. CMD_MC_AlgID does not match current algorithm ID assigned to the monotonic counter, except for owner changing the MC_AlgID and MC_RootKey.
			uint8_t err_range : 1;

			// Bit 2 shall be 0 on successful completion of a command operation and set to 1 when:
			// 1: Signature verification fails .
			uint8_t err_signature : 1;

			// Bit 3 shall be 0 as reserved for future use.
			uint8_t err_rsvd_1 : 1;

			// Bit 4 shall be 0 on successful completion of a command operation and set to 1 when:
			// 1: Monotonic counter operational state registers prevent this command to be executed.
			uint8_t err_state : 1;

			// Bit 5 shall be 0 on successful completion of a command operation and set to 1 when:
			// 1: Counter value in command Input does not match current monotonic counter value.
			uint8_t err_mc_value : 1;

			// Bit 6 shall be 0 as reserved for future use.
			uint8_t err_rsvd_2 : 1;

			// 1: Successful completion. All other bits shall be 0s.
			// 0: Error reported. Refer to bit 6 – 0 integrity checks status.
			uint8_t err_no : 1;
		} bit;
	};
} __packed;
typedef struct rc_status rc_status_t;
BUILD_ASSERT(sizeof(rc_status_t) == 1, "The RPMC status structure length is invalid!");

struct rc_header {
	uint8_t cmd_code;
	uint8_t cmd_mc_addr;
	uint8_t cmd_mc_aid;
} __packed;
BUILD_ASSERT(sizeof(struct rc_header) == 3, "The RPMC header structure length is invalid!");

struct hmac_sha256_data {
	uint8_t hsha_data[RPMC_HMACKEY_SHA256_LENGTH];
} __packed;
BUILD_ASSERT(sizeof(struct hmac_sha256_data) == 32, "The RPMC HMAC SHA256 data structure length is invalid!");

struct hmac_sha384_data {
	uint8_t hsha_data[RPMC_HMACKEY_SHA384_LENGTH];
} __packed;
BUILD_ASSERT(sizeof(struct hmac_sha384_data) == 48, "The RPMC HMAC SHA384 data structure length is invalid!");

struct hmac_sha {
	union {
		struct hmac_sha256_data b256;
		struct hmac_sha384_data b384;
	};
} __packed;

struct read_param {
	uint8_t empty_rsvd;
} __packed;

struct wt_k_data2 {
	uint8_t mc_root_key[RPMC_ROOTKEY_SHA256_LENGTH];
	struct hmac_sha cmd_sig;
} __packed;
BUILD_ASSERT(sizeof(struct wt_k_data2) == 80, "The RPMC write root key structure length is invalid!");

struct wt_k_data3 {
	uint8_t mc_root_key[RPMC_ROOTKEY_SHA384_LENGTH];
	struct hmac_sha cmd_sig;
} __packed;
BUILD_ASSERT(sizeof(struct wt_k_data3) == 96, "The RPMC write root key structure length is invalid!");

struct write_root_k {
	union {
		struct wt_k_data2 b256;
		struct wt_k_data3 b384;
	};
} __packed;
BUILD_ASSERT(sizeof(struct write_root_k) == 96, "The RPMC write root key structure length is invalid!");

struct update_hmac_k {
	uint8_t mc_keyData[RPMC_MC_KEYDATA_SIZE];
	struct hmac_sha cmd_sig;
} __packed;
BUILD_ASSERT(sizeof(struct update_hmac_k) == 52, "The RPMC update hmac key structure length is invalid!");

struct request_mc {
	uint8_t mc_tag[RPMC_MC_TAG_SIZE];
	struct hmac_sha cmd_sig;
} __packed;
BUILD_ASSERT(sizeof(struct request_mc) == 60, "The RPMC request monotonic counter structure length is invalid!");

struct increase_mc {
	uint8_t mc_val[RPMC_MC_VALUE_SIZE];
	struct hmac_sha cmd_sig;
} __packed;
BUILD_ASSERT(sizeof(struct increase_mc) == 52, "The RPMC increase monotonic counter structure length is invalid!");

struct rc_msg_req {
	struct rc_header header;
	union {
		struct read_param rd_param;
		struct write_root_k rt_key;
		struct update_hmac_k h_key;
		struct request_mc req_mc;
		struct increase_mc inc_mc;
	};
} __packed;
BUILD_ASSERT(sizeof(struct rc_msg_req) <= 119, "The RPMC message request structure length is invalid!");

struct rc_rsp {
	uint8_t mc_counter_addr;
	struct rc_status ext_status;
} __packed;
typedef struct rc_rsp rc_rsp_t;
BUILD_ASSERT(sizeof(struct rc_rsp) == 2, "The RPMC message response status structure length is invalid!");
#define LENGTH_DATA_RC_RSP  (sizeof(struct rc_rsp))

struct rpmc_param {
	uint32_t update_rate : 4;
	uint32_t spec_ver : 4;
	uint32_t mc_max_addr : 8;
	uint32_t hash_support : 8;
	uint32_t timeout : 3;
	uint32_t rpmc_support : 1;
	uint32_t far_2v0_en : 1;
	uint32_t rsvd : 3;
} __packed;
BUILD_ASSERT(sizeof(struct rpmc_param) == 4, "The RPMC read parameter structure length is invalid!");

struct read_param_rsp {
	uint8_t udid[RPMC_UDID_SIZE];
	struct rpmc_param param;
} __packed;
BUILD_ASSERT(sizeof(struct read_param_rsp) == 36, "The RPMC read parameter response structure length is invalid!");
#define LENGTH_DATA_RD_PARAM_RSP  sizeof(struct read_param_rsp)

struct write_root_k_rsp {
	struct hmac_sha sig;
} __packed;	
BUILD_ASSERT(sizeof(struct write_root_k_rsp) == 48, "The RPMC write root key response structure length is invalid!");
#define LENGTH_DATA_WT_K_RSP(sha_sz)  (RPMC_SIG_LENGTH(sha_sz))

struct update_hmac_k_rsp {
	struct hmac_sha sig;
} __packed;
BUILD_ASSERT(sizeof(struct update_hmac_k_rsp) == 48, "The RPMC update hmac key response structure length is invalid!");
#define LENGTH_DATA_UP_HK_RSP(sha_sz)  (RPMC_SIG_LENGTH(sha_sz))

struct request_mc_rsp {
	uint8_t mc_tag[RPMC_MC_TAG_SIZE];
	uint8_t mc_val[RPMC_MC_VALUE_SIZE];
	struct hmac_sha sig;
} __packed;
BUILD_ASSERT(sizeof(struct request_mc_rsp) == (RPMC_MC_TAG_SIZE + RPMC_MC_VALUE_SIZE + 48), "The RPMC response data structure length is invalid!");
#define LENGTH_DATA_RQ_MC_RSP(sha_sz)  (RPMC_SIG_LENGTH(sha_sz) + RPMC_MC_TAG_SIZE + RPMC_MC_VALUE_SIZE)

struct increase_mc_rsp {
	struct hmac_sha sig;
} __packed;
BUILD_ASSERT(sizeof(struct increase_mc_rsp) == 48, "The RPMC increase counter response structure length is invalid!");
#define LENGTH_DATA_INC_MC_RSP(sha_sz)  (RPMC_SIG_LENGTH(sha_sz))

struct rc_msg_rsp {
	struct rc_rsp response;
	union {
		struct read_param_rsp rd_param;
		struct write_root_k_rsp rt_key;
		struct update_hmac_k_rsp h_key;
		struct request_mc_rsp req_mc;
		struct increase_mc_rsp inc_mc;
	};
} __packed;
BUILD_ASSERT(sizeof(struct rc_msg_rsp) <= 119, "The RPMC message response structure length is invalid!");

struct rc_dev_data {
	uint8_t root_key[RPMC_SHA_LENGTH];
	uint8_t root_key_sz;
	uint8_t hmac_key[RPMC_SHA_LENGTH];
	uint8_t hmac_key_sz;
	uint8_t key_data[RPMC_MC_KEYDATA_SIZE];
	uint8_t udid[RPMC_UDID_SIZE];

	uint8_t st_rkr;
	uint8_t st_hkr;
	uint8_t st_mc;
	uint32_t mc_value;
	
	uint8_t updated;
	bool inited;
};

struct rc_dev_context {
	struct rc_hsha_api *hsha_api;
	uint8_t hsha_api_num;

	struct rc_api *api_fn;
	struct rc_dev_data *data;
	uint8_t mc_num;

	uint8_t aig_id;
	uint8_t *kdv_key;
	uint8_t kdv_key_sz;
	uint8_t *kdv_keydata;
	uint8_t kdv_keydata_sz;
};
typedef struct rc_dev_context rc_dev_context_t;

/**
 * Function name:   rc_algorithm_supported
 * Description:     Check if RPMC algorithm is supported
 * @param           alg_id - RPMC algorithm ID to check
 * @return          0 if supported, negative if not supported
 *****************************************************************************/
int rc_algorithm_supported(uint8_t alg_id);

/**
 * rc_read_parameter_service
 *
 * @param [out]  cmd: RPMC message supported instructions.
 *
 * @return value 0 indicates supported, otherwise is not correct.
 */
int rc_code_supported(uint8_t cmd);

/*****************************************************************************
 * Function name:   rc_read_parameter_service
 * Description:     Handle RPMC Read Parameter command
 * @param           header     - RPMC message header
 * @param           req      - Request data
 * @param           rsp  - Response data buffer
 * @param           status     - Response status
 * @param           len        - Response data length
 * @return          0 on success, negative on error
 *****************************************************************************/
 int rc_read_parameter_service(struct rc_header header, struct read_param req, struct read_param_rsp *rsp, rc_rsp_t *status, uint16_t *len);

/*****************************************************************************
 * Function name:   rc_write_root_k_service
 * Description:     Handle RPMC Write Root Key command
 * @param           head         - RPMC message header
 * @param           req          - Root key request data
 * @param           rsp          - Response data buffer
 * @param           status       - Response status
 * @param           len          - Response data length
 * @return          0 on success, negative on error
 *****************************************************************************/
 int rc_write_root_k_service(struct rc_header head, struct write_root_k req, struct write_root_k_rsp *rsp, rc_rsp_t *status, uint16_t *len);

/*****************************************************************************
 * Function name:   rc_update_hmac_k_service
 * Description:     Handle RPMC Update HMAC Key command
 * @param           head        - RPMC message header
 * @param           req        - HMAC key request data
 * @param           rsp    - Response data buffer
 * @param           status      - Response status
 * @param           len         - Response data length
 * @return          0 on success, negative on error
 *****************************************************************************/
 int rc_update_hmac_k_service(struct rc_header head, struct update_hmac_k req, struct update_hmac_k_rsp *rsp, rc_rsp_t *status, uint16_t *len);

/*****************************************************************************
 * Function name:   rc_request_mc_service
 * Description:     Handle RPMC Request Monotonic Counter command
 * @param           head         - RPMC message header
 * @param           req          - Request data
 * @param           rsp          - Response data buffer
 * @param           status       - Response status
 * @param           len          - Response data length
 * @return          0 on success, negative on error
 *****************************************************************************/
 int rc_request_mc_service(struct rc_header head, struct request_mc req, struct request_mc_rsp *rsp, rc_rsp_t *status, uint16_t *len);

/*****************************************************************************
 * Function name:   rc_increase_mc_service
 * Description:     Handle RPMC Increase Monotonic Counter command
 * @param           head         - RPMC message header
 * @param           req          - Request data
 * @param           rsp          - Response data buffer
 * @param           status       - Response status
 * @param           len          - Response data length
 * @return          0 on success, negative on error
 *****************************************************************************/
 int rc_increase_mc_service(struct rc_header head, struct increase_mc req, struct increase_mc_rsp *rsp, rc_rsp_t *status, uint16_t *len);

 /*****************************************************************************
 * Function name:   oob_rc_msg_proc
 * Description:     Process RPMC (Replay Protected Monotonic Counter) messages
 * @param           req  - Input RPMC message
 * @param           rsp  - Output RPMC response message
 * @param           pOut_len - Pointer to output message length
 * @return          0 on success, negative on error
 * @note            Handles all RPMC command types and validates parameters
 *****************************************************************************/
 int oob_rc_msg_proc(struct rc_msg_req *req, struct rc_msg_rsp *rsp, uint16_t *pOut_len);

/**
 *  RPMC postpone message service.
 */
void oob_rc_postpone_service(void);

/*****************************************************************************
 * Function name:   oob_rc_init
 * Description:     Initialize RPMC feature and all device contexts
 * @param           None
 * @return          0 on success, negative on error
 * @note            Sets up HMAC engine, ROM interface, and device contexts
 *****************************************************************************/
 int oob_rc_init(void);

#endif

