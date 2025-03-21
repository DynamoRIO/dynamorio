# **********************************************************
# Copyright (c) 2014-2020 Google, Inc.  All rights reserved.
# **********************************************************

# Dr. Memory: the memory debugger
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public
# License as published by the Free Software Foundation;
# version 2.1 of the License, and no later version.
#
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
# Library General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with this library; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

cmake_minimum_required(VERSION 3.7)

# Collapse VS generator into a single config, to match Makefiles.
if ("${CMAKE_GENERATOR}" MATCHES "Visual Studio")
  if ("${CMAKE_BUILD_TYPE}" MATCHES "Debug")
    set(CMAKE_CONFIGURATION_TYPES "Debug" CACHE STRING "" FORCE)
  else ()
    set(CMAKE_CONFIGURATION_TYPES "RelWithDebInfo" CACHE STRING "" FORCE)
  endif ()
  # We want to use the _LOCATION_<config> property
  string(TOUPPER "${CMAKE_CONFIGURATION_TYPES}" upper)
  set(location_suffix "_${upper}")
else ("${CMAKE_GENERATOR}" MATCHES "Visual Studio")
  set(location_suffix "")
endif ("${CMAKE_GENERATOR}" MATCHES "Visual Studio")

project(DRMF_samples)

set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/bin")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}")
if ("${CMAKE_GENERATOR}" MATCHES "Visual Studio")
  # We collapse the Debug and Release subdirs to match Makefile layout:
  foreach (config ${CMAKE_CONFIGURATION_TYPES})
    string(TOUPPER "${config}" config_upper)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${config_upper}
      "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${config_upper}
      "${CMAKE_ARCHIVE_OUTPUT_DIRECTORY}")
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_${config_upper}
      "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}")
  endforeach ()
endif ()

# If DR package is already set up, the "dynamorio" target should exist.
if (NOT TARGET dynamorio)
  if (NOT DEFINED DynamoRIO_DIR)
    # Try the location inside a DR+DRMF combined package.
    set(DynamoRIO_DIR "${PROJECT_SOURCE_DIR}/../../../cmake" CACHE PATH
      "DynamoRIO installation's cmake directory")
  endif ()
  find_package(DynamoRIO)
  if (NOT DynamoRIO_FOUND)
    message(FATAL_ERROR "DynamoRIO package required to build")
  endif ()
endif ()

# If DRMF package is already set up, the "drsyscall" target should exist.
if (NOT TARGET drsyscall)
  # For a combined DR+DRMF package we don't need this step.
  # For a separate DRMF package we do.
  if (NOT DEFINED DrMemoryFramework_DIR)
    set(DrMemoryFramework_DIR "${PROJECT_SOURCE_DIR}/.." CACHE PATH
      "Dr. Memory Framework cmake directory")
  endif ()
  find_package(DrMemoryFramework)
  if (NOT DrMemoryFramework_FOUND)
    message(FATAL_ERROR "DrMemoryFramework package required to build")
  endif ()
endif ()

# Build the samples themselves:

add_library(strace SHARED strace.c)
# We add the rpath so drsyscall is found within our build   # NON-COMBINED
# dir and standalone package.  This should not be needed in # NON-COMBINED
# a combined DR+DRMF package.                               # NON-COMBINED
if (WIN32 OR ANDROID)                                       # NON-COMBINED
  set(DynamoRIO_RPATH ON)                                   # NON-COMBINED
endif ()                                                    # NON-COMBINED
configure_DynamoRIO_client(strace)
use_DynamoRIO_extension(strace drmgr)
use_DynamoRIO_extension(strace drsyscall)
# Set link flags after configure_DynamoRIO_client.          # NON-COMBINED
if (NOT WIN32)                                              # NON-COMBINED
  DynamoRIO_add_rel_rpaths(strace drsyscall)                # NON-COMBINED
endif ()                                                    # NON-COMBINED

###########################################################################
# START NON-EXPORTED SECTION
# We also remove any lines above marked "NON-PUBLIC", and also
# "# NON-COMBINED" if building a combined package.

set(PROJECT_BINARY_DIR ${TOP_BINARY_DIR}) # fix Android paths
copy_target_to_device(strace)
copy_and_adjust_drpaths(${CMAKE_RUNTIME_OUTPUT_DIRECTORY} strace)

file(READ "${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt" string)
string(REGEX REPLACE "[^\n]*NON-PUBLIC[^\n]*\n" "" string "${string}")
if (BUILDING_SUB_PACKAGE)
  string(REGEX REPLACE "[^\n]*# NON-COMBINED[^\n]*\n" "" string "${string}")
else ()
  string(REGEX REPLACE "# NON-COMBINED" "" string "${string}")
endif ()
string(REGEX REPLACE "# START NON-EXPORTED SECTION.*$" "" string "${string}")
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/CMakeLists.txt.in" "${string}")
configure_file("${CMAKE_CURRENT_BINARY_DIR}/CMakeLists.txt.in"
  "${CMAKE_CURRENT_BINARY_DIR}/CMakeLists.txt"
  @ONLY)

if (BUILD_TOOL_TESTS)
  # Make this a test to ensure we don't break it.
  # We use the framework test "drsyscall_app" as the app, and just the
  # exit code as the success test.
  get_target_path_for_execution(client_path strace)
  get_target_path_for_execution(app_path drsyscall_app)
  if (DEBUG_BUILD)
    set(drrun_extra -debug)
  else ()
    set(drrun_extra "")
  endif ()
  set(drrun_path ${DynamoRIO_DIR}/../${BIN_ARCH}/drrun)
  convert_local_path_to_device_path(drrun_path ${drrun_path})
  prefix_cmd_if_necessary(drrun_path OFF ${drrun_path})
  add_test(strace_sample
    ${drrun_path} ${drrun_extra}
    -client ${client_path} 0 ""
    -msgbox_mask 0
    "--" ${app_path})
endif ()

set(INSTALL_SAMPLES ${INSTALL_PREFIX}${DRMF_BASEDIR}/samples)
if (X64)
  set(INSTALL_SAMPLES_BIN ${INSTALL_SAMPLES}/bin64)
else (X64)
  set(INSTALL_SAMPLES_BIN ${INSTALL_SAMPLES}/bin32)
endif (X64)

install(TARGETS strace DESTINATION ${INSTALL_SAMPLES_BIN})
install(FILES strace.c DESTINATION ${INSTALL_SAMPLES})
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/CMakeLists.txt"
  DESTINATION "${INSTALL_SAMPLES}")

install(DIRECTORY ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/
  DESTINATION ${INSTALL_SAMPLES_BIN}
  FILE_PERMISSIONS ${owner_access} OWNER_EXECUTE GROUP_READ GROUP_EXECUTE
  WORLD_READ WORLD_EXECUTE
  FILES_MATCHING
  PATTERN "*.debug"
  PATTERN "*.pdb"
  REGEX ".*.dSYM/.*DWARF/.*"
  )
