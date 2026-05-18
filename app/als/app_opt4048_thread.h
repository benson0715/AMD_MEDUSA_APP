/*****************************************************************************
 * Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.
 ******************************************************************************
 *
 * OPT4048 Sensor Thread Application - Header
 *
 *****************************************************************************/

#ifndef __APP_OPT4048_THREAD_H__
#define __APP_OPT4048_THREAD_H__

#include <stdint.h>
#include <stdbool.h>
#include "dev_opt4048.h"

/**
 * @brief Initialize OPT4048 application
 *
 * Initializes the OPT4048 sensor hardware and configures it for
 * continuous operation. Should be called once during startup.
 *
 * @return true if successful, false otherwise
 */
bool app_opt4048_init(void);

/**
 * @brief Light sensor trigger thread
 *
 * Called by task if it want to trigger light sensor thread run one cycle
 *
 * NOTE: This runs in interrupt context - keep it fast!
 */
void app_opt4048_giveSem(void);

/**
 * @brief OPT4048 sensor polling thread
 *
 * Thread that periodically polls the OPT4048 sensor and updates
 * cached values. Operates based on system power state.
 *
 * @param p1 Thread run period in milliseconds (uint32_t*)
 * @param p2 Task sleep ready flag pointer (uint8_t*)
 * @param p3 Reserved (unused)
 */
void opt4048_thread(void *p1, void *p2, void *p3);

/**
 * @brief Get current ambient light level
 *
 * @return Lux value from last sensor reading
 */
uint32_t app_opt4048_get_lux(void);

/**
 * @brief Get Correlated Color Temperature
 *
 * @return CCT in Kelvin
 */
double app_opt4048_get_cct(void);

/**
 * @brief Get ACPI ALS data for OS consumption
 *
 * Retrieves all ALS sensor data formatted for ACPI exposure.
 * Called by ACPI handler in app_acpi.c.
 *
 * @param lux    Pointer to store lux value
 * @param red    Pointer to store red channel value
 * @param green  Pointer to store green channel value
 * @param blue   Pointer to store blue channel value
 * @param white  Pointer to store white/IR channel value
 * @param cct    Pointer to store CCT value
 */
void app_opt4048_get_acpi_data(uint16_t *lux, uint16_t *red, uint16_t *green,
                                uint16_t *blue, uint16_t *white, uint16_t *cct);

/**
 * @brief Get current RGBW color data
 *
 * @param color Pointer to color structure to fill
 * @return 0 on success, -EINVAL if pointer is NULL
 */
int app_opt4048_get_color(opt4048_color_t *color);

/**
 * @brief Check if sensor is initialized
 *
 * @return true if sensor is initialized, false otherwise
 */
bool app_opt4048_is_initialized(void);

/**
 * @brief Check if sensor is enabled
 *
 * @return true if sensor is enabled and polling, false otherwise
 */
bool app_opt4048_is_enabled(void);

/**
 * @brief Light sensor interrupt callback (called from GPIO ISR)
 *
 * This function should be called by the GPIO interrupt handler when
 * the OPT4048 interrupt pin asserts. It signals the thread to process
 * sensor data.
 *
 * NOTE: This runs in interrupt context - it only sets flags and signals semaphore.
 */
void app_opt4048_interrupt_callback(void);

/**
 * @brief ACPI handler for OPT4048 ALS data
 *
 * ACPI register map (plane 0xB2):
 *   Offset 0-1:  ALS lux value (uint16_t)
 *   Offset 2-3:  Red channel (uint16_t)
 *   Offset 4-5:  Green channel (uint16_t)
 *   Offset 6-7:  Blue channel (uint16_t)
 *   Offset 8-9:  White/IR channel (uint16_t)
 *   Offset 10-11: CCT in Kelvin (uint16_t)
 *
 * @param isRead    1 for read, 0 for write
 * @param ui8Idx    Register offset (0-11)
 * @param pui8Data  Pointer to data byte
 * @return 0 on success
 */
uint8_t app_opt4048_acpi_handler(uint8_t isRead, uint8_t ui8Idx, uint8_t *pui8Data);
#endif /* __APP_OPT4048_THREAD_H__ */
