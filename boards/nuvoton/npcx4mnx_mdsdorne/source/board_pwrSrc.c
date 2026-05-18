/*****************************************************************************
 * Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
 ******************************************************************************
 */

#include "board_pwrSrc.h"
#include "board_smtbty.h"
#include "board_init.h"
#include "app_4cc.h"
#include "dev_isl9241.h"
#include "app_ucsi_tunnel.h"
#include "app_ccgx.h"
#include "app_usbc_task.h"
#include "board_id.h"
#include "app_pseq.h"
#include "board_config.h"
#include <zephyr/logging/log.h>
#include "app_acpi.h"
#include "app_usbc_task.h"
#include "app_manOs.h"
#include "periphmgmt.h"
#include "f_nv_options.h"
#include "task_handler.h"
LOG_MODULE_REGISTER(pwrSrc, CONFIG_POWER_SOURCE_LOG_LEVEL);

/**************************** *
 *          Macro             *
 * ************************** */
#define BOARD_PWRSRC_PD_DEFAULT_CURRENT 10000   /* 5000mA */
#define BOARD_PWRSRC_PD_DEFAULT_VOLTAGE 20000  /* 20000mV */
#define BOARD_CHAEGER_SENSE_RATIO_2_1_DEAFULT  (0u)    // 0 = 2:1 (default)
#define BOARD_CHAEGER_SENSE_RATIO_1_1          (1u)    // 1 = 1:1

static bool g_SenseRatio = BOARD_CHAEGER_SENSE_RATIO_1_1;

/** ************************* *
 *     static valuable        *
 * ************************** */
static struct {
	uint32_t CurLimit;           // mA
	uint32_t VadpVoltage;        // mV
} _s_pwrCap[2];
static BOARD_PWR_SRC_TYPE _s_emMainPwrSource = BOARD_PWR_BTY;
/* task slp ready */
static uint8_t *task_pwrsrcSlpReady = NULL;
/* timer delay process for power source event triggered */
static struct k_timer g_pwrSrcDelay;
extern uint8_t *task_smtbtySlpReady;

/** ************************* *
 *     global valuable        *
 * ************************** */
bool g_PdAdapterConnected = false;         /* debounced PD_SOURCE_ONn and PDO read from PD chip */
bool g_ChgAcOK = false;                    /* debounced CHG_ACOK */
bool g_pd_bootup_flag = false;
bool g_pd_autoSinkEnable = false;
bool g_pwrSrc_noPdBattery = false;
#if PD_DC_HOTPLUG_PROCHOT
static void _pdDcHotplugProchot_TimerCallback(struct k_timer *timer);
K_TIMER_DEFINE(_s_pdDcHotplugProchotTid, _pdDcHotplugProchot_TimerCallback, NULL);
#endif  /* PD_DC_HOTPLUG_PROCHOT */
#if PD_AC_HOTPLUG_PROCHOT
static void _pdAcHotplugProchot_TimerCallback(struct k_timer *timer);
K_TIMER_DEFINE(_s_pdAcHotplugProchotTid, _pdAcHotplugProchot_TimerCallback, NULL);
#endif  /* PD_AC_HOTPLUG_PROCHOT */

/* register the sem to give and take */
K_SEM_DEFINE(_s_statusChangeSem, 0, 1);
K_SEM_DEFINE(_s_statusChangeDelaySem, 0, 1);

#if PD_DC_HOTPLUG_PROCHOT
/* timer callback for DC prochot */
static void _pdDcHotplugProchot_TimerCallback(struct k_timer *timer)
{
	gpio_write_pin(EC_APU_PROCHOT_N, 1); /* de-assert APU_PROCHOT after timer fired. */
}
#endif  /* PD_DC_HOTPLUG_PROCHOT */
#if PD_AC_HOTPLUG_PROCHOT
/* timer callback for DC prochot */
static void _pdAcHotplugProchot_TimerCallback(struct k_timer *timer)
{
	gpio_write_pin(EC_APU_PROCHOT_N, 1); /* de-assert APU_PROCHOT after timer fired. */
}
#endif  /* PD_AC_HOTPLUG_PROCHOT */

/**
 * @brief clear PD capability data
 */
