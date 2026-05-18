# Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.

target_include_directories(app
    PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}
    ${CMAKE_CURRENT_LIST_DIR}/nuvoton
    ${CMAKE_CURRENT_LIST_DIR}/realtek
    )

if(CONFIG_BOARD_NPCX4MNX_MDSPLUM)
include(${CMAKE_CURRENT_LIST_DIR}/nuvoton/npcx4mnx_mdsplum/source/CMakeLists.txt)
endif()

if(CONFIG_BOARD_NPCX4MNX_MDSPLUM_ECMODULE)
include(${CMAKE_CURRENT_LIST_DIR}/nuvoton/npcx4mnx_mdsplum/source/CMakeLists.txt)
include(${CMAKE_CURRENT_LIST_DIR}/nuvoton/npcx4mnx_mdsplum_ecmodule/source/CMakeLists.txt)
endif()

if(CONFIG_BOARD_NPCX4MNX_MDSDORNE)
include(${CMAKE_CURRENT_LIST_DIR}/nuvoton/npcx4mnx_mdsdorne/source/CMakeLists.txt)
endif()

if(CONFIG_BOARD_NPCX4MNX_MDSAERIS)
include(${CMAKE_CURRENT_LIST_DIR}/nuvoton/npcx4mnx_mdsaeris/source/CMakeLists.txt)
endif()

# Realtek RTS5918 boards. Each board hosts its own per-board source
# subtree (mdsplum_rts5918.{c,h}, board_id.c, gpioAutoGen/, ...) that
# matches the layout used by the Nuvoton npcx4mnx_* boards.
if(CONFIG_BOARD_RTS5918_MDSPLUM)
include(${CMAKE_CURRENT_LIST_DIR}/realtek/rts5918_mdsplum/source/CMakeLists.txt)
endif()