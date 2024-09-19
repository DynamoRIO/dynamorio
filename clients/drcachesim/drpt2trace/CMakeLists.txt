# **********************************************************
# Copyright (c) 2023 Google, Inc.    All rights reserved.
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

add_subdirectory(libipt)

include_directories(
  ${PROJECT_BINARY_DIR}/third_party/include/libipt
)

# drpt2ir don't include configure.h so they don't get DR defines.
add_dr_defines()

# We build drpt2ir and drir2trace as static libraries so we can link it in the independent
# clients drpt2trace and drraw2trace.
set(drpt2ir_srcs
  pt2ir.cpp
  elf_loader.cpp
)
add_library(drpt2ir STATIC ${drpt2ir_srcs})
configure_DynamoRIO_decoder(drpt2ir)
add_dependencies(drpt2ir ipt ipt-sb api_headers)
target_link_libraries(drpt2ir ipt ipt-sb)
install_client_nonDR_header(drmemtrace elf_loader.h)
install_client_nonDR_header(drmemtrace pt2ir.h)
DR_export_target(drpt2ir)
install_exported_target(drpt2ir ${INSTALL_CLIENTS_LIB})

set(drir2trace_srcs
  ir2trace.cpp
)
add_library(drir2trace STATIC ${drir2trace_srcs})
configure_DynamoRIO_decoder(drir2trace)
add_dependencies(drir2trace api_headers)
install_client_nonDR_header(drmemtrace ir2trace.h)
DR_export_target(drir2trace)
install_exported_target(drir2trace ${INSTALL_CLIENTS_LIB})

include_directories(${PROJECT_BINARY_DIR}/clients/include/drmemtrace)
# drpt2trace is a client that uses drpt2ir and drir2trace to convert PT data to trace
# entries.
set(drpt2trace_srcs
  drpt2trace.cpp
)
add_definitions(-DBUILD_PT_POST_PROCESSOR)
add_executable(drpt2trace ${drpt2trace_srcs})
configure_DynamoRIO_standalone(drpt2trace)
use_DynamoRIO_extension(drpt2trace droption)
add_dependencies(drpt2trace ipt drpt2ir drir2trace)
target_link_libraries(drpt2trace ipt drpt2ir drir2trace)
