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

# From events.mc we create two different header files,
# shared among core and libutil.
# Custom commands and source file properties are per-subdir so we must
# include this in each instead of just placing in top-level CMakeLists.txt.

# FIXME i#68: move the work of gen_event_strings.pl into this script
set(SYSLOG_SRCS ${PROJECT_BINARY_DIR}/event_strings.h)
set_source_files_properties(${SYSLOG_SRCS} PROPERTIES GENERATED true)
add_custom_command(
  OUTPUT ${SYSLOG_SRCS}
  DEPENDS ${PROJECT_SOURCE_DIR}/core/win32/events.mc
          ${PROJECT_SOURCE_DIR}/core/gen_event_strings.pl
  COMMAND ${PERL_EXECUTABLE}
  ARGS ${PROJECT_SOURCE_DIR}/core/gen_event_strings.pl
       ${PROJECT_SOURCE_DIR}/core/win32/events.mc
       ${SYSLOG_SRCS}
  VERBATIM # recommended: p260
  )
if (WIN32 AND NOT ("${CMAKE_GENERATOR}" MATCHES "MSYS Makefiles"))
  # In MSYS2 we do not define CMAKE_MC_COMPILER
  # so we can't use Windows event log.
  set(EVENTS_SRCS ${PROJECT_BINARY_DIR}/events.h)
  set_source_files_properties(${EVENTS_SRCS} PROPERTIES GENERATED true)
  add_custom_command(
    OUTPUT ${EVENTS_SRCS}
    DEPENDS ${PROJECT_SOURCE_DIR}/core/win32/events.mc
    COMMAND ${CMAKE_MC_COMPILER}
    ARGS -h ${PROJECT_BINARY_DIR}
         -r ${PROJECT_BINARY_DIR}
         ${PROJECT_SOURCE_DIR}/core/win32/events.mc
    VERBATIM # recommended: p260
  )
else ()
  set(EVENTS_SRCS "")
endif ()
