/*****************************************************************************
 * Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
 ******************************************************************************
 */
 

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <string.h>
#include "flashhdr.h"
#include "amd_crb_drivers_spiFlash.h"
#include "ec_version.h"
#include "system.h"

#ifndef EC_VERSION_STRING_IDX_MAX
#define EC_VERSION_STRING_IDX_MAX     10
#endif

#if (EC_VERSION_STRING_IDX_MAX > 10)
#error "The maximum string index must less than or equal to 10"
#endif

#ifndef EC_VERSION_STRING_0_VALUE
#define EC_VERSION_STRING_0_VALUE     ""
#endif
#ifndef EC_VERSION_STRING_1_VALUE
#define EC_VERSION_STRING_1_VALUE     ""
#endif
#ifndef EC_VERSION_STRING_2_VALUE
#define EC_VERSION_STRING_2_VALUE     ""
#endif
#ifndef EC_VERSION_STRING_3_VALUE
#define EC_VERSION_STRING_3_VALUE     ""
#endif
#ifndef EC_VERSION_STRING_4_VALUE
#define EC_VERSION_STRING_4_VALUE     ""
#endif
#ifndef EC_VERSION_STRING_5_VALUE
#define EC_VERSION_STRING_5_VALUE     ""
#endif
#ifndef EC_VERSION_STRING_6_VALUE
#define EC_VERSION_STRING_6_VALUE     ""
#endif
#ifndef EC_VERSION_STRING_7_VALUE
#define EC_VERSION_STRING_7_VALUE     ""
#endif
#ifndef EC_VERSION_STRING_8_VALUE
#define EC_VERSION_STRING_8_VALUE     ""
#endif
#ifndef EC_VERSION_STRING_9_VALUE
#define EC_VERSION_STRING_9_VALUE     ""
#endif

#ifndef BOOT_ROM_START
#define BOOT_ROM_START                  0x10060000
#endif

/**
* Define all possible string lengths.
*/
#define STRING_0_LENGTH         (sizeof(EC_VERSION_STRING_0_VALUE) - 1)
#define STRING_1_LENGTH         (sizeof(EC_VERSION_STRING_1_VALUE) - 1)
#define STRING_2_LENGTH         (sizeof(EC_VERSION_STRING_2_VALUE) - 1)
#define STRING_3_LENGTH         (sizeof(EC_VERSION_STRING_3_VALUE) - 1)
#define STRING_4_LENGTH         (sizeof(EC_VERSION_STRING_4_VALUE) - 1)
#define STRING_5_LENGTH         (sizeof(EC_VERSION_STRING_5_VALUE) - 1)
#define STRING_6_LENGTH         (sizeof(EC_VERSION_STRING_6_VALUE) - 1)
#define STRING_7_LENGTH         (sizeof(EC_VERSION_STRING_7_VALUE) - 1)
#define STRING_8_LENGTH         (sizeof(EC_VERSION_STRING_8_VALUE) - 1)
#define STRING_9_LENGTH         (sizeof(EC_VERSION_STRING_9_VALUE) - 1)

/* The length of the string must be equal to the length of the string in the ec_version.h */
BUILD_ASSERT(STRING_0_LENGTH == 10);
BUILD_ASSERT(STRING_1_LENGTH == 10);
BUILD_ASSERT(STRING_2_LENGTH == 10);
BUILD_ASSERT(STRING_3_LENGTH == 5);
BUILD_ASSERT(STRING_4_LENGTH == 5);

/**
* Define all possible string table entry offsets.
*/
#define STRING_0_INDEX          0
#define STRING_1_INDEX          (STRING_0_INDEX + STRING_0_LENGTH)
#define STRING_2_INDEX          (STRING_1_INDEX + STRING_1_LENGTH)
#define STRING_3_INDEX          (STRING_2_INDEX + STRING_2_LENGTH)
#define STRING_4_INDEX          (STRING_3_INDEX + STRING_3_LENGTH)
#define STRING_5_INDEX          (STRING_4_INDEX + STRING_4_LENGTH)
#define STRING_6_INDEX          (STRING_5_INDEX + STRING_5_LENGTH)
#define STRING_7_INDEX          (STRING_6_INDEX + STRING_6_LENGTH)
#define STRING_8_INDEX          (STRING_7_INDEX + STRING_7_LENGTH)
#define STRING_9_INDEX          (STRING_8_INDEX + STRING_8_LENGTH)

/**
* Define start.
*/
__in_section(start_info, static, var) const  unsigned char startdata[64] = 
{
0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};


