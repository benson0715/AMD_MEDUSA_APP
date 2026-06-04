/*****************************************************************************
 * Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
 ******************************************************************************
 */
#include "board_ioexp.h"
#include "board_id.h"
#include "dev_utility.h"
#include "f_nv_options.h"
#include "dev_pca9535.h"
#include "board_config.h"
#include "board_smtbty.h"

/**************************** *
 *     global valuable        *
 * ************************** */
uint8_t g_S5AlwEnFlag = 0;

/**
 * @brief Init the IOEXP when it is powered on. (power rail in ALW_3V3)
 *        Need to call in the power sequence after S5 power rail ready
 */
void board_ioexp_initIoExp()
{
	/*
	* U5100, Slv_0x21, I2C_1
	*
	*        NetName                    BufType  POR_Def  Comment
	* IO_00  EP_TPM_PWREN               OD       0        EC to enable TPM power, high active
	* IO_01  EP_USBA_PWREN              O        1        Load Switch EN for all USBA port. Enable=1 (Default); OFF=0.
	* IO_02  EP_CODEC_DEPOP_MUTE#       O        0        To mute codec POP noise
	* IO_03  EP_HDR_DEPOP_MUTE#         O        0        To mute plugged codec module POP noise
	* IO_04  EP_SD_AUX_PWREN            OD       0        EC to enable SD controller power, high active
	* IO_05  EP_SD_MAIN_PWREN           OD       0        EC to control SD S0 domain power enable
	* IO_06  EP_SSD0_PWREN              OD       0        EC to enable M.2 SSD0 power, high active
	* IO_07  EP_SSD1_PWREN              OD       0        EC to enable M.2 SSD0 power, high active
	* IO_08  EP_RTL_ISOLATE#            OD       1        Reserved for PCIE link ISOLATE.  Low active.
	* IO_09  EP_LOM_S5_PWREN            OD       0        EC to enable LOM power, high active
	* IO_10  EP_WLAN_1V8_PWREN          OD       1        Reserved to enable WLAN 1V8 power separately, high active
	* IO_11  EP_WLAN_3V3_PWREN          OD       0        EC to enable WLAN power, high active; 
	* IO_12  EP_WLAN_RADIO_DIS          O        0        To disable WLAN's raido, high active.  High-disable; 
	* IO_13  EP_WLAN_WIFI_RST           O        0        To reset WLAN module, high active
	* IO_14  EP_BT_RADIO_DIS            O        0        To disable Bluetooth radio, high active
	* IO_15  EP_M2_BT_WAKE              O        0        EC to wake Blue Tooth module. high active
	*/
    dev_pca9535_init( IOEXP_I2C_ADDR_U5100_I2C1_0x21,
	                  IOEXP_I2C_PORT_U5100_I2C1_0x21,
	                  IOEXP_MASK_O___U5100_I2C1_0x21,
	                  IOEXP_MASK_OD__U5100_I2C1_0x21,
	                  IOEXP_MASK_POR_U5100_I2C1_0x21,
					  IOEXP_MASK_INT_U5100_I2C1_0x21);

	/*
	* U5101, Slv_0x22, I2C_1
	*
	*        NetName                    BufType  POR_Def  Comment
	* IO_00  EP_FPR_LOCK#               OD       1        Low- Finger print enters low power state, High - for normal operation
	* IO_01  EP_FPR_OFF#                OD       1        High to enable wake up function, low to disable wake up function
	* IO_02  EP_FPR_PWREN               OD       0        EC to enable Fingerprint power, high active
	* IO_03  EP_FPR_GPIO                OD       1        Reserved for Fingerprint side band control
	* IO_04  EP_TPAD_DIS#               OD       1        To disable touch pad, low active
	* IO_05  EP_TPAD_FW_SEC#            O        0        for reprogramming the FW image on the touchpad,default low, high active
	* IO_06  EP_TPNL_EN                 OD       0        EC to enable Touch panel function, high active
	* IO_07  EP_TPNL_PWREN              OD       0        EC to enable Touch panel power, high active
	* IO_08  EP_USBCAM_FW_WP#           O        0        Camera FW update write protect, low active, low for normal operation
	* IO_09  EP_USBCAM_PRIV             OD       1        Reserved for USBCAM. 
	* IO_10  EP_USBCAM_PWREN            O        0        EC to enable USB CAM power, high active
	* IO_11  EP_WWAN_PWREN              OD       0        EC to enable WWAN power, high active
	* IO_12  EP_WWAN_CARD_PWROFF#       O        0        To control WWAN power on/off;  High - pwr on ; Low - pwr off
	* IO_13  EP_WWAN_MODULE_RST#        O        0        To reset WWAN module, low active
	* IO_14  EP_GNSS_RADIO_DIS#         O        1        To disable WWAN's GNSS raido, low active
	* IO_15  EP_MAIN_RADIO_DIS#         O        1        To disable WWAN's radio, low active
	*/
    dev_pca9535_init( IOEXP_I2C_ADDR_U5101_I2C1_0x22,
	                  IOEXP_I2C_PORT_U5101_I2C1_0x22,
	                  IOEXP_MASK_O___U5101_I2C1_0x22,
	                  IOEXP_MASK_OD__U5101_I2C1_0x22,
	                  IOEXP_MASK_POR_U5101_I2C1_0x22,
					  IOEXP_MASK_INT_U5101_I2C1_0x22 );

	/*
	* U5102, Slv_0x23, I2C_1
	*
	*        NetName                    BufType  POR_Def  Comment
	* IO_00  EP_USBMUX_BT#_TPNL_SEL     O        0        EC to select USB6 routing, low-BT, high--TPNL
	* IO_01  EP_USBMUX_FPR#_GBE_SEL     O        1        EC to select USB5 routing, low-Fingerprint, high--GbE
	* IO_02  EP_WLAN#_SD_SEL            OD       1        EC to select PCIE routing, low- WLAN, high - SD.
	* IO_03  EP_WWAN#_LOM_SEL           OD       1        EC to select PCIE routing, low- WWAN, high - LOM
	* IO_04  EP_UART0_WLAN#_HDR_SEL     OD       1        EC to select APU UART0 routing, low- WLAN, hihg - Header
	* IO_05  EP_AUD_SW#_HDR_SEL         O        0        EC to select Audio routing, low - SW Codec, high - Header
	* IO_06  EP_EVAL_SMB_APU#_EC_SEL    OD       0        EC to select Eval SMBus routing, low - APU SMB1, high - EC I2C1
	* IO_07  EP_WAIIE#_MP2_SEL          O        0        To control I2C for PWR sensor switch, low--WALLE, devices, high--APU MP2
	* IO_08  EP_SMB0_BUFF_EN            OD       1        EC to enable SMBus0 buffer, high active
	* IO_09  EP_SMB1_BUFF_EN            OD       1        EC to enable SMBus1 buffer, high active
	* IO_10  EP_USBC_RT_SMB_BUFF_EN     O        0        EC to enable Re-timer I2C0/SMB1 buffer. Low- disable; high- enable for RT FW Update
	* IO_11  EP_TPAD_I2C_BUFF_EN        OD       1        EC to control TPAD I2C buffer enable, high active
	* IO_12  EP_TPNL_I2C_BUFF_EN        OD       1        EC to control TPNL I2C buffer enable, high active
	* IO_13  EP_NFC_I2C_BUFF_EN         OD       1        EC to control NFC I2C buffer enable, high active
	* IO_14  EP_SFH_I2C_BUFF_GATE#      OD       1        EC control to gate SFH 3V3 route path, low active
	* IO_15  EP_ESP32_I2C_EN#           O        1        EC to control E3 on I2C0 buffer enable, low active
	*/
    dev_pca9535_init( IOEXP_I2C_ADDR_U5102_I2C1_0x23,
	                  IOEXP_I2C_PORT_U5102_I2C1_0x23,
	                  IOEXP_MASK_O___U5102_I2C1_0x23,
	                  IOEXP_MASK_OD__U5102_I2C1_0x23,
	                  IOEXP_MASK_POR_U5102_I2C1_0x23,
					  IOEXP_MASK_INT_U5102_I2C1_0x23  );

	/*
	* U5103, Slv_0x25, I2C_1
	*
	*        NetName                    BufType  POR_Def  Comment
	* IO_00  EP_BAT_LED#                OD       1        Battery LED
	* IO_01  EP_KBC_LOW_BAT#            OD       1        Reserved to notify APU, to enter low battary mode, low active
	* IO_02  EP_ADAPTER_OFF             O        0        To gate adapter power source, high active
	* IO_04  EP_SB_TSI_SEL              O        1        To select SB-TSI routing. High- EC I2C0; low - ESP32
	* IO_05  EP_APU_PCC#                O        1        Reserved for APU PCC function, low active
	* IO_06  EP_DP0_HPD                 OD       1        Reserved to drive this signal High/Low to mimic Plug-in/ Plug-out
	* IO_07  EP_DP1_HPD                 OD       1        Reserved to drive this signal High/Low to mimic Plug-in/ Plug-out
	* IO_08  EP_EDP_SMUX_PWM_EN         O        0        Smart MUX PWM control
	* IO_09  EP_EDP_SMUX_BLON           O        0        Smart MUX back light on
	* IO_10  EP_EDP_SMUX_PNL_RST#       O        0        Smart MUX panel reset
	* IO_11  EP_EDP_SMUX_PWMMUX_SEL     O        0        Smart MUX PWM mux control
	* IO_15  EP_JTAG_SEL#               OD       1        EC to control APU JTAG path to UDC, low active
	*/
    dev_pca9535_init( IOEXP_I2C_ADDR_U5103_I2C1_0x25,
	                  IOEXP_I2C_PORT_U5103_I2C1_0x25,
	                  IOEXP_MASK_O___U5103_I2C1_0x25,
	                  IOEXP_MASK_OD__U5103_I2C1_0x25,
	                  IOEXP_MASK_POR_U5103_I2C1_0x25,
					  IOEXP_MASK_INT_U5103_I2C1_0x25 );

	/*
	* U5104, Slv_0x26, I2C_1
	*
	*        NetName                    BufType  POR_Def  Comment
	* IO_00  EP_MEM_SEL                 O        1        MEM VDD power selection for different speed, high - 8533, low - 9600.  
	* IO_01  EP_AUD_DMIC#_HDR_SEL       O        0        EC to select Audio DMIC routing, low - DMIC header,  high - I2S header
	* IO_02  DIS_DASH#                  OD       1        Reserved for DASH enable or disable,  low - Dash disable  ; high - Dash enable
	* IO_03  EC_EFFP_MODULE_RESET_N     OD       1        Reserved for eFFP module debug only, TBD
	* IO_07  TPM_OB#_HDR_SEL            O        0        EC to select TPM SPI routing , low- onboard TPM,  high- TPM header
	*/
    dev_pca9535_init( IOEXP_I2C_ADDR_U5104_I2C1_0x26,
						IOEXP_I2C_PORT_U5104_I2C1_0x26,
						IOEXP_MASK_O___U5104_I2C1_0x26,
						IOEXP_MASK_OD__U5104_I2C1_0x26,
						IOEXP_MASK_POR_U5104_I2C1_0x26,
						IOEXP_MASK_INT_U5104_I2C1_0x26 );

}

