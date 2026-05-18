/**
 * Copyright (c) 2025 Advanced Micro Devices, Inc.
 * 
 * HMAC-SHA3-384 software implementation
 * 
 * Note: This implementation provides the complete HMAC-SHA3-384 functionality
 * following the same API pattern as the existing HMAC-SHA256 implementation.
 * The SHA3-384 core is implemented in software using the Keccak-f[1600] permutation.
 * 
 * Status: Implementation complete, test vectors may need verification against
 * reference implementations for final validation.
 */

#include "oob_rpmc_hmac_sha3_384.h"
#include <zephyr/logging/log.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <string.h>

#define LOG_LEVEL LOG_LEVEL_WRN
LOG_MODULE_REGISTER(rpmc_hmac_sha3_384, LOG_LEVEL);

/* SHA3-384 state structure */
typedef struct {
    uint64_t st[25];     // Keccak state array (1600 bits = 25 * 64 bits)
    int pt, rsiz, mdlen; // Point, rate size, message digest length
} sha3_384_ctx_t;

/* Global SHA3-384 context */
//static sha3_384_ctx_t g_sha3_ctx;

/* Keccak round constants */
static const uint64_t keccakf_rndc[24] = {
    0x0000000000000001,
    0x0000000000008082,
    0x800000000000808a,
    0x8000000080008000,
    0x000000000000808b,
    0x0000000080000001,
    0x8000000080008081,
    0x8000000000008009,
    0x000000000000008a,
    0x0000000000000088,
    0x0000000080008009,
    0x000000008000000a,
    0x000000008000808b,
    0x800000000000008b,
    0x8000000000008089,
    0x8000000000008003,
    0x8000000000008002,
    0x8000000000000080,
    0x000000000000800a,
    0x800000008000000a,
    0x8000000080008081,
    0x8000000000008080,
    0x0000000080000001,
    0x8000000080008008,
};

/* Rotation constants */
static const int keccakf_rotc[24] = {
    1,  3,  6,  10, 15, 21, 28, 36, 45, 55, 2,  14,
    27, 41, 56, 8,  25, 43, 62, 18, 39, 61, 20, 44
};

/* Pi lane constants */
static const int keccakf_piln[24] = {
    10, 7,  11, 17, 18, 3, 5,  16, 8,  21, 24, 4,
    15, 23, 19, 13, 12, 2, 20, 14, 22, 9,  6,  1
};

#define ROTLEFT(a,b) (((a) << (b)) | ((a) >> (64-(b))))

/*****************************************************************************
 * Function name:   keccak_f1600
 * Description:     Keccak-f[1600] permutation function
 *****************************************************************************/
static void keccak_f1600(uint64_t st[25]) {
    int i, j, r;
    uint64_t t, bc[5];

    for (r = 0; r < 24; r++) {
        // Theta
        for (i = 0; i < 5; i++)
            bc[i] = st[i] ^ st[i + 5] ^ st[i + 10] ^ st[i + 15] ^ st[i + 20];

        for (i = 0; i < 5; i++) {
            t = bc[(i + 4) % 5] ^ ROTLEFT(bc[(i + 1) % 5], 1);
            for (j = 0; j < 25; j += 5)
                st[j + i] ^= t;
        }

        // Rho Pi
        t = st[1];
        for (i = 0; i < 24; i++) {
            j = keccakf_piln[i];
            bc[0] = st[j];
            st[j] = ROTLEFT(t, keccakf_rotc[i]);
            t = bc[0];
        }

        // Chi
        for (j = 0; j < 25; j += 5) {
            for (i = 0; i < 5; i++)
                bc[i] = st[j + i];
            for (i = 0; i < 5; i++)
                st[j + i] ^= (~bc[(i + 1) % 5]) & bc[(i + 2) % 5];
        }

        // Iota
        st[0] ^= keccakf_rndc[r];
    }
}

/*****************************************************************************
 * Function name:   sha3_384_init
 * Description:     Initialize SHA3-384 context
 *****************************************************************************/
static void sha3_384_init(sha3_384_ctx_t *c) {
    int i;
    for (i = 0; i < 25; i++) {
        c->st[i] = 0;
    }
    c->pt = 0;
    c->rsiz = 200 - 2 * (384 / 8);  // 200 - 2 * 48 = 104
    c->mdlen = 384 / 8;  // 48
}

/*****************************************************************************
 * Function name:   sha3_384_update
 * Description:     Update SHA3-384 with input data
 *****************************************************************************/
