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
	 *		  NetName					 BufType  POR_Def  Comment
	 * IO_00  EP_UART0_WLAN#_DBG_SEL	 O		  1 	   EC to select APU UART0 routing, low- WLAN, high - Serdes debug. 
	 * IO_02  EP_CODEC_DEPOP_MUTE#		 O		  0 	   To mute codec POP noise
	 * IO_04  EP_SMB0_BUFF_EN			 OD 	  1 	   EC to enable SMBus0 buffer (IPU), high active
	 * IO_05  EP_APU_PCC#				 O		  1 	   Reserved for APU PCC function, low active
	 * IO_06  EP_SSD0_PWREN 			 O		  0 	   EC to enable M.2 SSD0 power, high active
	 * IO_07  EP_HDMI_RD_PD 			 O		  0 	   HDMI RD power down. High active.  L for normal operation
	 * IO_08  EC_ESP32_RF_OFF			 O		  0 	   ESP32 RF off,  high active.	High-off, Low- on. 
	 * IO_09  EP_MEM_8533_9600#_SEL 	 O		  0 	   MEM VDD power selection for different speed, high - 8533, low - 9600.  
	 * IO_10  EP_WLAN_1V8_PWREN 		 O		  0 	   EC to enable WLAN 1V8 power separately, high active
	 * IO_11  EP_WLAN_3V3_PWREN 		 O		  0 	   EC to enable WLAN power, high active; 
	 * IO_12  EP_WLAN_RADIO_DIS 		 O		  0 	   To disable WLAN's raido, high active.  High-disable; 
	 * IO_13  EP_WLAN_WIFI_RST			 O		  0 	   To reset WLAN module, high active
	 * IO_14  EP_BT_RADIO_DIS			 O		  0 	   To disable Bluetooth radio, high active
	 * IO_15  EP_M2_BT_WAKE 			 O		  0 	   EC to wake Blue Tooth module. high active
	 */
	dev_pca9535_init( IOEXP_I2C_ADDR_U5100_I2C1_0x21,
	                  IOEXP_I2C_PORT_U5100_I2C1_0x21,
	                  IOEXP_MASK_O___U5100_I2C1_0x21,
	                  IOEXP_MASK_OD__U5100_I2C1_0x21,
	                  IOEXP_MASK_POR_U5100_I2C1_0x21,
	                  IOEXP_MASK_INT_U5100_I2C1_0x21);

	/*
	 * U5101, Slv_0x22, I2C_3
	 *
	 *		  NetName					 BufType  POR_Def  Comment
	 * IO_00  EP_USBA_PWREN 			 O		  1 	   Load Switch EN for all USBA port. Enable=1 (Default); OFF=0.
	 * IO_01  EP_SB_TSI_ESP32#_EC_SEL	 O		  1 	   To select SB-TSI routing. High- EC I2C0; low - ESP32
	 * IO_02  EP_FPR_PWREN				 O		  0 	   EC to enable Fingerprint power, high active
	 * IO_03  EP_TPM_PWREN				 O		  0 	   EC to enable TPM power, high active
	 * IO_04  EP_SENSOR_1V8_PWREN		 O		  0 	   EC to enable 1.8V SFH PWR load switch. High active
	 * IO_05  EP_SENSOR_3V3_PWREN		 O		  0 	   EC to enable 3.3V SFH PWR load switch. High active
	 * IO_06  EP_TPAD_PWREN 			 O		  0 	   EC to enable TPAD PWR load switch. High active
	 * IO_07  EP_TPNL_PWREN 			 O		  0 	   EC to enable Touch panel power, high active
	 * IO_08  EP_USBCAM_FW_WP#			 O		  0 	   Camera FW update write protect, low active, low for normal operation
	 * IO_09  EP_USBCAM_PRIV			 O		  0 	   USBCAM Privacy. High active.
	 * IO_10  EP_USBCAM_PWREN			 O		  0 	   EC to enable USB CAM power, high active
	 * IO_11  EP_USBMUX_USB20#_EUSB2V1_SEL	O		 0		  Drive it low when detect 
	 * IO_12  EP_ESP32_I2C_EN#			 O		  0 	   EC to control E3 on I2C0 buffer enable, low active
	 * IO_13  EP_HDMI_RD_PWREN			 O		  0 	   To enable HDMI RD PWR load switch.  High active
	 * IO_14  EP_DMIC_PWREN 			 O		  0 	   To enbale CAM DMIC PWR load switch. High active
	 * IO_15  EP_USBC_RT_SMB_BUFF_EN	 O		  0 	   EC to enable Re-timer I2C0/SMB1 buffer. Low- disable; high- enable for RT FW Update
	 */
	dev_pca9535_init( IOEXP_I2C_ADDR_U5101_I2C3_0x22,
	                  IOEXP_I2C_PORT_U5101_I2C3_0x22,
	                  IOEXP_MASK_O___U5101_I2C3_0x22,
	                  IOEXP_MASK_OD__U5101_I2C3_0x22,
	                  IOEXP_MASK_POR_U5101_I2C3_0x22,
	                  IOEXP_MASK_INT_U5101_I2C3_0x22);
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
 * @brief ACPI ECRAM callback handler for IOEXP1 (8 ioexp pins)
 *
 * @param     isRead:     True is read and false is write
 * @param     ui8Idx:     The ECRAM index
 * @param     pui8Data:   Read and write data buffer
 */
