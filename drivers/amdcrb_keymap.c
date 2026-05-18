/*****************************************************************************
 * Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
 ******************************************************************************
 */

#include "kbs_keymap.h"
#include <zephyr/logging/log.h>

/****************************************************************************/
/*  Celadon Scan Matrix LAYOUT                                              */
/*                                                                          */
/*  Sense7  Sense6  Sense5  Sense4  Sense3  Sense2  Sense1  Sense0          */
/*|-------+-------+-------+-------+-------+-------+-------+-------|         */
/*|       |       |       |       |       |       |  CRSR |       | Scan  0 */
/*|  (86) |  (85) |  (80) |  (76) | (124) |  (14) |  (89) |  (81) | (KEY #) */
/*|-------+-------+-------+-------+-------+-------+-------+-------|         */
/*|       |       |       |       |       |       |       |       | Scan  1 */
/*|  (29) |  (28) |  (15) |  (13) |  (90) | (126) |  (43) |  (41) | (KEY #) */
/*|-------+-------+-------+-------+-------+-------+-------+-------|         */
/*|       |       |       |       |       |       |  CRSU |       | Scan  2 */
/*|  (40) |  (27) |  (26) |  (11) | (123) |  (12) |  (83) |  (55) | (KEY #) */
/*|-------+-------+-------+-------+-------+-------+-------+-------|         */
/*|       |       |       | R Ctrl|       |       |       |       | Scan  3 */
/*|       |       |       |  (64) |       |       |  (58) |       | (KEY #) */
/*|-------+-------+-------+-------+-------+-------+-------+-------|         */
/*|  CRSL |       |       |       |   F9  |   F8  |       |       | Scan  4 */
/*|  (79) | (129) |       |  (56) | (120) | (119) | (132) | (133) | (KEY #) */
/*|-------+-------+-------+-------+-------+-------+-------+-------|         */
/*|       |       |       |   F5  |   F6  |   F7  |  DEL  |       | Scan  5 */
/*|  (38) |  (24) |  (9)  | (116) | (117) | (118) |  (76) |  (53) | (KEY #) */
/*|-------+-------+-------+-------+-------+-------+-------+-------|         */
/*|       |       |       |       |       |       |       |       | Scan  6 */
/*|       |       |       |       |       |       |  (44) |  (57) | (KEY #) */
/*|-------+-------+-------+-------+-------+-------+-------+-------|         */
/*|       |       |       |  F10  |       |       |  CRSD |       | Scan  7 */
/*|  (39) |  (25) |  (10) | (121) | (122) |       |  (84) |  (54) | (KEY #) */
/*|-------+-------+-------+-------+-------+-------+-------+-------|         */
/*|       |       |       |       |       |       |       |       | Scan  8 */
/*|  (37) |  (23) |  (8)  |  (7)  |  (22) |  (36) |  (51) |  (52) | (KEY #) */
/*|-------+-------+-------+-------+-------+-------+-------+-------|         */
/*|       |       |       |       |       |       |       |       | Scan  9 */
/*|  (34) |  (20) |  (5)  |  (6)  |  (21) |  (35) |  (50) |  (49) | (KEY #) */
/*|-------+-------+-------+-------+-------+-------+-------+-------|         */
/*|       |       |       |  Esc  |   F1  |   F2  |       |       | Scan 10 */
/*|  (32) |  (18) |  (3)  | (110) | (112) | (113) |  (46) |  (47) | (KEY #) */
/*|-------+-------+-------+-------+-------+-------+-------+-------|         */
/*|       |       |       |       |   F4  |   F3  |       |       | Scan 11 */
/*|  (33) |  (19) |  (4)  |  (30) | (115) | (114) |  (61) |  (48) | (KEY #) */
/*|-------+-------+-------+-------+-------+-------+-------+-------|         */
/*|       |       |       |       |       |       |       |       | Scan 12 */
/*|       |       |       |       |       |       |  (60) |  (62) | (KEY #) */
/*|-------+-------+-------+-------+-------+-------+-------+-------|         */
/*|       |       |       |       |       |   Fn  |       |       | Scan 13 */
/*|  (31) |  (17) |  (2)  |  (1)  |  (16) |  (59) |       | (131) | (KEY #) */
/*|-------+-------+-------+-------+-------+-------+-------+-------|         */
/*|       |       |       |       |       |       | L-WIN |       | Scan 14 */
/*|       |       |       |       |       |       | (127) |       | (KEY #) */
/*|-------+-------+-------+-------+-------+-------+-------+-------|         */
/*|       |       |       |       |       |       |       |       | Scan 15 */
/*|       |       |       | (125) |       |       |       |       | (KEY #) */
/*+---------------------------------------------------------------+         */
/*  Sense7  Sense6  Sense5  Sense4  Sense3  Sense2  Sense1  Sense0          */
/*                                                                          */
/****************************************************************************/

