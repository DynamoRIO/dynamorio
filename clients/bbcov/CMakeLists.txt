# **********************************************************
# Copyright (c) 2010-2013 Google, Inc.    All rights reserved.
# Copyright (c) 2009-2010 VMware, Inc.    All rights reserved.
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

cmake_minimum_required(VERSION 2.6)

option(CBR_COVERAGE, "build with conditional branch coverage checks" OFF)
if (CBR_COVERAGE)
  add_definitions(-DCBR_COVERAGE)
endif(CBR_COVERAGE)

# add bbcov client
if (STATIC_LIBRARY)
  set(libtype STATIC)
else()
  set(libtype SHARED)
endif ()

add_library(bbcov ${libtype} bbcov.c)
configure_DynamoRIO_client(bbcov)
use_DynamoRIO_extension(bbcov drmgr)
use_DynamoRIO_extension(bbcov drcontainers)

# ensure we rebuild if includes change
add_dependencies(bbcov api_headers)

# Provide a hint for how to use the client
if (NOT DynamoRIO_INTERNAL OR NOT "${CMAKE_GENERATOR}" MATCHES "Ninja")
  add_custom_command(TARGET bbcov
    POST_BUILD
    COMMAND ${CMAKE_COMMAND}
    ARGS -E echo "Usage: pass to drconfig or drrun: -t bbcov"
    VERBATIM)
endif ()

# add bbcov2lcov
add_executable(bbcov2lcov bbcov2lcov.c)
# We don't want an rpath as it makes it hard to switch
# between debug and release at runtime (rpath is removed
# on "make install"; here we avoid in build dir).
# DynamoRIOConfig.cmake sets for standalone but not lib.
set(DynamoRIO_RPATH OFF)
configure_DynamoRIO_standalone(bbcov2lcov)
use_DynamoRIO_extension(bbcov2lcov drsyms)
use_DynamoRIO_extension(bbcov2lcov drcontainers)

# ensure we rebuild if includes change
add_dependencies(bbcov2lcov api_headers)

# Provide a hint for running
if (NOT DynamoRIO_INTERNAL OR NOT "${CMAKE_GENERATOR}" MATCHES "Ninja")
  if (UNIX)
    set(FIND_MSG "(set LD_LIBRARY_PATH)")
  else (UNIX)
    set(FIND_MSG "(set PATH or copy to same directory)")
  endif (UNIX)
  add_custom_command(TARGET bbcov2lcov
    POST_BUILD
    COMMAND ${CMAKE_COMMAND}
    ARGS -E echo "Make sure the loader finds the dynamorio and drsyms library ${FIND_MSG}"
    VERBATIM)
endif ()

if (WIN32 AND GENERATE_PDBS)
  # I believe it's the lack of CMAKE_BUILD_TYPE that's eliminating this?
  # In any case we make sure to add it (for release and debug, to get pdb):
  append_property_string(TARGET bbcov LINK_FLAGS "/debug")
  append_property_string(TARGET bbcov2lcov LINK_FLAGS "/debug")
endif (WIN32 AND GENERATE_PDBS)

DR_export_target(bbcov)
install_exported_target(bbcov ${INSTALL_CLIENTS_LIB})
install_exported_target(bbcov2lcov ${INSTALL_CLIENTS_BIN})

DR_install(DIRECTORY "${DR_LIBRARY_OUTPUT_DIRECTORY}/"
  DESTINATION "${INSTALL_CLIENTS_BIN}"
  FILES_MATCHING
  PATTERN "*dynamorio*")
if (NOT DR_EXT_DRSYMS_STATIC)
  DR_install(DIRECTORY
    "${DR_LIBRARY_OUTPUT_DIRECTORY}/../../ext/${INSTALL_LIB}/"
    DESTINATION "${INSTALL_CLIENTS_BIN}"
    FILES_MATCHING
    PATTERN "*drsyms*")
endif (NOT DR_EXT_DRSYMS_STATIC)

set(INSTALL_BBCOV_CONFIG ${INSTALL_CLIENTS_BASE})

if (X64)
  set(CONFIG ${PROJECT_BINARY_DIR}/bbcov.drrun64)
else (X64)
  set(CONFIG ${PROJECT_BINARY_DIR}/bbcov.drrun32)
endif (X64)

if (UNIX)
  set(LIB_EXT ".so")
  set(LIB_PFX "lib")
else (UNIX)
  set(LIB_EXT ".dll")
  set(LIB_PFX "")
endif (UNIX)

file(WRITE  ${CONFIG} "# bbcov tool config file\n")
file(APPEND ${CONFIG} "# DynamoRIO options\n")
file(APPEND ${CONFIG} "DR_OP=-nop_initial_bblock\n")
file(APPEND ${CONFIG} "# client tool path\n")
file(APPEND ${CONFIG} "CLIENT_REL=${INSTALL_CLIENTS_LIB}/${LIB_PFX}bbcov${LIB_EXT}\n")
file(APPEND ${CONFIG} "# client tool options\n")
file(APPEND ${CONFIG} "TOOL_OP=\n")

DR_install(FILES "${CONFIG}" DESTINATION ${INSTALL_BBCOV_CONFIG})