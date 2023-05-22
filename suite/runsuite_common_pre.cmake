# **********************************************************
# Copyright (c) 2011-2023 Google, Inc.    All rights reserved.
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

# Test suite script for DynamoRIO split into common pieces for re-use
# by tools.
# Usage:
# 1) In tool test suite script, set the project name, src dir,
#    whether to run tests, and include the pre file:
#      set(CTEST_PROJECT_NAME "DynamoRIO")
#      set(cpack_project_name "DynamoRIO")
#      set(run_tests ON)
#      include("${PATH_TO_DR_EXPORTS}/cmake/runsuite_common_pre.cmake")
# 2) Do any custom argument processing and setting of base_cache
#    or other vars.  Can also do argument processing prior to
#    the pre include.
# 3) Define function (error_string str outvar)
#    It should set ${outvar} in PARENT_SCOPE to the set
#    of specific errors found in ${str}.
# 4) Define the build_package boolean and include the post script:
#      set(build_package ON)
#      include("${PATH_TO_DR_EXPORTS}/cmake/runsuite_common_post.cmake")
#    Also define build_source_package to build the package_source
#    CPack target (only for non-Visual Studio generators).
#
# extra_ctest_args can also be defined to pass extra args to ctest_test(),
# such as INCLUDE_LABEL.
# ARCH_IS_X86 is defined for running x86 specific tests.

# Unfinished features in i#66 (now under i#121):
# * have a list of known failures and label w/ " (known: i#XX)"

cmake_minimum_required(VERSION 3.7)
set(cmake_ver_string
  "${CMAKE_MAJOR_VERSION}.${CMAKE_MINOR_VERSION}.${CMAKE_RELEASE_VERSION}")
if (COMMAND cmake_policy)
  # avoid warnings on include()
  cmake_policy(VERSION 3.7)
endif()

# arguments are a ;-separated list (must escape as \; from ctest_run_script())
set(arg_nightly OFF)  # whether to report the results
set(arg_site "")      # site to report results for (for nightly reported runs)
set(arg_include "")   # cmake file to include up front
set(arg_preload "")   # cmake file to include prior to each 32-bit build
set(arg_preload64 "") # cmake file to include prior to each 64-bit build
set(arg_ssh OFF)      # running over cygwin ssh: disable pdbs
set(arg_use_nmake OFF)# use nmake instead of visual studio
set(arg_use_msbuild OFF) # use msbuild instead of devenv for visual studio
if (UNIX)
  set(arg_use_make ON)  # use unix make
else (UNIX)
  set(arg_use_make OFF) # use unix make instead of visual studio
endif (UNIX)
set(arg_use_ninja OFF)  # use ninja
set(arg_generator "") # specify precise cmake generator (minus any "Win64")
set(arg_long OFF)     # whether to run the long suite
set(arg_already_built OFF) # for testing w/ already-built suite
set(arg_exclude "")   # regex of tests to exclude
set(arg_verbose OFF)  # extra output
set(arg_32_only OFF)  # do not include 64-bit
set(arg_64_only OFF)  # do not include 64-bit
set(arg_build_only OFF) # do not run tests

foreach (arg ${CTEST_SCRIPT_ARG})
  if (${arg} STREQUAL "nightly")
    set(arg_nightly ON)
  endif (${arg} STREQUAL "nightly")
  if (${arg} MATCHES "^site=")
    string(REGEX REPLACE "^site=" "" arg_site "${arg}")
  endif (${arg} MATCHES "^site=")
  if (${arg} MATCHES "^include=")
    string(REGEX REPLACE "^include=" "" arg_include "${arg}")
  endif (${arg} MATCHES "^include=")
  if (${arg} MATCHES "^preload=")
    string(REGEX REPLACE "^preload=" "" arg_preload "${arg}")
  endif (${arg} MATCHES "^preload=")
  if (${arg} MATCHES "^preload64=")
    string(REGEX REPLACE "^preload64=" "" arg_preload64 "${arg}")
  endif (${arg} MATCHES "^preload64=")
  if (${arg} STREQUAL "ssh")
    set(arg_ssh ON)
  endif (${arg} STREQUAL "ssh")
  if (${arg} STREQUAL "use_nmake")
    set(arg_use_nmake ON)
  endif ()
  if (${arg} STREQUAL "use_msbuild")
    set(arg_use_msbuild ON)
  endif ()
  if (${arg} STREQUAL "use_make")
    set(arg_use_make ON)
  endif ()
  if (${arg} STREQUAL "use_ninja")
    set(arg_use_ninja ON)
  endif ()
  if (${arg} STREQUAL "verbose")
    set(arg_verbose ON)
  endif ()
  if (${arg} MATCHES "^generator=")
    string(REGEX REPLACE "^generator=" "" arg_generator "${arg}")
  endif ()
  if (${arg} STREQUAL "long")
    set(arg_long ON)
  endif (${arg} STREQUAL "long")
  if (${arg} STREQUAL "already_built")
    set(arg_already_built ON)
  endif (${arg} STREQUAL "already_built")
  if (${arg} MATCHES "^exclude=")
    # not parallel to include=.  this excludes individual tests.
    string(REGEX REPLACE "^exclude=" "" arg_exclude "${arg}")
  endif (${arg} MATCHES "^exclude=")
  if (${arg} MATCHES "^32_only")
    set(arg_32_only ON)
  endif ()
  if (${arg} MATCHES "^64_only")
    set(arg_64_only ON)
  endif ()
  if (${arg} MATCHES "^build_only")
    set(arg_build_only ON)
  endif ()
