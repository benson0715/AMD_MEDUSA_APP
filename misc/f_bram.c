/*****************************************************************************
 * Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
 ******************************************************************************
 */

#include "f_bram.h"
#include <assert.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(f_bram, CONFIG_BRAM_LOG_LEVEL);

#ifndef LOG_BRAM
#define LOG_BRAM      0
#endif

#if (LOG_BRAM && LOG_ENABLE)
#define LOGBRAM(frmt, ...)           LOG_INF("[BRAM]     "frmt, ##__VA_ARGS__)
#else
#define LOGBRAM(frmt, ...)           while(0){}
#endif

// Split the 32 DWORD cells into 2 group to reduce conflict
//static MD_MUTEX_RW_LOCK _s_rwLock0 = {0};  /* for even cells */
//static MD_MUTEX_RW_LOCK _s_rwLock1 = {0};  /* for odd cells */
#if CONFIG_SOC_SERIES_NPCX4
static F_BRAM_SPACE * _s_bramPtr = ((F_BRAM_SPACE * )0x400AF000);
#else
static F_BRAM_SPACE * _s_bramPtr = ((F_BRAM_SPACE * )0x4000A800);
#endif

static uint8_t _s_pageSelector = 0;
static uint32_t _s_bootReason = 0;


/**
 * f_bram_setSig 
 *
 * @param [in]   sig;     
 *
 * @note
 *  set bram sign
 */
void f_bram_setSig (uint32_t sig)
{
	LOGBRAM("set boot SIG = %c %c %c %c", (char)sig, (char)(sig >> 8), (char)(sig >> 16), (char)(sig >> 24));
	//_s_bramPtr->Comm.bramSig = sig;
	_s_bramPtr->u8Arr[0] = (char)sig;
	_s_bramPtr->u8Arr[1] = (char)(sig >> 8);
	_s_bramPtr->u8Arr[2] = (char)(sig >> 16);
	_s_bramPtr->u8Arr[3] = (char)(sig >> 24);
}

/**
 * f_bram_onBootUp 
 *
 * @note
 *  bram boot up
 */
void f_bram_onBootUp (void)
{
	_s_bootReason = _s_bramPtr->u8Arr[0]|(_s_bramPtr->u8Arr[1]<<8)|(_s_bramPtr->u8Arr[2]<<16)|(_s_bramPtr->u8Arr[3]<<24);
	LOGBRAM("OnBootup-SIG = %c %c %c %c", (char)_s_bootReason, (char)(_s_bootReason >> 8), (char)(_s_bootReason >> 16), (char)(_s_bootReason >> 24));

	switch (_s_bootReason) {
		case F_BRAM_VCI_BOOT_SIGNATURE:        // fall-through
		case F_BRAM_WDT_BOOT_SIGNATURE:
		case F_BRAM_FLASH_BIOS_WDT_BOOT_SIGNATURE:
		case F_BRAM_NV_OPTION_UPDATE_SIGNATURE:
			/* dump BRAM */
			break;
		case F_BRAM_UNEXPECTED_BOOT_SIGNATURE: // fall-through
			LOG_ERR("Boot to off-chip FW from unexpected shutdown !!");
			memset(_s_bramPtr, 0, F_BRAM_RAM_SIZE);
			/*  keep _s_bootReason as it is, i.e. F_BRAM_UNEXPECTED_BOOT_SIGNATURE */
			break;

		default:
			memset(_s_bramPtr, 0, F_BRAM_RAM_SIZE);
			/* for all other signatures in addition to above, override it to COLD_BOOT. */
			_s_bootReason = F_BRAM_COLD_BOOT_SIGNATURE;
			break;
	}

	f_bram_setSig(F_BRAM_UNEXPECTED_BOOT_SIGNATURE);
	LOGBRAM("Boot Reason  = %c %c %c %c", (char)_s_bootReason, (char)(_s_bootReason >> 8), (char)(_s_bootReason >> 16), (char)(_s_bootReason >> 24));
}

/**
 * f_bram_bootReason 
 *
 * @return boot reason.
 * @return -EIO General input / output error.
 *
 * @note
 *  get boot reason.
 */
uint32_t f_bram_bootReason (void)
{
	return _s_bootReason;
}

/**
 * f_bram_bootReasonClear
 *
 * @note
 *  clear boot reason.
 */
