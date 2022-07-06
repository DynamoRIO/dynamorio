# **********************************************************
# Copyright (c) 2010-2021 Google, Inc.    All rights reserved.
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

# i#801: We use custom target (from core/CMakeLists.txt) to ensure headers
# are generated before compiling .c files that use them.  We used to list the
# header as a source file dependence in the targets that use them, but that
# doesn't work well with non-Makefile generators.
function (add_gen_events_deps target)
  add_dependencies(${target} generate_syslog)
  if (WIN32)
    add_dependencies(${target} generate_events)
  endif ()
endfunction (add_gen_events_deps)

# FIXME i#60: add rules from old Makefile for building unit tests
# the tests had been broken for a while

# We avoid core/ to avoid conflicts between core/hashtable.h and drcontainers
# hashtable.h.  An alternative would be to rename core/hashtable.h.
include_directories(BEFORE
  ${PROJECT_SOURCE_DIR}/core/lib
  ${PROJECT_SOURCE_DIR}/core/win32
  ${PROJECT_SOURCE_DIR}/core/arch
  ${PROJECT_SOURCE_DIR}/core/arch/${ARCH_NAME}
  ${PROJECT_BINARY_DIR} # for events.h
  ${PROJECT_SOURCE_DIR}/core/drlibc # drlibc.h
  )

if (WIN32)
  include_directories(BEFORE ${PROJECT_SOURCE_DIR}/core/win32)
else (WIN32)
  include_directories(BEFORE
    ${PROJECT_SOURCE_DIR}/core/unix)
endif (WIN32)

