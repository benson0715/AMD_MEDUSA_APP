# Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.

target_sources(app
    PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/app.c
    )

include_directories(app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/app/peripheral_management)
include_directories(app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/app/power_sequencing)
include_directories(app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/app/power_management)
include_directories(app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/app/smchost)
#[AMD]++
include_directories(app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/app/thermal_management)
#[AMD]--
include_directories(app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/app/battery_management)
include_directories(app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/app/udc)
include_directories(app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/app/kbchost)
include_directories(app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/app/usbc)
include_directories(app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/app/sfh)
#[AMD]++ 
include_directories(app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/app/saf)
#[AMD]--
#
include_directories(app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/app/acpi)
include_directories(app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/app/wireless_manageability)
include_directories(app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/app/s0stest)
include_directories(app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/app/fanctrl)
include_directories(app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/app/ec_debug)

target_sources_ifdef(CONFIG_PERIPHERAL_MANAGEMENT app
    PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/peripheral_management/periphmgmt.c
    ${CMAKE_CURRENT_LIST_DIR}/peripheral_management/app_btn.c
    PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/peripheral_management/periphmgmt.h
    ${CMAKE_CURRENT_LIST_DIR}/peripheral_management/app_btn.h
    )
    
target_sources_ifdef(CONFIG_ACDCSWITCH app
    PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/power_management/app_AcDcSwitch.c
    PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/power_management/app_AcDcSwitch.h
    )

target_sources_ifdef(CONFIG_PWRMGMT app
    PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/power_sequencing/app_pseq.c
    PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/power_sequencing/app_pseq.h
    )

target_sources_ifdef(CONFIG_SMCHOST app
    PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/smchost/smchost.c
    ${CMAKE_CURRENT_LIST_DIR}/smchost/smchost_pm.c
    ${CMAKE_CURRENT_LIST_DIR}/smchost/smc.c
    ${CMAKE_CURRENT_LIST_DIR}/smchost/sci.c
    PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/smchost/smchost.h
    ${CMAKE_CURRENT_LIST_DIR}/smchost/smc.h
    ${CMAKE_CURRENT_LIST_DIR}/smchost/sci.h
    ${CMAKE_CURRENT_LIST_DIR}/smchost/scicodes.h
    )

target_sources_ifdef(CONFIG_APP_THERMAL app
     PRIVATE
     ${CMAKE_CURRENT_LIST_DIR}/thermal_management/app_thermal.c
     PUBLIC
     ${CMAKE_CURRENT_LIST_DIR}/thermal_management/app_thermal.h
     )

target_sources_ifdef(CONFIG_ESPI_PERIPHERAL_8042_KBC app
    PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/kbchost/kbchost.c
    ${CMAKE_CURRENT_LIST_DIR}/kbchost/keyboard_utility.c
    PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/kbchost/kbchost.h
    ${CMAKE_CURRENT_LIST_DIR}/kbchost/keyboard_utility.h
    )

target_sources_ifdef(CONFIG_SFH app
    PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/sfh/app_sfh.c
    ${CMAKE_CURRENT_LIST_DIR}/sfh/app_sfh_waie.c
    ${CMAKE_CURRENT_LIST_DIR}/sfh/app_sfh_fn.c
    PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/sfh/app_sfh.h
    ${CMAKE_CURRENT_LIST_DIR}/sfh/app_sfh_waie.h
    ${CMAKE_CURRENT_LIST_DIR}/sfh/app_sfh_fn.h
    )

target_sources_ifdef(CONFIG_USBC_CCGX app
    PRIVATE
    PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/usbc/app_ccgx.h
    )
    
target_sources_ifdef(CONFIG_USBC_4CC app
    PRIVATE
    PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/usbc/app_4cc.h
    )
    
target_sources_ifdef(CONFIG_USBC_UCSI_TUNNEL app
    PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/usbc/app_ucsi_tunnel.c
    PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/usbc/app_ucsi_tunnel.h
    )
    
target_sources_ifdef(CONFIG_USBC_UCSI_TUNNEL app
    PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/usbc/app_ucsi_tunnel.c
    PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/usbc/app_ucsi_tunnel.h
    )
    
target_sources_ifdef(CONFIG_USBC_POWER_DELIVERY app
    PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/usbc/app_usbc_task.c
    PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/usbc/app_usbc_task.h
    )

target_sources_ifdef(CONFIG_BATTERY_MANAGEMENT app
    PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/battery_management/app_smtbty.c
    PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/battery_management/app_smtbty.h
    )

target_sources_ifdef(CONFIG_UDC_MANAGEMENT app
    PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/udc/app_udc.c
    PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/udc/app_udc.h
    )

target_sources_ifdef(CONFIG_EC_ACPI app
    PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/acpi/app_acpi.c
    PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/acpi/app_acpi.h
    )

target_sources_ifdef(CONFIG_WIRELESS_MANAGEABILITY app
    PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/wireless_manageability/app_manOs.c
    PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/wireless_manageability/app_manOs.h
    )

target_sources_ifdef(CONFIG_SLEEP app
    PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/power_management/app_sleep.c
    PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/power_management/app_sleep.h
    )

target_sources_ifdef(CONFIG_VCI app
    PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/power_management/app_vci.c
    PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/power_management/app_vci.h
    )

if(CONFIG_ESPI_SAF)
target_sources_ifdef(CONFIG_ESPI_SAF app
    PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/saf/saf_config.c
    PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/saf/saf_config.h
    )
else()
target_sources_ifdef(CONFIG_S0S_TEST app
    PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/s0stest/app_spiS0.c
    PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/s0stest/app_spiS0.h
    )
endif()

target_sources_ifdef(CONFIG_FAN_CONTROL app
    PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/fanctrl/app_fanctrl.c
    PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/fanctrl/app_fanctrl.h
    PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/fanctrl/f_fanImplt.c
    PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/fanctrl/f_fanImplt.h
    )

if (CONFIG_ECDBGI_SUPPORT)
    target_sources_ifdef(CONFIG_ECDBGI_SUPPORT app
        PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/ec_debug/espi_validation.c
		${CMAKE_CURRENT_LIST_DIR}/ec_debug/debug_info.c
        )
    target_sources(app
        PUBLIC
        ${CMAKE_CURRENT_LIST_DIR}/ec_debug/ec_debug.h
		${CMAKE_CURRENT_LIST_DIR}/ec_debug/debug_info.h
        )
endif()

target_include_directories(app
    PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}
    )
