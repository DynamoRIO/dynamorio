# **********************************************************
# Copyright (c) 2013-2016 Google, Inc.    All rights reserved.
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

cmake_minimum_required(VERSION 3.7)

include(../../make/policies.cmake NO_POLICY_SCOPE)

# DynamoRIO eXtension utilities

# We do not need libc, and we avoid a 10x size increase in both our
# dll and pdb (plus we avoid stressing private library isolation) by
# not using it (i#714).
set(DynamoRIO_USE_LIBC OFF)

set(srcs
  drx.c
  drx_buf.c
  scatter_gather_${ARCH_NAME}.c
  # add more here
  )

# i#1491#2: VS generators fail if static lib has resources
set(srcs_static ${srcs})

if (WIN32)
  set(srcs ${srcs} ${PROJECT_SOURCE_DIR}/core/win32/resources.rc)
endif ()

add_library(drx SHARED ${srcs})
# while private loader means preferred base is not required, more efficient
# to avoid rebase so we avoid conflict w/ client and other exts
set(PREFERRED_BASE 0x77000000)
configure_extension(drx OFF)
use_DynamoRIO_extension(drx drcontainers)
use_DynamoRIO_extension(drx drmgr)
use_DynamoRIO_extension(drx drreg)

# While we use drmgr in our implementation of drx, we only use pieces that
# do not rely on drmgr's 4 phases or event orderings, and we do not include
# drmgr.h in drx.h, all so that drx users need not use drmgr.
use_DynamoRIO_extension(drx drmgr)

macro(configure_drx_target target)
  if (WIN32)
    target_link_libraries(${target} ntdll_imports)
  endif ()
endmacro()

configure_drx_target(drx)

add_library(drx_static STATIC ${srcs_static})
configure_extension(drx_static ON)
use_DynamoRIO_extension(drx_static drcontainers)
use_DynamoRIO_extension(drx_static drmgr_static)
use_DynamoRIO_extension(drx_static drreg_static)
configure_drx_target(drx_static)

install_ext_header(drx.h)
