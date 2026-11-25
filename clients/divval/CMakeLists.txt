# **********************************************************
# Copyright (c) 2026 Google, Inc.    All rights reserved.
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
# ARE DISCLAIMED. IN NO EVENT SHALL GOOGLE, INC. OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
# SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
# CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
# OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
# DAMAGE.

# divval: records operand values of AArch64 UDIV/SDIV instructions.
#
# This directory builds either as part of the DynamoRIO tree (it is picked up
# automatically by clients/CMakeLists.txt) or standalone against a DynamoRIO
# build or release package:
#   cmake -DDynamoRIO_DIR=<DR>/cmake <path-to-this-dir>

cmake_minimum_required(VERSION 3.14)

if (NOT DynamoRIO_INTERNAL)
  project(divval C CXX)
  find_package(DynamoRIO REQUIRED)
endif ()

if (NOT AARCH64)
  if (DynamoRIO_INTERNAL)
    # AArch64-only client: nothing to build for other architectures.
    return()
  endif ()
  message(FATAL_ERROR "divval only supports AArch64")
endif ()

add_library(divval SHARED divval.cpp)
configure_DynamoRIO_client(divval)
use_DynamoRIO_extension(divval drmgr)
use_DynamoRIO_extension(divval drreg)
use_DynamoRIO_extension(divval drcontainers)
use_DynamoRIO_extension(divval droption)

if (DynamoRIO_INTERNAL)
  add_dependencies(divval api_headers)
  install_target(divval ${INSTALL_CLIENTS_LIB})
endif ()
