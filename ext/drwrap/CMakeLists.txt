# **********************************************************
# Copyright (c) 2010-2011 Google, Inc.    All rights reserved.
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

# since LGPL, must be SHARED and not STATIC
add_library(drwrap SHARED
  drwrap.c
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
  get_target_property(cur_flags drwrap COMPILE_FLAGS)
  set_target_properties(drwrap PROPERTIES COMPILE_FLAGS "${cur_flags} -fPIC")
endif (UNIX)
# ensure we rebuild if includes change
add_dependencies(drwrap api_headers)

if (WIN32 AND GENERATE_PDBS)
  # I believe it's the lack of CMAKE_BUILD_TYPE that's eliminating this?
  # In any case we make sure to add it (for release and debug, to get pdb):
  get_target_property(cur_flags drwrap LINK_FLAGS)
  set_target_properties(drwrap PROPERTIES LINK_FLAGS "${cur_flags} /debug")
endif (WIN32 AND GENERATE_PDBS)

# documentation is put into main DR docs/ dir

export(TARGETS drwrap FILE ${PROJECT_BINARY_DIR}/cmake/${exported_targets_name}.cmake
  APPEND)
install(TARGETS drwrap EXPORT ${exported_targets_name} DESTINATION ${INSTALL_EXT_LIB})
install(FILES drwrap.h DESTINATION ${INSTALL_EXT_INCLUDE})
