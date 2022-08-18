# **********************************************************
# Copyright (c) 2010-2022 Google, Inc.    All rights reserved.
# Copyright (c) 2010 VMware, Inc.    All rights reserved.
# **********************************************************

# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#
# * Redistributions of source code must retain the above copyright notice,
#   this list of conditions and the following disclaimer.
#
# * Redistributions in binary form must reproduce the above copyright notice,
#   this list of conditions and the following disclaimer in the documentation
#   and/or other materials provided with the distribution.
#
# * Neither the name of VMware, Inc. nor the names of its contributors may be
#   used to endorse or promote products derived from this software without
#   specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED. IN NO EVENT SHALL VMWARE, INC. OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
# SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
# CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
# OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
# DAMAGE.

cmake_minimum_required(VERSION 3.7)

set(DynamoRIO_INTERNAL ON) # do not import dynamorio lib target
set(DynamoRIO_DIR ${PROJECT_BINARY_DIR}/cmake)
find_package(DynamoRIO ${VERSION_NUMBER_MAJOR}.${VERSION_NUMBER_MINOR})
if (NOT DynamoRIO_FOUND)
  message(FATAL_ERROR "DynamoRIO package required to build")
endif(NOT DynamoRIO_FOUND)

set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/clients/${INSTALL_LIB}")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}")
# We need the bin dir to match the install bin dir relative to the DR lib,
# so our relative rpath works on Linux.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/clients/${INSTALL_BIN}")
set_per_config_ouput_to_match_single_config()

# For end-user tools build on top of DR
# We use clients/ in the build dir where we already have tools/ for utilities
# used by DR. In our install/package, however, we use tools/ instead because
# we want to distinguish from the sample clients.
set(INSTALL_CLIENTS_BASE tools)
set(INSTALL_CLIENTS_BIN ${INSTALL_CLIENTS_BASE}/${INSTALL_BIN})
set(INSTALL_CLIENTS_LIB ${INSTALL_CLIENTS_BASE}/${INSTALL_LIB})
set(INSTALL_CLIENTS_CONFIG ${INSTALL_CLIENTS_BASE})
set(INSTALL_CLIENTS_CMAKE ${INSTALL_CLIENTS_BASE}/cmake)
# We install header files meant for use by DR clients to ext/include
set(INSTALL_CLIENTS_DR_INCLUDE ${INSTALL_CLIENTS_BASE}/../ext/include)
# For non-DR-client headers we use tools/include in the install package.
set(INSTALL_CLIENTS_NONDR_INCLUDE ${INSTALL_CLIENTS_BASE}/include)

macro(install_client_DR_header header)
  DR_install(FILES ${header} DESTINATION ${INSTALL_CLIENTS_DR_INCLUDE})
  # We also need a copy in the build dir for our --build-and-test (i#1586)
  get_filename_component(file ${header} NAME)
  configure_file(${header} ${PROJECT_BINARY_DIR}/ext/include/${file} COPYONLY)
endmacro()

macro(install_client_nonDR_header dest_dir src_header)
  DR_install(FILES ${src_header} DESTINATION ${INSTALL_CLIENTS_NONDR_INCLUDE}/${dest_dir})
  # We also need a copy in the build dir for our --build-and-test (i#1586)
  get_filename_component(file ${src_header} NAME)
  configure_file(${src_header} ${PROJECT_BINARY_DIR}/clients/include/${dest_dir}/${file}
    COPYONLY)
endmacro()

disable_compiler_warnings()
# Clients don't include configure.h so they don't get DR defines
add_dr_defines()

if (WIN32)
  # Avoid a race on competing copies of dynamorio.dll to the bin dir from
  # drcachsim and drcov via a higher-level target here (i#4668).
  add_custom_target(client_dr_copy ALL
    DEPENDS ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/dynamorio.dll)
  add_custom_command(OUTPUT ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/dynamorio.dll
    DEPENDS dynamorio
    COMMAND ${CMAKE_COMMAND}
    ARGS -E copy ${DR_LIBRARY_OUTPUT_DIRECTORY}/dynamorio.dll
    ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/dynamorio.dll
    VERBATIM)
endif ()

file(GLOB dirs "*/CMakeLists.txt")
foreach (dir ${dirs})
  get_filename_component(dir ${dir} PATH)
  # FIXME i#1874: named pipe not working on Android.
  # We should build anyway since -offline should work.
  # This also prevents us from building the docs on Android!
  if (NOT ANDROID OR NOT ${dir} MATCHES "drcachesim")
    # FIXME i#1732: drcpusim has no ARM support yet
    if (NOT ANDROID OR NOT ${dir} MATCHES "drcpusim")
      add_subdirectory(${dir})
    endif ()
  endif ()
endforeach (dir)

# install subdirs
install_subdirs(${INSTALL_CLIENTS_LIB} ${INSTALL_CLIENTS_BIN}
  PATTERN "histogram_launcher.debug" EXCLUDE
  PATTERN "drcachesim_ops.debug" EXCLUDE
  PATTERN "drcpusim_ops.debug" EXCLUDE
  )

# propagate to parent dir
set(exported_targets_append "${exported_targets_append}" PARENT_SCOPE)
