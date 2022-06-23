# **********************************************************
# Copyright (c) 2022 Google, Inc.    All rights reserved.
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
# ARE DISCLAIMED. IN NO EVENT SHALL VMWARE, INC. OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
# SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
# CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
# OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
# DAMAGE.

cmake_minimum_required(VERSION 3.7)

include(../../../../make/policies.cmake NO_POLICY_SCOPE)

set(PT_VERSION_MAJOR 2)
set(PT_VERSION_MINOR 1)
set(PT_VERSION_PATCH 0)
set(PT_VERSION_BUILD "0" CACHE STRING "")
set(PT_VERSION_EXT "" CACHE STRING "")

set(PT_VERSION "${PT_VERSION_MAJOR}.${PT_VERSION_MINOR}.${PT_VERSION_PATCH}")

add_definitions(
  -DPT_VERSION_MAJOR=${PT_VERSION_MAJOR}
  -DPT_VERSION_MINOR=${PT_VERSION_MINOR}
  -DPT_VERSION_PATCH=${PT_VERSION_PATCH}
  -DPT_VERSION_BUILD=${PT_VERSION_BUILD}
  -DPT_VERSION_EXT=\"${PT_VERSION_EXT}\"
)

include(GNUInstallDirs)
include(FindUnixCommands)

set(LIBIPT_SOURCE_DIR "${PROJECT_SOURCE_DIR}/third_party/libipt")
set(LIBIPT_BINARY_DIR "${PROJECT_BINARY_DIR}/clients/drcachesim/drpt2trace/libipt")
set(BUILD_SHARED_LIBS ON)

add_definitions(
    -DFEATURE_ELF
    -DFEATURE_SIDEBAND
    -DFEATURE_PEVENT
    -D_POSIX_C_SOURCE=200809L
  )
include_directories(
  ${LIBIPT_SOURCE_DIR}/include
  ${LIBIPT_SOURCE_DIR}/include/posix
  ${LIBIPT_BINARY_DIR}/libipt/include
  ${LIBIPT_BINARY_DIR}/sideband/include
  ${LIBIPT_SOURCE_DIR}/pevent/include
)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=c99")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fvisibility=hidden")


function(add_ptunit_c_test name)
    # Do nothing.
endfunction(add_ptunit_c_test)

function(add_ptunit_cpp_test name)
    # Do nothing.
endfunction(add_ptunit_cpp_test)

function(add_ptunit_libraries name)
    # Do nothing.
endfunction(add_ptunit_libraries)

add_subdirectory(${LIBIPT_SOURCE_DIR}/libipt ${LIBIPT_BINARY_DIR}/libipt)
add_subdirectory(${LIBIPT_SOURCE_DIR}/sideband ${LIBIPT_BINARY_DIR}/sideband)
add_subdirectory(${LIBIPT_SOURCE_DIR}/pevent ${LIBIPT_BINARY_DIR}/pevent)