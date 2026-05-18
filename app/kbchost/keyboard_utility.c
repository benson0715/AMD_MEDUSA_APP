/**
* Copyright (c) 2019 Intel Corporation
* Modifications copyright (c) 2023 Advanced Micro Devices, Inc.
*/

#include <zephyr/kernel.h>
#include "keyboard_utility.h"

#define START_BREAK_CODE		0xf0U

static bool found_break_code;

/**
 * @brief Translate keyboard scan code between scan code sets
 *
 * @param scan_code The scan code set to use for translation
 * @param data Pointer to the scan code data to translate
 * @return 0 on success, -EINVAL if break code found, -ENOTSUP if unsupported scan code set
 */
int translate_key(enum scan_code_set scan_code, uint8_t *data)
{
	switch (scan_code) {
	case SCAN_CODE_SET1:
		/* Do nothing */
		break;
	case SCAN_CODE_SET2:
		/* If scan code set 2, then translate to set 1 */
		if (*data == START_BREAK_CODE) {
			found_break_code = true;
			return -EINVAL;
		}
		*data = kb_translation_table[*data];
		if (found_break_code) {
			*data |= 0x80;
			found_break_code = false;
		}
		break;
	default:
		return -ENOTSUP;
	}

	return 0;
}