# **********************************************************
# Copyright (c) 2013 Google, Inc.    All rights reserved.
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
# ARE DISCLAIMED. IN NO EVENT SHALL GOOGLE, INC. OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
# SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
# CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
# OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
# DAMAGE.

cmake_minimum_required(VERSION 2.6)

# DynamoRIO eXtension utilities

# We do not need libc, and we avoid a 10x size increase in both our
# dll and pdb (plus we avoid stressing private library isolation) by
# not using it (i#714).
set(DynamoRIO_USE_LIBC OFF)

option(DR_EXT_DRX_STATIC "create drx as a static, not shared, library (N.B.: ensure no separately-linked components of your tool also use drx before enabling as a static library)" ${BUILD_EXT_STATIC})
if (DR_EXT_DRX_STATIC OR STATIC_LIBRARY)
  set(libtype STATIC)
else()
  set(libtype SHARED)
endif ()
add_library(drx ${libtype}
  drx.c
  # add more here
  )
# while private loader means preferred base is not required, more efficient
# to avoid rebase so we avoid conflict w/ client and other exts
set(PREFERRED_BASE 0x77000000)
configure_DynamoRIO_client(drx)

use_DynamoRIO_extension(drx drcontainers)

# While we use drmgr in our implementation of drx, we only use pieces that
# do not rely on drmgr's 4 phases or event orderings, and we do not include
# drmgr.h in drx.h, all so that drx users need not use drmgr.
use_DynamoRIO_extension(drx drmgr)

include(../../make/ntdll_imports.cmake)
target_link_libraries(drx "${ntimp_lib}")
if ("${CMAKE_GENERATOR}" MATCHES "Visual Studio")
  # for parallel build correctness we need a target dependence
  add_dependencies(drx ntimp_tgt)
endif ()

# ensure we rebuild if includes change
add_dependencies(drx api_headers)
if (UNIX)
  # static libs must be PIC to be linked into clients: else requires
  # relocations that run afoul of security policies, etc.
  append_property_string(TARGET drx COMPILE_FLAGS "-fPIC")
endif (UNIX)

if (WIN32 AND GENERATE_PDBS)
  # I believe it's the lack of CMAKE_BUILD_TYPE that's eliminating this?
  # In any case we make sure to add it (for release and debug, to get pdb):
  append_property_string(TARGET drx LINK_FLAGS "/debug")
endif (WIN32 AND GENERATE_PDBS)

# documentation is put into main DR docs/ dir

DR_export_target(drx)
install_exported_target(drx ${INSTALL_EXT_LIB})
DR_install(FILES
  drx.h
  # add more here
  DESTINATION ${INSTALL_EXT_INCLUDE})
