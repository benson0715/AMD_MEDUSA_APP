/**
* Copyright (c) 2025 Advanced Micro Devices, Inc.
*/

#include <zephyr/logging/log.h>
#include "oob_rpmc_msg.h"
#include "oob_rpmc_rom.h"
#include "oob_rpmc_hmac_sha2_256.h"
#include "oob_rpmc_hmac_sha3_384.h"

LOG_MODULE_REGISTER(rpmc_msg, CONFIG_OOB_RPMC_LOG_LEVEL);

#ifndef RPMC_ECAIG_SUPPORT_BITS
#define RPMC_ECAIG_SUPPORT_BITS  (RPMC_ECAIG_DRIVER_SUPPORT_BITS)
#endif

/* Supported algorithms */
static struct rc_hsha_api rc_hsha_api_init[] = {
    {
		.type = RPMC_SHA256_MC_ALGID,
		.hmac_init = hmac_sha2_256_init,
		.hmac_deinit = hmac_sha2_256_deinit,
		.hmac_cal = hmac_sha2_256_engine,
		.sha_cal = sha2_256_engine,
    }, {
		.type = RPMC_SHA3_384_MC_ALGID,
		.hmac_init = hmac_sha3_384_init,
		.hmac_deinit = hmac_sha3_384_deinit,
		.hmac_cal = hmac_sha3_384_engine,
		.sha_cal = sha3_384_engine,
    }
};

/*****************************************************************************
 * Global variable: g_rc_api_init
 * Description:     API context structure for RPMC operations.
 *                  Provides function pointers for HMAC initialization,
 *                  HMAC calculation (sync/async), and ROM access routines.
 *                  This structure is used to abstract hardware-specific
 *                  implementations and centralize RPMC-related operations.
 *                  The ROM access functions are designed to ensure that
 *                  key data cannot be attacked or tampered with by third parties.
 *****************************************************************************/
static struct rc_api rc_api_fn_init = {
	/* will be updated in running time, set to NULL for default */
	.hmac_init = NULL,
	.hmac_deinit = NULL,
	.hmac_cal = NULL,
	.sha_cal = NULL,

    /* Ensure the storage data can't be attacked by third party */
	.rom_init = rom_rpmc_data_init,
	.rom_root_key = rom_rpmc_rt_access,

	/* Should make sure the monotonic counter operation is not impacted by a power interruption event */
	.rom_counter_value = rom_rpmc_mc_access,

	/* The UDID shall be generated from a true random number generate (TRNG). 
	   UDID cannot be modified during the lifecycle of the RPMC function block.  */
	.unique_id_get = rom_unique_id_read,
};

static uint8_t kdv_key_256B_arr[] = KDV_ARR_256B;
static uint8_t kdv_key_384B_arr[] = KDV_ARR_384B;
static uint8_t kdv_keydata_arr[] = RPMC_KDV_KEYDATA_ARR;
static struct rc_dev_data rc_data_init[RPMC_PARAM_MC_ADDR_NUM] = {0};

static rc_dev_context_t g_rc_dev_ctx = {
	 .hsha_api = rc_hsha_api_init,
	 .hsha_api_num = sizeof(rc_hsha_api_init) / sizeof(rc_hsha_api_init[0]),
	 
	 .api_fn = &rc_api_fn_init,
	 .data = rc_data_init,
	 .mc_num = RPMC_PARAM_MC_ADDR_NUM,

	 .aig_id = RPMC_SHA256_MC_ALGID,      // Default algorithm ID
	 .kdv_key = kdv_key_256B_arr,         // Default root key for default algorithm ID
	 .kdv_key_sz = sizeof(kdv_key_256B_arr),
	 .kdv_keydata = kdv_keydata_arr, 	  // Default key data
	 .kdv_keydata_sz = sizeof(kdv_keydata_arr),
};

/*****************************************************************************
 * Function name:   _data_match
 * Description:     Compare two data buffers for equality
 * @param           pData_1  - First data buffer
 * @param           length_1 - First buffer length
 * @param           pData_2  - Second data buffer
 * @param           length_2 - Second buffer length
 * @return          0 if match, negative if different
 *****************************************************************************/
static inline int _data_match(const void *pData_1, size_t length_1, const void *pData_2, size_t length_2)
{
	if (length_1 != length_2) {
		return -1;
	}

	if (memcmp(pData_2, pData_1, length_2) != 0) {
		return -2;
	}

	return 0;
}

/*****************************************************************************
 * Function name:   _data_empty
 * Description:     Check if data buffer is empty (all 0xFF or all 0x00)
 * @param           pData      - Data buffer to check
 * @param           len        - Buffer length
 * @param           check_zero - Flag to also check for all zeros
 * @return          True if empty, false otherwise
 *****************************************************************************/
static bool _data_empty(uint8_t *pData, uint8_t len, bool check_zero)
{
    uint8_t first = pData[0];
    if (first != 0xFFu && (!check_zero || first != 0x0u)) {
        return false;
    }

    bool all_ff = true;
    bool all_00 = check_zero ? true : false;

    for (uint8_t i = 0; i < len; ++i) {
        if (pData[i] != 0xFFu) {
            all_ff = false;
        }
        if (check_zero && pData[i] != 0x0u) {
            all_00 = false;
        }
        if (!all_ff && (!all_00 || !check_zero)) {
            return false;
        }
    }

    return all_ff || (check_zero && all_00);
}

