# **********************************************************
# Copyright (c) 2010-2013 Google, Inc.    All rights reserved.
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

# DynamoRIO Multi-Instrumentation Manager: a mediator for combining
# and coordinating multiple instrumentation passes.

# We do not need libc, and we avoid a 10x size increase in both our
# dll and pdb (plus we avoid stressing private library isolation) by
# not using it (i#714).
set(DynamoRIO_USE_LIBC OFF)

# Must be shared to ensure only one copy, since can be used by
# the client and by multiple other libraries, some of which may
# be shared: the whole point is to have one master drmgr.
option(DR_EXT_DRMGR_STATIC "create drmgr as a static, not shared, library (N.B.: ensure no separately-linked components of your tool also use drmgr before enabling as a static library)" ${BUILD_EXT_STATIC})
if (DR_EXT_DRMGR_STATIC OR STATIC_LIBRARY)
  set(libtype STATIC)
else()
  set(libtype SHARED)
endif ()
add_library(drmgr ${libtype}
  drmgr.c
  # add more here
  )
# while private loader means preferred base is not required, more efficient
# to avoid rebase so we avoid conflict w/ client and other exts
set(PREFERRED_BASE 0x73000000)
configure_DynamoRIO_client(drmgr)
# ensure we rebuild if includes change
add_dependencies(drmgr api_headers)
if (UNIX)
  # static libs must be PIC to be linked into clients: else requires
  # relocations that run afoul of security policies, etc.
  append_property_string(TARGET drmgr COMPILE_FLAGS "-fPIC")
endif (UNIX)

if (WIN32 AND GENERATE_PDBS)
  # I believe it's the lack of CMAKE_BUILD_TYPE that's eliminating this?
  # In any case we make sure to add it (for release and debug, to get pdb):
  append_property_string(TARGET drmgr LINK_FLAGS "/debug")
endif (WIN32 AND GENERATE_PDBS)

# documentation is put into main DR docs/ dir

DR_export_target(drmgr)
install_exported_target(drmgr ${INSTALL_EXT_LIB})
DR_install(FILES
  drmgr.h
  # add more here
  DESTINATION ${INSTALL_EXT_INCLUDE})