void board_ioexp_AcpiHandler_IOExp1 (uint8_t isRead, uint8_t ui8Idx, uint8_t * pui8Data)
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
		retVal = dev_pca9535_bufferRead(IOEXP_I2C_ADDR_U5101_I2C3_0x22, IOEXP_I2C_PORT_U5101_I2C3_0x22, 0x00FF);
		if (pui8Data) *pui8Data = retVal & 0xFF;
	} else if (!pui8Data){
		return;
	} else {
		retVal = *pui8Data;
		dev_pca9535_setOutput(IOEXP_I2C_ADDR_U5101_I2C3_0x22, IOEXP_I2C_PORT_U5101_I2C3_0x22, 0x00FF, retVal);
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
		retVal = dev_pca9535_bufferRead(IOEXP_I2C_ADDR_U5101_I2C3_0x22, IOEXP_I2C_PORT_U5101_I2C3_0x22, 0xFF00);
		if (pui8Data) *pui8Data = retVal >> 8;
	} else if (!pui8Data){
		return;
	} else {
		retVal = ((uint16_t)(*pui8Data)) << 8;
		dev_pca9535_setOutput(IOEXP_I2C_ADDR_U5101_I2C3_0x22, IOEXP_I2C_PORT_U5101_I2C3_0x22, 0xFF00, retVal);
	}
}

/**
 * @brief ACPI ECRAM callback handler for IOEXP4 (8 ioexp pins) - stub
 *
 * @param     isRead:     True is read and false is write
 * @param     ui8Idx:     The ECRAM index
 * @param     pui8Data:   Read and write data buffer
 */
void board_ioexp_AcpiHandler_IOExp4 (uint8_t isRead, uint8_t ui8Idx, uint8_t * pui8Data){}

/**
 * @brief ACPI ECRAM callback handler for IOEXP5 (8 ioexp pins) - stub
 *
 * @param     isRead:     True is read and false is write
 * @param     ui8Idx:     The ECRAM index
 * @param     pui8Data:   Read and write data buffer
 */
void board_ioexp_AcpiHandler_IOExp5 (uint8_t isRead, uint8_t ui8Idx, uint8_t * pui8Data){}

/**
 * @brief ACPI ECRAM callback handler for IOEXP6 (8 ioexp pins) - stub
 *
 * @param     isRead:     True is read and false is write
 * @param     ui8Idx:     The ECRAM index
 * @param     pui8Data:   Read and write data buffer
 */
void board_ioexp_AcpiHandler_IOExp6 (uint8_t isRead, uint8_t ui8Idx, uint8_t * pui8Data){}

/**
 * @brief ACPI ECRAM callback handler for IOEXP7 (8 ioexp pins) - stub
 *
 * @param     isRead:     True is read and false is write
 * @param     ui8Idx:     The ECRAM index
 * @param     pui8Data:   Read and write data buffer
 */
void board_ioexp_AcpiHandler_IOExp7 (uint8_t isRead, uint8_t ui8Idx, uint8_t * pui8Data){}

/**
 * @brief ACPI ECRAM callback handler for IOEXP8 (8 ioexp pins) - stub
 *
 * @param     isRead:     True is read and false is write
 * @param     ui8Idx:     The ECRAM index
 * @param     pui8Data:   Read and write data buffer
 */
void board_ioexp_AcpiHandler_IOExp8 (uint8_t isRead, uint8_t ui8Idx, uint8_t * pui8Data){}

/**
 * @brief ACPI ECRAM callback handler for IOEXP9 (8 ioexp pins) - stub
 *
 * @param     isRead:     True is read and false is write
 * @param     ui8Idx:     The ECRAM index
 * @param     pui8Data:   Read and write data buffer
 */
void board_ioexp_AcpiHandler_IOExp9 (uint8_t isRead, uint8_t ui8Idx, uint8_t * pui8Data){}

/**
 * @brief ACPI ECRAM callback handler for IOEXP10 (8 ioexp pins) - stub
 *
 * @param     isRead:     True is read and false is write
 * @param     ui8Idx:     The ECRAM index
 * @param     pui8Data:   Read and write data buffer
 */
void board_ioexp_AcpiHandler_IOExp10 (uint8_t isRead, uint8_t ui8Idx, uint8_t * pui8Data){}

/**
 * @brief ACPI ECRAM callback handler for IOEXP11 (8 ioexp pins) - stub
 *
 * @param     isRead:     True is read and false is write
 * @param     ui8Idx:     The ECRAM index
 * @param     pui8Data:   Read and write data buffer
 */
void board_ioexp_AcpiHandler_IOExp11 (uint8_t isRead, uint8_t ui8Idx, uint8_t * pui8Data){}

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