# Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.

if(CONFIG_BOARD_NPCX4MNX_MDSPLUM_ECMODULE)
target_sources(app
    PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/ec_version.c
    ${CMAKE_CURRENT_LIST_DIR}/gpioAutoGen.c
    PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/gpioAutoGen.h
    ${CMAKE_CURRENT_LIST_DIR}/ec_version.h
    )
endif()

target_include_directories(app
    PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}
    )