LOG_MODULE_DECLARE(kbchost, CONFIG_KBCHOST_LOG_LEVEL);

int amdcrb_get_fn_key(uint8_t key_num, struct fn_data *data, bool pressed);

#if CONFIG_KSCAN_EC
#define MAX_MTX_KEY_COLS 16
#define MAX_MTX_KEY_ROWS 8
/* 0 in the first column  _, - is also marked as KM_RSVD */

/* Here we assign 255(KM_FN_KEY) to Fn on purpose since we don't have a
 * standard keymap which can give you an scan code using 59.
 * Also Fn does not produce scan codes. In the data sheet 59 is repated twice.
 */

#if defined(BRD_MDS_DORNE)
static const uint8_t amdcrb_keymap[MAX_MTX_KEY_COLS][MAX_MTX_KEY_ROWS] = {
	{123,     KM_RSVD, KM_RSVD, KM_RSVD, 86,      85,      81,      80     },
	{127,     KM_RSVD, KM_RSVD, KM_RSVD, KM_RSVD, KM_RSVD, KM_RSVD, KM_RSVD},
	{KM_RSVD, 60,      60,      KM_RSVD, KM_RSVD, KM_RSVD, KM_RSVD, KM_RSVD},
	{122,     40,      26,      KM_RSVD, 11,      41,      12,      27     },
	{76,      61,      43,      29,      15,      116,     121,     120    },
	{54,      39,      25,      55,      10,      118,     KM_RSVD, 119    },
	{53,      38,      24,      117,     9,       13,      28,      KM_RSVD},
	{KM_RSVD, KM_RSVD, KM_RSVD, KM_RSVD, KM_RSVD, KM_RSVD, KM_RSVD, 255    },
	{KM_RSVD, KM_RSVD, KM_RSVD, KM_RSVD, 57,      KM_RSVD, KM_RSVD, 44     },
	{KM_RSVD, KM_RSVD, KM_RSVD, KM_RSVD, KM_RSVD, 58,      58,      KM_RSVD},
	{89,      79,      84,      83,      KM_RSVD, KM_RSVD, KM_RSVD, KM_RSVD},
	{49,      34,      20,      50,      5,       21,      6,       35     },
	{52,      37,      23,      36,      8,       22,      51,      7      },

	{47,      32,      18,      30,      3,       KM_RSVD, KM_RSVD, 112    }, // KSO pin reverse
	{48,      33,      19,      KM_RSVD, 4,       115,     114,     113    },
	
	{46,      31,      17,      255,      2,      16,      1,       110    },
};
#elif defined(BRD_MDS_AERIS)
static const uint8_t amdcrb_keymap[MAX_MTX_KEY_COLS][MAX_MTX_KEY_ROWS] = {
 /* KSI0      KSI1     KSI2     KSI3     KSI4     KSI5     KSI6     KSI7 */
	{110,     16,      17,      1,       31,      2,       46,      131    }, //KSO0
	{115,     114,     19,      113,     33,      4,       48,      KM_RSVD}, //KSO1
	{45,      30,      18,      112,     32,      3,       47,      KM_RSVD}, //KSO2
	{KM_RSVD, KM_RSVD, KM_RSVD, 58,      KM_RSVD, KM_RSVD, 64,      KM_RSVD}, //KSO3
	{35,      21,      20,      6,       34,      5,       49,      50     }, //KSO4
	{36,      22,      23,      7,       37,      8,       52,      51     }, //KSO5
	{117,     28,      24,      13,      38,      9,       53,      56     }, //KSO6
	{132,     118,     25,      119,     39,      10,      54,      133    }, //KSO7
	{41,      27,      26,      12,      40,      11,      42,      55     }, //KSO8
	{KM_RSVD, 44,      KM_RSVD, KM_RSVD, KM_RSVD, KM_RSVD, 57,      KM_RSVD}, //KSO9
	{116,     15,      14,      120,     29,      121,     43,      61     }, //KSO10
	{KM_RSVD, KM_RSVD, KM_RSVD, 126,     KM_RSVD, 122,     KM_RSVD, 84     }, //KSO11
	{60,      KM_RSVD, KM_RSVD, KM_RSVD, KM_RSVD, 124,     KM_RSVD, 62     }, //KSO12
	/* KSO13 and KSO14 are reverted */
	{KM_RSVD, KM_RSVD, KM_RSVD, 80,     255,      86,      KM_RSVD, 79     }, //KSO14
	{83,      KM_RSVD, KM_RSVD, 75,      KM_RSVD, 123,     KM_RSVD, 89     }, //KSO13
	{KM_RSVD, KM_RSVD, 127,     81,      129,     85,      KM_RSVD, KM_RSVD}, //KSO15
};
#else
/* Two different keymaps are swapped on purpose for this keyboard. The keys to
 * swapped are 29 and 76. These keys are misplanced in the documentation
 */
