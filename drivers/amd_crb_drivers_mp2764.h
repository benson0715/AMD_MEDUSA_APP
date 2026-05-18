/*****************************************************************************
 * Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
 ******************************************************************************
 */

#ifndef __AMD_CRB_DRIVERS_MP2764_H__
#define __AMD_CRB_DRIVERS_MP2764_H__

#include <stdint.h>
#include <stdbool.h>
#include <zephyr/device.h>
#include "board_config.h"

#ifndef MAX_CHARGER_SUPPORTED
#define MAX_CHARGER_SUPPORTED  1
#endif

#ifndef DEV_CHARGER_SWITCH_TO_PORT
#define DEV_CHARGER_SWITCH_TO_PORT(port)  while(0){} /* do nothing by default */
#endif

#define DEV_CHARGER_REG_INVALID  (0x4F)              /* exclusive with the registers defined for battery or charger */

#ifndef DEV_CHARGER_ID_TO_PORT
#define DEV_CHARGER_ID_TO_PORT(chgId)    (0)     /* Always map to port 0 by default */
#endif

typedef enum {
    DEV_CHARGER_REG_ChargerChargingCurrent              = 0x14, // master device sends the desired charging rate (mA).
    DEV_CHARGER_REG_ChargerChargingVoltage              = 0x15, // master device sends the desired charging voltage
                                                                // Or MaxSystemVoltage (Vsys) in NVDC mode
    /*------------------------------------------*/

} DEV_CHARGER_REG;

typedef struct {
    uint8_t slaveAddr;
    uint32_t continuousErrCount;
    uint32_t maxContinuousErrCount;
} DEV_CHARGER_I2C_INTERFACE_ERROR_COUNT;
/* charger register ratio based on the sensing resistor */
#define DEV_CHARGER_RATIO_APP_1_REG_1     (0)   /* 1:1 */
#define DEV_CHARGER_RATIO_APP_1_REG_0P5   (1)   /* 1:2 */
#define DEV_CHARGER_RATIO_APP_0P5_REG_1   (2)   /* 2:1 */
#define DEV_CHARGER_RATIO_APP_1_REG_0P25  (3)   /* 1:4 */
/* changer register read/write permission */
#define DEV_CHARGER_REG_DISABLED          (0)
#define DEV_CHARGER_REG_READ_ONLY         (1)
#define DEV_CHARGER_REG_WRITE_ONLY        (2)
#define DEV_CHARGER_REG_READ_WRITE        (3)
/* charger register data settings */
typedef struct {
    uint8_t      width;
    union {
        uint32_t u32;
        uint16_t u16[2];
        uint8_t  u8[4];
        uint8_t* pBuf;
    } value;

    union {
        uint32_t u32;
        uint16_t u16[2];
        uint8_t  u8[4];
    } lastSuccessValue;
} DEV_CHARGER_REG_DATA;

typedef struct {
    uint8_t           reg;
    /* value and accessing lock */
    DEV_CHARGER_REG_DATA data;
    /* Polling intrval */
    uint32_t          u32refreshReload;
    uint32_t          u32refreshCounter;
    /* Control attribute */
    union {
        uint8_t       u8Ctrl;
        struct {
            uint8_t                : 1;
            uint8_t   autoRefersh  : 1;
            uint8_t   dataAvailable: 1;
            uint8_t   reserved_PEC : 1;       // PEC is not supportted right now
            uint8_t   ratio        : 2;       // ratio Reg:App;
                                              // 2'b00 - 1:1
                                              // 2'b01 - 1:2 or DEV_SMTBTY_RATIO_APP_1_REG_0P5
                                              // 2'b10 - 2:1 or DEV_SMTBTY_RATIO_APP_0P5_REG_1
                                              // 2'b11 - 1:4 or DEV_SMTBTY_RATIO_APP_1_REG_0P25
            uint8_t   accessType   : 2;       // 0 - REV; 1 - RO; 2 - WO; 3 - RW
        } f;
    } access;
    struct k_mutex  rwLock;
} DEV_CHARGER_REG_ITEM;


#pragma pack(push,1)
#define PACK_STRUCT                      __attribute__((packed, aligned(1)))
/* *****************************************************************************
 *                                                                             *
 * MP2764 register maps                   Reg,     Bit# | Comments | POR value *
 *                                                                             *
 * **************************************************************************** */
typedef enum {

    DEV_MP2764_REG_ChargeCurSetting0          = 0x07,
	DEV_MP2764_REG_InputMinVoltSetting        = 0x08,
	DEV_MP2764_REG_MinSysVoltSetting          = 0x09,
	DEV_MP2764_REG_InputCurLimitSetting       = 0x0A,
	DEV_MP2764_REG_OutputVoltSettingSrc       = 0x0B,
	DEV_MP2764_REG_BatCurOutputCurLimitSrc    = 0x0C,
	DEV_MP2764_REG_BatImpedanceCompensation   = 0x0D,
	DEV_MP2764_REG_ConfigReg0                 = 0x0E,
	DEV_MP2764_REG_JeitaNtcActionSetting      = 0x0F,
	DEV_MP2764_REG_TempProtectionSetting      = 0x10,
	DEV_MP2764_REG_ConfigReg1                 = 0x11,
	DEV_MP2764_REG_ConfigReg2                 = 0x12,
	DEV_MP2764_REG_ConfigReg3                 = 0x13,
	DEV_MP2764_REG_ChargeCurSetting1          = 0x14,
	DEV_MP2764_REG_BatRegulationVoltSetting   = 0x15,
	DEV_MP2764_REG_ConfigReg4                 = 0x16,
	DEV_MP2764_REG_ConfigReg5                 = 0x17,
	DEV_MP2764_REG_TwoLevInputCurLimitSetting = 0x18,
	DEV_MP2764_REG_ProchotConfig0             = 0x19,
	DEV_MP2764_REG_ProchotConfig1             = 0x1A,
	DEV_MP2764_REG_ProchotConfig2             = 0x1B,
	DEV_MP2764_REG_IntMaskCtrl                = 0x1C,
	DEV_MP2764_REG_WdtRstReg                  = 0x1D,
	DEV_MP2764_REG_RegRstReg                  = 0x1E,
	DEV_MP2764_REG_BatConfigLockReg           = 0x1F,
	DEV_MP2764_REG_VinVapSetting              = 0x2B,
	DEV_MP2764_REG_VSysVapSetting             = 0x2C,
	DEV_MP2764_REG_AdcResultInputVolt         = 0x43,
	DEV_MP2764_REG_AdcResultInputCur          = 0x44,
	DEV_MP2764_REG_AdcResultBatVoltPerCell    = 0x45,
	DEV_MP2764_REG_AdcResultSysVolt           = 0x46,
	DEV_MP2764_REG_AdcResultChargeCur         = 0x47,
	DEV_MP2764_REG_AdcResultJunctionTemp      = 0x48,
	DEV_MP2764_REG_AdcResultOutputVoltSrc     = 0x49,
	DEV_MP2764_REG_AdcResultOutputCurSrc      = 0x4A,
	DEV_MP2764_REG_AdcResultBatDischargeCur   = 0x4B,
	DEV_MP2764_REG_StatusFaultReg0            = 0x4C,
	DEV_MP2764_REG_StatusFaultReg1            = 0x4D,
	DEV_MP2764_REG_FaultStatusLatchReg        = 0x4E,
} DEV_MP2764_REG;

/* Register 0x07 Charge current setting including the trickle charge current, the pre-charge current and the charge
termination current */
typedef struct {
    union {
        struct {
        	uint16_t i_term      :4; /* Termination current setting. Default: 125mA */
        	uint16_t i_pre       :4; /* Pre-charge current setting. Default: 250mA */
        	uint16_t i_tc        :4; /* Trickle charge current setting. Default: 250mA */
        	uint16_t resv        :4;
        } PACK_STRUCT f;
        uint16_t data;
    };
} DEV_MP2764_REG_07;

/* Register 0x08 This register sets the input minimum voltage limit. */
typedef struct {
    union {
        struct {
        	uint16_t vin_min     :9; /* Input minimum voltage setting. Default: 4500mV */
        	uint16_t resv        :7;
        } PACK_STRUCT f;
        uint16_t data;
    };
} DEV_MP2764_REG_08;

