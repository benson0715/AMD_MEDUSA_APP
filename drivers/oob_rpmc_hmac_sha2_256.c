/**
* Copyright (c) 2025 Advanced Micro Devices, Inc.
*/
#include "oob_rpmc_hmac_sha2_256.h"
#include "board_config.h"
#include <zephyr/logging/log.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/crypto/crypto.h>
#include <zephyr/crypto/sha_hmac_npcx.h>

/* Global variables for SHA/HMAC device and context */
static const struct device *sha_dev;  // SHA hardware device pointer
static struct hash_ctx ctx;           // Hash context for operations
static bool is_inited = false;

#define LOG_LEVEL LOG_LEVEL_WRN
LOG_MODULE_REGISTER(rpmc_hmac_sha2_256, LOG_LEVEL);

/*****************************************************************************
 * Function name:   hmac_sha2_256_begin
 * Description:     Begin HMAC-SHA256 hash session
 * @return          0 on success, negative on error
 *****************************************************************************/
static int hmac_sha2_256_begin(void)
{
	enum hash_algo g_sha = CRYPTO_HASH_ALGO_SHA256;

	if (is_inited) {
		return 0;
	}

	ctx.flags = CAP_SYNC_OPS | CAP_SEPARATE_IO_BUFS;
	int ret = hash_begin_session(sha_dev, &ctx, g_sha);
	if (ret) {
		LOG_ERR("Failed to init session: %d", ret);
		return -1;
	}

	is_inited = true;
	return 0;
}

/*****************************************************************************
 * Function name:   hmac_sha2_256_free
 * Description:     Free the hash session
 * @return          0 on success, negative on error
 *****************************************************************************/
static int hmac_sha2_256_free(void)
{
	if (!is_inited) {
		return 0;
	}

	int ret = hash_free_session(sha_dev, &ctx);
	if (ret) {
		LOG_ERR("Failed to free hash session: %d", ret);
		return -1;
	}

	is_inited = false;
	return 0;
}

/*****************************************************************************
 * Function name:   hmac_sha2_256_init
 * Description:     Initialize HMAC-SHA256 hardware device and session
 * @return          0 on success, negative on error
 * @note            Sets up SHA hardware device and begins hash session
 *****************************************************************************/
int hmac_sha2_256_init(void)
{
	sha_dev = DEVICE_DT_GET(SHA);
	if (!device_is_ready(sha_dev)) {
		LOG_ERR("%s is not ready", sha_dev->name);
		return -1;
	}

	int ret = hmac_sha2_256_begin();
	if (ret) {
		LOG_ERR("Failed to set hash session: %d", ret);
		return -2;
	}

	return 0;
}

/*****************************************************************************
 * Function name:   hmac_sha2_256_deinit
 * Description:     free HMAC-SHA256 hardware device and session
 * @return          0 on success, negative on error
 * @note            Turn off SHA hardware device and free hash session
 *****************************************************************************/
int hmac_sha2_256_deinit(void)
{
	int ret = hmac_sha2_256_free();
	if (ret) {
		LOG_ERR("Failed to free hash session: %d", ret);
		return -5;
	}

	return 0;
}

/*****************************************************************************
 * Function name:   sha2_256_engine
 * Description:     Compute SHA256 for a message using hardware engine
 * @param           pInMsg   - Pointer to input message buffer
 * @param           inMsgLen - Length of the input message
 * @param           pOutDig  - Pointer to output buffer for the SHA256 result
 * @param           OutDigSize - Length of the output buffer (should be 32 bytes)
 * @return          0 on success, negative on error
 *****************************************************************************/
