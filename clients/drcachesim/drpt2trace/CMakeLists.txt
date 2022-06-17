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

# Add libipt to build system. For kernel traces decoding,
# we need to activate SIDEBAND, PEVENT and FEATURE_ELF.
# XXX: libipt does not use the CMake export mechanism for modular separation.
# It puts us at risk of CMake name collisions. Since libipt's build system is
# not complicated, we can rewrite a new CMakeList.txt for it and make it modular
# using the cmake export mechanism.
set(SIDEBAND ON CACHE BOOL "Enable libipt-sb, a sideband correlation library." FORCE)
set(PEVENT ON CACHE BOOL "Enable perf_event sideband support." FORCE)
set(FEATURE_ELF ON CACHE BOOL "Support ELF files." FORCE)
add_subdirectory(${PROJECT_SOURCE_DIR}/third_party/libipt
  ${PROJECT_BINARY_DIR}/third_party/libipt)

# drpt2trace depends on libipt and libipt-sb. Also, the head files
# intel-pt.h and libipt-sb.h are generated into the project binary directory.
# So we need to add the subdirectories that contain them to the include path.
include_directories(SYSTEM
  ${PROJECT_BINARY_DIR}/third_party/libipt/libipt/include
  ${PROJECT_BINARY_DIR}/third_party/libipt/sideband/include
)

# libipt.so and libipt-sb.so will be built in the project binary directory.
# Add link directories for libipt-ext and drpt2trace to find them.
link_directories(
  ${PROJECT_BINARY_DIR}/lib
)

# The library libipt-ext and drpt2trace need to find headers from
# the following directories.
include_directories(
  ${PROJECT_SOURCE_DIR}/third_party/libipt/libipt/internal/include
  ${PROJECT_SOURCE_DIR}/third_party/libipt/ptxed/include
)

# XXX: The library lipipt contains some functions that are useful for
# drpt2trace. But they are not built in libipt.so or libipt-sb.so.
# So we build them as an external library of libipt and link the external
# library to drpt2trace. In the future we may need to implement
# these functions in our library.
set(libipt-ext_srcs
  ${PROJECT_SOURCE_DIR}/third_party/libipt/ptxed/src/load_elf.c
  ${PROJECT_SOURCE_DIR}/third_party/libipt/libipt/src/pt_cpu.c
)
add_library(libipt-ext STATIC ${libipt-ext_srcs})
add_dependencies(libipt-ext libipt)
target_link_libraries(libipt-ext ipt)

# TODO i#5505: Currently, drpt2trace is built as an exe.
# In the future, we need to build a pt decoder library first.
# Then we link it to drpt2trace and drraw2trace.
set(drpt2trace_srcs
  pt2trace.c
)
add_executable(drpt2trace ${drpt2trace_srcs})
add_dependencies(drpt2trace libipt libipt-sb libipt-ext)
target_link_libraries(drpt2trace ipt ipt-sb libipt-ext)

# TODO i#5505: Currently, drpt2trace only counts instructions.
# In the future, we will add support for converting pt trace to memref_t.
# So we may need to link drdecode to drpt2trace or pt decoder library.