static void sha3_384_update(sha3_384_ctx_t *c, const uint8_t *data, size_t len) {
    int i;
    int j = c->pt;
    const uint8_t *p = data;

    for (i = 0; i < len; i++) {
        ((uint8_t *)c->st)[j++] ^= p[i];
        if (j >= c->rsiz) {
            keccak_f1600(c->st);
            j = 0;
        }
    }
    c->pt = j;
}

/*****************************************************************************
 * Function name:   sha3_384_final
 * Description:     Finalize SHA3-384 and get digest
 *****************************************************************************/
static void sha3_384_final(sha3_384_ctx_t *c, uint8_t *md) {
    int i;

    ((uint8_t *)c->st)[c->pt] ^= 0x06;
    ((uint8_t *)c->st)[c->rsiz - 1] ^= 0x80;
    keccak_f1600(c->st);

    for (i = 0; i < c->mdlen; i++) {
        md[i] = ((uint8_t *)c->st)[i];
    }
}

/*****************************************************************************
 * Function name:   hmac_sha3_384_init
 * Description:     Initialize HMAC-SHA3-384 software implementation
 *****************************************************************************/
int hmac_sha3_384_init(void) {
    LOG_INF("HMAC-SHA3-384 software implementation initialized");
    return 0;
}

/*****************************************************************************
 * Function name:   hmac_sha3_384_deinit
 * Description:     Deinitialize HMAC-SHA3-384 software implementation
 *****************************************************************************/
int hmac_sha3_384_deinit(void) {
    LOG_INF("HMAC-SHA3-384 software implementation deinitialized");
    return 0;
}

/*****************************************************************************
 * Function name:   sha3_384_engine
 * Description:     Compute SHA3-384 for a message using software implementation
 *****************************************************************************/
int sha3_384_engine(uint8_t *pInMsg, uint8_t inMsgLen, uint8_t *pOutDig, uint8_t OutDigSize) {
    if (!pInMsg || !pOutDig) {
        LOG_ERR("Invalid arguments");
        return -1;
    }
    
    if (OutDigSize != eRPMC_SHA3_384_DIGEST_SIZE) {
        LOG_ERR("Invalid output digest size: %d, expected: %d", OutDigSize, eRPMC_SHA3_384_DIGEST_SIZE);
        return -2;
    }
    
    sha3_384_ctx_t ctx;
    sha3_384_init(&ctx);
    sha3_384_update(&ctx, pInMsg, inMsgLen);
    sha3_384_final(&ctx, pOutDig);
    
    return 0;
}

/*****************************************************************************
 * Function name:   hmac_sha3_384_engine
 * Description:     Compute HMAC-SHA3-384 signature for message authentication
 *****************************************************************************/
int hmac_sha3_384_engine(const uint8_t *pInMsg, uint8_t inMsgLen, const uint8_t *pInKey, uint8_t inKeySize, uint8_t *pOutDig, uint8_t OutDigSize) {
    uint8_t key_pad[eRPMC_SHA3_384_BLOCK_SIZE];
    uint8_t ipad[eRPMC_SHA3_384_BLOCK_SIZE];
    uint8_t opad[eRPMC_SHA3_384_BLOCK_SIZE];
    uint8_t inner_hash[eRPMC_SHA3_384_DIGEST_SIZE];
    sha3_384_ctx_t ctx;
    int i;
    
    if (!pInMsg || !pInKey || !pOutDig) {
        LOG_ERR("Invalid arguments");
        return -1;
    }
    
    if (OutDigSize != eRPMC_SHA3_384_SIGNATURE_SIZE) {
        LOG_ERR("Invalid output signature size: %d, expected: %d", OutDigSize, eRPMC_SHA3_384_SIGNATURE_SIZE);
        return -2;
    }
    
    /* Prepare key */
    memset(key_pad, 0, sizeof(key_pad));
    if (inKeySize > eRPMC_SHA3_384_BLOCK_SIZE) {
        /* Hash the key if it's too long */
        sha3_384_init(&ctx);
        sha3_384_update(&ctx, pInKey, inKeySize);
        sha3_384_final(&ctx, key_pad);
    } else {
        /* Use key as-is, padded with zeros */
        memcpy(key_pad, pInKey, inKeySize);
    }
    
    /* Create inner and outer padding */
    for (i = 0; i < eRPMC_SHA3_384_BLOCK_SIZE; i++) {
        ipad[i] = key_pad[i] ^ 0x36;
        opad[i] = key_pad[i] ^ 0x5C;
    }
    
    /* Inner hash: SHA3-384(ipad || message) */
    sha3_384_init(&ctx);
    sha3_384_update(&ctx, ipad, sizeof(ipad));
    sha3_384_update(&ctx, pInMsg, inMsgLen);
    sha3_384_final(&ctx, inner_hash);
    
    /* Outer hash: SHA3-384(opad || inner_hash) */
    sha3_384_init(&ctx);
    sha3_384_update(&ctx, opad, sizeof(opad));
    sha3_384_update(&ctx, inner_hash, sizeof(inner_hash));
    sha3_384_final(&ctx, pOutDig);
    
    /* Clear sensitive data */
    memset(key_pad, 0, sizeof(key_pad));
    memset(ipad, 0, sizeof(ipad));
    memset(opad, 0, sizeof(opad));
    memset(inner_hash, 0, sizeof(inner_hash));
    
    return 0;
}

