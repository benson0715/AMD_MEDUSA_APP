/*****************************************************************************
 * Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
 ******************************************************************************
 */

#ifndef APP_UCSI_TUNNEL_H_
#define APP_UCSI_TUNNEL_H_

#include "amd_crb_drivers_pd.h"
#include "amd_crb_drivers_ucsi.h"
#include "system.h"

#if defined(__CC_ARM)
  #pragma push
  #pragma anon_unions
#elif defined(__ICCARM__)
  #pragma language=extended
#elif defined(__GNUC__)
  /* anonymous unions are enabled by default */
#elif defined(__TMS470__)
/* anonymous unions are enabled by default */
#elif defined(__TASKING__)
  #pragma warning 586
#else
  #warning Not supported compiler type
#endif

#pragma pack(push, 1)

typedef uint16_t APP_UCSI_VERSION;
typedef uint32_t APP_UCSI_CCI;

#ifndef _RO
#define _RO
#endif

#ifndef _WO
#define _WO
#endif

#ifndef _RW
#define _RW
#endif

typedef union {
	uint32_t u32Ctrl[2];
	uint8_t u8Ctrl[8];
} APP_UCSI_CONTROL;

/* UCSI V1.x */
#if UCSI_REV_1_x_ENABLE
typedef union {
	uint32_t u32Buf[4];
	uint16_t u16Buf[8];
	uint8_t  u8Buf[16];
} APP_UCSI_MESSAGE;

typedef struct {                          // Offset   Size (bits/bytes)
	_RO APP_UCSI_VERSION version;         //  0         16 / 2
	    uint16_t       reserved;          //  2         16 / 2
	_RO APP_UCSI_CCI CCI;                 //  4         32 / 4
	_WO APP_UCSI_CONTROL ctrl;            //  8         64 / 8
	_RO APP_UCSI_MESSAGE msgIn;           //  16       128 / 16
	_WO APP_UCSI_MESSAGE msgOut;          //  32       128 / 16
} F_UCSI_MAILBOX;

typedef enum {
	UCSI_MBOX_OFST_VERSION = 0,
	__UCSI_MBOX_OFST_RES   = 2,
	UCSI_MBOX_OFST_CCI     = 4,
	UCSI_MBOX_OFST_CONTROL = 8,
	UCSI_MBOX_OFST_MSGIN   = 16,
	UCSI_MBOX_OFST_MSGOUT  = 32,
} F_UCSI_MAILBOX_ITEMS;
#elif UCSI_REV_2_x_ENABLE
/* UCSI V2.x */
typedef union {
	uint32_t u32Buf[64];
	uint16_t u16Buf[128];
	uint8_t  u8Buf[256];
} APP_UCSI_MESSAGE;

typedef struct {                          // Offset   Size (bits/bytes)
	_RO APP_UCSI_VERSION version;         //  0         16 / 2
	    uint16_t       reserved;          //  2         16 / 2
	_RO APP_UCSI_CCI CCI;                 //  4         32 / 4
	_WO APP_UCSI_CONTROL ctrl;            //  8         64 / 8
	_RO APP_UCSI_MESSAGE msgIn;           //  16      2048 / 256
	_WO APP_UCSI_MESSAGE msgOut;          //  288     2048 / 256
} F_UCSI_MAILBOX;

typedef enum {
	UCSI_MBOX_OFST_VERSION = 0,
	__UCSI_MBOX_OFST_RES   = 2,
	UCSI_MBOX_OFST_CCI     = 4,
	UCSI_MBOX_OFST_CONTROL = 8,
	UCSI_MBOX_OFST_MSGIN   = 16,
	UCSI_MBOX_OFST_MSGOUT  = 288,
} F_UCSI_MAILBOX_ITEMS;
#endif

typedef enum {
	PD_DEVICE_ID_CCGx = 1,
	PD_DEVICE_ID_TIx = 2,
	PD_DEVICE_ID_ITEx = 3,
	PD_DEVICE_ID_RTKx = 4,
	PD_DEVICE_ID_NULL = 5,
} PD_DEVICE_ID;

#pragma pack(pop)

#if defined(__CC_ARM)
  #pragma pop
#elif defined(__ICCARM__)
  /* leave anonymous unions enabled */
#elif defined(__GNUC__)
  /* anonymous unions are enabled by default */
#elif defined(__TMS470__)
  /* anonymous unions are enabled by default */
#elif defined(__TASKING__)
  #pragma warning restore
#else
#warning Not supported compiler type
#endif

#define ACPI_UCSI_BASE            (0x60)
#define F_UCSI_SIZEOF_MAILBOX     (sizeof(F_UCSI_MAILBOX))

/* Init the ucsi tunnel */
void app_ucsi_tunnel_init (uint8_t deviceID, F_UCSI_MAILBOX * pUcsiMailBoxBuf);
/* start ucsi tunnel */
bool app_ucsi_tunnel_start(uint8_t chipID);
/* end ucsi tunnel */
bool app_ucsi_tunnel_end(uint8_t chipID);
/* acpi space handler for ucsi tunnel */
void app_ucsi_tunnel_AcpiSpaceHandler (uint8_t isRead, uint8_t ui8Idx, uint8_t * pui8Data);
/* ucsi interrupt handler */
bool app_ucsi_tunnel_intHandler (uint8_t chipID);
/* ucsi tunnel ppm to opm */
void app_ucsi_tunnel_Ppm2Opm (void);
/* ucsi tunnel opm to ppm */
void app_ucsi_tunnel_Opm2Ppm (void);
/* ucsi tunnel is support or not */
bool app_ucsi_tunnel_isSupported(void);
/* read ucsi version */
void app_ucsi_tunnel_ReadVersion(void);
/* ucsi tunnel host write done */
void app_ucsi_tunnel_hostWriteDone (void);
/* ucsi tunnel host read done */
void app_ucsi_tunnel_hostReadDone (void);
/* ucsi tunnel main task */
void app_ucsi_tunnel_task(void);
/* ucsi tunnel init the mail box */
void app_ucsi_tunnel_initMailBox(void);
/* ucsi tunnel write the device id */
void app_ucsi_tunnel_writeDevID(uint8_t id);
/* ucsi tunnel read the device id */
uint8_t app_ucsi_tunnel_readDevID(void);
/* ucsi tunnel read hpi mail box */
void app_ucsi_readHpiMailbox (uint8_t chipID);
/* ucsi tunnel write hpi mail box */
void app_ucsi_writeHpiMailbox (uint8_t chipID);
/* ucsi tunnel dummy read hpi mail box */
void app_ucsi_dummy_readHpiMailbox (uint8_t chipID);
/* ucsi tunnel error read hpi mail box */
void app_ucsi_error_readHpiMailbox (uint8_t chipID);
/* ucsi tunnel read ite mail box */
void app_ucsi_readIteMailbox (uint8_t chipID);
/* ucsi tunnel write ite mail box */
void app_ucsi_writeIteMailbox (uint8_t chipID);
/* ucsi tunnel dummy read ite mail box */
void app_ucsi_dummy_readIteMailbox (uint8_t chipID);
/* ucsi tunnel error read ite mail box */
void app_ucsi_error_readIteMailbox (uint8_t chipID);

#endif // end of APP_UCSI_TUNNEL_H_