/* Register 0x09 This register sets VTRACK and minimum system voltage by per cell in NVDC power path management
control, the minimum system voltage is regulated at (VSYS_MIN+VTRACK) x BAT_NUM. */
typedef struct {
    union {
        struct {
        	uint16_t vsys_min    :6; /* Minimum system voltage per cell setting for NVDC power path
    								    management control. Default: 3000mV */
        	uint16_t v_track     :3; /* Track voltage per cell setting for NVDC power path
    									management control. Default: 100mV */
        	uint16_t resv        :7;
        } PACK_STRUCT f;
        uint16_t data;
    };
} DEV_MP2764_REG_09;

/* Register 0x0A This register sets input current limit. While two-level current limit is enabled, IIN_LIM1 and IIN_LIM2 setting
are all valid, otherwise only IIN_LIM1 is valid. */
typedef struct {
    union {
        struct {
        	uint16_t iin_lim1    :8; /* The second level input current limit setting. Default: 4500mA */
        	uint16_t iin_lim2    :8; /* The first level input current limit setting. Default: 3000mA */
        } PACK_STRUCT f;
        uint16_t data;
    };
} DEV_MP2764_REG_0A;

/* Register 0x0B This register sets the output voltage at IN port in reverse source mode. */
typedef struct {
    union {
        struct {
        	uint16_t vin_src    :12; /* The output voltage setting in source mode. Default: 5000mV */
        	uint16_t resv       :4;
        } PACK_STRUCT f;
        uint16_t data;
    };
} DEV_MP2764_REG_0B;

/* Register 0x0C This register sets the battery current limit and output current limit in source mode. */
typedef struct {
    union {
        struct {
        	uint16_t iin_src       :7; /* The output voltage setting in source mode. Default: 2500mA. */
        	uint16_t resv          :1;
        	uint16_t ibatt_dischg  :8; /* The battery discharge current limit setting in source mode Default: 8000mA */
        } PACK_STRUCT f;
        uint16_t data;
    };
} DEV_MP2764_REG_0C;

/* Register 0x0D This register sets battery impedance compensation parameters and thermal regulation threshold. */
typedef struct {
    union {
        struct {
        	uint16_t tj_reg        :2; /* These bits are used to set the junction temperature regulation
                                          thresholds. Default: 120oC. */
        	uint16_t tj_reg_en     :1; /* This bit is used to enable thermal regulation loop. Default: enabled */
        	uint16_t v_clamp       :3; /* These bits set max compensation voltage for battery full
    									  according to the battery impedance. Default: 0mV */
        	uint16_t r_batt        :3; /* These bits are used to set the predicted battery impedance. Default: 0 mohm/Cell */
        	uint16_t resv          :7;
        } PACK_STRUCT f;
        uint16_t data;
    };
} DEV_MP2764_REG_0D;

/* Register 0x0E This register sets battery impedance compensation parameters and thermal regulation threshold. */
typedef struct {
    union {
        struct {
        	uint16_t i_batt_oc_en    :1; /* This bit enables the battery over-current protection in
    									    source mode. After IBATT>IBATT_OC for 100us, the DC/DC
    									    will be disabled, it will restart after 300ms. But BFET still
    									    keeps on to power system. Default: Enabled. */
        	uint16_t iin_oc_en       :1; /* This bit enables the input over-current protection in charge
    								   	    mode. After IIN>IIN_OCP (REG1Ah bit[8]) for 100us, the DC/DC
    									    will be disabled, it will restart after 300ms Default: Disabled */
        	uint16_t v_batt_pre      :3; /* These bits are used to set the pre-charge to linear CC charge
    									    transition threshold. Default: 3.0V/Cell */
        	uint16_t v_batt_uv_en    :1; /* This bit enables the battery under-voltage in both charge and
    									   source mode.. Default: Enabled */
        	uint16_t v_batt_low      :2; /* These bits are used to set battery low-voltage protection falling
    									   threshold in source mode. The hysteresis is 250mV. Default: 2.8V/Cell */
        	uint16_t v_batt_low_act  :1; /* This bit configures the action when VBATT<VBATT_LOW in source
    									    mode.Default: Only generate an INT to inform the host. */
        	uint16_t v_batt_low_en   :1; /* This bit is used to enable battery low-voltage protection in
    									    source mode. Default: Disabled */
        	uint16_t iin_lim_en      :1; /* This bit enables the input current loop in charge mode. Default: Enabled */
        	uint16_t i_batt_dschg_en :1; /* This bit enables the battery discharge current loop in source
    										mode. Default: Disabled */
        	uint16_t i_src_loop_en   :1; /* This bit enables the output current loop in source mode. Default: Enabled */
        	uint16_t v_rech          :1; /* This bit is used to configure the battery recharge threshold.
    									    VRECH will be invalid if charge termination is disabled. Default: 200mV/Cell */
        	uint16_t bfet_oc_act     :1; /* This bit decides the BFET behaviors when discharge current is
    										over OCP threshold in battery only mode. Default: Keeps on */
        	uint16_t resv            :1;
        } PACK_STRUCT f;
        uint16_t data;
    };
} DEV_MP2764_REG_0E;

/* battery pre-chrage voltage */
typedef enum
{
	DEV_MP2764_BATTERY_PRECHARG_VOLT_2_2V = 0,  /* 3'b000 */
	DEV_MP2764_BATTERY_PRECHARG_VOLT_2_4V,      /* 3'b001 */
	DEV_MP2764_BATTERY_PRECHARG_VOLT_2_5V,      /* 3'b010 */
	DEV_MP2764_BATTERY_PRECHARG_VOLT_2_6V,      /* 3'b011 */
	DEV_MP2764_BATTERY_PRECHARG_VOLT_2_8V,      /* 3'b100 */
	DEV_MP2764_BATTERY_PRECHARG_VOLT_3_0V,      /* 3'b101 (default) */
	DEV_MP2764_BATTERY_PRECHARG_VOLT_3_2V,      /* 3'b110 */
	DEV_MP2764_BATTERY_PRECHARG_VOLT_3_4V       /* 3'b111 */
} EM_DEV_MP2764_BATTERY_PRECHARG_VOLT;

/* battery low voltage */
typedef enum
{
	DEV_MP2764_BATTERY_LOW_VOLT_2_6V = 0,  /* 2'b00 */
	DEV_MP2764_BATTERY_LOW_VOLT_2_8V,      /* 2'b01 (default) */
	DEV_MP2764_BATTERY_LOW_VOLT_3_0V,      /* 2'b10 */
	DEV_MP2764_BATTERY_LOW_VOLT_3_1V       /* 2'b11 */
} EM_DEV_MP2764_BATTERY_LOW_VOLT;

/* Register 0x0F This register sets JEITA actions for hot, warm, cool and cold. */
typedef struct {
    union {
        struct {
        	uint16_t jeita_iset     :2; /* These bits are used to configure the charge current when NTC
    									   warm or cool protection Default: 1/2*ICC. */
        	uint16_t jeita_vset     :2; /* These bits are used to configure how much VBATT_REG is
    									   reduced when NTC warm or cool protection. Default: -100mV/Cell */
        	uint16_t cool_act       :2; /* These bits are used to configure the charging action when NTC
    									   cool protection. Default: Only reduce ICC */
        	uint16_t warm_act       :2; /* These bits are used to configure the charging action when NTC
    									   warm protection Default: Only reduce VBATT_REG */
        	uint16_t resv           :8;
        } PACK_STRUCT f;
        uint16_t data;
    };
} DEV_MP2764_REG_0F;

typedef enum
{
	DEV_MP2764_JEITA_ISET_1_2_ICC = 0,  /* 2'b00 (default) */
	DEV_MP2764_JEITA_ISET_1_4_ICC,      /* 2'b01 */
	DEV_MP2764_JEITA_ISET_1_8_ICC,      /* 2'b10 */
	DEV_MP2764_JEITA_ISET_DISABLE       /* 2'b11 */
} EM_DEV_MP2764_JEITA_ISET;

typedef enum
{
	DEV_MP2764_JEITA_VSET_100mV = 0,  /* 2'b00 (default) */
	DEV_MP2764_JEITA_VSET_150mV,      /* 2'b01 */
	DEV_MP2764_JEITA_VSET_200mV,      /* 2'b10 */
	DEV_MP2764_JEITA_VSET_250mV       /* 2'b11 */
} EM_DEV_MP2764_JEITA_VSET;

typedef enum
{
	DEV_MP2764_COOL_ACT_NO_ACTION = 0,      /* 2'b00  */
	DEV_MP2764_COOL_ACT_REDUCE_VBATTREG,    /* 2'b01 */
	DEV_MP2764_COOL_ACT_REDUCE_ICC,         /* 2'b10 (default)*/
	DEV_MP2764_COOL_ACT_REDUCE_BOTH         /* 2'b11 */
} EM_DEV_MP2764_COOL_ACT;

