# Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.

target_sources(app
    PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/u_crc8.c
    ${CMAKE_CURRENT_LIST_DIR}/u_crc8.h
    ${CMAKE_CURRENT_LIST_DIR}/u_numbers.c
    ${CMAKE_CURRENT_LIST_DIR}/u_numbers.h
    ${CMAKE_CURRENT_LIST_DIR}/u_rrq.c
    ${CMAKE_CURRENT_LIST_DIR}/u_rrq.h
    ${CMAKE_CURRENT_LIST_DIR}/u_misc.c
    ${CMAKE_CURRENT_LIST_DIR}/u_misc.h
    ${CMAKE_CURRENT_LIST_DIR}/u_time.c
    ${CMAKE_CURRENT_LIST_DIR}/u_time.h
    ${CMAKE_CURRENT_LIST_DIR}/dbgRw.c
    ${CMAKE_CURRENT_LIST_DIR}/dbgRw.h
    ${CMAKE_CURRENT_LIST_DIR}/dbgLogFifo.c
    ${CMAKE_CURRENT_LIST_DIR}/dbgLogFifo.h
    )

target_include_directories(app
    PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}
   )
