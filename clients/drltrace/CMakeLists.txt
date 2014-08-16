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

# We do not need libc, and we avoid a 10x size increase in both our
# dll and pdb (plus we avoid stressing private library isolation) by
# not using it (i#714).
set(DynamoRIO_USE_LIBC OFF)

add_library(drltrace SHARED drltrace.c)
configure_DynamoRIO_client(drltrace)
use_DynamoRIO_extension(drltrace drmgr)
use_DynamoRIO_extension(drltrace drwrap)
use_DynamoRIO_extension(drltrace drx)

# ensure we rebuild if includes change
add_dependencies(drltrace api_headers)

# Provide a hint for how to use the client
if (NOT DynamoRIO_INTERNAL OR NOT "${CMAKE_GENERATOR}" MATCHES "Ninja")
  add_custom_command(TARGET drltrace
    POST_BUILD
    COMMAND ${CMAKE_COMMAND}
    ARGS -E echo "Usage: pass to drconfig or drrun: -t drltrace"
    VERBATIM)
endif ()

if (WIN32 AND GENERATE_PDBS)
  # XXX: this is in a lot of these files: can we address this once top-level?
  append_property_string(TARGET drltrace LINK_FLAGS "/debug")
endif (WIN32 AND GENERATE_PDBS)

DR_export_target(drltrace)
install_exported_target(drltrace ${INSTALL_CLIENTS_LIB})

set(INSTALL_DRLTRACE_CONFIG ${INSTALL_CLIENTS_BASE})

if (X64)
  set(CONFIG ${PROJECT_BINARY_DIR}/drltrace.drrun64)
else (X64)
  set(CONFIG ${PROJECT_BINARY_DIR}/drltrace.drrun32)
endif (X64)

if (UNIX)
  set(LIB_EXT ".so")
  set(LIB_PFX "lib")
else (UNIX)
  set(LIB_EXT ".dll")
  set(LIB_PFX "")
endif (UNIX)

file(WRITE  ${CONFIG} "# drltrace tool config file\n")
file(APPEND ${CONFIG} "# DynamoRIO options: may as well optimize the bb lock\n")
file(APPEND ${CONFIG} "DR_OP=-nop_initial_bblock\n")
file(APPEND ${CONFIG} "# client tool path\n")
file(APPEND ${CONFIG} "CLIENT_REL=${INSTALL_CLIENTS_LIB}/${LIB_PFX}drltrace${LIB_EXT}\n")
file(APPEND ${CONFIG} "# client tool options\n")
file(APPEND ${CONFIG} "TOOL_OP=\n")

DR_install(FILES "${CONFIG}" DESTINATION ${INSTALL_DRLTRACE_CONFIG})
register_tool_file("drltrace")