/*****************************************************************************
 * Function name:   _algo_size
 * Description:     Get the size of the algorithm
 * @param           aid - Algorithm ID
 * @return          The size of the algorithm
 *****************************************************************************/
static uint8_t _algo_size(uint8_t aid)
{
	switch (aid) {
		case RPMC_SHA256_MC_ALGID:
		case RPMC_SHA3_256_MC_ALGID:
		case RPMC_SM3_256_MC_ALGID:
			return RPMC_SHA256_LENGTH;
		case RPMC_SHA384_MC_ALGID:
		case RPMC_SHA3_384_MC_ALGID:
			return RPMC_SHA384_LENGTH;
		default:
			return 0;
	}
}

/*****************************************************************************
 * Function name:   _algo_select
 * Description:     Select the algorithm
 * @param           ctx - Device context
 * @param           aid - Algorithm ID
 * @return          0 on success, negative on error
 *****************************************************************************/
static int _algo_select(struct rc_dev_context *ctx, uint8_t aid)
{
	static uint8_t _s_old_aid = RPMC_AIG_ID_NUM;
	int ret = 0;

	switch (aid) {
		case RPMC_SHA256_MC_ALGID:
		case RPMC_SHA3_256_MC_ALGID:
		case RPMC_SM3_256_MC_ALGID:
			ctx->kdv_key = kdv_key_256B_arr;
			ctx->kdv_key_sz = sizeof(kdv_key_256B_arr);
			break;
		case RPMC_SHA384_MC_ALGID:
		case RPMC_SHA3_384_MC_ALGID:
			ctx->kdv_key = kdv_key_384B_arr;
			ctx->kdv_key_sz = sizeof(kdv_key_384B_arr);
			break;
		default:
			ret = -1;
			break;
	}

	if (_s_old_aid == aid) {
		return 0;
	}

	struct rc_api *fn = ctx->api_fn;
	if (fn && fn->hmac_deinit) {
		ret = fn->hmac_deinit();
		if (ret) {
			LOG_ERR("hmac algorithm ID %d deinit fail: %d", aid, ret);
		}
	}

	struct rc_hsha_api *hsha_api_cur = NULL;
	for (uint8_t i = 0; i < ctx->hsha_api_num; i++) {
		if (ctx->hsha_api[i].type == aid) {
			hsha_api_cur = &ctx->hsha_api[i];
			break;
		}
	}

	if (hsha_api_cur) {
		fn->hmac_init = hsha_api_cur->hmac_init;
		fn->hmac_deinit = hsha_api_cur->hmac_deinit;
		fn->hmac_cal = hsha_api_cur->hmac_cal;
		fn->sha_cal = hsha_api_cur->sha_cal;
	} else {
		ret = -2;
	}

	ctx->aig_id = aid;

	if (ret) {
		LOG_ERR("Algorithm select fail, algorithm ID: %d, error: %d, keep using the default algorithm %d", aid, ret, RPMC_SHA256_MC_ALGID);
		ctx->kdv_key = kdv_key_256B_arr;
		ctx->kdv_key_sz = sizeof(kdv_key_256B_arr);
		fn->hmac_init = hmac_sha2_256_init;
		fn->hmac_deinit = hmac_sha2_256_deinit;
		fn->hmac_cal = hmac_sha2_256_engine;
		fn->sha_cal = sha2_256_engine;
		return ret;
	}

	if (_s_old_aid != aid) {
		ret = fn->hmac_init();
		if (ret) {
			LOG_ERR("hmac algorithm ID %d init fail: %d", aid, ret);
		}
	}

	LOG_INF("Algorithm select successful, algorithm ID: %d -> %d", _s_old_aid, aid);
	_s_old_aid = aid;
	return ret;
}

/*****************************************************************************
 * Function name:   _context_preload
 * Description:     Preload RPMC device context with keys and state information
 * @param           ctx - Device context to initialize
 * @param           inst - Device instance number
 * @return          0 on success, negative on error
 *****************************************************************************/
