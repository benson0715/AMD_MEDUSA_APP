/*****************************************************************************
 * Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
 ******************************************************************************
 */

#include "ioexp_hub.h"
#include "u_rrq.h"
#if (CONFIG_USBC_RTK)
#include "app_realtek_pd.h"
#endif
#include "amd_crb_drivers_pd.h"
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
#include <zephyr/drivers/bbram.h>
#include "dbgLogFifo.h"
#include "amd_crb_drivers_spiFlash.h"
#include "f_bram.h"

LOG_MODULE_REGISTER(brdint, CONFIG_BOARD_INIT_LOG_LEVEL);

#define BOARD_LED_CYCLE_TIME_MS (250)

/* ************************** *
 *      external valuable     *
 * ************************** */
extern uint8_t g_u8Pd0Slot;
extern uint8_t g_u8Pd1Slot;

/* ************************** *
 *      static valuable       *
 * ************************** */
static bool _s_forcePdUpdate = 0;
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
/* MPC update info flag */
static bool g_updateinfo = false;

/* ************************** *
 *       global variables     *
 * ************************** */
/* KBC reset trigger flag */
uint8_t g_triggerKbcRst = 0;
/* system ac limit value */
uint16_t g_sysAcLimit1 = 3000;  /* default 3A */
/* For runtime FW status */
uint8_t g_apuRstS3Flag = 0;
/* detect rtk rom boot */
bool g_rtk_romBootMode = false;

/**
 * board_init_apuRstTimerEnable
 *
 * @param [in] ms: delay time in milliseconds
 *
 * @note
 *  enable the timer after apu reset to delay the correspending handler by xxms
 */
void board_init_apuRstTimerEnable (uint32_t ms)
{
	k_timer_start(&g_apuRstTrigger, K_MSEC(ms), K_NO_WAIT);
}

/**
 * board_init_apuRstTrigger
 *
 * @param [in] timer: pointer to the timer structure
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
 * @param [in] timer: pointer to the timer structure
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
 * @param [in] val: 0: always turn off
 *                  1: always turn on
 *                  2 ~ 255: blinking speed
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

}

/**
 * board_init_platformInit
 *
 * @note
 *   board platform init
 */