typedef enum
{
	DEV_MP2764_WARM_ACT_NO_ACTION = 0,      /* 2'b00  */
	DEV_MP2764_WARM_ACT_REDUCE_VBATTREG,    /* 2'b01 */
	DEV_MP2764_WARM_ACT_REDUCE_ICC,         /* 2'b10 (default)*/
	DEV_MP2764_WARM_ACT_REDUCE_BOTH         /* 2'b11 */
} EM_DEV_MP2764_WARM_ACT;

/* Register 0x10 This register sets JEITA temperature thresholds */
typedef struct {
    union {
        struct {
        	uint16_t v_cold     :2; /* These bits are used to configure the charge current when NTC
    								   cold protection. Default: 75.43%. */
        	uint16_t v_cool     :2; /* These bits are used to configure how much VBATT_REG is
    								   reduced when NTC cool protection. Default: 67.12% */
        	uint16_t v_warm     :2; /* These bits are used to configure the charging action when NTC
    								   warm protection. Default: 40.64% */
        	uint16_t v_hot      :2; /* These bits are used to configure the charging action when NTC
    								   hot protection. Default: 33.25% */
        	uint16_t ntc_act    :1; /* This bit is used to configure the IC behavior after NTC fault
    								   happens. Default: Deliver INT and take corresponding JEITA actions. */
        	uint16_t ntc_en     :1; /* This bit is used to enable NTC protection. When it's disabled,
    								   no action is taken when NTC/VNTC is out of warm/cool/cold/hot
    								   range. Default: Enabled */
        	uint16_t resv       :6;
        } PACK_STRUCT f;
        uint16_t data;
    };
} DEV_MP2764_REG_10;

typedef enum
{
	DEV_MP2764_VCOLD_79_29 = 0,      /* 2'b00  */
	DEV_MP2764_VCOLD_75_43,          /* 2'b01 (default) */
	DEV_MP2764_VCOLD_71_34,          /* 2'b10 */
	DEV_MP2764_VCOLD_67_12           /* 2'b11 */
} EM_DEV_MP2764_VCOLD;

typedef enum
{
	DEV_MP2764_VCOOL_75_43 = 0,      /* 2'b00  */
	DEV_MP2764_VCOOL_71_34,          /* 2'b01 */
	DEV_MP2764_VCOOL_67_12,          /* 2'b10 (default) */
	DEV_MP2764_VCOOL_62_85           /* 2'b11 */
} EM_DEV_MP2764_VCOOL;

typedef enum
{
	DEV_MP2764_VWARM_43_69 = 0,      /* 2'b00  */
	DEV_MP2764_VWARM_40_64,          /* 2'b01 (default) */
	DEV_MP2764_VWARM_37_89,          /* 2'b10 */
	DEV_MP2764_VWRAM_35_43           /* 2'b11 */
} EM_DEV_MP2764_VWARM;

typedef enum
{
	DEV_MP2764_VHOT_37_89 = 0,      /* 2'b00  */
	DEV_MP2764_VHOT_35_43,          /* 2'b01 */
	DEV_MP2764_VHOT_33_25,          /* 2'b10 (default) */
	DEV_MP2764_VHOT_31_32           /* 2'b11 */
} EM_DEV_MP2764_VHOT;

/* Register 0x11 This register is control bits for some protections. */
typedef struct {
    union {
        struct {
        	uint16_t tmr_en              :1; /* This bit is used to enable charge safety timer. Default: Enabled. */
        	uint16_t weak_src_en     	 :1; /* This bit enables the weak source auto shutdown function.
    											When it's enabled, the DC/DC is turned off when VIN_MIN loop
    											kicks in and the input current is lower than 200mA Default: Enabled */
        	uint16_t src_scp_en      	 :1; /* This bit enables the output SCP in source mode. When SCP
    									    	happens, DC/DC enters hiccup mode with 300ms period. Default: Enabled */
        	uint16_t sys_scp_en    		 :1; /* When VSYS < 2V, the DC/DC enters hiccup mode with 300ms
    										    period. Default: Enabled */
        	uint16_t batt_ovp_en    	 :1; /* This bit is used decide whether turn off BEFET when BATT
    											OVP happens. Enable means turn off BFET and disable means
    											keeps on. Default: Disabled. */
        	uint16_t sys_ovp_en     	 :1; /* This bit is used to enable/disable system OVP. When this bit is
    											enabled, DCDC is turned off when SYS OVP happens. Default: Enabled */
        	uint16_t adc_en         	 :1; /* In battery only mode, when this bit is enabled, the ADC will be
    											turned on. When DC/DC is enabled the ADC will be enabled
    											automatically ignore this bit. Default: Disabled */
        	uint16_t ibatt_dschg_ocp_dgl :1; /* This bit is used to set the turn off time of BFET hiccup when
    								     	    battery discharge OCP happens in supplement mode. Default: 100ms */
        	uint16_t ibatt_dschg_ocp_th  :1; /* This bit is used to set the battery discharge OCP threshold in
    											both supplement and source mode. Default: 20A */
        	uint16_t src_fast_on_en      :1; /* This bit is used to set the deglitch time between forward charge
    											mode and reverse source mode, and shrinks the soft-start time
    											to minimum. Default: Disabled */
        	uint16_t src_uv_swap_en      :1; /* This bit set the source mode start-up condition When this bit is
    											enabled, output UV will be used as an essential condition to
    											enable source mode. Default: Disabled */
        	uint16_t force_bfet_off      :1; /* This bit is used to turn off the battery FET. (BGATE to GND) Default: Disabled */
        	uint16_t bgate_hiz           :1; /* This bit is used to enable BGATE_HIZ.
    											Disable HIZ (Enable BGATE driver)
    											Enable HIZ (Disable BGATE driver) Default: Disabled */
        	uint16_t low_pwr_en          :1; /* Enable the minimum battery quiescent current in battery only
    											mode by turning off battery discharge current detection. Default: Disabled */
        	uint16_t resv                :2;
        } PACK_STRUCT f;
        uint16_t data;
    };
} DEV_MP2764_REG_11;

/* Register 0x12 Configuration Register for discharge protection and threshold. */
typedef struct {
    union {
        struct {
        	uint16_t vin_ovp_dgl     :1; /* This bit is used to configure the input over-voltage protection
    										deglitch time Default: 10ms. */
        	uint16_t src_ov     	 :1; /* This bit is used to configure the output OVP threshold in source
    										mode. It is defined as a percentage refer to the source
    										regulation voltage. Default: 110% */
        	uint16_t vsys_ov      	 :1; /* This bit is used to configure the system over-voltage threshold.
    										It is defined as a percentage refer to the system regulation
    										voltage. Default: 106% */
        	uint16_t two_inlim_en    :1; /* This bit enables the two-level input current limit. Default: Disabled */
        	uint16_t aicl_step    	 :1; /* This bit is used to set change step of input current limit when
    										AICL algorithm runs. Default: 50mA. */
        	uint16_t aicl_en     	 :1; /* This bit is used to enable automatic input current limit (ACIL)
    										algorithm. Default: Disabled */
        	uint16_t resv1         	 :1;
        	uint16_t in_dschg_en     :1; /* This bit is used to enable the dummy load at IN terminal. When
    										this bit is 0, the dummy load is always turned off. When this bit
    										is 1, the dummy load will be turn on when SRC OVP happens
    										or VIN_MIN kicks and IIN is lower than 200mA. Default: Disabled */
        	uint16_t batt_dschg_en   :1; /* This bit is used to enable the dummy load at BATT terminal.
    										When it is enabled, the dummy load will turn on automatically
    										when battery OVP happens. Default: Disabled */
        	uint16_t resv2           :1;
        	uint16_t wtd             :2; /* These bits are used to configure the watchdog timer. Default: Disabled */
        	uint16_t term_en         :1; /* This bit is used to enable the charge termination.Default: Enabled */
        	uint16_t resv3           :3;
        } PACK_STRUCT f;
        uint16_t data;
    };
} DEV_MP2764_REG_12;

typedef enum
{
	DEV_MP2764_WDT_DISABLE = 0,     /* 2'b00 (default) */
	DEV_MP2764_WDT_40S,             /* 2'b01 */
	DEV_MP2764_WDT_80S,             /* 2'b10 */
	DEV_MP2764_WDT_160S             /* 2'b11 */
} EM_DEV_MP2764_WDT;