void board_pwrSrc_clearPdCap (void)
{
	int key = irq_lock();
	_s_pwrCap[PD_SINK].CurLimit = 0;
	_s_pwrCap[PD_SINK].VadpVoltage = 0;
	irq_unlock(key);
}

/**
 * @brief if AC or PD is the main power source
 *
 * @retval True if adapter connected.
 */
bool board_pwrSrc_isAdapterConnected (void)
{
	return g_PdAdapterConnected;
}

/**
 * @brief if AC is the main power source
 *
 * @retval True if AC main power source
 */
bool board_pwrSrc_hasAcPowerSource(void)
{
	/* Return true means at lease one of AC Brick or PD adapter had connected */
	return g_ChgAcOK;
}

/**
 * @brief return the charger AC limit
 *
 * @retval AC limit
 */
uint32_t board_pwrSrc_sysAcLimit(void)
{
	return _s_pwrCap[PD_SINK].CurLimit;
}

/**
 * @brief return the charger PD current limit
 *
 * @retval PD current limit
 */
uint32_t board_pwrSrc_PdCurLimit(void)
{
	return _s_pwrCap[PD_SINK].CurLimit;
}

/**
 * @brief return the system max power supply capability
 *
 * @retval max power supply capability
 */
uint32_t board_pwrSrc_sysPwrMaxWatt(void)
{
	uint32_t ret = 0;

	int key = irq_lock();
	ret = _s_pwrCap[PD_SINK].CurLimit * _s_pwrCap[PD_SINK].VadpVoltage;
	irq_unlock(key);

	return ret;
}

/**
 * @brief board power source change event (for example AC plugin battery unplug)
 */
void board_pwrSrcEvent(void)
{
	/* sleep is not allowed after give sem */
	task_status_set(task_pwrsrcSlpReady, TASK_STASTUS_SLEEP_READY, 0);
	task_status_set(task_smtbtySlpReady, TASK_STASTUS_SLEEP_READY, 0);

	k_sem_give(&_s_statusChangeSem);

}


/**
 * @brief board power source init during EC power on
 */
