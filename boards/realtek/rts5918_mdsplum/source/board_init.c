/*****************************************************************************
 * Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
 ******************************************************************************
 */

#include "ioexp_hub.h"
#include "u_rrq.h"
#if (CONFIG_USBC_4CC)
#include "app_4cc.h"
#endif
#if (CONFIG_USBC_CCGX)
#include "app_ccgx.h"
#endif
#if (CONFIG_USBC_ITE)
#include "app_ite_pd.h"
#endif
#if (CONFIG_USBC_RTK)
#include "app_realtek_pd.h"
#endif
#include "amd_crb_drivers_pd.h"
#include "amd_crb_drivers_hpi.h"
#if CONFIG_USBC_TIPD_TPS6599X
#include "amd_crb_drivers_tps6599x.h"
#endif
#if CONFIG_USBC_TIPD_TPS6699X
#include "amd_crb_drivers_tps6699x.h"
#endif
#include "app_usbc_task.h"
#include "board_init.h"
#include "app_btn.h"
#include "app_pseq.h"
#if (CONFIG_POWER_SOURCE_MANAGEMENT)
#include "board_pwrSrc.h"
#endif
#include "app_acpi.h"
#include "board_id.h"
#if (CONFIG_BATTERY_MANAGEMENT)
#include "board_smtbty.h"
#endif
#include "f_romSig.h"
#include "app_AcDcSwitch.h"
#include "app_udc.h"
#include "fwSt.h"
#if CONFIG_WIRELESS_MANAGEABILITY
#include "app_manOs.h"
#endif
#include "app_thermal.h"
#include "app_ucsi_tunnel.h"
#include "f_nv_options.h"
#include "espi_hub.h"
#include "periphmgmt.h"
#include "board_config.h"
#include <assert.h>
#include "postcode_led_hub.h"
#include <zephyr/logging/log.h>
#include "i2c_hub.h"
#include "app_usbc_task.h"
#if (CONFIG_SFH)
#include "app_sfh_waie.h"
#include "app_sfh.h"
#endif
#include "periphmgmt.h"
#include "app_spiS0.h"
#include "ec_version.h"
#include "fwSt.h"
#include "dev_mpc.h"
#include "board_dac.h"
#include <zephyr/drivers/bbram.h>
#include "dbgLogFifo.h"
#include "amd_crb_drivers_spiFlash.h"

LOG_MODULE_REGISTER(brdint, CONFIG_BOARD_INIT_LOG_LEVEL);


/* ************************** *
 *          Macro             *
 * ************************** */
#if 0 // TODO_RTK
#define ex_SmrtMux_IO1       ex_EP_EDP_SMUX_PWM_EN
#define ex_SmrtMux_IO2       ex_EP_EDP_SMUX_BLON
#define ex_SmrtMux_IO3       ex_EP_EDP_SMUX_PNL_RSTn
#define ex_SmrtMux_IO4       ex_EP_EDP_SMUX_PWMMUX_SEL
#endif
/* ************************** *
 *      external valuable     *
 * ************************** */
extern uint8_t g_u8Pd0Slot;
extern uint8_t g_u8Pd1Slot;

extern uint8_t g_biosVer[36];
extern uint8_t g_nicMacAddr[29];
extern uint8_t g_cpuId[16];
extern uint8_t g_cpuOpn[32];
#if CONFIG_SFH
extern uint8_t g_WAIE_RITE;
extern uint8_t waie_syc_flag;
extern void app_sfh_init();
#endif

/* ************************** *
 *      static valuable       *
 * ************************** */
static bool _s_forcePdUpdate = 0;
/* pmic DAC I2C tunnel for ACPI DAC control */
static struct k_work pmicTunnel_work;
/* Timer for the APU Rst Trigger */
static struct k_timer g_apuRstTrigger;
/* MPC update info flag */
static bool g_updateinfo = false;
static void board_init_mpc_update_callback(void);
/* ************************** *
 *       global variables     *
 * ************************** */
/* KBC reset trigger flag */
uint8_t g_triggerKbcRst = 0;
/* system ac limit value */
uint16_t g_sysAcLimit1 = F_AC_ADAPTER_130W_MAX_CURRENT;
/* For runtime FW status */
uint8_t g_apuRstS3Flag = 0;

#if 0 // TODO_RTK
struct {
	uint32_t Pin;
	uint8_t  MuxSetting[2];
} _s_smartMuxTab[] = {
		/* PIN,     Case #1, #2 */
		{ex_SmrtMux_IO1, {0, 0} },
		{ex_SmrtMux_IO2, {1, 1} },
		{ex_SmrtMux_IO3, {1, 1} },
		{ex_SmrtMux_IO4, {0, 1} }
};
#endif

/**
 * board_init_apuRstTimerEnable
 *
 * @note
 *  enable the timer after apu reset to delay the correspending handler by xxms
 */
void board_init_apuRstTimerEnable (uint32_t ms)
{
	k_timer_start(&g_apuRstTrigger, K_MSEC(ms), K_NO_WAIT);
}

/**
 * board_pmicTunnelProc
 *
 * @note
 *  pmic tunnel handler to access I2C DAC
 */
static void board_pmicTunnelProc (struct k_work *w)
{
	ACPI_PMIC_TunnelProc();
}

/**
 * board_init_apuRstTrigger
 *
 * @note
 *  APU reset trigger handler
 */
static void board_init_apuRstTrigger(struct k_timer *timer)
{
	if (!SLP_S5_ASSERT && (SLP_S3_ASSERT || g_apuRstS3Flag)) {
		/* S3 or S0i3 */
		LOG_DBG(">>>Skip ApuRstHandler");
	} else {
		/* S4 and S5 */
		board_init_apuResetHandler();

		LOG_DBG(">>>handle ApuRstHandler");
#if CONFIG_WIRELESS_MANAGEABILITY
		/**
		 * app_manOs hook for APU_RST
		 */
		app_manOs_ApuRstHook();
#endif
	}
}