/**
 * @brief Init the IOEXP when it is powered on. (power rail in EC_3V3)
 */
void board_ioexp_initIoExp0()
{
	/* XRA1201 is used, need to set the interrupt mask compared with PCA9535. Others are same */

    gpio_interrupt_configure_pin(EP_EC_INT_N, GPIO_INT_DISABLE); // disable interrupt of GPIO042 (IO_EXP0_INT_Handler)

	/*
	* U5105, Slv_0x27, I2C_3
	*
	*        NetName                    BufType  POR_Def  Comment
	* IO_00  HW_WWAN_RADIO#             I        x        Notify EC to toggle enable/disable WWAN radio, falling edge active
	* IO_01  HW_WIFI_RADIO#             I        x        Notify EC to toggle enable/disable WIFI radio, falling edge active
	* IO_02  DC_S5_PWRCTL               I        x        High: S5 POWER ON IN DC MODELow: S5 POWER OFF IN DC MODE(default)
	* IO_03  PD_FORCE_FW_UP#            I        x        Notify EC to force update PD FW when triggered in G3', falling edge activeNotify EC to toggle debug post code or Board basic info, falling edge active
	* IO_04  AC_330W_PRSNT#             I        x        Low when using 330W adaptor.
	* IO_05  FCH_DBG_BUS_EN             I        x        Notify EC that FCH debug mode is enabled, high active
	* IO_06  APU_EC_VDDCORE_LEDON       I        x        Reserved for Vcore LED indicator
	* IO_07  APU_EC_VDDCORE_HIGH        I        x        Reserved for Vcore LED indicator
	* IO_08  PSL_MODE_EN                I        x        High: EC PSL (VCI) mode Enable (default);Low: EC PSL (VCI) mode Disable; 
	* IO_09  EXT_TALERT#                I        x        To nofity EC, an overheating event is occurred. low active
	* IO_10  WLAN_IO33#_IO18_SEL        I        x        Notify EC the M.2 WLAN card I/O level. Low --3.3V level, High -- 1.8V level
	* IO_11  UDC_EP_DBG_CARD_PRSNT#     I        x        To notify EC, the UDC debug card is present. low active
	* IO_12  EC_UART_DOGL_PRSNT#        I        x        To notify EC, the UART dongle is present, low active
	* IO_13  WAIIE_EP_SENSOR_EN_STA#    I        x        Reserved to notify EC, the WALLE sensor power enable from system power. 
	* IO_14  EVAL_NEW#_LEGACY           I        x        Low: Plugged EVAL card is a new card ; High: Plugged EVAL card is a legacy card;
	*/
    dev_pca9535_init( IOEXP_I2C_ADDR_U5105_I2C3_0x27,
	                  IOEXP_I2C_PORT_U5105_I2C3_0x27,
	                  IOEXP_MASK_O___U5105_I2C3_0x27,
	                  IOEXP_MASK_OD__U5105_I2C3_0x27,
	                  IOEXP_MASK_POR_U5105_I2C3_0x27,
					  IOEXP_MASK_INT_U5105_I2C3_0x27);

	// Read initial status of DC_S5_ALW_EN_CTRL
	g_S5AlwEnFlag = ioexp_get(ex_DC_S5_PWRCTL);

	// set the interrupt mask
	dev_pca9535_intEnable(IOEXP_I2C_ADDR_U5105_I2C3_0x27, IOEXP_I2C_PORT_U5105_I2C3_0x27);
}