void board_pwrSrc_powerOnReset(void)
{
	/** *** GPIOs to variables mapping ***
	 * INPUT to EC:
	 *   BAT_IN#             -> g_batPresentFlag
	 *   CHG_ACOK            -> g_ChgAcOK
	 *   AC_POWER_PRSNT_ECn  -> g_AcAdapterConnected
	 *   (PDO data)          -> g_PdAdapterConnected
	 *   PD_SOURCE_ON#       -> in Cypress case, PD_SOURCE_ON# can be asserted only after EC turns on one of PD port.
	 *                          in TI case, this pin can reflact the status of PD power sink source as PD will turn on PD port by itself
	 *
	 */

	/* check power sources */
    if (g_SenseRatio) {
    	dev_isl9241_regRMW(DEV_CHARGER_CURRENT_PORT, DEV_ISL9241_REG_Control4, cbit(11), cbit(11));
    }

	/* PSYS enable */
	//dev_isl9241_regRMW(DEV_CHARGER_CURRENT_PORT, DEV_ISL9241_REG_Control1, cbit(3), cbit(3));

	/* 01. battery */
	g_batPresentFlag = false;
	if (!gpio_read_pin(BAT_PRSNT_N)) { /* in case BAT_PRSNT_N shows the battery is here */
		uint16_t tmp;               /* read by I2C for double confirm */
		if (dev_smtbty_i32Read(0, DEV_SMTBTY_REG_ChargingVoltage, &tmp) ||
			dev_smtbty_i32Read(0, DEV_SMTBTY_REG_ChargingCurrent, &tmp)) {

			g_batPresentFlag = true;
			_s_emMainPwrSource = BOARD_PWR_BTY;
		}
	}
	/* load CHG_ACOK */
	g_ChgAcOK = gpio_read_pin(CHG_ACOK);
	
	/* 03. PD adapter */
	if (!gpio_read_pin(PD_PRSNT_N)) {

		pd_do_t pdo = { .val = app_usbc_getAcLimitOfHighestPdPort() };
		if ( pdo.fixed_src.voltage > 240 &&             // source pdo > 12V (i.e 240 * 50 mV)
				 pdo.fixed_src.max_current) {               // current is not zero

			_s_pwrCap[PD_SINK].CurLimit    = pdo.fixed_src.max_current * 10;
			_s_pwrCap[PD_SINK].VadpVoltage = pdo.fixed_src.voltage * 50;

			g_PdAdapterConnected = true;
			_s_emMainPwrSource = BOARD_PWR_PD;
		} else {
			/* Copy the default value */
			_s_pwrCap[PD_SINK].CurLimit    = BOARD_PWRSRC_PD_DEFAULT_CURRENT;
			_s_pwrCap[PD_SINK].VadpVoltage = BOARD_PWRSRC_PD_DEFAULT_VOLTAGE;

			LOG_DBG("[ERROR] It seems the PD is the only source but PDO read failed!");
			LOG_DBG("[WARNING] Loading PDO default Cur %d, Vol %d", _s_pwrCap[PD_SINK].CurLimit, _s_pwrCap[PD_SINK].VadpVoltage);

			LOG_DBG("[WARNING] Force g_PdAdapterConnected to true");
			g_PdAdapterConnected = true;
		}
	} else {
		board_pwrSrc_clearPdCap();
		g_PdAdapterConnected = false;
	}

	/*
	 * In case both AC and BAT are not present, the PD must be the power source.
	 */
	if (!g_batPresentFlag) {

		g_pd_bootup_flag = true;     /* Set to true so USBC thread will check PD capability and
		                                lock low-cap if there's only 5V on VBUS */

		if (!g_PdAdapterConnected) {
			/* Copy the default value */
			_s_pwrCap[PD_SINK].CurLimit    = BOARD_PWRSRC_PD_DEFAULT_CURRENT;
			_s_pwrCap[PD_SINK].VadpVoltage = BOARD_PWRSRC_PD_DEFAULT_VOLTAGE;

			LOG_DBG("[ERROR] It seems the PD is the only source but PDO read failed!");
			LOG_DBG("[WARNING] Loading PDO default Cur %d, Vol %d", _s_pwrCap[PD_SINK].CurLimit, _s_pwrCap[PD_SINK].VadpVoltage);

			LOG_DBG("[WARNING] Force g_PdAdapterConnected to true");
			g_PdAdapterConnected = true;
			_s_emMainPwrSource = BOARD_PWR_PD;
			board_smtbty_setAcLimit(DEV_CHARGER_CURRENT_PORT, board_pwrSrc_PdCurLimit(), board_pwrSrc_PdCurLimit());
			/* lock the AcLimit update for test mode:
			 * There is no battery and no PD adapter for eFFP (no AC jacker)
			 * It must be some reworked power source and we need to set the Aclimit to 10A
			 * It also need to lock further AcLimit update in case someone plugin PD source */
			g_pwrSrc_noPdBattery = true;
		}
	} else {
		uint16_t tmp;
		dev_smtbty_i32Read(0, DEV_SMTBTY_REG_RemainingCapacity, &tmp);
		if (tmp > 0x05) {
			app_4cc_deadbatteryClear(TYPEC_PORT_0_IDX);
			/* Enable the multi-port sink policy */
			amd_crb_drivers_tps_autoSink(0, true);
			/* disable all the sink control */
			amd_crb_drivers_tps_lockSinkPath(TYPEC_PORT_0_IDX, false);
			amd_crb_drivers_tps_lockSinkPath(TYPEC_PORT_1_IDX, false);
			g_pd_autoSinkEnable = true;
		}
	}

	/*
	 * Init charger settings
	 */
	bool chgAck;
	uint16_t u16tmp;
	dev_isl9241_chgDisable(DEV_CHARGER_CURRENT_PORT);

	if (g_batPresentFlag)
		/* By default set dual charger MaxSystemVoltage to 13200mV to allow power in. */
		u16tmp = APP_BATTERY_FULL_CHARGE_VOLTAGE;
	else
		/* By default set dual charger MaxSystemVoltage to 12000mV to allow power in. (no battery) */
		u16tmp = APP_BATTERY_NO_BATT_CHARGE_VOLTAGE;

	chgAck = dev_isl9241_regRMW(DEV_CHARGER_CURRENT_PORT, DEV_CHARGER_REG_ChargerChargingVoltage, u16tmp, 0xFFFF);
	if (!chgAck) {
		LOG_DBG("[ERROR] Fail to write MaxSystemVoltage to 0.008V!");
	}

	/* DEV_SMTBTY_REG_ChargingCurrent = 3A (RAW 1500mA) */
	u16tmp = 1500;
	chgAck = dev_isl9241_regRMW(DEV_CHARGER_CURRENT_PORT, DEV_CHARGER_REG_ChargerChargingCurrent, u16tmp, 0xFFFF);
	if (!chgAck) {
		LOG_DBG("[ERROR] Fail to write ChargingCurrent to 3A!");
	}

	chgAck = dev_isl9241_RAA489110_ACOKReference(DEV_CHARGER_CURRENT_PORT, ISL9241_VAL_ACOKRef_08064);
	if (!chgAck) {
		LOG_DBG("[ERROR] Fail to write ACOKReference to 8064mV!");
	}

	chgAck = dev_isl9241_RAA489110_VIN_Voltage(DEV_CHARGER_CURRENT_PORT, ISL9241_VAL_VinVoltage_8192);
	if (!chgAck) {
		LOG_DBG("[ERROR] Fail to write VinVoltage to 8096mV!");
	}

	LOG_DBG("        Main Source: %s", (_s_emMainPwrSource == BOARD_PWR_PD) ? "PD Adapter" : ((_s_emMainPwrSource == BOARD_PWR_BTY) ? "Battery" : "unknown"));
	LOG_DBG(" Sys totoal Cur Lmt: %d", board_pwrSrc_sysAcLimit());
	LOG_DBG("     PD-apd Cur Lmt: %d", board_pwrSrc_PdCurLimit());
	LOG_DBG("     PD Vin voltage: %d", _s_pwrCap[PD_SINK].VadpVoltage);
	LOG_DBG("      Sys Power Max: %d", board_pwrSrc_sysPwrMaxWatt());
	LOG_DBG(" PD_PWR = %d; BATY = %d; ACOK = %d", g_PdAdapterConnected, g_batPresentFlag, g_ChgAcOK);
}