static int _context_preload(struct rc_dev_context *ctx, uint8_t inst)
{
	int ret = 0;
	struct rc_api *fn = ctx->api_fn;
	struct rc_dev_data *data = &ctx->data[inst];
	uint8_t udid[RPMC_UDID_SIZE] = {0};
	uint8_t sha_udid[RPMC_SHA_LENGTH] = {0};
	uint8_t alg_sz = _algo_size(ctx->aig_id);

	if (!ctx || !ctx->api_fn || !ctx->data) {
		LOG_ERR("Device context is invalid, abort preload");
		return -1;
	}

	if (inst >= ctx->mc_num) {
		LOG_ERR("Device instance number out of range: %d, max: %d", inst, ctx->mc_num);
		return -2;
	}

	if (data->inited) {
		LOG_ERR("Device instance %d already initialized, abort preload", inst);
		return -3;
	}

	/* Get and compute unique ID */
	ret = fn->unique_id_get(inst, udid, RPMC_UDID_SIZE);
	if (ret) {
		LOG_ERR("Unique id read fail: %d", ret);
		return -4;
	}

	/* First algorithm */
	ret = fn->sha_cal(udid, RPMC_UDID_SIZE, sha_udid, alg_sz);
	if (ret) {
		LOG_ERR("Unique id compute fail: %d", ret);
		return -5;
	}
	memcpy(data->udid, sha_udid, RPMC_UDID_SIZE);

	if (inst == 0) {
		LOG_HEXDUMP_INF(udid, RPMC_UDID_SIZE, "RAW UDID:");
		LOG_HEXDUMP_INF(data->udid, RPMC_UDID_SIZE, "SHA UDID:");
	}

	ret = fn->rom_counter_value(inst, (uint8_t *)&data->mc_value, RPMC_MC_VALUE_SIZE, RPMC_ROM_OP_READ, RPMC_ROM_BUS_SPI);
	if (ret) {
		LOG_ERR("ROM RPMC counter value read fail: %d", ret);
		return -6;
	}
	
	data->st_rkr = RC_ST_IN;
	data->st_mc = RC_ST_IN;
	data->st_hkr = RC_ST_UN;
	ret = fn->rom_root_key(inst, data->root_key, RPMC_SHA_LENGTH, RPMC_ROM_OP_READ, RPMC_ROM_BUS_SPI);
	if (ret) {
		LOG_ERR("ROM RPMC root key read fail: %d", ret);
		return -7;
	}

	if (_data_empty(data->root_key, RPMC_SHA_LENGTH, false)) {
		data->st_rkr = RC_ST_UN;
		data->st_mc = RC_ST_UN;
	}

	/* Check if root key matches either 256-bit or 384-bit KDV key */
	ret = _data_match(kdv_key_256B_arr, RPMC_SHA256_LENGTH, data->root_key, RPMC_SHA256_LENGTH);
	if (ret == 0) {
		data->st_rkr = RC_ST_UN;
	} else {
		ret = _data_match(kdv_key_384B_arr, RPMC_SHA384_LENGTH, data->root_key, RPMC_SHA384_LENGTH);
		if (ret == 0) {
			data->st_rkr = RC_ST_UN;
		}
	}

	data->inited = true;
	LOG_INF("Device %d context preload successful, rkr %d, mc %d, hkr %d", inst, data->st_rkr, data->st_mc, data->st_hkr);
	return 0;
}

/*****************************************************************************
 * Function name:   _compute_signature_req
 * Description:     Compute HMAC signature for RPMC request message
 * @param           header   - RPMC message header
 * @param           key      - HMAC key
 * @param           key_len  - Key length
 * @param           data     - Additional data to sign
 * @param           data_len - Data length
 * @param           sig      - Output signature buffer
 * @param           sig_len  - Signature buffer length
 * @return          0 on success, negative on error
 *****************************************************************************/
static int _compute_signature_req(struct rc_header header, uint8_t *key, uint8_t key_len, uint8_t *data, uint8_t data_len, uint8_t *sig, uint8_t sig_len)
{
	uint8_t msg[128] = { 0 };
	uint8_t len = sizeof(struct rc_header);

	/* Bounds check to prevent buffer overflow */
	if (len + data_len > sizeof(msg)) {
		LOG_ERR("Signature request buffer overflow: %d + %d > %d", len, data_len, sizeof(msg));
		return -1;
	}

	memcpy(&msg[0], (uint8_t *)&header, len);

	if ((data != NULL) && (data_len > 0)) {
		memcpy(&msg[len], (uint8_t *)data, data_len);
		len += data_len;
	}

	return g_rc_dev_ctx.api_fn->hmac_cal(msg, len, key, key_len, sig, sig_len);
}

/*****************************************************************************
 * Function name:   _compute_signature_rsp
 * Description:     Compute HMAC signature for RPMC response message
 * @param           rsp      - RPMC response structure
 * @param           key     - HMAC key
 * @param           key_len    - Key length
 * @param           data    - Additional data to sign
 * @param           data_len - Data length
 * @param           sig     - Output signature buffer
 * @param           sig_len  - Signature buffer length
 * @return          0 on success, negative on error
 *****************************************************************************/
static int _compute_signature_rsp(rc_rsp_t rsp, uint8_t *key, uint8_t key_len, uint8_t *data, uint8_t data_len, uint8_t *sig, uint8_t sig_len)
{
	uint8_t msg[128] = { 0 };
	uint8_t len = sizeof(rc_rsp_t);

	/* Bounds check to prevent buffer overflow */
	if (len + data_len > sizeof(msg)) {
		LOG_ERR("Signature response buffer overflow: %d + %d > %d", len, data_len, sizeof(msg));
		return -1;
	}

	memcpy(&msg[0], (uint8_t *)&rsp, len);

	if ((data != NULL) && (data_len > 0)) {
		memcpy(&msg[len], (uint8_t *)data, data_len);
		len += data_len;
	}

	return g_rc_dev_ctx.api_fn->hmac_cal(msg, len, key, key_len, sig, sig_len);
}

/*****************************************************************************
 * Function name:   _completed_status
 * Description:     Update response status to indicate operation completion
 * @param           pRsp - Response structure to update
 * @return          None
 *****************************************************************************/
void _completed_status(rc_rsp_t *pRsp)
{
	if (!(pRsp->ext_status.value & 0xFE)) {
		// Pass result.
		pRsp->ext_status.bit.err_no = 1;
	}
}

/*****************************************************************************
 * Function name:   rc_algorithm_supported
 * Description:     Check if RPMC algorithm is supported
 * @param           alg_id - RPMC algorithm ID to check
 * @return          0 if supported, negative if not supported
 *****************************************************************************/
int rc_algorithm_supported(uint8_t alg_id)
{
	return (RPMC_ECAIG_SUPPORT_BITS & RPMC_AIG_ID_BIT(alg_id)) ? 0 : -1;
}

/*****************************************************************************
 * Function name:   rc_code_supported
 * Description:     Check if RPMC command code is supported
 * @param           cmd - RPMC command code to check
 * @return          0 if supported, negative if not supported
 *****************************************************************************/
