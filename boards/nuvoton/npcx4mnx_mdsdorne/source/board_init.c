/*****************************************************************************
 * Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
 ******************************************************************************
 */

#include "ioexp_hub.h"
#include "u_rrq.h"
#if (CONFIG_USBC_4CC)
#include "app_4cc.h"
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
#include "board_pwrSrc.h"
#include "app_acpi.h"
#include "board_id.h"
#include "board_smtbty.h"
#include "f_romSig.h"
#include "app_AcDcSwitch.h"
#include "app_udc.h"
#include "fwSt.h"
#include "app_manOs.h"
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
#include "f_bram.h"

LOG_MODULE_REGISTER(brdint, CONFIG_BOARD_INIT_LOG_LEVEL);

/* ************************** *
 *      external valuable     *
 * ************************** */
extern uint8_t g_u8Pd0Slot;
extern uint8_t g_u8Pd1Slot;

extern uint8_t g_biosVer[36];
extern uint8_t g_nicMacAddr[29];
extern uint8_t g_cpuId[16];
extern uint8_t g_biosVer[36];
extern uint8_t g_cpuOpn[32];

/* ************************** *
 *      static valuable       *
 * ************************** */
static bool _s_forcePdUpdate = 0;
/* pmic DAC I2C tunnel for ACPI DAC control */
static struct k_work pmicTunnel_work;
/* Timer for the APU Rst Trigger */
static struct k_timer g_apuRstTrigger;

/* board status led symbol */
static struct k_timer g_board_led_timer;
static uint8_t g_board_led_sts = 0;
static uint8_t g_board_led_set_level = 0;
static k_ticks_t g_board_led_remain_tick = 0;
static k_ticks_t g_board_led_lock_tick = 0;
static uint32_t  g_board_led_lock_nest = 0;
static bool g_board_led_sts_resume = false;
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
/* MPC update info flag */
static bool g_updateinfo = false;

#define BOARD_LED_CYCLE_TIME_MS (250)

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

		/**
		 * app_manOs hook for APU_RST
		 */
		app_manOs_ApuRstHook();
	}
}

/**
 * board_status_ledOff
 *
 * @note
 *  Turn off led
 */
static void board_status_ledOff(void)
{
	gpio_write_pin(EC_BLINK_N, 1); // off
}

/**
 * board_status_ledOn
 *
 * @note
 *  Turn on led
 */
static void board_status_ledOn(void)
{
	gpio_write_pin(EC_BLINK_N, 0); // on
}

/**
 * board_status_ledCallback
 *
 * @note
 *  board status indicated by the blinked LED
 */
static void board_status_ledCallback(struct k_timer *timer)
{
	static uint8_t _s_led_sts = 0;
	static uint8_t _s_led_on_cnt = 0;

	if (g_board_led_sts_resume) {
		_s_led_on_cnt = 0;
		_s_led_sts = g_board_led_sts - 1;
		g_board_led_sts_resume = false;
	}

	if (_s_led_on_cnt) {
		board_status_ledOn();
		g_board_led_set_level = 0;
	} else {
		board_status_ledOff();
		g_board_led_set_level = 1;
	}

	uint8_t cnt = (_s_led_sts - _s_led_on_cnt);
	uint32_t ms = 0;
	if (cnt) {
		_s_led_on_cnt += cnt;
		ms = BOARD_LED_CYCLE_TIME_MS * cnt;
	} else {
		_s_led_on_cnt = 0;
		_s_led_sts = g_board_led_sts - 1;
		ms = BOARD_LED_CYCLE_TIME_MS;
	}
	k_timer_start(&g_board_led_timer, K_MSEC(ms), K_NO_WAIT);
}

/**
 * board_status_led_set
 *
 * @param [in]   val: 0: always turn off
 *                    1: always turn on
 *                    2 ~ 255: blinking speed
 *
 * @note
 *  board status indicated by the blinked LED
 */
