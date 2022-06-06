# **********************************************************
# Copyright (c) 2015-2022 Google, Inc.    All rights reserved.
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

include(../../make/policies.cmake NO_POLICY_SCOPE)

set(BUILD_DRPTPOSTPROCESSOR_IN_DR OFF)
# currently drptpostprocessor only works on Linux x86_64
if (LINUX AND X64)
  # building drptpostprocessor in dr requires initializing the libipt submodule
  if (HAVE_LIBIPT)
    set(BUILD_DRPTPOSTPROCESSOR_IN_DR ON)
  endif (HAVE_LIBIPT)
endif (LINUX AND X64)


if (BUILD_DRPTPOSTPROCESSOR_IN_DR)
  # set include directories to find intel-pt.h and libipt-sb.h
  include_directories(SYSTEM
    ${PROJECT_BINARY_DIR}/third_party/libipt/libipt/include
    ${PROJECT_BINARY_DIR}/third_party/libipt/sideband/include
  )

  # set include directories to find load_elf.h
  include_directories(
    ${PROJECT_SOURCE_DIR}/third_party/libipt/libipt/internal/include
    ${PROJECT_SOURCE_DIR}/third_party/libipt/ptxed/include
  )

  # set library directories to find libipt.so and libipt-sb.so
  link_directories(
    ${CMAKE_BINARY_DIR}/lib
  )

  set (drptpostprocessor_srcs
    ${PROJECT_SOURCE_DIR}/third_party/libipt/ptxed/src/load_elf.c
    ${PROJECT_SOURCE_DIR}/third_party/libipt/libipt/src/pt_cpu.c
    drptpostprocessor.c
  )

  add_executable(drptpostprocessor
    ${drptpostprocessor_srcs}
  )
  add_dependencies(drptpostprocessor libipt)
  add_dependencies(drptpostprocessor libipt-sb)

  target_link_libraries(drptpostprocessor ipt)
  target_link_libraries(drptpostprocessor ipt-sb)

  ## Todo: Fix the unfind library error when add the following lines.
  # target_link_libraries(drptpostprocessor drdecode)
  # configure_DynamoRIO_standalone(drptpostprocessor)

  if (DEFINED DR_INSTALL_DEPLOY_BIN_DEST)
    set(dst ${DR_INSTALL_DEPLOY_BIN_DEST})
  else ()
    set(dst "${INSTALL_BIN}")
  endif ()
  DR_target_install(TARGETS drptpostprocessor DESTINATION ${dst})
endif()

# BUILD_DRPTPOSTPROCESSOR_LOCAL is used to build drptpostprocessor in local directory
set(BUILD_DRPTPOSTPROCESSOR_LOCAL ON)
if (BUILD_DRPTPOSTPROCESSOR_IN_DR)
  set(BUILD_DRPTPOSTPROCESSOR_LOCAL OFF)
endif()
if (BUILD_DRPTPOSTPROCESSOR_LOCAL)
  project(drptpostprocessor)

  set(LIBIPT_INCLUDE "" CACHE PATH "")
  set(LIBIPT_SIDEBAND_INCLUDE "" CACHE PATH "")
  set(LIBIPT_LIBDIR "" CACHE PATH "")

  include_directories(SYSTEM
    ${LIBIPT_INCLUDE}
    ${LIBIPT_SIDEBAND_INCLUDE}
  )

  # set include directories to find load_elf.h
  include_directories(
    ${PROJECT_SOURCE_DIR}/../../third_party/libipt/libipt/internal/include
    ${PROJECT_SOURCE_DIR}/../../third_party/libipt/ptxed/include
  )

  # set library directories to find libipt.so and libipt-sb.so
  link_directories(
    ${LIBIPT_LIBDIR}
  )

  set (drptpostprocessor_srcs
    ${PROJECT_SOURCE_DIR}/../../third_party/libipt/ptxed/src/load_elf.c
    ${PROJECT_SOURCE_DIR}/../../third_party/libipt/libipt/src/pt_cpu.c
    drptpostprocessor.c
  )

  add_executable(drptpostprocessor
    ${drptpostprocessor_srcs}
  )
  target_link_libraries(drptpostprocessor ipt)
  target_link_libraries(drptpostprocessor ipt-sb)
endif(BUILD_DRPTPOSTPROCESSOR_LOCAL)