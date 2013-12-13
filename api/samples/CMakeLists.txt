# **********************************************************
# Copyright (c) 2010-2013 Google, Inc.    All rights reserved.
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

# To match Makefiles and have just one build type per configured build
# dir, we collapse VS generator configs to a single choice.
# This must be done prior to the project() command and the var
# must be set in the cache.
if ("${CMAKE_GENERATOR}" MATCHES "Visual Studio")
  if (DEBUG)
    set(CMAKE_CONFIGURATION_TYPES "Debug" CACHE STRING "" FORCE)
  else (DEBUG)
    set(CMAKE_CONFIGURATION_TYPES "Release" CACHE STRING "" FORCE)
  endif (DEBUG)
  # we want to use the _LOCATION_<config> property
  string(TOUPPER "${CMAKE_CONFIGURATION_TYPES}" upper)
  set(location_suffix "_${upper}")
else ("${CMAKE_GENERATOR}" MATCHES "Visual Studio")
  set(location_suffix "")
endif ("${CMAKE_GENERATOR}" MATCHES "Visual Studio")

project(DynamoRIO_samples)

set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/bin")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/bin")
if ("${CMAKE_GENERATOR}" MATCHES "Visual Studio")
  # we don't support the Debug and Release subdirs
  foreach (config ${CMAKE_CONFIGURATION_TYPES})
    string(TOUPPER "${config}" config_upper)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${config_upper}
      "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${config_upper}
      "${CMAKE_ARCHIVE_OUTPUT_DIRECTORY}")
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_${config_upper}
      "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}")
  endforeach ()
endif ()

if (BUILD_TESTS)
  # For automated sanity tests we can't have msgboxes or output
  set(SHOW_RESULTS_DEFAULT OFF)
else (BUILD_TESTS)
  set(SHOW_RESULTS_DEFAULT ON)
endif (BUILD_TESTS)

option(SHOW_RESULTS
  "Display client results in pop-up (Windows) or console message (Linux)"
  ${SHOW_RESULTS_DEFAULT})
if (SHOW_RESULTS)
  add_definitions(-DSHOW_RESULTS)
endif (SHOW_RESULTS)

option(SHOW_SYMBOLS "Use symbol lookup in clients that support it" ON)
if (SHOW_SYMBOLS)
  add_definitions(-DSHOW_SYMBOLS)
endif (SHOW_SYMBOLS)
if (NOT DEFINED GENERATE_PDBS)
  # support running tests over ssh where pdb building is problematic
  set(GENERATE_PDBS ON)
endif (NOT DEFINED GENERATE_PDBS)

if (STATIC_LIBRARY)
  set(libtype STATIC)
else()
  set(libtype SHARED)
endif ()

# i#379: We usually want to build the samples with optimizations to improve the
# chances of inlining, but it's nice to be able to turn that off easily.  A
# release build should already have optimizations, so this should only really
# affect debug builds.
option(OPTIMIZE_SAMPLES
  "Build samples with optimizations to increase the chances of clean call inlining (overrides debug flags)"
  ON)
if (WIN32)
  set(OPT_CFLAGS "/O2")
else (WIN32)
  set(OPT_CFLAGS "-O2")
endif (WIN32)

if (DEBUG)
  set(OPT_CFLAGS "-DDEBUG")
endif (DEBUG)

if (NOT DEFINED DynamoRIO_DIR)
  set(DynamoRIO_DIR "${PROJECT_SOURCE_DIR}/../cmake" CACHE PATH
    "DynamoRIO installation's cmake directory")
endif (NOT DEFINED DynamoRIO_DIR)

find_package(DynamoRIO ${VERSION_NUMBER_MAJOR}.${VERSION_NUMBER_MINOR})
if (NOT DynamoRIO_FOUND)
  message(FATAL_ERROR "DynamoRIO package required to build")
endif(NOT DynamoRIO_FOUND)

if (WIN32)
  # disable stack protection: "unresolved external symbol ___security_cookie"
  # disable the warning "unreferenced formal parameter" #4100
  # disable the warning "conditional expression is constant" #4127
  # disable the warning "cast from function pointer to data pointer" #4054
  set(CL_CFLAGS "/GS- /wd4100 /wd4127 /wd4054")
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${CL_CFLAGS}")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${CL_CFLAGS}")
  add_definitions(-D_CRT_SECURE_NO_WARNINGS)
