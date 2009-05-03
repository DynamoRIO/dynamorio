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

# Whether 64-bit is expected to be selected by user setting up compiler
# prior to invoking CMake: it has to be that way for Windows, and for
# Linux the user should set both CFLAGS and CXXFLAGS to -m32 or -m64
# to override gcc's default.
if (CMAKE_SIZEOF_VOID_P EQUAL 8)
  set(X64 ON)
else(CMAKE_SIZEOF_VOID_P EQUAL 8)
  set(X64 OFF)
endif (CMAKE_SIZEOF_VOID_P EQUAL 8)

if (UNIX)
  if (X64)
    set(ARCH_CFLAGS "-m64")
  else (X64)
    set(ARCH_CFLAGS "-m32")
  endif (X64)
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${ARCH_CFLAGS}")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${ARCH_CFLAGS}")
  # we're not assuming gcc >= 3.4 in our header files
  string(REGEX REPLACE "-fvisibility=internal" "" CMAKE_C_FLAGS "${CMAKE_C_FLAGS}")
else (UNIX)
  # disable stack protection: "unresolved external symbol ___security_cookie"
  # disable the warning "unreferenced formal parameter" #4100
  # disable the warning "conditional expression is constant" #4127
  # disable the warning "cast from function pointer to data pointer" #4054
  set(CL_CFLAGS "/GS- /wd4100 /wd4127 /wd4054")
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${CL_CFLAGS}")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${CL_CFLAGS}")
  add_definitions(-D_CRT_SECURE_NO_WARNINGS)
endif (UNIX)
string(STRIP "${CMAKE_C_FLAGS}" CMAKE_C_FLAGS)
string(STRIP "${CMAKE_CXX_FLAGS}" CMAKE_CXX_FLAGS)

if (NOT DEFINED DynamoRIO_DIR)
  set(DynamoRIO_DIR "${PROJECT_SOURCE_DIR}/../cmake" CACHE PATH
    "DynamoRIO installation's cmake directory")
endif (NOT DEFINED DynamoRIO_DIR)

find_package(DynamoRIO)
if (DynamoRIO_FOUND)
  if ("${CMAKE_BUILD_TYPE}" STREQUAL "Debug")
    set(DynamoRIO_DEBUG ON)
  else ("${CMAKE_BUILD_TYPE}" STREQUAL "Debug")
    set(DynamoRIO_DEBUG OFF)
  endif ("${CMAKE_BUILD_TYPE}" STREQUAL "Debug")
  set(DynamoRIO_CXX OFF)
  set(DynamoRIO_VISATT OFF)
  include(${DynamoRIO_USE_FILE})
endif(DynamoRIO_FOUND)

file(GLOB srcs *.c *.cpp)
# Not all clients are supported on all plaforms
string(REGEX REPLACE "[^;]*MF_moduledb.c" "" srcs "${srcs}")
if (UNIX OR X64)
  string(REGEX REPLACE "[^;]*stats.c" "" srcs "${srcs}")
endif (UNIX OR X64)
if (WIN32)
  string(REGEX REPLACE "[^;]*stl_test.cpp" "" srcs "${srcs}")
endif (WIN32)

foreach (src ${srcs})
  get_filename_component(tgt ${src} NAME_WE)
  
  if ("${tgt}" STREQUAL "tracedump")
    # standalone app
    add_executable(${tgt} ${src})

    # used by developers to run from install path
    # in final package this won't work: have to set LD_LIBRARY_PATH
    set_target_properties(${tgt} PROPERTIES
      INSTALL_RPATH ${DynamoRIO_LIBDIR})

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

    get_target_property(tgtnm ${tgt} LOCATION)
    add_custom_command(TARGET ${tgt}
      POST_BUILD
      COMMAND ${CMAKE_COMMAND}
      ARGS -E echo "Usage: pass to drdeploy: -client ${tgtnm} 0 \"\""
      VERBATIM)
  endif ("${tgt}" STREQUAL "tracedump")

  if ("${src}" MATCHES "\\.cpp$")
    # Normally we'd just set DynamoRIO_CXX to ON but we have both C and C++
    # clients so we manually do the C++ work here:
    set_target_properties(${tgt} PROPERTIES
      LINK_FLAGS "${DynamoRIO_CXXFLAGS}")
  endif ("${src}" MATCHES "\\.cpp$")

  target_link_libraries(${tgt} dynamorio)
  # We don't want an rpath as it makes it hard to switch
  # between debug and release at runtime (rpath is removed
  # on "make install"; here we avoid in build dir).
  set_target_properties(${tgt} PROPERTIES SKIP_BUILD_RPATH ON)

  set(tgts ${tgts} ${tgt})
endforeach (src)


# Strip out everything past this point for the user-exposed file
# Should we add some install targets for users?
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt" string)
string(REGEX REPLACE "# Strip out everything.*$" "" string "${string}")
# Should we put in the work to support in-source build?
if (${CMAKE_CURRENT_SOURCE_DIR} STREQUAL ${CMAKE_CURRENT_BINARY_DIR})
  message(FATAL_ERROR "in-source build not supported for BUILD_SAMPLES: please create separate build directory, or disable BUILD_SAMPLES")
endif (${CMAKE_CURRENT_SOURCE_DIR} STREQUAL ${CMAKE_CURRENT_BINARY_DIR})
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/CMakeLists.txt" "${string}")

foreach (tgt ${tgts})
  # ensure we rebuild samples if library or includes change
  add_dependencies(${tgt} api_headers dynamorio)
endforeach (tgt)

if (NOT DEFINED INSTALL_PREFIX)
  set(INSTALL_PREFIX "${PROJECT_SOURCE_DIR}/../../exports" CACHE PATH
    "where to install")
  set(CMAKE_INSTALL_PREFIX "${INSTALL_PREFIX}" CACHE INTERNAL
    "where to install" FORCE)
endif (NOT DEFINED INSTALL_PREFIX)
set(INSTALL_SAMPLES samples)
if (X64)
  set(INSTALL_SAMPLES_BIN ${INSTALL_SAMPLES}/bin64)
else (X64)
  set(INSTALL_SAMPLES_BIN ${INSTALL_SAMPLES}/bin32)
endif (X64)

install(TARGETS ${tgts} DESTINATION ${INSTALL_SAMPLES_BIN})
install(FILES ${srcs} DESTINATION ${INSTALL_SAMPLES})
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/CMakeLists.txt"
  DESTINATION ${INSTALL_SAMPLES})

install(DIRECTORY ${LIBRARY_OUTPUT_PATH}/
  DESTINATION ${INSTALL_SAMPLES_BIN}
  FILE_PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE
  FILES_MATCHING
  PATTERN "*.debug"
  PATTERN "*.pdb"
  )
