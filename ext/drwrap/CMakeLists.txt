# **********************************************************
# Copyright (c) 2010-2012 Google, Inc.    All rights reserved.
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

cmake_minimum_required(VERSION 2.6)

# DynamoRIO Function Wrapping and Replacing Extension

# since LGPL, must be SHARED and not STATIC by default.
# SHARED is also required if multiple separate components all want to
# use this same extension.
option(DR_EXT_DRWRAP_STATIC "create drwrap as a static, not shared, library (N.B.: ensure the LGPL license implications are acceptable for your tool, as well as ensuring no separately-linked components of your tool also use drwrap, before enabling as a static library)" ${BUILD_EXT_STATIC})
if (DR_EXT_DRWRAP_STATIC OR STATIC_LIBRARY)
  set(libtype STATIC)
else()
  set(libtype SHARED)
endif ()

##################################################
# we rely on DR proper's asm setup so we don't need to include cpp2asm_support.cmake)

# for cpp2asm_defines.h
include_directories(${DynamoRIO_DIR})

# we aren't using DR's configure.h so we have to set defines:
if (UNIX)
  if (APPLE)
    set(asm_defs ${asm_defs} -DASSEMBLE_WITH_NASM)
  else (APPLE)
    set(asm_defs ${asm_defs} -DASSEMBLE_WITH_GAS)
  endif (APPLE)
else (UNIX)
  set(asm_defs ${asm_defs} -DASSEMBLE_WITH_MASM)
endif (UNIX)
get_DynamoRIO_defines(DR_DEFINES OFF)
# We need defines to be a list to pass as separate args to custom command.
# We assume none have spaces inside them which seems reasonable.
string(REPLACE " " ";" DR_DEFINES "${DR_DEFINES}")
set(asm_flags ${asm_defs} ${DR_DEFINES} -I "${DynamoRIO_DIR}")
set(asm_deps "${DynamoRIO_DIR}/cpp2asm_defines.h")
add_asm_target(drwrap_asm.asm drwrap_asm_src drwrap_asm_tgt ""
  "${asm_flags}" "${asm_deps}")
#
##################################################

add_library(drwrap ${libtype}
  drwrap.c
  ${drwrap_asm_src}
  # add more here
  )
# while private loader means preferred base is not required, more efficient
# to avoid rebase so we avoid conflict w/ client and other exts
set(PREFERRED_BASE 0x74000000)
configure_DynamoRIO_client(drwrap)
use_DynamoRIO_extension(drwrap drmgr)
use_DynamoRIO_extension(drwrap drcontainers)
if (UNIX)
  # static containers must be PIC to be linked into clients: else requires
  # relocations that run afoul of security policies, etc.
  append_property_string(TARGET drwrap COMPILE_FLAGS "-fPIC")
endif (UNIX)

if (NOT "${CMAKE_GENERATOR}" MATCHES "Visual Studio")
  # we need to add asm_defs to target b/c add_asm_target() is not able to set flags
  # for non-VS generators
  append_property_string(TARGET drwrap COMPILE_FLAGS "${asm_defs}")
endif ()

# ensure we rebuild if includes change
add_dependencies(drwrap api_headers)

if ("${CMAKE_GENERATOR}" MATCHES "Visual Studio")
  # ensure race-free parallel builds
  add_dependencies(drwrap ${drwrap_asm_tgt})
endif ("${CMAKE_GENERATOR}" MATCHES "Visual Studio")

if (WIN32 AND GENERATE_PDBS)
  # I believe it's the lack of CMAKE_BUILD_TYPE that's eliminating this?
  # In any case we make sure to add it (for release and debug, to get pdb):
  append_property_string(TARGET drwrap LINK_FLAGS "/debug")
endif (WIN32 AND GENERATE_PDBS)
if (WIN32)
  # XXX i#893: our asm isn't seh complient
  append_property_string(TARGET drwrap LINK_FLAGS "/safeseh:no")
endif (WIN32)

# documentation is put into main DR docs/ dir

DR_export_target(drwrap)
install_exported_target(drwrap ${INSTALL_EXT_LIB})
DR_install(FILES drwrap.h DESTINATION ${INSTALL_EXT_INCLUDE})