int rc_code_supported(uint8_t cmd)
{
	int ret = 0;
	
	switch (cmd) {
		case RPMC_CMD_READ_PARAMETER_CODE:
		case RPMC_CMD_WRITE_ROOT_KEY_CODE:
		case RPMC_CMD_UPDATE_HMAC_KEY_CODE:
		case RPMC_CMD_REQUEST_MC_CODE:
		case RPMC_CMD_INCREASE_MC_CODE:{
			break;
		}
		default:
			ret = -1;
			break;
	}

	return ret;
}

/*****************************************************************************
 * Function name:   rc_read_parameter_service
 * Description:     Handle RPMC Read Parameter command
 * @param           header     - RPMC message header
 * @param           req        - Request data
 * @param           rsp        - Response data buffer
 * @param           status     - Response status
 * @param           len        - Response data length
 * @return          0 on success, negative on error
 *****************************************************************************/
int rc_read_parameter_service(struct rc_header header, struct read_param req, struct read_param_rsp *rsp, rc_rsp_t *status, uint16_t *len)
{
	uint8_t addr = header.cmd_mc_addr;
	struct rc_dev_data *data = &g_rc_dev_ctx.data[addr];
	*len = sizeof(rc_rsp_t) + LENGTH_DATA_RD_PARAM_RSP;
	memset((uint8_t *)rsp, 0x0, sizeof(struct read_param_rsp));

	rsp->param.update_rate = RPMC_PARAM_UPDATE_RATE_40_SEC; // ec system capability
	rsp->param.spec_ver = RPMC_PARAM_VERSION;
	rsp->param.mc_max_addr = RPMC_PARAM_MC_ADDR_NUM;
	rsp->param.hash_support = RPMC_ECAIG_SUPPORT_BITS;
	rsp->param.timeout = RPMC_PARAM_RESPONSE_TIMEOUT_500_MS; // flash busy + firmware running time, 500ms is enough
	rsp->param.rpmc_support = RPMC_PARAM_SUPPORTED;
	
	if (rsp->param.mc_max_addr >= RPMC_PARAM_FAR_2V0_MC_MINI_NUM) {
		rsp->param.far_2v0_en = RPMC_PARAM_FAR_2V0_EN;
	} else {
		rsp->param.far_2v0_en = RPMC_PARAM_FAR_2V0_DIS;
	}

	memcpy(rsp->udid, data->udid, sizeof(rsp->udid));
	LOG_INF("Read-Parameter!!!");

	_completed_status(status);
	return 0;
}

/*****************************************************************************
 * Function name:   rsp_data_init
 * Description:     Initialize response data buffer
 * @param           rsp_data       - Response data buffer to initialize
 * @param           payload_length - Length of payload data
 * @param           rsp_len        - Pointer to store total response length
 * @return          None
 *****************************************************************************/
static void rsp_data_init(void *rsp_data, uint8_t payload_length, uint16_t *rsp_len)
{
	uint16_t len = payload_length + LENGTH_DATA_RC_RSP;
	memset((uint8_t *)rsp_data, 0x0, len);
	*rsp_len = len;
}

/*****************************************************************************
 * Function name:   _hsha_get
 * Description:     Get HMAC/SHA data pointer based on algorithm ID
 * @param           alg_id - Algorithm ID
 * @param           hsha   - HMAC/SHA union structure
 * @return          Pointer to HMAC/SHA data buffer, NULL if invalid algorithm
 *****************************************************************************/
static uint8_t* _hsha_get(uint8_t alg_id, struct hmac_sha *hsha)
{
	uint8_t len = _algo_size(alg_id);

	if (len == RPMC_SHA256_LENGTH) {
		return hsha->b256.hsha_data;
	} else if (len == RPMC_SHA384_LENGTH) {
		return hsha->b384.hsha_data;
	}

	return NULL;
}

/*****************************************************************************
 * Function name:   _write_root_k_data_get
 * Description:     Extract root key data from write root key request
 * @param           alg_id - Algorithm ID
 * @param           rt_key - Write root key request structure
 * @param           sig    - Pointer to store signature pointer
 * @param           key    - Pointer to store key pointer
 * @param           alg_sz - Pointer to store algorithm size
 * @return          None
 *****************************************************************************/
static void _write_root_k_data_get(uint8_t alg_id, struct write_root_k *rt_key, uint8_t **sig, uint8_t **key, uint8_t *alg_sz)
{
	uint8_t len = _algo_size(alg_id);

	if (len == RPMC_SHA256_LENGTH) {
		*alg_sz = RPMC_SHA256_LENGTH;
		*key = rt_key->b256.mc_root_key;
		*sig = _hsha_get(alg_id, &rt_key->b256.cmd_sig);
	} else if (len == RPMC_SHA384_LENGTH) {
		*alg_sz = RPMC_SHA384_LENGTH;
		*key = rt_key->b384.mc_root_key;
		*sig = _hsha_get(alg_id, &rt_key->b384.cmd_sig);
	}
}

/*****************************************************************************
 * Function name:   rc_write_root_k_service
 * Description:     Handle RPMC Write Root Key command
 * @param           head       - RPMC message header
 * @param           req        - Root key request data
 * @param           rsp        - Response data buffer
 * @param           status     - Response status
 * @param           len        - Response data length
 * @return          0 on success, negative on error
 *****************************************************************************/
