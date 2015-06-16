# **********************************************************
# Copyright (c) 2015 Google, Inc.    All rights reserved.
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
# * Neither the name of Google, Inc. nor the names of its contributors may be
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

# FIXME i#1703: finish implementing for Windows
if (UNIX)

  if (WIN32)
    set(os_name "win")
  else ()
    set(os_name "unix")
  endif ()

  add_executable(drcachesim
    simulator/simulator.cpp
    simulator/ipc_reader.cpp
    common/named_pipe_${os_name}.cpp
    simulator/cache.cpp
    simulator/cache_stats.cpp
    )

  add_library(drmemtrace SHARED
    tracer/tracer.cpp
    common/named_pipe_${os_name}.cpp
    )
  configure_DynamoRIO_client(drmemtrace)
  use_DynamoRIO_extension(drmemtrace drmgr)
  use_DynamoRIO_extension(drmemtrace drutil)
  use_DynamoRIO_extension(drmemtrace drx)

  # Restore debug and other flags to our non-client executable
  set_target_properties(drcachesim PROPERTIES
    COMPILE_FLAGS "${ORIG_CMAKE_CXX_FLAGS}")

  place_shared_lib_in_lib_dir(drmemtrace)

  add_dependencies(drmemtrace api_headers)

  # Provide a hint for how to use the client
  if (NOT DynamoRIO_INTERNAL OR NOT "${CMAKE_GENERATOR}" MATCHES "Ninja")
    add_custom_command(TARGET drmemtrace
      POST_BUILD
      COMMAND ${CMAKE_COMMAND}
      ARGS -E echo "Usage: pass to drconfig or drrun: -t drcachesim"
      VERBATIM)
  endif ()

  if (WIN32 AND GENERATE_PDBS)
    # XXX: this is in a lot of these files: can we address this once top-level?
    append_property_string(TARGET drmemtrace LINK_FLAGS "/debug")
    append_property_string(TARGET drcachesim LINK_FLAGS "/debug")
  endif (WIN32 AND GENERATE_PDBS)

  install_target(drmemtrace ${INSTALL_CLIENTS_LIB})
  install_target(drcachesim ${INSTALL_CLIENTS_BIN})

  set(INSTALL_DRCACHESIM_CONFIG ${INSTALL_CLIENTS_BASE})

  if (X64)
    set(CONFIG ${PROJECT_BINARY_DIR}/drcachesim.drrun64)
  else (X64)
    set(CONFIG ${PROJECT_BINARY_DIR}/drcachesim.drrun32)
  endif (X64)

  if (UNIX)
    set(LIB_EXT ".so")
    set(LIB_PFX "lib")
  else (UNIX)
    set(LIB_EXT ".dll")
    set(LIB_PFX "")
  endif (UNIX)

  file(WRITE  ${CONFIG} "# drcachesim tool config file\n")
  file(APPEND ${CONFIG} "# DynamoRIO options: none at this time\n")
  file(APPEND ${CONFIG} "DR_OP=\n")
  file(APPEND ${CONFIG} "# client tool path\n")
  file(APPEND ${CONFIG} "CLIENT_REL=${INSTALL_CLIENTS_LIB}/${LIB_PFX}drmemtrace${LIB_EXT}\n")
  file(APPEND ${CONFIG} "# client tool options\n")
  file(APPEND ${CONFIG} "TOOL_OP=\n")

  DR_install(FILES "${CONFIG}" DESTINATION ${INSTALL_DRCACHESIM_CONFIG})
  register_tool_file("drcachesim")

endif (UNIX)
