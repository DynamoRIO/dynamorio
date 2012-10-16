# **********************************************************
# Copyright (c) 2010-2011 Google, Inc.    All rights reserved.
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

if (WIN32)
  # disable stack protection: "unresolved external symbol ___security_cookie"
  # disable the warning "unreferenced formal parameter" #4100
  # disable the warning "conditional expression is constant" #4127
  # disable the warning "cast from function pointer to data pointer" #4054
  set(CL_CFLAGS "/GS- /wd4100 /wd4127 /wd4054")
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${CL_CFLAGS}")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${CL_CFLAGS}")
  add_definitions(-D_CRT_SECURE_NO_WARNINGS)
endif (WIN32)

# Extensions don't include configure.h so they don't get DR defines
foreach (config "" ${CMAKE_BUILD_TYPE} ${CMAKE_CONFIGURATION_TYPES})
  if ("${config}" STREQUAL "")
    set(config_upper "")
  else ("${config}" STREQUAL "")
    string(TOUPPER "_${config}" config_upper)
  endif ("${config}" STREQUAL "")
  foreach (var CMAKE_C_FLAGS${config_upper};CMAKE_CXX_FLAGS${config_upper})
    if (DEBUG)
      set(${var} "${${var}} -DDEBUG")
    endif (DEBUG)
    # we're used to X64 instead of X86_64
    if (X64)
      set(${var} "${${var}} -DX64")
    endif (X64)
  endforeach (var)
endforeach (config)


file(GLOB dirs "*/CMakeLists.txt")
foreach (dir ${dirs})
  get_filename_component(dir ${dir} PATH)
  add_subdirectory(${dir})
endforeach (dir)

# documentation is put into main DR docs/ dir

# These cover all subdirs.
# Subdirs just need to install their targets.
DR_install(DIRECTORY ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/
  DESTINATION ${INSTALL_EXT_LIB}
  FILE_PERMISSIONS OWNER_READ OWNER_EXECUTE GROUP_READ GROUP_EXECUTE
  WORLD_READ WORLD_EXECUTE
  FILES_MATCHING
  PATTERN "*.debug"
  PATTERN "*.pdb"
  )
file(GLOB bin_files "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/*")
if (bin_files)
  DR_install(DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/
    DESTINATION ${INSTALL_EXT_BIN}
    FILE_PERMISSIONS OWNER_READ OWNER_EXECUTE GROUP_READ GROUP_EXECUTE
    WORLD_READ WORLD_EXECUTE
    FILES_MATCHING
    PATTERN "*.debug"
    PATTERN "*.pdb"
    )
endif (bin_files)

# propagate to parent dir
set(exported_targets_append "${exported_targets_append}" PARENT_SCOPE)