endforeach (arg)

# Returns a list of environment variable names to set in ${env_names}.
# For each "name" in the list, sets a variable "name_env_value" in the caller's
# scope containing the value to be set.
#
# We'd like to export this via utils_exposed.cmake but it is difficult to
# import it here from somewhere else.
# Our manual INCLUDEFILE expansion would require all users to point to a
# generated file somewhere, which requires updating all users including
# Dr. Memory which today points directly at the embedded submodule sources.
# Some of these uses have no build dir where a generated file could live.
# Using include() is difficult for a file that is itself included, because
# cmake won't search our same directory, and CTEST_SCRIPT_DIRECTORY is the
# includer's directory, so we can't easily have side-by-side files.
# For now we keep it here, and have the current only other use in
# tests/CMakeLists.txt include() this file.
function (_DR_set_VS_bitwidth_env_vars is64 env_names)
  # Convert env vars to run proper compiler.
  # Note that this is fragile and won't work with non-standard
  # directory layouts: we assume standard VS or SDK.
  # XXX: would be nice to have case-insensitive regex flag!
  # For now hardcoding VC, Bin, amd64
  set(must_change_path OFF)
  if (is64)
    list(APPEND names_list "ASM")
    set(ASM_env_value "ml64" PARENT_SCOPE)
    if (NOT "$ENV{LIB}" MATCHES "[Aa][Mm][Dd]64" AND
        NOT "$ENV{LIB}" MATCHES "[Xx]64")
      set(must_change_path ON)
      # Note that we can't set ENV{PATH} as the output var of the replace:
      # it has to be its own set().
      #
      # VS2017 has bin/HostX{86,64}/x{86,64}/
      string(REGEX REPLACE "((^|;)[^;]*)HostX86([/\\\\])x86" "\\1HostX64\\3x64"
        newpath "$ENV{PATH}")
      # i#1421: VS2013 needs base VC/bin on the path (for cross-compiler
      # used by cmake) so we duplicate and put amd64 first.  Older VS needs
      # Common7/IDE instead which should already be elsewhere on path.
      string(REGEX REPLACE "((^|;)[^;]*)VC([/\\\\])([Bb][Ii][Nn])"
        "\\1VC\\3\\4\\3amd64;\\1VC\\3\\4"
        newpath "${newpath}")
      # VS2008's SDKs/Windows/v{6.0A,7.0} uses "x64" instead of "amd64"
      string(REGEX REPLACE "([/\\\\]v[^/\\\\]*)([/\\\\])([Bb][Ii][Nn])"
        "\\1\\2\\3\\2x64"
        newpath "${newpath}")
      if (arg_verbose)
        message("Env setup: setting PATH to ${newpath}")
      endif ()
      # VS2017 does not append so we replace first.
      string(REGEX REPLACE "([/\\\\])x86" "\\1x64" newlib "$ENV{lib}")
      # Now try to support pre-VS2017.
      string(REGEX REPLACE "([/\\\\])([Ll][Ii][Bb])([^/\\\\])" "\\1\\2\\1amd64\\3"
        newlib "${newlib}")
      # VS2008's SDKs/Windows/v{6.0A,7.0} uses "x64" instead of "amd64": grrr
      string(REGEX REPLACE
        "([/\\\\]v[^/\\\\]*[/\\\\][Ll][Ii][Bb][/\\\\])[Aa][Mm][Dd]64"
        "\\1x64"
        newlib "${newlib}")
      # Win8 SDK uses um/x86 and um/x64 after "Lib/win{8,v6.3}/"
      string(REGEX REPLACE
        "([Ll][Ii][Bb])[/\\\\]amd64([/\\\\][Ww][Ii][Nn][v0-9.]+[/\\\\]um[/\\\\])x86"
        "\\1\\2x64" newlib "${newlib}")
      if (arg_verbose)
        message("Env setup: setting LIB to ${newlib}")
      endif ()
      string(REGEX REPLACE "([/\\\\])([Ll][Ii][Bb][/\\\\])[Xx]86" "\\1\\2"
        newlibpath "$ENV{LIBPATH}")
      string(REGEX REPLACE "([/\\\\])([Ll][Ii][Bb])" "\\1\\2\\1amd64"
        newlibpath "${newlibpath}")
      if (arg_verbose)
        message("Env setup: setting LIBPATH to ${newlibpath}")
      endif ()
    endif ()
  else (is64)
    list(APPEND ${env_names} "ASM")
    set(ASM_env_value "ml" PARENT_SCOPE)
    if ("$ENV{LIB}" MATCHES "[Aa][Mm][Dd]64" OR
        "$ENV{LIB}" MATCHES "[Xx]64")
      set(must_change_path ON)
      # VS2017 has bin/HostX{86,64}/x{86,64}/
      string(REGEX REPLACE "((^|;)[^;]*)HostX64([/\\\\])x64" "\\1HostX86\\3x86"
        newpath "$ENV{PATH}")
      # Remove the duplicate we added (see i#1421 comment above).
      string(REGEX REPLACE "((^|;)[^;]*)VC[/\\\\][Bb][Ii][Nn][/\\\\][Aa][Mm][Dd]64"
        "" newpath "${newpath}")
      if (arg_verbose)
        message("Env setup: setting PATH to ${newpath}")
      endif ()
      string(REGEX REPLACE "([Ll][Ii][Bb])[/\\\\][Aa][Mm][Dd]64" "\\1"
        newlib "$ENV{LIB}")
      string(REGEX REPLACE "([Ll][Ii][Bb][/\\\\])[Xx]64" "\\1x86"
        newlib "${newlib}")
      # Win8 SDK uses um/x86 and um/x64
      string(REGEX REPLACE "([/\\\\]um[/\\\\])x64" "\\1x86" newlib "${newlib}")
      string(REGEX REPLACE "([/\\\\]ucrt[/\\\\])x64" "\\1x86" newlib "${newlib}")
      if (arg_verbose)
        message("Env setup: setting LIB to ${newlib}")
      endif ()
      string(REGEX REPLACE "([Ll][Ii][Bb])[/\\\\][Aa][Mm][Dd]64" "\\1"
        newlibpath "$ENV{LIBPATH}")
      string(REGEX REPLACE "([Ll][Ii][Bb][/\\\\])[Xx]64" "\\1x86"
        newlibpath "${newlibpath}")
      if (arg_verbose)
        message("Env setup: setting LIBPATH to ${newlibpath}")
      endif ()
    endif ()
  endif (is64)
  if (must_change_path)
    list(APPEND names_list "PATH")
    set(PATH_env_value "${newpath}" PARENT_SCOPE)
    list(APPEND names_list "LIB")
    set(LIB_env_value "${newlib}" PARENT_SCOPE)
    list(APPEND names_list "LIBPATH")
    set(LIBPATH_env_value "${newlibpath}" PARENT_SCOPE)
  endif ()
  set(${env_names} ${names_list} PARENT_SCOPE)
