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

include(../../../make/policies.cmake NO_POLICY_SCOPE)

if (NOT LINUX OR NOT X86 OR NOT X64)
  message(FATAL_ERROR "This is only for Linux x86_64.")
endif ()

# Add libipt to build system. For kernel traces decoding,
# we need to activate SIDEBAND, PEVENT and FEATURE_ELF.
# XXX: libipt does not use the CMake export mechanism for modular separation.
# It puts us at risk of CMake name collisions. Since libipt's build system is
# not complicated, we could rewrite a new CMakeList.txt for it and make it modular
# using the cmake export mechanism.
set(SIDEBAND ON CACHE BOOL "Enable libipt-sb, a sideband correlation library." FORCE)
set(PEVENT ON CACHE BOOL "Enable perf_event sideband support." FORCE)
set(FEATURE_ELF ON CACHE BOOL "Support ELF files." FORCE)
add_subdirectory(${PROJECT_SOURCE_DIR}/third_party/libipt
  ${PROJECT_BINARY_DIR}/third_party/libipt)

# Import libipt and libipt-sb as modules and set include paths and libraries.
add_library(ipt SHARED IMPORTED)
set_target_properties(ipt PROPERTIES
  IMPORTED_LOCATION "${PROJECT_BINARY_DIR}/lib/libipt.so"
  INTERFACE_INCLUDE_DIRECTORIES "${PROJECT_BINARY_DIR}/third_party/libipt/libipt/include"
)
add_library(ipt-sb SHARED IMPORTED)
set_target_properties(ipt-sb PROPERTIES
  IMPORTED_LOCATION "${PROJECT_BINARY_DIR}/lib/libipt-sb.so"
  INTERFACE_INCLUDE_DIRECTORIES "${PROJECT_BINARY_DIR}/third_party/libipt/sideband/include"
)

# The library libipt-ext and drpt2ir need to find headers from
# the following directories.
include_directories(
  ${PROJECT_SOURCE_DIR}/third_party/libipt/ptxed/include
)

# XXX: The repository of lipipt contains some functions that are useful
# for drpt2ir. But they are not built in libipt.so or libipt-sb.so.
# So we build them as an external library of libipt and link the external
# library to drpt2ir. In the future we may need to implement
# these functions in our library.
set(ipt-ext_srcs
  ${PROJECT_SOURCE_DIR}/third_party/libipt/ptxed/src/load_elf.c
)
add_library(ipt-ext STATIC ${ipt-ext_srcs})
add_dependencies(ipt-ext ipt)
target_link_libraries(ipt-ext ipt)

# We build drpt2ir to a static library. So we can link it in the independent
# clients drpt2trace and drraw2trace.
set(drpt2ir_srcs
  pt2ir.cpp
)
add_library(drpt2ir STATIC ${drpt2ir_srcs})
configure_DynamoRIO_decoder(drpt2ir)
add_dependencies(drpt2ir ipt ipt-sb ipt-ext api_headers)
target_link_libraries(drpt2ir ipt ipt-sb ipt-ext)
install_client_nonDR_header(drmemtrace pt2ir.h)

# TODO i#5505: Currently, drpt2trace only counts instructions.
# In the future, we will add support for converting pt trace to memref_t.
# So we may need to link drdecode to drpt2ir library.
set(drpt2trace_srcs
  drpt2trace.cpp
)
add_executable(drpt2trace ${drpt2trace_srcs})
configure_DynamoRIO_standalone(drpt2trace)
use_DynamoRIO_extension(drpt2trace droption)
add_dependencies(drpt2trace drpt2ir)
# Add the directory containing libipt.so and libipt-sb.so to the linker search path.
SET(CMAKE_EXE_LINKER_FLAGS  "${CMAKE_EXE_LINKER_FLAGS} -Wl,-rpath='${PROJECT_BINARY_DIR}/lib'")
target_link_libraries(drpt2trace drpt2ir)
