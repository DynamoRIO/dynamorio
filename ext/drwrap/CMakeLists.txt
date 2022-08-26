# **********************************************************
# Copyright (c) 2010-2020 Google, Inc.    All rights reserved.
# **********************************************************

# drwrap: DynamoRIO Function Wrapping and Replacing Extension
# Derived from Dr. Memory: the memory debugger
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public
# License as published by the Free Software Foundation;
# version 2.1 of the License, and no later version.
#
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
# Library General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with this library; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

cmake_minimum_required(VERSION 3.7)

include(../../make/policies.cmake NO_POLICY_SCOPE)

# DynamoRIO Function Wrapping and Replacing Extension

# We do not need libc, and we avoid a 10x size increase in both our
# dll and pdb (plus we avoid stressing private library isolation) by
# not using it (i#714).
set(DynamoRIO_USE_LIBC OFF)

##################################################
# we rely on DR proper's asm setup so we don't need to include cpp2asm_support.cmake)

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
  set(asm_file "drwrap_asm_arm.asm")
elseif (DR_HOST_AARCH64)
  set(asm_file "drwrap_asm_aarch64.asm")
  elseif (DR_HOST_RISCV64)
  set(asm_file "drwrap_asm_riscv64.asm")
else ()
  set(asm_file "drwrap_asm_x86.asm")
endif ()
add_asm_target(${asm_file} drwrap_asm_src drwrap_asm_tgt ""
  "${asm_flags}" "${asm_deps}")

#
##################################################

set(srcs
  drwrap.c
  ${drwrap_asm_src}
  # add more here
  )

# i#1491#2: VS generators fail if static lib has resources
set(srcs_static ${srcs})

if (WIN32)
  set(srcs ${srcs} ${PROJECT_SOURCE_DIR}/core/win32/resources.rc)
endif ()

add_library(drwrap SHARED ${srcs})
# while private loader means preferred base is not required, more efficient
# to avoid rebase so we avoid conflict w/ client and other exts
set(PREFERRED_BASE 0x74000000)
configure_extension(drwrap OFF)
use_DynamoRIO_extension(drwrap drmgr)
use_DynamoRIO_extension(drwrap drcontainers)

macro(configure_drwrap_target target)
  if (NOT "${CMAKE_GENERATOR}" MATCHES "Visual Studio")
    # we need to add asm_defs to target b/c add_asm_target() is not able to set flags
    # for non-VS generators
    string(REPLACE "-D" "" asm_defs_list "${asm_defs}")
    append_property_list(TARGET ${target} COMPILE_DEFINITIONS "${asm_defs_list}")
  endif ()

  if ("${CMAKE_GENERATOR}" MATCHES "Visual Studio")
    # ensure race-free parallel builds
    add_dependencies(${target} ${drwrap_asm_tgt})
  endif ("${CMAKE_GENERATOR}" MATCHES "Visual Studio")
endmacro()

configure_drwrap_target(drwrap)

# Since LGPL, most users will want this as a shared library.
# A shared library is also required if multiple separate components all want to
# use this same extension.
# We do provide the static library for those who are ok w/ the license and who
# are building an end tool.
add_library(drwrap_static STATIC ${srcs_static})
configure_extension(drwrap_static ON)
use_DynamoRIO_extension(drwrap_static drmgr_static)
use_DynamoRIO_extension(drwrap_static drcontainers)
configure_drwrap_target(drwrap_static)

install_ext_header(drwrap.h)
