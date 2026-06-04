# Copyright (c) 2026 Realtek Semiconductor Corporation, SIBG-SD7
# SPDX-License-Identifier: Apache-2.0

# RTS5918 vendor shim layer — only compiled in when the SoC series is
# RTS5918. Mirror of drivers/nuvoton/CMakeLists.txt with Realtek
# equivalents for the helpers AMD's app/ and drivers/* code calls
# through Nuvoton-named entry points.

target_sources(app
    PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/psl.c
    ${CMAKE_CURRENT_LIST_DIR}/realtek_espi.c
    ${CMAKE_CURRENT_LIST_DIR}/realtek_gpio_impl.c
    ${CMAKE_CURRENT_LIST_DIR}/scaffold_stubs.c
    ${CMAKE_CURRENT_LIST_DIR}/realtek_acpi.c
    PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/psl.h
    ${CMAKE_CURRENT_LIST_DIR}/realtek_espi.h
    ${CMAKE_CURRENT_LIST_DIR}/gpio_ec.h
    ${CMAKE_CURRENT_LIST_DIR}/realtek_gpio_ctrl.h
    )

target_include_directories(app
    PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}
    )
