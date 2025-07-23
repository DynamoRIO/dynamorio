# **********************************************************
# Copyright (c) 2015-2025 Google, Inc.    All rights reserved.
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

cmake_minimum_required(VERSION 3.14)

project(DynamoRIO_drmemtrace)

if (UNIX) # The shipped drmemtrace on Windows has no compression support.
  # The CMake exported target rule for drmemtrace_analyzer includes linking in
  # external libraries like zlib and lz4, but those libraries may not be on primary
  # search paths, so we set up those paths if available.
  find_package(ZLIB)
  find_library(liblz4 lz4)
  find_library(libsnappy snappy)
endif ()

find_package(DynamoRIO)
if (NOT DynamoRIO_FOUND)
  message(FATAL_ERROR "DynamoRIO package required to build")
endif()

configure_DynamoRIO_main_headers() # for dr_frontend.h

add_executable(analyzer_separate analyzer_separate.cpp)
use_DynamoRIO_extension(analyzer_separate droption) # for droption.h
use_DynamoRIO_drmemtrace(analyzer_separate) # for analysis framework headers
target_link_libraries(analyzer_separate drmemtrace_analyzer drmemtrace_histogram
  drfrontendlib)

# Test drmemtrace_static.
if (UNIX) # XXX i#7560: Windows link errors remain but we don't have time to solve.
  add_executable(burst_separate burst_static.cpp)
  set_property(SOURCE burst_static.cpp APPEND PROPERTY COMPILE_DEFINITIONS "DEFINE_MAIN")
  configure_DynamoRIO_static(burst_separate)
  use_DynamoRIO_static_client(burst_separate drmemtrace_static)
endif ()
