/**
* Copyright (c) 2025 Advanced Micro Devices, Inc.
*/

#ifndef __OOB_RPMC_HMAC_SHA3_384_H__
#define __OOB_RPMC_HMAC_SHA3_384_H__

#include <stdint.h>
#include <stdbool.h>
#include <zephyr/toolchain/gcc.h>

/* SHA3-384 constants */
#define eRPMC_SHA3_384_DIGEST_SIZE    (48u)  // SHA3-384 digest size in bytes
#define eRPMC_SHA3_384_BLOCK_SIZE     (104u) // SHA3-384 block size in bytes (832 bits)
#define eRPMC_SHA3_384_KEY_SIZE       (48u)  // Recommended key size for HMAC-SHA3-384
#define eRPMC_SHA3_384_SIGNATURE_SIZE (48u)  // SHA3-384 signature size in bytes

/*****************************************************************************
 * Function name:   hmac_sha3_384_init
 * Description:     Initialize HMAC-SHA3-384 software implementation
 * @return          0 on success, negative on error
 * @note            Sets up software SHA3-384 implementation
 *****************************************************************************/
int hmac_sha3_384_init(void);

/*****************************************************************************
 * Function name:   hmac_sha3_384_deinit
 * Description:     Deinitialize HMAC-SHA3-384 software implementation
 * @return          0 on success, negative on error
 * @note            Cleanup software SHA3-384 implementation
 *****************************************************************************/
int hmac_sha3_384_deinit(void);

/*****************************************************************************
 * Function name:   hmac_sha3_384_engine
 * Description:     Compute HMAC-SHA3-384 signature for message authentication
 * @param           pInMsg     - Input message data to authenticate
 * @param           inMsgLen   - Length of input message
 * @param           pInKey     - HMAC key (recommended 48 bytes)
 * @param           inKeySize  - Length of HMAC key
 * @param           pOutDig    - Output buffer for computed signature
 * @param           OutDigSize - Expected output signature size (must be 48 bytes)
 * @return          0 on success, negative on error
 * @note            Uses software SHA3-384 implementation for computation
 *****************************************************************************/
int hmac_sha3_384_engine(const uint8_t *pInMsg, uint8_t inMsgLen, const uint8_t *pInKey, uint8_t inKeySize, uint8_t *pOutDig, uint8_t OutDigSize);

/*****************************************************************************
 * Function name:   sha3_384_engine
 * Description:     Compute SHA3-384 for a message using software implementation
 * @param           pInMsg   - Pointer to input message buffer
 * @param           inMsgLen - Length of the input message
 * @param           pOutDig  - Pointer to output buffer for the SHA3-384 result
 * @param           OutDigSize - Length of the output buffer (should be 48 bytes)
 * @return          0 on success, negative on error
 *****************************************************************************/
int sha3_384_engine(uint8_t *pInMsg, uint8_t inMsgLen, uint8_t *pOutDig, uint8_t OutDigSize);

#endif
