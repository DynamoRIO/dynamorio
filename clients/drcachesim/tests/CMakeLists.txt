# **********************************************************
# Copyright (c) 2015-2017 Google, Inc.    All rights reserved.
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

# This is NOT a CMake file for building the various test suite executables in
# this tests/ directory: this is only for the --build-and-test test!

cmake_minimum_required(VERSION 3.7)

project(DynamoRIO_drmemtrace)

if (UNIX) # The shipped drmemtrace on Windows has no zlib support.
  find_package(ZLIB)
  if (NOT ZLIB_FOUND)
    # Not a fatal error to succeed on AArch64 pre-commit and other places where
    # zlib is not installed.
    # XXX i#2006: Can we automate the zlib link somehow?  Should we provide two versions
    # of our libs, one with and one without?  Can we include some version of zlib.a?
    message(WARNING "zlib not found: linking will fail if drmemtrace has compressed "
      "file support built-in")
  endif()
endif ()

find_package(DynamoRIO)
if (NOT DynamoRIO_FOUND)
  message(FATAL_ERROR "DynamoRIO package required to build")
endif()

# OS and ARCH defines needed for dr_api.h
if (WIN32)
  set(os_name "win")
  # Our non-client files assume this is set, yet don't include headers that set it.
  add_definitions(-DWINDOWS)
else ()
  set(os_name "unix")
  # Ditto.
  add_definitions(-DUNIX)
  if (APPLE)
    add_definitions(-DMACOS)
  else ()
    # 'if (LINUX)' added in 3.25 https://cmake.org/cmake/help/latest/variable/LINUX.html
    add_definitions(-DLINUX)
  endif ()
endif ()
# Ditto here, in particular for options.cpp.
if (X86)
  if (X64)
    add_definitions(-DX86_64)
  else ()
    add_definitions(-DX86_32)
  endif ()
elseif (AARCHXX)
  if (X64)
    add_definitions(-DARM_64)
  else ()
    add_definitions(-DARM_32)
  endif ()
elseif (RISCV64)
    add_definitions(-DRISCV_64)
endif ()



configure_DynamoRIO_main_headers() # for dr_frontend.h

add_executable(analyzer_separate analyzer_separate.cpp)
use_DynamoRIO_extension(analyzer_separate droption) # for droption.h
use_DynamoRIO_extension(analyzer_separate drdecode)
use_DynamoRIO_extension(analyzer_separate drutil)
use_DynamoRIO_drmemtrace(analyzer_separate) # for analysis framework headers
target_link_libraries(analyzer_separate drmemtrace_analyzer drmemtrace_histogram
  drfrontendlib)
if (ZLIB_FOUND)
  target_link_libraries(analyzer_separate ${ZLIB_LIBRARIES})
endif ()