/**
 * @brief return current power main source
 *
 * @retval power main source
 */
BOARD_PWR_SRC_TYPE board_pwrSrc_getCurrentPowerSource (void)
{
	return _s_emMainPwrSource;
}

/**
 * @brief return power source related GPIO status
 *
 * @retval GPIO status
 */
static uint32_t _pwrSrc_stateSnapshot(void)
{
	uint32_t xor = 0x5555;
	for (uint8_t i = 0; i < sizeof(_s_pwrCap) / 2; i++) {
		xor ^= ((uint16_t *)_s_pwrCap)[i];
	}

	uint32_t helper = (xor << 16)                             |
		                (_s_emMainPwrSource & 0x0F)             |
										(g_batPresentFlag         ? 0x0010 : 0) |
										(g_ChgAcOK                ? 0x0020 : 0) |
										(g_PdAdapterConnected     ? 0x0080 : 0);

	return helper;
}

/**
 * @brief board power source status monitor
 */
static void board_pwrSrc_stateMonitor (void)
{
	uint8_t AcDcSwitchEnableFlag;
	static uint32_t lastSnapshot = 0;
#if PD_AC_HOTPLUG_PROCHOT
	BOARD_PWR_SRC_TYPE oldMainPwrSource = _s_emMainPwrSource;
#endif /* PD_AC_HOTPLUG_PROCHOT */
#if (PD_DC_HOTPLUG_PROCHOT || PD_AC_HOTPLUG_PROCHOT)
	uint8_t inS0 = 0;
	if (!SLP_S5_ASSERT && !SLP_S3_ASSERT) {
		inS0 = 1;
	}
#endif /* PD_DC_HOTPLUG_PROCHOT or PD_AC_HOTPLUG_PROCHOT */

	/* *** GPIOs to variables mapping ***
	 * INPUT to EC:
	 *   BAT_IN#             -> g_batPresentFlag
	 *   CHG_ACOK            -> g_ChgAcOK
	 *   AC_POWER_PRSNT_ECn  -> g_AcAdapterConnected
	 *   (PDO data)          -> g_PdAdapterConnected
	 *   PD_SOURCE_ON#       -> in Cypress case, PD_SOURCE_ON# can be asserted only after EC turns on one of PD port.
	 *                          in TI case, this pin can reflact the status of PD power sink source as PD will turn on PD port by itself
	 *
	 * OUTPUT from EC:
	 *   EC_AC_PD# (POR IN+200K_Pd) -> O_0 to allow the main rail of PD power; O_1 to gate off the main rail
	 */
	GET_NV_OPTIONS(chg_AcDcSwitch, AcDcSwitchEnableFlag);

	LOG_DBG("  ++++ board_pwrSrc_stateMonitor loop start");
	LOG_DBG("       PD_PWR = %d; BATY = %d; ACOK = %d", g_PdAdapterConnected, g_batPresentFlag, g_ChgAcOK);
	LOG_DBG("       Charging = %d", dev_isl9241_isChgEn(DEV_CHARGER_CURRENT_PORT));
	LOG_DBG("       Status Snapshot = %08X", lastSnapshot);

	/* Disable both charging before additional operation */
	board_smtbty_ChgDisable();

	if (g_PdAdapterConnected) {
		pd_do_t pdo = { .val = app_usbc_getAcLimitOfHighestPdPort() };
		if ( pdo.fixed_src.voltage && pdo.fixed_src.max_current) {

		int key = irq_lock();
		_s_pwrCap[PD_SINK].CurLimit = pdo.fixed_src.max_current * 10;
		_s_pwrCap[PD_SINK].VadpVoltage = pdo.fixed_src.voltage * 50;
		irq_unlock(key);

		} else {
			g_PdAdapterConnected = false;
			board_pwrSrc_clearPdCap();
		}
	} else {
		board_pwrSrc_clearPdCap();
	}


	/* Identify the power source - start */
	if (g_batPresentFlag && !g_ChgAcOK) {
		_s_emMainPwrSource = BOARD_PWR_BTY;
		board_pwrSrc_clearPdCap();
	} else if (g_ChgAcOK) {
		if (g_PdAdapterConnected) {
			_s_emMainPwrSource = BOARD_PWR_PD;
#if PD_DC_HOTPLUG_PROCHOT
			if ((_s_pwrCap[PD_SINK].CurLimit <= 4250) &&
			    (_s_pwrCap[PD_SINK].VadpVoltage <= 20000) &&
				(g_batPresentFlag == true) &&
				(inS0 == 1)) {
				gpio_write_pin(EC_APU_PROCHOT_N, 0);
				k_timer_start(&_s_pdDcHotplugProchotTid, K_MSEC(2000), K_NO_WAIT);
			} else {
				gpio_write_pin(EC_APU_PROCHOT_N, 1);
			}
#endif /* PD_DC_HOTPLUG_PROCHOT */
		} else {

		}
	} else if (AcDcSwitchEnableFlag) {  // (No battery && Ac/Dc Timer EN and !g_ChgAcOK)
		LOG_DBG("Fake DC case ...");
		_s_emMainPwrSource = BOARD_PWR_PD;
	} else {
		LOG_DBG("[ERROR] !!!Unable to detect power source !!! set as battery only");
		_s_emMainPwrSource = BOARD_PWR_BTY;

		board_pwrSrc_clearPdCap();
	}

	/* Identify the power source - end */
	uint32_t tmp = _pwrSrc_stateSnapshot();
	if (lastSnapshot != tmp) { // in case any status had changed
		LOG_DBG("**** Snapshot changed = %08X -> %08X", lastSnapshot, tmp);
		lastSnapshot = tmp; // update the snapshot and notify itself
		k_timer_start(&g_pwrSrcDelay, K_MSEC(1200), K_NO_WAIT); /* 1200ms */
	}
}