endif (WIN32)

if (OPTIMIZE_SAMPLES)
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${OPT_CFLAGS}")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${OPT_CFLAGS}")
endif ()

function (add_sample_client name source_file_list extension_list)
  add_library(${name} ${libtype} ${source_file_list})
  configure_DynamoRIO_client(${name})
  foreach (ext ${extension_list})
    use_DynamoRIO_extension(${name} ${ext})
  endforeach (ext)

  # Provide a hint for how to use the client
  if (NOT DynamoRIO_INTERNAL OR NOT "${CMAKE_GENERATOR}" MATCHES "Ninja")
    get_target_property(path ${name} LOCATION${location_suffix})
    add_custom_command(TARGET ${name}
      POST_BUILD
      COMMAND ${CMAKE_COMMAND}
      ARGS -E echo "Usage: pass to drconfig or drrun: -c ${path}"
      VERBATIM)
  endif ()

  get_property(sample_list GLOBAL PROPERTY DynamoRIO_sample_list) # NON-PUBLIC
  set_property(GLOBAL PROPERTY DynamoRIO_sample_list              # NON-PUBLIC
    "${sample_list};${name}")                                     # NON-PUBLIC
  set(srcs ${srcs} ${source_file_list} PARENT_SCOPE)              # NON-PUBLIC
  set(tgts ${tgts} ${name} PARENT_SCOPE)                          # NON-PUBLIC
endfunction (add_sample_client)

function (add_sample_standalone name source_file_list)
  add_executable(${name} ${source_file_list})

  # We don't want an rpath as it makes it hard to switch
  # between debug and release at runtime (rpath is removed
  # on "make install"; here we avoid in build dir).
  # DynamoRIOConfig.cmake sets for standalone but not lib.
  set(DynamoRIO_RPATH OFF)
  configure_DynamoRIO_standalone(${name})

  # Provide a hint for running
  if (NOT DynamoRIO_INTERNAL OR NOT "${CMAKE_GENERATOR}" MATCHES "Ninja")
    if (UNIX)
      set(FIND_MSG "(set LD_LIBRARY_PATH)")
    else (UNIX)
      set(FIND_MSG "(set PATH or copy to same directory)")
    endif (UNIX)
    add_custom_command(TARGET ${name}
      POST_BUILD
      COMMAND ${CMAKE_COMMAND}
      ARGS -E echo "Make sure the loader finds the dynamorio library ${FIND_MSG}"
      VERBATIM)
  endif ()
  set(srcs ${srcs} ${source_file_list} PARENT_SCOPE)              # NON-PUBLIC
  set(tgts ${tgts} ${name} PARENT_SCOPE)                          # NON-PUBLIC
endfunction (add_sample_standalone)

###########################################################################

# As we'll be calling configure_DynamoRIO_{client,standalone} from within
# a function scope, we must set the global vars ahead of time:
configure_DynamoRIO_global(OFF ON)

# Use ;-separated lists for source files and extensions.

# For C clients that only rely on the DR API and not on any 3rd party
# library routines, we could shrink the size of the client binary
# by disabling libc via "set(DynamoRIO_USE_LIBC OFF)".

add_sample_client(bbbuf       "bbbuf.c"         "")
add_sample_client(bbcount     "bbcount.c"       "")
add_sample_client(bbsize      "bbsize.c"        "")
add_sample_client(cbr         "cbr.c"           "")
add_sample_client(countcalls  "countcalls.c"    "")
add_sample_client(div         "div.c"           "")
add_sample_client(empty       "empty.c"         "")
add_sample_client(inc2add     "inc2add.c"       "")
add_sample_client(inline      "inline.c"        "")
add_sample_client(inscount    "inscount.c"      "")
add_sample_client(memtrace    "memtrace.c"      "drmgr;drutil")
add_sample_client(prefetch    "prefetch.c"      "")
add_sample_client(signal      "signal.c"        "")
add_sample_client(strace      "strace.c"        "drmgr")
add_sample_client(wrap        "wrap.c"          "drwrap")
add_sample_client(modxfer     "modxfer.c;utils.c"       "drx")
# add utils.h for installation  # NON-PUBLIC
set(srcs ${srcs} "utils.h")     # NON-PUBLIC
add_sample_client(modxfer_app2lib "modxfer_app2lib.c"   "")
add_sample_client(stl_test    "stl_test.cpp"    "")
add_sample_client(instrcalls  "instrcalls.c"    "drsyms")

