# Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.

target_sources_ifdef(CONFIG_SAF_SPI_WINBOND app
    PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/saf_spi_winbond.c
    PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/saf_spi_winbond.h
   )


target_include_directories(app
    PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}
    )