/**
 * @brief timer callback to give sem to power source event
 *
 * @param timer pointer to the timer structure
 */
static void board_pwrSrc_delayHandler(struct k_timer *timer)
{
	k_sem_give(&_s_statusChangeDelaySem);
}

/**
 * @brief handle the power source event
 */
static void board_pwrSrc_delayCallback(void)
{
	LOG_DBG("  ---------------------");
	LOG_DBG("        Main Source: %s", (_s_emMainPwrSource == BOARD_PWR_PD) ? "PD Adapter" : ((_s_emMainPwrSource == BOARD_PWR_BTY) ? "Battery" : "AC and PD"));
	LOG_DBG(" Sys totoal Cur Lmt: %d", board_pwrSrc_sysAcLimit());
	LOG_DBG("     PD-apd Cur Lmt: %d mA", board_pwrSrc_PdCurLimit());
	LOG_DBG("     PD Vin voltage: %d mV", _s_pwrCap[PD_SINK].VadpVoltage);
	LOG_DBG("      Sys Power Max: %d mW", board_pwrSrc_sysPwrMaxWatt());
	LOG_DBG(" PD_PWR = %d; BATY = %d; ACOK = %d", g_PdAdapterConnected, g_batPresentFlag, g_ChgAcOK);
	LOG_DBG("  ---- board_pwrSrc_stateMonitor loop end");

	if (BOARD_PWR_BTY == _s_emMainPwrSource) {
		bool chgAck = 0;
		/* MaxSystemVoltage = 0.008V */
		dev_isl9241_regRMW(DEV_CHARGER_CURRENT_PORT, DEV_CHARGER_REG_ChargerChargingVoltage, 8, 0xFFFF);
		if (!chgAck) {
			LOG_DBG("[ERROR] Fail to write Chg0 MaxSystemVoltage to 0.008V!");
		}

		/* DEV_SMTBTY_REG_ChargingCurrent = 3A (RAW 1500mA) */
		chgAck = dev_isl9241_regRMW(DEV_CHARGER_CURRENT_PORT, DEV_CHARGER_REG_ChargerChargingCurrent, 1500, 0xFFFF);
		if (!chgAck) {
			LOG_DBG("[ERROR] Fail to write Chg0 ChargingCurrent to 3A!");
		}

		chgAck = dev_isl9241_setBgate(DEV_CHARGER_CURRENT_PORT, 1);
		if (!chgAck) {
			LOG_DBG("[ERROR] Fail to turn on Chg0 BGATE");
		}

		if (g_isThrottleApuInDcOnly) {
			/* do nothing */
		} else { /* Charger power saving */
			dev_isl9241_battery_only(DEV_CHARGER_CURRENT_PORT, true);
		}
	} else {
		dev_isl9241_battery_only(DEV_CHARGER_CURRENT_PORT, false);
		if (g_batPresentFlag)
			dev_isl9241_regRMW(DEV_CHARGER_CURRENT_PORT, DEV_CHARGER_REG_ChargerChargingVoltage, APP_BATTERY_FULL_CHARGE_VOLTAGE, 0xFFFF);
		else
			dev_isl9241_regRMW(DEV_CHARGER_CURRENT_PORT, DEV_CHARGER_REG_ChargerChargingVoltage, APP_BATTERY_NO_BATT_CHARGE_VOLTAGE, 0xFFFF);
		dev_isl9241_setBgate(DEV_CHARGER_CURRENT_PORT, 1);
	}
	/**
	 * Hook of the app_manOs trigger for power source changing.
	 */
	app_manOs_PwrSourceChange();

	/**
	 * Setup PROCHOT# gate on power source changing
	 * The IOEXP only works in S5/S3/S0
	 */
	if (app_pseq_systemState() >= SYSTEM_S0_STATE) {
		dev_isl9241_setProchot();
	} 
}

