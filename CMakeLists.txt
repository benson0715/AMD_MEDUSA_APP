# Platform Configration
# Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.

cmake_minimum_required(VERSION 3.13.1)
set(BOARD_ROOT ${CMAKE_CURRENT_LIST_DIR}/out_of_tree_boards)
include($ENV{ZEPHYR_BASE}/cmake/app/boilerplate.cmake NO_POLICY_SCOPE)
project(ecfw)

include(${CMAKE_CURRENT_LIST_DIR}/app/CMakeLists.txt)
include(${CMAKE_CURRENT_LIST_DIR}/boards/CMakeLists.txt)
include(${CMAKE_CURRENT_LIST_DIR}/drivers/CMakeLists.txt)
include(${CMAKE_CURRENT_LIST_DIR}/misc/CMakeLists.txt)
include(${CMAKE_CURRENT_LIST_DIR}/shell/CMakeLists.txt)
include(${CMAKE_CURRENT_LIST_DIR}/utilities/CMakeLists.txt)

if(CONFIG_SOC_SERIES_NPCX4)
include(${CMAKE_CURRENT_LIST_DIR}/drivers/nuvoton/CMakeLists.txt)
endif()

# Realtek RTS5918 vendor shim. Picks up gpio_ec.h / realtek_espi.h /
# psl.{c,h} and the EC_GPIO wrapper impl so app/ code that calls
# gpio_write_pin(EC_GPIO_xx,..) works against the realtek,rts5918-gpio
# banks.
if(CONFIG_SOC_SERIES_RTS5918 OR CONFIG_SOC_FAMILY_REALTEK_EC)
include(${CMAKE_CURRENT_LIST_DIR}/drivers/realtek/CMakeLists.txt)
endif()

# The external static library that we are linking with does not know
# how to build for this platform so we export all the flags used in
# this zephyr build to the external build system.
#
# Other external build systems may be self-contained enough that they
# do not need any build information from zephyr. Or they may be
# incompatible with certain zephyr options and need them to be
# filtered out.
zephyr_get_include_directories_for_lang_as_string(       C includes)
zephyr_get_system_include_directories_for_lang_as_string(C system_includes)
zephyr_get_compile_definitions_for_lang_as_string(       C definitions)
zephyr_get_compile_options_for_lang_as_string(           C options)

set(external_project_cflags
  "${includes} ${definitions} ${options} ${system_includes}"
  )

include(ExternalProject)

# Add an external project to be able download and build the third
# party library. In this case downloading is not necessary as it has
# been committed to the repository.
set(mylib_src_dir   ${CMAKE_CURRENT_SOURCE_DIR}/libamd)
set(mylib_build_dir ${CMAKE_CURRENT_BINARY_DIR}/libamd)
set(mylib_include   ${CMAKE_CURRENT_LIST_DIR}/include/)

set(MYLIB_LIB_DIR     ${CMAKE_CURRENT_SOURCE_DIR}/libamd/lib)
set(MYLIB_INCLUDE_DIR ${mylib_src_dir}/include)

if(CMAKE_GENERATOR STREQUAL "Unix Makefiles")
# https://www.gnu.org/software/make/manual/html_node/MAKE-Variable.html
set(submake "$(MAKE)")
else() # Obviously no MAKEFLAGS. Let's hope a "make" can be found somewhere.
set(submake "make")
endif()

ExternalProject_Add(
  amdlib_project                 # Name for custom target
  PREFIX     ${mylib_src_dir} # Root dir for entire project
  SOURCE_DIR ${mylib_src_dir}
  BINARY_DIR ${mylib_src_dir} # This particular build system is invoked from the root
  CONFIGURE_COMMAND ""    # Skip configuring the project, e.g. with autoconf
  BUILD_COMMAND
  ${submake}
  PREFIX=${mylib_src_dir}
  INCDIRS=${mylib_include}
  CC=${CMAKE_C_COMPILER}
  AR=${CMAKE_AR}
  CFLAGS=${external_project_cflags}
  INSTALL_COMMAND ""      # This particular build system has no install command
  BUILD_BYPRODUCTS ${MYLIB_LIB_DIR}/libamd.a
  )

# Create a wrapper CMake library that our app can link with
add_library(amdlib_lib STATIC IMPORTED GLOBAL)
add_dependencies(
  amdlib_lib
  amdlib_project
  )
set_target_properties(amdlib_lib PROPERTIES IMPORTED_LOCATION             ${MYLIB_LIB_DIR}/libamd.a)
set_target_properties(amdlib_lib PROPERTIES INTERFACE_INCLUDE_DIRECTORIES ${MYLIB_INCLUDE_DIR})
target_link_libraries(app PUBLIC amdlib_lib)

#Note : The target named "app" is the target name expected by Zephyr RTOS
include_directories(ecfw_app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)

# [forward-port] -Werror disabled during bring-up so cross-version
# warning drift (Nuvoton 3.2 → upstream 3.7.1) doesn't block progress.
# Re-enable once the Realtek board source/* stubs are filled with
# real schematic data and warning count goes to zero.
# zephyr_compile_options(-Werror)
# AMDECVER linker section only exists in the NPCX4 Plum board memory
# layout. On RTS5918 the corresponding header lands in BBRAM / the
# image-tool header instead, so the custom section is unnecessary.
if(CONFIG_SOC_SERIES_NPCX4)
zephyr_linker_sources(SECTIONS ${CMAKE_CURRENT_LIST_DIR}/linker/custom-sections.ld)
endif()