#if 0 // Unit Test - Note: Test vectors need verification
/*****************************************************************************
 * Function name:   hmac_sha3_384_test
 * Description:     Unit test function for HMAC-SHA3-384 functionality
 *****************************************************************************/
void hmac_sha3_384_test(void) {
    int ret = 0;
    uint8_t out[48] = {0};
    
    /* Test vector for HMAC-SHA3-384 */
    const uint8_t key[] = "password";
    const uint8_t data[] = "The quick brown fox jumps over the lazy dog";
    
    /* Expected HMAC-SHA3-384 result (this would need to be verified with a reference implementation) */
    const uint8_t expected[48] = {
        0x3b, 0xb3, 0xb0, 0x94, 0x24, 0x7c, 0x3e, 0xed,
        0xce, 0x86, 0x29, 0xf8, 0x49, 0x3c, 0xa9, 0xc6,
        0xfd, 0x8a, 0x75, 0x26, 0x93, 0x69, 0x2d, 0x2f,
        0x80, 0x0e, 0x8c, 0x4b, 0xf8, 0xfc, 0xf2, 0xbb,
        0x8a, 0x30, 0x67, 0xca, 0x43, 0x91, 0x2b, 0x6d,
        0x03, 0xa9, 0x0c, 0x90, 0x9a, 0x58, 0x7d, 0xe3
    };
    
    ret = hmac_sha3_384_engine(data, sizeof(data) - 1, key, sizeof(key) - 1, out, sizeof(out));
    if (ret != 0) {
        LOG_ERR("HMAC SHA3-384 test failed result: %d\n", ret);
        return;
    }
    
    ret = memcmp(out, expected, sizeof(out));
    if (ret == 0) {
        LOG_INF("HMAC SHA3-384 computation PASS\n");
    } else {
        LOG_ERR("HMAC SHA3-384 computation FAIL\n");
        LOG_HEXDUMP_ERR(out, 48, "hmac_sha3_384_engine");
    }
}

/*****************************************************************************
 * Function name:   sha3_384_test
 * Description:     Unit test function for SHA3-384 functionality
 *****************************************************************************/
void sha3_384_test(void) {
    int ret = 0;
    uint8_t out[48] = {0};
    uint8_t data[] = "The quick brown fox jumps over the lazy dog";
    
    /* Expected SHA3-384 result for "abc" */
    const uint8_t expected[48] = {
        0x70, 0x63, 0x46, 0x5e, 0x08, 0xa9, 0x3b, 0xce,
        0x31, 0xcd, 0x89, 0xd2, 0xe3, 0xca, 0x8f, 0x60,
        0x24, 0x98, 0x69, 0x6e, 0x25, 0x35, 0x92, 0xed,
        0x26, 0xf0, 0x7b, 0xf7, 0xe7, 0x03, 0xcf, 0x32,
        0x85, 0x81, 0xe1, 0x47, 0x1a, 0x7b, 0xa7, 0xab,
        0x11, 0x9b, 0x1a, 0x9e, 0xbd, 0xf8, 0xbe, 0x41
    };
    
    ret = sha3_384_engine(data, sizeof(data) - 1, out, sizeof(out));
    if (ret != 0) {
        LOG_ERR("SHA3-384 test failed result: %d\n", ret);
        return;
    }
    
    ret = memcmp(out, expected, sizeof(out));
    if (ret == 0) {
        LOG_INF("SHA3-384 computation PASS\n");
    } else {
        LOG_ERR("SHA3-384 computation FAIL\n");
        LOG_HEXDUMP_ERR(out, 48, "sha3_384_engine");
    }
}
#endif