/**
 * @brief board power source handler main thread
 *
 * @param p1 thread input 1
 * @param p2 thread input 2
 * @param p3 thread input 3
 */
void board_pwrSrc_thread(void *p1, void *p2, void *p3)
{
	task_pwrsrcSlpReady = (uint8_t*) p2;
	task_status_set(task_pwrsrcSlpReady, TASK_STASTUS_SLEEP_READY, 1);

	static bool _s_hasBty = false;
	_s_hasBty = board_battery_isPresent(0);

	k_timer_init(&g_pwrSrcDelay, board_pwrSrc_delayHandler, NULL);
	
	while(1) {
		/**
		* wait sem for available jobs
		*/
		k_sem_take(&_s_statusChangeSem, K_FOREVER);

		/* Power source state machine and monitor */
		board_pwrSrc_stateMonitor();

		/* don't sleep if AC and DC present */
		if (board_pwrSrc_hasAcPowerSource() && board_battery_isPresent(0)) {
			/* do nothing */
		} else {
			/* if bty status changed, not sleep */
			if (_s_hasBty != board_battery_isPresent(0)) {
				_s_hasBty = board_battery_isPresent(0);
			} else {
				task_status_set(task_pwrsrcSlpReady, TASK_STASTUS_SLEEP_READY, 1);
			}
		}
	}
}

