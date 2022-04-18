# **********************************************************
# Copyright (c) 2011-2015 Google, Inc.    All rights reserved.
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

cmake_minimum_required(VERSION 3.7)

include(../../make/policies.cmake NO_POLICY_SCOPE)

project(DynamoRIO_standalone)

set(LIBRARY_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/bin")
set(ARCHIVE_OUTPUT_DIRECTORY "${LIBRARY_OUTPUT_DIRECTORY}")
set(RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/bin")

if (WIN32)
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

if (WIN32)
  file(GLOB srcs winsysnums.c) # only supporting one for now
else ()
  set(srcs ) # empty list
endif ()
# Not all clients are supported on all plaforms
string(REGEX REPLACE "[^;]*MF_moduledb.c" "" srcs "${srcs}")
if (UNIX OR CMAKE_C_SIZEOF_DATA_PTR EQUAL 8)
  string(REGEX REPLACE "[^;]*stats.c" "" srcs "${srcs}")
endif (UNIX OR CMAKE_C_SIZEOF_DATA_PTR EQUAL 8)

set(DynamoRIO_REG_COMPATIBILITY ON)

foreach (src ${srcs})
  get_filename_component(tgt ${src} NAME_WE)

  # these are all standalone apps
  add_executable(${tgt} ${src})
  # We don't want an rpath as it makes it hard to switch
  # between debug and release at runtime (rpath is removed
  # on "make install"; here we avoid in build dir).
  # DynamoRIOConfig.cmake sets for standalone but not lib.
  set(DynamoRIO_RPATH OFF)
  configure_DynamoRIO_standalone(${tgt})

  if ("${tgt}" STREQUAL "winsysnums")
    use_DynamoRIO_extension(${tgt} drsyms)
  endif ("${tgt}" STREQUAL "winsysnums")

  target_link_libraries(${tgt} imagehlp.lib)

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

  set(tgts ${tgts} ${tgt})
endforeach (src)

