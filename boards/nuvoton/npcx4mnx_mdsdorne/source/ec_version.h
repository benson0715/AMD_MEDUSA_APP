/*****************************************************************************
 * Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
 ******************************************************************************
 */

#ifndef __EC_VERSION_H__
#define __EC_VERSION_H__

#define FIX_ROM_LOCATION   0x10070000   /* RO start from 0x10070000 with length 0x68000 */

#define EC_VERSION_STRING_IDX_MAX     7 /* The max index of the ec version string */

#define EC_VERSION_STRING_0_VALUE     "NPCX_498S_"         /* 0x2000: 10-bytes */
#define EC_VERSION_STRING_1_VALUE     "MDS_DORNE_"         /* 0x200A: 10-bytes */
#define EC_VERSION_STRING_2_VALUE     "2026-03-10"         /* 0x2014: 10-bytes */
#define EC_VERSION_STRING_3_VALUE     "0.1.3"              /* 0x201E:  5-bytes */
#define EC_VERSION_STRING_4_VALUE     "7321 "              /* 0x2023:  5-bytes */
#define EC_VERSION_STRING_5_VALUE     "aa30b03."           /* 0x2028:  Repository Version; Local changes flag(*|.) */
#define EC_VERSION_STRING_6_VALUE     "163601"             /*          Build time */
#define EC_VERSION_STRING_7_VALUE     "7487980e186532191c16f0664ee48e3a218f896a" /* VERSION SHA1SUM */
#define EC_VERSION_U32_MAGIC          0x1977D231ul         /* uint32_t magic of SHA1SUM */

#define VERSION_STR(a)                EC_VERSION_STRING_##a##_VALUE
#define VERSION_CHAR(a, b)            *( ((const char *)VERSION_STR(a)) + b )

bool ec_version_checkEcFwVersion(void);
uint8_t ec_version_stringGet(uint8_t ui8String, uint8_t *pui8Buffer, uint8_t ui8Length);
#endif /* __EC_VERSION_H__ */