/**
 * board_init_pmicTunnelTrigger
 *
 * @note
 *   trigger pmic i2c tunnel access for DAC.
 */
void board_init_pmicTunnelTrigger(void)
{
	k_work_submit(&pmicTunnel_work);
}

/**
 * board_hpiFwUpdateCallback
 *
 * @note
 *   callback handler for HPI firmware update status indication
 */
void board_hpiFwUpdateCallback (DRV_HPI_FW_UPDATE_INDICATOR st)
{
	switch (st) {
		case DRV_HPI_FW_UPDATE_INIT:
		case DRV_HPI_FW_UPDATE_HAD_SKIPPED:
		case DRV_HPI_FW_UPDATE_ONE_ROW:
			break;
		case DRV_HPI_FW_UPDATE_CHECKSUM_ERROR:
			while(1);
		case DRV_HPI_FW_UPDATE_FAILED:
		case DRV_HPI_FW_UPDATE_SUCCESS:
		default:
			break;
	}
}

#if (CONFIG_PERIPHERAL_LOG_LEVEL)
	extern void _dumpPinSt(uint8_t st, const char * pMsg);
	#define LOG_PWRSEQ_PIN_STATUS(s, x)       _dumpPinSt(s, x);
#else
	#define LOG_PWRSEQ_PIN_STATUS(s, x)       while(0){}
#endif

/**
 * board_readDispSwSt
 *
 * @note
 *   Read if DISP_SW button is press before system power on.
 *   To trigger PD FW force update. Need to force enable 3.3V ALW if pullup is 3.3V ALW. Otherwise don't.
 */
static void board_readDispSwSt(void)
{
#if 0 // TODO_RTK
	_s_forcePdUpdate = false;

	if (!ioexp_get(ex_PD_FORCE_FW_UPn)) {
		k_msleep(5);
		if (!ioexp_get(ex_PD_FORCE_FW_UPn)) {
			k_msleep(10);
			if (!ioexp_get(ex_PD_FORCE_FW_UPn)) {
				LOG_DBG("Force the PD FW update as ex_DIGIT_DISP_SW is LOW during PorInit!!");
				_s_forcePdUpdate = true;
			}
		}
	}
#endif
}

/**
 * board_init_ifNeedForcePdFwUpdate
 *
 * @return true when need to force upgarde
 *
 * @note
 *   return if it needs force update PD FW even if the version match
 */
bool board_init_ifNeedForcePdFwUpdate(void)
{
	return _s_forcePdUpdate;
}

/**
 * board_init_informUserForPdUpdate
 *
 * @note
 *   inform user when PD FW upgarde by postcode led message
 */
void board_init_informUserForPdUpdate(void)
{
	/**
	 * This routine assumes below two variables are set as 0xFF before call to PD FW udpate routine
	 *
	 * 	g_u8Pd0Slot = 0xFF;
	 *  g_u8Pd1Slot = 0xFF;
	 *
	 * and it is doing after dev_max695x_Init()
	 *
	 * also the routine doesn't handle max695x power
	 */

	k_msleep(10);   /* wait for 3.3V_ALW stable */

	/* Print the Slot as 1-based number - "slotId + 1" - to align with BVM IDX. */
	postcode_led_hub_32bit_turnOn(16);
	if (g_u8Pd0Slot != 0xFF && g_u8Pd1Slot != 0xFF) {
		postcode_led_hub_print("P0-%X.P1-%X", (g_u8Pd0Slot + 1) & 0x0F, (g_u8Pd1Slot + 1) & 0x0F);
	} else if (g_u8Pd0Slot != 0xFF) {
		postcode_led_hub_print("PD0-SLT%X", (g_u8Pd0Slot + 1) & 0x0F);
	} else if (g_u8Pd1Slot != 0xFF) {
		postcode_led_hub_print("PD1-SLT%X", (g_u8Pd1Slot + 1) & 0x0F);
	} else {
		postcode_led_hub_print("Flash-PD");
	}
}

/**
 * board_init_platformInit
 *
 * @note
 *   board platform init
 */