static const uint8_t amdcrb_keymap[MAX_MTX_KEY_COLS][MAX_MTX_KEY_ROWS] = {
	{KM_RSVD, KM_RSVD, KM_RSVD, KM_RSVD, 125U,    KM_RSVD, KM_RSVD, KM_RSVD},
	{KM_RSVD, 127U,    KM_RSVD, KM_RSVD, KM_RSVD, KM_RSVD, KM_RSVD, KM_RSVD},
	{131U,    KM_RSVD, 255U,    16U,     1U,      2U,      17U,     31U},
	{62U,     60U,     KM_RSVD, KM_RSVD, KM_RSVD, KM_RSVD, KM_RSVD, KM_RSVD},
	{48U,     61U,     114U,    115U,    30U,     4U,      19U,     33U},
	{47U,     46U,     113U,    112U,    110U,    3U,      18U,     32U},
	{49U,     50U,     35U,     21U,     6U,      5U,      20U,     34U},
	{52U,     51U,     36U,     22U,     7U,      8U,      23U,     37U},
	{54U,     84U,     0U,      122U,    121U,    10U,     25U,     39U},
	{57U,     44U,     KM_RSVD, KM_RSVD, KM_RSVD, KM_RSVD, KM_RSVD, KM_RSVD},
	{53U,     76U,     118U,    117U,    116U,    9U,      24U,     38U},
	{133U,    132U,    119U,    120U,    56U,     KM_RSVD, 129U,    79U},
	{KM_RSVD, 58U,     KM_RSVD, KM_RSVD, 64U ,    KM_RSVD, KM_RSVD, KM_RSVD},
#ifndef CONFIG_EC_MODULE
	{41U,     43U,     126U,    90U,     13U,     15U,     28U,     29U},
	{55U,     83U,     12U,     123U,    11U,     26U,     27U,     40U},
#else
	{55U,     83U,     12U,     123U,    11U,     26U,     27U,     40U},
	{41U,     43U,     126U,    90U,     13U,     15U,     28U,     29U},
#endif
	{81U,     89U,     14U,     124U,    76U,     80U,     85U,     86U},
};
#endif


int amdcrb_get_keynum(uint8_t col, uint8_t row);

struct km_api amdcrb_keyboard_api = {
	.get_keynum = amdcrb_get_keynum,
	.get_fnkey = amdcrb_get_fn_key,
};

int amdcrb_get_keynum(uint8_t col, uint8_t row)
{
	if (col > MAX_MTX_KEY_COLS &&
	    row > MAX_MTX_KEY_ROWS) {
		return -EINVAL;
	}

	return amdcrb_keymap[col][row];
}
#else
/* We still want to compile the function that handles FN top row keys since
 * we want to test it via PS/2 keyboard
 */
struct km_api amdcrb_keyboard_api = {
	.get_keynum = NULL,
	.get_fnkey = amdcrb_get_fn_key,
};
#endif

