/*****************************************************************************
 * Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
 ******************************************************************************
 */

#ifndef APP_4CC_H_
#define APP_4CC_H_

#include "amd_crb_drivers_pd.h"
#if CONFIG_USBC_TIPD_TPS6599X
#include "amd_crb_drivers_tps6599x.h"
#endif
#if CONFIG_USBC_TIPD_TPS6699X
#include "amd_crb_drivers_tps6699x.h"
#include "amd_crb_drivers_raa489300.h"
#endif
#include "amd_crb_drivers_ucsi.h"
#include "system.h"

/* define the TI PD patch mode */
#define APP_4CC_PD_LOAD_MODE_EC                    (1)
#define APP_4CC_PD_LOAD_MODE_FLASH                 (2)
#define APP_4CC_PD_LOAD_MODE_FLASH_OVERWRITE       (3)
#define APP_4CC_PD_LOAD_MODE_IMAGE                 (4)
#define APP_4CC_PD_LOAD_MODE_CONFIG                (5)

#if CONFIG_USBC_TIPD_TPS6699X
/* TIPD 6699x TFU FW upgrade */
#pragma pack(push,1)
#define PACK_STRUCT                      __attribute__((packed, aligned(1)))

#define HASH_MATCH                  (0x00)
#define HASH_MISMATCH               (0x01)
#define HASH_LENGTH                 (32) // SHA-256 hash length

#define APPCONFIG_DATA_BLOCK        (12)
#define MAX_DATA_BLOCKS             (12)
#define TFUC_MAGIC_KEY              (0xAC)

// Anant: only keep those that you use in C file - others should be deleted
#define BLOCK_ID_APPCONFIG_ONLY     0xACEA0003  // indicates a block or file from AppConfig data
#define BLOCK_ID_FW_IMAGE_ONLY      0xACEB0003  // indicates TI FW (flash image) Binary
#define BLOCK_ID_REVOCATION         0xACEC0003  // reserved for Key revoCation block
#define BLOCK_ID_ROOT_HEADER        0xACED0003  // indicates a Root Header block
#define BLOCK_ID_FIRMWARE_HEADER    0xACEE0003  // indicates a FW Header block
#define BLOCK_ID_FULL_FLASH_BUNDLE  0xACEF0003  // indicates a full flash binary file (that includes AppConfig data)

#define HEADER_BLOCK_SIZE           (0x800)     // (2K)
#define DATA_BLOCK_SIZE             (0x4000)    // 16K
#define DATA_BLOCK_CHUNK            (0x800)     // (2K)
#define APP_CONFIG_BLOCK_SIZE       (0x800)     // (2K)
#define TRIM_CONFIG_SIZE            (0x400)     // (1K)
#define SIGNATURE_LENGTH            (384)

#define TFU_MAX_TIMEOUT             (2000)      // Max Timeout Value (33 Mins)
#define TFUD_MAX_TIMEOUT            (150)       // Max Timeout Value (2.5 Mins)

#define HEADER_BLOCK_STATUS_SIZE    (13)        // TBD_APK: Using 13 instead of 12, and not allocating space for tfuiAuthStatus
                                                // as defined in Section 13.5 of TFU's UG
                                                // Check w/ Systems if this is OK - New definition in tTFUiBlockStatus

#define FW_IMAGE_ID_OFFSET          (0x000)
#define FW_IMAGE_ID_LENGTH          (0x004)

#define HB_ROOT_HEADER_METADATA_OFFSET  (FW_IMAGE_ID_OFFSET + FW_IMAGE_ID_LENGTH)
#define HB_ROOT_HEADER_METADATA_LENGTH  (8)

#define HB_ROOT_HEADER_HASH_OFFSET      (HB_ROOT_HEADER_METADATA_OFFSET + HB_ROOT_HEADER_METADATA_LENGTH)
#define HB_ROOT_HEADER_LENGTH           (HEADER_BLOCK_SIZE)

#define DB_FIRMARE_IMAGE_METADATA_LENGTH            (8)
#define DB_FIRMARE_IMAGE_METADATA_OFFSET(blockNum)  ((HB_ROOT_HEADER_HASH_OFFSET + \
                                                        HB_ROOT_HEADER_LENGTH) + \
                                                        ((DATA_BLOCK_SIZE + \
                                                        DB_FIRMARE_IMAGE_METADATA_LENGTH) * \
                                                        blockNum))

#define DB_FIRMARE_IMAGE_OFFSET(blockNum)   (DB_FIRMARE_IMAGE_METADATA_OFFSET(blockNum) + \
                                                DB_FIRMARE_IMAGE_METADATA_LENGTH)

#define DB_APP_CONFIG_METADATA_LENGTH   (8)

#define HB_ROOT_HEADER_FIRMIMAGE_SIZE_OFFSET    (0x4F8) // Firmware Image Size is stored at this offset of TFU Bundle
                                                        // This size excludes App Configuration and Header Block