/* Register 0x13 Configuration Register for charge enable control and charge timer setting. */
typedef struct {
    union {
        struct {
        	uint16_t chg_en      :1; /* This bit is used to enable charge mode. Default: Enabled. */
        	uint16_t dcdc_en     :1; /* This bit is used to enable buck-boost converter. Default: Enabled */
        	uint16_t clk_en   	 :1; /* This bit is used to force enable system clock in battery only. Default: Disabled */
        	uint16_t resv1       :1;
        	uint16_t src_en    	 :1; /* This bit is used to enable source mode. Default: Disabled. */
        	uint16_t otg_vap_sel :1; /* This bit is used to configure the OTG pin function. When this bit
    									is 0, it's used to control VAP; when this bit is 1, it's used to
    									control OTG. Default: OTG */
        	uint16_t chg_tmr     :4; /* These bits are used to configure CC/CV charge safety timer. Default: 20hous */
        	uint16_t resv2       :6;
        } PACK_STRUCT f;
        uint16_t data;
    };
} DEV_MP2764_REG_13;

/* Register 0x14 This register sets the fast CC Charge Current Setting. */
typedef struct {
    union {
        struct {
        	uint16_t icc         :8; /* These bits are used to set charge current in switching
    								    mode when VBATT > VSYS_REG_MIN. Default: 3A. */
        	uint16_t icc_lnr     :5; /* These bits are used to set charge current in linear mode
    									when VBATT < VSYS_REG_MIN. Default: 250mA */
        	uint16_t resv        :3;
        } PACK_STRUCT f;
        uint16_t data;
    };
} DEV_MP2764_REG_14;

/* Register 0x15 This register sets battery full regulation voltage */
typedef struct {
    union {
        struct {
        	uint16_t vbtt_reg    :10; /* These bits are used to set the battery-full regulation voltage per
    									 Cell. Default: 4.2V/Cell. */
        	uint16_t resv        :6;
        } PACK_STRUCT f;
        uint16_t data;
    };
} DEV_MP2764_REG_15;

/* Register 0x16 This register sets some monitors control */
typedef struct {
    union {
        struct {
        	uint16_t ibm_en                :1; /* This bit is used to enable the battery current monitor buffer in
    											  battery only mode. Default: Disabled. */
        	uint16_t psys_en               :1; /* This bit is used to enable the PSYS measurement circuit in
    										      battery only mode. Default: Disabled. */
        	uint16_t resv1                 :3;
        	uint16_t sys_uv_prochot_cmp_en :1; /* This bit is used to enable the PROCHOT system UV
    										  	  comparator in battery only mode. Default: Disabled. */
        	uint16_t resv2                 :10;
        } PACK_STRUCT f;
        uint16_t data;
    };
} DEV_MP2764_REG_16;

/* Register 0x17 Configuration Register for frequency and battery cell number control */
typedef struct {
    union {
        struct {
        	uint16_t bat_num          :2; /* These bits are used to configure the battery cell counts in
    										 series. Default: 3Cell. */
        	uint16_t ibatt_rsense_sel :1; /* This bit is used to configure the current sense gain of the
    										 battery current. Default: 10V/V. */
        	uint16_t inn_rsense_sel   :1; /* This bit is used to configure the current sense gain of the input
    										 current. Default: 20V/V. */
        	uint16_t sw_freq          :3; /* These bits are used to configure the frequency of DC/DC
    										 converter. Default: 600kHz. */
        	uint16_t in_gain          :1; /* This bit provides an addition option to reduce the gain by half to
    										 support higher input current. Default: x1. */
        	uint16_t ibatt_gain       :1; /* This bit provides an addition option to reduce the gain by half to
    										 support higher battery current. Default: x1. */
        	uint16_t wd_run_only_batt :1; /* Enable watchdog timer or not when only battery is connected.
    										 Only WDT enabled, this bit is valid. Default: Disable. */
        	uint16_t i2c_two_en       :1; /* When this bit is enabled, SMBUS clk keeps low for longer than
    										 25ms will reset SMBUS state machine. In battery only mode,
    										 REG13h bit[2] should also be enabled when this function is
    										 needed. Default: Disabled. */
        	uint16_t resv2            :5;
        } PACK_STRUCT f;
        uint16_t data;
    };
} DEV_MP2764_REG_17;

typedef enum
{
	DEV_MP2764_SW_SREQ_500KH = 2,         /* 3'b010 */
	DEV_MP2764_SW_SREQ_600KH,             /* 3'b011 (default) */
	DEV_MP2764_SW_SREQ_800KH,             /* 3'b100 */
	DEV_MP2764_SW_SREQ_1000KH             /* 3'b101 */
} EM_DEV_MP2764_SW_SREQ;

typedef enum
{
	DEV_MP2764_BAT_NUM_2CELL = 0,         /* 2'b00 */
	DEV_MP2764_BAT_NUM_3CELL,             /* 2'b01 (default) */
	DEV_MP2764_BAT_NUM_4CELL,             /* 2'b10 */
} EM_DEV_MP2764_BAT_NUM;

/* Register 0x18 This register sets two-level current limit related timings */
typedef struct {
    union {
        struct {
        	uint16_t tin_lim2              :7; /* These bits are used to configure the max duration time to allow
    											  input current limit changes from IINLIMIT1 to IINLIMIT2 setting
    											   within TADP_PERIOD. Default: 1.2ms. */
        	uint16_t resv1                 :1;
        	uint16_t tadp_period           :7; /* These bits are used to configure the total time duration of the
    											  second and first input current limit. Default: 20ms. */
        	uint16_t resv2                 :1;
        } PACK_STRUCT f;
        uint16_t data;
    };
} DEV_MP2764_REG_18;

/* Register 0x19 This register sets PROCHOT related parameters */
typedef struct {
    union {
        struct {
        	uint16_t t_degl_vin_plug       :1; /* This bit is used to configure the deglitch time of input plug out
    											  PROCHOT output. Default: 1us */
        	uint16_t t_degl_vbat_plug      :1; /* This bit is used to configure the deglitch time of battery removal
    										      PROCHOT output. Default: 1us */
        	uint16_t t_degl_vsys_uv        :1; /* This bit is used to configure the deglitch time of system under voltage
    										      PROCHOT output. Default: 1us */
        	uint16_t t_prochot_dur         :3; /* These bits are used to configure the duration of low state of
    											  PROCHOT output. Default: 2ms */
        	uint16_t t_ibatt_oc_degl       :2; /* These bits are used to configure the deglitch time of battery
    											  over-current PROCHOT output. Default: 100us */
        	uint16_t t_iin_phot2_degl      :2; /* These bits are used to configure the deglitch time of input
    											  current higher than IIN_PHOT2 to output PROCHOT. Default: 15us */
        	uint16_t vsys_uv_prochot       :6; /* These bits are used to configure the system under-voltage
    											  threshold for PROCHOT interrupt. Default: 5.4V */
        } PACK_STRUCT f;
        uint16_t data;
    };
} DEV_MP2764_REG_19;

/* prochot duration */
typedef enum
{
    DEV_MP2764_PROCHOT_DURATION_100us = 0,  /* 3'b000 */
    DEV_MP2764_PROCHOT_DURATION_500us,      /* 3'b001 */
    DEV_MP2764_PROCHOT_DURATION_1ms,        /* 3'b010 */
    DEV_MP2764_PROCHOT_DURATION_2ms,        /* 3'b011 (default) */
    DEV_MP2764_PROCHOT_DURATION_5ms,        /* 3'b100 */
    DEV_MP2764_PROCHOT_DURATION_10ms,       /* 3'b101 */
    DEV_MP2764_PROCHOT_DURATION_20ms,       /* 3'b110 */
    DEV_MP2764_PROCHOT_DURATION_5s          /* 3'b111 */
} EM_DEV_MP2764_PROCHOT_DURATION;

typedef enum
{
    DEV_MP2764_PROCHOT_DC_DEBOUNCE_100us = 0,  /* 2'b00 (default) */
    DEV_MP2764_PROCHOT_DC_DEBOUNCE_2ms,        /* 2'b01 */
    DEV_MP2764_PROCHOT_DC_DEBOUNCE_5ms,        /* 2'b10 */
    DEV_MP2764_PROCHOT_DC_DEBOUNCE_10ms        /* 2'b11 */
} EM_DEV_MP2764_PROCHOT_DC_DEBOUNCE;