/**
 * @brief ACPI ECRAM callback handler for IOEXP0 (8 ioexp pins)
 *
 * @param     isRead:     True is read and false is write
 * @param     ui8Idx:     The ECRAM index
 * @param     pui8Data:   Read and write data buffer
 */
void board_ioexp_AcpiHandler_IOExp0 (uint8_t isRead, uint8_t ui8Idx, uint8_t * pui8Data)
{
    uint16_t retVal = 0;

	if (isRead) {
		retVal = dev_pca9535_smartRead(IOEXP_I2C_ADDR_U5103_I2C1_0x25, IOEXP_I2C_PORT_U5103_I2C1_0x25, 0x00FF);
		if (pui8Data) *pui8Data = retVal & 0xFF;
	} else if (!pui8Data){
		return;
	} else {
		retVal = *pui8Data;
		dev_pca9535_setOutput(IOEXP_I2C_ADDR_U5103_I2C1_0x25, IOEXP_I2C_PORT_U5103_I2C1_0x25, 0x00FF, retVal);
	}
}

/**
 * @brief ACPI ECRAM callback handler for IOEXP1 (8 ioexp pins)
 *
 * @param     isRead:     True is read and false is write
 * @param     ui8Idx:     The ECRAM index
 * @param     pui8Data:   Read and write data buffer
 */
