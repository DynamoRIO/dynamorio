# **********************************************************
# Copyright (c) 2023 Google, Inc.    All rights reserved.
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

cmake_minimum_required(VERSION 3.7)

include(../../../../../make/policies.cmake NO_POLICY_SCOPE)

add_dr_defines()

get_filename_component(DRCACHESIM_SOURCE_DIR
                       ${CMAKE_CURRENT_SOURCE_DIR}/../../.. ABSOLUTE)

add_exported_library(drmemtrace_elam STATIC
                     elam.cpp
                     ${DRCACHESIM_SOURCE_DIR}/common/options.cpp)
set_property(TARGET drmemtrace_elam PROPERTY POSITION_INDEPENDENT_CODE ON)
add_exported_library(elam_creator SHARED elam_creator.cpp)
target_link_libraries(elam_creator drmemtrace_elam)

place_shared_lib_in_lib_dir(elam_creator)

add_executable(elam_launcher elam_launcher.cpp)
_DR_get_static_libc_list(static_libc)
target_link_libraries(elam_launcher
                      drmemtrace_analyzer
                      drmemtrace_elam
                      ${static_libc})
use_DynamoRIO_extension(elam_launcher droption)
add_dependencies(elam_launcher api_headers)

# These add_win32_flags() calls are necessary only for building the project inside
# the DR top level project.
add_win32_flags(drmemtrace_elam)
add_win32_flags(elam_creator)
add_win32_flags(elam_launcher)

function (write_config_file name dst libdir)
  if (X64)
    string(REPLACE "lib64" "lib32" alt_libdir ${libdir})
    set(CUR_BIT "64")
    set(ALT_BIT "32")
  else ()
    set(CUR_BIT "32")
    set(ALT_BIT "64")
    string(REPLACE "lib64" "lib32" alt_libdir ${libdir})
  endif ()
  file(GENERATE OUTPUT ${dst} CONTENT
"# analysis tool creator config file\n\
TOOL_NAME=${name}
CREATOR_BIN${CUR_BIT}=${libdir}/${LIB_PFX}${name}_creator${LIB_EXT}\n\
CREATOR_BIN${ALT_BIT}=${alt_libdir}/${LIB_PFX}${name}_creator${LIB_EXT}\n\
")
endfunction ()

set(elam_CONFIG_INSTALL ${PROJECT_BINARY_DIR}/elam.drcachesim)
set(elam_CONFIG_BUILD ${PROJECT_BINARY_DIR}/tools/elam.drcachesim)
write_config_file(elam ${elam_CONFIG_BUILD} clients/${INSTALL_LIB})
write_config_file(elam ${elam_CONFIG_INSTALL} tools/${INSTALL_LIB})
DR_install(FILES "${elam_CONFIG_INSTALL}" DESTINATION ${INSTALL_CLIENTS_BASE})


##################################################
# Tests

set(test_seconds 90)

if (X86 AND X64 AND ZIP_FOUND)
# Simple test to ensure elam launcher keeps working.
  set(trace_dir
    "${PROJECT_SOURCE_DIR}/clients/drcachesim/tests/drmemtrace.threadsig.x64.tracedir")
  add_test(NAME tool.elam_launcher
    COMMAND elam_launcher -trace ${trace_dir})
  set_tests_properties(tool.elam_launcher PROPERTIES TIMEOUT ${test_seconds})

  # Simple test to ensure elam tool can be loaded by drcachesim.
  set(trace_dir
    "${PROJECT_SOURCE_DIR}/clients/drcachesim/tests/drmemtrace.threadsig.x64.tracedir")
  add_test(NAME drcachesim.elam_load
    COMMAND ${PROJECT_BINARY_DIR}/bin64/drrun -t drcachesim -offline
     -simulator_type elam -indir ${trace_dir})
  set(elam_regex "elam tool created\nelam tool results:")
  set_tests_properties(drcachesim.elam_load
    PROPERTIES PASS_REGULAR_EXPRESSION "${elam_regex}")

  # Simple test to ensure non-existent tool load by drcachesim should fail.
  add_test(NAME drcachesim.non-existent_load
    COMMAND ${PROJECT_BINARY_DIR}/bin64/drrun -t drcachesim -offline
     -simulator_type non-existent -indir ${trace_dir})
  set(nonexistent_regex "Usage error: unsupported analyzer type \"non-existent\". Please choose cache, miss_analyzer, TLB, histogram, reuse_distance, basic_counts, opcode_mix, syscall_mix, view, func_view, or some external analyzer.\nERROR: failed to initialize analyzer: Failed to create analysis tool:")
  set_tests_properties(drcachesim.non-existent_load
    PROPERTIES PASS_REGULAR_EXPRESSION "${nonexistent_regex}")
endif ()
