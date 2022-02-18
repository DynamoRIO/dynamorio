# **********************************************************
# Copyright (c) 2010-2016 Google, Inc.    All rights reserved.
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

# DynamoRIO Multi-Instrumentation Manager: a mediator for combining
# and coordinating multiple instrumentation passes.

# We do not need libc, and we avoid a 10x size increase in both our
# dll and pdb (plus we avoid stressing private library isolation) by
# not using it (i#714).
set(DynamoRIO_USE_LIBC OFF)

set(srcs
  drmgr.c
  # add more here
  )

# i#1491#2: VS generators fail if static lib has resources
set(srcs_static ${srcs})

if (WIN32)
  set(srcs ${srcs} ${PROJECT_SOURCE_DIR}/core/win32/resources.rc)
endif ()

macro(configure_drmgr_target target)
  if (WIN32)
    # Avoid link errors about missing __chkstk.
    append_property_string(TARGET ${target} COMPILE_FLAGS "/Gs65536")
  endif ()
  use_DynamoRIO_extension(${target} drcontainers)
endmacro(configure_drmgr_target)


add_library(drmgr SHARED ${srcs})
# while private loader means preferred base is not required, more efficient
# to avoid rebase so we avoid conflict w/ client and other exts
set(PREFERRED_BASE 0x73000000)
configure_extension(drmgr OFF)
configure_drmgr_target(drmgr)

# Must be shared to ensure only one copy, since can be used by
# the client and by multiple other libraries, some of which may
# be shared: the whole point is to have one unique drmgr.
# But, we do want to support projects using drmgr statically for
# an end tool, for performance.
# To support simultaneously exporting components for use with other clients,
# we build drmgr both shared and static (i#1342).
add_library(drmgr_static STATIC ${srcs_static})
configure_extension(drmgr_static ON)
configure_drmgr_target(drmgr_static)

install_ext_header(drmgr.h)
