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

add_executable(run_in_bg run_in_bg.c)

if (UNIX)
  add_executable(runstats runstats.c)
  add_executable(nudgeunix nudgeunix.c)

  include_directories(
    ${PROJECT_SOURCE_DIR}/core/lib
    )

  # set the default arch
  # we duplicate tools/dynamorio in tools/drdeploy.in to keep
  # dynamorio runnable w/ no arch arg: alternative is to add_custom_target,
  # add_custom_command, and add CMake_drdeploy.cmake to modify ^arch
  if (X64)
    set(archdefault 64)
  else (X64)
    set(archdefault 32)
  endif (X64)
  configure_file(${CMAKE_CURRENT_SOURCE_DIR}/drdeploy.in
    ${CMAKE_CURRENT_BINARY_DIR}/drdeploy
    @ONLY)
  install(PROGRAMS ${CMAKE_CURRENT_BINARY_DIR}/drdeploy
    DESTINATION ${INSTALL_BIN})
else (UNIX)
  # FIXME i#98: eventually upgrade to W4 with pragma exceptions.
  # W2 is default (we're using W3).  We should also replace
  # all references to unsafe functions (e.g., fopen) and
  # remove /wd4996
  string(REGEX REPLACE "/W4" "/W3" CMAKE_C_FLAGS "${CMAKE_C_FLAGS}")
  set(tool_cflags "/wd4996")
  add_definitions(-DNOT_DYNAMORIO_CORE -DWIN32)

  set(RESOURCES ${PROJECT_SOURCE_DIR}/core/win32/resources.rc)

  set(tgts 
    DRview 
    DRcontrol 
    DRkill 
    closewnd
    winstats 
    svccntrl 
    DRload
    balloon 
    dummy 
    drdeploy
    )
  if (NOT X64)
    # FIXME i#118: ldmp not yet 64-bit
    set(tgts ${tgts} ldmp)
  endif (NOT X64)

  include_directories(
    ${PROJECT_SOURCE_DIR}/libutil
    ${PROJECT_SOURCE_DIR}/core
    ${PROJECT_SOURCE_DIR}/core/lib
    )

  target_link_libraries(run_in_bg ntdll)

  foreach (tgt ${tgts})
    add_executable(${tgt} ${tgt}.c ${RESOURCES})

    if ("${tgt}" STREQUAL "ldmp")
      set(extra_lib "ntdll")
    elseif ("${tgt}" STREQUAL "drdeploy")
      set(extra_lib "drconfig")
    endif ("${tgt}" STREQUAL "ldmp")
    target_link_libraries(${tgt}
      policy_static user32 advapi32 pdh imagehlp shell32 "${extra_lib}")

    string(REGEX MATCH "(closewnd|winstats)" no_unicode "${tgt}")
    if (no_unicode)
      set(unicode_cflags "")
    else (no_unicode)
      set(unicode_cflags "-DUNICODE -D_UNICODE")
    endif (no_unicode)

    set_target_properties(${tgt} PROPERTIES
      # Set define parameters for resources.rc
      COMPILE_FLAGS "${tool_cflags} ${unicode_cflags} -DRC_IS_${tgt}")
  endforeach (tgt)

  # drdeploy.exe links to libutil/drconfig.dll.  When we build drdeploy,
  # we'll copy the DLL to the tools directory so users won't need to
  # update their path.

  # We could add a manifest so that we can request elevation on Vista
  # (xref PR 233520).  Works fine for double clicking the app, but
  # running from a cmd shell just gives access denied if the cmd shell
  # isn't runas admin which isn't quite as nice.  Disable for now.
  # $(MT) -manifest drdeploy.exe.manifest -outputresource:drdeploy.exe

  install(TARGETS ${tgts} DESTINATION ${INSTALL_BIN})

endif (UNIX)
