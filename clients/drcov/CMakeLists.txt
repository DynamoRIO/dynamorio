# **********************************************************
# Copyright (c) 2010-2014 Google, Inc.    All rights reserved.
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

option(CBR_COVERAGE "build with conditional branch coverage checks" OFF)
if (CBR_COVERAGE)
  add_definitions(-DCBR_COVERAGE)
endif(CBR_COVERAGE)

# We do not need libc, and we avoid a 10x size increase in both our
# dll and pdb (plus we avoid stressing private library isolation) by
# not using it (i#714).
set(DynamoRIO_USE_LIBC OFF)

# add drcov client
if (STATIC_LIBRARY)
  set(libtype STATIC)
else()
  set(libtype SHARED)
endif ()

add_library(drcov ${libtype}
  drcov.c
  ../common/modules.c
  # add more here
  )
configure_DynamoRIO_client(drcov)
use_DynamoRIO_extension(drcov drmgr)
use_DynamoRIO_extension(drcov drx)
use_DynamoRIO_extension(drcov drcontainers)

# ensure we rebuild if includes change
add_dependencies(drcov api_headers)

# Provide a hint for how to use the client
if (NOT DynamoRIO_INTERNAL OR NOT "${CMAKE_GENERATOR}" MATCHES "Ninja")
  add_custom_command(TARGET drcov
    POST_BUILD
    COMMAND ${CMAKE_COMMAND}
    ARGS -E echo "Usage: pass to drconfig or drrun: -t drcov"
    VERBATIM)
endif ()

# add drcov2lcov
add_executable(drcov2lcov drcov2lcov.c)
# We do want an rpath, which is on by default for standalone
configure_DynamoRIO_standalone(drcov2lcov)
use_DynamoRIO_extension(drcov2lcov drsyms_static)
use_DynamoRIO_extension(drcov2lcov drcontainers)

# ensure we rebuild if includes change
add_dependencies(drcov2lcov api_headers)

# Provide a hint for running
if (NOT DynamoRIO_INTERNAL OR NOT "${CMAKE_GENERATOR}" MATCHES "Ninja")
  if (UNIX)
    set(FIND_MSG "(set LD_LIBRARY_PATH)")
  else (UNIX)
    set(FIND_MSG "(set PATH or copy to same directory)")
  endif (UNIX)
  add_custom_command(TARGET drcov2lcov
    POST_BUILD
    COMMAND ${CMAKE_COMMAND}
    ARGS -E echo "Make sure the loader finds the dynamorio library ${FIND_MSG}"
    VERBATIM)
endif ()

if (WIN32 AND GENERATE_PDBS)
  # I believe it's the lack of CMAKE_BUILD_TYPE that's eliminating this?
  # In any case we make sure to add it (for release and debug, to get pdb):
  append_property_string(TARGET drcov LINK_FLAGS "/debug")
  append_property_string(TARGET drcov2lcov LINK_FLAGS "/debug")
endif (WIN32 AND GENERATE_PDBS)

DR_export_target(drcov)
install_exported_target(drcov ${INSTALL_CLIENTS_LIB})
install_exported_target(drcov2lcov ${INSTALL_CLIENTS_BIN})
# Create a legacy-named copy of the executable
get_target_property(exe_path drcov2lcov LOCATION${location_suffix})
get_filename_component(legacy_exe "${exe_path}" NAME)
string(REGEX REPLACE "drcov" "bbcov" legacy_exe ${legacy_exe})
if (DEFINED DR_INSTALL_TARGETS_DEST)
  set(inst_dst ${DR_INSTALL_TARGETS_DEST})
else ()
  set(inst_dst ${INSTALL_CLIENTS_BIN})
endif ()
DR_install(FILES ${exe_path} DESTINATION ${inst_dst} RENAME ${legacy_exe})

# On Linux we rely on the rpath
if (WIN32)
  DR_install(DIRECTORY "${DR_LIBRARY_OUTPUT_DIRECTORY}/"
    DESTINATION "${INSTALL_CLIENTS_BIN}"
    FILES_MATCHING
    PATTERN "dynamorio.dll" PATTERN "libdynamorio.so*"
    PATTERN "lib*.debug" EXCLUDE)
endif (WIN32)

set(INSTALL_DRCOV_CONFIG ${INSTALL_CLIENTS_BASE})

# Originally this tool was called "bbcov", so we keep a legacy .drrun for back-compat.
if (X64)
  set(CONFIG ${PROJECT_BINARY_DIR}/drcov.drrun64)
  set(CONFIG_LEGACY bbcov.drrun64)
else (X64)
  set(CONFIG ${PROJECT_BINARY_DIR}/drcov.drrun32)
  set(CONFIG_LEGACY bbcov.drrun32)
endif (X64)

if (UNIX)
  set(LIB_EXT ".so")
  set(LIB_PFX "lib")
else (UNIX)
  set(LIB_EXT ".dll")
  set(LIB_PFX "")
endif (UNIX)

file(WRITE  ${CONFIG} "# drcov tool config file\n")
file(APPEND ${CONFIG} "# DynamoRIO options\n")
file(APPEND ${CONFIG} "DR_OP=-nop_initial_bblock\n")
file(APPEND ${CONFIG} "# client tool path\n")
file(APPEND ${CONFIG} "CLIENT_REL=${INSTALL_CLIENTS_LIB}/${LIB_PFX}drcov${LIB_EXT}\n")
file(APPEND ${CONFIG} "# client tool options\n")
file(APPEND ${CONFIG} "TOOL_OP=\n")

DR_install(FILES "${CONFIG}" DESTINATION ${INSTALL_DRCOV_CONFIG})
DR_install(FILES "${CONFIG}" RENAME "${CONFIG_LEGACY}" DESTINATION
  ${INSTALL_DRCOV_CONFIG})
register_tool_file("drcov")
