# **********************************************************
# Copyright (c) 2010-2013 Google, Inc.    All rights reserved.
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

cmake_minimum_required(VERSION 2.6)

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

if (STATIC_LIBRARY)
  set(libtype STATIC)
else()
  set(libtype SHARED)
endif ()

# This lets us share common code among all extensions.  We use a macro to
# avoid scope issues and avoid having to call configure_DynamoRIO_global().
macro(configure_extension target)
  configure_DynamoRIO_client(${target})

  # ensure we rebuild if includes change
  add_dependencies(${target} api_headers)

  if (WIN32 AND GENERATE_PDBS)
    # I believe it's the lack of CMAKE_BUILD_TYPE that's eliminating this?
    # In any case we make sure to add it (for release and debug, to get pdb):
    append_property_string(TARGET ${target} LINK_FLAGS "/debug")
  endif (WIN32 AND GENERATE_PDBS)

  if (UNIX)
    # static libs must be PIC to be linked into clients: else requires
    # relocations that run afoul of security policies, etc.
    # Doesn't hurt to apply to shared libs as well, though CMake
    # should already be adding it.
    append_property_string(TARGET ${target} COMPILE_FLAGS "-fPIC")
  endif ()

  # documentation is put into main DR docs/ dir

  DR_export_target(${target})
  install_exported_target(${target} ${INSTALL_EXT_LIB})
endmacro(configure_extension)

file(GLOB dirs "*/CMakeLists.txt")
foreach (dir ${dirs})
  get_filename_component(dir ${dir} PATH)
  add_subdirectory(${dir})
endforeach (dir)

# documentation is put into main DR docs/ dir

install_subdirs(${INSTALL_EXT_LIB} ${INSTALL_EXT_BIN})

# propagate to parent dir
set(exported_targets_append "${exported_targets_append}" PARENT_SCOPE)