int rc_write_root_k_service(struct rc_header head, struct write_root_k req, struct write_root_k_rsp *rsp, rc_rsp_t *status, uint16_t *len)
{
	int ret = 0;
	uint8_t *cmd_sig = NULL;
	uint8_t *mc_root_key = NULL;
	uint8_t alg_sz = 0;
	uint8_t csig[RPMC_SHA_LENGTH] = { 0 };
	uint8_t chkey[RPMC_SHA_LENGTH] = { 0 };
	uint8_t alg_id = head.cmd_mc_aid;
	uint8_t addr = head.cmd_mc_addr;
	struct rc_api *fn = g_rc_dev_ctx.api_fn;
	struct rc_dev_data *data = &g_rc_dev_ctx.data[addr];

	_write_root_k_data_get(alg_id, &req, &cmd_sig, &mc_root_key, &alg_sz);

	rsp_data_init(rsp, LENGTH_DATA_WT_K_RSP(alg_sz), len);
	ret = _algo_select(&g_rc_dev_ctx, alg_id);
	if (ret) {
		LOG_ERR("Algorithm select fail: %d", ret);
		status->ext_status.bit.err_rsvd_1 = 1;
		goto exit;
	}

	if (data->st_rkr == RC_ST_IN) { // Root key register is already initialized
		LOG_ERR("Write-ROOTKEY %d error state", addr);
		status->ext_status.bit.err_state = 1;
		goto exit;
	}

	// HMACKey[CMD_MC_Addr] = HMAC(RootKey[CMD_MC_Addr], KDV_KeyData);
	ret = fn->hmac_cal(g_rc_dev_ctx.kdv_keydata, g_rc_dev_ctx.kdv_keydata_sz, mc_root_key, alg_sz, chkey, alg_sz);
	if (ret != 0) {
		LOG_ERR("Write-ROOTKEY %d signature compute error_1", addr);
		status->ext_status.bit.err_rsvd_1 = 1;
		goto exit;
	}

	// CMD_Sig = HMAC (MC_HMACKey[MC_Addr], (CMD_Code | CMD_MC_Addr | CMD_MC_AlgID) | CMD_Input)
	ret = _compute_signature_req(head, chkey, alg_sz, mc_root_key, alg_sz, csig, alg_sz);
	if (ret != 0) {
		LOG_ERR("Write-ROOTKEY %d signature compute error_2", addr);
		status->ext_status.bit.err_rsvd_1 = 1;
		goto exit;
	}
	ret = _data_match(cmd_sig, alg_sz, csig, alg_sz);
	if (ret != 0) {
		LOG_ERR("Write-ROOTKEY %d signature mismatch", addr);
		status->ext_status.bit.err_signature = 1;
		goto exit;
	}

	ret = _data_match(g_rc_dev_ctx.kdv_key, alg_sz, mc_root_key, alg_sz);
	if (ret) {
		// provisions the root key into mission state from the manufacturing/testing state.
		data->st_rkr = RC_ST_IN;
	}

	if (data->st_mc == RC_ST_UN) {
		// provisions the counter into testing/mission state from the manufacturing state.
		data->st_mc = RC_ST_IN;
		data->mc_value = 0x0;
	}

	data->st_hkr = RC_ST_UN;
	g_rc_dev_ctx.aig_id = alg_id;

	data->root_key_sz = alg_sz;
	memcpy(data->root_key, mc_root_key, alg_sz);
	memcpy(data->hmac_key, chkey, alg_sz);
	LOG_INF("Write-RootKey %d!!!", addr);

#if (RPMC_ROM_ACCESS_POSTPONE)
	if (data->st_rkr == RC_ST_IN) {
		data->updated |= RC_DATA_UPDATED_RK;
	}

	if (data->st_mc == RC_ST_IN) {
		data->updated |= RC_DATA_UPDATED_CT;
	}
#else
    // 1. Manufacturing/testing state: a known default value
	// 2. Mission state: security-sensitive values.
	// if (data->st_rkr == RC_ST_IN) {
		ret = fn->rom_root_key(addr, data->root_key, alg_sz, RPMC_ROM_OP_WRITE, RPMC_ROM_BUS_SPI);
		if (ret) {
			LOG_ERR("ROM RPMC root key write fail: %d", ret);
			status->ext_status.bit.err_rsvd_1 = 1;
			goto exit;
		}
	// }

	if (data->st_mc == RC_ST_IN) {
		ret = fn->rom_counter_value(addr, (uint8_t *)&data->mc_value, sizeof(data->mc_value), RPMC_ROM_OP_WRITE, RPMC_ROM_BUS_SPI);
		if (ret) {
			LOG_ERR("ROM RPMC counter value write fail: %d", ret);
			status->ext_status.bit.err_rsvd_1 = 1;
			goto exit;
		}
	}
#endif

exit:
	_completed_status(status);
	uint8_t *rsp_sig = _hsha_get(alg_id, &rsp->sig);
	return _compute_signature_rsp(*status, chkey, alg_sz, NULL, 0, rsp_sig, alg_sz);
}

/*****************************************************************************
 * Function name:   rc_update_hmac_k_service
 * Description:     Handle RPMC Update HMAC Key command
 * @param           head        - RPMC message header
 * @param           req         - HMAC key request data
 * @param           rsp         - Response data buffer
 * @param           status      - Response status
 * @param           len         - Response data length
 * @return          0 on success, negative on error
 *****************************************************************************/