void f_bram_bootReasonClear (void)
{
	/* revert it to COLD_BOOT as default. */
	_s_bootReason = F_BRAM_COLD_BOOT_SIGNATURE;
}


/**
 * f_bram_getRow
 *
 * @param [in]   r;   index offset 
 *
 * @return 0 If successful.
 * @return -EIO General input / output error.
 *
 * @note
 *  get row data from bram
 */
uint32_t f_bram_getRow(uint8_t r)
{
	assert(r < F_BRAM_RAM_SIZE/sizeof(uint32_t));
	assert(r != 0); /* first row is allocated for boot-reason */

	int key = irq_lock();
	uint32_t u32Ret;

	//u32Ret = _s_bramPtr->u32Arr[r];
	u32Ret =  _s_bramPtr->u8Arr[r*4]|(_s_bramPtr->u8Arr[r*4+1]<<8)|(_s_bramPtr->u8Arr[r*4+2]<<16)|(_s_bramPtr->u8Arr[r*4+3]<<24);
	irq_unlock(key);

	LOGBRAM("GetRow %2d - %08X", r, u32Ret);
	return u32Ret;
}

/**
 * f_bram_setRow
 *
 * @param [in]        r;     index offset 
 * @param [in]   u32Val;     the data need to set in row
 * @param [in]   u32Msk;     mark 
 *
 * @return 0 If successful.
 * @return -EIO General input / output error.
 *
 * @note
 *  Set the row as mask specified field
 * Example: u32Val = 0x11223344; u32Msk = 0x0FF000F0;
 *          Effect field = 0x0FF000F0;
 *          Set Value    = 0x01200040;
 */
void f_bram_setRow(uint8_t r, uint32_t u32Val, uint32_t u32Msk)
{
	assert(r < F_BRAM_RAM_SIZE/sizeof(uint32_t));
	assert(r != 0); // first row is allocated for boot-reason

	uint32_t u32tmp;

//	u32tmp = _s_bramPtr->u32Arr[r];
	u32tmp = _s_bramPtr->u8Arr[r*4]|(_s_bramPtr->u8Arr[r*4+1]<<8)|(_s_bramPtr->u8Arr[r*4+2]<<16)|(_s_bramPtr->u8Arr[r*4+3]<<24);
	u32tmp &= ~u32Msk;
	u32tmp |= (u32Val & u32Msk);
	_s_bramPtr->u8Arr[0] = (char)u32tmp;
	_s_bramPtr->u8Arr[1] = (char)(u32tmp >> 8);
	_s_bramPtr->u8Arr[2] = (char)(u32tmp >> 16);
	_s_bramPtr->u8Arr[3] = (char)(u32tmp >> 24);
	//_s_bramPtr->u32Arr[r] = u32tmp;

	LOGBRAM("SetRow %2d - val %08X & msk %08X => %08X", r, u32Val, u32Msk, u32tmp);
}

/**
 * f_bram_acpiHandler 
 * 
 * @param [in]   isRead;     0 - to write, 1 - to read
 * @param [in]   ui8Idx;     registers' offset (see HPI spec.)
 * @param [in] pui8Data;     the data need to read or write through acpi
 * @return 0 If successful.
 * @return -EIO General input / output error.
 *
 * @note
 *  Read/write the bram through acpi 
 */
uint8_t f_bram_acpiHandler (uint8_t isRead, uint8_t ui8Idx, uint8_t * pui8Data)
{
	ui8Idx -= F_BRAM_ACPI_SPACE_PAGE_BASE;

	if (ui8Idx == F_BRAM_ACPI_SPACE_PAGE_SELECTOR_OFFSET) {
		if (isRead) {
			*pui8Data = _s_pageSelector;
		} else if (*pui8Data < 3) {
			_s_pageSelector = *pui8Data;
		}

		return 1;
	} else {
		uint8_t offset = _s_pageSelector * F_BRAM_ACPI_SPACE_PAGE_SELECTOR_OFFSET + ui8Idx;
		if (offset < F_BRAM_RAM_SIZE) {
			if (isRead) {
				*pui8Data = _s_bramPtr->u8Arr[offset];
			} else {
				_s_bramPtr->u8Arr[offset] = *pui8Data;
			}

			return 1;
		}
	}

	return 0;
}