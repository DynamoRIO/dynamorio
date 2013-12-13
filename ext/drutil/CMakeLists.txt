# **********************************************************
# Copyright (c) 2010-2013 Google, Inc.    All rights reserved.
# **********************************************************

# drutil: DynamoRIO Function Wrapping and Replacing Extension
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

# We do not need libc, and we avoid a 10x size increase in both our
# dll and pdb (plus we avoid stressing private library isolation) by
# not using it (i#714).
set(DynamoRIO_USE_LIBC OFF)

# since LGPL, must be SHARED and not STATIC by default.
# SHARED is also required if multiple separate components all want to
# use this same extension.
option(DR_EXT_DRUTIL_STATIC "create drutil as a static, not shared, library (N.B.: ensure the LGPL license implications are acceptable for your tool, as well as ensuring no separately-linked components of your tool also use drutil, before enabling as a static library)" ${BUILD_EXT_STATIC})
if (DR_EXT_DRUTIL_STATIC OR STATIC_LIBRARY)
  set(libtype STATIC)
else()
  set(libtype SHARED)
endif ()
add_library(drutil ${libtype}
  drutil.c
  # add more here
  )
# while private loader means preferred base is not required, more efficient
# to avoid rebase so we avoid conflict w/ client and other exts
set(PREFERRED_BASE 0x75000000)
configure_DynamoRIO_client(drutil)
use_DynamoRIO_extension(drutil drmgr)
if (UNIX)
  # static containers must be PIC to be linked into clients: else requires
  # relocations that run afoul of security policies, etc.
  append_property_string(TARGET drutil COMPILE_FLAGS "-fPIC")
endif (UNIX)
# ensure we rebuild if includes change
add_dependencies(drutil api_headers)

if (WIN32 AND GENERATE_PDBS)
  # I believe it's the lack of CMAKE_BUILD_TYPE that's eliminating this?
  # In any case we make sure to add it (for release and debug, to get pdb):
  append_property_string(TARGET drutil LINK_FLAGS "/debug")
endif (WIN32 AND GENERATE_PDBS)

# documentation is put into main DR docs/ dir

DR_export_target(drutil)
install_exported_target(drutil ${INSTALL_EXT_LIB})
DR_install(FILES drutil.h DESTINATION ${INSTALL_EXT_INCLUDE})