int sha2_256_engine(uint8_t *pInMsg, uint8_t inMsgLen, uint8_t *pOutDig, uint8_t OutDigSize)
{
	int ret = 0;
	uint8_t update_sz = 0;
	uint8_t digest[eRPMC_DIGEST_SIZE] = {0};

	if (!pInMsg || !pOutDig) {
		LOG_ERR("Invalid arguments");
		return -1;
	}

	struct hash_pkt pkt;

	if (inMsgLen > eRPMC_SAH256_BLOCK_SIZE) {
		update_sz = inMsgLen & ~(eRPMC_SAH256_BLOCK_SIZE - 1u);
		LOG_WRN("SHA256 update szie %d", update_sz);

		pkt.in_buf = pInMsg;
		pkt.in_len = update_sz;
		pkt.out_buf = digest;
		ret = hash_update(&ctx, &pkt);
		if (ret) {
			LOG_ERR("Failed to update sha256: %d", ret);
			return -3;
		}

		pInMsg += update_sz;
		inMsgLen -= update_sz;
	}

	pkt.in_buf = pInMsg;
	pkt.in_len = inMsgLen;
	pkt.out_buf = digest;
	ret = hash_compute(&ctx, &pkt);
	if (ret) {
		LOG_ERR("Failed to compute sha256: %d", ret);
		return -4;
	}

	memcpy(pOutDig, digest, MIN(eRPMC_DIGEST_SIZE, OutDigSize));
	return 0;
}

/*****************************************************************************
 * Function name:   hmac_sha2_256_engine
 * Description:     Compute HMAC-SHA256 signature for message authentication
 * @param           pInMsg     - Input message data to authenticate
 * @param           inMsgLen   - Length of input message
 * @param           pInKey     - HMAC key (must be 32 bytes)
 * @param           inKeySize  - Length of HMAC key
 * @param           pOutDig    - Output buffer for computed signature
 * @param           OutDigSize - Expected output signature size (must be 32 bytes)
 * @return          0 on success, negative on error
 * @note            Uses NPCX hardware HMAC accelerator for computation
 *****************************************************************************/
int hmac_sha2_256_engine(const uint8_t *pInMsg, uint8_t inMsgLen, const uint8_t *pInKey, uint8_t inKeySize, uint8_t *pOutDig, uint8_t OutDigSize)
{
	int ret = 0;
	uint8_t sig[eRPMC_SIGNATURE_SIZE] = {0};

	if (!pInMsg || !pInKey || !pOutDig) {
		LOG_ERR("Invalid arguments");
		return -1;
	}

	if ((inKeySize != eRPMC_ROOT_KEY_SIZE) || (OutDigSize != eRPMC_SIGNATURE_SIZE)) {
		LOG_ERR("Invalid arguments");
		return -2;
	}

	ret = npcx_hmac_compute(sha_dev, &ctx, pInKey, inKeySize, pInMsg, inMsgLen, sig);
	if(ret != 0) {
		LOG_ERR("Failed to compute hmac: %d", ret);
		return -3;
	}

	memcpy(pOutDig, sig, MIN(eRPMC_SIGNATURE_SIZE, OutDigSize));

	return 0;
}

#if 0 // Unit Test
/*****************************************************************************
 * Function name:   hmac_sha2_256_test
 * Description:     Unit test function for HMAC-SHA256 functionality
 * @param           None
 * @return          None
 * @note            Disabled - compile with #if 1 to enable test
 *****************************************************************************/
