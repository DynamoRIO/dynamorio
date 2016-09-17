# **********************************************************
# Copyright (c) 2015-2016 Google, Inc.    All rights reserved.
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

include(../../make/policies.cmake NO_POLICY_SCOPE)

# FIXME i#1727: finish implementing for Windows
if (UNIX)

  if (WIN32)
    set(os_name "win")
  else ()
    set(os_name "unix")
  endif ()

  add_executable(drcachesim
    launcher.cpp
    analyzer.cpp
    reader/reader.cpp
    reader/file_reader.cpp
    reader/ipc_reader.cpp
    simulator/simulator.cpp
    common/named_pipe_${os_name}.cpp
    common/options.cpp
    common/trace_entry.cpp
    simulator/cache.cpp
    simulator/cache_lru.cpp
    simulator/cache_fifo.cpp
    simulator/caching_device.cpp
    simulator/caching_device_stats.cpp
    simulator/cache_stats.cpp
    simulator/cache_simulator.cpp
    simulator/tlb.cpp
    simulator/tlb_simulator.cpp
    )
  target_link_libraries(drcachesim drinjectlib drconfiglib drfrontendlib)
  use_DynamoRIO_extension(drcachesim droption)

  add_library(drmemtrace SHARED
    tracer/tracer.cpp
    tracer/physaddr.cpp
    common/named_pipe_${os_name}.cpp
    common/options.cpp
    common/trace_entry.cpp
    )
  configure_DynamoRIO_client(drmemtrace)
  use_DynamoRIO_extension(drmemtrace drmgr)
  use_DynamoRIO_extension(drmemtrace drreg)
  use_DynamoRIO_extension(drmemtrace drutil)
  use_DynamoRIO_extension(drmemtrace drx)
  use_DynamoRIO_extension(drmemtrace droption)

  # Restore debug and other flags to our non-client executable
  set_target_properties(drcachesim PROPERTIES
    COMPILE_FLAGS "${ORIG_CMAKE_CXX_FLAGS}")
  if (NOT DEBUG)
    append_property_list(TARGET drcachesim COMPILE_DEFINITIONS "NDEBUG")
  endif ()
  # However, we need the target os and arch defines (XXX: better way?) for
  # the config, inject, and frontend headers:
  DynamoRIO_extra_cflags(extra_cflags "" ON)
  append_property_string(TARGET drcachesim COMPILE_FLAGS "${extra_cflags}")

  place_shared_lib_in_lib_dir(drmemtrace)

  add_dependencies(drmemtrace api_headers)
  add_dependencies(drcachesim api_headers)

  # Provide a hint for how to use the client
  if (NOT DynamoRIO_INTERNAL OR NOT "${CMAKE_GENERATOR}" MATCHES "Ninja")
    add_custom_command(TARGET drmemtrace
      POST_BUILD
      COMMAND ${CMAKE_COMMAND}
      ARGS -E echo "Usage: pass to drconfig or drrun: -t drcachesim"
      VERBATIM)
  endif ()

  install_target(drmemtrace ${INSTALL_CLIENTS_LIB})
  install_target(drcachesim ${INSTALL_CLIENTS_BIN})

  set(INSTALL_DRCACHESIM_CONFIG ${INSTALL_CLIENTS_BASE})

  function (write_config_file dst bindir libdir)
    file(WRITE  ${dst} "# drcachesim tool config file\n")
    file(APPEND ${dst} "FRONTEND_REL=${bindir}/drcachesim\n")
    file(APPEND ${dst} "TOOL_OP=-dr\n")
    file(APPEND ${dst} "TOOL_OP_DR_PATH\n")
    file(APPEND ${dst} "TOOL_OP_DR_BUNDLE=-dr_ops\n")
    file(APPEND ${dst} "TOOL_OP=-tracer\n")
    file(APPEND ${dst} "CLIENT_REL=${libdir}/${LIB_PFX}drmemtrace${LIB_EXT}\n")
    if (DEBUG)
      file(APPEND ${dst} "TOOL_OP=-dr_debug\n")
    endif ()
  endfunction ()

  if (X64)
    set(CONFIG_INSTALL ${PROJECT_BINARY_DIR}/drcachesim.drrun64)
    set(CONFIG_BUILD ${PROJECT_BINARY_DIR}/tools/drcachesim.drrun64)
  else (X64)
    set(CONFIG_INSTALL ${PROJECT_BINARY_DIR}/drcachesim.drrun32)
    set(CONFIG_BUILD ${PROJECT_BINARY_DIR}/tools/drcachesim.drrun32)
  endif (X64)

  set(BUILD_CLIENTS_BIN clients/${INSTALL_BIN})
  set(BUILD_CLIENTS_LIB clients/${INSTALL_LIB})

  write_config_file(${CONFIG_INSTALL} ${INSTALL_CLIENTS_BIN} ${INSTALL_CLIENTS_LIB})
  write_config_file(${CONFIG_BUILD} ${BUILD_CLIENTS_BIN} ${BUILD_CLIENTS_LIB})

  DR_install(FILES "${CONFIG_INSTALL}" DESTINATION ${INSTALL_DRCACHESIM_CONFIG})
  register_tool_file("drcachesim")

endif (UNIX)

##################################################
# Documentation

# We auto-generate the list of options in the html docs via this helper app:
add_executable(drcachesim_ops
  optionlist.cpp
  common/options.cpp)
if (WIN32)
  # There was no client yet, so ORIG_ is empty: plus it doesn't have /EHsc.
  append_property_string(TARGET drcachesim_ops COMPILE_FLAGS "/EHsc")
else ()
  set_target_properties(drcachesim_ops PROPERTIES COMPILE_FLAGS "${ORIG_CMAKE_CXX_FLAGS}")
endif ()
use_DynamoRIO_extension(drcachesim_ops droption)

get_target_property(ops_path drcachesim_ops LOCATION${location_suffix})

# We then have to insert it into the doxygen files at build time:
set(srcdoc ${CMAKE_CURRENT_SOURCE_DIR}/drcachesim.dox.in)
set(gendoc ${CMAKE_CURRENT_BINARY_DIR}/drcachesim.dox)
set(doctgt drcachesim_docs)

get_property(dox_extras GLOBAL PROPERTY DynamoRIO_dox_extras)
set_property(GLOBAL PROPERTY DynamoRIO_dox_extras ${dox_extras} ${gendoc})

get_property(dox_targets GLOBAL PROPERTY DynamoRIO_dox_targets)
set_property(GLOBAL PROPERTY DynamoRIO_dox_targets ${dox_targets} ${doctgt})

add_custom_target(${doctgt} DEPENDS ${gendoc})
add_custom_command(
  OUTPUT ${gendoc}
  DEPENDS ${srcdoc}
  drcachesim_ops
  common/options.h
  COMMAND ${CMAKE_COMMAND}
  ARGS -D src=${srcdoc}
       -D dst=${gendoc}
       -D prog=${ops_path}
       -P ${CMAKE_CURRENT_SOURCE_DIR}/../common/gendocs.cmake
  VERBATIM)