void board_status_led_set(uint8_t val)
{
	if (val == g_board_led_sts) {
		return;
	}

	bool stop = false;
	if (val == 0) {
		stop = true;
		board_status_ledOff();
		g_board_led_set_level = 1;
	} else if (val == 1) {
		stop = true;
		board_status_ledOn();
		g_board_led_set_level = 0;
	}

	if (stop) {
		k_timer_stop(&g_board_led_timer);
	} else {
		// start a new cycle.
		if (g_board_led_sts == 0 || g_board_led_sts == 1) {
			k_timer_start(&g_board_led_timer, K_MSEC(1), K_NO_WAIT);
			g_board_led_sts_resume = true;
		}
	}

	g_board_led_sts = val;
}

/**
 * board_status_led_lock
 *
 * @note
 *   led statue lock.
 */
void board_status_led_lock(void)
{
	g_board_led_lock_nest++;
	if (g_board_led_lock_nest > 1) {
		return ;
	}

	g_board_led_remain_tick = k_timer_remaining_ticks(&g_board_led_timer);
	g_board_led_lock_tick = k_uptime_ticks();
	k_timer_stop(&g_board_led_timer);
	board_status_ledOff();
}

/**
 * board_status_led_unlock
 *
 * @note
 *   led statue unlock.
 */
