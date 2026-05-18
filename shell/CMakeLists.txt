# Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.

target_sources_ifdef(CONFIG_REBOOT_SHELL app
      PRIVATE    
      ${CMAKE_CURRENT_LIST_DIR}/reboot_service.c
      )
target_sources_ifdef(CONFIG_AMD_EEPROM_SHELL app
      PRIVATE    
      ${CMAKE_CURRENT_LIST_DIR}/eeprom_service.c
      )
target_sources_ifdef(CONFIG_NVOPTION_SHELL app
      PRIVATE    
      ${CMAKE_CURRENT_LIST_DIR}/nvoption_service.c
      )
target_sources_ifdef(CONFIG_AMD_MEMORY_SHELL app
      PRIVATE    
      ${CMAKE_CURRENT_LIST_DIR}/memory_service.c
      )
target_sources_ifdef(CONFIG_AMD_GPIO_SHELL app
      PRIVATE    
      ${CMAKE_CURRENT_LIST_DIR}/gpio_service.c
      )
target_sources_ifdef(CONFIG_AMD_SPIFLASH_SHELL app
      PRIVATE    
      ${CMAKE_CURRENT_LIST_DIR}/spiflash_service.c
      )

target_include_directories(app
    PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}
    )