void board_init_platformInit(void)
{
	uint32_t optionVal;
	int ret;

	/** ***************************************
	 * !!! Initialization order sensitively !!!
	 * ****************************************
	 *
	 * Read comments carefully before change 
	 * sequence.
	 *
	 * Summary:
	 *    - md_tick_stall() can work after md_tick_Init();
	 *    - AC_POWER_PRSNT_ECn pull-up to EC 3.3V
	 *    - CHG_ACOK can be used after F_ISL9241_REG_ACOKReference is set
	 *    - TI PD FW need to be downloaded before board_pwrSrc_powerOnReset to read PDO
	 *    - SPI access needs EC_1_8V_ALW_EN
	 *    - EC_1_8V_ALW_EN needs 10ms+ before stable (pwrSeq uses 20ms)
	 *    - Charger write needs app_smtbty_start()
	 *    - SPI bus must be free even no use, otherwise it can impact CPU to fetch code.
	 * ****************************************/

	/* Init the RRQ */
	u_rrq_init();
	dbgLogFifo_init();
	/******************************************
	 *
	 * EC_1_8V_ALW_EN needs to be 1 before SPI
	 * can access and AC_POWER_PRSNT_EC# taking
	 * effect.
	 *
	 * ****************************************/
	/* Set EC_1_8V_ALW_EN as POR O_1 to avoid power rail glitch on power on sequence */
	if (!gpio_read_pin(EC_1V8_S5_EN)) {
		gpio_write_pin(EC_1V8_S5_EN, 1);
		/* Wait 10ms until 1.8V_ALW stable */
		k_msleep(10);
	}
	/*
	 * Load board id should be the 1st thing after I2C is initilized
	 * Other components may depend on it.
	 */
	ret = brdId_Init();
	if (ret) {
	    LOG_ERR("Failed to fetch brd id: %d", ret);
	} else {
		LOG_DBG ("Read board id 0x%02X", brdId());
	}
	static const struct device *dev_bbram;
	dev_bbram =  DEVICE_DT_GET(BBRAM);
	if (!device_is_ready(dev_bbram)) {
			LOG_ERR("bbarm device not ready");
		};

	ret = bbram_check_power(dev_bbram);
	LOG_INF("bbram_check_power = %x",ret);

	/**
	 * Initialize NV option
	 */
	ret = f_nv_options_init();
	if (ret) {
	   LOG_ERR("Failed to init nv_options %d", ret);
	}

#if (BRDID_isTV)
	/* hard code the DAC */
	uint8_t offset;
	uint8_t value;
	if (BRDID_isPlum || BRDID_isJuniper) {
		offset = 0xF9;
		value = 0x45;
		i2c_hub_burst_write_multi_cmd(I2C_DAC_PWR_PORT, 0X70, &offset, 1, &value, 1);
	}
	if (BRDID_isPlum || BRDID_isHickory || BRDID_isJuniper) {
		offset = 0xFA;
		value = 0xB2;
		i2c_hub_burst_write_multi_cmd(I2C_DAC_PWR_PORT, 0X30, &offset, 1, &value, 1);
		// offset = 0xFB;
		// value = 0x32;
		// i2c_hub_burst_write_multi_cmd(I2C_DAC_PWR_PORT, 0X10, &offset, 1, &value, 1);
	}
#endif

	/* hard code the DAC */
	uint8_t offset;
	uint8_t value;

	offset = 0xF8;
	value = 0x8A;
    #if 0 // TODO_RTK
	i2c_hub_burst_write_multi_cmd(I2C_DAC_PWR_PORT, 0X50, &offset, 1, &value, 1);
    #endif
	/**
	 * Init IoExp and IoExp0 
	 */
#if CONFIG_IO_EXPANDER
	board_ioexp_initIoExp();
	board_ioexp_initIoExp0();
#endif

    #if 0 // TODO_RTK
	/* Set saf boot mode */
	espihub_set_saf_boot_mode(!ioexp_get(ex_EC_SHARE_SAFn_MODE));
    #endif
	/* Init other submodules */
#if CONFIG_SELECTABLE_DAC_VALUE
      board_dac_Init();
#endif

	/* Sync up critical NV options */
	/* Below two are used everywhere of thread init proc, sync the stauts here to ensure they are up to date */
	GET_NV_OPTIONS(chg_DcOnlyProchot, g_isThrottleApuInDcOnly);
	GET_NV_OPTIONS(chg_AcOnlyProchot, g_isThrottleApuInAcOnly);

	/* Enable the event for PMIC/VR adjustment */
	k_work_init(&pmicTunnel_work, board_pmicTunnelProc);
#if 0 // TODO_RTK
	/**
	 * Initialize postcode led
	 */
	if(BRDID_isHemlock||BRDID_isSequoia)
		postcode_led_hub_Init(MAX695, MPC_I2C_PORT);
	else {
		postcode_led_hub_Init(MPC, MPC_I2C_PORT);
		/* Register MPC update callback */
		dev_mpc_register_update_callback(board_init_mpc_update_callback);
	}

	/* preload the postcode on/off setting */
	GET_NV_OPTIONS(f_TurnOnPostCode, optionVal);
	if (optionVal)
		postcode_led_hub_set_sts(true);
	else
		postcode_led_hub_set_sts(false);
#endif
	/* turn off JTAG interface, as Zephyr cannot use Jtag to debug */
	board_turnOffJtagInterface();

	/* Initialize rom sig */
	f_romSig_init();

	/**
	 * Set the selected slot ID to 0xFF and it will be updated later depend on board config.
	 */
	g_u8Pd0Slot = g_u8Pd1Slot = 0xFF;

	/**
	 * read ex_DIGIT_DISP_SW status to override PD FW update flag
	 */
	if (0 == gpio_read_pin(AC_PRSNT_N)) {
		board_readDispSwSt();
	}
	/**
	 * MDS format
	 *
	 * Slot#  FW_NAME       Offset        Size
	 *   -    EC SIG        0x00000000    0x00001000
	 *  #0    PD TI 0       0x00001000    0x00008000  SKU1: PF10 silicon, TPS6699x,    USB4,   Supp PS8835,   Polling/Interrupt mode.
	 *  #1    PD TI 1       0x00009000    0x00008000  SKU2: FP10 silicon, TPS6699x,    USB4,   Supp PS8835,   Polling/Interrupt mode.
	 *  #2    PD TI 2       0x00011000    0x00008000  NA
	 *        **hole**      0x00019000    0x00007000  Hole
	 *   -    PSP SIG       0x00020000    0x00001000  PSP
	 *  #3    PD TI 3       0x00021000    0x00030000  SKU3: TPS6699x PD0/1 image
	 *  #4    PD RTK 0      0x00051000    0x00020000  SKU1: FP10 silicon, RTS5453P-VB, USB4,   Supp PS8835,   Polling/Interrupt mode.
	 *  #5    PD RTK 1      0x00071000    0x00020000  SKU2: FP10 silicon, RTS5453P-VB, USB4,   Supp PS8835,   Polling/Interrupt mode.
	 *  #6    PD IFX 0      0x00091000    0x00020000  SKU1: FP10 silicon, CCG8DF_CFP,  USB4,   Supp PS8835,   Polling/Interrupt mode.
	 *  #7    PD IFX 1      0x000B1000    0x00020000  SKU2: FP10 silicon, CCG8SF_CFP,  USB4,   Supp PS8835,   Polling/Interrupt mode.
	 *   -    EC FW         0x000D1000    0x00060000
	 *   ~    BIOS others   0x00131000    ....
	 */

#if CONFIG_SOC_SERIES_NPCX4
	amd_crb_drivers_spi_reset();
#endif

#if CONFIG_USBC_POWER_DELIVERY
	/* Load the default status from option */
	app_usbc_status.ui8Reg = 0;
	GET_NV_OPTIONS(usbc_status, app_usbc_status.ui8Reg);
#endif

	/* Separate the Parade and Kandou re-timer from board. 
	   But still need PD controller to report P8828A/PS8830 and KB8001/KB8002 */
#if (CONFIG_USBC_4CC)
	/* PdSig0 for TI PD Mayan/Mayan_KD/Lilac_KD_ER use TI PD*/
	if (CHECK_CFG(PD0, TPS66994BF) || CHECK_CFG(PD1, TPS66994BF)) {
		app_usbc_writeDevID(PD_DEVICE_ID_TIx);
		
		/* Fix the re-timer to PS8835 */
		app_usbc_status.field.retimer = APP_USBC_RETIMER_PS8835;
		
		/* Identify TI PD type before upgrade FW BH or BF */
		app_4cc_deviceInfo();

		/* Update TI PD only with AC plugged and is not boot from VCI */
		/* Chen: Need to add "F_BRAM_VCI_BOOT_SIGNATURE != f_bram_bootReason()" */
		if ((0 == gpio_read_pin(AC_PRSNT_N)) &&
			(1 == gpio_read_pin(EC_1V8_S5_EN))) {

			/* TIPD0 Tomcat */
			if ((g_pdCtrlSt[0].type[2] == 'T') && (g_pdCtrlSt[0].type[3] == 'C')) {
				/* point to the chip0 FW location in Bios ROM (offset 0) */
				g_u8Pd0Slot = 0;
				if (f_romSig_isValid(4))
					app_4cc_init(f_romSig_getOffset(4), f_romSig_getSize(4), APP_4CC_PD_LOAD_MODE_IMAGE, 0);
				if (f_romSig_isValid(0))
					app_4cc_init(f_romSig_getOffset(0), f_romSig_getSize(0), APP_4CC_PD_LOAD_MODE_CONFIG, 0);
			}
#if PD_TRIPPORT_ENABLE
			/* TIPD1 Tomcat */
			if ((g_pdCtrlSt[1].type[2] == 'T') && (g_pdCtrlSt[1].type[3] == 'C')) {
				/* point to the chip1 FW location in Bios ROM (offset 1) */
				g_u8Pd1Slot = 1;
				if (f_romSig_isValid(4))
					app_4cc_init(f_romSig_getOffset(4), f_romSig_getSize(4), APP_4CC_PD_LOAD_MODE_IMAGE, 1);
				if (f_romSig_isValid(1))
					app_4cc_init(f_romSig_getOffset(1), f_romSig_getSize(1), APP_4CC_PD_LOAD_MODE_CONFIG, 1);
			}
#endif
		} else {
			g_u8Pd0Slot = 0xFE;
			app_4cc_init(f_romSig_getOffset(0), f_romSig_getSize(0), APP_4CC_PD_LOAD_MODE_FLASH, 0);
#if PD_TRIPPORT_ENABLE
			g_u8Pd1Slot = 0xFE;
			app_4cc_init(f_romSig_getOffset(1), f_romSig_getSize(1), APP_4CC_PD_LOAD_MODE_FLASH, 1);
#endif	
		}	

		/* reload PD info again */
		app_4cc_deviceInfo();
		
		/* Support DP alt mode in port-0 and port-1 */
		dpm_set_status(TYPEC_PORT_0_IDX)->alt_mode_enable = true;
#if PD_DUALPORT_ENABLE
		dpm_set_status(TYPEC_PORT_1_IDX)->alt_mode_enable = true;
#endif
#if PD_TRIPPORT_ENABLE
		dpm_set_status(TYPEC_PORT_2_IDX)->alt_mode_enable = true;
#endif
		
		/* Update the retimer */
		if (app_usbc_status.field.retimer != g_4cc_tiPdRetimerType[0]) {
			if (g_4cc_tiPdRetimerType[0] >= APP_USBC_RETIMER_PS8828) {
				app_usbc_status.field.retimer = g_4cc_tiPdRetimerType[0];
				SET_NV_OPTIONS(usbc_status, app_usbc_status.ui8Reg);
			}
		}
	}
#endif
#if (CONFIG_USBC_CCGX)
    if (CHECK_CFG(PD0, CCG8DF) || CHECK_CFG(PD1, CCG8SF)) {
		app_usbc_writeDevID(PD_DEVICE_ID_CCGx);
		
		/* Fix the re-timer to PS8835 */
		app_usbc_status.field.retimer = APP_USBC_RETIMER_PS8835;
		
		/* Update CY PD only with AC plugged in */
		/* Chen: Need to add "F_BRAM_VCI_BOOT_SIGNATURE != f_bram_bootReason()" */
		if ((0 == gpio_read_pin(AC_PRSNT_N)) && f_romSig_isValid(7) && f_romSig_isValid(8) &&
			(1 == gpio_read_pin(EC_1V8_S5_EN))) {
			/* point to the chip0 FW location in Bios ROM (offset 7) - PS8835 */
			g_u8Pd0Slot = 5;
			if (app_ccgx_firmwareUpdateCcg8(board_hpiFwUpdateCallback, f_romSig_getOffset(7), f_romSig_getSize(7), 0)) {
				LOG_DBG("chipID:0 CCGx FW updated success, offset7!");
			}
			/* point to the chip1 FW location in Bios ROM (offset 8) - PS8835 */
			g_u8Pd1Slot = 6;
			if (app_ccgx_firmwareUpdateCcg8(board_hpiFwUpdateCallback, f_romSig_getOffset(8), f_romSig_getSize(8), 1)) {
				LOG_DBG("chipID:1 CCGx FW updated success, offset8!");
			}
		}	
		
		/* Set dead battery if detect PD presented */
		if (gpio_read_pin(PD_PRSNT_N) == 0) {
			dpm_set_status (TYPEC_PORT_0_IDX)->dead_bat = true;
			LOG_DBG("CYPD dead bat");
		}
	}
#endif
#if (CONFIG_USBC_RTK)
	if (CHECK_CFG(PD0, RTK) || CHECK_CFG(PD1, RTK)) {
		app_usbc_writeDevID(PD_DEVICE_ID_RTKx);
		if (f_romSig_isValid(5) && f_romSig_isValid(6)) {  /* 7 and 8 */
			/* Update PD0 Dual-ports */
			app_rtk_init(f_romSig_getOffset(5), 0x20000, 0);
			/* Update PD1 Single-port */
			app_rtk_init(f_romSig_getOffset(6), 0x20000, 1);
		}
	}
#endif

	memcpy(g_pdCtrlSt[0].retimer, "PS8835", 7);
	memcpy(g_pdCtrlSt[1].retimer, "PS8835", 7);


	/* ****************************************
	 *
	 * Restore EC_S5_PWREN/RSMRSTn after PD FW had
	 * been loaded. These pins will be set
	 * properly later by power sequence routine.
	 *
	 * ****************************************/
	gpio_write_pin(EC_S5_PWREN, 0);
	gpio_write_pin(RSMRST_N, 0);
	// gpio_write_pin(BAT_CHG1_CHG2n_SEL, 0);

	/**
	 * Initialize ACPI planes
	 */
	md_acpiplanes_init();
	ACPI_planeInit();

#if (CONFIG_USBC_CCGX)
	if (app_usbc_readDevID() == PD_DEVICE_ID_CCGx)
		app_ccgx_updatePwrStatus(SYSTEM_G3_STATE);
#endif

#if CONFIG_BATTERY_MANAGEMENT
	/* Init the smart battery and charger */
	board_smtbty_taskInit();
#endif /* CONFIG_BATTERY_MANAGEMENT */

#if CONFIG_USBC_POWER_DELIVERY
	/* app usbc task init before 1.8V alw shut down (EC cannot access PD controller I2C in G3) */
	/* after 1.8V alw shut down EC will run the G3->S5 power sequence in app_pseq thread */
	app_usbc_task_init();
#endif

#if CONFIG_POWER_SOURCE_MANAGEMENT
	/* power on reset charger setting */
	board_pwrSrc_powerOnReset();
#endif /* CONFIG_POWER_SOURCE_MANAGEMENT */

    /* set PD to sink only from G3' to G3 */
	brdId_pdPowerRoleG3();

	/* Reset AUX and PWRENs */
	board_init_apuResetHandler();

	/* Init FW status planes */
	fwSt_init();
#if CONFIG_WIRELESS_MANAGEABILITY
	/* Init manOs management*/
	app_manOs_init();
#endif
	/* SPI S0 sharing tester */
#if CONFIG_S0S_TEST
	if (espihub_get_saf_boot_mode() == false) {
		app_spiS0_Init();
	}
#endif

	/* Init the timer for APU reset trigger */
	k_timer_init(&g_apuRstTrigger, board_init_apuRstTrigger, NULL);
#if (CONFIG_SFH)
	/* Init SFH */
	GET_NV_OPTIONS(waie_b_value, g_WAIE_RITE);
	if(g_WAIE_RITE)
	{
		app_sfh_waie_init();
	} else {
		/* Init SFH */
		app_sfh_init ();
	}
#endif

	/** ****************************************
	 *
	 * Turn EC_1_8V_ALW_EN off before exit platform
	 * initialization. This power rail will set
	 * properly later by power sequence routine.
	 *
	 * ****************************************/
	gpio_write_pin(EC_1V8_S5_EN, 0);
}