void board_status_led_unlock(void)
{
	if (g_board_led_lock_nest) {
		g_board_led_lock_nest--;
	}

	if (g_board_led_lock_nest) {
		return ;
	}
	k_ticks_t t = k_uptime_ticks() - g_board_led_lock_tick;
	if (t <= g_board_led_remain_tick) {
		t = g_board_led_remain_tick - t;
		gpio_write_pin(EC_BLINK_N, g_board_led_set_level);
	} else {
		t = 1;
	}
	k_timer_start(&g_board_led_timer, K_TICKS(t), K_NO_WAIT);
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
 *   HPI firmware update callback handler
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
	_s_forcePdUpdate = false;
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

	/* To turn off all BGPO pins (BGPO0 is NC on EC module. And it cannot be disabled) */
	//bgpo_disable();

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
	k_timer_init(&g_board_led_timer, board_status_ledCallback, NULL);

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

	if (BRDID_isTV) {
		gpio_write_pin(TV_EN, 1);
	} else {
		gpio_write_pin(TV_EN, 0);
	}

#ifdef BRD_MDS_DORNE
	extern uint8_t g_S5AlwEnFlag;
	GET_NV_OPTIONS(s5_alw_enable, g_S5AlwEnFlag);
#endif

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

	/* hard code the DAC */
	uint8_t offset = 0xF9;
	uint8_t value = 0x45;
	i2c_hub_burst_write_multi_cmd(I2C_DAC_PWR_PORT, 0X70, &offset, 1, &value, 1);
	offset = 0xFA;
	value = 0xB2;
	i2c_hub_burst_write_multi_cmd(I2C_DAC_PWR_PORT, 0X30, &offset, 1, &value, 1);
	offset = 0xFB;
	value = 0x32;
	i2c_hub_burst_write_multi_cmd(I2C_DAC_PWR_PORT, 0X10, &offset, 1, &value, 1);

	/**
	 * Init IoExp and IoExp0
	 */
#if CONFIG_IO_EXPANDER
	board_ioexp_initIoExp();
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

	/**
	 * Initialize postcode led
	 */
#ifndef CONFIG_EC_MODULE
	postcode_led_hub_Init(MPC, MPC_I2C_PORT);
	/* Register MPC update callback */
	dev_mpc_register_update_callback(board_init_mpc_update_callback);
#endif

	/* preload the postcode on/off setting */
	GET_NV_OPTIONS(f_TurnOnPostCode, optionVal);
	if (optionVal)
		postcode_led_hub_set_sts(true);
	else
		postcode_led_hub_set_sts(false);

	/* turn off JTAG interface, as Zephyr cannot use Jtag to debug */
	board_turnOffJtagInterface();

	/* Initialize rom sig */
	f_romSig_init();

	/**
	 * Set the selected slot ID to 0xFF and it will be updated later depend on board config.
	 */
	g_u8Pd0Slot = g_u8Pd1Slot = 0xFF;

	board_readDispSwSt();
	/**
	 * MDS Dorne eFFP format
	 *
	 * Slot#  FW_NAME       Offset        Size
	 *   -    EC SIG        0x00000000    0x00001000
	 *  #0    PD TI 0       0x00001000    0x00008000  SKU0:  PF10 silicon,   TPS6699x AppConfig, USB4,      Supp PS8835,         Interrupt mode.
	 *  #1    PD TI 1       0x00009000    0x00008000  NA
	 *  #2    PD TI 2       0x00011000    0x00008000  NA
	 *        **hole**      0x00019000    0x00007000  Hole
	 *   -    PSP SIG       0x00020000    0x00001000  PSP
	 *  #3    PD TI 3       0x00021000    0x00040000  Image: FP10 silicon,   TPS6699x Image
	 *   -    EC FW         0x00061000    0x00060000
	 *   ~    BIOS others   0x000C1000    ....
	 */

	/* eFFP uses TI PD and no need to detect */
	app_usbc_writeDevID(PD_DEVICE_ID_TIx);
	LOG_WRN("Detect tps");


#if CONFIG_SOC_SERIES_NPCX4
	amd_crb_drivers_spi_reset();
#endif

	/* Load the default status from option */
	app_usbc_status.ui8Reg = 0;
	GET_NV_OPTIONS(usbc_status, app_usbc_status.ui8Reg);

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

		board_status_led_set(4);
		/* Update TI PD only with AC plugged and is not boot from VCI */
		/* Chen: Need to add "F_BRAM_VCI_BOOT_SIGNATURE != f_bram_bootReason()" */
		if (1 == gpio_read_pin(EC_1V8_S5_EN)) {

			/* TIPD0 Tomcat */
			if ((g_pdCtrlSt[0].type[2] == 'T') && (g_pdCtrlSt[0].type[3] == 'C')) {
				/* point to the chip0 FW location in Bios ROM (offset 0) */
				g_u8Pd0Slot = 0;
				app_4cc_init(f_romSig_getOffset(3), f_romSig_getSize(3), APP_4CC_PD_LOAD_MODE_IMAGE, 0);
				app_4cc_init(f_romSig_getOffset(0), f_romSig_getSize(0), APP_4CC_PD_LOAD_MODE_CONFIG, 0);
			}
		} else {
			g_u8Pd0Slot = 0xFE;
			app_4cc_init(f_romSig_getOffset(0), f_romSig_getSize(0), APP_4CC_PD_LOAD_MODE_FLASH, 0);
		}
		board_status_led_set(0);

		/* reload PD info again */
		app_4cc_deviceInfo();

		/* Wait status update start (Dead battery + all unattached) */
		if ((dpm_get_info (TYPEC_PORT_0_IDX)->dead_bat == true) ||
#if PD_TRIPPORT_ENABLE
			(dpm_get_info (TYPEC_PORT_2_IDX)->dead_bat == true) ||
#endif
			(dpm_get_info (TYPEC_PORT_1_IDX)->dead_bat == true)) {
			/* Waiting attach event */
			if ((dpm_get_info (TYPEC_PORT_0_IDX)->attach == false)
#if PD_DUALPORT_ENABLE
			 && (dpm_get_info (TYPEC_PORT_1_IDX)->attach == false)
#endif
#if PD_TRIPPORT_ENABLE
			 && (dpm_get_info (TYPEC_PORT_2_IDX)->attach == false)
#endif
				 ) {
				/* wait 10 cycle */
				uint8_t waitCnt = 10;
				while(waitCnt--) {
					if (app_4cc_waitAttachEvent(TYPEC_PORT_0_IDX)
#if PD_DUALPORT_ENABLE
						|| app_4cc_waitAttachEvent(TYPEC_PORT_1_IDX)
#endif
#if PD_TRIPPORT_ENABLE
						|| app_4cc_waitAttachEvent(TYPEC_PORT_2_IDX)
#endif
						 ) {
						/* Reload the default value */
						app_4cc_loadInitialStatus(TYPEC_PORT_0_IDX);
#if PD_DUALPORT_ENABLE
						app_4cc_loadInitialStatus(TYPEC_PORT_1_IDX);
#endif
#if PD_TRIPPORT_ENABLE
						app_4cc_loadInitialStatus(TYPEC_PORT_2_IDX);
#endif
						break;
					}
					/* 100ms one cycle */
					k_msleep(100);
				}
			}
		}
		/* Wait status update end */

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
	gpio_write_pin(EC_USB32_RD_EN, 0);
	gpio_write_pin(EC_USB32_RD_RST_N, 0);
	// gpio_write_pin(BAT_CHG1_CHG2n_SEL, 0);

	/**
	 * Initialize ACPI planes
	 */
	md_acpiplanes_init();
	ACPI_planeInit();


#if CONFIG_BATTERY_MANAGEMENT
	/* Init the smart battery and charger */
	board_smtbty_taskInit();
#endif /* CONFIG_BATTERY_MANAGEMENT */

	/* app usbc task init before 1.8V alw shut down (EC cannot access PD controller I2C in G3) */
	/* after 1.8V alw shut down EC will run the G3->S5 power sequence in app_pseq thread */
	app_usbc_task_init();

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

	/* Init FW status planes */
	app_manOs_init();

#ifndef CONFIG_ESPI_SAF
		//
		// SPI S0 sharing tester
		//
#if CONFIG_S0S_TEST
	app_spiS0_Init();
#endif

#endif

	/* Init the timer for APU reset trigger */
	k_timer_init(&g_apuRstTrigger, board_init_apuRstTrigger, NULL);
#if (CONFIG_SFH)
	/* Init SFH */
	app_sfh_init ();
#endif
	/** ****************************************
	 *
	 * Turn EC_1_8V_ALW_EN off before exit platform
	 * initialization. This power rail will set
	 * properly later by power sequence routine.
 *
	 * ****************************************/
	gpio_write_pin(EC_1V8_S5_EN, 0);

	if (F_BRAM_VCI_BOOT_SIGNATURE == f_bram_bootReason()) {
		gpio_write_pin(EC_FPR_OP_BOOT_LOGIN_N, 0);
	} else {
		gpio_write_pin(EC_FPR_OP_BOOT_LOGIN_N, 1);
	}
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

	/* this setting can keep across S0i3 */
	g_p3tConfigLimit = 0xFF;

	/* EC will not set P3T again after S0i3 */
	app_thermal_resetP3tSetting ();

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
static uint16_t _s_pwrEnRest = 0;

/**
 * board_init_apuSleepEnterHandler
 *
 * @note
 *   handler to process apu sleep mode enter
 */
void board_init_apuSleepEnterHandler (void)
{
	/**
	 * Don't touch AUX reset across S3/S0i3
	 */
	_s_pinStU5100 = dev_pca9535_smartRead(IOEXP_I2C_ADDR_U5100_I2C1_0x21, IOEXP_I2C_PORT_U5100_I2C1_0x21, 0xFFFF);
	_s_pinStU5101 = dev_pca9535_smartRead(IOEXP_I2C_ADDR_U5101_I2C3_0x22, IOEXP_I2C_PORT_U5101_I2C3_0x22, 0xFFFF);


	_s_pwrEnRest = 0;

	uint32_t need2keepWlanPwr;
	GET_NV_OPTIONS(pwr_keepWlanPwrInS3, need2keepWlanPwr);

	if (g_ui8MondernStandbySupport) {
		/* Modern standby case */
		LOG_DBG("S0i3 enter proc ... U141:%04X, U148:%04X, x8Slot:%02X", _s_pinStU5100, _s_pinStU5101, _s_pwrEnRest);

		IOEXP_BatchStart;
		/* U124 */
		IOEXP_BatchSet(ex_EP_SSD0_PWREN, 0);   /* Depend on NVMe D3Cold is enabled or not, */
		                                       /* EC shall optional turn on NVMe on S0i3 exit. */
		// IOEXP_BatchSet(ex_EP_WLAN_3V3_PWREN, 0);
		IOEXP_BatchEnd;

#if CONFIG_BATTERY_MANAGEMENT
		/* PLAT-85968 */
		board_smtbty_chgPwrSavingS0i3Enter();
#endif

	} else {
		/* S3 */
		LOG_DBG("S3 enter proc ... U141:%04X, U148:%04X, x8Slot:%02X", _s_pinStU5100, _s_pinStU5101, _s_pwrEnRest);

		if (!need2keepWlanPwr && !app_manOs_getEnFlag()) {  /* pwr_keepWlanPwrInS3, required by [CZN] PLAT-64301 */
			ioexp_set(ex_EP_WLAN_3V3_PWREN, 0);   /* IO_12 (Keep if need2keepWlanPwr or mOS_S3) */
		}
		/* U124 */
		IOEXP_BatchStart;
		/** Assume mOS only enabled in S0i3 platform **
		 * Thus S3 can only occurs for mOS sleep.
		 * Otherwise, below expression has to differentiate it is mOS_S3 vs HOST_S3 case
		 */
		IOEXP_BatchSet(ex_EP_TPM_PWREN, 0);     /* IO_0 = 0 */
		IOEXP_BatchSet(ex_EP_SSD0_PWREN, 0);	/* IO_6 = 0 */
		IOEXP_BatchEnd;

	}
}

/**
 * board_init_apuSleepExitHandler
 *
 * @note
 *   handler to process apu sleep mode exit
 */
void board_init_apuSleepExitHandler (void)
{
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
		LOG_DBG("S0i3 exit proc ... U141:%04X, U148:%04X, x8Slot:%02X, SSD3C(%d, %d)", _s_pinStU5100, _s_pinStU5101, _s_pwrEnRest, ssd0d3c, ssd1d3c);
		IOEXP_BatchStart;
		if ((!ssd0d3c) && (_s_pinStU5100 & IOEXP_BitMask(ex_EP_SSD0_PWREN))) { /* if SSD0 power was enabled */
			IOEXP_BatchSet(ex_EP_SSD0_PWREN, 1);                              /* then turn it on */
		}
		if (_s_pinStU5100 & IOEXP_BitMask(ex_EP_WLAN_3V3_PWREN)) { /* if SSD4 power was enabled */
			IOEXP_BatchSet(ex_EP_WLAN_3V3_PWREN, 1);              /* then turn it on */
		}
		IOEXP_BatchEnd;

#if CONFIG_BATTERY_MANAGEMENT
		/* PLAT-85968 */
		board_smtbty_chgPwrSavingS0i3Exit();
#endif

	} else {
		/* S3 */
		LOG_DBG("S3 exit proc ... U141:%04X, U148:%04X, x8Slot:%02X", _s_pinStU5100, _s_pinStU5101, _s_pwrEnRest);

		IOEXP_BatchStart;
		/* U148 */
		if (_s_pinStU5100 & IOEXP_BitMask(ex_EP_WLAN_3V3_PWREN)) {         /* if WLAN/SSD4 power was enabled */
			IOEXP_BatchSet(ex_EP_WLAN_3V3_PWREN, 1);                      /* then turn it on */
		}
		if (_s_pinStU5100 & IOEXP_BitMask(ex_EP_SSD0_PWREN)) {      /* if SSD0 power was enabled */
			IOEXP_BatchSet(ex_EP_SSD0_PWREN, 1);                   /* then turn it on */
		}
		if (_s_pinStU5101 & IOEXP_BitMask(ex_EP_TPM_PWREN)) {
			IOEXP_BatchSet(ex_EP_TPM_PWREN, 1);
		}
		IOEXP_BatchEnd;

	}
}

/**
 * board_init_smartMux_V0
 *
 * @note
 *   smart mux setting V0
 */
void board_init_smartMux_V0(void)
{

}

/**
 * board_init_smartMux_V1
 *
 * @note
 *   smart mux setting V1
 */
void board_init_smartMux_V1(void)
{

}

/**
 * board_init_smartMux_V2
 *
 * @note
 *   smart mux setting V2
 */
void board_init_smartMux_V2(void)
{

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
	g_updateinfo = true;
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

	/* Check if update is requested */
	if (g_updateinfo == false) {
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
	ec_version_stringGet(3, romVersion, 12);

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