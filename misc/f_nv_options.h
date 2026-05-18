/*****************************************************************************
 * Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
 ******************************************************************************
 */

#ifndef __U_OPTIONS_H__
#define __U_OPTIONS_H__

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <assert.h>
#include "eeprom.h"
#include "ec_version.h"
#include "u_crc8.h"


#define NV_OPTIONS_SIZE                (48)
#define NV_OPTIONS_MAGIC               (EC_VERSION_U32_MAGIC)

#define DEF_NV_OPTIONS(name, bits)      uint32_t name : bits

#define GET_NV_OPTIONS(name, Lval)      do {                                      \
                                          assert(g_ptrNvSpace->f.magic == NV_OPTIONS_MAGIC); \
                                          Lval = g_ptrNvSpace->f.name;          \
                                        } while (0)

#define SET_NV_OPTIONS(name, Rval)      do {                                      \
                                          assert(g_ptrNvSpace->f.magic == NV_OPTIONS_MAGIC); \
                                          if (g_ptrNvSpace->f.name != (Rval)) { \
                                            g_ptrNvSpace->f.name = (Rval);      \
                                            g_ptrNvSpace->f.hasUpdate = 1;        \
                                          }                                       \
                                        } while (0)

#include "board_nvoption.h"

//#pragma pack(push,1)
typedef union {
  uint8_t arr[NV_OPTIONS_SIZE];
  struct {
    /* fixed field */
    uint32_t length;

    /* platform specific options - begin */
    NV_OPTIONS_DECLARATION;
    /* platform specific options - end */

    /* fixed field */
    uint32_t magic;
    uint8_t  crc8_alg0;
    uint8_t  crc8_alg1;
    uint8_t  reserved;
	    /*
     * 'hasUpdate' is last field of the whole structure,
     * Loader will check its offset doesn't exceed the boundary.
     */
    uint8_t  hasUpdate;
  } f;
} EC_NV_OPTIONS;

struct EC_NV_OPTIONS_F {
  /* fixed field */
  uint32_t length;

  /* platform specific options - begin */
  NV_OPTIONS_DECLARATION;
  /* platform specific options - end */

  /* fixed field */
  uint32_t magic;
  uint8_t  crc8_alg0;
  uint8_t  crc8_alg1;
  uint8_t  reserved;
  /*
   * 'hasUpdate' is last field of the whole structure,
   * Loader will check its offset doesn't exceed the boundary.
   */
  uint8_t  hasUpdate;
}  ;

//#pragma pack(pop)

extern EC_NV_OPTIONS * g_ptrNvSpace;

int f_nv_options_NvStorageAccess(bool isRead, uint16_t offset, void * pBuf, uint32_t len);
bool f_nv_options_load(void);
bool f_nv_options_save(void);
bool f_nv_options_init();

#endif // end of __U_OPTIONS_H__