/**
* Define all g_pui8StringDatas.
*/
__in_section(ecfw_info, static, var) const  unsigned char g_pui8StringData[] = 
{
	EC_VERSION_STRING_0_VALUE
	EC_VERSION_STRING_1_VALUE
	EC_VERSION_STRING_2_VALUE
	EC_VERSION_STRING_3_VALUE
	EC_VERSION_STRING_4_VALUE
	EC_VERSION_STRING_5_VALUE
	EC_VERSION_STRING_6_VALUE
	EC_VERSION_STRING_7_VALUE
	EC_VERSION_STRING_8_VALUE
	EC_VERSION_STRING_9_VALUE
};
/**
* Define an table of string pointers.
*/
static const unsigned char* g_pui8StringTable[EC_VERSION_STRING_IDX_MAX] =
{
#if (EC_VERSION_STRING_IDX_MAX > 0)
	&g_pui8StringData[STRING_0_INDEX],
#endif
#if (EC_VERSION_STRING_IDX_MAX > 1)
	&g_pui8StringData[STRING_1_INDEX],
#endif
#if (EC_VERSION_STRING_IDX_MAX > 2)
	&g_pui8StringData[STRING_2_INDEX],
#endif
#if (EC_VERSION_STRING_IDX_MAX > 3)
	&g_pui8StringData[STRING_3_INDEX],
#endif
#if (EC_VERSION_STRING_IDX_MAX > 4)
	&g_pui8StringData[STRING_4_INDEX],
#endif
#if (EC_VERSION_STRING_IDX_MAX > 5)
	&g_pui8StringData[STRING_5_INDEX],
#endif
#if (EC_VERSION_STRING_IDX_MAX > 6)
	&g_pui8StringData[STRING_6_INDEX],
#endif
#if (EC_VERSION_STRING_IDX_MAX > 7)
	&g_pui8StringData[STRING_7_INDEX],
#endif
#if (EC_VERSION_STRING_IDX_MAX > 8)
	&g_pui8StringData[STRING_8_INDEX],
#endif
#if (EC_VERSION_STRING_IDX_MAX > 9)
	&g_pui8StringData[STRING_9_INDEX],
#endif
};

/**
* Define an table of string lengths.
*/
static const unsigned char g_pui8StringLength[EC_VERSION_STRING_IDX_MAX] =
{
#if (EC_VERSION_STRING_IDX_MAX > 0)
	STRING_0_LENGTH,
#endif
#if (EC_VERSION_STRING_IDX_MAX > 1)
	STRING_1_LENGTH,
#endif
#if (EC_VERSION_STRING_IDX_MAX > 2)
	STRING_2_LENGTH,
#endif
#if (EC_VERSION_STRING_IDX_MAX > 3)
	STRING_3_LENGTH,
#endif
#if (EC_VERSION_STRING_IDX_MAX > 4)
	STRING_4_LENGTH,
#endif
#if (EC_VERSION_STRING_IDX_MAX > 5)
	STRING_5_LENGTH,
#endif
#if (EC_VERSION_STRING_IDX_MAX > 6)
	STRING_6_LENGTH,
#endif
#if (EC_VERSION_STRING_IDX_MAX > 7)
	STRING_7_LENGTH,
#endif
#if (EC_VERSION_STRING_IDX_MAX > 8)
	STRING_8_LENGTH,
#endif
#if (EC_VERSION_STRING_IDX_MAX > 9)
	STRING_9_LENGTH,
#endif
};

/**
 * ec_version_stringGet
 *
 * @param [in]   ui8String;     select version string
 * @param [in]  pui8Buffer;     the buffer pointer to get 
 * @param [in]   ui8Length;     the lenghth want ot get 
 *
 * @return 0 If successful.
 * @return -EIO General input / output error.
 *
 * @note
*/
unsigned char ec_version_stringGet(unsigned char ui8String, unsigned char *pui8Buffer, unsigned char ui8Length)
{
	unsigned char ui8Index;

	if (ui8String >= EC_VERSION_STRING_IDX_MAX) {
		return 0;
	}

	if (g_pui8StringLength[ui8String] == 0) {
		return 0;
	}

	if(ui8Length <= g_pui8StringLength[ui8String]) {
		return 0;
	}

	if (!pui8Buffer) {
		return 0;
	}

	for(ui8Index = 0; ui8Index < g_pui8StringLength[ui8String]; ui8Index++) {
		pui8Buffer[ui8Index] = g_pui8StringTable[ui8String][ui8Index];
	}
	pui8Buffer[ui8Index + 1] = 0;

	// Return the number of bytes copied
	return ui8Index;
}