void board_ioexp_AcpiHandler_IOExp1 (uint8_t isRead, uint8_t ui8Idx, uint8_t * pui8Data)
{
	static uint16_t retVal = 0;
	if (isRead) {
		retVal = dev_pca9535_smartRead(IOEXP_I2C_ADDR_U5103_I2C1_0x25, IOEXP_I2C_PORT_U5103_I2C1_0x25, 0xFF00);
		if (pui8Data) *pui8Data = retVal >> 8;
	} else if (!pui8Data){
		return;
	} else {
		retVal = ((uint16_t)(*pui8Data)) << 8;
		dev_pca9535_setOutput(IOEXP_I2C_ADDR_U5103_I2C1_0x25, IOEXP_I2C_PORT_U5103_I2C1_0x25, 0xFF00, retVal);
	}
}

/**
 * @brief ACPI ECRAM callback handler for IOEXP2 (8 ioexp pins)
 *
 * @param     isRead:     True is read and false is write
 * @param     ui8Idx:     The ECRAM index
 * @param     pui8Data:   Read and write data buffer
 */
void board_ioexp_AcpiHandler_IOExp2 (uint8_t isRead, uint8_t ui8Idx, uint8_t * pui8Data)
{
	uint16_t retVal = 0;

	if (isRead) {
		retVal = dev_pca9535_smartRead(IOEXP_I2C_ADDR_U5100_I2C1_0x21, IOEXP_I2C_PORT_U5100_I2C1_0x21, 0x00FF);
		if (pui8Data) *pui8Data = retVal & 0xFF;
	} else if (!pui8Data){
		return;
	} else {
		retVal = *pui8Data;
		dev_pca9535_setOutput(IOEXP_I2C_ADDR_U5100_I2C1_0x21, IOEXP_I2C_PORT_U5100_I2C1_0x21, 0x00FF, retVal);
	}
}