/**
 * board_init_kbcRst
 *
 * @note
 *   Handle KBRSTn by FW (6ms applied here but the Spec required 6us)
 *   For AMD, at least 150us is needed.
 */
void board_init_kbcRst(void)
{
	if (g_triggerKbcRst == 1) {
		gpio_write_pin(KBRST_N, 1);
		k_msleep(6);
		gpio_write_pin(KBRST_N, 0);

		g_triggerKbcRst = 0;
	}
}

/**
 * board_init_apuResetHandler
 *
 * @note
 *   This handler is called whenever APU_RST# had asserted
 *   >>EXCEPT<<
 *   the assertion in S3/S0i3 state
 */
void board_init_apuResetHandler (void)
{
	LOG_DBG("APU RESET proc ... Reset ex_TPM_S0I3n to 1");
	// /* U117 */
	// IOEXP_BatchStart;
	// IOEXP_BatchSet(ex_TPM_S0I3n, 1);        /* IO_0 = 1 to allow PCIe reset to TPM; */
	//                                         /* BIOS set it to 0 in S0i3 platform just before hand over to OS */
	// IOEXP_BatchEnd;

	/*
	 * Reset EVAL ctrl pins to 0
	 */
	board_evalCardCtrlPins();

	/*
	 * Reset system config on APU-RESET
	 */
#if CONFIG_APP_THERMAL
	/* this setting can keep across S0i3 */
	g_p3tConfigLimit = 0xFF;

	/* EC will not set P3T again after S0i3 */
	app_thermal_resetP3tSetting ();
#endif
#if CONFIG_SFH
	/* clear waie on APU_RST# event */
	waie_syc_flag = 0xD1;
#endif
	/*
	 * Hook eSPI on-reset handler
	 */
	//app_espi_onReset();
}

