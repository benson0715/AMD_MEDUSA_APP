#EC drivers build options

# Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.

if (CONFIG_PS2_KEYBOARD OR CONFIG_PS2_MOUSE)
target_sources(app
    PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/ps2kbaux.c
    PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/ps2kbaux.h
    )
endif()

target_sources_ifdef(CONFIG_KSCAN_EC app
    PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/kbs_matrix.c
    PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/kbs_matrix.h
    ${CMAKE_CURRENT_LIST_DIR}/kbs_keymap.h
    )

if (CONFIG_KSCAN_EC OR CONFIG_PS2_KEYBOARD)
    if (CONFIG_EC_AMDCRB_KEYBOARD)
        target_sources(app
        PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/amdcrb_keymap.c
    )
    endif()
endif()

if (CONFIG_USBC_DRIVERS)
    if (CONFIG_USBC_TIPD_TPS6599X AND CONFIG_USBC_4CC)
        target_sources(app
        PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/amd_crb_drivers_tps6599x.c
        PUBLIC
        ${CMAKE_CURRENT_LIST_DIR}/amd_crb_drivers_tps6599x.h
        ${CMAKE_CURRENT_LIST_DIR}/amd_crb_drivers_ucsi.h
    )
    endif()
endif()

if (CONFIG_USBC_DRIVERS)
    if (CONFIG_USBC_TIPD_TPS6699X AND CONFIG_USBC_4CC)
        target_sources(app
        PRIVATE
        PUBLIC
        ${CMAKE_CURRENT_LIST_DIR}/amd_crb_drivers_tps6699x.h
        ${CMAKE_CURRENT_LIST_DIR}/amd_crb_drivers_ucsi.h
    )
    endif()
endif()

if (CONFIG_USBC_DRIVERS)
    if (CONFIG_USBC_ITEPD)
        target_sources(app
        PRIVATE
        PUBLIC
        ${CMAKE_CURRENT_LIST_DIR}/amd_crb_drivers_ite_pd.h
    )
    endif()
endif()

if (CONFIG_USBC_DRIVERS)
    if (CONFIG_USBC_RTKPD)
        target_sources(app
        PRIVATE
        PUBLIC
        ${CMAKE_CURRENT_LIST_DIR}/amd_crb_drivers_realtek_pd.h
        ${CMAKE_CURRENT_LIST_DIR}/amd_crb_drivers_realtek_ucsi.h
    )
    endif()
endif()

if (CONFIG_USBC_DRIVERS)
    if (CONFIG_USBC_CCGX)
        target_sources(app
        PRIVATE

        PUBLIC
        ${CMAKE_CURRENT_LIST_DIR}/amd_crb_drivers_hpi.h
    )
    endif()
endif()


target_sources_ifdef(CONFIG_USBC_DRIVERS app
    PRIVATE
    PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/amd_crb_drivers_pd.h
    ${CMAKE_CURRENT_LIST_DIR}/amd_crb_drivers_ucsi_internal.h
    ${CMAKE_CURRENT_LIST_DIR}/amd_crb_drivers_raa489300.h
    )

target_sources_ifdef(CONFIG_IO_EXPANDER app
    PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/dev_pca9535.c
    ${CMAKE_CURRENT_LIST_DIR}/ioexp_hub.c
    PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/dev_pca9535.h
    ${CMAKE_CURRENT_LIST_DIR}/ioexp_hub.h
    )

target_sources_ifdef(CONFIG_THERMAL_MANAGEMENT app
    PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/adc_sensors.c
    ${CMAKE_CURRENT_LIST_DIR}/peci_hub.c
    PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/adc_sensors.h
    ${CMAKE_CURRENT_LIST_DIR}/peci_hub.h
    )

target_sources(app
    PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/acpiplanes.c
    ${CMAKE_CURRENT_LIST_DIR}/eeprom.c
    ${CMAKE_CURRENT_LIST_DIR}/i2c_hub.c
    ${CMAKE_CURRENT_LIST_DIR}/espi_hub.c
    PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/acpiplanes.h
    ${CMAKE_CURRENT_LIST_DIR}/eeprom.h
    ${CMAKE_CURRENT_LIST_DIR}/i2c_hub.h
    ${CMAKE_CURRENT_LIST_DIR}/espioob_mngr.h
    ${CMAKE_CURRENT_LIST_DIR}/fan.h
    ${CMAKE_CURRENT_LIST_DIR}/acpi.h
    ${CMAKE_CURRENT_LIST_DIR}/fwSt.h
    ${CMAKE_CURRENT_LIST_DIR}/espi_hub.h
    ${CMAKE_CURRENT_LIST_DIR}/amd_crb_drivers_spiFlash.h
    )

# Gate Nuvoton-specific .c files. espioob_mngr.c assumes the AMD-patched
# espi_oob_packet has a `tag` field; amd_crb_drivers_spiFlash.c uses
# NPCX_SCFG_REG_ADDR / FLASH_NPCX_EX_OP_SPI_* register hooks. Both stay
# NPCX-only until equivalent Realtek implementations land.
target_sources_ifdef(CONFIG_SOC_SERIES_NPCX4 app
    PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/espioob_mngr.c
    ${CMAKE_CURRENT_LIST_DIR}/amd_crb_drivers_spiFlash.c
    )
    