int rc_update_hmac_k_service(struct rc_header head, struct update_hmac_k req, struct update_hmac_k_rsp *rsp, rc_rsp_t *status, uint16_t *len)
{
	int ret = 0;
	uint8_t csig[RPMC_SHA_LENGTH] = { 0 };
	uint8_t chkey[RPMC_SHA_LENGTH] = { 0 };
	uint8_t crkey[RPMC_SHA_LENGTH] = { 0 };
	uint8_t alg_id = head.cmd_mc_aid;
	uint8_t alg_sz = _algo_size(alg_id);
	uint8_t addr = head.cmd_mc_addr;
	uint8_t *mc_keyData = req.mc_keyData;
	uint8_t *cmd_sig = _hsha_get(alg_id, &req.cmd_sig);
	uint8_t *rsp_sig = _hsha_get(alg_id, &rsp->sig);
	struct rc_api *fn = g_rc_dev_ctx.api_fn;
	struct rc_dev_data *data = &g_rc_dev_ctx.data[addr];

	rsp_data_init(rsp, LENGTH_DATA_UP_HK_RSP(alg_sz), len);
	ret = _algo_select(&g_rc_dev_ctx, alg_id);
	if (ret) {
		LOG_ERR("Algorithm select fail: %d", ret);
		status->ext_status.bit.err_rsvd_1 = 1;
		goto exit;
	}

	if (data->st_mc == RC_ST_UN) {
		LOG_ERR("Update-HMAC %d error state", addr);
		status->ext_status.bit.err_state = 1;
		goto exit;
	}

	if (data->st_rkr == RC_ST_UN) {
		memcpy(crkey, g_rc_dev_ctx.kdv_key, g_rc_dev_ctx.kdv_key_sz);
	} else {
		memcpy(crkey, data->root_key, alg_sz);
	}
	
	// HMACKey[CMD_MC_Addr] = HMAC(RootKey[CMD_MC_Addr], MC_KeyData);
	ret = fn->hmac_cal(mc_keyData, RPMC_MC_KEYDATA_SIZE, crkey, alg_sz, chkey, alg_sz);
	if (ret != 0) {
		LOG_ERR("Update-HMAC %d signature compute error_1" , addr);
		status->ext_status.bit.err_rsvd_1 = 1;
		goto exit;
	}

	// CMD_Sig = HMAC (MC_HMACKey[MC_Addr], (CMD_Code | CMD_MC_Addr | CMD_MC_AlgID) | CMD_Input)
	ret = _compute_signature_req(head, chkey, alg_sz, mc_keyData, RPMC_MC_KEYDATA_SIZE, csig, alg_sz);
	if (ret != 0) {
		LOG_ERR("Update-HMAC %d signature compute error_2", addr);
		status->ext_status.bit.err_rsvd_1 = 1;
		goto exit;
	}
	ret = _data_match(cmd_sig, alg_sz, csig, alg_sz);
	if (ret != 0) {
		LOG_ERR("Update-HMAC %d signature mismatch", addr);
		status->ext_status.bit.err_signature = 1;
		goto exit;
	}

	data->st_hkr = RC_ST_IN;
	memcpy(data->key_data, mc_keyData, RPMC_MC_KEYDATA_SIZE);
	memcpy(data->hmac_key, chkey, alg_sz);
	LOG_INF("Update-hmac %d!!!", addr);

exit:
	_completed_status(status);
	return _compute_signature_rsp(*status, chkey, alg_sz, NULL, 0, rsp_sig, alg_sz);
}

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
int rc_request_mc_service(struct rc_header head, struct request_mc req, struct request_mc_rsp *rsp, rc_rsp_t *status, uint16_t *len)
{
	int ret = 0;
	uint8_t csig[RPMC_SHA_LENGTH] = { 0 };
	uint8_t alg_id = head.cmd_mc_aid;
	uint8_t alg_sz = _algo_size(alg_id);
	uint8_t *mc_tag = req.mc_tag;
	uint8_t *cmd_sig = _hsha_get(alg_id, &req.cmd_sig);
	uint8_t *rsp_sig = _hsha_get(alg_id, &rsp->sig);
	uint8_t addr = head.cmd_mc_addr;
	struct rc_dev_data *data = &g_rc_dev_ctx.data[addr];

	rsp_data_init(rsp, LENGTH_DATA_RQ_MC_RSP(alg_sz), len);
	ret = _algo_select(&g_rc_dev_ctx, alg_id);
	if (ret) {
		LOG_ERR("Algorithm select fail: %d", ret);
		status->ext_status.bit.err_rsvd_1 = 1;
		goto exit;
	}

	if ((data->st_mc == RC_ST_UN) || (data->st_hkr == RC_ST_UN)) {
		LOG_ERR("Request-MC %d error state", addr);
		status->ext_status.bit.err_state = 1;
		goto exit;
	}

	// CMD_Sig = HMAC (MC_HMACKey[MC_Addr], (CMD_Code | CMD_MC_Addr | CMD_MC_AlgID) | CMD_Input)
	ret = _compute_signature_req(head, data->hmac_key, alg_sz, mc_tag, RPMC_MC_TAG_SIZE, csig, alg_sz);
	if (ret != 0) {
		LOG_ERR("Request-MC %d signature compute error", addr);
		status->ext_status.bit.err_rsvd_1 = 1;
		goto exit;
	}
	ret = _data_match(cmd_sig, alg_sz, csig, alg_sz);
	if (ret != 0) {
		LOG_ERR("Request-MC %d signature mismatch", addr);
		status->ext_status.bit.err_signature = 1;
		goto exit;
	}

	memcpy(rsp->mc_tag, mc_tag, RPMC_MC_TAG_SIZE);
	memcpy(rsp->mc_val, (uint8_t*)&data->mc_value , RPMC_MC_VALUE_SIZE);
	LOG_INF("Request-MC %d value %d!!!", addr, data->mc_value);

exit:
	_completed_status(status);
	return _compute_signature_rsp(*status, data->hmac_key, alg_sz, rsp->mc_tag, (RPMC_MC_TAG_SIZE + RPMC_MC_VALUE_SIZE), rsp_sig, alg_sz);
}

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
int rc_increase_mc_service(struct rc_header head, struct increase_mc req, struct increase_mc_rsp *rsp, rc_rsp_t *status, uint16_t *len)
{
	int ret = 0;
	uint32_t data_u32 = 0;
	uint8_t csig[RPMC_SHA_LENGTH] = { 0 };
	uint8_t alg_id = head.cmd_mc_aid;
	uint8_t alg_sz = _algo_size(alg_id);
	uint8_t *mc_val = req.mc_val;
	uint8_t *cmd_sig = _hsha_get(alg_id, &req.cmd_sig);
	uint8_t *rsp_sig = _hsha_get(alg_id, &rsp->sig);
	uint8_t addr = head.cmd_mc_addr;
	struct rc_api *fn = g_rc_dev_ctx.api_fn;
	struct rc_dev_data *data = &g_rc_dev_ctx.data[addr];

	rsp_data_init(rsp, LENGTH_DATA_INC_MC_RSP(alg_sz), len);
	ret = _algo_select(&g_rc_dev_ctx, alg_id);
	if (ret) {
		LOG_ERR("Algorithm select fail: %d", ret);
		status->ext_status.bit.err_rsvd_1 = 1;
		goto exit;
	}

	if ((data->st_mc == RC_ST_UN) || (data->st_hkr == RC_ST_UN)) {
		LOG_ERR("Increase-MC %d error state", addr);
		status->ext_status.bit.err_state = 1;
		goto exit;
	}

	// CMD_Sig = HMAC (MC_HMACKey[MC_Addr], (CMD_Code | CMD_MC_Addr | CMD_MC_AlgID) | CMD_Input)
	ret = _compute_signature_req(head, data->hmac_key, alg_sz, mc_val, RPMC_MC_VALUE_SIZE, csig, alg_sz);
	if (ret != 0) {
		LOG_ERR("Increase-MC %d signature compute error", addr);
		status->ext_status.bit.err_rsvd_1 = 1;
		goto exit;
	}
	ret = _data_match(cmd_sig, alg_sz, csig, alg_sz);
	if (ret != 0) {
		status->ext_status.bit.err_signature = 1;
		LOG_ERR("Increase-MC %d signature mismatch", addr);
		goto exit;
	}
	
	memcpy((uint8_t*)&data_u32, mc_val, RPMC_MC_VALUE_SIZE);
	LOG_INF("Increase-MC %d input %d current %d!!!", addr, data_u32, data->mc_value);
	ret = _data_match(mc_val, RPMC_MC_VALUE_SIZE, (uint8_t *)&data->mc_value , RPMC_MC_VALUE_SIZE);
	if (ret != 0) {
		status->ext_status.bit.err_mc_value = 1;
		LOG_ERR("Increase-MC %d mismatch %d %d", addr, data_u32, data->mc_value);
		goto exit;
	}
	data->mc_value++;

#if (RPMC_ROM_ACCESS_POSTPONE)
	data->updated |= RC_DATA_UPDATED_CT;
#else
	ret = fn->rom_counter_value(addr, (uint8_t *)&data->mc_value, RPMC_MC_VALUE_SIZE, RPMC_ROM_OP_WRITE, RPMC_ROM_BUS_SPI);
	if (ret) {
		LOG_ERR("ROM RPMC counter value write fail: %d", ret);
		status->ext_status.bit.err_rsvd_1 = 1;
		goto exit;
	}
#endif

exit:
	_completed_status(status);
	return _compute_signature_rsp(*status, data->hmac_key, alg_sz, NULL, 0, rsp_sig, alg_sz);
}

