# **********************************************************
# Copyright (c) 2011-2022 Google, Inc.    All rights reserved.
# Copyright (c) 2009-2010 VMware, Inc.    All rights reserved.
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
# package.bat.

# Usage: invoke via "ctest -S package.cmake,build=<build#>"
# See other available args below

# To include Dr. Memory, use the invoke= argument to point at the
# Dr. Memory package script.
# For example:
# ctest -V -S /my/path/to/dynamorio/src/make/package.cmake,build=1\;version=5.0.0\;invoke=/my/path/to/drmemory/src/package.cmake\;drmem_only\;cacheappend=TOOL_VERSION_NUMBER:STRING=1.6.0

cmake_minimum_required(VERSION 3.7)
set(CTEST_SOURCE_DIRECTORY "${CTEST_SCRIPT_DIRECTORY}/..")

# arguments are a ;-separated list (must escape as \; from ctest_run_script())
# required args:
set(arg_build "")      # build #
# optional args:
set(arg_ubuild "")     # unique build #: if not specified, set to arg_build
set(arg_version "")    # version #
set(arg_outdir ".")    # directory in which to place deliverables
set(arg_cacheappend "")# string to append to every build's cache
set(arg_no64 OFF)      # skip the 64-bit builds?
set(arg_no32 OFF)      # skip the 32-bit builds?
set(arg_invoke "")     # sub-project package.cmake to invoke
# also takes args parsed by runsuite_common_pre.cmake, in particular:
set(arg_preload "")    # cmake file to include prior to each 32-bit build
set(arg_preload64 "")  # cmake file to include prior to each 64-bit build
set(arg_cpackappend "")# string to append to CPackConfig.cmake before packaging
set(arg_copy_docs OFF)  # copy html/ dir into top level
set(cross_aarchxx_linux_only OFF)
set(cross_android_only OFF)

foreach (arg ${CTEST_SCRIPT_ARG})
  if (${arg} MATCHES "^build=")
    string(REGEX REPLACE "^build=" "" arg_build "${arg}")
  endif ()
  if (${arg} MATCHES "^ubuild=")
    string(REGEX REPLACE "^ubuild=" "" arg_ubuild "${arg}")
  endif ()
  if (${arg} MATCHES "^version=")
    string(REGEX REPLACE "^version=" "" arg_version "${arg}")
  endif ()
  if (${arg} MATCHES "^outdir=")
    string(REGEX REPLACE "^outdir=" "" arg_outdir "${arg}")
  endif ()
  if (${arg} MATCHES "^cacheappend=")
    # support multiple, appending each time
    string(REGEX REPLACE "^cacheappend=" "" entry "${arg}")
    set(arg_cacheappend "${arg_cacheappend}\n${entry}")
  endif ()
  if (${arg} MATCHES "^no64" OR
      ${arg} MATCHES "^32_only")
    set(arg_no64 ON)
  endif ()
  if (${arg} MATCHES "^no32" OR
      ${arg} MATCHES "^64_only")
    set(arg_no32 ON)
  endif ()
  if (${arg} MATCHES "^invoke=")
    string(REGEX REPLACE "^invoke=" "" arg_invoke "${arg}")
  endif ()
  if (${arg} MATCHES "^cpackappend=")
    string(REGEX REPLACE "^cpackappend=" "" arg_cpackappend "${arg}")
  endif ()
  if (${arg} MATCHES "^copy_docs")
    set(arg_copy_docs ON)
  endif ()
endforeach (arg)