/* prochot AC debounce */
typedef enum
{
	DEV_MP2764_PROCHOT_DEGL_IIN2_15uS = 0,   /* 2'b00 (default) */
	DEV_MP2764_PROCHOT_DEGL_IIN2_100uS,      /* 2'b01 */
	DEV_MP2764_PROCHOT_DEGL_IIN2_400uS,      /* 2'b10 */
	DEV_MP2764_PROCHOT_DEGL_IIN2_800uS       /* 2'b11 */
} EM_DEV_MP2764_PROCHOT_DEGL_IIN_PHOT2;

/* prochot uv debounce */
typedef enum
{
    DEV_MP2764_PROCHOT_UV_DEBOUNCE_1us = 0,    /* 1'b0 (default) */
    DEV_MP2764_PROCHOT_UV_DEBOUNCE_5us         /* 1'b1 */
} EM_DEV_MP2764_PROCHOT_UV_DEBOUNCE;

/* prochot dc remove debounce */
typedef enum
{
    DEV_MP2764_PROCHOT_DCOUT_DEBOUNCE_1us = 0,  /* 1'b0 (default) */
    DEV_MP2764_PROCHOT_DCOUT_DEBOUNCE_5us       /* 1'b1 */
} EM_DEV_MP2764_PROCHOT_DCOUT_DEBOUNCE;

/* prochot ac plug debounce */
typedef enum
{
    DEV_MP2764_PROCHOT_ACIN_DEBOUNCE_1us = 0,   /* 1'b0 (default) */
    DEV_MP2764_PROCHOT_ACIN_DEBOUNCE_5us        /* 1'b1 */
} EM_DEV_MP2764_PROCHOT_ACIN_DEBOUNCE;

/* Register 0x1A This register sets PROCHOT protection related parameters. */
typedef struct {
    union {
        struct {
        	uint16_t ibatt_prochot    :3; /* These bits are used to configure the battery over-current
    										 threshold for PROCHOT interrupt. Default: 20A */
        	uint16_t psys_gain        :2; /* These bits are used to configure the PSYS output current gain. Default: 1uA/W */
        	uint16_t t_in_max2        :2; /* These bits configure the time limitation for input current stays at
    										 2nd input current limit. Once this limitation is reached, a
    										 PROCHOT should be asserted. Default: 1ms */
        	uint16_t t_in_max1        :2; /* These bits are used to configure the time limitation for input
    										 current stays above 1st input current limit 1 but below 2nd input
    										 current limit. Once this limitation is reached, a PROCHOT
    										 should be asserted. Default: 5s */
        	uint16_t iin_oc           :1; /* This is for input over-current protection threshold setting. When
    										 the input current is over this threshold for 100us, the DC/DC
    										 can be shutdown optionally via Reg1Ah, bit [10]. Default: 10A */
        	uint16_t t_degl_iin_phot1 :1; /* This bit configures the deglitch time of IIN PROCHOT1 Default: 1ms */
        	uint16_t resv1            :1;
        	uint16_t iin_phot2        :1; /* This bit is used to configure the threshold of IIN_PROT2. Default: 110%*IIN_LIM2 */
        	uint16_t resv2            :3;
        } PACK_STRUCT f;
        uint16_t data;
    };
} DEV_MP2764_REG_1A;

/* prochot dc threshold */
typedef enum
{
    DEV_MP2764_PROCHOT_DC_THRESHOLD_6A = 0,    /* 3'b000 */
	DEV_MP2764_PROCHOT_DC_THRESHOLD_8A,        /* 3'b001 */
	DEV_MP2764_PROCHOT_DC_THRESHOLD_10A,       /* 3'b010 */
	DEV_MP2764_PROCHOT_DC_THRESHOLD_12A,       /* 3'b011 */
	DEV_MP2764_PROCHOT_DC_THRESHOLD_14A,       /* 3'b100 */
	DEV_MP2764_PROCHOT_DC_THRESHOLD_15A,       /* 3'b101 */
	DEV_MP2764_PROCHOT_DC_THRESHOLD_18A,       /* 3'b110 */
	DEV_MP2764_PROCHOT_DC_THRESHOLD_20A        /* 3'b111 (default) */
} EM_DEV_MP2764_PROCHOT_DC_THRESHOLD;

/* prochot PSYS Gain */
typedef enum
{
    DEV_MP2764_PROCHOT_PSYS_GAIN_1uAW = 0,    /* 2'b00 (default) */
	DEV_MP2764_PROCHOT_PSYS_GAIN_0_25uAW,     /* 2'b01 */
	DEV_MP2764_PROCHOT_PSYS_GAIN_0_5uAW       /* 2'b10 */
} EM_DEV_MP2764_PROCHOT_PSYS_GAIN;

/* prochot t_in_max2 */
typedef enum
{
    DEV_MP2764_PROCHOT_TIN_MAX2_1mS = 0,    /* 2'b00 (default) */
	DEV_MP2764_PROCHOT_TIN_MAX2_2mS,        /* 2'b01 */
	DEV_MP2764_PROCHOT_TIN_MAX2_5mS,        /* 2'b10 */
	DEV_MP2764_PROCHOT_TIN_MAX2_10mS        /* 2'b11 */
} EM_DEV_MP2764_PROCHOT_TIN_MAX2;

/* prochot t_in_max2 */
typedef enum
{
    DEV_MP2764_PROCHOT_TIN_MAX1_1S = 0,    /* 2'b00 */
	DEV_MP2764_PROCHOT_TIN_MAX1_2S,        /* 2'b01 */
	DEV_MP2764_PROCHOT_TIN_MAX1_5S,        /* 2'b10 (default) */
	DEV_MP2764_PROCHOT_TIN_MAX1_10S        /* 2'b11 */
} EM_DEV_MP2764_PROCHOT_TIN_MAX1;

/* prochot t_DEGL_IIN_PHOT1 */
typedef enum
{
    DEV_MP2764_PROCHOT_DEGL_IIN1_1mS = 0,    /* 1'b0 (default) */
	DEV_MP2764_PROCHOT_DEGL_IIN1_50mS        /* 1'b1 */
} EM_DEV_MP2764_PROCHOT_DEGL_IIN_PHOT1;

/* Register 0x1B This register sets PROCHOT signals control. */
typedef struct {
    union {
        struct {
        	uint16_t in_pres_phot_en    :1; /* This bit is used to enable the input plug-out PROCHOT output. */
        	uint16_t batt_pres_phot_en  :1; /* This bit is used to enable the battery removal PROCHOT output. */
        	uint16_t vsys_phot_en       :1; /* This bit is used to enable the VSYS_UV PROCHOT output. */
        	uint16_t iin_phot1_en       :1; /* This bit is used to enable the 1st input OC PROCHOT output. */
        	uint16_t iin_phot2_en       :1; /* This bit is used to enable the 2nd input OC PROCHOT output. */
        	uint16_t bat_oc_phot_en     :1; /* This bit is used to enable the battery OC PROCHOT output. */
        	uint16_t vin_min_phot_en    :1; /* This bit is used to enable the VIN_MIN PROCHOT output. */
        	uint16_t iinlim_phot_en     :1; /* This bit is used to enable the IINLIM PROCHOT output. */
        	uint16_t vap_exit_phot_en   :1; /* This bit is used to enable the VAP exit PROCHOT output. */
        	uint16_t resv               :7;
        } PACK_STRUCT f;
        uint16_t data;
    };
} DEV_MP2764_REG_1B;

/* Register 0x1C This register sets INT mask control */
typedef struct {
    union {
        struct {
        	uint16_t bat_ocp_mask      :1; /* This bit is used to mask the INT for battery over current protection. Default: Masked */
        	uint16_t iin_lim_stat_mask :1; /* This bit is used to mask the INT for IIN_MIN regulation loop. Default: Masked */
        	uint16_t vin_lim_stat_mask :1; /* This bit is used to mask the INT for VIN_MIN regulation loop. Default: Masked */
        	uint16_t thermal_mask      :1; /* This bit is used to mask the INT for thermal shutdown or thermal regulation loop. Default: Masked */
        	uint16_t vap_stat_mask     :1; /* This bit is used to mask the INT for VAP operation. Default: Masked */
        	uint16_t chg_stat_mask     :1; /* This bit is used to mask the INT for charge status change. Default: Masked */
        	uint16_t md_stat_mask      :1; /* This bit is used to mask the INT for DCDC operation mode change. Default: Masked */
        	uint16_t ntc_flt_mask      :1; /* This bit is used to mask the INT for NTC status change. Default: Masked */
        	uint16_t tmr_flt_masl      :1; /* This bit is used to mask the INT for safety timer fault. Default: Masked */
        	uint16_t batt_miss_mask    :1; /* This bit is used to mask the INT for battery missing fault. Default: Masked */
        	uint16_t wdt_flt_mask      :1; /* This bit is used to mask the INT for watchdog timer fault. Default: Not Masked*/
        	uint16_t vbat_ovp_mask     :1; /* This bit is used to mask the INT for battery OVP fault. Default: Masked */
        	uint16_t pg_stat_mask      :1; /* This bit is used to mask the INT for good input voltage. Default: Masked */
        	uint16_t vin_ovp_mask      :1; /* This bit is used to mask the INT for input voltage OVP fault. Default: Masked */
        	uint16_t otg_ovp_mask      :1; /* This bit is used to mask the INT for OTG OVP and SCP. Default: Masked */
        	uint16_t sys_ovp_mask      :1; /* This bit is used to mask the INT for SYS OVP and SCP. Default: Masked */
        } PACK_STRUCT f;
        uint16_t data;
    };
} DEV_MP2764_REG_1C;