target_sources_ifdef(CONFIG_OOB_MSG_MNGR app
    PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/oob_msg_mngr.c
    ${CMAKE_CURRENT_LIST_DIR}/oob_rpmc_msg.c
    ${CMAKE_CURRENT_LIST_DIR}/oob_rpmc_hmac_sha2_256.c
    ${CMAKE_CURRENT_LIST_DIR}/oob_rpmc_hmac_sha3_384.c
    ${CMAKE_CURRENT_LIST_DIR}/oob_rpmc_rom.c
    ${CMAKE_CURRENT_LIST_DIR}/oob_misc_msg.c
    PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/oob_msg_mngr.h
    ${CMAKE_CURRENT_LIST_DIR}/oob_rpmc_msg.h
    ${CMAKE_CURRENT_LIST_DIR}/oob_rpmc_hmac_sha2_256.h
    ${CMAKE_CURRENT_LIST_DIR}/oob_rpmc_hmac_sha3_384.h
    ${CMAKE_CURRENT_LIST_DIR}/oob_rpmc_rom.h
    ${CMAKE_CURRENT_LIST_DIR}/oob_misc_msg.h
    )

target_sources_ifdef(CONFIG_VR_CONFIG_TABLE app
    PRIVATE
    # ${CMAKE_CURRENT_LIST_DIR}/dev_mp2845_SVI3.c
    ${CMAKE_CURRENT_LIST_DIR}/dev_mp2815_SVI3.c
    PUBLIC
    # ${CMAKE_CURRENT_LIST_DIR}/dev_mp2845_SVI3.h
    ${CMAKE_CURRENT_LIST_DIR}/dev_mp2815_SVI3.h
    )

target_sources_ifdef(CONFIG_VR_CONFIG_TABLE app
    PRIVATE
    # ${CMAKE_CURRENT_LIST_DIR}/dev_mp2845_SVI3.c
    ${CMAKE_CURRENT_LIST_DIR}/dev_rrv686xx.c
    PUBLIC
    # ${CMAKE_CURRENT_LIST_DIR}/dev_mp2845_SVI3.h
    ${CMAKE_CURRENT_LIST_DIR}/dev_rrv686xx.h
    )

target_sources_ifdef(CONFIG_BATTERY_MANAGEMENT app
   PRIVATE
   ${CMAKE_CURRENT_LIST_DIR}/dev_smtbty.c
   PUBLIC
   ${CMAKE_CURRENT_LIST_DIR}/dev_smtbty.h
   )
   
target_sources_ifdef(CONFIG_CHARGER_ISL9241 app
   PRIVATE

   PUBLIC
   ${CMAKE_CURRENT_LIST_DIR}/dev_isl9241.h
   )
   
target_sources_ifdef(CONFIG_CHARGER_MP2764 app
   PRIVATE
   ${CMAKE_CURRENT_LIST_DIR}/amd_crb_drivers_mp2764.c
   PUBLIC
   ${CMAKE_CURRENT_LIST_DIR}/amd_crb_drivers_mp2764.h
   )

target_sources_ifdef(CONFIG_MODBUSRTU app
   PRIVATE
   ${CMAKE_CURRENT_LIST_DIR}/amd_crb_modbusrtu_driver.c
   PUBLIC
   ${CMAKE_CURRENT_LIST_DIR}/amd_crb_modbusrtu_driver.h
   )

target_sources_ifdef(CONFIG_APP_THERMAL app
   PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/dev_sbtsi.c
    ${CMAKE_CURRENT_LIST_DIR}/dev_tmp432.c
   PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/dev_sbtsi.h
    ${CMAKE_CURRENT_LIST_DIR}/dev_tmp432.h
   )
   
target_sources_ifdef(CONFIG_ALS app
   PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/dev_opt4048.c
   PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/dev_opt4048.h
   )

target_sources(app
    PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/dev_ina219.c
    ${CMAKE_CURRENT_LIST_DIR}/dev_mpc.c
    ${CMAKE_CURRENT_LIST_DIR}/dev_tcs3430.c
    # ${CMAKE_CURRENT_LIST_DIR}/dev_pca9557.c
    ${CMAKE_CURRENT_LIST_DIR}/dev_pca9535.c
    ${CMAKE_CURRENT_LIST_DIR}/dev_max695x.c
    ${CMAKE_CURRENT_LIST_DIR}/postcode_led_hub.c
    ${CMAKE_CURRENT_LIST_DIR}/dev_lm95234.c
    ${CMAKE_CURRENT_LIST_DIR}/dev_utility.c
    ${CMAKE_CURRENT_LIST_DIR}/dev_cpld.c
    ${CMAKE_CURRENT_LIST_DIR}/fwSt.c
    PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/dev_ina219.h
    ${CMAKE_CURRENT_LIST_DIR}/dev_mpc.h
    ${CMAKE_CURRENT_LIST_DIR}/dev_tcs3430.h
    # ${CMAKE_CURRENT_LIST_DIR}/dev_pca9557.h
    ${CMAKE_CURRENT_LIST_DIR}/dev_pca9535.h
    ${CMAKE_CURRENT_LIST_DIR}/dev_max695x.h
    ${CMAKE_CURRENT_LIST_DIR}/postcode_led_hub.h
    ${CMAKE_CURRENT_LIST_DIR}/dev_lm95234.h
    ${CMAKE_CURRENT_LIST_DIR}/dev_utility.h
    ${CMAKE_CURRENT_LIST_DIR}/dev_cpld.h
    )
    
target_include_directories(app
    PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}
    )
