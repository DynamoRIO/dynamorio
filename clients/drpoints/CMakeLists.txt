# **********************************************************
# Copyright (c) 2025 Google, Inc.    All rights reserved.
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

cmake_minimum_required(VERSION 3.14)

include(../../make/policies.cmake NO_POLICY_SCOPE)

# We do not need libc, and we avoid a 10x size increase in both our
# dll and pdb (plus we avoid stressing private library isolation) by
# not using it (i#714).
set(DynamoRIO_USE_LIBC OFF)

# Add drpoints client.
add_library(drpoints SHARED drpoints.cpp)
configure_DynamoRIO_client(drpoints)
use_DynamoRIO_extension(drpoints drmgr)
use_DynamoRIO_extension(drpoints drx)
use_DynamoRIO_extension(drpoints drcontainers)
use_DynamoRIO_extension(drpoints droption)
add_dependencies(drpoints api_headers)

# Provide a hint for how to use the client.
if (NOT DynamoRIO_INTERNAL OR NOT "${CMAKE_GENERATOR}" MATCHES "Ninja")
  add_custom_command(TARGET drpoints
    POST_BUILD
    COMMAND ${CMAKE_COMMAND}
    ARGS -E echo "Usage: pass to drrun: -t drpoints"
    VERBATIM)
endif ()

install_target(drpoints ${INSTALL_CLIENTS_LIB})

set(INSTALL_DRPOINTS_CONFIG ${INSTALL_CLIENTS_BASE})

if (X64)
  set(CONFIG_INSTALL ${PROJECT_BINARY_DIR}/drpoints.drrun64)
  set(CONFIG_BUILD ${PROJECT_BINARY_DIR}/tools/drpoints.drrun64)
else (X64)
  set(CONFIG_INSTALL ${PROJECT_BINARY_DIR}/drpoints.drrun32)
  set(CONFIG_BUILD ${PROJECT_BINARY_DIR}/tools/drpoints.drrun32)
endif (X64)

function (write_config_file dst libdir)
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
  file(WRITE  ${dst} "# drpoints tool config file\n")
  file(APPEND ${dst} "# client tool path\n")
  file(APPEND ${dst} "CLIENT${CUR_BIT}_REL=${libdir}/${LIB_PFX}drpoints${LIB_EXT}\n")
  file(APPEND ${dst} "CLIENT${ALT_BIT}_REL=${alt_libdir}/${LIB_PFX}drpoints${LIB_EXT}\n")
  file(APPEND ${dst} "# client tool options\n")
  file(APPEND ${dst} "TOOL_OP=\n")
endfunction ()

set(BUILD_CLIENTS_LIB clients/${INSTALL_LIB})

write_config_file(${CONFIG_INSTALL} ${INSTALL_CLIENTS_LIB})
write_config_file(${CONFIG_BUILD} ${BUILD_CLIENTS_LIB})

DR_install(FILES "${CONFIG_INSTALL}" DESTINATION ${INSTALL_DRPOINTS_CONFIG})
register_tool_file("drpoints")