/**
 * @brief ACPI ECRAM callback handler for IOEXP3 (8 ioexp pins)
 *
 * @param     isRead:     True is read and false is write
 * @param     ui8Idx:     The ECRAM index
 * @param     pui8Data:   Read and write data buffer
 */
void board_ioexp_AcpiHandler_IOExp3 (uint8_t isRead, uint8_t ui8Idx, uint8_t * pui8Data)
{
	uint16_t retVal = 0;

	if (isRead) {
		retVal = dev_pca9535_smartRead(IOEXP_I2C_ADDR_U5100_I2C1_0x21, IOEXP_I2C_PORT_U5100_I2C1_0x21, 0xFF00);
		if (pui8Data) *pui8Data = retVal >> 8;
		} else if (!pui8Data){
		return;
	} else {
		retVal = ((uint16_t)(*pui8Data)) << 8;
		dev_pca9535_setOutput(IOEXP_I2C_ADDR_U5100_I2C1_0x21, IOEXP_I2C_PORT_U5100_I2C1_0x21, 0xFF00, retVal);
	}
}

/**
 * @brief ACPI ECRAM callback handler for IOEXP4 (8 ioexp pins)
 *
 * @param     isRead:     True is read and false is write
 * @param     ui8Idx:     The ECRAM index
 * @param     pui8Data:   Read and write data buffer
 */
void board_ioexp_AcpiHandler_IOExp4 (uint8_t isRead, uint8_t ui8Idx, uint8_t * pui8Data)
{
	uint16_t retVal = 0;

	if (isRead) {
		retVal = dev_pca9535_bufferRead(IOEXP_I2C_ADDR_U5105_I2C3_0x27, IOEXP_I2C_PORT_U5105_I2C3_0x27, 0x00FF);
		if (pui8Data) *pui8Data = retVal & 0xFF;
	} else if (!pui8Data){
		return;
	} else {
		retVal = *pui8Data;
		dev_pca9535_setOutput(IOEXP_I2C_ADDR_U5105_I2C3_0x27, IOEXP_I2C_PORT_U5105_I2C3_0x27, 0x00FF, retVal);
	}
}

/**
 * @brief ACPI ECRAM callback handler for IOEXP5 (8 ioexp pins)
 *
 * @param     isRead:     True is read and false is write
 * @param     ui8Idx:     The ECRAM index
 * @param     pui8Data:   Read and write data buffer
 */