/* Register 0x1D This register reset watchdog timer. */
typedef struct {
    union {
        struct {
        	uint16_t wdt_rst     :1; /* This bit is used to reset the watchdog timer. It resets to 0 after
    									the bit is written 1. Default: Normal */
        	uint16_t resv        :15;
        } PACK_STRUCT f;
        uint16_t data;
    };
} DEV_MP2764_REG_1D;

/* Register 0x1E This register reset all the registers. */
typedef struct {
    union {
        struct {
        	uint16_t reg_rst     :1; /* This bit is used to reset all the registers. It resets to 0 after the
    									bit is written 1. Default: Keep current register setting */
        	uint16_t resv        :15;
        } PACK_STRUCT f;
        uint16_t data;
    };
} DEV_MP2764_REG_1E;

/* Register 0x1F This register reset all the registers.. */
typedef struct {
    union {
        struct {
        	uint16_t bat_num_lock  :1; /* Only when this bit is 0, can customer configure VBAT_REG via
    									  I2C/SMBUS. Default: Unlocked */
        	uint16_t vbat_reg_lock :1; /* Only when this bit is 0, can customer configure battery cell
    									  count via I2C/SMBUS. Default: Unlocked */
        	uint16_t resv          :14;
        } PACK_STRUCT f;
        uint16_t data;
    };
} DEV_MP2764_REG_1F;

/* Register 0x2B This register sets VIN_MIN_VAP threshold. */
typedef struct {
    union {
        struct {
        	uint16_t vin_min_vap   :8; /* These bits are used to configure VIN_MIN_VAP threshold in VAP
    									  mode, when this threshold is kicked, a PROCHOT INT should
    									  be asserted.
    									  Range: 0V to 25.5V/cell Default: 4000mV */
        	uint16_t resv          :8;
        } PACK_STRUCT f;
        uint16_t data;
    };
} DEV_MP2764_REG_2B;

/* Register 0x2C This register sets VSYS_REG_VAP threshold. */
typedef struct {
    union {
        struct {
        	uint16_t vsys_reg_vap  :12; /* These bits are used to set the system regulation voltage in VAP mode.
    									   Range: 0V to 6400mV/cell. Default: 3050mV/Cell */
        	uint16_t resv          :4;
        } PACK_STRUCT f;
        uint16_t data;
    };
} DEV_MP2764_REG_2C;

/* Register 0x40 This register stores the device I2C/SMBUS address. */
typedef struct {
    union {
        struct {
        	uint16_t rev          :4; /* These bits are used for customized rev tracking */
        	uint16_t customer_id  :4; /* These bits are used for customized ID tracking */
        	uint16_t resv         :1;
        	uint16_t addr_set     :7; /* These bits are used for device address.
    									 When PROG pin is configured as address setting, the highest 4
    									 bits is “0010” and the lowest 3bits can be configured by external
    									 resistor value connected between PROG and GND. Default: 0x5C */
        } PACK_STRUCT f;
        uint16_t data;
    };
} DEV_MP2764_REG_40;

/* Register 0x4C This register indicates charge status and faults */
typedef struct {
    union {
        struct {
        	uint16_t vsys_min_reg :1; /* This bit indicates whether the device stays in VSYS_MIN
    									 regulation loop. Default: Not in VSYS_MIN regulation */
        	uint16_t ibatt_ocp    :1; /* This bit indicates whether the battery discharge current OC is
    									 kicked. The device asserts an INT when the fault happens. Default: Normal */
        	uint16_t aicl_run     :1; /* This bit indicates whether MPPT or AICL algorithm is enabled. Default: Not in MPPT or AICL */
        	uint16_t thermal_stat :1; /* This bit indicates whether the device stays in thermal regulation
    									 loop. The device asserts an INT signal when state changes. Default: Not in thermal regulation */
        	uint16_t chg_stat     :3; /* These bits indicate the charging status. The device asserts an
    									 INT signal when the state changes. Default: Not charging */
        	uint16_t iin_lim_stat :1; /* This bit indicates whether the device stays in IIN_LIM regulation
    									 loop. The device asserts an INT signal when state changes. Default: Not in IIN_LIM regulation */
        	uint16_t vin_min_stat :1; /* This bit indicates whether the device stays in VIN_MIN regulation
    									 loop. The device asserts an INT signal when state changes. Default: Not in VIN_LIM regulation */
        	uint16_t vap_stat     :1; /* This bit indicates whether the device works in VAP mode. The
    									 device asserts an INT signal when state changes. Default: Not in VAP mode */
        	uint16_t batt_miss    :1; /* This bit indicates whether battery is missing. The device asserts
    									 an INT signal when state changes. Default: Not missing */
        	uint16_t dc_dc_stat   :2; /* These bits indicate the DC/DC operation modes. Default: Suspend */
        	uint16_t vinpg_stat   :1; /* This bit indicates whether input power is good. The device
    									 asserts an INT signal when states change. Default: VIN is not power good*/
        	uint16_t dir_stat     :1; /* This bit indicates the device operation modes. Default: Forward mode */
        	uint16_t mode_stat    :1; /* This bit indicates the DC/DC operation status. Default: Standby Mode */
        } PACK_STRUCT f;
        uint16_t data;
    };
} DEV_MP2764_REG_4C;

typedef enum
{
    DEV_MP2764_CHARGE_STATE_NOT_CHARING = 0,    /* 3'b000 (default) */
	DEV_MP2764_CHARGE_STATE_TRICKLE_CHARGE,     /* 3'b001 */
	DEV_MP2764_CHARGE_STATE_PRE_CHARGE,         /* 3'b010 */
	DEV_MP2764_CHARGE_STATE_LINEAR_CC,          /* 3'b011 */
	DEV_MP2764_CHARGE_STATE_SWITCH_CC,          /* 3'b100 */
	DEV_MP2764_CHARGE_STATE_CV_CHARGE,          /* 3'b101 */
	DEV_MP2764_CHARGE_STATE_CHARGE_TERMINATION, /* 3'b110 */
	DEV_MP2764_CHARGE_STATE_SUPPLEMENT          /* 3'b111 */
} EM_DEV_MP2764_CHARGE_STATE;

typedef enum
{
    DEV_MP2764_DC_DC_STATE_SUSPEND = 0,     /* 2'b00 (default) */
	DEV_MP2764_DC_DC_STATE_BUCK_MODE,       /* 2'b01 */
	DEV_MP2764_DC_DC_STATE_BUCK_BROOST_MODE,/* 2'b10 */
	DEV_MP2764_DC_DC_STATE_BOOST_MODE       /* 2'b11 */
} EM_DEV_MP2764_DC_DC_STATE;

/* Register 0x4D This register indicates protection status */
typedef struct {
    union {
        struct {
        	uint16_t resv1     :2;
        	uint16_t ntc_flt   :3; /* These bits indicate NTC fault. The device asserts an INT signal
    								  when fault occurs. Default: Normal */
        	uint16_t resv2     :1;
        	uint16_t ther_shdn :1; /* This bit indicates thermal shutdown fault. The device asserts an
    								  INT signal when fault occurs. Default: Normal */
        	uint16_t tmr_flt   :1; /* This bit indicates charge timer expired fault. The device asserts
    								  an INT signal when fault changes. Default: Normal */
        	uint16_t wdt_flt   :1; /* This bit indicates the WDT expired fault. The device asserts an
    								  INT signal when fault occurs. Default: Normal */
        	uint16_t batt_low  :1; /* This bit indicates a low battery voltage in source mode. Default: Normal */
        	uint16_t batt_ovp  :1; /* This bit indicates whether battery OVP fault. The device asserts
    								  an INT signal fault occurs. Default: Normal */
        	uint16_t in_ovp    :1; /* This bit indicates input OVP fault. The device asserts an INT
    								  signal fault occurs. Default: Normal */
        	uint16_t sys_scp   :1; /* This bit indicates SYS SCP fault. The device asserts an INT
    								  signal fault occurs. Default: Normal */
        	uint16_t otg_scp   :1; /* This bit indicates OTG SCP fault. The device asserts an INT
    								  signal fault occurs. Default: Normal*/
        	uint16_t sys_ovp   :1; /* This bit indicates SYS OVP fault. The device asserts an INT
    								  signal fault occurs. Default: Normal */
        	uint16_t otg_ovp   :1; /* This bit indicates output OVP fault in source mode. The device
    								  asserts an INT signal fault occurs. Default: Normal */
        } PACK_STRUCT f;
        uint16_t data;
    };
} DEV_MP2764_REG_4D;