void board_init_platformInit(void)
{
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
	extern uint8_t g_S5AlwEnFlag;
	GET_NV_OPTIONS(s5_alw_enable, g_S5AlwEnFlag);
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

	/* Sync up critical NV options */
	/* Below two are used everywhere of thread init proc, sync the stauts here to ensure they are up to date */
	GET_NV_OPTIONS(chg_DcOnlyProchot, g_isThrottleApuInDcOnly);
	GET_NV_OPTIONS(chg_AcOnlyProchot, g_isThrottleApuInAcOnly);

	/* Initialize rom sig */
	f_romSig_init();

	/**
	 * Set the selected slot ID to 0xFF and it will be updated later depend on board config.
	 */
	g_u8Pd0Slot = g_u8Pd1Slot = 0xFF;

	board_readDispSwSt();
	/**
	 * MDS Aeris eFFP format
	 *
	 * Slot#  FW_NAME       Offset        Size
	 *   -    EC SIG        0x00000000    0x00001000
	 *   -    PD TI 0       0x00001000    0x00008000  NA
	 *   -    PD TI 1       0x00009000    0x00008000  NA
	 *   -    PD TI 2       0x00011000    0x00008000  NA
	 *        **hole**      0x00019000    0x00007000  Hole
	 *   -    PSP SIG       0x00020000    0x00001000  PSP
	 *  #0    PD RTK 0      0x00021000    0x00020000  SKU1: FP10 silicon, RTS5453P-VB, USB4,   Supp PS8835,   Polling/Interrupt mode.
	 *  #1    PD RTK 1      0x00041000    0x00020000  SKU2: FP10 silicon, RTS5453P-VB, USB4,   Supp PS8835,   Polling/Interrupt mode.
	 *   -    EC FW         0x00061000    0x00060000
	 *   ~    BIOS others   0x000C1000    ....
	 */

	/* eFFP uses TI PD and no need to detect */
	app_usbc_writeDevID(PD_DEVICE_ID_RTKx);

	if (app_rtk_vonderIdentify()) {
		/* detect normal boot or rom boot */
	} else {
		/* cannot detect I2C address for port0 and port2. Need to update with Port1 and Port3 */
		LOG_ERR("rtk rom mode");
		g_rtk_romBootMode = true;
		app_rtk_vonderIdentify();
	}

#if CONFIG_SOC_SERIES_NPCX4
	amd_crb_drivers_spi_reset();
#endif

	/* Load the default status from option */
	app_usbc_status.ui8Reg = 0;
	GET_NV_OPTIONS(usbc_status, app_usbc_status.ui8Reg);

#if (CONFIG_USBC_RTK)
	/* init the realtek PD and check if it needs to update FW */
	if (CHECK_CFG(PD0, RTK) || CHECK_CFG(PD1, RTK)) {
		app_usbc_writeDevID(PD_DEVICE_ID_RTKx);
		if (f_romSig_isValid(3) && f_romSig_isValid(4)) {  /* 3 and 4 */
			board_status_led_set(4);
			/* Update PD0 Dual-ports */
			app_rtk_init(f_romSig_getOffset(3), 0x20000, 0);
			/* Update PD1 Single-port */
			app_rtk_init(f_romSig_getOffset(4), 0x20000, 1);
			board_status_led_set(0);
		}
	}
	/* revert the rom boot flag after update PDFW */
	g_rtk_romBootMode = false;
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

	/* this setting can keep across S0i3 */
	g_p3tConfigLimit = 0xFF;

	/* EC will not set P3T again after S0i3 */
	app_thermal_resetP3tSetting ();
}

/** ***************************************************
 * AUX# reset and PwrEn may have to be restored on Sleep Exit.
 * Below two static variables are shared between SleepEnter
 * and SleepExit so as to restore the status.
 * ***************************************************/
static uint16_t _s_pinSt = 0;  /* status of pins */
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
	_s_pinSt = gpio_read_pin(EC_SSD0_PWREN);

	_s_pwrEnRest = 0;

	uint32_t need2keepWlanPwr;
	GET_NV_OPTIONS(pwr_keepWlanPwrInS3, need2keepWlanPwr);

	if (g_ui8MondernStandbySupport) {
		/* Modern standby case */
		LOG_DBG("S0i3 enter proc ... pin:%04X, x8Slot:%02X", _s_pinSt, _s_pwrEnRest);

		gpio_write_pin(EC_SSD0_PWREN, 0);   /* Depend on NVMe D3Cold is enabled or not, */
		                                       /* EC shall optional turn on NVMe on S0i3 exit. */

#if CONFIG_BATTERY_MANAGEMENT
		/* PLAT-85968 */
		board_smtbty_chgPwrSavingS0i3Enter();
#endif

	} else {
		/* skip S3 */
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
	if (g_ui8MondernStandbySupport) {
		uint32_t ssd0d3c;

		GET_NV_OPTIONS(pwr_SSD0D3Cold, ssd0d3c);
		/* Modern standby case */
		LOG_DBG("S0i3 exit proc ... pin:%04X, x8Slot:%02X, SSD3C(%d)", _s_pinSt, _s_pwrEnRest, ssd0d3c);
		if ((!ssd0d3c) && (_s_pinSt & 0x01)) { /* if SSD0 power was enabled */
			gpio_write_pin(EC_SSD0_PWREN, 1);         /* then turn it on */
		}

#if CONFIG_BATTERY_MANAGEMENT
		/* PLAT-85968 */
		board_smtbty_chgPwrSavingS0i3Exit();
#endif

	} else {
		/* skip S3 */
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