/*****************************************************************************
 * Function name:   oob_rc_msg_proc
 * Description:     Process RPMC (Replay Protected Monotonic Counter) messages
 * @param           req  - Input RPMC message
 * @param           rsp - Output RPMC response message
 * @param           pOut_len - Pointer to output message length
 * @return          0 on success, negative on error
 * @note            Handles all RPMC command types and validates parameters
 *****************************************************************************/
 int oob_rc_msg_proc(struct rc_msg_req *req, struct rc_msg_rsp *rsp, uint16_t *pOut_len)
 {
	 int ret = 0;
	 struct rc_header *req_header = (struct rc_header *)&req->header;
	 struct rc_rsp *rsp_status = (struct rc_rsp *)&rsp->response;
  
	 uint8_t cmd = req_header->cmd_code;
	  
	 rsp_status->ext_status.value = 0;
	 rsp_status->mc_counter_addr = req_header->cmd_mc_addr;
	 if (req_header->cmd_mc_addr >= RPMC_PARAM_MC_ADDR_NUM) {
		 LOG_ERR("Doesn't support the PRMC mc addr %d.", req_header->cmd_mc_addr);
		 rsp_status->ext_status.bit.err_range = 1;
	 }
  
	 if ((cmd != RPMC_CMD_READ_PARAMETER_CODE) && (rc_algorithm_supported(req_header->cmd_mc_aid) != 0)) {
		 LOG_ERR("Doesn't support the PRMC algorithm %d.", req_header->cmd_mc_aid);
		 rsp_status->ext_status.bit.err_range = 1;
	 }
  
	 if (rc_code_supported(cmd) != 0) {
		 LOG_ERR("Doesn't support the PRMC cmd %d.", cmd);
		 rsp_status->ext_status.bit.err_range = 1;
	 }
  
	 if (cmd == RPMC_CMD_READ_PARAMETER_CODE) {
		 ret = rc_read_parameter_service(req->header, req->rd_param, &rsp->rd_param, rsp_status, pOut_len);
		 if (ret != 0) {
			 LOG_ERR("PRMC read parameter service failed result: %d", ret);
		 }
	 } else if (cmd == RPMC_CMD_WRITE_ROOT_KEY_CODE) {
		 ret = rc_write_root_k_service(req->header, req->rt_key, &rsp->rt_key, rsp_status, pOut_len);
		 if (ret != 0) {
			 LOG_ERR("PRMC write root key service failed result: %d", ret);
		 }
	 } else if (cmd == RPMC_CMD_UPDATE_HMAC_KEY_CODE) {
		 ret = rc_update_hmac_k_service(req->header, req->h_key, &rsp->h_key, rsp_status, pOut_len);
		 if (ret != 0) {
			 LOG_ERR("PRMC update hmac key service failed result: %d", ret);
		 }
	 } else if (cmd == RPMC_CMD_REQUEST_MC_CODE) {
		 ret = rc_request_mc_service(req->header, req->req_mc, &rsp->req_mc, rsp_status, pOut_len);
		 if (ret != 0) {
			 LOG_ERR("PRMC request monotonic counter service failed result: %d", ret);
		 }
	 } else if (cmd == RPMC_CMD_INCREASE_MC_CODE) {
		 ret = rc_increase_mc_service(req->header, req->inc_mc, &rsp->inc_mc, rsp_status, pOut_len);
		 if (ret != 0) {
			 LOG_ERR("PRMC increase monotonic counter service failed result: %d", ret);
		 }
	 }
	 
	 return ret;
 }