typedef enum
{
	EM_DEV_MP2764_NTC_FLT_NORMAL = 0,   /* 3'b000 (default) */
	EM_DEV_MP2764_NTC_FLT_NTC_COLD,     /* 3'b001 */
	EM_DEV_MP2764_NTC_FLT_NTC_COOL,     /* 3'b010 */
	EM_DEV_MP2764_NTC_FLT_NTC_WARM,     /* 3'b011 */
	EM_DEV_MP2764_NTC_FLT_NTC_HOT,      /* 3'b100 */
	EM_DEV_MP2764_NTC_FLT_NTC_FLOATING  /* 3'b101 */
} EM_DEV_MP2764_NTC_FLT;

/* Register 0x4E Once a fault indication bit of this register is set, it is latched even the fault is removable. It doesn’t reset
until it is read by I2C/SMBUS. */
typedef struct {
    union {
        struct {
        	uint16_t resv1     :2;
        	uint16_t ntc_flt   :3; /* These bits indicate NTC fault. The device asserts an INT signal
    								  when fault occurs. Default: Normal */
        	uint16_t resv2     :1;
        	uint16_t ther_shdn :1; /* This bit indicates thermal shutdown fault. The device asserts an
    								  INT signal when fault occurs. Default: Normal */
        	uint16_t tmr_flt   :1; /* This bit indicates charge timer expired fault. The device asserts
    								  an INT signal when fault changes. Default: Normal */
        	uint16_t wdt_flt   :1; /* This bit indicates the WDT expired fault. The device asserts an
    								  INT signal when fault occurs. Default: Normal */
        	uint16_t batt_low  :1; /* This bit indicates a low battery voltage in source mode. Default: Normal */
        	uint16_t batt_ovp  :1; /* This bit indicates whether battery OVP fault. The device asserts
    								  an INT signal fault occurs. Default: Normal */
        	uint16_t in_ovp    :1; /* This bit indicates input OVP fault. The device asserts an INT
    								  signal fault occurs. Default: Normal */
        	uint16_t sys_scp   :1; /* This bit indicates SYS SCP fault. The device asserts an INT
    								  signal fault occurs. Default: Normal */
        	uint16_t otg_scp   :1; /* This bit indicates OTG SCP fault. The device asserts an INT
    								  signal fault occurs. Default: Normal*/
        	uint16_t sys_ovp   :1; /* This bit indicates SYS OVP fault. The device asserts an INT
    								  signal fault occurs. Default: Normal */
        	uint16_t otg_ovp   :1; /* This bit indicates output OVP fault in source mode. The device
    								  asserts an INT signal fault occurs. Default: Normal */
        } PACK_STRUCT f;
        uint16_t data;
    };
} DEV_MP2764_REG_4E;

/* Register 0x43 This register indicates VIN ADC result. */
typedef struct {
    union {
        struct {
        	uint16_t vin     :10; /* TADC results of source output voltage.
    								 Range: 0V to 25.5 V
    								 LSB: 25mV
    								 IIN_SRC = DEC(bits[9:0]) x 25 (mV) */
        	uint16_t resv    :6;
        } PACK_STRUCT f;
        uint16_t data;
    };
} DEV_MP2764_REG_43;

/* Register 0x44 This register indicates IIN ADC result. */
typedef struct {
    union {
        struct {
        	uint16_t iin     :10; /* ADC results of input current. (IIN sense gain is 0.2V/A)
    								 Range: 0A to 7 A
    								 LSB: 7.8125mA
    								 IIN_SRC = DEC(bits[9:0]) x 7.8125 (mA) */
        	uint16_t resv    :6;
        } PACK_STRUCT f;
        uint16_t data;
    };
} DEV_MP2764_REG_44;

/* Register 0x45 This register indicates IIN ADC result. */
typedef struct {
    union {
        struct {
        	uint16_t iin     :12; /* ADC results of battery voltage per cell.
    								 Range: 0V to 6400mV/cell
    								 LSB: 1.5625mV/cell
    								 VBATT = DEC(bits[9:0]) x 1.5625 (mV) x BAT_MUM */
        	uint16_t resv    :4;
        } PACK_STRUCT f;
        uint16_t data;
    };
} DEV_MP2764_REG_45;

/* Register 0x46 This register indicates VSYS ADC result by per cell. */
typedef struct {
    union {
        struct {
        	uint16_t vsys    :10; /* ADC results of system voltage.
    							 	 Range: 0V to 6393mV/cell
    								 LSB: 6.25mV/cell
    								 VSYS = DEC(bits[9:0]) x 6.25 (mV) x BAT_NUM */
        	uint16_t resv    :6;
        } PACK_STRUCT f;
        uint16_t data;
    };
} DEV_MP2764_REG_46;

/* Register 0x47 This register indicates charge current ADC result. */
typedef struct {
    union {
        struct {
        	uint16_t ichg    :12; /* ADC results of charge current. (IBATT sense gain is 0.1V/A)
    								 Range: 0A to 14A
    								 LSB: 3.90625mA
    								 ICHG = DEC(bits[11:0]) x 3.9025 (mA) */
        	uint16_t resv    :4;
        } PACK_STRUCT f;
        uint16_t data;
    };
} DEV_MP2764_REG_47;

/* Register 0x48 This register indicates junction temperature ADC result. */
typedef struct {
    union {
        struct {
        	uint16_t tj      :10; /* ADC results of junction temperature.
    								 Range: -33.9oC to 170.7oC
    								 LSB: 0.3989oC
    								 TJ=374.54-0.3989*DEC(bits[9:0]) (oC) */
        	uint16_t resv    :6;
        } PACK_STRUCT f;
        uint16_t data;
    };
} DEV_MP2764_REG_48;

/* Register 0x49 This register indicates output voltage ADC result in source mode. */
typedef struct {
    union {
        struct {
        	uint16_t vin_src :10; /* ADC results of source output voltage.
    								 Range: 0V to 25.5 V
    								 LSB: 25mV
    								 IIN_SRC = DEC(bits[9:0]) x 25 (mV) */
        	uint16_t resv    :6;
        } PACK_STRUCT f;
        uint16_t data;
    };
} DEV_MP2764_REG_49;

/* Register 0x4A This register indicates output voltage ADC result in source mode. */
typedef struct {
    union {
        struct {
        	uint16_t iin_src :10; /* ADC results of source output current.
    								 Range: 0A to 7 A
    								 LSB: 7.8125mA
    								 IIN_SRC = DEC(bits[9:0]) x 7.8125 (mA) */
        	uint16_t resv    :6;
        } PACK_STRUCT f;
        uint16_t data;
    };
} DEV_MP2764_REG_4A;

/* Register 0x4B This register indicates battery discharge current in supplement mode or source mode. */
typedef struct {
    union {
        struct {
        	uint16_t ibatt_dsg :10; /* ADC results of battery discharge current.
    								   Range: 0A to 25.5 A
    								   LSB: 15.625mA
    								   IBATT_DSG = DEC(bits[9:0]) x 15.625 (mA) */
    		uint16_t resv      :6;
        } PACK_STRUCT f;
        uint16_t data;
    };
} DEV_MP2764_REG_4B;

#pragma pack(pop)

extern DEV_CHARGER_REG_ITEM *_s_chgBuf[MAX_CHARGER_SUPPORTED];
extern DEV_CHARGER_REG_ITEM *_s_platform_chgBufTable[MAX_CHARGER_SUPPORTED];

/**
 * @brief Find reg from platform and default reg table.
 *
 * @param chgId Index of charger.
 * @param reg reg to find.
 *
 * @retval reg item find in table or NULL when not fonud.
 */