void amdcrb_fn_key_led_set(uint8_t key_num)
{
	switch (key_num) {
#if defined(BRD_MDS_DORNE)
		case KM_F4_KEY: {
			if (gpio_read_pin(EC_F4_LED)) {
				gpio_write_pin(EC_F4_LED, 0);
			} else {
				gpio_write_pin(EC_F4_LED, 1);
			}
		}
		break;
		case KM_F10_KEY: {
			if (gpio_read_pin(EC_KB_BL_LED)) {
				gpio_write_pin(EC_KB_BL_LED, 0);
			} else {
				gpio_write_pin(EC_KB_BL_LED, 1);
			}
		}
		break;
#elif defined(BRD_MDS_AERIS)
		case KM_ESC_KEY: {
			if (gpio_read_pin(EC_FN_LED)) {
				gpio_write_pin(EC_FN_LED, 0);
			} else {
				gpio_write_pin(EC_FN_LED, 1);
			}
		}
		break;
#endif
		default:
			break;
	}
}

#if defined(BRD_MDS_DORNE)
int amdcrb_get_fn_key(uint8_t key_num, struct fn_data *data,
		     bool pressed)
{
	switch (key_num) {
	case KM_ESC_KEY:
		/* Do nothing. As long as the length is 0 we don't care
		 * to indicate whether the type is a scan code or an sci
		 */
		data->type = FN_SCAN_CODE;
		data->sc.typematic = false;
		data->sc.code[0] = SC_UNMAPPED;
		data->sc.len = 1U;
		break;
		break;

	/* Disable speaker */
	case KM_F1_KEY:
		data->type = FN_SCAN_CODE;
		data->sc.typematic = false;
		if (pressed) {
			data->sc.code[0] = 0xE0U;
			data->sc.code[1] = 0x23U;
			data->sc.len = 2U;
		} else {
			data->sc.code[0] = 0xE0U;
			data->sc.code[1] = 0xF0U;
			data->sc.code[2] = 0x23U;
			data->sc.len = 3U;
		}
		break;
		
	/* Multimedia scan code set 2: Volume down */
	case KM_F2_KEY:
		data->type = FN_SCAN_CODE;
		data->sc.typematic = false;
		if (pressed) {
			data->sc.code[0] = 0xE0U;
			data->sc.code[1] = 0x21U;
			data->sc.len = 2U;
			data->sc.typematic = true;
		} else {
			data->sc.code[0] = 0xE0U;
			data->sc.code[1] = 0xF0U;
			data->sc.code[2] = 0x21U;
			data->sc.len = 3U;
		}
		break;
		
	/* Multimedia scan code set 2: Volume up */
	case KM_F3_KEY:
		data->type = FN_SCAN_CODE;
		data->sc.typematic = false;
		if (pressed) {
			data->sc.code[0] = 0xE0U;
			data->sc.code[1] = 0x32U;
			data->sc.len = 2U;
			data->sc.typematic = true;
		} else {
			data->sc.code[0] = 0xE0U;
			data->sc.code[1] = 0xF0U;
			data->sc.code[2] = 0x32U;
			data->sc.len = 3U;
		}
		break;
		
	/*  Multimedia scan code set 2: Mute */
	case KM_F4_KEY:
		data->type = SCI_CODE;
		if (pressed) {
			data->sci_code = ACPI_SCI_MIC_MUTE;
		} else {
			data->sci_code = 0U;
		}
		break;
		
	/* SCI: Brightness down */
	case KM_F5_KEY:
		data->type = SCI_CODE;
		if (pressed) {
			data->sci_code = ACPI_SCI_BRIGHTNESS_DOWN;
		} else {
			data->sci_code  = 0U;
		}
		break;

	/* SCI: Brightness up */
	case KM_F6_KEY:
		data->type = SCI_CODE;
		if (pressed) {
			data->sci_code = ACPI_SCI_BRIGHTNESS_UP;
		} else {
			data->sci_code = 0U;
		}
		break;

	/* Scan code set 2: display switch */
	case KM_F7_KEY:
		data->type = SCI_CODE;
		if (pressed) {
			data->sci_code = ACPI_SCI_DISPLAY_SWITCH;
		} else {
			data->sci_code = 0U;
		}
		break;

	/* Cut fucntion */
	case KM_F8_KEY:
		data->type = FN_SCAN_CODE;
		if (pressed) {
			data->sci_code = ACPI_SCI_EDIT_CUT;
		} else {
			data->sci_code = 0U;
		}
		break;

	/* setting */
	case KM_F9_KEY:
		data->type = SCI_CODE;
		if (pressed) {
			data->sci_code = ACPI_SCI_COMPUTE_SETTING;
		} else {
			data->sci_code = 0U;
		}
		break;

	/* Scan code set 2: keyboard backlight */
	case KM_F10_KEY:
		data->type = SCI_CODE;
		if (pressed) {
			data->sci_code = ACPI_SCI_KEYPAD_BACKLIGHT;
		} else {
			data->sci_code = 0U;
		}
		break;

	/* Scan code set 2: Print screen */
	case KM_F11_KEY:
		data->type = FN_SCAN_CODE;
		data->sc.typematic = false;
		if (pressed) {
			data->sc.code[0] = 0xE0U;
			data->sc.code[1] = 0x12U;
			data->sc.code[2] = 0xE0U;
			data->sc.code[3] = 0x7CU;
			data->sc.len = 4U;
		} else {
			data->sc.code[0] = 0xE0U;
			data->sc.code[1] = 0xF0U;
			data->sc.code[2] = 0x7CU;
			data->sc.code[3] = 0xE0U;
			data->sc.code[4] = 0xF0U;
			data->sc.code[5] = 0x12U;
			data->sc.len = 6U;
		}
		break;
		
	/* Scan code set 2: Insert key */
	case KM_F12_KEY:
		data->type = FN_SCAN_CODE;
		data->sc.typematic = false;
		if (pressed) {
			data->sc.code[0] = 0xE0U;
			data->sc.code[1] = 0x70U;
			data->sc.len = 2U;
		} else {
			data->sc.code[0] = 0xE0U;
			data->sc.code[1] = 0xF0U;
			data->sc.code[2] = 0x70U;
			data->sc.len = 3U;
		}
		break;
		
	/* Multimedia scan code set 2: Volume down */
	case KM_LFT_ARROW_KEY:
		data->type = FN_SCAN_CODE;
		if (pressed) {
			data->sc.code[0] = 0xE0U;
			data->sc.code[1] = 0x21U;
			data->sc.len = 2U;
			data->sc.typematic=true;
		} else {
			data->sc.code[0] = 0xE0U;
			data->sc.code[1] = 0xF0U;
			data->sc.code[2] = 0x21U;
			data->sc.len = 3U;
			data->sc.typematic=false;
		}
		break;
		
	/* Multimedia scan code set 2: Volume up */
	case KM_RGT_ARROW_KEY:
		data->type = FN_SCAN_CODE;
		if (pressed) {
			data->sc.code[0] = 0xE0U;
			data->sc.code[1] = 0x32U;
			data->sc.len = 2U;
			data->sc.typematic=true;
		} else {
			data->sc.code[0] = 0xE0U;
			data->sc.code[1] = 0xF0U;
			data->sc.code[2] = 0x32U;
			data->sc.len = 3U;
			data->sc.typematic=false;
		}
		break;
		
	/* SCI: Brightness up */
	case KM_UP_ARROW_KEY:
		data->type = SCI_CODE;
		if (pressed) {
			data->sci_code = ACPI_SCI_BRIGHTNESS_UP;
		} else {
			data->sci_code = 0U;
		}
		break;
		
	/* SCI: Brightness down */
	case KM_DN_ARROW_KEY:
		data->type = SCI_CODE;
		if (pressed) {
			data->sci_code = ACPI_SCI_BRIGHTNESS_DOWN;
		} else {
			data->sci_code  = 0U;
		}
		break;
		
	/* Scan code set 2: APP */
	case KM_RCNTRL_KEY:
		data->type = FN_SCAN_CODE;
		data->sc.typematic = false;
		if (pressed) {
			data->sc.code[0] = 0xE0U;
			data->sc.code[1] = 0x2FU;
			data->sc.len = 2U;
		} else {
			data->sc.code[0] = 0xE0U;
			data->sc.code[1] = 0xF0U;
			data->sc.code[2] = 0x2FU;
			data->sc.len = 3U;
		}
		break;
	default:
		return -EINVAL;
	}

	if (pressed) {
		amdcrb_fn_key_led_set(key_num);
	}

	return 0;
}