/*****************************************************************************
 * Function name:   oob_rc_postpone_service
 * Description:     Process postponed ROM write operations for RPMC data
 * @param           None
 * @return          None
 * @note            Only active when RPMC_ROM_ACCESS_POSTPONE is enabled
 *****************************************************************************/
void oob_rc_postpone_service(void)
{
#if (RPMC_ROM_ACCESS_POSTPONE)
	int ret = 0;
	struct rc_api *fn = g_rc_dev_ctx.api_fn;
	uint8_t i = g_rc_dev_ctx.mc_num;
	while (i--) {
		struct rc_dev_data *data = &g_rc_dev_ctx.data[i];
		if (data->updated & RC_DATA_UPDATED_RK) {
			
			data->updated &= ~RC_DATA_UPDATED_RK;
			LOG_INF("Write ROM ROOTKEY: %d", i);
			ret = fn->rom_root_key(i, data->root_key, data->root_key_sz, RPMC_ROM_OP_WRITE, RPMC_ROM_BUS_SPI);
			if (ret) {
				LOG_ERR("ROM RPMC root key write fail: %d", ret);
			}
		}

		if (data->updated & RC_DATA_UPDATED_CT) {
		
			data->updated &= ~RC_DATA_UPDATED_CT;
			LOG_INF("Write ROM MC: %d, value: %d", i, data->mc_value);
			ret = fn->rom_counter_value(i, (uint8_t *)&data->mc_value, RPMC_MC_VALUE_SIZE, RPMC_ROM_OP_WRITE, RPMC_ROM_BUS_SPI);
			if (ret) {
				LOG_ERR("ROM RPMC counter value write fail: %d", ret);
			}
		}
	}
#endif
}

/*****************************************************************************
 * Function name:   oob_rc_init
 * Description:     Initialize RPMC feature and all device contexts
 * @param           None
 * @return          0 on success, negative on error
 * @note            Sets up HMAC engine, ROM interface, and device contexts
 *****************************************************************************/
int oob_rc_init(void)
{
	int ret = 0;
	
	LOG_WRN("Initializing RPMC feature...");

	uint32_t unmatch_bits = RPMC_ECAIG_SUPPORT_BITS & (~RPMC_ECAIG_DRIVER_SUPPORT_BITS);
	if (unmatch_bits) {
		LOG_ERR("The current driver does not support the algorithms: %d", unmatch_bits);
		return -1;
	}

	uint8_t aid = RPMC_SHA256_MC_ALGID;
	for(int i = RPMC_AIG_ID_NUM - 1; i >= 0; --i) {
		if (RPMC_ECAIG_DRIVER_SUPPORT_BITS & (1 << i)) { // Always use the first supported algorithm for default initialization
			aid = RPMC_AIG_ID_REVERSE(i);
			break;
		}
	}

	ret = _algo_select(&g_rc_dev_ctx, aid);
	if (ret) {
		LOG_ERR("Algorithm select fail: %d", ret);
		return -2;
	}

	ret = g_rc_dev_ctx.api_fn->rom_init(g_rc_dev_ctx.mc_num);
	if (ret) {
		LOG_ERR("RPMC ROM init fail: %d", ret);
		return -3;
	}

	for (uint8_t i = 0; i < g_rc_dev_ctx.mc_num; i++) {
		ret = _context_preload(&g_rc_dev_ctx, i);
		if (ret) {
			LOG_ERR("RPMC Device %d init fail: %d", i, ret);
			return -4;
		}
	}

	return 0;
}