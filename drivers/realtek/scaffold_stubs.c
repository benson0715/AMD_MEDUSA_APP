/*
 * Copyright (c) 2026 Realtek Semiconductor Corporation, SIBG-SD7
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Weak no-op stubs for AMD app symbols whose owning translation unit
 * is gated off during RTS5918 first-light bring-up. Once the matching
 * board source / Kconfig flag is re-enabled (see prj.conf
 * forward-port-scaffold block) the strong definition in the upstream
 * .c file overrides each weak stub here.
 *
 * Grouped by owning module so it is clear which `=y` Kconfig flip
 * removes which stubs.
 *
 * TODO(realtek-schematic): every entry here represents a hardware
 * touch point that needs a real Realtek-side implementation before
 * the corresponding feature can run on Plum + RTS5918.
 */

#include <zephyr/kernel.h>
#include <stdint.h>
#include <stdbool.h>

/* --- app/acpi/acpi.c (gated by other config) ----------------------------- */
__weak void ACPI_ClearQueuedEvts(void) {}
__weak void ACPI_DisableHostNotify(void) {}
__weak void ACPI_EnableHostNotify(void) {}
__weak bool ACPI_isHostNotifyDisable(void) { return false; }
__weak int  acpi_get_flag(int acpi_num, int flag) { (void)acpi_num; (void)flag; return 0; }
__weak void acpi_set_flag(int acpi_num, int flag, int val) { (void)acpi_num; (void)flag; (void)val; }
__weak void acpi_ibf_intr(int acpi_num, int val) { (void)acpi_num; (void)val; }
__weak void acpi_obe_intr(int acpi_num, int val) { (void)acpi_num; (void)val; }
__weak int  acpi_read_idr(int acpi_num) { (void)acpi_num; return 0; }
__weak int  acpi_send_byte(int acpi_num, uint8_t byte) { (void)acpi_num; (void)byte; return 0; }
__weak int  acpi_write_odr(int acpi_num, uint8_t byte) { (void)acpi_num; (void)byte; return 0; }

/* --- app/peripheral_management/app_btn.c (CONFIG_PERIPHERAL_MANAGEMENT) -- */
__weak void app_btn_assertCallback(void) {}
__weak void app_btn_deassertCallback(void) {}
__weak void app_btn_assertFchPwrBtn(void) {}
__weak void app_btn_deassertFchPwrBtn(void) {}
__weak void app_btn_ecReset(void) {}
__weak void app_btn_monitor(void) {}
__weak void app_btn_registerHandler(void *h) { (void)h; }
__weak void app_btn_triggerPwrBtn(uint32_t ms) { (void)ms; }

/* --- app/ec_debug/espi_validation.c (CONFIG_ECDBGI_*) -------------------- */
__weak void app_espi_fa_testTrigger(void) {}
__weak void app_espi_oob_reset(void) {}

/* --- boards/realtek/.../source/board_init.c (CONFIG_PLATFORM_INIT) ------- */
__weak void board_init_apuSleepEnterHandler(void) {}
__weak void board_init_apuSleepExitHandler(void) {}

/* --- boards/realtek/.../source/ec_version.c default placeholder ---------- */
__weak int ec_version_checkEcFwVersion(void) { return 0; }

/* --- app-level global flags exported by power_management/wireless code --- */
__weak uint8_t g_slideToShutdownEnabled;
__weak uint8_t g_ui8MondernStandbySupport;

/* --- gpioAutoGen runtime hooks ------------------------------------------- */
__weak void __autoGen_runtimeGpioSwitching(uint32_t pin, int level)
{
	(void)pin;
	(void)level;
}

/* --- extra GPIO wrapper helpers (drivers/realtek/realtek_gpio_impl.c only
 * provides the core dispatch; these two were Nuvoton-specific extensions
 * referenced from a few app paths).
 */
__weak int gpio_interrupt_configure_pin(uint32_t port_pin, uint32_t flags)
{
	(void)port_pin;
	(void)flags;
	return 0;
}

__weak int gpio_set_power(uint32_t port_pin, uint32_t pwr_mode)
{
	(void)port_pin;
	(void)pwr_mode;
	return 0;
}

/* --- drivers/oob_msg_mngr.c (CONFIG_OOB_MSG_MNGR=n) ---------------------- */
__weak void oob_rx_cb_handler(uint32_t opcode, void *pdata)
{
	(void)opcode;
	(void)pdata;
}