/** ***************************************************
 * AUX# reset and PwrEn may have to be restored on Sleep Exit.
 * Below two static variables are shared between SleepEnter 
 * and SleepExit so as to restore the status.
 * ***************************************************/
static uint16_t _s_pinStU5100 = 0;  /* status of U5100 */
static uint16_t _s_pinStU5101 = 0;  /* status of U5101 */
static uint16_t _s_pinStU5102 = 0;  /* status of U5102 */
static uint16_t _s_pwrEnRest = 0;

/**
 * board_init_apuSleepEnterHandler
 *
 * @note
 *   handler to process apu sleep mode enter
 */
void board_init_apuSleepEnterHandler (void)
{
#if 0 // TODO_RTK
	/**
	 * Don't touch AUX reset across S3/S0i3
	 */
	_s_pinStU5100 = dev_pca9535_smartRead(IOEXP_I2C_ADDR_U5100_I2C1_0x21, IOEXP_I2C_PORT_U5100_I2C1_0x21, 0xFFFF);
	_s_pinStU5101 = dev_pca9535_smartRead(IOEXP_I2C_ADDR_U5101_I2C1_0x22, IOEXP_I2C_PORT_U5101_I2C1_0x22, 0xFFFF);
	_s_pinStU5102 = dev_pca9535_smartRead(IOEXP_I2C_ADDR_U5102_I2C1_0x23, IOEXP_I2C_PORT_U5102_I2C1_0x23, 0xFFFF);


	_s_pwrEnRest = 0;
	_s_pwrEnRest |= gpio_read_pin(EC_EVAL_SLT_PWREN) ? 0x0001 : 0; /* bit0 EC_EVAL_SLT_PWREN */

	uint32_t need2keepWlanPwr;
	GET_NV_OPTIONS(pwr_keepWlanPwrInS3, need2keepWlanPwr);

	if (g_ui8MondernStandbySupport) {
		/* Modern standby case */
		LOG_DBG("S0i3 enter proc ... U141:%04X, U148:%04X, x8Slot:%02X", _s_pinStU5100, _s_pinStU5102, _s_pwrEnRest);

		if (BRDID_isPlum && BRDID_isPEO) { /* Includes SSD2/SSD3/SSD4 on SLT board */
			ioexp_set(ex_EP_WWAN_PWREN, 0);
		}
		IOEXP_BatchStart;
		/* U124 */
		IOEXP_BatchSet(ex_EP_SSD0_PWREN, 0);   /* Depend on NVMe D3Cold is enabled or not, */
		                                       /* EC shall optional turn on NVMe on S0i3 exit. */
		IOEXP_BatchSet(ex_EP_SSD1_PWREN, 0);   /* Depend on NVMe D3Cold is enabled or not, */
		                                       /* EC shall optional turn on NVMe on S0i3 exit. */
	  if (BRDID_isPlum && BRDID_isPEO) {      /* Includes SSD2/SSD3/SSD4 on SLT board */
			IOEXP_BatchSet(ex_EP_WLAN_3V3_PWREN, 0);
			gpio_write_pin(EC_EVAL_SLT_PWREN, 0);
	  }
		IOEXP_BatchEnd;

		/* ex_SMBUS1_BUFF_EN : S0i3 power saving purpose */
		if (_s_pinStU5102 & IOEXP_BitMask(ex_EP_SMB1_BUFF_EN)) { /* if ex_SMBUS1_BUFF_EN is enabled */
			/* set it to off in S0i3 for power saving */
			ioexp_set(ex_EP_SMB1_BUFF_EN, 0);
		}

#if CONFIG_BATTERY_MANAGEMENT
		/* PLAT-85968 */
		board_smtbty_chgPwrSavingS0i3Enter();
#endif

	} else {
		/* S3 */
		LOG_DBG("S3 enter proc ... U141:%04X, U148:%04X, x8Slot:%02X", _s_pinStU5100, _s_pinStU5102, _s_pwrEnRest);

		if (BRDID_isPlum && BRDID_isPEO) {
			ioexp_set(ex_EP_WWAN_PWREN, 0);   /* IO_11 = 0 */
			ioexp_set(ex_EP_WLAN_3V3_PWREN, 0);   /* IO_12 (Turn off SSD4 power) */
			gpio_write_pin(EC_EVAL_SLT_PWREN, 0);     /* (Turn off SSD2 power) */
#if CONFIG_WIRELESS_MANAGEABILITY
		} else if (!need2keepWlanPwr && !app_manOs_getEnFlag()) {  /* pwr_keepWlanPwrInS3, required by [CZN] PLAT-64301 */
			ioexp_set(ex_EP_WWAN_PWREN, 0);   /* IO_11 = 0 */
			ioexp_set(ex_EP_WLAN_3V3_PWREN, 0);   /* IO_12 (Keep if need2keepWlanPwr or mOS_S3) */
		}
#else
		} else if (!need2keepWlanPwr) {  /* pwr_keepWlanPwrInS3, required by [CZN] PLAT-64301 */
			ioexp_set(ex_EP_WWAN_PWREN, 0);   /* IO_11 = 0 */
			ioexp_set(ex_EP_WLAN_3V3_PWREN, 0);   /* IO_12 (Keep if need2keepWlanPwr or mOS_S3) */
		}
#endif
		/* U124 */
		IOEXP_BatchStart;
		/** Assume mOS only enabled in S0i3 platform **
		 * Thus S3 can only occurs for mOS sleep.
		 * Otherwise, below expression has to differentiate it is mOS_S3 vs HOST_S3 case
		 */
		IOEXP_BatchSet(ex_EP_TPM_PWREN, 0);     /* IO_0 = 0 */
		IOEXP_BatchSet(ex_EP_SD_AUX_PWREN, 0);  /* IO_4 = 0 */
		IOEXP_BatchSet(ex_EP_SSD0_PWREN, 0);	/* IO_6 = 0 */
		IOEXP_BatchSet(ex_EP_SSD1_PWREN, 0);	/* IO_7 = 0 */
		IOEXP_BatchEnd;

	}
