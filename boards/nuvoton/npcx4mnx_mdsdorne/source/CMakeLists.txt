# Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.

target_sources_ifdef(CONFIG_POWER_SOURCE_MANAGEMENT app
    PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/board_pwrSrc.c
    PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/board_pwrSrc.h
    )
    
target_sources_ifdef(CONFIG_PLATFORM_INIT app
    PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/board_init.c
    PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/board_init.h
    ${CMAKE_CURRENT_LIST_DIR}/board_nvoption.h
    )

target_sources_ifdef(CONFIG_BATTERY_MANAGEMENT app
    PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/board_smtbty.c
    PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/board_smtbty.h
    )

target_sources_ifdef(CONFIG_IO_EXPANDER app
    PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/board_ioexp.c
    PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/board_ioexp.h
    ${CMAKE_CURRENT_LIST_DIR}/board_ioexpTable.h
    )

target_sources_ifdef(CONFIG_SLEEP app
    PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/board_sleep.c
    PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/board_sleep.h
    )
    
target_sources_ifdef(CONFIG_PERIPHERAL_MANAGEMENT app
    PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/board_periph.c
    PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/board_periph.h
    )
target_sources_ifdef(CONFIG_APP_THERMAL app
    PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/board_thermal.c
    PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/board_thermal.h
    )
    
target_sources_ifdef(CONFIG_VR_CONFIG_TABLE app
    PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/board_vrCfg.c
    PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/board_vrCfg.h
    )

target_sources_ifdef(CONFIG_SELECTABLE_DAC_VALUE app
    PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/board_vrCfg.c
    ${CMAKE_CURRENT_LIST_DIR}/board_dac.c
    PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/board_vrCfg.h
    ${CMAKE_CURRENT_LIST_DIR}/board_dac.h
    )
	
target_sources_ifdef(CONFIG_FAN_RPM_CONTROL app
    PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/board_fanRpmCtrl.c
    PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/board_fanRpmCtrl.h
    )

target_sources(app
    PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/board_id.c
    ${CMAKE_CURRENT_LIST_DIR}/board_sys_config.c
    ${CMAKE_CURRENT_LIST_DIR}/mdsdorne_npcx4mnx.c
    PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/board_id.h
    ${CMAKE_CURRENT_LIST_DIR}/board_sys_config.h
    ${CMAKE_CURRENT_LIST_DIR}/mdsdorne_npcx4mnx.h
    )

if(CONFIG_BOARD_NPCX4MNX_MDSDORNE)
target_sources(app
    PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/ec_version.c
    ${CMAKE_CURRENT_LIST_DIR}/gpioAutoGen/gpioAutoGen.c
    PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/ec_version.h
    ${CMAKE_CURRENT_LIST_DIR}/gpioAutoGen/gpioAutoGen.h
    )

target_include_directories(app
    PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/gpioAutoGen
    )
endif()

target_include_directories(app
    PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}
    )