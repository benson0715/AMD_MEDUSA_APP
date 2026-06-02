# Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.

cmake_minimum_required(VERSION 3.20.0)
set(BOARD_ROOT ${CMAKE_CURRENT_LIST_DIR}/../out_of_tree_boards)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(ecfw)

target_sources(app PRIVATE src/main.c)
target_sources(app PRIVATE src/memory_service.c)
target_sources(app PRIVATE src/gpio_shell.c)
target_sources(app PRIVATE src/flash_shell.c)

zephyr_compile_options(-Werror)
zephyr_compile_options(-Wno-unused-variable)