#endif
}

/**
 * board_init_apuSleepExitHandler
 *
 * @note
 *   handler to process apu sleep mode exit
 */
void board_init_apuSleepExitHandler (void)
{
#if 0 // TODO_RTK
	/**
	 * Bugcheck 0xEF CRITICAL_PROCESS_DIED
	 *
	 *   One possible casue of Bugcheck 0xEF is NVMe/SSD be powered off
	 *   unexpectedly by on S0i3 resume as it executes app_misc_ApuResetHandler()
	 *
	 *   Log trace of a failed case as below:
	 *
	 *   >>> APU_RST_L_BUF <<< is 0
	 *   >>> SLP_S3_S0A3n <<< is 0
	 *   [PWRSEQ]     State transfer <17, 1>
	 *   [PWRSEQ]     Set pwr state 3
	 *   [PWRSEQ]     PwrStChangeHandler: from 0 to 3
	 *   S0i3 enter proc ... AUX:01DC, PwrEn:01FC
	 *   R IoExp 20 00 5F A 2
	 *   W IoExp 1A 01 1C A 2
	 *   [ManOS]      Set        _s_mosSt            (0xFF) st_UNINIT
	 *   R IoExp 21 00 AD A 2
	 *   R IoExp 21 00 AC A 2
	 *   >>> SLP_S3_S0A3n <<< is 1
	 *   [PWRSEQ]     State transfer <14, 1>
	 *   [PWRSEQ]     Set pwr state -1
	 *   [PWRSEQ]     PwrStChangeHandler: from 3 to -1
	 *   S0i3 exit proc ... AUX:01DC, PwrEn:01FC
	 *   W IoExp 1A 01 3C A 2
	 *   [PWRSEQ]     State transfer <16, 100>
	 *   R IoExp 21 00 AE A 2
	 *   R IoExp 21 00 AF A 2
	 *   HOOK_Init: set MsFlag 1, u8hadS3 1, KbcWakeEvt 0
	 *   HOOK_Init: set KbcWakeEvt = 2
	 *   APU RESET proc ... Set all AUX reset to 0 and all PwrEn (except network devices) to 0  << **unexpected**
	 *   W IoExp 1A 01 04 A 2
	 *   W IoExp 18 01 90 A 2
	 *   >>> APU_RST_L_BUF <<< is 1
	 *   [PWRSEQ]     Set pwr state 0
	 *   
	 *   The only possibility that causing this sequence is SoC exit S0i3 right away
	 *   (less than APP_MISC_APU_RST_MINIMUM_PULSE) millisecond. Hence ApuSleepExitHandler
	 */

	if (g_ui8MondernStandbySupport) {
		uint32_t ssd0d3c, ssd1d3c;

		GET_NV_OPTIONS(pwr_SSD0D3Cold, ssd0d3c);
		GET_NV_OPTIONS(pwr_SSD1D3Cold, ssd1d3c);
		/* Modern standby case */
		LOG_DBG("S0i3 exit proc ... U141:%04X, U148:%04X, x8Slot:%02X, SSD3C(%d, %d)", _s_pinStU5100, _s_pinStU5102, _s_pwrEnRest, ssd0d3c, ssd1d3c);
		if (BRDID_isPlum && BRDID_isPEO) {             /* Assume SSD2/SSD3/SSD4 on SLT board don't support D3Cold sequence */
			if (_s_pinStU5101 & IOEXP_BitMask(ex_EP_WWAN_PWREN)) { /* if SSD3 power was enabled */
				ioexp_set(ex_EP_WWAN_PWREN, 1);              /* then turn it on */
			}
		}
		IOEXP_BatchStart;
		if ((!ssd0d3c) && (_s_pinStU5100 & IOEXP_BitMask(ex_EP_SSD0_PWREN))) { /* if SSD0 power was enabled */
			IOEXP_BatchSet(ex_EP_SSD0_PWREN, 1);                              /* then turn it on */
		}
		if ((!ssd1d3c) && (_s_pinStU5100 & IOEXP_BitMask(ex_EP_SSD1_PWREN))) { /* if SSD1 power was enabled */
			IOEXP_BatchSet(ex_EP_SSD1_PWREN, 1);                              /* then turn it on */
		}
		if (BRDID_isPlum && BRDID_isPEO) {             /* Assume SSD2/SSD3/SSD4 on SLT board don't support D3Cold sequence */
			if (_s_pwrEnRest & 0x0001) {                /* if SSD2 power was enabled */
				gpio_write_pin(EC_EVAL_SLT_PWREN, 1);       /* then turn it on */
			}
			if (_s_pinStU5100 & IOEXP_BitMask(ex_EP_WLAN_3V3_PWREN)) { /* if SSD4 power was enabled */
				IOEXP_BatchSet(ex_EP_WLAN_3V3_PWREN, 1);              /* then turn it on */
			}
		}
		IOEXP_BatchEnd;

		/* ex_SMBUS1_BUFF_EN : S0i3 power saving purpose */
		if (_s_pinStU5102 & IOEXP_BitMask(ex_EP_SMB1_BUFF_EN)) {   /* if ex_SMBUS1_BUFF_EN was enabled */
			ioexp_set(ex_EP_SMB1_BUFF_EN, 1);                     /* restore its status */
		}

#if CONFIG_BATTERY_MANAGEMENT
		/* PLAT-85968 */
		board_smtbty_chgPwrSavingS0i3Exit();
#endif

	} else {
		/* S3 */
		LOG_DBG("S3 exit proc ... U141:%04X, U148:%04X, x8Slot:%02X", _s_pinStU5100, _s_pinStU5102, _s_pwrEnRest);

		if (BRDID_isPlum && BRDID_isPEO) {                      /* On Maple SLT */
			if (_s_pwrEnRest & 0x0001) {                         /* if SSD2 power was enabled */
				gpio_write_pin(EC_EVAL_SLT_PWREN, 1);                /* then turn it on */
			}
		}
		if (_s_pinStU5101 & IOEXP_BitMask(ex_EP_WWAN_PWREN)) {         /* if WWAN/SSD3 power was enabled */
			ioexp_set(ex_EP_WWAN_PWREN, 1);                      /* then turn it on */
		}
		IOEXP_BatchStart;
		/* U148 */
		if (_s_pinStU5100 & IOEXP_BitMask(ex_EP_WLAN_3V3_PWREN)) {         /* if WLAN/SSD4 power was enabled */
			IOEXP_BatchSet(ex_EP_WLAN_3V3_PWREN, 1);                      /* then turn it on */
		}
		if (_s_pinStU5100 & IOEXP_BitMask(ex_EP_SD_AUX_PWREN)) {           /* if SD power was enabled */
			IOEXP_BatchSet(ex_EP_SD_AUX_PWREN, 1);                        /* then turn it on */
		}
		IOEXP_BatchSet(ex_EP_SD_MAIN_PWREN, 1);					   // IO_12 = 1
		if (_s_pinStU5100 & IOEXP_BitMask(ex_EP_SSD0_PWREN)) {      /* if SSD0 power was enabled */
			IOEXP_BatchSet(ex_EP_SSD0_PWREN, 1);                   /* then turn it on */
		}
		if (_s_pinStU5100 & IOEXP_BitMask(ex_EP_SSD1_PWREN)) {      /* if SSD1 power was enabled */
			IOEXP_BatchSet(ex_EP_SSD1_PWREN, 1);                   /* then turn it on */
		}
		if (_s_pinStU5100 & IOEXP_BitMask(ex_EP_TPM_PWREN)) {
			IOEXP_BatchSet(ex_EP_TPM_PWREN, 1);
		}
		IOEXP_BatchEnd;

	}
#endif
}

