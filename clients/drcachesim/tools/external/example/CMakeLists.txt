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

macro (add_exported_library name type)
  add_library(${name} ${type} ${ARGN})
  DR_export_target(${name})
  install_exported_target(${name} ${INSTALL_CLIENTS_LIB})
  add_dependencies(${name} api_headers)
endmacro ()

get_filename_component(DRCACHESIM_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../.. ABSOLUTE)

add_exported_library(drmemtrace_empty STATIC empty.cpp ${DRCACHESIM_SOURCE_DIR}/common/options.cpp)
set_property(TARGET drmemtrace_empty PROPERTY POSITION_INDEPENDENT_CODE ON)
add_exported_library(empty_creator SHARED empty_creator.cpp)
target_link_libraries(empty_creator drmemtrace_empty)

place_shared_lib_in_lib_dir(empty_creator)

# We need to pass /EHsc and we pull in libcmtd into drcachesim from a dep lib.
# Thus we need to override the /MT with /MTd.
#
# TODO i#6040: We should add a new configure_original_flage(target) function
# which restores the ORIG_ flags at the source level for all other targets
# in this file which are neither clients nor use DR standalone. We can then
# get rid of add_win32_flags(target) since that just adds a couple of flags
# back, and using the ORIG_ values is preferable.
macro(add_win32_flags target)
  if (DEBUG AND NOT DEFINED ${target}_uses_configure)
    append_property_list(TARGET ${target} COMPILE_DEFINITIONS "DEBUG")
  endif ()
  if (WIN32)
    if (DEBUG)
      get_property(cur TARGET ${target} PROPERTY COMPILE_FLAGS)
      string(REPLACE "/MT " "" cur "${cur}") # Avoid override warning.
      set_target_properties(${target} PROPERTIES COMPILE_FLAGS "${cur} /EHsc /MTd /Zi")
      append_property_string(TARGET ${target} LINK_FLAGS "/nodefaultlib:libcmt")
    else ()
      append_property_string(TARGET ${target} COMPILE_FLAGS "/EHsc /MT")
    endif ()
  else ()
    # Work around configure_DynamoRIO_static() clobbering flags by re-adding
    # the important ones for our tests, so they can include our headers
    # with C++11-isms.
    get_property(cur TARGET ${target} PROPERTY COMPILE_FLAGS)
    if (NOT cur MATCHES "-std=")
      append_property_string(TARGET ${target} COMPILE_FLAGS "-std=c++11")
    endif ()
    if (DEBUG AND NOT cur MATCHES "-g")
      # We can't use restore_nonclient_flags() for clients, so we manually re-add debug
      # here.
      append_property_string(TARGET ${target} COMPILE_FLAGS "-g")
    endif ()
    if (APPLE)
      # Match the core/ flags.
      append_property_string(TARGET ${target}
        COMPILE_FLAGS "-mmacosx-version-min=${OLDEST_OSX_SUPPPORTED}")
    endif ()
  endif ()
endmacro ()

add_executable(empty_launcher empty_launcher.cpp)
_DR_get_static_libc_list(static_libc)
target_link_libraries(empty_launcher drmemtrace_analyzer drmemtrace_empty ${static_libc})
use_DynamoRIO_extension(empty_launcher droption)
add_dependencies(empty_launcher api_headers)

add_win32_flags(drmemtrace_empty)
add_win32_flags(empty_creator)
add_win32_flags(empty_launcher)

function (write_config_file id dst libdir)
  file(GENERATE OUTPUT ${dst} CONTENT
"# analysis tool creator config file\n\
TOOL_ID=${id}
CREATOR_BIN=${libdir}/${LIB_PFX}${id}_creator${LIB_EXT}\n\
")
endfunction ()

set(EMPTY_CONFIG_INSTALL ${PROJECT_BINARY_DIR}/empty.drcachesim)
set(EMPTY_CONFIG_BUILD ${PROJECT_BINARY_DIR}/tools/empty.drcachesim)
write_config_file(empty ${EMPTY_CONFIG_BUILD} clients/${INSTALL_LIB})
write_config_file(empty ${EMPTY_CONFIG_INSTALL} tools/${INSTALL_LIB})
DR_install(FILES "${EMPTY_CONFIG_INSTALL}" DESTINATION ${INSTALL_CLIENTS_BASE})
