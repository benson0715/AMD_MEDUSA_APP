/*****************************************************************************
 * Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
 ******************************************************************************
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/shell/shell.h>
#include <stdlib.h>
#include "f_nv_options.h"

struct args_index {
	uint8_t name;
	uint8_t data;
};

static const struct args_index args_indx = {
	.name = 1,
	.data = 2
};

/**
 * @brief Initialize NV options by loading from storage
 */
static int cmd_init(const struct shell *shell, size_t argc, char **argv)
{
	f_nv_options_load();
	return 0;
}

/**
 * @brief Save NV options to storage
 */
static int cmd_save(const struct shell *shell, size_t argc, char **argv)
{
	f_nv_options_save();
	return 0;
}

/**
 * @brief Read and display all NV options
 */
static int cmd_read(const struct shell *shell, size_t argc, char **argv)
{
	uint32_t optionVal;
	GET_NV_OPTIONS ( pwr_keepWlanPwrInS3    ,  optionVal);
	shell_print(shell, "pwr_keepWlanPwrInS3     = %02d",optionVal);

	GET_NV_OPTIONS( pwr_keepWlanPwrInS4     ,  optionVal);
	shell_print(shell, "pwr_keepWlanPwrInS4     = %02d",optionVal);
	GET_NV_OPTIONS( pwr_SSD0D3Cold          ,  optionVal);
	shell_print(shell, "pwr_SSD0D3Cold          = %02d",optionVal);
	GET_NV_OPTIONS( pwr_keepWlanPwrInS4     ,  optionVal);
	shell_print(shell, "pwr_keepWlanPwrInS4     = %02d",optionVal);
	GET_NV_OPTIONS( pwr_WWAND3Cold          ,  optionVal);
	shell_print(shell, "pwr_WWAND3Cold          = %02d",optionVal);
	GET_NV_OPTIONS( pwr_WLAND3Cold          ,  optionVal);
	shell_print(shell, "pwr_WLAND3Cold          = %02d",optionVal);
	GET_NV_OPTIONS( chg_UseNvdcMode         ,  optionVal);
	shell_print(shell, "chg_UseNvdcMode         = %02d",optionVal);
	GET_NV_OPTIONS( chg_DcOnlyProchot       ,  optionVal);
	shell_print(shell, "chg_DcOnlyProchot       = %02d",optionVal);
	GET_NV_OPTIONS( chg_AcOnlyProchot       ,  optionVal);
	shell_print(shell, "chg_AcOnlyProchot       = %02d",optionVal);
	GET_NV_OPTIONS( chg_AcDcSwitch          ,  optionVal);
	shell_print(shell, "chg_AcDcSwitch          = %02d",optionVal);
	GET_NV_OPTIONS( chg_AcTime              ,  optionVal);
	shell_print(shell, "chg_AcTime              = %02d",optionVal);
	GET_NV_OPTIONS( chg_DcTime              ,  optionVal);
	shell_print(shell, "chg_DcTime              = %02d",optionVal);
	GET_NV_OPTIONS( chg_FakeDcLevel         ,  optionVal);
	shell_print(shell, "chg_FakeDcLevel         = %02d",optionVal);
	GET_NV_OPTIONS( chg_DcProchotLevel      ,  optionVal);
	shell_print(shell, "chg_DcProchotLevel      = %02d",optionVal);
	GET_NV_OPTIONS( chg_ProchotAcDebounce   ,  optionVal);
	shell_print(shell, "chg_ProchotAcDebounce   = %02d",optionVal);
	GET_NV_OPTIONS( chg_ProchotDuration     ,  optionVal);
	shell_print(shell, "chg_ProchotDuration     = %02d",optionVal);
	GET_NV_OPTIONS( chg_ProchotDcDebounce   ,  optionVal);
	shell_print(shell, "chg_ProchotDcDebounce   = %02d",optionVal);
	                                                        
	GET_NV_OPTIONS( thm_p3tLimitEn_DC       ,  optionVal);
	shell_print(shell, "thm_p3tLimitEn_DC       = %02d",optionVal);
	GET_NV_OPTIONS( thm_p3tLimitEn_AC       ,  optionVal);
	shell_print(shell, "thm_p3tLimitEn_AC       = %02d",optionVal);
	GET_NV_OPTIONS( thm_uploadOnboardTemp   ,  optionVal);
	shell_print(shell, "thm_uploadOnboardTemp   = %02d",optionVal);
	GET_NV_OPTIONS( thm_uploadEvalCardTemp  ,  optionVal);
	shell_print(shell, "thm_uploadEvalCardTemp  = %02d",optionVal);
	                                                        
	GET_NV_OPTIONS( f_wirelessManagability  ,  optionVal);
	shell_print(shell, "f_wirelessManagability  = %02d",optionVal);
	GET_NV_OPTIONS( f_s0i3KbcWake           ,  optionVal);
	shell_print(shell, "f_s0i3KbcWake           = %02d",optionVal);
	GET_NV_OPTIONS( f_TurnOnPostCode        ,  optionVal);
	shell_print(shell, "f_TurnOnPostCode        = %02d",optionVal);
	                                                        
	GET_NV_OPTIONS( spi_ba_Type             ,  optionVal);
	shell_print(shell, "spi_ba_Type             = %02d",optionVal);
	                                                        
	GET_NV_OPTIONS( espi_peripheralSupport  ,  optionVal);
	shell_print(shell, "espi_peripheralSupport  = %02d",optionVal);
	GET_NV_OPTIONS( espi_virtialWireSupport ,  optionVal);
	shell_print(shell, "espi_virtialWireSupport = %02d",optionVal);
	GET_NV_OPTIONS( espi_OobSupport         ,  optionVal);
	shell_print(shell, "espi_OobSupport         = %02d",optionVal);
	GET_NV_OPTIONS( espi_FlashAccSupport    ,  optionVal);
	shell_print(shell, "espi_FlashAccSupport    = %02d",optionVal);
	GET_NV_OPTIONS( espi_modeSupport        ,  optionVal);
	shell_print(shell, "espi_modeSupport        = %02d",optionVal);
	GET_NV_OPTIONS( espi_maxSpeed           ,  optionVal);
	shell_print(shell, "espi_maxSpeed           = %02d",optionVal);
	GET_NV_OPTIONS( espi_mmio_ecram_bar     ,  optionVal);
	shell_print(shell, "espi_mmio_ecram_bar     = %08x",optionVal);
	GET_NV_OPTIONS( espi_mmio_test_bar      ,  optionVal);
	shell_print(shell, "espi_mmio_test_bar      = %08x",optionVal);
	GET_NV_OPTIONS( usbc_status             ,  optionVal);
	shell_print(shell, "usbc_status             = %02x",optionVal);
	
	return 0;
}

