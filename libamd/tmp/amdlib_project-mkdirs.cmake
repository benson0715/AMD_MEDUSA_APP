# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file LICENSE.rst or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION ${CMAKE_VERSION}) # this file comes with cmake

# If CMAKE_DISABLE_SOURCE_CHANGES is set to true and the source directory is an
# existing directory in our source tree, calling file(MAKE_DIRECTORY) on it
# would cause a fatal error, even though it would be a no-op.
if(NOT EXISTS "/home/benson0715/forward_3.7.1/ecfw-zephyr-amd/libamd")
  file(MAKE_DIRECTORY "/home/benson0715/forward_3.7.1/ecfw-zephyr-amd/libamd")
endif()
file(MAKE_DIRECTORY
  "/home/benson0715/forward_3.7.1/ecfw-zephyr-amd/libamd"
  "/home/benson0715/forward_3.7.1/ecfw-zephyr-amd/libamd"
  "/home/benson0715/forward_3.7.1/ecfw-zephyr-amd/libamd/tmp"
  "/home/benson0715/forward_3.7.1/ecfw-zephyr-amd/libamd/src/amdlib_project-stamp"
  "/home/benson0715/forward_3.7.1/ecfw-zephyr-amd/libamd/src"
  "/home/benson0715/forward_3.7.1/ecfw-zephyr-amd/libamd/src/amdlib_project-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "/home/benson0715/forward_3.7.1/ecfw-zephyr-amd/libamd/src/amdlib_project-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "/home/benson0715/forward_3.7.1/ecfw-zephyr-amd/libamd/src/amdlib_project-stamp${cfgdir}") # cfgdir has leading slash
endif()
