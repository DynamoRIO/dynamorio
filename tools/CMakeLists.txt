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

include(../make/policies.cmake NO_POLICY_SCOPE)

add_executable(run_in_bg run_in_bg.c)

if (UNIX)
  add_executable(runstats runstats.c)
  copy_target_to_device(runstats "${location_suffix}")
  if (APPLE)
    # XXX i#1286: implement nudge for MacOS
  else (APPLE)
    add_executable(drnudgeunix nudgeunix.c ${PROJECT_SOURCE_DIR}/core/unix/nudgesig.c)
  endif ()
  add_executable(drloader drloader.c)

  include_directories(
   ${PROJECT_SOURCE_DIR}/libutil
   ${PROJECT_SOURCE_DIR}/core
   ${PROJECT_SOURCE_DIR}/core/lib
  )

  add_definitions(-DNOT_DYNAMORIO_CORE -DDYNAMORIO_INTERNAL)

  # i#1092: remove any stale symlink that might be there.
  file(REMOVE "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/drdeploy")
  # we leave drdeploy for backward compat w/ old script
  if (NOT ANDROID) # symlinks not supported on Android
    execute_process(COMMAND ${CMAKE_COMMAND} -E create_symlink
      "drrun" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/drdeploy"
      RESULT_VARIABLE symlink_result
      ERROR_VARIABLE symlink_error
      OUTPUT_QUIET)
    if (symlink_result OR symlink_error)
      message(FATAL_ERROR
        "*** drdeploy symlink failed (${symlink_result}): ***\n${symlink_error}")
    endif (symlink_result OR symlink_error)
  endif ()
  # simpler to install all and exclude
  DR_install(DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/"
    DESTINATION "${INSTALL_BIN}"
    FILE_PERMISSIONS ${owner_access} OWNER_EXECUTE
    GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
    PATTERN "run_in_bg*" EXCLUDE
    PATTERN "runstats*" EXCLUDE
    )

  # Set up our debugging support for gdb in the build directory.
  # We need to compute the final path of libdynamorio.so, with version number
  # applied for gdb's script autoloading to work.
  # i#2127: we eliminated soversioning so we do not need to rename or
  # make a symlink anymore.
  DR_install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/libdynamorio.so-gdb.py"
    DESTINATION "${INSTALL_LIB}"
    PERMISSIONS ${owner_access} OWNER_EXECUTE
    GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)

  set(tool_cflags "")
  set(RESOURCES "")

  if (NOT APPLE) # FIXME i#1286: add MacOS nudge support
    DR_install(TARGETS drnudgeunix DESTINATION ${INSTALL_BIN})
    # for backward compatibility we add a symlink to the old name
    add_custom_command(TARGET drnudgeunix POST_BUILD
      WORKING_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}"
      COMMAND ${CMAKE_COMMAND} -E create_symlink drnudgeunix nudgeunix)
  endif ()
else (UNIX)
  # FIXME i#98: eventually upgrade to W4 with pragma exceptions.
  # W2 is default (we're using W3).  We should also replace
  # all references to unsafe functions (e.g., fopen) and
  # remove /wd4996
  string(REGEX REPLACE "/W4" "/W3" CMAKE_C_FLAGS "${CMAKE_C_FLAGS}")
  set(tool_cflags "/wd4996")
  add_definitions(-DNOT_DYNAMORIO_CORE -DWIN32 -DDYNAMORIO_INTERNAL)

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
    create_process
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

  target_link_libraries(run_in_bg ntdll_imports)

  foreach (tgt ${tgts})
    add_executable(${tgt} ${tgt}.c ${RESOURCES})

    if ("${tgt}" STREQUAL "ldmp")
      _DR_get_static_libc_list(static_libc)
      set(extra_lib ${static_libc} ntdll_imports)
    endif ("${tgt}" STREQUAL "ldmp")
    target_link_libraries(${tgt}
      policy_static user32 advapi32 pdh imagehlp shell32 ${extra_lib})

    string(REGEX MATCH "(closewnd|winstats|create_process)" no_unicode "${tgt}")
    if (no_unicode)
      set(unicode_cflags "")
    else (no_unicode)
      set(unicode_cflags "-DUNICODE -D_UNICODE")
    endif (no_unicode)

    set_target_properties(${tgt} PROPERTIES
      # Set define parameters for resources.rc
      COMPILE_FLAGS "${tool_cflags} ${unicode_cflags}"
      COMPILE_DEFINITIONS "RC_IS_${tgt}")
  endforeach (tgt)

  # drdeploy.exe links to libutil/drconfiglib.dll.  When we build drdeploy,
  # we'll copy the DLL to the tools directory so users won't need to
  # update their path.  (i#1737 would eliminate this.)

  # We could add a manifest so that we can request elevation on Vista
  # (xref PR 233520).  Works fine for double clicking the app, but
  # running from a cmd shell just gives access denied if the cmd shell
  # isn't runas admin which isn't quite as nice.  Disable for now.
  # $(MT) -manifest drdeploy.exe.manifest -outputresource:drdeploy.exe