set(CORE_SRCS
  ${PROJECT_SOURCE_DIR}/core/options.c
  ${PROJECT_SOURCE_DIR}/core/win32/inject_shared.c
  ${PROJECT_SOURCE_DIR}/core/win32/module_shared.c
  ${PROJECT_SOURCE_DIR}/core/win32/ntdll_shared.c
  ${PROJECT_SOURCE_DIR}/core/win32/drmarker.c
  ${PROJECT_SOURCE_DIR}/core/io.c
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

# drfrontendlib doesn't want NOT_DYNAMORIO_CORE, so we set that individually
# on all the other targets.  They all want this:
add_definitions(-DNOT_DYNAMORIO_CORE_PROPER -DDYNAMORIO_INTERNAL)

if (WIN32)
  add_definitions(-D_CRT_SECURE_NO_WARNINGS -DUNICODE -D_UNICODE)
  if (NOT HOT_PATCHING_INTERFACE)
    # We define this to get processes.h included: PROBE_API controls whether we
    # expose it
    add_definitions(-DHOT_PATCHING_INTERFACE)
  endif (NOT HOT_PATCHING_INTERFACE)

  # FIXME i#98: fix the underlying cause of these warnings and remove these exceptions
  set(nowarn "/wd4996 /wd4047 /wd4024 /wd4133 /wd4127 /wd4100 /wd4292 /wd4127 /wd4100")
  set(libutil_cflags "/wd4996 /wd4047 /wd4024 /wd4133 /wd4127 /wd4100 /wd4292 /wd4127 /wd4100")
  set(libutil_ldflags "/nodefaultlib:libc.lib")

  if (DEBUG)
    set(libutil_cflags "${libutil_cflags} /Zi")
    set(libutil_ldflags "${libutil_ldflags} /debug")
  endif (DEBUG)

  add_library(policy_static STATIC ${BASE_SRCS} ${ROOT_SRCS} ${CORE_SRCS})
  set_target_properties(policy_static PROPERTIES
    COMPILE_FLAGS "${libutil_cflags}" COMPILE_DEFINITIONS "NOT_DYNAMORIO_CORE")
  set_target_properties(policy_static PROPERTIES
    LINK_FLAGS "${libutil_ldflags}")
  add_gen_events_deps(policy_static)

  set(DRCONFIG_SRCS dr_config.c ${BASE_SRCS} ${ROOT_SRCS} ${CORE_SRCS}
    ${RESOURCES})
else () # UNIX
  # Most of it is unported.  We're just reusing some utils.c and the config
  # file writing code currently.
  set(DRCONFIG_SRCS dr_config.c utils.c ${PROJECT_SOURCE_DIR}/core/io.c)
  if (APPLE)
    # XXX i#1286: implement nudge for MacOS
  else (APPLE)
    set(DRCONFIG_SRCS ${DRCONFIG_SRCS} ${PROJECT_SOURCE_DIR}/core/unix/nudgesig.c)
  endif ()
  set(libutil_cflags "")
endif ()

# i#1737: we used to have drconfiglib as a shared library, but that led to a
# lot of complexity on UNIX (setting
# INSTALL_NAME_DIR to "@rpath" for Mac i#1375, calling add_rel_rpaths() for
# every executable using them, problems on Android where DT_RUNPATH isn't
# supported, etc.) so we switched to static.
# It would be nice to be static on Windows to avoid copying the lib into bin dirs,
# but duplicate symbol problems make that non-trivial: we need i#1409.
if (WIN32)
  set(config_lib_type SHARED)
else ()
  set(config_lib_type STATIC)
endif ()
add_library(drconfiglib ${config_lib_type} ${DRCONFIG_SRCS})
set_target_properties(drconfiglib PROPERTIES
  # Set define parameters for resources.rc
  # Since it's hard to pass spaces in defines (I can get a " through using
  # \\\\\\\" but no spaces) we put strings in resources.rc and select
  # using RC_ defines.
  COMPILE_FLAGS "${libutil_cflags}"
  COMPILE_DEFINITIONS "RC_IS_DRCONFIGLIB;NOT_DYNAMORIO_CORE"
  # put in lib dir
  RUNTIME_OUTPUT_DIRECTORY${location_suffix} "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}")
add_gen_events_deps(drconfiglib)

if (WIN32)
  set_target_properties(drconfiglib PROPERTIES
    LINK_FLAGS "${libutil_ldflags}")
  # drinject.exe needs a copy in the bin dir (i#1737 would eliminate this)
  add_custom_command(TARGET drconfiglib POST_BUILD COMMAND ${CMAKE_COMMAND}
    ARGS -E copy "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/drconfiglib.dll"
    "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/" VERBATIM)

  add_executable(dumpevts dumpevts.c elm.c ${RESOURCES})
  set_target_properties(dumpevts PROPERTIES
    # Set define parameters for resources.rc
    COMPILE_FLAGS "${libutil_cflags}"
    COMPILE_DEFINITIONS "RC_IS_DUMPEVTS;NOT_DYNAMORIO_CORE")
  set_target_properties(dumpevts PROPERTIES
    LINK_FLAGS "${libutil_ldflags}")
  add_gen_events_deps(dumpevts)

  # used at Determina: not supported anymore
  if (MFAPI_ENABLED)
    add_library(mfapi SHARED ${BASE_SRCS} ${ROOT_SRCS} ${CORE_SRCS}
      ${RESOURCES} mfapi.c mfapi.res)
    set_target_properties(mfapi PROPERTIES
      # Set define parameters for resources.rc
      COMPILE_FLAGS "${libutil_cflags}"
      COMPILE_DEFINITIONS "RC_IS_MFAPI;NOT_DYNAMORIO_CORE")
    set_target_properties(mfapi PROPERTIES
      LINK_FLAGS "${libutil_ldflags}")
    add_gen_events_deps(mfapi)
  endif (MFAPI_ENABLED)
endif ()

DR_export_header(${PROJECT_SOURCE_DIR}/core/lib/dr_config.h dr_config.h)

install_exported_target(drconfiglib ${INSTALL_LIB_BASE})
DR_export_target(drconfiglib)
# drinject.exe and company need a copy in the bin dir (i#1737 would eliminate this)
DR_install(TARGETS drconfiglib DESTINATION ${INSTALL_BIN})

# drconfiglib now uses drfront_access()
# drconfiglib needs drlibc for dr_fpu_exception_init()
target_link_libraries(drconfiglib drfrontendlib drlibc)
if (UNIX)
  # We need our own memcpy + memset to avoid glibc versioning (i#1504).
  target_link_libraries(drconfiglib drmemfuncs)
endif ()

if (WIN32)
  set(dr_frontend_srcs dr_frontend_win.c)
else ()
  set(dr_frontend_srcs dr_frontend_unix.c)
endif ()
set(dr_frontend_srcs ${dr_frontend_srcs} dr_frontend_common.c)
add_library(drfrontendlib STATIC ${dr_frontend_srcs})
if (UNIX)
  # We need drlibc for module-parsing routines.
  target_link_libraries(drfrontendlib drlibc)
endif ()
if (UNIX)
  # We need our own memcpy + memset to avoid glibc versioning (i#1504).
  target_link_libraries(drfrontendlib drmemfuncs)
endif ()
add_static_lib_debug_info(drfrontendlib "${INSTALL_BIN}")
DR_export_target(drfrontendlib)
install_exported_target(drfrontendlib ${INSTALL_BIN})
DR_export_header(${CMAKE_CURRENT_SOURCE_DIR}/dr_frontend.h dr_frontend.h)