void board_ioexp_AcpiHandler_IOExp5 (uint8_t isRead, uint8_t ui8Idx, uint8_t * pui8Data)
{
	uint16_t retVal = 0;

	if (isRead) {
		retVal = dev_pca9535_bufferRead(IOEXP_I2C_ADDR_U5105_I2C3_0x27, IOEXP_I2C_PORT_U5105_I2C3_0x27, 0xFF00);
		if (pui8Data) *pui8Data = retVal >> 8;
	} else if (!pui8Data){
		return;
	} else {
		retVal = ((uint16_t)(*pui8Data)) << 8;
		dev_pca9535_setOutput(IOEXP_I2C_ADDR_U5105_I2C3_0x27, IOEXP_I2C_PORT_U5105_I2C3_0x27, 0xFF00, retVal);
	}
}

/**
 * @brief ACPI ECRAM callback handler for IOEXP6 (8 ioexp pins)
 *
 * @param     isRead:     True is read and false is write
 * @param     ui8Idx:     The ECRAM index
 * @param     pui8Data:   Read and write data buffer
 */
void board_ioexp_AcpiHandler_IOExp6 (uint8_t isRead, uint8_t ui8Idx, uint8_t * pui8Data)
{
	uint16_t retVal = 0;

	if (isRead) {
	retVal = dev_pca9535_smartRead(IOEXP_I2C_ADDR_U5101_I2C1_0x22, IOEXP_I2C_PORT_U5101_I2C1_0x22, 0x00FF);
		if (pui8Data) *pui8Data = retVal & 0xFF;
	} else if (!pui8Data){
		return;
	} else {
		retVal = *pui8Data;
	dev_pca9535_setOutput(IOEXP_I2C_ADDR_U5101_I2C1_0x22, IOEXP_I2C_PORT_U5101_I2C1_0x22, 0x00FF, retVal);
	}
}

/**
 * @brief ACPI ECRAM callback handler for IOEXP7 (8 ioexp pins)
 *
 * @param     isRead:     True is read and false is write
 * @param     ui8Idx:     The ECRAM index
 * @param     pui8Data:   Read and write data buffer
 */
void board_ioexp_AcpiHandler_IOExp7 (uint8_t isRead, uint8_t ui8Idx, uint8_t * pui8Data)
{
	uint16_t retVal = 0;

	if (isRead) {
		retVal = dev_pca9535_smartRead(IOEXP_I2C_ADDR_U5101_I2C1_0x22, IOEXP_I2C_PORT_U5101_I2C1_0x22, 0xFF00);
		if (pui8Data) *pui8Data = retVal >> 8;
	} else if (!pui8Data){
	return;
	} else {
		retVal = ((uint16_t)(*pui8Data)) << 8;
		dev_pca9535_setOutput(IOEXP_I2C_ADDR_U5101_I2C1_0x22, IOEXP_I2C_PORT_U5101_I2C1_0x22, 0xFF00, retVal);
	}
}

/**
 * @brief ACPI ECRAM callback handler for IOEXP8 (8 ioexp pins)
 *
 * @param     isRead:     True is read and false is write
 * @param     ui8Idx:     The ECRAM index
 * @param     pui8Data:   Read and write data buffer
 */
void board_ioexp_AcpiHandler_IOExp8 (uint8_t isRead, uint8_t ui8Idx, uint8_t * pui8Data)
{
	uint16_t retVal = 0;

	if (isRead) {
		retVal = dev_pca9535_smartRead(IOEXP_I2C_ADDR_U5104_I2C1_0x26, IOEXP_I2C_PORT_U5104_I2C1_0x26, 0x00FF);
		if (pui8Data) *pui8Data = retVal & 0xFF;
	} else if (!pui8Data){
		return;
	} else {
		retVal = *pui8Data;
		dev_pca9535_setOutput(IOEXP_I2C_ADDR_U5104_I2C1_0x26, IOEXP_I2C_PORT_U5104_I2C1_0x26, 0x00FF, retVal);
	}
}

