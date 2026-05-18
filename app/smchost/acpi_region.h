/**
* Copyright (c) 2019 Intel Corporation
* Modifications copyright (c) 2023 Advanced Micro Devices, Inc.
*/

#ifndef __ACPI_REGION_H__
#define __ACPI_REGION_H__

#include <soc.h>

/** Maximum value allowed for ACPI accesses */
#define ACPI_MAX_DATA        0xFF
/** Max Number of ACPI Bytes */
#define ACPI_MAX_INDEX       255ul

struct acpi_state_flags {
	uint8_t acpi_mode:1;
	uint8_t smi_notify:1;
	uint8_t smi_enabled:1;
	uint8_t sci_enabled:1;
};
struct acpi_status_flags {
	uint8_t ac_prsnt:1;
	uint8_t usb_prsnt:1;
	uint8_t lpc_docked:1;
	uint8_t fan_on:1;
	uint8_t ri_wake:1;
	uint8_t sleep_s3:1;
	uint8_t lid_open:1;
	uint8_t pme_active:1;
};
struct acpi_status2_flags {
	uint8_t pcie_docked:1;
	uint8_t pcie_pwr_down:1;
	uint8_t exp_card_prsnt:1;
	uint8_t pwr_btn:1;
	uint8_t vb_sw_closed:1;
	uint8_t dimm_ts:1;
	uint8_t bt_attach:1;
	uint8_t bt_pwr_off:1;
};
struct acpi_bat_flags {
	uint8_t dischrg:1;
	uint8_t chrg:1;
	uint8_t crit:1;
	uint8_t prsnt:1;
	uint8_t unused4:1;
	uint8_t unused5:1;
	uint8_t unused6:1;
	uint8_t unused7:1;
};
struct acpi_power_source {
	uint8_t psrc:4;
	uint8_t pdscsn:4;
	uint8_t pdscsn_rcvd;
};
struct acpi_concept_flags {
	uint8_t acpi_cs_dbg_led_en:1;
	uint8_t acpi_winb_pwr_btn_en:1;
	uint8_t unused2:1;
	uint8_t pg3_exit_after_counter_exp:1;
	uint8_t pg3_entered_successfully:1;
	uint8_t unused5:1;
	uint8_t unused6:1;
	uint8_t unused7:1;
};

struct acpi_hid_btn_sci {
	uint8_t pwr_btn_en_dis:1;
	uint8_t win_btn_en_dis:1;
	uint8_t vol_up_en_dis:1;
	uint8_t vol_down_en_dis:1;
	uint8_t rot_lock_en_dis:1;
	uint8_t rsvd0:3;
};

#ifndef ACPIPLANES_SUPPORTED
struct acpi_tbl {
	/* Start of ACPI space. */
	/* [0x00-0x8F] Reserved */
	uint8_t ACPI_SPACE_RSV_00[144];
	/* [0x90-0x92] ACPI PMIC Tunnel Base */
	uint8_t ACPI_PMIC_TUNNEL_BASE[3];
	/* [0x93-0x9C] Board ID Information */
	uint8_t ACPI_BOARD_INFO[10];
	/* [0x9D-0x9F] Reserved */
	uint8_t ACPI_SPACE_RSV_9D[3];
	/* [0xA0-0xA4] EC Native GPIOs */
	uint8_t ACPI_EC_GPIO[5];
    /* [0xA5-0xA6] Reserved */
	uint8_t ACPI_SPACE_RSV_A5[2];
    /* [0xA7-0xB2] IO Expander GPIOs */
	uint8_t ACPI_IOEXP_GPIO[12];
	/* [0xB3-0xB4] Reserved */
	uint8_t ACPI_SPACE_RSV_AD[2];
    /* [0xB5-0xB7] ACPI Switch Base */
	uint8_t ACPI_SWITCH_BASE[3];
    /* [0xB8-0xBC] EC Firmware Version */
	uint8_t ACPI_EC_VER[5];
	/* [0xBD-0xC0] EC Build Date */
	uint8_t ACPI_EC_DATE[4];	
	/* [0xC1-0xC6] EC Build Time */
	uint8_t ACPI_EC_Time[6];	
	/* [0xC7-0xC8] AC/DC time for AC/DC Switch */
	uint8_t ACPI_ACDC_TIME[2];
    /* [0xC9-0xCA] Reserved */
	uint8_t ACPI_SPACE_RSV_CA[2];
	/* [0xCB] System Event Flags */
	uint8_t ACPI_SYS_EVENT[2];
	/* [0xCD-0xCE] Reserved */
	uint8_t ACPI_SPACE_RSV_CD[2];
	/* [0xCF] Miscellaneous Status and Control */
	uint8_t ACPI_MISC_CTL;
    /* [0xD0-0xEF] Reserved for Smart Battery Usage */
	uint8_t ACPI_SPACE_RSV_D0[32];
    /* [0xF0-0xF1] Reserved for BIOS */
	uint8_t ACPI_SPACE_RSV_F0[2];
    /* [0xF2-0xFB] Thermal Zone */
	uint8_t ACPI_THERMAL[10];
	/* [0xFC-0xFD] Reserved */
	uint8_t ACPI_SPACE_RSV_FC[2];
	/* [0xFE] USB Ready */
	uint8_t ACPI_USB_RDY;
	/* [0xFF] Reserved */
	uint8_t ACPI_SPACE_FF;
} __attribute__((__packed__));
extern struct acpi_tbl g_acpi_tbl;
#endif /* ACPIPLANES_SUPPORTED */

extern struct acpi_state_flags g_acpi_state_flags;

#endif /* __ACPI_REGION_H__ */
