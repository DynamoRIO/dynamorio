# **********************************************************
# Copyright (c) 2009 VMware, Inc.    All rights reserved.
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

if (UNIX)
  message(FATAL_ERROR "libutil is Windows-only")
endif (UNIX)

# These custom commands are per-subdir so can't just place at top level
include(../make/CMake_events.cmake)

# FIXME i#60: add rules from old Makefile for building unit tests
# the tests had been broken for a while

set(CORE_SRCS
  ${PROJECT_SOURCE_DIR}/core/options.c
  ${PROJECT_SOURCE_DIR}/core/win32/inject_shared.c
  ${PROJECT_SOURCE_DIR}/core/win32/module_shared.c
  ${PROJECT_SOURCE_DIR}/core/win32/drmarker.c
  ${EVENTS_SRCS}
  ${SYSLOG_SRCS}
  )
set(RESOURCES ${PROJECT_SOURCE_DIR}/core/win32/resources.rc)
set(BASE_SRCS
  services.c
  config.c
  elm.c
  processes.c
  utils.c
  policy.c
  parser.c
  )
set(ROOT_SRCS
  detach.c
  mfapi.c
  )

include_directories(BEFORE 
  ${PROJECT_SOURCE_DIR}/core
  ${PROJECT_SOURCE_DIR}/core/lib
  ${PROJECT_SOURCE_DIR}/core/win32
  ${PROJECT_SOURCE_DIR}/core/x86
  ${PROJECT_BINARY_DIR} # for events.h
  )
add_definitions(-D_CRT_SECURE_NO_WARNINGS -DUNICODE -D_UNICODE)
add_definitions(-DNOT_DYNAMORIO_CORE)
if (NOT HOT_PATCHING_INTERFACE)
  # We define this to get processes.h included: PROBE_API controls whether we
  # expose it
  add_definitions(-DHOT_PATCHING_INTERFACE)
endif (NOT HOT_PATCHING_INTERFACE)

# FIXME i#98: fix the underlying cause of these warnings and remove these exceptions
set(nowarn "/wd4996 /wd4047 /wd4024 /wd4133 /wd4127 /wd4100 /wd4292 /wd4127 /wd4100")
set(libutil_cflags "/wd4996 /wd4047 /wd4024 /wd4133 /wd4127 /wd4100 /wd4292 /wd4127 /wd4100")
set(libutil_ldflags "/nodefaultlib:libc.lib")

add_library(policy_static ${BASE_SRCS} ${ROOT_SRCS} ${CORE_SRCS})
set_target_properties(policy_static PROPERTIES
  COMPILE_FLAGS "${libutil_cflags}")
set_target_properties(policy_static PROPERTIES
  LINK_FLAGS "${libutil_ldflags}")

add_library(drconfig SHARED ${BASE_SRCS} ${ROOT_SRCS} ${CORE_SRCS}
  ${RESOURCES} dr_config.c drconfig.def)
set_target_properties(drconfig PROPERTIES
  # Set define parameters for resources.rc
  # Since it's hard to pass spaces in defines (I can get a " through using
  # \\\\\\\" but no spaces) we put strings in resources.rc and select
  # using RC_ defines.
  COMPILE_FLAGS "${libutil_cflags} -DRC_IS_DRCONFIG")
set_target_properties(drconfig PROPERTIES
  LINK_FLAGS "${libutil_ldflags}")

add_executable(dumpevts dumpevts.c elm.c ${RESOURCES})
set_target_properties(dumpevts PROPERTIES
  # Set define parameters for resources.rc
  COMPILE_FLAGS "${libutil_cflags} -DRC_IS_DUMPEVTS")
set_target_properties(dumpevts PROPERTIES
  LINK_FLAGS "${libutil_ldflags}")

# used at Determina: not supported anymore
if (MFAPI_ENABLED)
  add_library(mfapi SHARED ${BASE_SRCS} ${ROOT_SRCS} ${CORE_SRCS}
    ${RESOURCES} mfapi.c mfapi.res)
  set_target_properties(mfapi PROPERTIES
    # Set define parameters for resources.rc
    COMPILE_FLAGS "${libutil_cflags} -DRC_IS_MFAPI")
  set_target_properties(mfapi PROPERTIES
    LINK_FLAGS "${libutil_ldflags}")
endif (MFAPI_ENABLED)

install(TARGETS drconfig
  RUNTIME DESTINATION ${INSTALL_LIB_BASE} # .dll
  LIBRARY DESTINATION ${INSTALL_LIB_BASE} # .so
  ARCHIVE DESTINATION ${INSTALL_LIB_BASE} # .lib
  )
# drdeploy.exe needs a copy in the bin dir
install(TARGETS drconfig DESTINATION ${INSTALL_BIN})

