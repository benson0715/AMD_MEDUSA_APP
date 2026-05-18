/*****************************************************************************
 * Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
 ******************************************************************************
 */

#ifndef __APP_ACPI_H__
#define __APP_ACPI_H__

#include <errno.h>
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/logging/log.h>
#include <assert.h>
#include "acpiplanes.h"
#include "scicodes.h"

/**
 * Definitions for AMD ACPI Table
 */
#define ACPI_FAKE_QEVENT_BASE                (0x5F)
#define ACPI_PMIC_TUNNEL_BASE                (0x90)
#define ACPI_BOARD_ID_EXPORT_FIELD_BASE      (BID_ACPI_SPACE_EXPORTED_FIELD_OFFSET)
#define ACPI_GPIO_BASE                       (0x9D)
#define ACPI_SWITCH_BASE                     (0xB6)
#define ACPI_EC_FW_VERSION                   (0xB8)
#define ACPI_EC_FW_BUILD_DATE                (0xBD)
#define ACPI_EC_FW_BUILD_TIME                (0xC1)
#define ACPI_ACDC_SWITCH_BASE                (0xC7)
#define ACPI_DC_TIME                         (ACPI_ACDC_SWITCH_BASE)
#define ACPI_AC_TIME                         (ACPI_ACDC_SWITCH_BASE + 1)
#define ACPI_SYS_EVENT_FLAGS                 (0xCB)
#define ACPI_MISC_STATUS_CONTROL             (0xCF)
#define AMD_AC_DC_SWITCH_ENABLE              (0x02)
#define AMD_ACPI_MODE_ENABLE                 (0x04)
#define ACPI_BATTERY_BASE                    (0xD0)
#define ACPI_BATTERY_MEASUREACCURACY         (ACPI_BATTERY_BASE)
#define ACPI_BATTERY_LOW                     (ACPI_BATTERY_BASE + 0x02)
#define ACPI_BATTERY_CRITICAL                (ACPI_BATTERY_BASE + 0x04)
#define ACPI_BATTERY_TRIP_POINT              (ACPI_BATTERY_BASE + 0x06)
#define ACPI_BATTERY_TEMPERATURE             (ACPI_BATTERY_BASE + 0x08)
#define ACPI_BATTERY_MAX_ERROR               (ACPI_BATTERY_BASE + 0x0A)
#define ACPI_BATTERY_CYCLE_COUNT             (ACPI_BATTERY_BASE + 0x0C)
#define ACPI_BATTERY_CURRENT                 (ACPI_BATTERY_BASE + 0x0E)
#define ACPI_BATTERY_VOLTAGE                 (ACPI_BATTERY_BASE + 0x10)
#define ACPI_BATTERY_DESIGN_VOLTAGE          (ACPI_BATTERY_BASE + 0x12)
#define ACPI_BATTERY_REMAINING_CAPACITY      (ACPI_BATTERY_BASE + 0x14)
#define ACPI_BATTERY_FULL_CHARGE_CAPACITY    (ACPI_BATTERY_BASE + 0x16)
#define ACPI_BATTERY_DESIGN_CAPACITY         (ACPI_BATTERY_BASE + 0x18)
#define ACPI_BATTERY_CHARGE_THROTTLE         (ACPI_BATTERY_BASE + 0x1A)
#define ACPI_BATTERY_COUNT                   (ACPI_BATTERY_BASE + 0x1B)
#define ACPI_BATTERY_SELECT                  (ACPI_BATTERY_BASE + 0x1C)
#define ACPI_BATTERY_STATUS                  (ACPI_BATTERY_BASE + 0x1D)
#define AMD_BATTERY_CHARGING                  0x01
#define AMD_BATTERY_TRIPPED                   0x80
#define ACPI_CHARGER_STATUS                  (ACPI_BATTERY_BASE + 0x1E)
#define ACPI_BATTERY_CONNECTED                0x40
#define ACPI_AC_CONNECTED                     0x80
#define ACPI_BATTERY_ALERT                   (ACPI_BATTERY_BASE + 0x1F)
#define ACPI_BIOS_HOOK_SPACE                 (0xF0) /* 0xF0 ~ 0xF1 bios hook */
#define ACPI_THERMAL                         (0xF2) /* 0xF2 ~ 0xFB thermal zone */
#define ACPI_LIGHT_SENSOR_LUX                (0xFC) /* 0xFC ~ 0xFD light sensor lux */
#define ACPI_USB_HOST_STATUS                 (0xFE) /* 0xFE USB host status: ready, RMB A0/B0 */
#define ACPI_SWITCH_BASE5                    (0xFF)

typedef enum {
	PMF_NUKNOW = 0,
	PMF_BOOT,
	PMF_UNLOAD,
	PMF_SUSPEND,
	PMF_RESUME
} PMF_STATUS;

/**
 * Definitions for VW MMIO tunnel status
 */
#define ACPI_VW_MMIO_SUCCESS                 (0xAC)
#define ACPI_VW_MMIO_ERROR                   (0xE2)
#define ACPI_VW_MMIO_ONGOING                 (0xCC)

 /**
 * Definitions for uPEP hook id
 */
#define MS_uPEP_HOOK_DISPLAY_OFF   (3)
#define MS_uPEP_HOOK_DISPLAY_ON    (4)
#define MS_uPEP_HOOK_DRIPs_ENTER   (5)
#define MS_uPEP_HOOK_DRIPs_EXIT    (6)
#define MS_uPEP_HOOK_MS_ENTER      (7)
#define MS_uPEP_HOOK_MS_EXIT       (8)
#define MS_uPEP_HOOK_OS_ENTER      (9)
// old define (no DRIPs hook)
// #define MS_uPEP_HOOK_DISPLAY_OFF   (2)
// #define MS_uPEP_HOOK_DISPLAY_ON    (3)
#define MS_uPEP_HOOK_Q_EVT         (0x70)  // AMD specific

/* extern value */
extern uint8_t g_slyType;
extern uint8_t g_ui8MondernStandbySupport;

/* functions */
void ACPI_planeInit(void);
void ACPI_NotifyHost(uint8_t ui8QEvt);
void ACPI_EnableHostNotify(void);
void ACPI_DisableHostNotify(void);
uint8_t ACPI_isHostNotifyDisable(void);
void ACPI_PMIC_TunnelProc (void);
uint8_t ACPI_VW_MMIO_Handler(uint8_t isRead, uint8_t ui8Idx, uint8_t *pui8Data);
void ACPI_VW_MMIO_TunnelProc(void);
void ACPI_ClearQueuedEvts (void);
void ACPI_pmfHeartBeatStop(void);
void ACPI_pmfHeartBeatRefresh(void);

extern uint8_t g_slideToShutdownEnabled;

#endif // end of __APP_ACPI_H__
