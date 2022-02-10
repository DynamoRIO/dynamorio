# **********************************************************
# Copyright (c) 2015-2020 Google, Inc.    All rights reserved.
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

include(../../make/policies.cmake NO_POLICY_SCOPE)

add_library(drcpusim SHARED
  drcpusim.cpp
  options.cpp
  )
configure_DynamoRIO_client(drcpusim)
use_DynamoRIO_extension(drcpusim drmgr)
use_DynamoRIO_extension(drcpusim drutil)
use_DynamoRIO_extension(drcpusim drx)
use_DynamoRIO_extension(drcpusim droption)

place_shared_lib_in_lib_dir(drcpusim)

add_dependencies(drcpusim api_headers)

# Provide a hint for how to use the client
if (NOT DynamoRIO_INTERNAL OR NOT "${CMAKE_GENERATOR}" MATCHES "Ninja")
  add_custom_command(TARGET drcpusim
    POST_BUILD
    COMMAND ${CMAKE_COMMAND}
    ARGS -E echo "Usage: pass to drconfig or drrun: -t drcpusim"
    VERBATIM)
endif ()

install_target(drcpusim ${INSTALL_CLIENTS_LIB})

set(INSTALL_DRCPUSIM_CONFIG ${INSTALL_CLIENTS_BASE})

function (write_config_file dst bindir libdir)
  # We include the alternate-bitwidth path, though it won't be there for
  # a single build dir and such a child will have a fatal error.
  if (X64)
    string(REPLACE "lib64" "lib32" alt_libdir ${libdir})
    set(CUR_BIT "64")
    set(ALT_BIT "32")
  else ()
    set(CUR_BIT "32")
    set(ALT_BIT "64")
    string(REPLACE "lib64" "lib32" alt_libdir ${libdir})
  endif ()
  file(WRITE  ${dst} "# drcpusim tool config file\n")
  file(APPEND ${dst} "CLIENT${CUR_BIT}_REL=${libdir}/${LIB_PFX}drcpusim${LIB_EXT}\n")
  file(APPEND ${dst} "CLIENT${ALT_BIT}_REL=${alt_libdir}/${LIB_PFX}drcpusim${LIB_EXT}\n")
endfunction ()

if (X64)
  set(CONFIG_INSTALL ${PROJECT_BINARY_DIR}/drcpusim.drrun64)
  set(CONFIG_BUILD ${PROJECT_BINARY_DIR}/tools/drcpusim.drrun64)
else (X64)
  set(CONFIG_INSTALL ${PROJECT_BINARY_DIR}/drcpusim.drrun32)
  set(CONFIG_BUILD ${PROJECT_BINARY_DIR}/tools/drcpusim.drrun32)
endif (X64)

set(BUILD_CLIENTS_BIN clients/${INSTALL_BIN})
set(BUILD_CLIENTS_LIB clients/${INSTALL_LIB})

write_config_file(${CONFIG_INSTALL} ${INSTALL_CLIENTS_BIN} ${INSTALL_CLIENTS_LIB})
write_config_file(${CONFIG_BUILD} ${BUILD_CLIENTS_BIN} ${BUILD_CLIENTS_LIB})

DR_install(FILES "${CONFIG_INSTALL}" DESTINATION ${INSTALL_DRCPUSIM_CONFIG})
register_tool_file("drcpusim")

##################################################
# Documentation

# We auto-generate the list of options in the html docs via this helper app:
add_executable(drcpusim_ops
  optionlist.cpp
  options.cpp)
set_target_properties(drcpusim_ops PROPERTIES COMPILE_FLAGS "${ORIG_CMAKE_CXX_FLAGS}")
if (WIN32)
  # ORIG_ doesn't have /EHsc so I'm not sure where it comes from for a regular app?
  append_property_string(TARGET drcpusim_ops COMPILE_FLAGS "/EHsc")
endif ()
use_DynamoRIO_extension(drcpusim_ops droption)

# We then have to insert it into the doxygen files at build time:
set(srcdoc ${CMAKE_CURRENT_SOURCE_DIR}/drcpusim.dox.in)
set(gendoc ${CMAKE_CURRENT_BINARY_DIR}/drcpusim.dox)
set(doctgt drcpusim_docs)

get_property(dox_extras GLOBAL PROPERTY DynamoRIO_dox_extras)
set_property(GLOBAL PROPERTY DynamoRIO_dox_extras ${dox_extras} ${gendoc})

get_property(dox_targets GLOBAL PROPERTY DynamoRIO_dox_targets)
set_property(GLOBAL PROPERTY DynamoRIO_dox_targets ${dox_targets} ${doctgt})

add_custom_target(${doctgt} DEPENDS ${gendoc})
add_custom_command(
  OUTPUT ${gendoc}
  DEPENDS ${srcdoc}
  drcpusim_ops
  options.h
  COMMAND ${CMAKE_COMMAND}
  ARGS -D src=${srcdoc}
       -D dst=${gendoc}
       -D CMAKE_CROSSCOMPILING=${CMAKE_CROSSCOMPILING}
       -D prog=$<TARGET_FILE:drcpusim_ops>
       -P ${CMAKE_CURRENT_SOURCE_DIR}/../common/gendocs.cmake
  VERBATIM)
