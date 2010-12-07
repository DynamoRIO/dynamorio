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

cmake_minimum_required(VERSION 2.6)

project(DynamoRIO_samples)

set(LIBRARY_OUTPUT_PATH "${PROJECT_BINARY_DIR}/bin")
set(EXECUTABLE_OUTPUT_PATH "${PROJECT_BINARY_DIR}/bin")

option(SHOW_RESULTS
  "Display client results in pop-up (Windows) or console message (Linux)"
  ON)
if (SHOW_RESULTS)
  add_definitions(-DSHOW_RESULTS)
endif (SHOW_RESULTS)

if (WIN32)
  # DRSyms symbol access Extension Not yet implemented on Linux
  option(SHOW_SYMBOLS "Use symbol lookup in clients that support it" ON)
  if (SHOW_SYMBOLS)
    add_definitions(-DSHOW_SYMBOLS)
  endif (SHOW_SYMBOLS)
  if (NOT DEFINED GENERATE_PDBS)
    # support running tests over ssh where pdb building is problematic
    set(GENERATE_PDBS ON)
  endif (NOT DEFINED GENERATE_PDBS)
endif (WIN32)

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

file(GLOB srcs *.c *.cpp)
# Not all clients are supported on all plaforms
string(REGEX REPLACE "[^;]*MF_moduledb.c" "" srcs "${srcs}")
if (UNIX OR CMAKE_C_SIZEOF_DATA_PTR EQUAL 8)
  string(REGEX REPLACE "[^;]*stats.c" "" srcs "${srcs}")
endif (UNIX OR CMAKE_C_SIZEOF_DATA_PTR EQUAL 8)

foreach (src ${srcs})
  get_filename_component(tgt ${src} NAME_WE)
  
  if ("${tgt}" STREQUAL "tracedump")
    # standalone app
    add_executable(${tgt} ${src})
    # We don't want an rpath as it makes it hard to switch
    # between debug and release at runtime (rpath is removed
    # on "make install"; here we avoid in build dir).
    # DynamoRIOConfig.cmake sets for standalone but not lib.
    set(DynamoRIO_RPATH OFF)
    configure_DynamoRIO_standalone(${tgt})

    get_target_property(tgtnm ${tgt} LOCATION)
    if (UNIX)
      set(FIND_MSG "(set LD_LIBRARY_PATH)")
    else (UNIX)
      set(FIND_MSG "(set PATH or copy to same directory)")
    endif (UNIX)
    add_custom_command(TARGET ${tgt}
      POST_BUILD
      COMMAND ${CMAKE_COMMAND}
      ARGS -E echo "Make sure the loader finds the dynamorio library ${FIND_MSG}"
      VERBATIM)
  else ("${tgt}" STREQUAL "tracedump")
    add_library(${tgt} SHARED ${src})
    configure_DynamoRIO_client(${tgt})

    if (SHOW_SYMBOLS AND "${tgt}" STREQUAL "instrcalls")
      use_DynamoRIO_extension(${tgt} drsyms)
    endif ()

    get_target_property(tgtnm ${tgt} LOCATION)
    add_custom_command(TARGET ${tgt}
      POST_BUILD
      COMMAND ${CMAKE_COMMAND}
      ARGS -E echo "Usage: pass to drconfig or drrun: -client ${tgtnm} 0 \"\""
      VERBATIM)
  endif ("${tgt}" STREQUAL "tracedump")

  set(tgts ${tgts} ${tgt})
endforeach (src)

# Strip out everything past this point for the user-exposed file
# Should we add some install targets for users?
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt" string)
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

DR_install(DIRECTORY ${LIBRARY_OUTPUT_PATH}/
  DESTINATION ${INSTALL_SAMPLES_BIN}
  FILE_PERMISSIONS OWNER_READ OWNER_EXECUTE GROUP_READ GROUP_EXECUTE
  WORLD_READ WORLD_EXECUTE
  FILES_MATCHING
  PATTERN "*.debug"
  PATTERN "*.pdb"
  )