/**
 * @brief ACPI ECRAM callback handler for IOEXP9 (8 ioexp pins)
 *
 * @param     isRead:     True is read and false is write
 * @param     ui8Idx:     The ECRAM index
 * @param     pui8Data:   Read and write data buffer
 */
void board_ioexp_AcpiHandler_IOExp9 (uint8_t isRead, uint8_t ui8Idx, uint8_t * pui8Data)
{
	uint16_t retVal = 0;

	if (isRead) {
		retVal = dev_pca9535_smartRead(IOEXP_I2C_ADDR_U5104_I2C1_0x26, IOEXP_I2C_PORT_U5104_I2C1_0x26, 0xFF00);
		if (pui8Data) *pui8Data = retVal >> 8;
	} else if (!pui8Data){
		return;
	} else {
		retVal = ((uint16_t)(*pui8Data)) << 8;
		dev_pca9535_setOutput(IOEXP_I2C_ADDR_U5104_I2C1_0x26, IOEXP_I2C_PORT_U5104_I2C1_0x26, 0xFF00, retVal);
	}
}

/**
 * @brief ACPI ECRAM callback handler for IOEXP10 (8 ioexp pins)
 *
 * @param     isRead:     True is read and false is write
 * @param     ui8Idx:     The ECRAM index
 * @param     pui8Data:   Read and write data buffer
 */
void board_ioexp_AcpiHandler_IOExp10 (uint8_t isRead, uint8_t ui8Idx, uint8_t * pui8Data)
{
	uint16_t retVal = 0;

		if (isRead) {
				retVal = dev_pca9535_smartRead(IOEXP_I2C_ADDR_U5102_I2C1_0x23 & 0x7F, IOEXP_I2C_PORT_U5102_I2C1_0x23, 0x00FF);
				if (pui8Data) *pui8Data = retVal & 0xFF;
		} else if (!pui8Data){
				return;
		} else {
				retVal = *pui8Data;
				dev_pca9535_setOutput(IOEXP_I2C_ADDR_U5102_I2C1_0x23 & 0x7F, IOEXP_I2C_PORT_U5102_I2C1_0x23, 0x00FF, retVal);
		}
}

/**
 * @brief ACPI ECRAM callback handler for IOEXP11 (8 ioexp pins)
 *
 * @param     isRead:     True is read and false is write
 * @param     ui8Idx:     The ECRAM index
 * @param     pui8Data:   Read and write data buffer
 */
void board_ioexp_AcpiHandler_IOExp11 (uint8_t isRead, uint8_t ui8Idx, uint8_t * pui8Data)
{
	uint16_t retVal = 0;

	if (isRead) {
		retVal = dev_pca9535_smartRead(IOEXP_I2C_ADDR_U5102_I2C1_0x23, IOEXP_I2C_PORT_U5102_I2C1_0x23, 0xFF00);
		if (pui8Data) *pui8Data = retVal >> 8;
	} else if (!pui8Data){
		return;
	} else {
		retVal = ((uint16_t)(*pui8Data)) << 8;
		dev_pca9535_setOutput(IOEXP_I2C_ADDR_U5102_I2C1_0x23, IOEXP_I2C_PORT_U5102_I2C1_0x23, 0xFF00, retVal);
	}
}

/**
 * @brief ACPI ECRAM callback handler for IOEXP12 (8 ioexp pins)
 *
 * @param     isRead:     True is read and false is write
 * @param     ui8Idx:     The ECRAM index
 * @param     pui8Data:   Read and write data buffer
 */
void board_ioexp_AcpiHandler_IOExp12 (uint8_t isRead, uint8_t ui8Idx, uint8_t * pui8Data)
{

}

/**
 * @brief ACPI ECRAM callback handler for IOEXP13 (8 ioexp pins)
 *
 * @param     isRead:     True is read and false is write
 * @param     ui8Idx:     The ECRAM index
 * @param     pui8Data:   Read and write data buffer
 */
void board_ioexp_AcpiHandler_IOExp13 (uint8_t isRead, uint8_t ui8Idx, uint8_t * pui8Data)
{

}