// TPS6699X Firmware update protocol states
typedef enum {
    TFU_START,
    TFU_INIT,
    TFU_STREAM_HEADERBLOCK,
    TFU_DOWNLOAD,
    TFU_STREAM_DATABLOCK,
    TFU_APPCONFIG,
    TFU_STREAM_APPCONFIGBLOCK,
    TFU_COMPLETE,
    TFU_APPCONFIG_COMPLETE,
    TFU_QUERY,
    TFU_ERROR
} tTFUState;

// TPS6699X Firmware update error states
typedef enum {
    TFU_ERROR_CODE_NONE,
    TFU_ERROR_TFUI_STREAM_FAILED,           // Failed to Stream Header Block
    TFU_ERROR_TFUD_STREAM_FAILED,           // Failed to Stream Data Block
} tTFUErrorCode;

// TPS6699X Firmware update protocol book-keeping variables
typedef struct {
    uint8_t     numDataBlock;
    uint8_t     blockIncrementer;
    uint8_t     streamingI2CAddress;
    uint8_t     portI2CAddress;

    uint8_t     tfuMode;

    volatile tTFUState   TFUStateMachine;
    volatile bool        RunTFUStateMachine;
    volatile bool        AppConfigOnly;
    volatile bool        interruptActive;

    uint32_t    CurrentBlockSize;
    uint32_t    FirmwareImageSize;
} tTFUHandle;

/* TPS6699X Firmware update protocol TFUc inputparameters */
typedef struct {
	union {
			struct {
				uint8_t switchBank;
				uint8_t copyActiveToBackup;
				uint8_t resetPdc;
			} PACK_STRUCT f;
			uint8_t buf[3];
    };
}tTFUcInput;

/* TPS6699X Firmware update protocol block output parameters */
typedef struct
{
	union {
			struct {
				uint16_t BlockNum;                  // Data block number
				uint16_t BlockSize;                 // Header block size (Variable, Max 16kB)
				uint16_t TimeoutInSecs;             // Timeout for Rx/authentication of block
																						// LSB is 1000ms - This shall be reflected in TFU_TIMEOUT_RESOLUTION_MS
				uint16_t SlaveAddr;                 // Slave address for the burst data
			} PACK_STRUCT f;
			uint8_t buf[8];
	};
} tTFUdInput;

/* TPS6699X Firmware update protocol block output parameters */
typedef struct
{
	union {
			struct {
				uint8_t result;                     // Result
			} PACK_STRUCT f;
			uint8_t buf[1];
	};
} tTFUdOutput;

/* TPS6699X Firmware update protocol block input parameters */
typedef struct
{
	union {
			struct {
				uint16_t NumOfBlock;                // Total number of data blocks,
																						// excluding the Header and AppConfig blocks
				uint16_t BlockSize;                 // Header block size (Fixed, Max 2kB)
				uint16_t TimeoutInSecs;             // Timeout for the entire firmware update
																						// including the Header and AppConfig blocks
																						// LSB is 1000ms - This shall be reflected in TFU_TIMEOUT_RESOLUTION_MS
				uint16_t SlaveAddr;                 // Slave address for the burst data
			} PACK_STRUCT f;
			uint8_t buf[8];
	};
} tTFUiInput;

/* TPS6699X Firmware update protocol block output parameters */
typedef struct
{
	union {
		struct {
			uint8_t result;                     // Result
		} PACK_STRUCT f;
		uint8_t buf[1];
	};
} tTFUiOutput;

/* TPS6699X Firmware update protocol query status */
typedef enum {
	TFU_QUERY_TFU_STATUS = 0,
} tTFUqCommandType;

/* TPS6699X Firmware update protocol state machine status */
typedef enum {
	TFU_DEFAULT_STATUS          = 0,
	TFU_IN_PROGRESS_STATUS      = 1,
	TFU_STORE_BANK0_STATUS      = 2,
	TFU_STORE_BANK1_STATUS      = 3,
} tTFUqRegionNum;

/* TPS6699X Firmware update protocol TFUq input parameters */
typedef struct
{
	union {
			struct {
				tTFUqRegionNum regionNum;                  // 0 & 1= Status of TFU under process
																									 // 2 = Status of Stored Bank 0 Status
																									 // 3 = Status of Stored Bank 1 Status
				tTFUqCommandType commandType;              // 0 = Query, >0 = TBD
			} PACK_STRUCT f;
			uint8_t buf[2];
	};
} tTFUqInput;

/* TPS6699X Firmware update protocol TFUq output parameters */
typedef struct
{
	union {
			struct {
				uint8_t activeHost;
				uint8_t tfuState;
				uint8_t completeImageWrittenOrNot;
				uint16_t blocksWrittenToFlash;
				uint8_t detailHeaderBlockStatus;
				uint8_t detailBlockStatusOfEachDataBlock[12];
				uint8_t numOfHeaderBytesWritten;
				uint8_t numOfDataBytesWritten;
				uint8_t numOfAppConfigBytesWritten;
			} PACK_STRUCT f;
			uint8_t buf[21];
	};
} tTFUqOutput;