#elif defined(BRD_MDS_AERIS)
int amdcrb_get_fn_key(uint8_t key_num, struct fn_data *data,
		     bool pressed)
{
	switch (key_num) {
	case KM_ESC_KEY:
		/* Do nothing. As long as the length is 0 we don't care
		 * to indicate whether the type is a scan code or an sci
		 */
		data->type = FN_SCAN_CODE;
		data->sc.typematic = false;
		data->sc.code[0] = SC_UNMAPPED;
		data->sc.len = 1U;
		break;
		break;

	/* SCI: Brightness down */
	case KM_F1_KEY:
		data->type = SCI_CODE;
		if (pressed) {
			data->sci_code = ACPI_SCI_BRIGHTNESS_DOWN;
		} else {
			data->sci_code  = 0U;
		}
		break;

	/* SCI: Brightness up */
	case KM_F2_KEY:
		data->type = SCI_CODE;
		if (pressed) {
			data->sci_code = ACPI_SCI_BRIGHTNESS_UP;
		} else {
			data->sci_code = 0U;
		}
		break;

	/* Disable speaker */
	case KM_F3_KEY:
		data->type = FN_SCAN_CODE;
		data->sc.typematic = false;
		if (pressed) {
			data->sc.code[0] = 0xE0U;
			data->sc.code[1] = 0x23U;
			data->sc.len = 2U;
		} else {
			data->sc.code[0] = 0xE0U;
			data->sc.code[1] = 0xF0U;
			data->sc.code[2] = 0x23U;
			data->sc.len = 3U;
		}
		break;

	/* Multimedia scan code set 2: Volume down */
	case KM_F4_KEY:
		data->type = FN_SCAN_CODE;
		data->sc.typematic = false;
		if (pressed) {
			data->sc.code[0] = 0xE0U;
			data->sc.code[1] = 0x21U;
			data->sc.len = 2U;
			data->sc.typematic = true;
		} else {
			data->sc.code[0] = 0xE0U;
			data->sc.code[1] = 0xF0U;
			data->sc.code[2] = 0x21U;
			data->sc.len = 3U;
		}
		break;

	/* Multimedia scan code set 2: Volume up */
	case KM_F5_KEY:
		data->type = FN_SCAN_CODE;
		data->sc.typematic = false;
		if (pressed) {
			data->sc.code[0] = 0xE0U;
			data->sc.code[1] = 0x32U;
			data->sc.len = 2U;
			data->sc.typematic = true;
		} else {
			data->sc.code[0] = 0xE0U;
			data->sc.code[1] = 0xF0U;
			data->sc.code[2] = 0x32U;
			data->sc.len = 3U;
		}
		break;

	/*  Multimedia scan code set 2: Mute */
	case KM_F6_KEY:
		data->type = SCI_CODE;
		if (pressed) {
			data->sci_code = ACPI_SCI_MIC_MUTE;
		} else {
			data->sci_code = 0U;
		}
		break;

	/* Scan code set 2: keyboard backlight */
	case KM_F7_KEY:
		data->type = SCI_CODE;
		if (pressed) {
			data->sci_code = ACPI_SCI_KEYPAD_BACKLIGHT;
		} else {
			data->sci_code = 0U;
		}
		break;

	/* Scan code set 2: display switch */
	case KM_F8_KEY:
		data->type = SCI_CODE;
		if (pressed) {
			data->sci_code = ACPI_SCI_DISPLAY_SWITCH;
		} else {
			data->sci_code = 0U;
		}
		break;

	/* */
	case KM_F9_KEY:
		break;

	/* Scan code set 2: APP */
	case KM_F10_KEY:
		data->type = FN_SCAN_CODE;
		data->sc.typematic = false;
		if (pressed) {
			data->sc.code[0] = 0xE0U;
			data->sc.code[1] = 0x2FU;
			data->sc.len = 2U;
		} else {
			data->sc.code[0] = 0xE0U;
			data->sc.code[1] = 0xF0U;
			data->sc.code[2] = 0x2FU;
			data->sc.len = 3U;
		}
		break;

	/* Scan code set 2: Print screen */
	case KM_F11_KEY:
		data->type = FN_SCAN_CODE;
		data->sc.typematic = false;
		if (pressed) {
			data->sc.code[0] = 0xE0U;
			data->sc.code[1] = 0x12U;
			data->sc.code[2] = 0xE0U;
			data->sc.code[3] = 0x7CU;
			data->sc.len = 4U;
		} else {
			data->sc.code[0] = 0xE0U;
			data->sc.code[1] = 0xF0U;
			data->sc.code[2] = 0x7CU;
			data->sc.code[3] = 0xE0U;
			data->sc.code[4] = 0xF0U;
			data->sc.code[5] = 0x12U;
			data->sc.len = 6U;
		}
		break;

	/* Scan code set 2: Insert key */
	case KM_F12_KEY:
		data->type = FN_SCAN_CODE;
		data->sc.typematic = false;
		if (pressed) {
			data->sc.code[0] = 0xE0U;
			data->sc.code[1] = 0x70U;
			data->sc.len = 2U;
		} else {
			data->sc.code[0] = 0xE0U;
			data->sc.code[1] = 0xF0U;
			data->sc.code[2] = 0x70U;
			data->sc.len = 3U;
		}
		break;

		/*  scan code set 2: Page Up */
	case KM_LFT_ARROW_KEY:
		data->type = FN_SCAN_CODE;
		if (pressed) {
			data->sc.code[0] = 0xE0U;
			data->sc.code[1] = 0x7DU;
			data->sc.len = 2U;
			data->sc.typematic=true;
		} else {
			data->sc.code[0] = 0xE0U;
			data->sc.code[1] = 0xF0U;
			data->sc.code[2] = 0x7DU;
			data->sc.len = 3U;
			data->sc.typematic=false;
		}
		break;

	/*  scan code set 2: Page Down */
	case KM_RGT_ARROW_KEY:
		data->type = FN_SCAN_CODE;
		if (pressed) {
			data->sc.code[0] = 0xE0U;
			data->sc.code[1] = 0x7AU;
			data->sc.len = 2U;
			data->sc.typematic=true;
		} else {
			data->sc.code[0] = 0xE0U;
			data->sc.code[1] = 0xF0U;
			data->sc.code[2] = 0x7AU;
			data->sc.len = 3U;
			data->sc.typematic=false;
		}
		break;

	default:
		return -EINVAL;
	}

	if (pressed) {
		amdcrb_fn_key_led_set(key_num);
	}

	return 0;
}


