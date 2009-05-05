# **********************************************************
# Copyright (c) 2009 VMware, Inc.    All rights reserved.
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

# Packages up a DynamoRIO release via ctest command mode.
# This is a cross-platform script that replaces package.sh and
# package.bat: though we assume can use Unix Makefiles on Windows,
# in order to build in parallel.

# Usage: invoke via "ctest -S package.cmake,<build#>"

cmake_minimum_required (VERSION 2.2)

if ("${CTEST_SCRIPT_ARG}" STREQUAL "")
  message(FATAL_ERROR "build number not set")
endif()

get_filename_component(BINARY_BASE "." ABSOLUTE)
set(SUITE_TYPE Experimental)
set(DO_UPDATE OFF)

set(CTEST_SOURCE_DIRECTORY "${CTEST_SCRIPT_DIRECTORY}/..")
# CTest goes and does our builds and then wants to configure
# and build again and complains there's no top-level setting of
# CTEST_BINARY_DIRECTORY: 
#   "CMake Error: Some required settings in the configuration file were missing:"
# but we don't want to do another build so we just ignore the error.
set(CTEST_CMAKE_COMMAND "${CMAKE_EXECUTABLE_NAME}")
# FIXME: add support for NMake on Windows if devs want to use that
# Doesn't support -j though
set(CTEST_CMAKE_GENERATOR "Unix Makefiles")
set(CTEST_PROJECT_NAME "DynamoRIO")
find_program(MAKE_COMMAND make DOC "make command")
set(CTEST_BUILD_COMMAND "${MAKE_COMMAND} -j5")
set(CTEST_COMMAND "${CTEST_EXECUTABLE_NAME}")

function(dobuild name is64 initial_cache)
  set(CTEST_BUILD_NAME "${name}")
  set(CTEST_BINARY_DIRECTORY "${BINARY_BASE}/build_${CTEST_BUILD_NAME}")
  set(CTEST_INITIAL_CACHE "${initial_cache}
    BUILD_NUMBER:STRING=${CTEST_SCRIPT_ARG}
    UNIQUE_BUILD_NUMBER:STRING=${CTEST_SCRIPT_ARG}
    ")
  ctest_empty_binary_directory(${CTEST_BINARY_DIRECTORY})
  file(WRITE "${CTEST_BINARY_DIRECTORY}/CMakeCache.txt" "${CTEST_INITIAL_CACHE}")

  if (WIN32)
    # Convert env vars to run proper compiler.
    # Note that this is fragile and won't work with non-standard
    # directory layouts: we assume standard VS2005 or SDK.
    # FIXME: would be nice to have case-insensitive regex flag!
    # For now hardcoding VC, Bin, amd64
    if (is64)
      if (NOT "$ENV{LIB}" MATCHES "[Aa][Mm][Dd]64")
        # Note that we can't set ENV{PATH} as the output var of the replace:
        # it has to be its own set().
        string(REGEX REPLACE "VC([/\\\\])Bin" "VC\\1Bin\\1amd64"
          newpath "$ENV{PATH}")
        set(ENV{PATH} "${newpath}")
        string(REGEX REPLACE "([/\\\\])([Ll][Ii][Bb])" "\\1\\2\\1amd64"
          newlib "$ENV{LIB}")
        set(ENV{LIB} "${newlib}")
        string(REGEX REPLACE "([/\\\\])([Ll][Ii][Bb])" "\\1\\2\\1amd64"
          newlibpath "$ENV{LIBPATH}")
        set(ENV{LIBPATH} "${newlibpath}")
      endif (NOT "$ENV{LIB}" MATCHES "[Aa][Mm][Dd]64")
    else (is64)
      if ("$ENV{LIB}" MATCHES "[Aa][Mm][Dd]64")
        string(REGEX REPLACE "(VC[/\\\\]Bin[/\\\\])amd64" "\\1"
          newpath "$ENV{PATH}")
        set(ENV{PATH} "${newpath}")
        string(REGEX REPLACE "([Ll][Ii][Bb])[/\\\\]amd64" "\\1"
          newlib "$ENV{LIB}")
        set(ENV{LIB} "${newlib}")
        string(REGEX REPLACE "([Ll][Ii][Bb])[/\\\\]amd64" "\\1"
          newlibpath "$ENV{LIBPATH}")
        set(ENV{LIBPATH} "${newlibpath}")
      endif ("$ENV{LIB}" MATCHES "[Aa][Mm][Dd]64")
    endif (is64)
  else (WIN32)
    if (is64)
      set(ENV{CFLAGS} "-m64")
      set(ENV{CXXFLAGS} "-m64")
    else (is64)
      set(ENV{CFLAGS} "-m32")
      set(ENV{CXXFLAGS} "-m32")
    endif (is64)
  endif (WIN32)

  ctest_start(${SUITE_TYPE})
  if (DO_UPDATE)
    ctest_update(SOURCE "${CTEST_SOURCE_DIRECTORY}")
  endif (DO_UPDATE)
  ctest_configure(BUILD "${CTEST_BINARY_DIRECTORY}")
  ctest_build(BUILD "${CTEST_BINARY_DIRECTORY}")

  # communicate w/ caller
  set(last_build "${CTEST_BINARY_DIRECTORY}" PARENT_SCOPE)
  # prepend rather than append to get debug first, so we take release
  # files preferentially in case of overlap
  set(cpack_projects 
    "\"${CTEST_BINARY_DIRECTORY};DynamoRIO;ALL;/\"\n  ${cpack_projects}" PARENT_SCOPE)

endfunction(dobuild)


dobuild("release-32" OFF "")
dobuild("debug-32" OFF "
  DEBUG:BOOL=ON
  BUILD_TOOLS:BOOL=OFF
  BUILD_DOCS:BOOL=OFF
  BUILD_DRGUI:BOOL=OFF
  BUILD_SAMPLES:BOOL=OFF
  ")
dobuild("release-64" ON "")
dobuild("debug-64" ON "
  DEBUG:BOOL=ON
  BUILD_TOOLS:BOOL=OFF
  BUILD_DOCS:BOOL=OFF
  BUILD_DRGUI:BOOL=OFF
  BUILD_SAMPLES:BOOL=OFF
  ")


# now package up all the builds
file(APPEND "${last_build}/CPackConfig.cmake"
  "set(CPACK_INSTALL_CMAKE_PROJECTS\n  ${cpack_projects})")
set(CTEST_BUILD_COMMAND "${MAKE_COMMAND} package")
set(CTEST_BUILD_NAME "final package")
ctest_build(BUILD "${last_build}")


# copy the final archive into cur dir
# "cmake -E copy" only takes one file so use 'z' => .tar.gz or .zip
file(GLOB results ${last_build}/DynamoRIO-*z*)
execute_process(COMMAND ${CMAKE_COMMAND} -E copy ${results} .)