void hmac_sha2_256_test(void)
{
	int ret = 0;
	uint8_t out[32] = {0};
	const uint8_t key[40] = {
		0x6f, 0x35, 0x62, 0x8d,  0x65, 0x81, 0x34, 0x35,
		0x53, 0x4b, 0x5d, 0x67,  0xfb, 0xdb, 0x54, 0xcb,
		0x33, 0x40, 0x3d, 0x04,  0xe8, 0x43, 0x10, 0x3e,
		0x63, 0x99, 0xf8, 0x06,  0xcb, 0x5d, 0xf9, 0x5f,
		0xeb, 0xbd, 0xd6, 0x12,  0x36, 0xf3, 0x32, 0x45
	};

	const uint8_t data[128] = {
		0x75, 0x2c, 0xff, 0x52,  0xe4, 0xb9, 0x07, 0x68,
		0x55, 0x8e, 0x53, 0x69,  0xe7, 0x5d, 0x97, 0xc6,
		0x96, 0x43, 0x50, 0x9a,  0x5e, 0x59, 0x04, 0xe0,
		0xa3, 0x86, 0xcb, 0xe4,  0xd0, 0x97, 0x0e, 0xf7,
		0x3f, 0x91, 0x8f, 0x67,  0x59, 0x45, 0xa9, 0xae,
		0xfe, 0x26, 0xda, 0xea,  0x27, 0x58, 0x7e, 0x8d,
		0xc9, 0x09, 0xdd, 0x56,  0xfd, 0x04, 0x68, 0x80,
		0x5f, 0x83, 0x40, 0x39,  0xb3, 0x45, 0xf8, 0x55,
		0xcf, 0xe1, 0x9c, 0x44,  0xb5, 0x5a, 0xf2, 0x41,
		0xff, 0xf3, 0xff, 0xcd,  0x80, 0x45, 0xcd, 0x5c,
		0x28, 0x8e, 0x6c, 0x4e,  0x28, 0x4c, 0x37, 0x20,
		0x57, 0x0b, 0x58, 0xe4,  0xd4, 0x7b, 0x8f, 0xee,
		0xed, 0xc5, 0x2f, 0xd1,  0x40, 0x1f, 0x69, 0x8a,
		0x20, 0x9f, 0xcc, 0xfa,  0x3b, 0x4c, 0x0d, 0x9a,
		0x79, 0x7b, 0x04, 0x6a,  0x27, 0x59, 0xf8, 0x2a,
		0x54, 0xc4, 0x1c, 0xcd,  0x7b, 0x5f, 0x59, 0x2b
	};

	const uint8_t expected[32] = {
		0x05, 0xd1, 0x24, 0x3e,  0x64, 0x65, 0xed, 0x96, 
		0x20, 0xc9, 0xae, 0xc1,  0xc3, 0x51, 0xa1, 0x86,
		0x8e, 0x22, 0x51, 0xb9,  0x33, 0xa3, 0x94, 0x75,
		0x2a, 0xb1, 0x7b, 0xff,  0x99, 0xb8, 0x0e, 0x29
	};

	ret = hmac_sha2_256_engine(data, sizeof(data), key, sizeof(key), out, sizeof(out));
	if (ret != 0) {
		LOG_ERR("HMAC sha256 test failed result: %d\n", ret);
		return;
	}

	ret = strncmp(out, expected, sizeof(out));
	if (ret == 0) {
		LOG_INF("hamc sha256 computation PASS\n");
	} else {
		LOG_ERR("hmac sha256 computation FAIL\n");
		LOG_HEXDUMP_ERR(out, 32, "hmac_sha2_256_engine");
	}
}

/*****************************************************************************
 * Function name:   sha2_256_test
 * Description:     Unit test function for SHA256 functionality
 * @return          None
 * @note            Disabled - compile with #if 1 to enable test
 *****************************************************************************/
void sha2_256_test(void)
{
	int ret = 0;
	uint8_t out[32] = {0};
	uint8_t data[6] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06};
	const uint8_t expected[32] = {
		0x71, 0x92, 0x38, 0x5c,  0x3c, 0x06, 0x05, 0xde, 
		0x55, 0xbb, 0x94, 0x76,  0xce, 0x1d, 0x90, 0x74,
		0x81, 0x90, 0xec, 0xb3,  0x2a, 0x8e, 0xed, 0x7f,
		0x52, 0x07, 0xb3, 0x0c,  0xf6, 0xa1, 0xfe, 0x89
	};

	ret = sha2_256_engine(data, sizeof(data), out, sizeof(out));
	if (ret != 0) {
		LOG_ERR("sha2_256 test failed result: %d\n", ret);
		return;
	}

	ret = strncmp(out, expected, sizeof(out));
	if (ret == 0) {
		LOG_INF("sha2_256 computation PASS\n");
	} else {
		LOG_ERR("sha2_256 computation FAIL\n");
		LOG_HEXDUMP_ERR(out, 32, "sha2_256_engine");
	}
}

#endif