#else
int amdcrb_get_fn_key(uint8_t key_num, struct fn_data *data,
		     bool pressed)
{
	switch (key_num) {
		/* Scan code set 2: Insert key */
	case KM_ESC_KEY:
		data->type = FN_SCAN_CODE;
		data->sc.typematic = false;
		if (pressed) {
			data->sc.code[0] = 0xE0U;
			data->sc.code[1] = 0x3FU;
			data->sc.len = 2U;
		}
		else {
			data->sc.code[0] = 0xE0U;
			data->sc.code[1] = 0xF0U;
			data->sc.code[2] = 0x3FU;
			data->sc.len = 3U;
		}
		break;
	/* SCI: WWAN RADIO switch */
	case KM_F1_KEY:
		data->type = SCI_CODE;
		if (pressed) {
			data->sci_code = ACPI_SCI_WWAN_RADIO_SW_PRESSED;
		} else {
			data->sci_code = 0U;
		}
		break;
	/* SCI: screen switch */
	case KM_F2_KEY:
		data->type = SCI_CODE;
		if (pressed) {
			data->sci_code = ACPI_SCI_DISPLAY_SWITCH;
		} else {
			data->sci_code = 0U;
		}
		break;
	/* Multimedia scan code set 2: Mute */
	case KM_F3_KEY:
		data->type = FN_SCAN_CODE;
		data->sc.typematic = false;
		if (pressed) {
			data->sc.code[0] = 0xE0U;
			data->sc.code[1] = 0x23U;
			data->sc.len = 2U;
		} else {
			data->sc.code[0] = 0xE0U;
			data->sc.code[1] = 0xF0U;
			data->sc.code[2] = 0x23U;
			data->sc.len = 3U;
		}
		break;
	case KM_F4_KEY:
		/* Do nothing. As long as the length is 0 we don't care
		 * to indicate whether the type is a scan code or an sci
		 */
		data->type = FN_SCAN_CODE;
		data->sc.typematic = false;
		data->sc.code[0] = SC_UNMAPPED;
		data->sc.len = 1U;
		break;
	/* Scan code set 2: Insert key */
	case KM_F5_KEY:
		/* Do nothing. As long as the length is 0 we don't care
		 * to indicate whether the type is a scan code or an sci
		 */
		data->type = FN_SCAN_CODE;
		data->sc.typematic = false;
		data->sc.code[0] = SC_UNMAPPED;
		data->sc.len = 1U;
		break;
	/* Scan code set 2: Insert key */
	case KM_F6_KEY:
		data->type = FN_SCAN_CODE;
		data->sc.typematic = false;
		if (pressed) {
			data->sc.code[0] = 0xE0U;
			data->sc.code[1] = 0x70U;
			data->sc.len = 2U;
		} else {
			data->sc.code[0] = 0xE0U;
			data->sc.code[1] = 0xF0U;
			data->sc.code[2] = 0x70U;
			data->sc.len = 3U;
		}
		break;
	case KM_F7_KEY:
	/* Scan code set 2: Print screen */
		data->type = FN_SCAN_CODE;
		data->sc.typematic = false;
		if (pressed) {
			data->sc.code[0] = 0xE0U;
			data->sc.code[1] = 0x12U;
			data->sc.code[2] = 0xE0U;
			data->sc.code[3] = 0x7CU;
			data->sc.len = 4U;
		} else {
			data->sc.code[0] = 0xE0U;
			data->sc.code[1] = 0xF0U;
			data->sc.code[2] = 0x7CU;
			data->sc.code[3] = 0xE0U;
			data->sc.code[4] = 0xF0U;
			data->sc.code[5] = 0x12U;
			data->sc.len = 6U;
		}
		break;
	/* Scan code set 2: F11 */
	case KM_F9_KEY:
		data->type = FN_SCAN_CODE;
		data->sc.typematic = false;
		if (pressed) {
			data->sc.code[0] = 0x78U;
			data->sc.len = 1U;
		} else {
			data->sc.code[0] = 0xF0U;
			data->sc.code[1] = 0x78U;
			data->sc.len = 2U;
		}
		break;
	/* Scan code set 2: F12 */
	case KM_F10_KEY:
		data->type = FN_SCAN_CODE;
		data->sc.typematic = false;
		if (pressed) {
			data->sc.code[0] = 0x07U;
			data->sc.len = 1U;
		} else {
			data->sc.code[0] = 0xF0U;
			data->sc.code[1] = 0x07U;
			data->sc.len = 2U;
		}
		break;
	/* Multimedia scan code set 2: Volume down */
	case KM_LFT_ARROW_KEY:
		data->type = FN_SCAN_CODE;
		if (pressed) {
			data->sc.code[0] = 0xE0U;
			data->sc.code[1] = 0x21U;
			data->sc.len = 2U;
			data->sc.typematic=true;
		} else {
			data->sc.code[0] = 0xE0U;
			data->sc.code[1] = 0xF0U;
			data->sc.code[2] = 0x21U;
			data->sc.len = 3U;
			data->sc.typematic=false;
		}
		break;
	/* Multimedia scan code set 2: Volume up */
	case KM_RGT_ARROW_KEY:
		data->type = FN_SCAN_CODE;
		if (pressed) {
			data->sc.code[0] = 0xE0U;
			data->sc.code[1] = 0x32U;
			data->sc.len = 2U;
			data->sc.typematic=true;
		} else {
			data->sc.code[0] = 0xE0U;
			data->sc.code[1] = 0xF0U;
			data->sc.code[2] = 0x32U;
			data->sc.len = 3U;
			data->sc.typematic=false;
		}
		break;
	/* SCI: Brightness up */
	case KM_UP_ARROW_KEY:
		data->type = SCI_CODE;
		if (pressed) {
			data->sci_code = ACPI_SCI_BRIGHTNESS_UP;
		} else {
			data->sci_code = 0U;
		}
		break;
	/* SCI: Brightness down */
	case KM_DN_ARROW_KEY:
		data->type = SCI_CODE;
		if (pressed) {
			data->sci_code = ACPI_SCI_BRIGHTNESS_DOWN;
		} else {
			data->sci_code  = 0U;
		}
		break;
	/* Scan code set 2: APP */
	case KM_RCNTRL_KEY:
		data->type = FN_SCAN_CODE;
		data->sc.typematic = false;
		if (pressed) {
			data->sc.code[0] = 0xE0U;
			data->sc.code[1] = 0x2FU;
			data->sc.len = 2U;
		} else {
			data->sc.code[0] = 0xE0U;
			data->sc.code[1] = 0xF0U;
			data->sc.code[2] = 0x2FU;
			data->sc.len = 3U;
		}
		break;
	default:
		return -EINVAL;
	}

	if (pressed) {
		amdcrb_fn_key_led_set(key_num);
	}

	return 0;
}
#endif

struct km_api *amdcrb_init(void)
{
	return &amdcrb_keyboard_api;
}
