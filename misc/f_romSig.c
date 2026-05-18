/*****************************************************************************
 * Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
 ******************************************************************************
 */

#include "f_romSig.h"
#include <assert.h>
#include <zephyr/logging/log.h>
#include "flashhdr.h"
#include "amd_crb_drivers_spiFlash.h"

LOG_MODULE_REGISTER(f_romSig, CONFIG_ROMSIG_LOG_LEVEL);

F_ROMSIG_BLOCK g_xRomSig;

static bool _s_validFlag[F_ROMSIG_MAX_FIELDS] = {false};


/*
 * f_romSig_init
 *
 * @note
 *  initization rom sign
 */
void f_romSig_init()
{
	memset((void *)&g_xRomSig, 0xFF, sizeof(g_xRomSig));
	memset(_s_validFlag, 0, sizeof(_s_validFlag));

	amd_crb_drivers_sFlashRead(0, F_ROMSIG_OFFSET, F_ROMSIG_SIZE, (uint8_t *)&g_xRomSig);
	/* Verify checksum of each field */
	for (uint8_t f = 0; f < F_ROMSIG_SIZE; f += 8) {
		uint8_t checksum = 0;
		uint8_t allOne = 1;
		LOG_DBG("PD_SIG 0x%02X:", f);
		for(uint8_t i = f; i < f + 8; i++) {
			if (0xFF != g_xRomSig.buf[i])
				allOne = 0;
			checksum += g_xRomSig.buf[i];
			LOG_DBG(" %02X", g_xRomSig.buf[i]);
		}

		if (allOne) {
			/* empty */
			LOG_DBG(" -- (null)");
		} else if (0xFF == checksum) {
			/* valid field */
			_s_validFlag[(f / 8)] = true;
			LOG_DBG(" -- VALID");
		} else {
			/* invalid field */
			LOG_ERR(" -- !! ERROR !!");

			for(uint8_t i = f; i < f + 8; i++) {
				g_xRomSig.buf[i] = 0xFF;
			}
		}
	}

}

/*
 * f_romSig_isValid
 *
 * @param [in]   idx rom sign idx
 * @return 0 if rom is valid
 *
 * @note
 *  check if rom  is valid
 */
bool f_romSig_isValid(uint8_t idx)
{
	assert (idx < F_ROMSIG_MAX_FIELDS);

	return _s_validFlag[idx];
}

/*
 * f_romSig_getOffset
 *
 * @param [in]   idx rom sign idx
 * @return offset
 *
 * @note
 *  get offset by idx
 */
uint32_t f_romSig_getOffset(uint8_t idx)
{
	assert (idx < F_ROMSIG_MAX_FIELDS);

	if (_s_validFlag[idx]) {
		return g_xRomSig.sigArr[idx].offset;
	}

	return 0xFFFFFFFF;
}

/*
 * f_romSig_getSize
 *
 * @param [in]   idx rom sign idx
 * @return if flag enabled is size otherwise is 0
 *
 * @note
 *  get size by idx
 */
uint32_t f_romSig_getSize(uint8_t idx)
{
	assert (idx < F_ROMSIG_MAX_FIELDS);

	if (_s_validFlag[idx]) {
		return g_xRomSig.sigArr[idx].size;
	}

	return 0;
}