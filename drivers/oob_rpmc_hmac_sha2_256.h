/**
* Copyright (c) 2025 Advanced Micro Devices, Inc.
*/

#ifndef __OOB_RPMC_HMAC_SHA2_256_H__
#define __OOB_RPMC_HMAC_SHA2_256_H__

#include <stdint.h>
#include <stdbool.h>
#include <zephyr/toolchain/gcc.h>

/* RPMC HMAC-SHA256 constants */
#define eRPMC_ROOT_KEY_SIZE		(32u)  // SHA256 key size in bytes
#define eRPMC_SIGNATURE_SIZE	(32u)  // SHA256 signature size in bytes
#define eRPMC_DIGEST_SIZE		(32u)  // SHA256 digest size in bytes
#define eRPMC_SAH256_BLOCK_SIZE (64u)

/*****************************************************************************
 * Function name:   hmac_sha2_256_init
 * Description:     Initialize HMAC-SHA256 hardware device and session
 * @param           pCtx - HMAC context (currently unused)
 * @return          0 on success, negative on error
 * @note            Sets up SHA hardware device and begins hash session
 *****************************************************************************/
int hmac_sha2_256_init(void);

/*****************************************************************************
 * Function name:   hmac_sha2_256_deinit
 * Description:     free HMAC-SHA256 hardware device and session
 * @param           pCtx - HMAC context (currently unused)
 * @return          0 on success, negative on error
 * @note            Turn off SHA hardware device and free hash session
 *****************************************************************************/
int hmac_sha2_256_deinit(void);

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
int hmac_sha2_256_engine(const uint8_t *pInMsg, uint8_t inMsgLen, const uint8_t *pInKey, uint8_t inKeySize, uint8_t *pOutDig, uint8_t OutDigSize);

/*****************************************************************************
 * Function name:   sha2_256_engine
 * Description:     Compute SHA256 for a message using hardware engine
 * @param           pInMsg   - Pointer to input message buffer
 * @param           inMsgLen - Length of the input message
 * @param           pOutDig  - Pointer to output buffer for the SHA256 result
 * @param           OutDigSize - Length of the output buffer (should be 32 bytes)
 * @return          0 on success, negative on error
 *****************************************************************************/
int sha2_256_engine(uint8_t *pInMsg, uint8_t inMsgLen, uint8_t *pOutDig, uint8_t OutDigSize);

#endif

