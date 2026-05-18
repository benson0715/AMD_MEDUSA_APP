/*****************************************************************************
 * Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
 ******************************************************************************
 */

#ifndef __APP_SFH_H__
#define __APP_SFH_H__

#include <zephyr/kernel.h>
#include <zephyr/drivers/i2c.h>

#define SFH_DEV_SPEC_VER    "1.00" // Sensor Hub to Embedded Controller Connection Specification, Revision: 1.00
#define SFH_REG_BITS        (8u)

#define SFH_REG_BANK_SIZE   (256)
#define SFH_REG_ACCESS_RO   (1u)
#define SFH_REG_ACCESS_WO   (2u)
#define SFH_REG_ACCESS_RW   (3u)
#define SFH_REG_ACCESS_RSVD (0u)

#define SFH_CACHE_SIZE      (SFH_REG_BANK_SIZE)

#define SFH_REG_VAL_ERROR    (0xFEu) // 0xFE can be used for invalid/error output
#define SFH_REG_VAL_NOT_USED (0x00u) // when not used
#define SFH_REG_INVALID      (0xFFFFu)

#define SFH_ERR              (0) // npcx driver doesn't support negative return.

#define OPCODE_WRITE_REG        BIT(0)
#define OPCODE_WRITE_DATA       BIT(1)
#define OPCODE_STOP             BIT(2)
#define OPCODE_READ_DATA_REQ    BIT(3)
#define OPCODE_READ_DATA        BIT(4)

#define OPCODE_WRITE_DATA_DONE_BITS   (OPCODE_WRITE_REG | OPCODE_WRITE_DATA | OPCODE_STOP)
#define OPCODE_READ_DATA_REQ_BITS     (OPCODE_WRITE_REG | OPCODE_READ_DATA_REQ)
#define OPCODE_READ_DATA_BITS         (OPCODE_WRITE_REG | OPCODE_READ_DATA_REQ | OPCODE_READ_DATA)
#define OPCODE_READ_DATA_DONE_BITS    (OPCODE_WRITE_REG | OPCODE_READ_DATA_REQ | OPCODE_STOP)
#define OPCODE_READ_N_DATA_DONE_BITS  (OPCODE_WRITE_REG | OPCODE_READ_DATA_REQ | OPCODE_READ_DATA | OPCODE_STOP)

typedef int (*sfh_register_cb_t)(volatile void* reg, uint16_t index, uint8_t *u8_data, bool is_read);

#define SFH_REG_SETTING_INIT(register, rw, fun) \
		{.address = offsetof(struct sfh_reg, register), .reg_ptr = &g_reg_init.register, .size = sizeof(g_reg_init.register), .access = rw, .fn = fun}

#define SFH_REG_SETTING_INIT_N(register, num, rw, fun) \
		{.address = offsetof(struct sfh_reg, register), .reg_ptr = &g_reg_init.register[0], .size = sizeof(g_reg_init.register[num]), .access = rw, .fn = fun}

#define SFH_REG_SETTING_END() {.address = SFH_REG_INVALID, .size = 0, .access = 0, .fn = NULL}

struct sfh_reg_setting {
	uint16_t address;
	uint16_t size;
	volatile void* reg_ptr;
	uint8_t access : 2;
	uint8_t rsvd : 6;
	sfh_register_cb_t fn;
};

struct sfh_reg {
	volatile uint8_t notification;
	volatile uint8_t platform_operation_mode;
	volatile uint8_t keyboard_enable;
	volatile uint8_t on_desk_lap;
	volatile uint8_t ec2mp2_cmd_reg;
	volatile uint8_t lid_switch;
	volatile uint8_t hpd_distance;
	volatile uint8_t keyboard_light_control_hpd;
	volatile uint8_t keyboard_light_control_als;
	volatile uint8_t pickup_in_transport;
	volatile uint8_t rsvd_0a_0f[6];

	volatile uint16_t als_value;
	volatile uint16_t red_color_value;
	volatile uint16_t green_color_value;
	volatile uint16_t blue_color_value;
	volatile uint16_t ir_value;
	volatile uint8_t rsvd_1a_1f[6];

	volatile uint8_t thermal_sensor_1;
	volatile uint8_t rsvd_21;
	volatile uint8_t thermal_sensor_3;
	volatile uint8_t rsvd_23_2f[13];

	volatile uint8_t power_profile_setting;
	volatile uint8_t fan_policy_setting;
	volatile uint8_t ad_dc;
	volatile uint8_t rsvd_33_3f[13];

	volatile uint8_t custom_sensor_flags[16];

	volatile uint8_t rsvd_50_9F[80];

	volatile uint16_t gpio_expander;
	volatile uint8_t rsvd_a2_af[14];

	volatile uint8_t rsvd_b0_ff[80];
} __packed;
BUILD_ASSERT(sizeof(struct sfh_reg) == 256, "Invalid sfh_reg structure!");

struct sfh_data {
	uint16_t reg;
	uint8_t cache[SFH_CACHE_SIZE];
	uint8_t write_index;
	uint8_t read_index;
	uint16_t write_cnt;
	uint8_t opcode;
};

struct i2c_sfh_context {
	struct i2c_target_config config;
	struct sfh_reg_setting* setting;
	struct sfh_reg *reg;
	struct sfh_data data;
	const char *version;
};

/*****************************************************************************
 * Function name:   app_sfh_init
 * Description:     Initialize SFH (Smart Fan Hub) I2C slave interface
 * @param           None
 * @return          None
 * @note            Registers SFH as I2C slave device on configured port
 *****************************************************************************/
void app_sfh_init(void);

#endif // end of __APP_SFH_H__