DEV_CHARGER_REG_ITEM *amd_crb_drivers_mp2764_findRegItem(uint8_t chgId, uint8_t reg);

/**
 * @brief Register platform charger.
 *
 * @param chgId Index of the charger.
 * @param pTable charger tabble pointer.
 * @param chgBufTable platform charger table.
 */
void amd_crb_drivers_mp2764_regPlatTable(uint8_t chgId, DEV_CHARGER_REG_ITEM *pTable, DEV_CHARGER_REG_ITEM **chgBufTable);
/* ****************************************************************
 *
 * Thread-safe functions. Non-reenterable !!
 *
 * bool dev_isl9241_i32CacheGet(uint8_t reg, void * pVal);
 * bool dev_isl9241_i32Read(uint8_t reg, void * pVal);
 * bool dev_isl9241_i32Write(uint8_t reg, int32_t val);
 *
 * ****************************************************************/

/**
 * @brief Get charger register value from cache.
 *
 * @param chgId Index of the charger.
 * @param reg Charger reg.
 * @param pVal Address of the variable to save value read from cache.
 *
 * @retval true is successful.
 */
bool amd_crb_drivers_mp2764_i32CacheGet(uint8_t chgId, uint8_t reg, void *pVal);

/**
 * @brief Get charger register value from charger reg.
 *
 * @param chgId Index of the charger.
 * @param reg Charger reg.
 * @param pVal Address of the variable to save value read from cache.
 *
 * @retval true is success.
 */
bool amd_crb_drivers_mp2764_i32Read(uint8_t chgId, uint8_t reg, void *pVal);

/**
 * @brief Write value to charger register.
 *
 * @param chgId Index of the charger.
 * @param reg Charger reg.
 * @param pVal Target value.
 *
 * @retval true is success.
 */
bool amd_crb_drivers_mp2764_i32Write(uint8_t chgId, uint8_t reg, int32_t val);

/**
 * @brief Refresh charger reg in the charger buffer.
 *
 * @param reg Charger reg.
 * @param pBuf Charger buffer pointer.
 */
void amd_crb_drivers_mp2764_regRefresh(uint8_t *reg, void *pBuf);

/**
 * @brief Read charger reg item from specific i2c port and address.
 *
 * @param pItem Charger reg item.
 * @param port Charger i2c port.
 * @param slaveAddr Charger i2c slave address.
 *
 * @retval false if fail.
 * @retval dataAvailable flag.
 */
bool amd_crb_drivers_mp2764_autoRead(DEV_CHARGER_REG_ITEM *pItem, uint8_t port, uint8_t slaveAddr);

/**
 * @brief Modify charger reg with mask.
 *
 * @param chgId Index of the charger.
 * @param reg Charger reg.
 * @param val Value want to write.
 * @param msk Bit mask.
 * 
 * @retval True if success.
 */
bool amd_crb_drivers_mp2764_regRMW(uint8_t chgId, uint8_t reg, uint16_t val, uint16_t msk);

/**
 * @brief Enable charger.
 *
 * Set F_CHG_REG_MinSystemVoltage to non-zero
 * 
 * @param chgId Index of the charger.
 * 
 * @retval True if success.
 */
bool amd_crb_drivers_mp2764_chgEnable(uint8_t chgId);

/**
 * @brief Return charge enable status.
 * 
 * @param chgId Index of the charger.
 * 
 * @retval True if charging ICC is non-zero.
 */
bool amd_crb_drivers_mp2764_chgDisable(uint8_t chgId);

/**
 * @brief Set BGATE status.
 * 
 * @param chgId Index of the charger.
 * @param turnOn Target BGATE status. (Disable battery charging and discharging)
 * 
 * @retval True if success.
 */
bool amd_crb_drivers_mp2764_setBgate(uint8_t chgId, bool turnOn);

/**
 * @brief Set charger Vin voltage.
 *
 * amd_crb_drivers_mp2764_VinMinVoltage
 *
 * @param chgId Index of the charger.
 * @param VinVoltage Target VinVoltage.
 *
 * @retval True if success.
 */
bool amd_crb_drivers_mp2764_VinMinVoltage(uint8_t chgId, uint32_t VinVoltage);

/**
 * @brief Set charger Sys voltage.
 *
 * amd_crb_drivers_mp2764_SysMinVoltage
 *
 * @param chgId Index of the charger.
 * @param VinVoltage Target SysVoltage.
 *
 * @retval True if success.
 */
bool amd_crb_drivers_mp2764_SysMinVoltage(uint8_t chgId, uint32_t SysVoltage);

/**
 * @brief Set adapter CurrentLimit1.
 * 
 * @param chgId Index of the charger.
 * @param u16limit Target CurrentLimit1.
 * 
 * @retval True if success.
 */
bool amd_crb_drivers_mp2764_AcLimit1(uint8_t chgId, uint16_t u16limit);

/**
 * @brief Set adapter CurrentLimit2.
 * 
 * @param chgId Index of the charger.
 * @param u16limit Target CurrentLimit2.
 * 
 * @retval True if success.
 */
bool amd_crb_drivers_mp2764_AcLimit2(uint8_t chgId, uint16_t u16limit);

/**
 * @brief Set adapter ACProchot.
 * 
 * @param chgId Index of the charger.
 * @param enable enable the prochot
 * @param enable the aclimit 150% or it is 110%
 * 
 * @retval True if success.
 */
bool amd_crb_drivers_mp2764_AdapterACProchotL(uint8_t chgId, bool enable, bool iin_phot2);

/**
 * @brief Set adapter DCProchot.
 * 
 * @param chgId Index of the charger.
 * @param u16limit Target DCProchot threshold.
 * 
 * @retval True if success.
 */
bool amd_crb_drivers_mp2764_AdapterDCProchotL(uint8_t chgId, EM_DEV_MP2764_PROCHOT_DC_THRESHOLD current);

/**
 * @brief Set charger prochot debounce.
 * 
 * @param chgId Index of the charger.
 * @param debounce enum of debounce time.
 * 
 * @retval True if success.
 */
bool amd_crb_drivers_mp2764_SetProchotDebounce(uint8_t chgId, EM_DEV_MP2764_PROCHOT_DEGL_IIN_PHOT1 debounceAc1,
		                                                      EM_DEV_MP2764_PROCHOT_DEGL_IIN_PHOT2 debounceAc2,
															  EM_DEV_MP2764_PROCHOT_DC_DEBOUNCE debounceDc);

/**
 * @brief Set charger prochot duration.
 * 
 * @param chgId Index of the charger.
 * @param debounce enum of debounce time.
 * 
 * @retval True if success.
 */
bool amd_crb_drivers_mp2764_SetProchotDuration(uint8_t chgId, EM_DEV_MP2764_PROCHOT_DURATION duration);

/**
 * @brief Set charger full battery voltage.
 *
 * @param chgId Index of the charger.
 * @param full voltage per cell
 *
 * @retval True if success.
 */
bool amd_crb_drivers_mp2764_fullChargeVoltage(uint8_t chgId, uint32_t fullVoltage);

/**
 * @brief Set charger battery charging current.
 *
 * @param chgId Index of the charger.
 * @param charging current
 *
 * @retval True if success.
 */
bool amd_crb_drivers_mp2764_chargeCurrent(uint8_t chgId, uint16_t current);

/**
 * @brief mask the interrupt event
 *
 * @param [in]   chgId;      charger index
 * @param [in]   mask;       mask interrupt event
 *
 * @retval True if success.
 */
bool amd_crb_drivers_mp2764_interruptMask(uint8_t chgId, uint16_t mask);

/**
 * @brief mask the interrupt event
 *
 * @param [in]   chgId;      charger index
 * @param [in]   status;     fault status
 *
 * @retval True if success.
 */
bool amd_crb_drivers_mp2764_faultStatus(uint8_t chgId, uint32_t *status);

/**
 * @brief Set charger battery-only mode.
 *
 * @param chgId Index of the charger.
 * @param enable Enable status.
 */
bool amd_crb_drivers_mp2764_batteryOnly(uint8_t chgId, bool enable);

typedef void (*pfnSetupProchotGate_t)(void);

/**
 * @brief Register set prochot handler.
 * 
 * @param handler Set prochot handler pointer.
 */
void amd_crb_drivers_mp2764_prochot_register(pfnSetupProchotGate_t handler);

/**
 * @brief Trigger set prochot handler.
 */
void amd_crb_drivers_mp2764_setProchot(void);
#endif // end of __AMD_CRB_DRIVERS_MP2764_H__