#pragma pack(pop)
#endif /* TIPD_SILICON_VER == TIPD_6699X */

extern uint8_t g_4cc_tiPdRetimerType[2];

/* detect TI PD based on I2C address */
bool app_4cc_vonderIdentify(void);
/* tipd 4cc command init */
uint8_t app_4cc_init(uint32_t fwOffset, uint32_t fwSize, uint8_t pd_load_mode, uint8_t chipID);
/* tipd top level interrupt handler */
void app_4cc_intTopHalf(uint8_t pinSt);
/* tipd bottom level interrupt handler */
void app_4cc_intBottomHalf(void);
/* tipd get higher sink pdo */
pd_do_t app_4cc_getHigherPortSnkPdo(void);
/* tipd recover iomux config */
void app_4cc_recoverIomux(uint8_t port);
/* tipd recovery ports status */
void app_4cc_recoverPorts(void);
/* set all the type-c port to sink only mode */
void app_4cc_sinkOnlyAll(void);
/* tipd 4cc command to force re-timer poweron */
uint8_t app_4cc_retimerForcePower(uint8_t port, bool status);
#if CONFIG_USBC_TIPD_TPS6599X
/* tipd change gpio to input */
uint8_t app_4cc_gpioInput(uint8_t gpioNum, uint8_t chipID);
/* tipd change gpio to output */
uint8_t app_4cc_gpioOutput(uint8_t gpioNum, uint8_t chipID);
#endif
/* tipd change gpio to output low */
uint8_t app_4cc_gpioOutputLow(uint8_t gpioNum, uint8_t chipID);
/* tipd change gpio to output high */
uint8_t app_4cc_gpioOutputHigh(uint8_t gpioNum, uint8_t chipID);
/* tipd clear dead battery status */
uint8_t app_4cc_deadbatteryClear(uint8_t port);
/* tipd load the pd initial status */
void app_4cc_loadInitialStatus(uint8_t port);
/* wait attach event  */
bool app_4cc_waitAttachEvent(uint8_t port);
/* read device information */
void app_4cc_deviceInfo(void);
/* read current pdo */
uint32_t app_4cc_curPDO(uint8_t port);
/* pd enter patch mode */
uint8_t app_4cc_patchPdController(uint8_t chipID);
/* tipd i2c tunnel */
uint8_t app_4cc_i2cTunnel(bool isRead, uint8_t address, uint8_t reg, uint8_t dataLen, uint8_t* data, uint8_t chipID);
/* tipd interrupt pending to process */
bool app_4cc_intPending(void);
/* tipd disable both usb4 and tbt3 */
uint8_t app_4cc_usb4Tbt3Disable(uint8_t port);
/* tipd enable both usb4 and tbt3 */
uint8_t app_4cc_usb4Tbt3Enable(uint8_t port);
/* tipd disable usb4 and enable tbt3 */
uint8_t app_4cc_usb4DisableTbt3Enable(uint8_t port);
/* tipd enable usb4 and disable tbt3 */
uint8_t app_4cc_usb4EnableTbt3Disable(uint8_t port);
/* tipd send data reset command */
uint8_t app_4cc_dataReset(uint8_t port);
/* tipd trigger discover vid process */
uint8_t app_4cc_discoveryProcess(uint8_t port);
/* tipd recover external flash */
uint8_t app_4cc_recoverExternalFlash(uint32_t fwOffset, uint32_t fwSize, uint32_t fullregion_array, uint32_t fullregion_length, uint8_t chipID);
/* tipd send crossbar mailbox data */
uint8_t app_4cc_crossbarMailBox(bool isRead, uint8_t port, uint8_t* dataBuf);
/* return I2C address for each port */
uint8_t app_4cc_getI2cAddr(uint8_t port);
/* interrupt handler */
uint32_t app_4cc_usbcInterruptProc(uint8_t port, DRV_TPS_INT_EVENT event);
/* force power on RT */
void app_4cc_forceRtPower(uint8_t port, bool Enable);
/* enable the sink port */
uint8_t app_4cc_srdy(uint8_t port);
/* disable the sink port */
uint8_t app_4cc_sryr(uint8_t port);
/* dynamic change the ppext VBUS SW config */
bool app_4cc_ppextVbusSwConfig(void);
/* enable or disable the EPR mode */
uint8_t app_4cc_eprEnable(uint8_t port, bool en);
/* set GPIO to high */
uint8_t app_4cc_setGpioHigh(uint8_t port, uint8_t gpio_port, uint8_t gpio_index);
/* set GPIO to low */
uint8_t app_4cc_setGpioLow(uint8_t port, uint8_t gpio_port, uint8_t gpio_index);

#endif // end of APP_4CC_H_