/**
 * @brief board power source delay callback handler thread
 *
 * @param p1 thread input 1
 * @param p2 thread input 2
 * @param p3 thread input 3
 */
void board_pwrSrc_thread_delay_callback(void *p1, void *p2, void *p3)
{
	while(1) {
		/**
		* wait sem for available jobs
		*/
		k_sem_take(&_s_statusChangeDelaySem, K_FOREVER);

		board_pwrSrc_delayCallback();
	}
}

/**
 * @brief firmware status handler for power source
 *
 * @param isRead read or write operation
 * @param ui8Idx index of the data
 * @param pui8Data pointer to the data
 * @retval 1 if read operation, 0 otherwise
 *
 * 0/1 : PwrSrc: AC/PD/BY
 *
 * 4:    g_AcAdapterConnected
 * 5:    g_PdAdapterConnected
 * 6:    g_ChgAcOK
 *
 * 10/11/12/13: _s_u32SysAcLimit     (little-endian)
 * 14/15/16/17: _s_u32SysVadpVoltage (little-endian)
 * 18/19/1A/1B: _s_u32SysVadpVoltage (little-endian)
 */
 uint8_t board_pwrSrc_fwStHandler (uint8_t isRead, uint8_t ui8Idx, uint8_t * pui8Data)
 {
	 if (ui8Idx >= MD_ACPI_HYPERPLANE_SELECTOR_OFFSET)
	 	return 0;

 	static uint8_t * _s_pStr[] = {
 			"AC",
 			"PD",
			"BY",    // Battery
			"DL"     // Dual source
 		};

 	if (isRead) {
 		switch (ui8Idx) {
 			case 0x00: case 0x01:
 				*pui8Data = _s_pStr[(uint32_t)_s_emMainPwrSource][ui8Idx];
 				break;
 			case 0x04:
 				*pui8Data = 0;
 				break;
 			case 0x05:
 				*pui8Data = g_PdAdapterConnected;
 				break;
 			case 0x06:
 				*pui8Data = g_ChgAcOK;
 				break;
 			case 0x10: case 0x11: case 0x12: case 0x13:
				*pui8Data = 0u;
				break;
			case 0x14: case 0x15: case 0x16: case 0x17:
				*pui8Data = 0u;
				break;
			case 0x18: case 0x19: case 0x1A: case 0x1B:
				*pui8Data = *(((uint8_t *)&_s_pwrCap[PD_SINK].CurLimit) + (ui8Idx - 0x18));
				break;
			case 0x1C: case 0x1D: case 0x1E: case 0x1F:
				*pui8Data = *(((uint8_t *)&_s_pwrCap[PD_SINK].VadpVoltage) + (ui8Idx - 0x1C));
				break;
			default:
				break;
 		}

 		return 1;
 	}

 	return 0;
 }

/**
 * @brief set ACOK reference before entering vci
 */
void board_pwrSrc_setACOKReference_vci(void)
{
	// Two thresholds affect ACOK. The higher of the two is in control of ACOK.
	//    One threshold is fixed at 3.6V rising and
	//    Another one programmable threshold using the DAC (ACOK Ref register 0x40).
	//
	// ---------------------------------------------------------------------------
	// if ACOK_Ref is MAX, CHG_ACOK can never assert after it enters VCI mode.
	// Change it back to default "0" to disable the DAC threshold. As thus the
	// other ACOK Ref (fixed at 3.6V) will take effect
	dev_isl9241_RAA489110_ACOKReference(DEV_CHARGER_CURRENT_PORT, 0);
}