# These are set by env var instead of arg to match CI test jobs.
if ($ENV{DYNAMORIO_CROSS_AARCHXX_LINUX_ONLY} MATCHES "yes")
  set(cross_aarchxx_linux_only ON)
  set(ARCH_IS_X86 OFF)
  if (arg_no32)
    set(arg_cacheappend "${arg_cacheappend}
      PACKAGE_PLATFORM:STRING=AArch64-
      CMAKE_TOOLCHAIN_FILE:PATH=${CTEST_SOURCE_DIRECTORY}/make/toolchain-aarch64.cmake")
  elseif (arg_no64)
    set(arg_cacheappend "${arg_cacheappend}
      PACKAGE_PLATFORM:STRING=ARM-
      PACKAGE_SUBSYS:STRING=-EABIHF
      CMAKE_TOOLCHAIN_FILE:PATH=${CTEST_SOURCE_DIRECTORY}/make/toolchain-arm32.cmake")
  else ()
    message(FATAL_ERROR "package.cmake supports just one package at a time and 32-bit "
      "and 64-bit AArch cannot be combined")
  endif ()
endif()
if ($ENV{DYNAMORIO_CROSS_ANDROID_ONLY} MATCHES "yes")
  set(cross_android_only ON)
  set(ARCH_IS_X86 OFF)
  if (arg_no64)
    set(arg_cacheappend "${arg_cacheappend}
      PACKAGE_PLATFORM:STRING=ARM-
      PACKAGE_SUBSYS:STRING=-EABI
      CMAKE_TOOLCHAIN_FILE:PATH=${CTEST_SOURCE_DIRECTORY}/make/toolchain-android.cmake
      ANDROID_TOOLCHAIN:PATH=$ENV{DYNAMORIO_ANDROID_TOOLCHAIN}")
  else ()
    message(FATAL_ERROR "Android is only supported as 32_only")
  endif ()
endif()

if ("${arg_build}" STREQUAL "")
  message(FATAL_ERROR "build number not set: pass as build= arg")
endif()
if ("${arg_ubuild}" STREQUAL "")
  set(arg_ubuild "${arg_build}")
endif()

set(CTEST_PROJECT_NAME "DynamoRIO")
set(cpack_project_name "DynamoRIO")
set(run_tests OFF)
set(CTEST_SOURCE_DIRECTORY "${CTEST_SCRIPT_DIRECTORY}/..")
include("${CTEST_SCRIPT_DIRECTORY}/utils.cmake")
include("${CTEST_SCRIPT_DIRECTORY}/../suite/runsuite_common_pre.cmake")

if (APPLE)
  # DRi#58: core DR does not yet support 64-bit Mac
  set(arg_32_only ON)
endif ()

if (NOT APPLE AND NOT arg_no32)
  identify_clang(CMAKE_COMPILER_IS_CLANG)
  if (CMAKE_COMPILER_IS_CLANG)
    # i#2632: 32-bit recent clang release builds are inserting text relocs.
    message(FATAL_ERROR "clang is not supported for package builds until i#2632 is fixed")
  endif ()
endif ()

set(base_cache "
  ${base_cache}
  BUILD_NUMBER:STRING=${arg_build}
  UNIQUE_BUILD_NUMBER:STRING=${arg_ubuild}
  BUILD_PACKAGE:BOOL=ON
  AUTOMATED_TESTING:BOOL=ON
  ${arg_cacheappend}
  ")

# version is optional
if (arg_version)
  set(base_cache "${base_cache}
    VERSION_NUMBER:STRING=${arg_version}")
endif (arg_version)

# 64-bit first, to match DrMem, so that the last package dir has the right
# Windows VS env var setup.
if (NOT arg_no64)
  testbuild_ex("release-64" ON "" OFF ON "")
  testbuild_ex("debug-64" ON "
    DEBUG:BOOL=ON
    INTERNAL:BOOL=ON
    BUILD_DOCS:BOOL=OFF
    BUILD_DRSTATS:BOOL=OFF
    BUILD_SAMPLES:BOOL=OFF
    " OFF ON "")
endif (NOT arg_no64)
if (NOT arg_no32)
  # open-source now, no reason for debug to not be internal as well
  testbuild_ex("release-32" OFF "" OFF ON "")
  # note that we do build TOOLS so our DynamoRIOTarget{32,64}.cmake files match
  # and the DynamoRIOTarget{32,64}-debug.cmake file is complete (i#390)
  testbuild_ex("debug-32" OFF "
    DEBUG:BOOL=ON
    INTERNAL:BOOL=ON
    BUILD_DOCS:BOOL=OFF
    BUILD_DRSTATS:BOOL=OFF
    BUILD_SAMPLES:BOOL=OFF
    " OFF ON "")
endif (NOT arg_no32)

if (NOT arg_invoke STREQUAL "")
  message("\n***************\ninvoking sub-project ${arg_invoke}\n")
  # sub-package has its own version, assumed to be set by caller via cacheappend
  string(REGEX REPLACE ";version=[^;]*;" ";" CTEST_SCRIPT_ARG "${CTEST_SCRIPT_ARG}")
  set(save_last_dir ${last_package_build_dir})
  set(arg_sub_package ON)
  set(arg_sub_script ${arg_invoke})
  include("${arg_invoke}" RESULT_VARIABLE invoke_res)
  if (NOT invoke_res)
    message(FATAL_ERROR "Failed in invoke sub-project ${arg_invoke}")
  endif ()
  set(last_package_build_dir ${save_last_dir})
endif ()

if (arg_cpackappend)
  # Like DrMem, we use ${last_package_build_dir} from testbuild_ex to locate
  # CPackConfig.cmake.
  file(APPEND "${last_package_build_dir}/CPackConfig.cmake"
    "\n${arg_cpackappend}\n")
endif ()

set(build_package ON)
include("${CTEST_SCRIPT_DIRECTORY}/../suite/runsuite_common_post.cmake")

# copy the final archive into cur dir
# "cmake -E copy" only takes one file so use 'z' => .tar.gz or .zip
file(GLOB results ${last_package_build_dir}/DynamoRIO-*z*)
if (EXISTS "${results}")
  execute_process(COMMAND ${CMAKE_COMMAND} -E copy ${results} "${arg_outdir}")
else (EXISTS "${results}")
  message(FATAL_ERROR "failed to create package")
endif (EXISTS "${results}")

if (arg_copy_docs)
  # Prepare for copying the documentation to our Github Pages site by placing it
  # in a single fixed-name location with a .nojekyll file.
  message("Copying standalone documentation into ${arg_outdir}/html")
  message("Looking for ${last_package_build_dir}/_CPack_Packages/*/*/DynamoRIO-*/docs/html")
  file(GLOB allhtml "${last_package_build_dir}/_CPack_Packages/*/*/DynamoRIO-*/docs/html")
  # If there's a source package we'll have multiple.  Just take the first one.
  list(GET allhtml 0 html)
  if (EXISTS "${html}")
    execute_process(COMMAND ${CMAKE_COMMAND} -E make_directory "${arg_outdir}/html")
    execute_process(COMMAND
      ${CMAKE_COMMAND} -E copy_directory ${html} "${arg_outdir}/html")
    # Create a .nojekyll file so Github Pages will display this as raw html.
    execute_process(COMMAND ${CMAKE_COMMAND} -E touch "${arg_outdir}/html/.nojekyll")
    message("Successully copied docs")
  else ()
    message(FATAL_ERROR "failed to find html docs")
  endif ()

  message("Copying embedded documentation into ${arg_outdir}/html_embed")
  message("Looking for ${last_package_build_dir}/_CPack_Packages/*/*/DynamoRIO-*/docs_embed/html")
  file(GLOB allembed
    "${last_package_build_dir}/_CPack_Packages/*/*/DynamoRIO-*/docs_embed/html")
  # If there's a source package we'll have multiple.  Just take the first one.
  list(GET allembed 0 embed)
  if (EXISTS "${embed}")
    execute_process(COMMAND ${CMAKE_COMMAND} -E make_directory "${arg_outdir}/html_embed")
    execute_process(COMMAND
      ${CMAKE_COMMAND} -E copy_directory ${embed} "${arg_outdir}/html_embed")
    message("Successully copied embedded docs")
  else ()
    message(FATAL_ERROR "failed to find html docs")
  endif ()
endif ()