endfunction ()

# allow setting the base cache variables via an include file
set(base_cache "")
if (arg_include)
  message("including ${arg_include}")
  include(${arg_include})
endif (arg_include)

if (arg_ssh)
  # avoid problems creating pdbs as cygwin ssh user (i#310)
  set(base_cache "${base_cache}
    GENERATE_PDBS:BOOL=OFF")
endif (arg_ssh)

# Make it clear that single-bitwidth packages only contain that bitwidth
# (and provide unique names for CI deployment).
if (NOT "${base_cache}" MATCHES "PACKAGE_PLATFORM")
  if (arg_64_only)
    set(base_cache "${base_cache}
      PACKAGE_PLATFORM:STRING=x86_64-")
  elseif (arg_32_only)
    set(base_cache "${base_cache}
      PACKAGE_PLATFORM:STRING=i386-")
  endif ()
endif ()
if (arg_use_make)
  find_program(MAKE_COMMAND make DOC "make command")
  if (NOT MAKE_COMMAND)
    message(FATAL_ERROR "make requested but make not found")
  endif (NOT MAKE_COMMAND)
endif (arg_use_make)

if (arg_use_ninja)
  find_program(NINJA_COMMAND ninja DOC "ninja command")
  if (NOT NINJA_COMMAND)
    message(FATAL_ERROR "ninja requested but ninja not found")
  endif (NOT NINJA_COMMAND)
endif (arg_use_ninja)

if (arg_long)
  set(TEST_LONG ON)
else (arg_long)
  set(TEST_LONG OFF)
endif (arg_long)

if (WIN32)
  find_program(CYGPATH cygpath)
  if (CYGPATH)
    set(have_cygwin ON)
  else (CYGPATH)
    set(have_cygwin OFF)
  endif (CYGPATH)
endif (WIN32)

get_filename_component(BINARY_BASE "." ABSOLUTE)

if (arg_nightly)
  # i#11: nightly run
  # Caller should have set CTEST_SITE via site= arg
  if (arg_site)
    set(CTEST_SITE "${arg_site}")
  else (arg_site)
    message(FATAL_ERROR "must set sitename via site= arg")
  endif (arg_site)

  set(CTEST_DASHBOARD_ROOT "${BINARY_BASE}")

  # We assume a manual check out was done, and that CTest can just do "update".
  # If we want a fresh checkout we can set CTEST_BACKUP_AND_RESTORE
  # and CTEST_CHECKOUT_COMMAND but the update should be fine.
  find_program(CTEST_UPDATE_COMMAND git DOC "source code update command")

  set(SUITE_TYPE Nightly)
  set(DO_UPDATE ON)
  set(DO_SUBMIT ON)
  set(SUBMIT_LOCAL OFF)
else (arg_nightly)
  # a local run, not a nightly
  set(SUITE_TYPE Experimental)
  set(DO_UPDATE OFF)
  set(DO_SUBMIT ON)
  set(SUBMIT_LOCAL ON)
  # CTest does "scp file ${CTEST_DROP_SITE}:${CTEST_DROP_LOCATION}" so for
  # local copy w/o needing sshd on localhost we arrange to have : in the
  # absolute filepath.  Note that I would prefer having the results inside
  # each build dir, but having : in the build dir name complicates
  # LD_LIBRARY_PATH.
  if (WIN32)
    # Colon not allowed in name so use drive
    string(REGEX MATCHALL "^[A-Za-z]" drive "${BINARY_BASE}")
    string(REGEX REPLACE "^[A-Za-z]:" "" nondrive "${BINARY_BASE}")
    set(CTEST_DROP_SITE "${drive}")
    set(CTEST_DROP_LOCATION "${nondrive}/xmlresults")
    set(RESULTS_DIR "${CTEST_DROP_SITE}:${CTEST_DROP_LOCATION}")
  else (WIN32)
    set(CTEST_DROP_SITE "${BINARY_BASE}/xml")
    set(CTEST_DROP_LOCATION "results")
    set(RESULTS_DIR "${CTEST_DROP_SITE}:${CTEST_DROP_LOCATION}")
  endif (WIN32)
  if (EXISTS "${RESULTS_DIR}")
    file(REMOVE_RECURSE "${RESULTS_DIR}")
  endif (EXISTS "${RESULTS_DIR}")
  file(MAKE_DIRECTORY "${CTEST_DROP_SITE}:${CTEST_DROP_LOCATION}")
endif (arg_nightly)

# Find the number of CPUs on the current system.
# Cribbed from http://www.kitware.com/blog/home/post/63
if(NOT DEFINED PROCESSOR_COUNT)
  # Unknown:
  set(PROCESSOR_COUNT 4)  # Guess

  # Linux:
  set(cpuinfo_file "/proc/cpuinfo")
  if(EXISTS "${cpuinfo_file}")
    file(STRINGS "${cpuinfo_file}" procs REGEX "^processor.: [0-9]+$")
    list(LENGTH procs PROCESSOR_COUNT)
  endif()

  # Mac:
  if(APPLE)
    find_program(cmd_sys_pro "system_profiler")
    if(cmd_sys_pro)
      execute_process(COMMAND ${cmd_sys_pro} SPHardwareDataType OUTPUT_VARIABLE info)
      string(REGEX REPLACE "^.*Total Number [Oo]f Cores: ([0-9]+).*$" "\\1"
        PROCESSOR_COUNT "${info}")
    endif()
  endif()

  # Windows:
  if(WIN32)
    if ($ENV{NUMBER_OF_PROCESSORS})
      set(PROCESSOR_COUNT "$ENV{NUMBER_OF_PROCESSORS}")
    endif ()
  endif()
endif()

math(EXPR PARALLEL_COUNT_BUILD "${PROCESSOR_COUNT} + 2")
# Typically no benefit and sometimes detrimental to exceed core count here
math(EXPR PARALLEL_COUNT_TEST "${PROCESSOR_COUNT}")

# CTest goes and does our builds and then wants to configure
# and build again and complains there's no top-level setting of
# CTEST_BINARY_DIRECTORY:
#   "CMake Error: Some required settings in the configuration file were missing:"
# but we don't want to do another build so we just ignore the error.
set(CTEST_CMAKE_COMMAND "${CMAKE_EXECUTABLE_NAME}")
# outer file should set CTEST_PROJECT_NAME
set(CTEST_COMMAND "${CTEST_EXECUTABLE_NAME}")

# Raise the default of 50 to avoid warnings blocking the detection of
# build failures (i#1137):
set(CTEST_CUSTOM_MAXIMUM_NUMBER_OF_ERRORS 200)
set(CTEST_CUSTOM_MAXIMUM_NUMBER_OF_WARNINGS 200)

# Detect if the arch is x86
if (NOT DEFINED ARCH_IS_X86)
  if (CMAKE_SYSTEM_PROCESSOR MATCHES "^(arm|aarch64)")
    set(ARCH_IS_X86 OFF)
  else ()
    set(ARCH_IS_X86 ON)
  endif ()
endif ()

# Detect if the kernel is ia32 or x64.  If the kernel is ia32, there's no sense
# in trying to run any x64 code.  On Windows, the x64 toolchain is built as x64
# code, so we can't even build.  On Linux, it's possible to have an ia32
# toolchain that targets x64, but we don't currently support it.
if (NOT DEFINED KERNEL_IS_X64)  # Allow variable override.
  if (WIN32)
    # Check both PROCESSOR_ARCHITECTURE and PROCESSOR_ARCHITEW6432 in case CMake
    # was built x64.
    if ("$ENV{PROCESSOR_ARCHITECTURE}" MATCHES "AMD64" OR
        "$ENV{PROCESSOR_ARCHITEW6432}" MATCHES "AMD64")
      set(KERNEL_IS_X64 ON)
    else ()
      set(KERNEL_IS_X64 OFF)
    endif ()
  else ()
    # uname -m is what the kernel supports.
    execute_process(COMMAND uname -m
      OUTPUT_VARIABLE machine
      RESULT_VARIABLE cmd_result)
    # If for some reason uname fails (not on PATH), assume the kernel is x64
    # anyway.
    if (cmd_result OR "${machine}" MATCHES "x86_64|aarch64|arm64")
      set(KERNEL_IS_X64 ON)
    else ()
      set(KERNEL_IS_X64 OFF)
    endif ()
  endif ()
endif ()
if (NOT KERNEL_IS_X64)
  message("WARNING: Kernel is not x64, skipping x64 builds")
endif ()

if (arg_use_ninja)
  set(CTEST_CMAKE_GENERATOR "Ninja")
elseif (arg_use_make)
  set(CTEST_CMAKE_GENERATOR "Unix Makefiles")
  find_program(MAKE_COMMAND make DOC "make command")
  if (have_cygwin)
    # seeing errors building in parallel: pdb collision?
    # can't repro w/ VERBOSE=1
    set(CTEST_BUILD_COMMAND_BASE "${MAKE_COMMAND} -j2")
  else (have_cygwin)
    set(CTEST_BUILD_COMMAND_BASE "${MAKE_COMMAND} -j${PARALLEL_COUNT_BUILD}")
  endif (have_cygwin)
elseif (arg_use_nmake)
  set(CTEST_CMAKE_GENERATOR "NMake Makefiles")
else ()
  if (arg_generator)
    set(vs_generator ${arg_generator})
  else ()
    set(vs_generator "NOTFOUND")
    get_filename_component(current_directory_path "${CMAKE_CURRENT_LIST_FILE}" PATH)
    include(${current_directory_path}/lookup_visualstudio.cmake)
    get_visualstudio_info(vs_dir vs_version vs_generator)
    if (NOT vs_generator)
      message(FATAL_ERROR "Cannot determine Visual Studio version")
    endif ()
  endif ()
  message(STATUS "Using ${vs_generator} generators")
  if (arg_use_msbuild)
    find_program(MSBUILD_PROGRAM msbuild)
    if (MSBUILD_PROGRAM)
      set(base_cache "${base_cache}
        CMAKE_MAKE_PROGRAM:FILEPATH=${MSBUILD_PROGRAM}")
      # Unfortunately we have to provide all the args (setting CMAKE_MAKE_PROGRAM
      # doesn't do it for ctest builds).  We want ALL_BUILD.vcproj for pre-VS2010
      # and ALL_BUILD.vcxproj for VS2010.
      if (vs_generator MATCHES "Visual Studio 1.")
        set(proj_file "ALL_BUILD.vcxproj")
      else ()
        set(proj_file "ALL_BUILD.vcproj")
      endif ()
      # Request parallel build on all available cores (sequential by default: i#800)
      # via "/m".
      # Configuration will be adjusted per build below.
      set(CTEST_BUILD_COMMAND
        "${MSBUILD_PROGRAM} ${proj_file} /p:Configuration=REPLACE_CONFIG /m")
    else (MSBUILD_PROGRAM)
      message("WARNING: cannot find msbuild; disabling")
      set(arg_use_msbuild OFF)
    endif (MSBUILD_PROGRAM)
  endif (arg_use_msbuild)
endif ()

function(get_default_config config builddir)
  file(READ "${builddir}/CMakeCache.txt" cache)
  string(REGEX MATCH "CMAKE_CONFIGURATION_TYPES:STRING=([^\n]*)" cache "${cache}")
  string(REGEX REPLACE "CMAKE_CONFIGURATION_TYPES:STRING=([^;]*)" "\\1"
    cache "${cache}")
  set(${config} ${cache} PARENT_SCOPE)
endfunction(get_default_config)

# Sets two vars in PARENT_SCOPE:
# + returns the build dir in "last_build_dir"
# + for each build for which add_to_package is true:
#   - returns the build dir in "last_package_build_dir"
#   - adds to "cpack_projects" for project "${cpack_project_name}"
#     (set by outer file)
function(testbuild_ex name is64 initial_cache test_only_in_long
    add_to_package build_args)
  set(CTEST_BUILD_NAME "${name}")

  # Skip x64 builds on a true ia32 machine.
  if (is64 AND NOT KERNEL_IS_X64)
    return()
  endif ()
  if (is64 AND arg_32_only)
    return()
  endif ()
  if (NOT is64 AND arg_64_only)
    return()
  endif ()

  # Skip 32-bit builds on an AArch64 machine.
  if (NOT is64 AND CMAKE_SYSTEM_PROCESSOR MATCHES "^aarch64")
    return()
  endif ()

  if (NOT arg_use_nmake AND NOT arg_use_make AND NOT arg_use_ninja)
    # we need a separate generator for 64-bit as well as the PATH
    # env var changes below (since we run cl directly)
    if (is64)
      set(CTEST_CMAKE_GENERATOR "${vs_generator} Win64")
    else (is64)
      set(CTEST_CMAKE_GENERATOR "${vs_generator}")
    endif (is64)
    # we need to set this for the package build using the last build
    set(CTEST_CMAKE_GENERATOR "${CTEST_CMAKE_GENERATOR}" PARENT_SCOPE)
  endif ()
  set(CTEST_BINARY_DIRECTORY "${BINARY_BASE}/build_${CTEST_BUILD_NAME}")
  set(last_build_dir "${CTEST_BINARY_DIRECTORY}" PARENT_SCOPE)

  if (NOT arg_already_built)
    # Support other VC installations than VS2005 via pre-build include file.
    # Preserve path so include file can simply prepend each time.
    set(pre_path "$ENV{PATH}")
    set(pre_lib "$ENV{LIB}")
    if (is64)
      set(preload_file "${arg_preload64}")
    else (is64)
      set(preload_file "${arg_preload}")
    endif (is64)
    if (preload_file)
      # Command-style CTest (i.e., using ctest_configure(), etc.) does
      # not support giving args to CTEST_CMAKE_COMMAND so we are forced
      # to do an include() instead of -C
      include("${preload_file}")
    endif (preload_file)
    if (WIN32)
      # i#111: Disabling progress and status messages can speed the Windows build
      # up by up to 50%.  I filed CMake bug 8726 on this and this variable will be
      # in the 2.8 release; going ahead and setting now.
      # For Linux these messages make little perf difference, and can help
      # diagnose errors or watch progress.
      # The messages only apply to Makefile generators, not Visual Studio.
      if (NOT "${CTEST_CMAKE_GENERATOR}" MATCHES "Visual Studio")
        set(os_specific_defines "CMAKE_RULE_MESSAGES:BOOL=OFF")
      endif ()
    else (WIN32)
      set(os_specific_defines "")
    endif (WIN32)
    set(CTEST_INITIAL_CACHE "${initial_cache}
      TEST_SUITE:BOOL=ON
      ${os_specific_defines}
      ${base_cache}
    ")
    ctest_empty_binary_directory(${CTEST_BINARY_DIRECTORY})
    file(WRITE "${CTEST_BINARY_DIRECTORY}/CMakeCache.txt" "${CTEST_INITIAL_CACHE}")

    if (WIN32)
      # If other compilers also on path ensure we pick cl
      set(ENV{CC} "cl")
      set(ENV{CXX} "cl")
      # Convert env vars to run proper compiler.
      _DR_set_VS_bitwidth_env_vars(${is64} env_names)
      foreach (env ${env_names})
        set(ENV{${env}} "${${env}_env_value}")
      endforeach ()
    else (WIN32)
      if (ARCH_IS_X86)
        if (is64)
          set(ENV{CFLAGS} "-m64")
          set(ENV{CXXFLAGS} "-m64")
        else (is64)
          set(ENV{CFLAGS} "-m32")
          set(ENV{CXXFLAGS} "-m32")
        endif (is64)
      endif (ARCH_IS_X86)
    endif (WIN32)
  else (NOT arg_already_built)
    # remove the Last* files from the prior run
    file(GLOB lastrun ${CTEST_BINARY_DIRECTORY}/Testing/Temporary/Last*)
    if (lastrun)
      file(REMOVE ${lastrun})
    endif (lastrun)
  endif (NOT arg_already_built)

  ctest_start(${SUITE_TYPE})
  if (NOT arg_already_built)
    if (DO_UPDATE)
      ctest_update(SOURCE "${CTEST_SOURCE_DIRECTORY}")
    endif (DO_UPDATE)
    ctest_configure(BUILD "${CTEST_BINARY_DIRECTORY}" RETURN_VALUE config_success)
    if (config_success EQUAL 0)

      # support "make install"
      if (DEFINED CTEST_BUILD_COMMAND_BASE)
        set(CTEST_BUILD_COMMAND "${CTEST_BUILD_COMMAND_BASE} ${build_args}")
      else () # else use default
        if ("${CTEST_CMAKE_GENERATOR}" MATCHES "Visual Studio")
          # workaround for cmake bug #11830 where assumes "Debug" is
          # the default config
          get_default_config(defconfig "${CTEST_BINARY_DIRECTORY}")
          set(CTEST_BUILD_CONFIGURATION "${defconfig}")
          message("building default config \"${defconfig}\"")
          if (NOT "${build_args}" STREQUAL "" OR
              NOT "${extra_build_args}" STREQUAL "")
            set(CTEST_BUILD_FLAGS "-- ${extra_build_args} ${build_args}")
          endif ()
          if (arg_use_msbuild)
            string(REPLACE "REPLACE_CONFIG" "${defconfig}" CTEST_BUILD_COMMAND
              "${CTEST_BUILD_COMMAND}")
          endif (arg_use_msbuild)
        else ()
          set(CTEST_BUILD_FLAGS "${build_args}")
        endif ()
        message("building with args \"${CTEST_BUILD_FLAGS}\"")
      endif ()

      ctest_build(BUILD "${CTEST_BINARY_DIRECTORY}" RETURN_VALUE build_success)

    else (config_success EQUAL 0)
      set(build_success -1)
      if (optional_cross_compile)
        # XXX: for platforms requiring special hardware such as ARM, this global indicates
        # an optional cross-compilation, for use on more common platforms like x86 Unix
        # (would use a parameter instead of a global, except for backward compatibility)
        file(GLOB last_config ${CTEST_BINARY_DIRECTORY}/Testing/*/*.xml)
        if (last_config)
          file(REMOVE ${last_config})
        endif (last_config)
        set(DO_SUBMIT OFF)
        message("Warning: optional cross-compilation \"${name}\" is not available"
          "--skipping")
        set(add_to_package OFF)
        file(WRITE ${CTEST_BINARY_DIRECTORY}/Testing/missing-cross-compile "${name}")
      endif (optional_cross_compile)
    endif (config_success EQUAL 0)
  else (NOT arg_already_built)
    set(build_success 0)
  endif (NOT arg_already_built)

  if (build_success EQUAL 0 AND run_tests AND NOT arg_build_only)
    if (NOT test_only_in_long OR ${TEST_LONG})
      # to run a subset of tests add an INCLUDE regexp to ctest_test.  e.g.:
      #   INCLUDE broadfun
      if (NOT "${arg_exclude}" STREQUAL "")
        set(ctest_test_args ${ctest_test_args} EXCLUDE ${arg_exclude})
      endif (NOT "${arg_exclude}" STREQUAL "")
      set(ctest_test_args ${ctest_test_args} ${extra_ctest_args})
      if ("${CMAKE_VERSION}" VERSION_GREATER_EQUAL "3.17")
        # CMake 3.17 supports retrying failed tests.  We avoid flaky tests on
        # particular platforms failing the whole suite by trying multiple times.
        set(ctest_test_args ${ctest_test_args} "REPEAT" "UNTIL_PASS:3")
      endif ()
      if (WIN32 AND TEST_LONG)
        # FIXME i#265: on Windows we can't run multiple instances of
        # the same app b/c of global reg key conflicts: should support
        # env vars and not require registry
        set(RUN_PARALLEL OFF)
      else ()
        set(RUN_PARALLEL ON)
      endif ()
      if (RUN_PARALLEL)
        # i#111: run tests in parallel, supported on CTest 2.8.0+
        # Note that adding -j to CMAKE_COMMAND does not work, though invoking
        # this script with -j does work, but we want parallel by default.
        ctest_test(BUILD "${CTEST_BINARY_DIRECTORY}"
          PARALLEL_LEVEL ${PARALLEL_COUNT_TEST} ${ctest_test_args})
      else (RUN_PARALLEL)
        ctest_test(BUILD "${CTEST_BINARY_DIRECTORY}" ${ctest_test_args})
      endif (RUN_PARALLEL)
    endif (NOT test_only_in_long OR ${TEST_LONG})
  endif (build_success EQUAL 0 AND run_tests AND NOT arg_build_only)

  if (DO_SUBMIT)
    if (CTEST_DROP_METHOD MATCHES "http")
      # include any notes via set(CTEST_NOTES_FILES )?
      ctest_submit()
    else ()
      # We used to use the "scp" (could also have used "cp") drop method
      # to copy these xml files out for us, but cmake 3.14 dropped that.
      # Thus we copy them ourselves.
      file(GLOB xml_files "${CTEST_BINARY_DIRECTORY}/Testing/*/*.xml")
      set(prefix "___${CTEST_BUILD_NAME}___${SUITE_TYPE}___XML___")
      foreach (xml ${xml_files})
        get_filename_component(base "${xml}" NAME)
        # Avoid confusion with a later package build in the same dir by
        # renaming instead of copying.
        file(RENAME "${xml}"
          "${CTEST_DROP_SITE}:${CTEST_DROP_LOCATION}/${prefix}${base}")
        message("Moving ${xml} to "
          "${CTEST_DROP_SITE}:${CTEST_DROP_LOCATION}/${prefix}${base}")
      endforeach ()
    endif ()
  endif (DO_SUBMIT)
  if (NOT arg_already_built)
    set(ENV{PATH} "${pre_path}")
    set(ENV{LIB} "${pre_lib}")
  endif (NOT arg_already_built)

  if (add_to_package)
    # support having the suite test building the full package
    # FIXME: perhaps should replace package.cmake w/ invocation of runsuite.cmake
    # w/ certain params, since they're pretty similar at this point?
    # communicate w/ caller
    set(last_package_build_dir "${CTEST_BINARY_DIRECTORY}" PARENT_SCOPE)
    if (CTEST_BINARY_DIRECTORY MATCHES "debug")
      # prepend rather than append to get debug first, so we take release
      # files preferentially in case of overlap
      set(cpack_projects
        "\"${CTEST_BINARY_DIRECTORY};${cpack_project_name};ALL;/\"\n  ${cpack_projects}"
        PARENT_SCOPE)
    else ()
      set(cpack_projects
        "${cpack_projects}\n  \"${CTEST_BINARY_DIRECTORY};${cpack_project_name};ALL;/\""
        PARENT_SCOPE)
    endif ()

    if ("${CTEST_CMAKE_GENERATOR}" MATCHES "Visual Studio")
      # i#390: workaround for cpack limitation where cpack is run w/
      # one config and each build's install rules check only for their
      # own config.  we change each config check to "TRUE OR <check>".
      file(READ "${CTEST_BINARY_DIRECTORY}/cmake_install.cmake" str)
      # grab first-level includes
      string(REGEX MATCHALL "INCLUDE\\\(\"[^\"]+\"" includes "${str}")
      string(REGEX REPLACE "INCLUDE\\\(\"([^\"]+)\"" "\\1" includes "${includes}")
      # grab second-level includes
      set(includes2nd )
      foreach (config ${includes})
        file(READ "${config}" str)
        string(REGEX MATCHALL "INCLUDE\\\(\"[^\"]+\"" newincs "${str}")
        string(REGEX REPLACE "INCLUDE\\\(\"([^\"]+)\"" "\\1" newincs "${newincs}")
        set(includes2nd ${includes2nd} ${newincs})
      endforeach (config)
      # now process each
      foreach (config "${CTEST_BINARY_DIRECTORY}/cmake_install.cmake"
          ${includes} ${includes2nd})
        file(READ "${config}" str)
        string(REGEX REPLACE "IF\\\((\"\\\${CMAKE_INSTALL_CONFIG_NAME}\" MATCHES)"
          "IF(TRUE OR \\1" str "${str}")
        # set CMP0012 policy to treat TRUE as literal
        file(WRITE "${config}" "cmake_policy(VERSION 2.8)\n${str}")
      endforeach (config)
    endif ()
  endif (add_to_package)

endfunction(testbuild_ex)

function(testbuild name is64 initial_cache)
  # by default run all tests and do not include in package
  testbuild_ex(${name} ${is64} ${initial_cache} OFF OFF "")
  # propagate
  set(last_build_dir "${last_build_dir}" PARENT_SCOPE)
  set(last_package_build_dir "${last_package_build_dir}" PARENT_SCOPE)
  set(cpack_projects "${cpack_projects}" PARENT_SCOPE)
endfunction(testbuild)