/**
 * @brief Write NV option by index
 */
static int cmd_write(const struct shell *shell, size_t argc, char **argv)
{
	uint32_t optionVal = strtoul(argv[args_indx.data], NULL, 0);
	uint32_t select = strtoul(argv[args_indx.name], NULL, 0);
	switch (select) {
		  case 0:
		  	SET_NV_OPTIONS(pwr_keepWlanPwrInS3,optionVal);
			break;
		  case 1:
		  	SET_NV_OPTIONS(pwr_keepWlanPwrInS4,optionVal);
			break;
		  case 2:
		  	SET_NV_OPTIONS(pwr_SSD0D3Cold,optionVal);
			break;
		  case 3:
		  	SET_NV_OPTIONS(pwr_WWAND3Cold,optionVal);
			break;
		  case 4:
		  	SET_NV_OPTIONS(pwr_WLAND3Cold,optionVal);
			break;
		}
	
	f_nv_options_save();
	return 0;
}

SHELL_STATIC_SUBCMD_SET_CREATE(nvoption_cmds,
	SHELL_CMD(init, NULL, "Load NV options from storage", cmd_init),
	SHELL_CMD(save, NULL, "Save NV options to storage", cmd_save),
	SHELL_CMD(read, NULL, "Read all NV options", cmd_read),
	SHELL_CMD_ARG(write, NULL, "<select> <value>", cmd_write, 3, 0),
	SHELL_SUBCMD_SET_END
);

SHELL_CMD_REGISTER(nvoption, &nvoption_cmds, "NVOPTION shell commands", NULL);