endif (UNIX)

# We generate 3 different tools from drdeploy.c.
if (CMAKE_COMPILER_IS_GNUCC AND NOT CMAKE_COMPILER_IS_CLANG)
  # Work around a false positive snprintf warning from gcc: i#4363.
  # XXX i#4363: A better solution would be nice to avoid missing
  # warnings on real snprintf issues.
  set_source_files_properties(drdeploy.c PROPERTIES COMPILE_FLAGS "-Wno-restrict")
endif ()
add_executable(drconfig drdeploy.c ${RESOURCES})
set_target_properties(drconfig PROPERTIES
  COMPILE_FLAGS "${tool_cflags}" COMPILE_DEFINITIONS "RC_IS_drconfig;DRCONFIG")
add_executable(drrun drdeploy.c ${RESOURCES})
set_target_properties(drrun PROPERTIES
  COMPILE_FLAGS "${tool_cflags}" COMPILE_DEFINITIONS "RC_IS_drrun;DRRUN")
add_executable(drinject drdeploy.c ${RESOURCES})
set_target_properties(drinject PROPERTIES
  COMPILE_FLAGS "${tool_cflags}" COMPILE_DEFINITIONS "RC_IS_drinject;DRINJECT")
set(tgts ${tgts} drconfig drrun drinject)

foreach (deploytgt drconfig drrun drinject)
  copy_target_to_device(${deploytgt} "${location_suffix}")
  if (WINDOWS)
    target_link_libraries(${deploytgt}
      policy_static user32 advapi32 pdh imagehlp shell32)
  endif ()
  target_link_libraries(${deploytgt} drconfiglib drfrontendlib)
  if (NOT ${deploytgt} MATCHES "drconfig")
    target_link_libraries(${deploytgt} drinjectlib)
  endif ()
  if (LINUX) # No checks for MacOS
    set(locvar_name ${deploytgt}loc)
    file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/${locvar_name}.cmake" CONTENT
      "set(${locvar_name} \"$<TARGET_FILE:${deploytgt}>\")\n")
    add_custom_command(TARGET ${deploytgt} POST_BUILD
      COMMAND ${CMAKE_COMMAND}
      ARGS -D lib_fileloc=${CMAKE_CURRENT_BINARY_DIR}/${locvar_name}
           -D check_textrel=ON
           -D check_deps=OFF
           -D check_libc=${BUILD_PACKAGE}
           -D READELF_EXECUTABLE=${READELF_EXECUTABLE}
           -P ${PROJECT_SOURCE_DIR}/core/CMake_readelf.cmake
      VERBATIM # recommended: p260
      )
  endif ()
endforeach ()

if (DEFINED DR_INSTALL_DEPLOY_BIN_DEST)
  set(dst ${DR_INSTALL_DEPLOY_BIN_DEST})
else ()
  set(dst "${INSTALL_BIN}")
endif ()
DR_target_install(TARGETS ${tgts} DESTINATION ${dst})

if (X64)
  set(SYMSFX "64")
else (X64)
  set(SYMSFX "")
endif (X64)

DR_install(FILES
  "${CMAKE_CURRENT_SOURCE_DIR}/windbg-scripts/load_syms${SYMSFX}"
  DESTINATION "${INSTALL_BIN}"
  RENAME "load_syms${SYMSFX}.txt")
