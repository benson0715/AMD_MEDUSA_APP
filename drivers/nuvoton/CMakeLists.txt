# Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.

target_sources(app
    PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/psl.c
    PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/psl.h
    )

target_include_directories(app
    PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}
    )