if (WIN32)
  # This client is Windows-only.
  add_sample_client(stats     "stats.c"         "")

  # XXX i#893: drwrap's asm isn't SEH compliant.
  append_property_string(TARGET wrap LINK_FLAGS "/safeseh:no")
endif ()

add_sample_standalone(tracedump   "tracedump.c")

# Strip out everything past this point for the user-exposed file.
# We also remove any lines above marked "NON_PUBLIC".
# Should we add some install targets for users?
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt" string)
string(REGEX REPLACE "[^\n]*NON-PUBLIC[^\n]*\n" "" string "${string}")
string(REGEX REPLACE "# Strip out everything.*$" "" string "${string}")
string(REGEX REPLACE "\\$\\{(VERSION_NUMBER[^\\}]*)\\}" "@\\1@" string "${string}")
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/CMakeLists-public.txt.in" "${string}")
configure_file("${CMAKE_CURRENT_BINARY_DIR}/CMakeLists-public.txt.in"
  "${CMAKE_CURRENT_BINARY_DIR}/CMakeLists-public.txt"
  @ONLY)

foreach (tgt ${tgts})
  # ensure we rebuild samples if includes change
  add_dependencies(${tgt} api_headers)
  if (WIN32 AND GENERATE_PDBS) # for release and debug
    # I believe it's the lack of CMAKE_BUILD_TYPE that's eliminating this?
    # In any case we make sure to add it:
    get_target_property(cur_flags ${tgt} LINK_FLAGS)
    if (NOT cur_flags)
      set(cur_flags "") # XXX: if we require cmake 2.8.6 we can simply use APPEND_STRING
    endif (NOT cur_flags)
    set_target_properties(${tgt} PROPERTIES LINK_FLAGS "${cur_flags} /debug")
  endif (WIN32 AND GENERATE_PDBS)
endforeach (tgt)

if (NOT DEFINED INSTALL_PREFIX)
  set(INSTALL_PREFIX "${PROJECT_SOURCE_DIR}/../../exports" CACHE PATH
    "where to install")
  set(CMAKE_INSTALL_PREFIX "${INSTALL_PREFIX}" CACHE INTERNAL
    "where to install" FORCE)
endif (NOT DEFINED INSTALL_PREFIX)
set(INSTALL_SAMPLES samples)
if (NOT DEFINED X64)
  message(FATAL_ERROR "we assume X64 is inherited from parent scope")
endif (NOT DEFINED X64)
if (X64)
  set(INSTALL_SAMPLES_BIN ${INSTALL_SAMPLES}/bin64)
else (X64)
  set(INSTALL_SAMPLES_BIN ${INSTALL_SAMPLES}/bin32)
endif (X64)

DR_install(TARGETS ${tgts} DESTINATION ${INSTALL_SAMPLES_BIN})
DR_install(FILES ${srcs} DESTINATION ${INSTALL_SAMPLES})
DR_install(FILES "${CMAKE_CURRENT_BINARY_DIR}/CMakeLists-public.txt"
  DESTINATION "${INSTALL_SAMPLES}" RENAME CMakeLists.txt)

DR_install(DIRECTORY ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/
  DESTINATION ${INSTALL_SAMPLES_BIN}
  FILE_PERMISSIONS OWNER_READ OWNER_EXECUTE GROUP_READ GROUP_EXECUTE
  WORLD_READ WORLD_EXECUTE
  FILES_MATCHING
  PATTERN "*.debug"
  PATTERN "*.pdb"
  )

# Include a copy of a recent (> 6.0) redistributable version of dbghelp, if
# we found one, so our drsyms-using samples work pre-Vista.
if (dbghelp_path)
  DR_install(FILES "${dbghelp_path}" DESTINATION ${INSTALL_SAMPLES_BIN})
endif ()