/**
 * ec_version_checkEcFwVersion
 *
 * @return boot reason.
 * @return -EIO General input / output error.
 *
 * @note
 *   Check if FW version is aligned between External ROM and running EC code
 */
bool ec_version_checkEcFwVersion(void)
{
	uint8_t signature[16];
	uint8_t runingVersion[16];
	uint8_t romVersion[16];
	uint8_t runingDate[16];
	uint8_t romDate[16] = {0};
	uint8_t runingTime[16];
	uint8_t romTime[16];
	uint8_t startAddr[16];
	
	/* Read the EC APP start address from external ROM */
	amd_crb_drivers_sFlashRead(0, 0x00, 16, startAddr);
	
	/* Read the FW Date from external ROM */
	/* APP length (0xC0000 - EC_ROM_START) + 
	   Header length (0x80) + 
	   EC start address (0x200) + 
	   Version offset (sizeof(EC_VERSION_STRING_0_VALUE) + sizeof(EC_VERSION_STRING_1_VALUE))*/
	amd_crb_drivers_sFlashRead(0, ((FIX_ROM_LOCATION - BOOT_ROM_START) +
	                                                            0x80 +
									             (startAddr[4]) +
												 (startAddr[5] << 8) +
												 (startAddr[6] << 16) +
	                           sizeof(EC_VERSION_STRING_0_VALUE) - 1 +
			  sizeof(EC_VERSION_STRING_1_VALUE) - 1), 16, (uint8_t*)runingDate);
	
	/* Read the FW version from external ROM */
	/* APP length (0xC0000 - EC_ROM_START) + 
	   Header length (0x80) + 
	   EC start address (0x200) + 
	   Version offset (sizeof(EC_VERSION_STRING_0_VALUE) + .....)*/
	   amd_crb_drivers_sFlashRead(0, ((FIX_ROM_LOCATION - BOOT_ROM_START) +
									 0x80 +
									             (startAddr[4]) +
												 (startAddr[5] << 8) +
												 (startAddr[6] << 16) +
					sizeof(EC_VERSION_STRING_0_VALUE) - 1 +
					sizeof(EC_VERSION_STRING_1_VALUE) - 1 +
					sizeof(EC_VERSION_STRING_2_VALUE) - 1), 16, (uint8_t*)runingVersion);
									
	/* Read the FW time from external ROM */
	/* APP length (0xC0000 - EC_ROM_START) + 
	   Header length (0x80) + 
	   EC start address (0x200) + 
	   Version offset (sizeof(EC_VERSION_STRING_0_VALUE) + ......)*/
	   amd_crb_drivers_sFlashRead(0, ((FIX_ROM_LOCATION - BOOT_ROM_START) +
									 0x80 +
									             (startAddr[4]) +
												 (startAddr[5] << 8) +
												 (startAddr[6] << 16) +
					sizeof(EC_VERSION_STRING_0_VALUE) - 1 +
					sizeof(EC_VERSION_STRING_1_VALUE) - 1 +
					sizeof(EC_VERSION_STRING_2_VALUE) - 1 +
					sizeof(EC_VERSION_STRING_3_VALUE) - 1 +
					sizeof(EC_VERSION_STRING_4_VALUE) - 1 +
					sizeof(EC_VERSION_STRING_5_VALUE) - 1), 16, (uint8_t*)runingTime);

	/* Read the FW version signature */
	/* APP length (0xC0000 - EC_ROM_START) + 
	Header length (0x80) + 
	EC start address (0x200) */
	amd_crb_drivers_sFlashRead(0, ((FIX_ROM_LOCATION - BOOT_ROM_START) +
								  0x80 +
							      (startAddr[4]) +
								  (startAddr[5] << 8) +
								  (startAddr[6] << 16)), 
								  16, (uint8_t*)signature);
	/* 
	 * Assume EC_VERSION_STRING_0_VALUE never change. 
	 * Confirm the signature is read properly before additional check
	 */
		if (memcmp(signature, romDate, sizeof(EC_VERSION_STRING_0_VALUE) - 1) != 0) {
		return true;
	}
	
	/* Compare FW time, version and Date only if the FW version signature is read properly */
	ec_version_stringGet(6, romTime, 12);
	ec_version_stringGet(3, romVersion, 12);
	ec_version_stringGet(2, romDate, 12);
	
	if ((memcmp(runingDate, romDate, sizeof(EC_VERSION_STRING_2_VALUE) - 1) == 0) && 
		(memcmp(runingVersion, romVersion, sizeof(EC_VERSION_STRING_3_VALUE) - 1) == 0))
	{
		/* match */
		return true;
	}
	else
	{
		/* not match */
		return false;
	}
}