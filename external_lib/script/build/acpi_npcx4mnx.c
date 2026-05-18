/*****************************************************************************
 * Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
 ******************************************************************************
 */

#include <errno.h>
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <soc.h>
#include <zephyr/logging/log.h>
#include "acpi.h"

LOG_MODULE_DECLARE(smchost, CONFIG_SMCHOST_LOG_LEVEL);

#define OS2EC_LSBBYTE_MASK    0xFF
#define ACPI_EC_STRUCT_SIZE	0x400

struct acpi_ec_int {
	uint32_t ibf_ecia_info;
	uint32_t obe_ecia_info;
};

static uintptr_t regbase[] = {
	DT_REG_ADDR_BY_NAME(DT_NODELABEL(host_sub), pm_acpi),
	DT_REG_ADDR_BY_NAME(DT_NODELABEL(host_sub), pm_hcmd),
	// DT_REG_ADDR(DT_NODELABEL(acpi_ec2)),
	// DT_REG_ADDR(DT_NODELABEL(acpi_ec3)),
};

/**
 * @brief Enable or disable ACPI IBF (Input Buffer Full) interrupt
 *
 * @param num ACPI EC interface number
 * @param val true to enable, false to disable
 */
void acpi_ibf_intr(enum acpi_ec_interface num, bool val)
{
	if (val) {
		switch (num) {
		case 0:
			irq_enable(DT_IRQ_BY_NAME(DT_NODELABEL(host_sub), pmch_ibf, irq));
			break;
		case 1:
			break;
		case 2:
			break;
		case 3:
			break;
		default:
			return;
		}
	} else {
		switch (num) {
		case 0:
			irq_disable(DT_IRQ_BY_NAME(DT_NODELABEL(host_sub), pmch_ibf, irq));
			break;
		case 1:
			irq_disable(DT_IRQ_BY_NAME(DT_NODELABEL(host_sub), pmch_ibf, irq));
			break;
		case 2:
			break;
		case 3:
			break;
		default:
			return;
		}
	}
}

/**
 * @brief Enable or disable ACPI OBE (Output Buffer Empty) interrupt
 *
 * @param num ACPI EC interface number
 * @param val true to enable, false to disable
 */
void acpi_obe_intr(enum acpi_ec_interface num, bool val)
{
	if (val) {
		switch (num) {
		case 0:
		case 1:
			irq_enable(DT_IRQ_BY_NAME(DT_NODELABEL(host_sub), pmch_obe, irq));
			break;
		case 2:
			break;
		case 3:
			break;
		default:
			return;
		}
	} else {
		switch (num) {
		case 0:
		case 1:
			irq_disable(DT_IRQ_BY_NAME(DT_NODELABEL(host_sub), pmch_obe, irq));
			break;
		case 2:
			break;
		case 3:
			break;
		default:
			return;
		}
	}
}

/**
 * @brief Get ACPI status flag
 *
 * @param num ACPI EC interface number
 * @param type Flag type to check
 * @return true if flag is set, false otherwise
 */
bool acpi_get_flag(enum acpi_ec_interface num, uint8_t type)
{
	struct pmch_reg *regs =  (struct pmch_reg *)regbase[num];
	return regs->HIPMST & type;
}

/**
 * @brief Set or clear ACPI status flag
 *
 * @param num ACPI EC interface number
 * @param type Flag type to modify
 * @param val true to set, false to clear
 */
void acpi_set_flag(enum acpi_ec_interface num, uint8_t type, bool val)
{
	struct pmch_reg *regs =  (struct pmch_reg *)regbase[num];

	if (val) {
		regs->HIPMST |= type;
	} else {
		regs->HIPMST &= ~type;
	}
}

/**
 * @brief Read from ACPI input data register
 *
 * @param num ACPI EC interface number
 * @return Data byte read from input register
 */
uint8_t acpi_read_idr(enum acpi_ec_interface num)
{
	struct pmch_reg *regs =  (struct pmch_reg *)regbase[num];
	regs->HIPMST |= BIT(NPCX_HIPMST_F0);

	return (regs->HIPMDI & OS2EC_LSBBYTE_MASK);
}

/**
 * @brief Write to ACPI output data register
 *
 * @param num ACPI EC interface number
 * @param byte Data byte to write
 */
void acpi_write_odr(enum acpi_ec_interface num, uint8_t byte)
{
	struct pmch_reg *regs =  (struct pmch_reg *)regbase[num];

	regs->HIPMDO = byte;
}

/**
 * @brief Read ACPI status register
 *
 * @param num ACPI EC interface number
 * @return Status register value
 */
uint8_t acpi_read_str(enum acpi_ec_interface num)
{
	struct pmch_reg *regs =  (struct pmch_reg *)regbase[num];

	return regs->HIPMST;
}

/**
 * @brief Write to ACPI status register
 *
 * @param num ACPI EC interface number
 * @param byte Value to write to status register
 */
void acpi_write_str(enum acpi_ec_interface num, uint8_t byte)
{
	struct pmch_reg *regs =  (struct pmch_reg *)regbase[num];

	regs->HIPMST = byte;
}

/**
 * @brief Send a byte to host via ACPI interface
 *
 * @param num ACPI EC interface number
 * @param data Data byte to send
 * @return 0 on success, -1 on timeout
 */
int acpi_send_byte(enum acpi_ec_interface num, uint8_t data)
{
	for (uint16_t i = 0; i < HOST_TIMEOUT; i++) {

		if (acpi_get_flag(num, ACPI_FLAG_OBF) == 1) {
			continue;
		}

		acpi_write_odr(num, data);
		return 0;
	}
	return -1;
}