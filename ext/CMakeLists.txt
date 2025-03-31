# **********************************************************
# Copyright (c) 2010-2025 Google, Inc.    All rights reserved.
# Copyright (c) 2010 VMware, Inc.    All rights reserved.
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

# i#277/PR 540817: DynamoRIO Extensions support

cmake_minimum_required(VERSION 3.14)

include(../make/policies.cmake NO_POLICY_SCOPE)

set(DynamoRIO_INTERNAL ON) # do not import dynamorio lib target
set(DynamoRIO_DIR ${PROJECT_BINARY_DIR}/cmake)
find_package(DynamoRIO ${VERSION_NUMBER_MAJOR}.${VERSION_NUMBER_MINOR})
if (NOT DynamoRIO_FOUND)
  message(FATAL_ERROR "DynamoRIO package required to build")
endif(NOT DynamoRIO_FOUND)

set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/ext/${INSTALL_LIB}")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}")
# we have no exe's yet, and we want our dll's in the lib dir
# (could use MODULE instead of SHARED if it would let us link)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/ext/${INSTALL_LIB}")
set_per_config_ouput_to_match_single_config()

set(INSTALL_EXT_BASE ext)
set(INSTALL_EXT_BIN ${INSTALL_EXT_BASE}/${INSTALL_BIN})
set(INSTALL_EXT_LIB ${INSTALL_EXT_BASE}/${INSTALL_LIB})
set(INSTALL_EXT_INCLUDE ${INSTALL_EXT_BASE}/include)
set(INSTALL_EXT_CMAKE ${INSTALL_EXT_BASE}/cmake)

disable_compiler_warnings()
# Extensions don't include configure.h so they don't get DR defines
add_dr_defines()

# This lets us share common code among all extensions.  We use a macro to
# avoid scope issues and avoid having to call configure_DynamoRIO_global().
macro(configure_extension target is_static static_DR)
  if (${static_DR})
    configure_DynamoRIO_static_client(${target})
  else ()
    configure_DynamoRIO_client(${target})
  endif ()

  # ensure we rebuild if includes change
  add_dependencies(${target} api_headers)

  # i#1713: extension headers may include a shared header from this directory
  include_directories(${INSTALL_EXT_LIB})

  if (${is_static})
    add_static_lib_debug_info(${target} "${INSTALL_EXT_LIB}")
  endif ()

  if (UNIX)
    # static libs must be PIC to be linked into clients: else requires
    # relocations that run afoul of security policies, etc.
    # Doesn't hurt to apply to shared libs as well, though CMake
    # should already be adding it.
    append_property_string(TARGET ${target} COMPILE_FLAGS "-fPIC")
  else ()
    # For version info we need global_shared.h included in resources.rc.
    # The extension must add core/win32/resources.rc as a source.
    include_directories(${PROJECT_SOURCE_DIR}/core/lib)
    append_property_list(TARGET ${target} COMPILE_DEFINITIONS "RC_IS_${target}")
  endif ()

  # documentation is put into main DR docs/ dir

  DR_export_target(${target})
  install_exported_target(${target} ${INSTALL_EXT_LIB})
  copy_target_to_device(${target} "${location_suffix}")
endmacro(configure_extension)

macro(install_ext_header header)
  DR_install(FILES ${header} DESTINATION ${INSTALL_EXT_INCLUDE})
  # We also need a copy in the build dir for our --build-and-test (i#1586)
  configure_file(${header} ${PROJECT_BINARY_DIR}/ext/include/${header} COPYONLY)
endmacro()

macro(add_ext_asm_target asm_file_arm asm_file_aarch64 asm_file_riscv64 asm_file_x86
  asm_src asm_tgt)
  # for cpp2asm_defines.h
  include_directories(${DynamoRIO_DIR})
  # we aren't using DR's configure.h so we have to set defines:
  if (UNIX)
    if (APPLE AND NOT AARCH64)
      set(asm_defs ${asm_defs} -DASSEMBLE_WITH_NASM)
    else (APPLE AND NOT AARCH64)
      set(asm_defs ${asm_defs} -DASSEMBLE_WITH_GAS)
    endif (APPLE AND NOT AARCH64)
  else (UNIX)
    set(asm_defs ${asm_defs} -DASSEMBLE_WITH_MASM)
  endif (UNIX)

  get_DynamoRIO_defines(DR_DEFINES OFF)
  # We need defines to be a list to pass as separate args to custom command.
  # We assume none have spaces inside them which seems reasonable.
  string(REPLACE " " ";" DR_DEFINES "${DR_DEFINES}")
  set(asm_flags ${asm_defs} ${DR_DEFINES} -I "${DynamoRIO_DIR}")
  set(asm_deps "${DynamoRIO_DIR}/cpp2asm_defines.h")
  if (DR_HOST_ARM)
    set(asm_file ${asm_file_arm})
  elseif (DR_HOST_AARCH64)
    set(asm_file ${asm_file_aarch64})
  elseif (DR_HOST_RISCV64)
    set(asm_file ${asm_file_riscv64})
  else ()
    set(asm_file ${asm_file_x86})
  endif ()
  add_asm_target(${asm_file} ${asm_src} ${asm_tgt} "" "${asm_flags}" "${asm_deps}")
endmacro()

add_subdirectory(drcontainers)
if (NOT ANDROID AND NOT ARM AND NOT AARCH64)
  # XXX i#1701: fails to build on Android.  May not be worth porting.
  # XXX: fails when cross-compiling for ARM or A64 on a machine with x86 Qt5 installing.
  # Working around that by just disabling for ARM and A64 completely.
  add_subdirectory(drgui)
endif ()
add_subdirectory(drmgr)
add_subdirectory(drx)
add_subdirectory(drwrap)
add_subdirectory(drreg)
add_subdirectory(drbbdup)
add_subdirectory(drsyms)
add_subdirectory(drutil)
add_subdirectory(droption)
add_subdirectory(drcovlib)
add_subdirectory(drstatecmp)
add_subdirectory(drsyscall)
if (BUILD_PT_TRACER)
  add_subdirectory(drpttracer)
endif (BUILD_PT_TRACER)
if (LINUX) # TODO i#2414: Port to Windows, Mac, and Android.
  if (HAVE_LIBUNWIND_H)
    add_subdirectory(drcallstack)
  else ()
    # Require libunwind.h for automated testing.
    # The TEST_SUITE var is set for packages too, to ensure we publish drcallstack.
    # We do not install for Android nor a64-on-x86 in the suite.
    if (TEST_SUITE AND NOT ANDROID AND NOT DR_HOST_NOT_TARGET)
      message(FATAL_ERROR "libunwind-dev is required to build drcallstack")
    else ()
      message(STATUS "libunwind-dev package not installed: not building drcallstack")
    endif ()
  endif ()
endif ()

# documentation is put into main DR docs/ dir

install_subdirs(${INSTALL_EXT_LIB} ${INSTALL_EXT_BIN})

# propagate to parent dir
set(exported_targets_append "${exported_targets_append}" PARENT_SCOPE)

install_ext_header("drext.h")