/**
 * board_init_smartMux_V0
 *
 * @note
 *   smart mux setting V0
 */
void board_init_smartMux_V0(void)
{
#if 0 // TODO_RTK
	for (uint8_t i = 0; i < 4; i++) {
		IOEXP_SETBIT( IOEXP_I2cSlvAddr(_s_smartMuxTab[i].Pin), IOEXP_I2cPort(_s_smartMuxTab[i].Pin), IOEXP_IdxOfGroup(_s_smartMuxTab[i].Pin),
			IOEXP_IsPushPullPin(_s_smartMuxTab[i].Pin) ? 1 : 0,
			IOEXP_IsOpenDrainPin(_s_smartMuxTab[i].Pin) ? 1 : 0,
			IOEXP_PORSeting(_s_smartMuxTab[i].Pin));
	}
#endif
}

/**
 * board_init_smartMux_V1
 *
 * @note
 *   smart mux setting V1
 */
void board_init_smartMux_V1(void)
{
#if 0 // TODO_RTK
	for (uint8_t i = 0; i < 4; i++) {
		IOEXP_SETBIT( IOEXP_I2cSlvAddr(_s_smartMuxTab[i].Pin), IOEXP_I2cPort(_s_smartMuxTab[i].Pin), IOEXP_IdxOfGroup(_s_smartMuxTab[i].Pin),
			1,
			0,
			_s_smartMuxTab[i].MuxSetting[0]);

	}
#endif
}

/**
 * board_init_smartMux_V2
 *
 * @note
 *   smart mux setting V2
 */
void board_init_smartMux_V2(void)
{
#if 0 // TODO_RTK
	for (uint8_t i = 0; i < 4; i++) {
		IOEXP_SETBIT( IOEXP_I2cSlvAddr(_s_smartMuxTab[i].Pin), IOEXP_I2cPort(_s_smartMuxTab[i].Pin), IOEXP_IdxOfGroup(_s_smartMuxTab[i].Pin),
			1,
			0,
			_s_smartMuxTab[i].MuxSetting[1]);

	}
#endif
}

/**
 * board_init_smartMux_V3
 *
 * @note
 *   smart mux setting V3
 */
void board_init_smartMux_V3(void)
{

}

/**
 * board_init_updateEcInfoEventTrigger
 *
 * @note
 *   trigger update EC information event
 */
void board_init_updateEcInfoEventTrigger(void)
{
	/* Directly set the flag to trigger MPC update in callback */
#ifndef CONFIG_EC_MODULE
	g_updateinfo = true;
#endif
}

/**
 * board_init_mpc_update_callback
 *
 * @note
 *   MPC update callback - updates EC, PD, BIOS version, MAC address, CPU ID and OPN to MPC
 *   This callback is called periodically by MPC thread and checks if update is needed
 */
static void board_init_mpc_update_callback(void)
{
	extern uint8_t g_biosVer[36];
	extern uint8_t g_nicMacAddr[29];
	extern uint8_t g_cpuId[16];
	extern uint8_t g_cpuOpn[32];
	extern uint8_t g_cpuSn[8];
	uint8_t _s_ledDevice;

	/* Check if update is requested */
	if (g_updateinfo == false) {
		return;
	}

	/* Check if device is MAX695 - this feature is not supported */
	_s_ledDevice = postcode_get_led_device();
	if (_s_ledDevice == MAX695) {
		g_updateinfo = false;
		return;
	}
	uint8_t ver0[3];
	uint8_t ver1[3];
	uint8_t romVersion[16];
	/* Collect PD version information */
	ver0[0] = g_pdCtrlSt[0].VVVV;
	ver0[1] = g_pdCtrlSt[0].MM;
	ver0[2] = g_pdCtrlSt[0].RR;
	
	ver1[0] = g_pdCtrlSt[1].VVVV;
	ver1[1] = g_pdCtrlSt[1].MM;
	ver1[2] = g_pdCtrlSt[1].RR;
	
	/* Get EC ROM version string */
	// ec_version_stringGet(3, romVersion, 12); // TODO_RTK

	/* Update all version information to MPC */
	dev_mpc_setEcVersion(romVersion, ver0, ver1);
	dev_mpc_setBiosVersion(g_biosVer, sizeof(g_biosVer));
	dev_mpc_setMacVersion(g_nicMacAddr, sizeof(g_nicMacAddr));
	dev_mpc_setCpuId(g_cpuId, sizeof(g_cpuId));
	dev_mpc_setCpuOpn(g_cpuOpn, sizeof(g_cpuOpn));
	dev_mpc_setCpuSN(g_cpuSn, sizeof(g_cpuSn));

	/* Clear the update flag */
	g_updateinfo = false;
}