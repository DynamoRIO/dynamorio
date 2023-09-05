# **********************************************************
# Copyright (c) 2010-2023 Google, Inc.    All rights reserved.
# Copyright (c) 2009-2010 VMware, Inc.    All rights reserved.
# Copyright (c) 2018 Arm Limited          All rights reserved.
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

###########################################################################
# Missing features:
# i#77: make DRstats a subproject to support with different compiler from libutil
# i#84: nmake adds space before 1st / in 1st arg to a command invoked with quotes
# i#74: create source tarball via 'make package_source'
# i#68: replace perl with cmake scripts and CTest
# i#70: symbol store support in core/Makefile and tools/Makefile
# i#72: RHEL3 linker script has no __executable_start
# i#60: re-add libutil/ unit tests
#
# Not filed:
# * move info like "build core solely from DDK" from make/compiler.mk into
#   HowToBuild.wiki
###########################################################################

# Visual Studio 2017 requires 3.7.
cmake_minimum_required(VERSION 3.7)

include(make/policies.cmake NO_POLICY_SCOPE)

# for non-makefile-based generators (i.e., Visual Studio) there is no
# CMAKE_BUILD_TYPE and instead there are multiple configs.
# note that we would love cmake to support multi-config for makefiles too,
# but given that it doesn't, and that we don't have full control
# over the output dir names (until cmake 2.8.4), I'm collapsing
# the VS generator configs to a single choice to match the makefiles.
# this must be done prior to the project() command and the var
# must be set in the cache.
if ("${CMAKE_GENERATOR}" MATCHES "Visual Studio")
  # Workaround for https://gitlab.kitware.com/cmake/cmake/issues/17992
  set(CMAKE_SYSTEM_VERSION_TEMP $ENV{WindowsSDKVersion})
  string(REGEX REPLACE "([0-9.]+).*" "\\1" CMAKE_SYSTEM_VERSION_TEMP "${CMAKE_SYSTEM_VERSION_TEMP}")
  if (CMAKE_SYSTEM_VERSION_TEMP)
    set(CMAKE_SYSTEM_VERSION ${CMAKE_SYSTEM_VERSION_TEMP} CACHE INTERNAL "")
  endif()
  if (DEBUG)
    set(CMAKE_CONFIGURATION_TYPES "Debug" CACHE STRING "" FORCE)
  else (DEBUG)
    set(CMAKE_CONFIGURATION_TYPES "RelWithDebInfo" CACHE STRING "" FORCE)
  endif (DEBUG)
  # we want to use the <VAR>_<config> variants of config-dependent properties
  string(TOUPPER "${CMAKE_CONFIGURATION_TYPES}" upper)
  set(location_suffix "_${upper}")
else ("${CMAKE_GENERATOR}" MATCHES "Visual Studio")
  set(location_suffix "")
endif ("${CMAKE_GENERATOR}" MATCHES "Visual Studio")

# I want to override the default CMAKE_INSTALL_PREFIX, but allow it to
# be set (as the same var name, so CPack and other standard tools
# work) externally.  The best solution is to check whether defined BEFORE
# the project() command.
# If we didn't use standard tools we could set CMAKE_INSTALL_PREFIX
# to be CACHE INTERNAL FORCE to INSTALL_PREFIX.
if (NOT DEFINED CMAKE_INSTALL_PREFIX)
  set(install_override ON)
else (NOT DEFINED CMAKE_INSTALL_PREFIX)
  set(install_override OFF)
endif (NOT DEFINED CMAKE_INSTALL_PREFIX)

# Allow users to set -m32 in just CFLAGS and have it apply to CXXFLAGS as well.
# CMake puts such flags in various variables early on and we'd have to go
# manually add to CMAKE_SHARED_LIBRARY_CXX_FLAGS or something to fix later.
if (NOT DEFINED ENV{CXXFLAGS})
  set(ENV{CXXFLAGS} "$ENV{CFLAGS}")
endif (NOT DEFINED ENV{CXXFLAGS})

# Don't use the default-Debug build type set for the try-compiles
set(specified_build_type "${CMAKE_BUILD_TYPE}")
project(DynamoRIO NONE)
if (DEFINED GENERATE_PDBS AND NOT GENERATE_PDBS)
  # i#310: support building over cygwin ssh where we cannot build pdbs.
  # To prevent cmake's try-compile for its working compiler test and
  # its ABI determination test we request a Release build config
  # via a custom Plaform/Windows-cl.cmake in our make/ dir.
  set(CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/make")
endif ()
# We have some Find*.cmake modules of our own
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${PROJECT_SOURCE_DIR}/make/modules")
enable_language(C)
enable_language(CXX)
if ("${CMAKE_GENERATOR}" MATCHES "Ninja")
  # i#1040: Ninja sets the type to Debug if it's not in the cache
  set(CMAKE_BUILD_TYPE "${specified_build_type}" CACHE STRING
    "Choose the type of build: Debug or Release" FORCE)
else ()
  set(CMAKE_BUILD_TYPE "${specified_build_type}")
endif ()

include(CheckCCompilerFlag)
include(CheckCXXCompilerFlag)
include(CheckIncludeFile)

###########################################################################
# utility functions

include(make/utils.cmake)

if (UNIX)
  set(LIB_PFX "lib")
  if (APPLE)
    set(LIB_EXT ".dylib")
  else ()
    set(LIB_EXT ".so")
  endif ()
else (UNIX)
  set(LIB_PFX "")
  set(LIB_EXT ".dll")
endif (UNIX)

###########################################################################
# configuration options

# many are core-specific

# whether this build of DynamoRIO is meant for distribution, in which case
# glibc should not have any too-recent imports. See core/CMake_readelf.cmake
# for more information. This is off by default, but gets set by
# make/package.cmake.
option(BUILD_PACKAGE "build DynamoRIO for packaging purposes by performing glibc checks" OFF)

# configurations that also have defines
option(DRSTATS_DEMO "build DRstats without the no-longer-supported Run, etc. controls" ON)

# we honor CMAKE_BUILD_TYPE since many cmake users are used to it
if ("${CMAKE_BUILD_TYPE}" MATCHES "Debug")
  set(DEBUG_DEFAULT ON)
else ()
  set(DEBUG_DEFAULT OFF)
endif ()
set(DEBUG_DESCR "Build with asserts and logging enabled (also controlled by CMAKE_BUILD_TYPE=Debug)")
option(DEBUG ${DEBUG_DESCR} ${DEBUG_DEFAULT})
# support later changes
if ("${CMAKE_BUILD_TYPE}" MATCHES "Debug")
  set(DEBUG ON CACHE BOOL ${DEBUG_DESCR} FORCE)
elseif (NOT "${CMAKE_BUILD_TYPE}" STREQUAL "")
  set(DEBUG OFF CACHE BOOL ${DEBUG_DESCR} FORCE)
endif ()

# INTERNAL option now matches DEBUG
if (DEBUG)
  set(INTERNAL_DEFAULT ON)
else (DEBUG)
  set(INTERNAL_DEFAULT OFF)
endif (DEBUG)
set(INTERNAL "" CACHE STRING "for developer use: ON or OFF overrides default")
if ("${INTERNAL}" STREQUAL "")
  set(INTERNAL ${INTERNAL_DEFAULT})
endif()

# The target OS:
if (APPLE)
  set(MACOS 1)
  # Ensure our binaries can run on older OSX but only as far back as is
  # officially supported by Apple.
  set(OLDEST_OSX_SUPPPORTED "11.7")
elseif (UNIX)
  set(LINUX 1)
endif (APPLE)
if (WIN32)
  set(WINDOWS 1)
endif (WIN32)

# Assuming we cross compile on a Linux system and UNIX is set by CMake.
# ANDROID is not mutually exclusive with LINUX or UNIX.
if (CMAKE_SYSTEM_NAME MATCHES "^Android")
  set(ANDROID 1)
  # We prefix new options with DR_ to make it easier for containing projects including
  # DR to avoid name conflicts and to separate options.
  set(DR_DEVICE_BASEDIR "/data/local/tmp" CACHE STRING "base dir for Android binaries")
  option(DR_COPY_TO_DEVICE "copy cross-compiled binaries to DR_DEVICE_BASEDIR" OFF)
  if (DR_COPY_TO_DEVICE)
    find_program(ADB adb DOC "adb Android utility")
    if (NOT ADB)
      message(FATAL_ERROR "Unable to find adb for DR_COPY_TO_DEVICE")
    else ()
      execute_process(COMMAND ${ADB} get-state
        RESULT_VARIABLE adb_result
        ERROR_VARIABLE adb_err
        OUTPUT_VARIABLE adb_out OUTPUT_STRIP_TRAILING_WHITESPACE)
      if (adb_result OR NOT adb_out STREQUAL "device")
        message(FATAL_ERROR "Android device not connected for DR_COPY_TO_DEVICE")
      endif ()
    endif ()
  endif ()
endif ()

# The target architecture.
# For cross-compilation this should still work as you're supposed to set this var.
# X64 mean 64-bit generically, whether AMD64 or AARCH64.
set(TARGET_ARCH "${CMAKE_SYSTEM_PROCESSOR}" CACHE STRING "Target architecture")
if (TARGET_ARCH MATCHES "^arm64" OR TARGET_ARCH MATCHES "^aarch64")
  set(AARCH64 1)
  set(X64 1)
  message(STATUS "Building for AArch64")
elseif (TARGET_ARCH MATCHES "^arm")
  set(ARM 1) # This means AArch32.
  set(X64 OFF)
  message(STATUS "Building for ARM")
elseif (TARGET_ARCH MATCHES "^riscv64")
  set(RISCV64 1)
  set(X64 1)
  message(STATUS "Building for riscv64")
else ()
  set(X86 1) # This means IA-32 or AMD64
  message(STATUS "Building for x86")
  # Whether 64-bit is expected to be selected by user setting up compiler
  # prior to invoking CMake: it has to be that way for Windows, and for
  # Linux the user should set CFLAGS to -m32 or -m64
  # to override gcc's default.  To simplify matters we only look at
  # CMAKE_C_SIZEOF_DATA_PTR, controlled by CFLAGS, so the user doesn't
  # have to also set CXXFLAGS (CMAKE_SIZEOF_VOID_P happens
  # to come from CXXFLAGS).  (CMAKE_C_SIZEOF_DATA_PTR can be relied on
  # to be set in all CMake versions we support.)
  # For target!=host, we still need the host compiler to match the target,
  # so we do not support differing bitwidths.
  if (CMAKE_C_SIZEOF_DATA_PTR EQUAL 8)
    set(X64 ON)
  else ()
    set(X64 OFF)
  endif ()
endif ()

if (X86)
  set(ARCH_NAME x86)
elseif (ARM)
  set(ARCH_NAME arm)
elseif (AARCH64)
  set(ARCH_NAME aarch64)
elseif (RISCV64)
  set(ARCH_NAME riscv64)
else ()
  message(FATAL_ERROR "Unknown architecture target")
endif ()

if (ARM OR AARCH64)
  set(AARCHXX 1)
  set(ARCH_NAME_SHARED aarchxx)
else ()
  set(ARCH_NAME_SHARED ${ARCH_NAME})
endif ()

# The execution architecture, which might differ from the target for building
# an AArch64 decoder to execute on x86 machines (i#1684).
if (CMAKE_SYSTEM_PROCESSOR MATCHES "^aarch64" OR CMAKE_SYSTEM_PROCESSOR MATCHES "^arm64")
  set(DR_HOST_AARCH64 1)
  set(DR_HOST_ARCH_NAME "aarch64")
  set(DR_HOST_AARCHXX 1)
  set(DR_HOST_ARCH_NAME_SHARED aarchxx)
  set(DR_HOST_X64 1)
elseif (CMAKE_SYSTEM_PROCESSOR MATCHES "^arm")
  set(DR_HOST_ARM 1)
  set(DR_HOST_ARCH_NAME "arm")
  set(DR_HOST_AARCHXX 1)
  set(DR_HOST_ARCH_NAME_SHARED aarchxx)
elseif (CMAKE_SYSTEM_PROCESSOR MATCHES "^riscv64")
  set(DR_HOST_RISCV64 1)
  set(DR_HOST_ARCH_NAME "riscv64")
  set(DR_HOST_ARCH_NAME_SHARED ${DR_HOST_ARCH_NAME})
  set(DR_HOST_X64 1)
elseif (CMAKE_C_SIZEOF_DATA_PTR EQUAL 8)
  set(DR_HOST_X86 1)
  set(DR_HOST_ARCH_NAME "x86")
  set(DR_HOST_ARCH_NAME_SHARED ${DR_HOST_ARCH_NAME})
  set(DR_HOST_X64 1)
else ()
  set(DR_HOST_X86 1)
  set(DR_HOST_ARCH_NAME "x86")
  set(DR_HOST_ARCH_NAME_SHARED ${DR_HOST_ARCH_NAME})
endif ()
if (NOT "${TARGET_ARCH}" STREQUAL "${CMAKE_SYSTEM_PROCESSOR}")
  set(DR_HOST_NOT_TARGET 1)
  if (WIN32)
    # TODO i#1684: Fix Windows compiler warnings for AArch64 on x86.
    message(FATAL_ERROR "Host != target is not yet supported on Windows")
  endif ()
  if ((DR_HOST_X64 AND NOT X64) OR (NOT DR_HOST_X64 AND X64))
    # XXX i#1345: We need to resolve the hardcoded ELF bitwidth types.
    # We likely also have general C type issues if the host is 32-bit.
    message(FATAL_ERROR "Different-bitwidth host-vs-target not supported: i#1345.")
  endif ()
  if (ARM)
    # TODO i#1684: We don't have full support for targeting arm in i386 yet.
    message(FATAL_ERROR "Targeting ARM on i386 is not yet supported: i#1684.")
  endif ()
  if (RISCV64)
    # TODO #1684: We don't have full support for targeting RISC-V in i386 yet.
    message(FATAL_ERROR "Targeting riscv64 on i386 is not yet supported.")
  endif ()
endif ()

# Support for running cross-compiled tests under emulation.
if (CMAKE_CROSSCOMPILING AND DEFINED CMAKE_FIND_ROOT_PATH)
  find_program(QEMU_BINARY qemu-${CMAKE_SYSTEM_PROCESSOR} DOC "QEMU emulation tool")
  if (NOT QEMU_BINARY)
    message(STATUS "Did not find qemu-${CMAKE_SYSTEM_PROCESSOR}: tests will not run")
  else ()
    message(STATUS "Found qemu-${CMAKE_SYSTEM_PROCESSOR} for tests under emulation")
  endif ()
endif ()

option(VMKERNEL "target VMkernel (not officially supported yet)")

# We no longer support building without a client interface: it is always enabled.

# TODO i#4819: Remove {DR_}APP_EXPORTS and replace with a runtime option.
# We'll still need something to swap DR_APP_API from import to export and
# set that for static DR, but the rest of the defines can go.
set(APP_EXPORTS 1)

# TODO: Consider removing these no-longer-maintained options:
# - VMKERNEL
# - PROGRAM_SHEPHERDING
# - PROCESS_CONTROL
# - GBOP

# TODO: Revive HOT_PATCHING_INTERFACE and combine with PROBE.
option(PROBE "enable not-yet-supported Probe API")
mark_as_advanced(PROBE)
if (PROBE)
  set(HOT_PATCHING_INTERFACE 1)
endif (PROBE)

option(TEST_SUITE "we are running a series of builds for official purposes")

# For developers
# CMake note: never set option vars as it prevents override by cache var:
# instead set default value in separate var and use that to initialize option.
# For a dependent option, use a string var and only set it if the string is ""
# (xref i#170 and see below).
if (INTERNAL OR DEBUG)
  set(KSTATS_DEFAULT ON)
else (INTERNAL OR DEBUG)
  set(KSTATS_DEFAULT 0FF)
endif (INTERNAL OR DEBUG)

# no KSTATS for caller profiling: we want to be as close to release
# build as we can, but w/o optimizations
if (CALLPROF)
  set(KSTATS_DEFAULT 0FF)
endif (CALLPROF)

set(KSTATS "" CACHE STRING "internal kstat profiling: ON or OFF overrides default")
# FIXME i#170: once CMake 2.8 is released we can detect whether 2.8 is
# in use, and if so use a value-enum to get drop-down of possible values, like so:
#   set_property(CACHE KSTATS PROPERTY STRINGS "" "ON" "OFF")
if ("${KSTATS}" STREQUAL "")
    set(KSTATS ${KSTATS_DEFAULT})
endif()

option(CALLPROF "internal caller profiling support")
option(PROFILE "profiling build: disables FPO and tweaks other flags" OFF)

if (UNIX)
  option(RECORD_MEMQUERY "generate test cases for memquery_library_bounds_by_iterator")
endif ()

if (X86)
  set(ANNOTATIONS_DEFAULT ON)
else ()
  # TODO i#1672: Add annotation support to AArchXX.
  set(ANNOTATIONS_DEFAULT OFF)
endif ()
# TODO i#4819: Remove the define and replace with a runtime option.
option(ANNOTATIONS "annotations" ${ANNOTATIONS_DEFAULT})

if (WIN32)
  # xref PR 192750 - runregression uses this to avoid over-ssh pdb issues
  option(GENERATE_PDBS "generate Windows debug information" ON)
  # instead of config files use registry like in the old days
  # (i#85/PR 212034 and i#265/PR 486139 switched DR to config files)
  option(PARAMS_IN_REGISTRY "parameters from registry instead of config files")
endif (WIN32)

# for users
option(DISABLE_WARNINGS "disable warnings")

option(SET_PREFERRED_BASE "set a preferred library base address")
if (WIN32 AND DEBUG)
  # apparently no numeric type so we use STRING
  set(preferred_base "0x15000000" CACHE STRING "Preferred library base address")
elseif (APPLE AND X64)
  # Set to higher than _PAGEZERO which is [0..0x1'00000000).
  set(preferred_base "0x171000000" CACHE STRING "Preferred library base address")
else ()
  set(preferred_base "0x71000000" CACHE STRING "Preferred library base address")
endif ()

# for x64: PR 253624: we need our library to be next to our heap
# for win32: not PIC so need a base
# we studied existing exe + dll bases and tried to pick non-conflicting addresses
# for 32-bit Linux: we put dynamorio at top of address space so little chance
# of conflict with app for early injection.
# there should be no noticeable perf hit from this (b/c no relocations for DR lib)
# so it's ok to be on for non-early injection
set(SET_PREFERRED_BASE 1)

if (VMKERNEL)
  # we end up with the default executable base (0x08*) so go back to 0 base
  # (else we fail to load on esx)
  set(SET_PREFERRED_BASE 1)
  set(preferred_base "0x00000000")
endif (VMKERNEL)

# we do not support most users choosing these
mark_as_advanced(
  PROGRAM_SHEPHERDING
  PROCESS_CONTROL
  GBOP
  VMKERNEL
  KSTATS
  CALLPROF
  PROFILE
  SET_PREFERRED_BASE
  preferred_base
  GENERATE_PDBS
  DRSTATS_DEMO
  )

# we clear both base and build-type cflags and then use just base.
# XXX: could use CMAKE_USER_MAKE_RULES_OVERRIDE to set these but not any cleaner
#
# i#919: Ninja generator does not leave CMAKE_BUILD_TYPE blank, and that affects
# the exported target files, so we set it here.  This shouldn't hurt containing
# tools as they should set our DEBUG to match the incoming CMAKE_BUILD_TYPE.
if ("${CMAKE_BUILD_TYPE}" STREQUAL "")
  if (DEBUG)
    set(CMAKE_BUILD_TYPE "Debug")
  else (DEBUG)
    set(CMAKE_BUILD_TYPE "RelWithDebInfo")
  endif (DEBUG)
endif ()
string(TOUPPER "${CMAKE_BUILD_TYPE}" CMAKE_BUILD_TYPE_UPPER)
foreach (config ${CMAKE_BUILD_TYPE} ${CMAKE_CONFIGURATION_TYPES})
  string(TOUPPER "${config}" config_upper)
  foreach (var
      CMAKE_C_FLAGS_${config_upper};
      CMAKE_CXX_FLAGS_${config_upper};
      CMAKE_EXE_LINKER_FLAGS_${config_upper};
      CMAKE_MODULE_LINKER_FLAGS_${config_upper};
      CMAKE_SHARED_LINKER_FLAGS_${config_upper})
    set(${var} " ") # if "" defaults come back
  endforeach ()
endforeach ()
foreach (var CMAKE_C_FLAGS;CMAKE_CXX_FLAGS)
  set(${var} " ") # if "" defaults come back
endforeach ()

if (APPLE)
  # Enable @rpath for all shared library install names.
  set(CMAKE_MACOSX_RPATH 1)
endif ()

##################################################
# resources when packaging

# We use a monotonically increasing integer that's larger than any bugfix
# release version as the patchlevel ver# to distinguish
# mid-release builds.
# We used to use the svn revision (i#83) and we leave that code in place
# (for now at least) for anyone building an old checkout.
# For git, we follow i#1565 and use a date.
set(VERSION_NUMBER_PATCHLEVEL 0)
# for now using a hack of running svn or git to get the ver#,
# rather than having it stored in the sources and auto-updated
if (EXISTS "${PROJECT_SOURCE_DIR}/.svn")
  find_program(SVN svn DOC "subversion client")
  if (SVN)
    execute_process(COMMAND ${SVN} info
      WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
      RESULT_VARIABLE svn_result
      ERROR_VARIABLE svn_err
      OUTPUT_VARIABLE svn_out)
    if (svn_result OR svn_err)
      message(FATAL_ERROR "*** ${SVN} info failed: ***\n${svn_result} ${svn_err}")
    endif (svn_result OR svn_err)
    string(REGEX MATCH "Revision: [0-9]+" svn_ver "${svn_out}")
    string(REGEX REPLACE "Revision: " "" svn_ver "${svn_ver}")
    if ("${svn_ver}" STREQUAL "")
      # could be another language (xref i#401) so look just for number
      # which will rule out URL and UUID fields
      string(REGEX MATCH ": [0-9]+\r?\n" svn_ver "${svn_out}")
      string(REGEX REPLACE ": ([0-9]+)\r?\n" "\\1" svn_ver "${svn_ver}")
    endif ("${svn_ver}" STREQUAL "")
    if ("${svn_ver}" STREQUAL "")
      # indicate we failed w/ impossible ver#
      message(STATUS "WARNING: Unable to obtain actual revision number")
      set(VERSION_NUMBER_PATCHLEVEL "42")
    else ("${svn_ver}" STREQUAL "")
      set(VERSION_NUMBER_PATCHLEVEL "${svn_ver}")
    endif ("${svn_ver}" STREQUAL "")
  endif (SVN)
else (EXISTS "${PROJECT_SOURCE_DIR}/.svn")
  if (EXISTS "${PROJECT_SOURCE_DIR}/.git")
    find_program(GIT git DOC "git client")
    if (GIT)
      # We want the committer date (not author date) (xref i#1565).  We request UNIX
      # timestamp format and then divide down to days to get a small enough number
      # for the Windows resource limits.
      execute_process(COMMAND ${GIT} log -n 1 --format=%ct
        WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
        RESULT_VARIABLE git_result
        ERROR_VARIABLE git_err
        OUTPUT_VARIABLE git_out)
      if (git_result OR git_err)
        message("*** ${GIT} log failed: ***\n${git_err}")
      else (git_result OR git_err)
        math(EXPR daycount "${git_out} / (60*60*24)")
      endif (git_result OR git_err)
    endif (GIT)
    if (NOT daycount)
      # XXX i#1565: to support building when not in a git repo (e.g., from a source
      # tarball) we use date to get current time for timestamp.
      # This is not ideal as it confuses the build timestamp with the commit
      # timestamp.  We should add support for a local file holding the version.
      find_program(DATE date DOC "system date")
      if (DATE)
        execute_process(COMMAND ${DATE} +%s
          RESULT_VARIABLE date_result
          ERROR_VARIABLE date_err
          OUTPUT_VARIABLE date_out)
        if (date_result OR date_err)
          message("*** ${DATE} failed: ***\n${date_err}")
        else (date_result OR date_err)
          math(EXPR daycount "${date_out} / (60*60*24)")
        endif (date_result OR date_err)
      endif (DATE)
    endif (NOT daycount)
    if (NOT daycount)
      # set a much further date in the future to avoid confusing
      # this fake date with the real date from git log
      set(daycount 33333)
    endif (NOT daycount)
    set(VERSION_NUMBER_PATCHLEVEL "${daycount}")
  endif (EXISTS "${PROJECT_SOURCE_DIR}/.git")
endif (EXISTS "${PROJECT_SOURCE_DIR}/.svn")

# N.B.: When updating this, update all the default versions in ci-package.yml
# and ci-docs.yml.  We should find a way to share (xref i#1565).
set(VERSION_NUMBER_DEFAULT "10.0.${VERSION_NUMBER_PATCHLEVEL}")
# do not store the default VERSION_NUMBER in the cache to prevent a stale one
# from preventing future version updates in a pre-existing build dir
set(VERSION_NUMBER "" CACHE STRING "Version number: leave empty for default")
if ("${VERSION_NUMBER}" STREQUAL "")
  set(VERSION_NUMBER ${VERSION_NUMBER_DEFAULT})
endif()
string(REGEX REPLACE "\\." "," VERSION_COMMA_DELIMITED "${VERSION_NUMBER}")
message(STATUS "Version number: ${VERSION_NUMBER}")

set(BUILD_NUMBER "0" CACHE STRING "Build number (must be <64K)")
set(UNIQUE_BUILD_NUMBER "0" CACHE STRING "Unique build number")
set(CUSTOM_PRODUCT_NAME "" CACHE STRING "Custom product name")
set(PACKAGE_PLATFORM "" CACHE STRING "Platform for package name (should have trailing -)")
set(PACKAGE_SUBSYS "" CACHE STRING
  "Platform for sub-system name (should have leading -: e.g., -EABIHF")
mark_as_advanced(
  VERSION_NUMBER
  VERSION_COMMA_DELIMITED
  BUILD_NUMBER
  UNIQUE_BUILD_NUMBER
  CUSTOM_PRODUCT_NAME
  PACKAGE_PLATFORM
  PACKAGE_SUBSYS
  )
# This is hardcoded in globals_shared.h: going to leave it that way, but
# adding indirection within cmake files
set(PRODUCT_NAME "DynamoRIO")

if (install_override)
  set(CMAKE_INSTALL_PREFIX "${PROJECT_SOURCE_DIR}/exports"
    CACHE PATH "install path" FORCE)
endif (install_override)
# for historical reasons we have a separate mirror var
set(INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}")

###########################################################################
# toolchain

# Figure out avx flags before asm setup, which requires it.
set(proc_supports_avx OFF)
set(proc_supports_avx2 OFF)
set(proc_supports_avx512 OFF)
if (X86 AND UNIX)
  set(CFLAGS_AVX "-mavx")
  # BMI2 instructions are typically also supported on AVX2 processors.
  set(CFLAGS_AVX2 "-mavx2 -mbmi2")
  set(CFLAGS_AVX512 "-mavx512f")
  check_avx_processor_and_compiler_support(proc_supports_avx)
  check_avx2_processor_and_compiler_support(proc_supports_avx2)
  check_avx512_processor_and_compiler_support(proc_supports_avx512)
endif ()

# Ensure that _AMD64_ or _X86_ are defined on Microsoft Windows, as otherwise
# um/winnt.h provided since Windows 10.0.22000 will error.
if (NOT UNIX)
  if (X64)
    add_definitions(-D_AMD64_)
  else (X64)
    add_definitions(-D_X86_)
  endif (X64)
endif (NOT UNIX)
if (UNIX)
  # Ensure we can export dr_stat_syscall() from drlibc and get everyone to agree
  # that struct stat64 is the stat struct.
  add_definitions(-D_LARGEFILE64_SOURCE)
endif ()

# Set up assembly support and CMAKE_CPP.
# Note that in cmake < 2.6.4, I had to fix a bug in
# /usr/share/cmake/Modules/CMakeASMInformation.cmake
# where @VAR@ expansion was used, which only works for configure_file()
#   now fixed in CMake/Modules/CMakeASMInformation.cmake:1.5
# See top of top-level CMakeLists.txt for our workaround.
if (NOT UNIX)
  get_filename_component(cl_path ${CMAKE_C_COMPILER} PATH)
endif (NOT UNIX)

set(cpp2asm_newline_script_path "${PROJECT_SOURCE_DIR}/make/CMake_asm.cmake")
include(make/cpp2asm_support.cmake)
if (UNIX)
  # We require gas >= 2.18.50 for --32, --64, and the new -msyntax=intel, etc.
  if (NOT CMAKE_ASM_SUPPORTS_INTEL_SYNTAX)
    message(FATAL_ERROR
      "${CMAKE_ASM_COMPILER} is too old and does not support -msyntax=intel")
  endif (NOT CMAKE_ASM_SUPPORTS_INTEL_SYNTAX)
endif (UNIX)

if (UNIX)
  identify_clang(CMAKE_COMPILER_IS_CLANG)
  if (CMAKE_COMPILER_IS_CLANG)
    set(CLANG 1)
  endif (CMAKE_COMPILER_IS_CLANG)

  if (CYGWIN)
    message(FATAL_ERROR "building using gcc within cygwin is not supported")
  endif (CYGWIN)
  if (NOT CMAKE_COMPILER_IS_GNUCC)
    # we use gcc extensions
    message(FATAL_ERROR "gcc is required to build")
  endif (NOT CMAKE_COMPILER_IS_GNUCC)

  check_if_linker_is_gnu_gold(LINKER_IS_GNU_GOLD)

  # FIXME i#2949: static 32-bit release-build linking with gcc 7.3.1 fails when
  # static C++ clients like drmemtrace or drmemtrace_raw2trace are linked in.
  # For now we disable those builds.
  if (UNIX AND NOT X64 AND NOT DEBUG AND NOT CLANG AND
      "${CMAKE_C_COMPILER_VERSION}" VERSION_GREATER "7.3")
    message(STATUS "gcc 7.3+ non-debug 32-bit detected: disabling static C++ client "
      "tests to work around i#2949")
    set(DISABLE_FOR_BUG_2949 ON)
  else ()
    set(DISABLE_FOR_BUG_2949 OFF)
  endif ()

else (UNIX)

  if (NOT ${COMPILER_BASE_NAME} STREQUAL "cl")
    # we use cl pragmas and intrinsics
    message(FATAL_ERROR "cl (Microsoft C++ compiler) is required to build")
  endif (NOT ${COMPILER_BASE_NAME} STREQUAL "cl")

  # cmake has CMAKE_RC_COMPILER, but no message compiler
  if ("${CMAKE_GENERATOR}" MATCHES "Visual Studio")
    # First checks for the Windows SDK through Windows Kits.
    get_filename_component(kits_dir "[HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Windows Kits\\Installed Roots;KitsRoot10]" REALPATH)

    if (EXISTS ${kits_dir})
      if (X64)
        set(sdk_bindir "${kits_dir}/bin/${CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION}/x64")
      else (X64)
        set(sdk_bindir "${kits_dir}/bin/${CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION}/x86")
      endif (X64)
    else (EXISTS ${kits_dir})
      # this path is only present for 2008+, but we currently require PATH to
      # be set up anyway
      get_filename_component(sdk_dir "[HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Microsoft SDKs\\Windows;CurrentInstallFolder]" REALPATH)
      if (X64)
        set(sdk_bindir "${sdk_dir}/bin/x64")
      else (X64)
        set(sdk_bindir "${sdk_dir}/bin")
      endif (X64)
    endif (EXISTS ${kits_dir})
  endif ()
  if (NOT ("${CMAKE_GENERATOR}" MATCHES "MSYS Makefiles"))
    # For MinGW (MSYS Makefiles) we do not have a message compiler.
    # We build without it (which means we can't use the Windows event log).
    find_program(CMAKE_MC_COMPILER mc.exe HINTS "${sdk_bindir}"
      DOC "path to message compiler")
    if (NOT CMAKE_MC_COMPILER)
      message(FATAL_ERROR "message compiler not found: required to build")
    endif (NOT CMAKE_MC_COMPILER)
    message(STATUS "Found message compiler: ${CMAKE_MC_COMPILER}")
    mark_as_advanced(CMAKE_MC_COMPILER)
  endif ()
endif (UNIX)

find_package(Perl)
if (NOT PERL_FOUND)
  message(FATAL_ERROR "perl is required to build")
endif (NOT PERL_FOUND)

if (UNIX) # unlikely to be an issue on Windows
  # Check for uint, etc. typedef conflicts like on rhel3 (i#18)
  # and set DR_DO_NOT_DEFINE_*
  # Note that for later gcc uint and ushort seem to be "soft typedefs":
  # defined, but overridable: ?!?
  include(CheckTypeSize)
  CHECK_TYPE_SIZE(uint DR_DO_NOT_DEFINE_uint)
  CHECK_TYPE_SIZE(ushort DR_DO_NOT_DEFINE_ushort)
  CHECK_TYPE_SIZE(bool DR_DO_NOT_DEFINE_bool)
  CHECK_TYPE_SIZE(byte DR_DO_NOT_DEFINE_byte)
  CHECK_TYPE_SIZE(sbyte DR_DO_NOT_DEFINE_sbyte)
  CHECK_TYPE_SIZE(uint32 DR_DO_NOT_DEFINE_uint32)
  CHECK_TYPE_SIZE(uint64 DR_DO_NOT_DEFINE_uint64)
  CHECK_TYPE_SIZE(int32 DR_DO_NOT_DEFINE_int32)
  CHECK_TYPE_SIZE(int64 DR_DO_NOT_DEFINE_int64)
  # we could do CHECK_SYMBOL_EXISTS for MAX and MIN but they're not
  # in standard headers so up to user to define if an issue
  if (DR_DO_NOT_DEFINE_bool)
    # i#488: ensure bool compatibility with C++
    if (NOT ${DR_DO_NOT_DEFINE_bool} EQUAL 1)
      message(FATAL_ERROR "incompatible pre-defined \"bool\" type is larger than 1 byte")
    endif (NOT ${DR_DO_NOT_DEFINE_bool} EQUAL 1)
  endif (DR_DO_NOT_DEFINE_bool)
  CHECK_TYPE_SIZE(_Bool DR__Bool_EXISTS)
  if (DR__Bool_EXISTS)
    if (NOT ${DR__Bool_EXISTS} EQUAL 1)
      message(FATAL_ERROR "incompatible \"_Bool\" type is larger than 1 byte")
    endif (NOT ${DR__Bool_EXISTS} EQUAL 1)
  endif (DR__Bool_EXISTS)
endif (UNIX)

###########################################################################
# basic build rules and flags

# i#1781: cmake 2.8.12+ fails to create static lib pdb by default
macro(add_static_lib_debug_info target dest_dir)
  if (WIN32)
    if ("${CMAKE_VERSION}" VERSION_EQUAL "3.1" OR
        "${CMAKE_VERSION}" VERSION_GREATER "3.1")
      append_property_string(TARGET ${target}
        COMPILE_PDB_NAME${location_suffix} "${target}"
        COMPILE_PDB_OUTPUT_DIRECTORY{location_suffix} "${dest_dir}")
    else ()
      # We just don't support it for < 3.1
    endif ()
  endif ()
endmacro()

# compiler flags
# BASE_CFLAGS applies to both C and C++.
# BASE_CONLY_FLAGS applies to just C.
# BASE_CXXONLY_FLAGS applies to just C++.
set(BASE_CFLAGS "")
set(BASE_CONLY_FLAGS "")
set(BASE_CXXONLY_FLAGS "")
CHECK_CXX_COMPILER_FLAG("-std=c++17" cxx17_available)
if (UNIX)
  set(BASE_CXXONLY_FLAGS "${BASE_CXXONLY_FLAGS} -std=c++11")
  # -std=c99 doesn't quite work
  # FIXME case 191480: we used to pass -pedantic just to cpp;
  # now w/ no separate cpp step we should eliminate the
  # warnings and pass -pedantic to main gcc
  set(BASE_CONLY_FLAGS "${BASE_CONLY_FLAGS} -std=gnu99")
  # disable strict aliasing opt in gcc 3.3.3 -- gives warnings and makes bad assumptions
  set(BASE_CFLAGS "${BASE_CFLAGS} -fno-strict-aliasing")
  # Ensure DR is interoperable with other toolchains: do not assume char is signed
  # (see i#1034 where this caused problems in the past).
  set(BASE_CFLAGS "${BASE_CFLAGS} -funsigned-char")
  # Ubuntu defaults to -fstack-protector these days, which depends on app TLS.
  CHECK_C_COMPILER_FLAG("-fno-stack-protector" no_stack_protector_avail)
  if (no_stack_protector_avail)
    set(BASE_CFLAGS "${BASE_CFLAGS} -fno-stack-protector")
  endif (no_stack_protector_avail)
  # Prefer the new 'override' keyword.
  CHECK_CXX_COMPILER_FLAG("-Wsuggest-override" suggest_override_avail)
  if (suggest_override_avail)
    set(BASE_CXXONLY_FLAGS "${BASE_CXXONLY_FLAGS} -Wsuggest-override")
  endif ()
  # Try to support users setting -m32 in CMAKE_C_FLAGS rather than the
  # CFLAGS env var.
  if (X86)
    if (X64)
      set(BASE_CFLAGS "-m64 ${BASE_CFLAGS}")
      set(LD_FLAGS "-melf_x86_64")
    else (X64)
      set(BASE_CFLAGS "-m32 ${BASE_CFLAGS}")
      set(LD_FLAGS "-melf_i386")
    endif (X64)
  elseif (ARM)
    # i#1551: add necessary flags for ARM build.
    if (X64)
    else (X64)
      # On newer gcc versions such as 11.2 we need an explicit +fp to denote our
      # gnueablihf hardware-fp-capabilities target.
      CHECK_C_COMPILER_FLAG("-march=armv7-a+fp" armv7_fp_available)
      if (armv7_fp_available)
        set(BASE_CFLAGS "-mthumb -march=armv7-a+fp ${BASE_CFLAGS}")
      else ()
        set(BASE_CFLAGS "-mthumb -march=armv7-a ${BASE_CFLAGS}")
      endif ()
      set(LD_FLAGS "-marmelf_linux_eabi")
      if (ANDROID OR CMAKE_C_LIBRARY_ARCHITECTURE MATCHES "gnueabi$")
        # We use eabihf by default for ARM build.
        # According to https://developer.android.com/ndk/guides/abis.html#v7a,
        # we need extra flags to use eabihf for Android build, i.e.,:
        # TARGET_CFLAGS += -mhard-float -D_NDK_MATH_NO_SOFTFP=1
        # TARGET_LDFLAGS += -Wl,--no-warn-mismatch -lm_hard
        # so we use softfp for Android build instead.
        # We also need to add softfp for the gnueabi target.
        set(BASE_CFLAGS "-mfloat-abi=softfp ${BASE_CFLAGS}")
      endif ()
    endif (X64)
  endif ()
  if (APPLE AND CMAKE_COMPILER_IS_CLANG)
    set(BASE_CFLAGS "${BASE_CFLAGS} -mmacosx-version-min=${OLDEST_OSX_SUPPPORTED}")
  endif ()
  if (APPLE)
    set(ld_entry_flag "-e")
  else ()
    set(ld_entry_flag "--entry")
  endif ()
  # there's no cmake warning control so we hardcode it
  set(WARN "-Wall -Werror -Wwrite-strings -Wvla")
  if (NOT CMAKE_COMPILER_IS_CLANG)
    # Old gcc's ignore unknown -W flags, but -Wall -Werror causes clang to
    # complain that it doesn't recognize it.
    # Actually this is not true: gcc 4.1.2 aborts on unknown -W so we check
    CHECK_C_COMPILER_FLAG("-Wno-unused-but-set-variable" nounused_avail)
    if (nounused_avail)
      set(WARN "${WARN} -Wno-unused-but-set-variable")
    endif (nounused_avail)
    # XXX i#3792: DynamoRIO manages '\0' termination and error states itself in
    # too many places. In order to activate this warning, this code needs to get
    # re-factored for no good reason.
    CHECK_C_COMPILER_FLAG("-Wstringop-truncation" stringop_truncation_avail)
    if (stringop_truncation_avail)
      set(WARN "${WARN} -Wno-stringop-truncation")
    endif (stringop_truncation_avail)
    # XXX i#3792: see comment above.
    CHECK_C_COMPILER_FLAG("-Wformat-truncation" format_truncation_avail)
    if (format_truncation_avail)
      set(WARN "${WARN} -Wno-format-truncation")
    endif (format_truncation_avail)
    # XXX i#3792: see comment above.
    CHECK_C_COMPILER_FLAG("-Wstringop-overflow" stringop_overflow_avail)
    if (stringop_overflow_avail)
      set(WARN "${WARN} -Wno-stringop-overflow")
    endif (stringop_overflow_avail)
    CHECK_C_COMPILER_FLAG("-Wtype-limits" HAVE_TYPELIMITS_CONTROL)
  else (NOT CMAKE_COMPILER_IS_CLANG)
    # clang turns off color when it's writing to a pipe, but the user may still
    # wish to force color if it eventually goes to a terminal.
    option(CLANG_COLOR_DIAGNOSTICS "force colored clang diagnostics" OFF)
    set(clang_args "")
    if (CLANG_COLOR_DIAGNOSTICS)
      set(clang_args "-fcolor-diagnostics ${clang_args}")
    endif (CLANG_COLOR_DIAGNOSTICS)
    set(BASE_CFLAGS "${clang_args} ${BASE_CFLAGS}")
  endif (NOT CMAKE_COMPILER_IS_CLANG)
  set(DBG "-g3")
  # gcc doesn't change its optimizations based on -g
  set(OPT "-O3 ${DBG}")
  if (PROFILE)
    set(OPT "${OPT} -fno-omit-frame-pointer")
  endif ()

  # Omit unwind tables in optimized mode.
  if (NOT DEBUG)
    set(BASE_CONLY_FLAGS "${BASE_CONLY_FLAGS} -fno-unwind-tables")
  endif ()

  if (NOT APPLE AND NOT ANDROID) # no .gnu.hash support on Android
    # Generate the .hash section in addition to .gnu.hash for every target, to
    # avoid SIGFPE when running our binaries on old systems:
    foreach (config ${CMAKE_BUILD_TYPE} ${CMAKE_CONFIGURATION_TYPES})
      string(TOUPPER "${config}" config_upper)
      foreach (var
          CMAKE_EXE_LINKER_FLAGS_${config_upper};
          CMAKE_MODULE_LINKER_FLAGS_${config_upper};
          CMAKE_SHARED_LINKER_FLAGS_${config_upper})
        set(${var} "-Wl,--hash-style=both")
      endforeach ()
    endforeach ()
  endif ()

  # XXX: core-specific: move to core/CMakeLists.txt?
  #
  # XXX: i#374: we used to use -O to avoid stack overflow in debug
  # build (makes a huge difference by having locals/temps share stack
  # slots) and had -fno-omit-frame-pointer to keep things more easily
  # debuggable (though gcc claims only omits when debuggable), but
  # even then many locals were optimized away, and utils.c's first
  # statsx.h expansion took over a minute to compile with some
  # versions of gcc, so removing the -O (can't repro the stack
  # overflows now anyway).  If we hit the overflows again, we should
  # use -O0 and optimize attributes on the few funcs that need higher
  # opt.
  set(DBG_OPT "-fno-omit-frame-pointer")
  # We disable strcmp intrinsic to avoid stack overflow in
  # set_dynamo_options(): case 7853.
  if (NOT CMAKE_COMPILER_IS_CLANG)
    set(DBG_OPT "${DBG_OPT} -fno-builtin-strcmp")
  endif ()
  set(LINK_EXTRA_FLAGS "")
else (UNIX)
  # FIXME: why isn't ${CMAKE_CL_NOLOGO} set?
  set(BASE_CFLAGS "${BASE_CFLAGS} /nologo")
  # build in parallel, always.
  # note that /MP is not officially supported on VS 2005 and others
  # have seen occasional problems: we'll risk it.  we could check for
  # "MSVC10 OR MSVC90".
  set(BASE_CFLAGS "${BASE_CFLAGS} /MP")
  # read-only string pooling
  set(BASE_CFLAGS "${BASE_CFLAGS} /GF")
  if (NOT CMAKE_C_COMPILER_VERSION VERSION_LESS 18.0)
    # i#1376: VS2013 requires /FS w/ multiple cl.exe in parallel (which Ninja
    # uses).  While /MP is supposed to enable it, it doesn't seem to.
    # This is recommended after /Fd but it seems to work here.
    set(BASE_CFLAGS "${BASE_CFLAGS} /FS")
  endif()
  # FIXME case 191729: we should try to enable this.
  # Currently we get "unresolved external symbol ___security_cookie"
  set(BASE_CFLAGS "${BASE_CFLAGS} /GS-")
  # VS2010 has /MD as the default (and does declspec(dllimport) for
  # libc routines) so make sure we explicitly request /MT
  if (DEBUG)
    set(BASE_CFLAGS "${BASE_CFLAGS} /MTd")
  else ()
    set(BASE_CFLAGS "${BASE_CFLAGS} /MT")
  endif ()
  set(WARN "/W4 /WX")
  # Default from cmake has /W3 so remove to avoid warning about overriding
  string(REGEX REPLACE "/W[0-9]" "" CMAKE_C_FLAGS "${CMAKE_C_FLAGS}")
  string(REGEX REPLACE "/W[0-9]" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
  # Shrink binaries and pdbs (/Gy should already be there)
  set(LINK_EXTRA_FLAGS "/opt:ref /opt:icf /pdbcompress")
  if (GENERATE_PDBS)
    set(DBG "/Zi")
    set(LINK_EXTRA_FLAGS "${LINK_EXTRA_FLAGS} /debug")
  else (GENERATE_PDBS)
    # xref PR 192750 - runregression uses this to avoid over-ssh pdb issues
    set(DBG "")
    set(LINK_EXTRA_FLAGS "")
    # Default from cmake in DEBUG and RELWITHDEBINFO has /debug
    string(REGEX REPLACE "/debug" "" CMAKE_EXE_LINKER_FLAGS
      "${CMAKE_EXE_LINKER_FLAGS}")
    string(REGEX REPLACE "/debug" "" CMAKE_MODULE_LINKER_FLAGS
      "${CMAKE_MODULE_LINKER_FLAGS}")
    string(REGEX REPLACE "/debug" "" CMAKE_SHARED_LINKER_FLAGS
      "${CMAKE_SHARED_LINKER_FLAGS}")
  endif (GENERATE_PDBS)

  # i#1424: target the oldest possible platform we can
  if (X64)
    set(os_target "5.02") # Win2003x64/WinXPx64
  else (X64)
    if (CMAKE_C_COMPILER_VERSION VERSION_LESS 17.0) # up to and including VS2010
      set(os_target "5.00") # Win2K
    else () # VS2012, VS2013
      set(os_target "5.01") # WinXP
    endif ()
  endif (X64)
  foreach (lflags CMAKE_EXE_LINKER_FLAGS
      CMAKE_MODULE_LINKER_FLAGS
      CMAKE_SHARED_LINKER_FLAGS)
    set(${lflags} "${${lflags}} /subsystem:console,${os_target}")
  endforeach()
  message(STATUS "Targeting subsystem ${os_target}")

  # w/ cl, using DBG here won't mess up the optimizations, debug line number info
  # gets a little messed up, but is better than nothing
  set(OPT "/O2 ${DBG}")
  # do not use /O2 on windows, it messes up debug info!
  # explicitly request /Od even though it's cl's default b/c VS has /O2 as default
  # and cl 14.00 hangs compiling core/fragment.c with /Od and DEBUG.
  set(DBG_OPT "/Od")
  if (NOT "${CMAKE_GENERATOR}" MATCHES "Visual Studio")
    # i#1821: cmake 3.3.1 (but not 2.8.12) somehow messes up this /nologo flag.
    # We just avoid it for VS generators as a workaround.
    if (CMAKE_RC_FLAGS)
      set(CMAKE_RC_FLAGS "${CMAKE_RC_FLAGS} /nologo")
    else ()
      set(CMAKE_RC_FLAGS "/nologo")
    endif ()
  endif ()
endif (UNIX)

if (UNIX)
  # Check for -fvisibility
  # PR 262458: for gcc >= 3.4 we can use -fvisibility instead of a
  # linker version script.  Note that we're still using a linker
  # script to set our preferred base (PR 253624) and will need to
  # for section ordering as well (PR 208267) but those are separate
  # scripts.
  # Only export functions so marked via attributes
  # (For older gcc we use an ugly linker script and an auto-generated export list)
  # Using "internal" instead of "hidden" b/c we don't need any indirect
  # calls to our non-exported functions
  CHECK_C_COMPILER_FLAG("-fvisibility=internal" HAVE_FVISIBILITY_INTERNAL)
  CHECK_C_COMPILER_FLAG("-fvisibility=hidden" HAVE_FVISIBILITY_HIDDEN)

  # XXX: on macos w/ gcc 4.2.1, the internal flag is accepted with just a
  # warning, so the CHECK_C_COMPILER_FLAG passes above.  For now we just go to
  # hidden for macos instead of a manual test for the warning.
  if (HAVE_FVISIBILITY_INTERNAL AND NOT APPLE)
    set(VISIBILITY "internal")
  elseif (HAVE_FVISIBILITY_HIDDEN)
    set(VISIBILITY "hidden")
  else ()
    message("${CMAKE_C_COMPILER} missing flag -fvisibility, using linker "
            "script instead")
    set(VISIBILITY " ")
  endif ()

  if (VISIBILITY)
    set(BASE_CFLAGS "${BASE_CFLAGS} -fvisibility=${VISIBILITY}")
    set(HAVE_FVISIBILITY ON)
  endif (VISIBILITY)

  # Check for -fno-sanitize=null
  CHECK_C_COMPILER_FLAG("-fno-sanitize=null" HAVE_FNOSANITIZE_NULL)

  if (APPLE OR LINKER_IS_GNU_GOLD)
    # XXX: for macos, currently assuming using xcode toolchain.
    set(ld_script_option "")
  else ()
    # Better to use -dT when passing linker options through gcc, but ld
    # prior to 2.18 only supports -T
    # FIXME: should we duplicate this in DynamoRIOConfig.cmake?
    execute_process(COMMAND
      ${CMAKE_LINKER} --help
      RESULT_VARIABLE ld_result
      ERROR_VARIABLE ld_error
      OUTPUT_VARIABLE ld_out)
    if (ld_result OR ld_error)
      message(FATAL_ERROR "*** ${CMAKE_LINKER} failed: ***\n${ld_error}")
    endif (ld_result OR ld_error)
    string(REGEX MATCH "dT" flag_present "${ld_out}")
    if (NOT flag_present)
      message("${CMAKE_LINKER} missing flag -dT, using -T instead")
      set(ld_script_option "-T")
    else (NOT flag_present)
      set(ld_script_option "-dT")
    endif (NOT flag_present)
  endif ()

  # In the past, there have been tools on Linux that don't know how to follow
  # .gnu_debuglink.  While we aren't aware of any major tools with this bug
  # today, it's useful to be able to turn this off while experimenting with new
  # tools.
  option(SPLIT_SYMBOLS "whether to split debug symbols from binaries" ON)
  mark_as_advanced(SPLIT_SYMBOLS)

  # We want separate .debug files for all shared libraries
  if (SPLIT_SYMBOLS AND NOT DEFINED CMAKE_OBJCOPY)
    find_package(BinUtils)
  endif ()
  if (SPLIT_SYMBOLS AND EXISTS ${CMAKE_OBJCOPY} AND EXISTS ${CMAKE_STRIP})
    # Check for --only-keep-debug support: added ~2.15
    execute_process(COMMAND
      ${CMAKE_OBJCOPY} --help
      RESULT_VARIABLE objcopy_result
      ERROR_QUIET
      OUTPUT_VARIABLE objcopy_out)
    if (objcopy_result)
      message(FATAL_ERROR "*** ${CMAKE_OBJCOPY} failed to run ***\n")
    endif (objcopy_result)
    set(strip_local "-x")  # Strip local (non-exported) symbols from .symtab.
    if (PROFILE)
      # Keep .symtab for tools (objdump) that don't always handle split debug
      # info gracefully.
      set(strip_local "")
    endif ()
    string(REGEX MATCH "only-keep-debug" flag_present "${objcopy_out}")
    if (NOT flag_present)
      message("${CMAKE_OBJCOPY} missing flag --only-keep-debug: leaving debug info in .so files")
      if (APPLE)
        # Incredibly, for both clang and g++, while a single compile-and-link
        # invocation will create an executable.dSYM/ dir with debug info,
        # with separate compilation the final link does NOT create the
        # dSYM dir.
        # The "dsymutil" program will create the dSYM dir for us.
        # Strangely it takes in the executable and not the object
        # files even though it's the latter that contain the debug info.
        # Thus it will only work if the object files are still sitting around.
        find_program(DSYMUTIL_PROGRAM dsymutil)
        if (DSYMUTIL_PROGRAM)
          set(CMAKE_C_LINK_EXECUTABLE
            "${CMAKE_C_LINK_EXECUTABLE}"
            "${DSYMUTIL_PROGRAM} <TARGET>")
          set(CMAKE_C_CREATE_SHARED_LIBRARY
            "${CMAKE_C_CREATE_SHARED_LIBRARY}"
            "${DSYMUTIL_PROGRAM} <TARGET>")
          set(CMAKE_CXX_LINK_EXECUTABLE
            "${CMAKE_CXX_LINK_EXECUTABLE}"
            "${DSYMUTIL_PROGRAM} <TARGET>")
          set(CMAKE_CXX_CREATE_SHARED_LIBRARY
            "${CMAKE_CXX_CREATE_SHARED_LIBRARY}"
            "${DSYMUTIL_PROGRAM} <TARGET>")
        endif ()
      endif ()
      if (ANDROID)
        # For code simplicity (to avoid -fPIE rules here) we just don't support this
        message(FATAL_ERROR "${CMAKE_OBJCOPY} required flag --only-keep-debug missing")
      endif ()
    else (NOT flag_present)
      if (ANDROID)
        # Android requires PIE
        set(exe_link_extra "-fPIE -pie")
      else ()
        set(exe_link_extra "")
      endif ()
      set(CMAKE_C_CREATE_SHARED_LIBRARY
        # standard rule
        "<CMAKE_C_COMPILER> <CMAKE_SHARED_LIBRARY_C_FLAGS> <LANGUAGE_COMPILE_FLAGS> <LINK_FLAGS> <CMAKE_SHARED_LIBRARY_CREATE_C_FLAGS> <CMAKE_SHARED_LIBRARY_SONAME_C_FLAG><TARGET_SONAME> -o <TARGET> <OBJECTS> <LINK_LIBRARIES>"
        # now create a .debug copy
        "${CMAKE_OBJCOPY} --only-keep-debug <TARGET> <TARGET>.debug"
        # link original to point at .debug copy
        # directory components are removed, so "../lib/" is fine
        "${CMAKE_OBJCOPY} --add-gnu-debuglink=<TARGET>.debug <TARGET>"
        # We can't strip everything since a client's _USES_DR_VERSION_ will be
        # removed, so we only strip debug (-g) and local (-x):
        # i#572: We used to pass -p here, but that floors the mtime.
        "${CMAKE_STRIP} -g ${strip_local} <TARGET>"
        )
      SET(CMAKE_C_LINK_EXECUTABLE
        "<CMAKE_C_COMPILER> <FLAGS> <CMAKE_C_LINK_FLAGS> <LINK_FLAGS> ${exe_link_extra} <OBJECTS>  -o <TARGET> <LINK_LIBRARIES>"
        "${CMAKE_OBJCOPY} --only-keep-debug <TARGET> <TARGET>.debug"
        "${CMAKE_OBJCOPY} --add-gnu-debuglink=<TARGET>.debug <TARGET>"
        "${CMAKE_STRIP} -g ${strip_local} <TARGET>"
        )
      set(CMAKE_CXX_CREATE_SHARED_LIBRARY
        # standard rule
        "<CMAKE_CXX_COMPILER> <CMAKE_SHARED_LIBRARY_CXX_FLAGS> <LANGUAGE_COMPILE_FLAGS> <LINK_FLAGS> <CMAKE_SHARED_LIBRARY_CREATE_CXX_FLAGS> <CMAKE_SHARED_LIBRARY_SONAME_CXX_FLAG><TARGET_SONAME> -o <TARGET> <OBJECTS> <LINK_LIBRARIES>"
        # now create a .debug copy
        "${CMAKE_OBJCOPY} --only-keep-debug <TARGET> <TARGET>.debug"
        # link original to point at .debug copy
        # directory components are removed, so "../lib/" is fine
        "${CMAKE_OBJCOPY} --add-gnu-debuglink=<TARGET>.debug <TARGET>"
        # We can't strip everything since a client's _USES_DR_VERSION_ will be
        # removed, so we only strip debug (-g) and local (-x):
        # i#572: We used to pass -p here, but that floors the mtime.
        "${CMAKE_STRIP} -g ${strip_local} <TARGET>"
        )
      SET(CMAKE_CXX_LINK_EXECUTABLE
        "<CMAKE_CXX_COMPILER> <FLAGS> <CMAKE_CXX_LINK_FLAGS> <LINK_FLAGS> ${exe_link_extra} <OBJECTS>  -o <TARGET> <LINK_LIBRARIES>"
        "${CMAKE_OBJCOPY} --only-keep-debug <TARGET> <TARGET>.debug"
        "${CMAKE_OBJCOPY} --add-gnu-debuglink=<TARGET>.debug <TARGET>"
        "${CMAKE_STRIP} -g ${strip_local} <TARGET>"
        )
    endif (NOT flag_present)
  else (SPLIT_SYMBOLS AND EXISTS ${CMAKE_OBJCOPY} AND EXISTS ${CMAKE_STRIP})
    if (ANDROID)
      # For code simplicity (to avoid -fPIE rules here) we just don't support this
      message(FATAL_ERROR "SPLIT_SYMBOLS required for Android")
    endif ()
  endif (SPLIT_SYMBOLS AND EXISTS ${CMAKE_OBJCOPY} AND EXISTS ${CMAKE_STRIP})

endif (UNIX)

# Should we be using fewer of these and using cmake's Debug vs Release?
#   Release => -O3 -NDEBUG
# Right now we only support gcc and cl but could change in future
if (DEBUG)
  set(CMAKE_C_FLAGS "${BASE_CFLAGS} ${BASE_CONLY_FLAGS} ${DBG} ${DBG_OPT} $ENV{CFLAGS}")
  set(CMAKE_CXX_FLAGS "${BASE_CFLAGS} ${BASE_CXXONLY_FLAGS} ${DBG} ${DBG_OPT} $ENV{CXXFLAGS}")
else (DEBUG)
  if (CALLPROF)
    # no opts -- we need to avoid messing up call frame walking
    # FIXME: just disable frame ptr elim opt: but /Oy- ran into some issues
    set (OPT "")
  endif (CALLPROF)
  set(CMAKE_C_FLAGS "${BASE_CFLAGS} ${BASE_CONLY_FLAGS} ${OPT} $ENV{CFLAGS}")
  set(CMAKE_CXX_FLAGS "${BASE_CFLAGS} ${BASE_CXXONLY_FLAGS} ${OPT} $ENV{CXXFLAGS}")
endif (DEBUG)
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${LINK_EXTRA_FLAGS}")
set(CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} ${LINK_EXTRA_FLAGS}")
set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} ${LINK_EXTRA_FLAGS}")
if (NOT DISABLE_WARNINGS)
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${WARN}")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${WARN}")
endif (NOT DISABLE_WARNINGS)

if (LINUX)
  CHECK_INCLUDE_FILE("linux/rseq.h" HAVE_RSEQ)
  CHECK_INCLUDE_FILE("libunwind.h" HAVE_LIBUNWIND_H)
else ()
  set(HAVE_RSEQ OFF)
  set(HAVE_LIBUNWIND_H OFF)
endif ()

# Currently only AArch64 targets supported for half-precision FP.
if (DR_HOST_AARCH64)
  set(HAVE_HALF_FLOAT ON)
else ()
  set(HAVE_HALF_FLOAT OFF)
endif ()

set(BUILD_PT_TRACER OFF)
set(BUILD_PT_POST_PROCESSOR OFF)
set(proc_supports_pt OFF)
# Right now we only build PT related libraries on Linux x86_64.
if (LINUX AND X86 AND X64)
  # We will always build the PT tracer on Linux X86_64 no matter whether the system
  # supports PT, because users may want to use the built binaries on a different system
  # that does not support PT itself.
  set(BUILD_PT_TRACER ON)
  # Check if the building machine supports Intel PT. This function only checks if
  # PT-related tests need to be built. PT-capable binaries can be built on any system.
  check_intel_pt_support(proc_supports_pt)
  # libdrpt2ir and drpt2trace depend on libipt. If the libipt submodule is not
  # initialized, cmake will report a warning here.
  if (EXISTS "${PROJECT_SOURCE_DIR}/third_party/libipt/.git" )
    set(BUILD_PT_POST_PROCESSOR ON)
  else ()
    message(STATUS "libipt submodule is not initialized. libdrpt2ir and drpt2trace will not be built.")
  endif ()
else (LINUX AND X86 AND X64)
  message(STATUS "PT related libraries only supported on Linux x86_64")
endif (LINUX AND X86 AND X64)

###########################################################################

# Issue 20: cross-arch execve depends on these being distinct and not
# subsets of each other (e.g., not "lib" and "lib64") and on the
# release package using these same names.
set(INSTALL_LIB_X64 lib64)
set(INSTALL_LIB_X86 lib32)
if (X64)
  set(INSTALL_LIB_BASE ${INSTALL_LIB_X64})
  set(INSTALL_BIN bin64)
else (X64)
  set(INSTALL_LIB_BASE ${INSTALL_LIB_X86})
  set(INSTALL_BIN bin32)
endif (X64)
if (DEBUG)
  set(INSTALL_LIB ${INSTALL_LIB_BASE}/debug)
else (DEBUG)
  set(INSTALL_LIB ${INSTALL_LIB_BASE}/release)
endif (DEBUG)
set(INSTALL_INCLUDE include)
set(INSTALL_DOCS docs)
set(INSTALL_DOCS_EMBED docs_embed)
# samples are installed via api/samples/ separate CMake project
set(INSTALL_CMAKE cmake)
set(BUILD_INCLUDE "${PROJECT_BINARY_DIR}/${INSTALL_INCLUDE}")
set(BUILD_CMAKE "${PROJECT_BINARY_DIR}/${INSTALL_CMAKE}")

# support DynamoRIO being included as a subdir inside a tool's project,
# where the tool will likely want to suppress DR's install rules.
if (NOT DEFINED DO_DR_INSTALL)
  set(DO_DR_INSTALL ON)
endif (NOT DEFINED DO_DR_INSTALL)
macro(DR_install)
  if (DO_DR_INSTALL)
    install(${ARGV})
  endif (DO_DR_INSTALL)
endmacro(DR_install)
# To support an including project exporting its own targets we need
# to export dynamorio, our extensions, and our exports file, but
# we want the includer to be able to stick the actual binaries
# in one place.  So we have both a bool DO_DR_INSTALL_TARGETS
# and a path DR_INSTALL_TARGETS_DEST:
if (DO_DR_INSTALL OR NOT DEFINED DO_DR_INSTALL_TARGETS)
  set(DO_DR_INSTALL_TARGETS ON)
endif ()
macro(DR_target_install)
  if (DO_DR_INSTALL_TARGETS)
    install(${ARGV})
  endif (DO_DR_INSTALL_TARGETS)
endmacro(DR_target_install)
macro(DR_target_install_dst dst)
  if (DO_DR_INSTALL_TARGETS)
    if (DEFINED DR_INSTALL_TARGETS_DEST)
      set(inst_dst ${DR_INSTALL_TARGETS_DEST})
    else ()
      set(inst_dst ${dst})
    endif ()
    set(export_flag "")
    foreach (n ${ARGN})
      if (n MATCHES EXPORT)
        set(export_flag "EXPORT_LINK_INTERFACE_LIBRARIES")
      endif ()
      break ()
    endforeach ()
    install(${ARGN} DESTINATION ${inst_dst} ${export_flag})
  endif (DO_DR_INSTALL_TARGETS)
endmacro(DR_target_install_dst)
function (install_exported_target target dest)
  if (DO_DR_INSTALL_TARGETS)
    DR_target_install_dst(${dest} TARGETS ${target} EXPORT ${exported_targets_name}
      ${ARGN})
  endif (DO_DR_INSTALL_TARGETS)
endfunction (install_exported_target)
function (install_target target dest)
  if (DO_DR_INSTALL_TARGETS)
    DR_target_install_dst(${dest} TARGETS ${target} ${ARGN})
  endif (DO_DR_INSTALL_TARGETS)
endfunction (install_target)

# mirror install path for easier use from build dir
# most libs go in base; DR and preload go in {debug,release} subdir
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/${INSTALL_LIB_BASE}")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}")
set(DR_LIBRARY_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/${INSTALL_LIB}")
set(DR_LIBRARY_BASE_DIRECTORY "${PROJECT_BINARY_DIR}/${INSTALL_LIB_BASE}")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/${INSTALL_BIN}")
# if drinjectlib is built before any exe target this dir won't exist
# and the "cmake -E copy" will fail (PR 549174)
file(MAKE_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")

function (set_per_config_ouput_to_match_single_config)
  if ("${CMAKE_GENERATOR}" MATCHES "Visual Studio")
    # we don't want to support the Debug and Release subdirs so
    # we rely on cmake 2.8.2's control over their names
    # (up above we require 2.8.2+ for VS generators)
    foreach (config ${CMAKE_CONFIGURATION_TYPES})
      string(TOUPPER "${config}" config_upper)
      set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${config_upper}
        "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}" PARENT_SCOPE)
      set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${config_upper}
        "${CMAKE_ARCHIVE_OUTPUT_DIRECTORY}" PARENT_SCOPE)
      set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_${config_upper}
        "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}" PARENT_SCOPE)
    endforeach ()
  endif ()
endfunction (set_per_config_ouput_to_match_single_config)

set_per_config_ouput_to_match_single_config()

# set var needed by configure.cmake.h
string(REGEX REPLACE
  "^([0-9]+)\\..*" "\\1" VERSION_NUMBER_MAJOR "${VERSION_NUMBER}")
string(REGEX REPLACE
  "^[0-9]+\\.([0-9]+)\\..*" "\\1" VERSION_NUMBER_MINOR "${VERSION_NUMBER}")
math(EXPR VERSION_NUMBER_INTEGER
  "${VERSION_NUMBER_MAJOR}*100 + ${VERSION_NUMBER_MINOR}")

# Every release since has had minor compat breakages: see api/docs/release.dox.
set(OLDEST_COMPATIBLE_VERSION_DEFAULT "1000")
set(OLDEST_COMPATIBLE_VERSION "" CACHE STRING
  "Oldest compatible version: leave empty for default")
if ("${OLDEST_COMPATIBLE_VERSION}" STREQUAL "")
  set(OLDEST_COMPATIBLE_VERSION ${OLDEST_COMPATIBLE_VERSION_DEFAULT})
endif()

# i#955: a Windows version of ELF DT_RPATH where a local file stores lib paths
set(DR_RPATH_SUFFIX "drpath")

# This does a copy-if-different, so it won't trigger recompilation of
# every single source file if no changes
configure_file(
  ${PROJECT_SOURCE_DIR}/make/configure.cmake.h
  ${PROJECT_BINARY_DIR}/configure.h
  )

# We want a define that's set to all the options.
# Something like cpp -dN but we need it on Windows and want only defines.
# Strategy: add -D<define> token after each #define, and then run
# cpp => left with just the -D<define> that are defined.
# If we append this to configure.h we'll trigger recompilation of
# everything, so we generate a separate header file.
file(READ ${PROJECT_BINARY_DIR}/configure.h configure_h)
string(REGEX REPLACE
  "(# *define *)([^_][^ \r\n]+)( *\r?\n)"
  "\\1\\2\\3-D\\2\n" configure_h_mod "${configure_h}")
set(defines_tmpfile ${PROJECT_BINARY_DIR}/configure_temp.h)
# we do NOT add ${defines_tmpfile} to set_directory_properties
# ADDITIONAL_MAKE_CLEAN_FILES b/c it's built at configure time:
# would be part of "make distclean" but cmake does not have that.
file(WRITE ${defines_tmpfile} "${configure_h_mod}")
# No way to get cmdline defines from cmake: but shouldn't be any, all
# in configure.h, except the -Ddynamorio_EXPORTS that cmake defines.
# Note: cpp w/ no 2nd filename prints to stdout (explicit "-" => pause at end)
execute_process(COMMAND
  ${CMAKE_CPP} ${CMAKE_CPP_FLAGS} -E ${CPP_NO_LINENUM} ${defines_tmpfile}
  RESULT_VARIABLE cpp_result
  ERROR_VARIABLE cpp_err
  OUTPUT_VARIABLE cpp_out
  )
if (WIN32)
  # cl prints out name of file: no way to quiet it
  get_filename_component(tmpfile_nm ${defines_tmpfile} NAME)
  string(REGEX REPLACE "${tmpfile_nm}[ \r\n]*" "" cpp_err "${cpp_err}")
  string(STRIP "${cpp_err}" cpp_err)
endif (WIN32)
if (cpp_result OR cpp_err)
  message(FATAL_ERROR "*** ${CMAKE_CPP} failed: ***\n${cpp_err}")
endif (cpp_result OR cpp_err)
string(REGEX MATCHALL
  "-D[^ \r\n]+" defines "${cpp_out}")
string(REGEX REPLACE
  ";"
  " " defines "${defines}")
set(defines_tofile
  "#ifndef _CONFIGDEFS_\n#define _CONFIGDEFS_ 1\n\n#define DYNAMORIO_DEFINES \"${defines}\"\n\n#endif /* _CONFIGDEFS_ */\n")
# Try to avoid triggering re-compilation if no changes
set(defines_file ${PROJECT_BINARY_DIR}/configure_defines.h)
if (EXISTS ${defines_file})
  file(READ ${defines_file} defines_fromfile)
  if (NOT "${defines_fromfile}" STREQUAL "${defines_tofile}")
    file(WRITE ${defines_file} "${defines_tofile}")
  endif (NOT "${defines_fromfile}" STREQUAL "${defines_tofile}")
else (EXISTS ${defines_file})
  file(WRITE ${defines_file} "${defines_tofile}")
endif (EXISTS ${defines_file})

include_directories(BEFORE
  ${PROJECT_BINARY_DIR} # for configure.h, configure_defines.h, events.h, and event_strings.h
  )

# Export targets for importing by clients.
# We include DynamoRIO and all Extensions in the same file for simplicity,
# since the Extensions depend on DynamoRIO and in some cases each other
# and so need to be locatable as a group.
# To support packaging both together we need different names.
# Debug vs release is automatically added as a suffix.
if (X64)
  set(exported_targets_name "DynamoRIOTarget64")
  set(exported_map_name "DynamoRIOMap64")
else (X64)
  set(exported_targets_name "DynamoRIOTarget32")
  set(exported_map_name "DynamoRIOMap32")
endif (X64)

# i#948: we need to map RelWithDebInfo and RelMinSize to Release so importing
# projects don't end up with Debug instead, yet Release for static libs
# set in DynamoRIOConfig.cmake.
# We do this with the MAP_IMPORTED_CONFIG_<CONFIG> target property in a
# special DynamoRIOMap file included by DynamoRIOConfig.cmake for build dirs, and
# directly in the DynamoRIOTarget file for install dirs (where we can append easily).
# For the Map file, we need to start empty:
file(WRITE ${PROJECT_BINARY_DIR}/cmake/${exported_map_name}.cmake "")
set(exported_targets_append "")
macro(DR_export_target target)
  export(TARGETS ${ARGV} APPEND FILE ${PROJECT_BINARY_DIR}/cmake/${exported_targets_name}.cmake
    EXPORT_LINK_INTERFACE_LIBRARIES)
  set(toadd "
SET_PROPERTY(TARGET ${ARGV} PROPERTY MAP_IMPORTED_CONFIG_RELEASE RelWithDebInfo)
SET_PROPERTY(TARGET ${ARGV} PROPERTY MAP_IMPORTED_CONFIG_RELMINSIZE RelWithDebInfo)
")
  if (NOT DEBUG)
    file(APPEND ${PROJECT_BINARY_DIR}/cmake/${exported_map_name}.cmake ${toadd})
  endif (NOT DEBUG)
  # We append to the installed file even in debug build so the ordering of
  # release vs debug doesn't matter in the final package.
  set(exported_targets_append "${exported_targets_append}${toadd}")
  set(exported_targets_append "${exported_targets_append}" PARENT_SCOPE)
endmacro(DR_export_target)

file(MAKE_DIRECTORY ${BUILD_INCLUDE})
macro(DR_export_header src_path dst_fname)
  # This is not COPYONLY nor @ONLY so this will expand ${} references, which
  # we do have in dr_api.h.
  configure_file(${src_path} ${BUILD_INCLUDE}/${dst_fname})
endmacro()

# We rely on dbghelp >= 6.0 for any use of drsyms: some sample clients and some
# tests in our suite.
# The system dbghelp pre-Vista is too old, so we copy one from VS.
# We locate it here for use in both subdirs.
# We just try all variants that we can to avoid the complexities of which
# versions are in "Program Files" vs "Program Files (x86)".
set(PROGFILES "$ENV{PROGRAMW6432}")
set(PROGFILES32 "$ENV{ProgramFiles\(x86\)}")
if ("${PROGFILES}" STREQUAL "")
  set(PROGFILES "$ENV{PROGRAMFILES}")
endif ()
if (X64)
  set(ARCH_SFX "x64")
else (X64)
  set(ARCH_SFX "x86")
endif (X64)
set(dbghelp_paths
  "${PROGFILES32}/Microsoft Visual Studio/*/Professional/Common7/IDE/Remote Debugger/${ARCH_SFX}/dbghelp.dll"
  "${PROGFILES}/Microsoft Visual Studio/*/Professional/Common7/IDE/Remote Debugger/${ARCH_SFX}/dbghelp.dll"
  "${PROGFILES32}/Microsoft Visual Studio */Common7/IDE/Remote Debugger/${ARCH_SFX}/dbghelp.dll"
  "${PROGFILES}/Microsoft Visual Studio */Common7/IDE/Remote Debugger/${ARCH_SFX}/dbghelp.dll"
  "${PROGFILES32}/Microsoft Visual Studio */Common7/IDE/dbghelp.dll"
  "${PROGFILES}/Microsoft Visual Studio */Common7/IDE/dbghelp.dll"
  "${PROGFILES32}/Windows Kits/*/Debuggers/${ARCH_SFX}/dbghelp.dll"
  "${PROGFILES}/Windows Kits/*/Debuggers/${ARCH_SFX}/dbghelp.dll")
# Older DTFW installed into its own dir:
if (X64)
  set(dbghelp_paths ${dbghelp_paths}
    "${PROGFILES}/Debugging Tools for Windows (x64)/dbghelp.dll")
else ()
  set(dbghelp_paths ${dbghelp_paths}
  "${PROGFILES}/Debugging Tools for Windows/dbghelp.dll")
endif ()
file(GLOB dbghelp_loc ${dbghelp_paths})
message(STATUS "For dbghelp, choosing among: ${dbghelp_loc}")
if (dbghelp_loc)
  # i#1219: exclude VS2005 x64 dbghelp as it is buggy
  list(LENGTH dbghelp_loc dbghelp_max)
  math(EXPR dbghelp_max "${dbghelp_max} - 1")
  set(dbghelp_index 0)
  list(GET dbghelp_loc 0 dbghelp_path)
  while (X64 AND dbghelp_path MATCHES "Visual Studio 8" AND
         ${dbghelp_index} LESS ${dbghelp_max})
    math(EXPR dbghelp_index "${dbghelp_index} + 1")
    list(GET dbghelp_loc ${dbghelp_index} dbghelp_path)
  endwhile()
  if (X64 AND dbghelp_path MATCHES "Visual Studio 8")
    # Not fatal: just nothing to put in package, and some tests may fail
    message(STATUS "Unable to find non-VS2005 dbghelp.dll")
    set(dbghelp_path dbghelp_path-NOTFOUND)
  else ()
    # DR_install requires all forward slashes
    string(REPLACE "\\" "/" dbghelp_path ${dbghelp_path})
    message(STATUS "Found ${dbghelp_path}")
  endif ()
else ()
  set(dbghelp_path dbghelp_path-NOTFOUND)
endif ()

if (APPLE)
  # install_name_tool needs write access (i#1372)
  set(owner_access OWNER_READ OWNER_WRITE)
else (APPLE)
  set(owner_access OWNER_READ)
endif (APPLE)

###########################################################################

# Let user pick which components to build.  This is especially
# important b/c w/o 64-bit tools they must configure two different
# build dirs for 64-bit windows: one for 64-bit core and one for
# 32-bit tools.
if (WIN32)
  option(BUILD_CORE "build core library and drinject tool" ON)
else (WIN32)
  option(BUILD_CORE "build core library" ON)
endif (WIN32)
option(BUILD_TOOLS "build tools" ON)
if (WIN32)
  option(BUILD_DRSTATS "build DRstats viewer (requires MFC)" ON)
endif (WIN32)

option(BUILD_EXT "build Extension libraries" ON)

# should we disallow selecting samples when core is not built?
# right now the samples build will just fail
option(BUILD_SAMPLES "build client samples" ON)

option(BUILD_CLIENTS "build client tools" ON)

if (ANDROID)
  # TODO i#1874: Android does not build drcachesim or drcpusim, which have
  # references spread throughout the docs.
  option(BUILD_DOCS "build documentation" OFF)
else ()
  option(BUILD_DOCS "build documentation" ON)
endif ()

# We include all tests and build them at configure time if requested.
# An alternative is to keep them in separate projects and only build
# when we run them via --build-and-test.
option(BUILD_TESTS "build tests" OFF)
# This is to disable the default -msgbox_mask.
option(AUTOMATED_TESTING "build for automated testing" OFF)
if (BUILD_TESTS)
  # Tests require tools
  set(BUILD_TOOLS ON)
  enable_testing()
  # add Dashboard support
  include(CTest)
endif (BUILD_TESTS)

if (BUILD_CORE)
  add_subdirectory(core)
endif (BUILD_CORE)

if (BUILD_TOOLS OR BUILD_DRSTATS OR BUILD_CLIENTS)
  add_subdirectory(libutil)
endif (BUILD_TOOLS OR BUILD_DRSTATS OR BUILD_CLIENTS)

if (BUILD_TOOLS)
  add_subdirectory(tools)
endif (BUILD_TOOLS)

if (BUILD_DRSTATS)
  # i#1376: we have our own variant of the standard FindMFC.cmake
  # so we can pass -D_UNICODE, as MBCS MFC support is not part of VS2013.
  # Update: To get things going with VS2017 I'm switching to the regular one
  # to get some version of DRstats building.  We don't really use it much
  # anymore; only if someone notices any unicode issues is it worth more time
  # updating make/modules/FindMFCUnicode.cmake.
  find_package(MFC)
  if (NOT MFC_FOUND)
    message(STATUS "MFC not found: disabling DRstats")
    set(BUILD_DRSTATS OFF)
  endif ()

  if (BUILD_DRSTATS)
    add_subdirectory(tools/DRstats)
  endif ()
endif (BUILD_DRSTATS)

# create empty logs dir for running out of build dir
file(MAKE_DIRECTORY "${PROJECT_BINARY_DIR}/logs")
# export visual studio lookup file just like full install, for using from build dir
# with projects that include DR sources
configure_file(${PROJECT_SOURCE_DIR}/suite/lookup_visualstudio.cmake
  ${BUILD_CMAKE}/lookup_visualstudio.cmake COPYONLY)
# export cpp2asm files just like full install, for using from build dir
# with projects that include DR sources
configure_file(${PROJECT_SOURCE_DIR}/make/cpp2asm_support.cmake
  ${BUILD_CMAKE}/cpp2asm_support.cmake COPYONLY)
configure_file(${PROJECT_SOURCE_DIR}/make/CMake_asm.cmake
  ${BUILD_CMAKE}/cpp2asm_add_newlines.cmake COPYONLY)
# we need to strip out the #include of configure.h
# XXX: this should be a build-time rule as it depends on asm_defines.asm!
file(READ "${PROJECT_SOURCE_DIR}/core/arch/asm_defines.asm" str)
string(REPLACE "#include \"configure.h\"" "" str "${str}")
file(WRITE "${BUILD_CMAKE}/cpp2asm_defines.h" "${str}")

# Export docs files just like full install, for using from build dir
# with projects that include DR sources.  Do this even if not BUILD_DOCS,
# for containing projects.
configure_file(${PROJECT_SOURCE_DIR}/api/docs/CMake_rundoxygen.cmake
  ${BUILD_CMAKE}/docs_rundoxygen.cmake COPYONLY)
configure_file(${PROJECT_SOURCE_DIR}/api/docs/CMake_doxyutils.cmake
  ${BUILD_CMAKE}/docs_doxyutils.cmake COPYONLY)

###########################################################################

# create CMake configuration files for clients and tests to use
# we also create cmake/DynamoRIOTarget.cmake in core/CMakeLists.txt
set(public_config_dir ${PROJECT_BINARY_DIR}/cmake)
set(public_config_file ${public_config_dir}/DynamoRIOConfig.cmake)
configure_file(
  ${PROJECT_SOURCE_DIR}/make/DynamoRIOConfig.cmake.in
  ${public_config_file}
  @ONLY)

# INCLUDEFILE feature
file(READ ${public_config_file} contents)
string(REGEX MATCHALL "\nINCLUDEFILE [^ \r\n]*" includes "${contents}")
foreach (inc ${includes})
  string(REGEX REPLACE "\nINCLUDEFILE " "" incfile "${inc}")
  file(READ "${incfile}" subst)
  # strip out copyright header, which we prefixed with "##"
  string(REGEX REPLACE "(^|\n)##[^\n]*" "" subst "${subst}")
  string(REGEX REPLACE "${inc}" "\n${subst}" contents "${contents}")
endforeach (inc)
file(WRITE ${public_config_file} "${contents}")

configure_file(
  ${PROJECT_SOURCE_DIR}/make/DynamoRIOConfigVersion.cmake.in
  ${PROJECT_BINARY_DIR}/cmake/DynamoRIOConfigVersion.cmake
  @ONLY)
DR_install(FILES
  ${public_config_file}
  ${PROJECT_BINARY_DIR}/cmake/DynamoRIOConfigVersion.cmake
  DESTINATION ${INSTALL_CMAKE}
)
# export suite scripts even if not building tests
DR_install(FILES
  ${PROJECT_SOURCE_DIR}/suite/runsuite_common_pre.cmake
  ${PROJECT_SOURCE_DIR}/suite/runsuite_common_post.cmake
  DESTINATION ${INSTALL_CMAKE}
)
# export visual studio lookup support
DR_install(FILES ${PROJECT_SOURCE_DIR}/suite/lookup_visualstudio.cmake
  DESTINATION ${INSTALL_CMAKE}
)
# export cpp2asm support, with consistent names
DR_install(FILES ${PROJECT_SOURCE_DIR}/make/cpp2asm_support.cmake
  DESTINATION ${INSTALL_CMAKE}
)
DR_install(FILES ${PROJECT_SOURCE_DIR}/make/CMake_asm.cmake
  DESTINATION ${INSTALL_CMAKE} RENAME cpp2asm_add_newlines.cmake
)
# we need to strip out the #include of configure.h
DR_install(FILES ${BUILD_CMAKE}/cpp2asm_defines.h DESTINATION ${INSTALL_CMAKE})

# export docs support, with consistent names
DR_install(FILES ${PROJECT_SOURCE_DIR}/api/docs/CMake_rundoxygen.cmake
  DESTINATION ${INSTALL_CMAKE} RENAME docs_rundoxygen.cmake
)
DR_install(FILES ${PROJECT_SOURCE_DIR}/api/docs/CMake_doxyutils.cmake
  DESTINATION ${INSTALL_CMAKE} RENAME docs_doxyutils.cmake
)

###########################################################################

# clear tools file to avoid duplicates on re-configure
if (X64)
  set(TOOLS_LISTNAME "list64")
else ()
  set(TOOLS_LISTNAME "list32")
endif ()
file(WRITE ${PROJECT_BINARY_DIR}/tools/${TOOLS_LISTNAME} "")
DR_install(FILES ${PROJECT_BINARY_DIR}/tools/${TOOLS_LISTNAME} DESTINATION tools)

function (register_tool_file name)
  file(APPEND ${PROJECT_BINARY_DIR}/tools/${TOOLS_LISTNAME} "${name}\n")
endfunction ()

# Used by docs, samples, and tests, so here instead of in core/CMakeLists.txt.
# Set VERSION_NUMBER_INTEGER in dr_api.h:
DR_export_header(${PROJECT_SOURCE_DIR}/core/lib/dr_api.h dr_api.h)
if (NOT ANNOTATIONS)
  # Kind of a hack, in case the user explicitly disables annotations
  file(READ ${BUILD_INCLUDE}/dr_api.h contents)
  string(REGEX REPLACE "\n[^\n]*nnotation[^\n]*" "" contents "${contents}")
  file(WRITE ${BUILD_INCLUDE}/dr_api.h "${contents}")
endif ()
add_custom_target(api_headers)
if (AARCH64)
  add_dependencies(api_headers gen_aarch64_codec gen_aarch64_opcodes)
elseif(RISCV64)
  add_dependencies(api_headers gen_riscv64_codec gen_riscv64_headers)
endif()

DR_export_header(${PROJECT_SOURCE_DIR}/core/lib/dr_app.h dr_app.h)

DR_install(DIRECTORY ${BUILD_INCLUDE} DESTINATION .
  REGEX "annotations/valgrind.h|annotations/memcheck.h" EXCLUDE)

# Used by tests in multiple places, and by some drcachesim non-test binaries.
if (UNIX)
  if (ANDROID)
    # pthreads is inside Bionic on Android, and for some reason find_library() can't
    # locate libm or libdl within the Android toolchain: we just assume they're there.
    set(libmath m)
    set(libdl dl)
    set(libpthread "")
  else ()
    # i#720: cmake fails to find 32-bit libraries in Ubuntu 11.10.
    # This is because cmake uses CMAKE_LIBRARY_ARCHITECTURE to handle
    # multi-arch compilation, whose value is x86_64-linux-gnu or i386-linux-gnu
    # for 64/32-bit. However, in Ubuntu 11.10, some of the 32-bit libraries
    # like libm.so, libdl.so, and libpthread.so are in /usr/lib32 or /lib32
    # instead of /lib/i386-linux-gnu, so cmake cannot to find them.
    # solution: add explicit paths for lookup.
    find_library(libmath m)
    if (NOT libmath AND NOT X64)
      find_library(libmath m PATHS /usr/lib32 /lib32)
    endif ()
    find_library(libdl dl)
    if (NOT libdl AND NOT X64)
      find_library(libdl dl PATHS /usr/lib32 /lib32)
    endif ()
    find_library(libpthread pthread)
    if (NOT libpthread AND NOT X64)
      find_library(libpthread pthread PATHS /usr/lib32 /lib32)
    endif ()
    if (NOT libmath OR NOT libdl OR NOT libpthread)
      message(FATAL_ERROR "cannot find required libs m, dl, and/or pthread")
    endif ()
  endif ()
endif ()

function (link_with_pthread target)
  if (UNIX AND NOT ANDROID) # pthreads is inside Bionic on Android
    target_link_libraries(${target} ${libpthread})
  endif ()
endfunction ()

function (mac_add_inc_and_lib header lib)
  # Locate include and lib directories on MacOS.
  # TODO: Write proper package for snappy and lz4 lookup.
  find_path(INCLUDE_DIR ${header} HINTS /usr/local/include /opt/homebrew/include)
  if (INCLUDE_DIR)
    include_directories(${INCLUDE_DIR})
  else ()
    message(WARNING "Failed to locate header ${header}")
  endif ()
  find_path(LIB_DIR ${lib} HINTS /usr/local/lib /opt/homebrew/lib)
  if (LIB_DIR)
    link_directories(${LIB_DIR})
  else ()
    message(WARNING "Failed to locate lib ${lib}")
  endif ()
endfunction ()

# zlib, snappy, and lz4 are used for some clients/ and tests.
# TODO i#5767: Install an explicit zlib package on our Windows GA CI images
# (this find_package finds a strawberry perl zlib which causes 32-bit build
# and 64-bit private loader issues).
if (WIN32 AND AUTOMATED_TESTING)
  set(ZLIB_FOUND OFF)
else ()
  find_package(ZLIB)
endif ()
# On Ubuntu 14.10, 32-bit builds fail to link with -lsnappy, just ignore.
if (UNIX AND X64)
  find_library(libsnappy snappy)
  if (libsnappy)
    message(STATUS "Found snappy: ${libsnappy}")
    if (APPLE)
      mac_add_inc_and_lib(snappy.h libsnappy.a)
    endif ()
  endif ()
  find_library(liblz4 lz4)
  if (liblz4)
    message(STATUS "Found liblz4: ${liblz4}")
    if (APPLE)
      mac_add_inc_and_lib(lz4.h liblz4.a)
    endif ()
  endif ()
endif ()

if (BUILD_CLIENTS)
  # Must be prior to api/docs
  add_subdirectory(clients)
endif (BUILD_CLIENTS)

if (BUILD_DOCS)
  find_package(Doxygen)
  if (NOT DOXYGEN_FOUND)
    # We require for any automated suite on Linux or Windows.
    if (TEST_SUITE AND NOT APPLE)
      message(FATAL_ERROR "doxygen is required to build the documentation")
    else ()
      # Non-fatal for a single, un-official build.
      message(WARNING "doxygen not found: documentation will NOT be built")
      set(BUILD_DOCS OFF)
    endif ()
  endif (NOT DOXYGEN_FOUND)

  if (BUILD_DOCS)
    add_subdirectory(api/docs)
  else (BUILD_DOCS)
    message("*** NOT building documentation *** (must re-enable BUILD_DOCS manually if fix up the component paths)")
    # Note the difference between cache vars and live vars in CMake.
    # The cache var indicates "would like to build docs" and the live var
    # "capable of building docs."  The problem is that if we FORCE the
    # cache var OFF as further confirmation of the warning message, the
    # user must not only fix up the paths in the required component vars
    # but also re-enable BUILD_DOCS before re-configuring due to our
    # guard above.  An alternative is to add an option BUILD_DOCS_CAPABLE
    # as an informational message that is peristent and right next to the
    # real option, but I think forcing the user to re-enable BUILD_DOCS
    # is actually simpler and reasonable.
    set(BUILD_DOCS OFF CACHE BOOL "build documentation" FORCE)
  endif (BUILD_DOCS)
endif (BUILD_DOCS)

###########################################################################
# Style checks:

option(VERA_ERROR "Turn vera++ checks into error (default)" ON)

find_package(vera++ QUIET)
if (vera++_FOUND)
  message(STATUS "Using vera++ for code style checks")
  include(${VERA++_USE_FILE})
  # We use our own modified copy in order to pass --error and for
  # regex exclusions with vera++ 1.2.1.
  include(third_party/vera++/use_vera++.cmake)
  add_vera_targets_for_dynamorio(*.h *.c *.cpp
    RECURSE
    EXCLUSION "${PROJECT_SOURCE_DIR}/make/style_checks/exclude"
    ROOT "${PROJECT_SOURCE_DIR}/make/style_checks")
else ()
  message(STATUS "WARNING: vera++ not found: disabling code style checks")
endif ()

###########################################################################

DR_install(FILES
  ${PROJECT_SOURCE_DIR}/README
  ${PROJECT_SOURCE_DIR}/License.txt
  ${PROJECT_SOURCE_DIR}/ACKNOWLEDGEMENTS
  DESTINATION .)

# We put all our libs and exes in common dirs, making it easier
# to copy our symbols.  It's a pain to construct the pdb name
# counterparts for targets, so we use the DR_install(DIRECTORY) commands
# where we can use wildcards.
# Slightly ugly to have hardcoded names: long-term CMake should
# support auto-installing pdb files.
DR_install(DIRECTORY ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/
  DESTINATION ${INSTALL_LIB_BASE}
  FILE_PERMISSIONS ${owner_access} OWNER_EXECUTE GROUP_READ GROUP_EXECUTE
  WORLD_READ WORLD_EXECUTE
  FILES_MATCHING
  PATTERN "*.debug"
  PATTERN "*.pdb"
  REGEX ".*.dSYM/.*DWARF/.*" # too painful to get right # of backslash for literal .
  PATTERN "dynamorio.pdb" EXCLUDE # in ${INSTALL_LIB}
  PATTERN "libdynamorio.so.*debug" EXCLUDE # in ${INSTALL_LIB}
  PATTERN "libdrpreload.so.debug" EXCLUDE # in ${INSTALL_LIB}
  PATTERN "policy_static.pdb" EXCLUDE
  REGEX ".*dynamorio.*.dSYM/.*" EXCLUDE
  REGEX "libdrpreload.*.dSYM/.*" EXCLUDE
  REGEX "policy_static.*.dSYM/.*" EXCLUDE
  )
DR_install(DIRECTORY ${DR_LIBRARY_OUTPUT_DIRECTORY}/
  DESTINATION ${INSTALL_LIB}
  FILE_PERMISSIONS ${owner_access} OWNER_EXECUTE GROUP_READ GROUP_EXECUTE
  WORLD_READ WORLD_EXECUTE
  FILES_MATCHING
  PATTERN "dynamorio.pdb"
  PATTERN "libdynamorio.so.*debug"
  PATTERN "libdrpreload.so.debug"
  REGEX ".*dynamorio.*.dSYM/.*DWARF/.*"
  REGEX "libdrpreload.*.dSYM/.*DWARF/.*"
  )
if (BUILD_TOOLS OR WIN32)
  DR_install(DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/
    DESTINATION ${INSTALL_BIN}
    FILE_PERMISSIONS ${owner_access} OWNER_EXECUTE GROUP_READ GROUP_EXECUTE
    WORLD_READ WORLD_EXECUTE
    FILES_MATCHING
    PATTERN "*.debug"
    PATTERN "*.pdb"
    REGEX ".*.dSYM/.*DWARF/.*" # too painful to get right # of backslash for literal .
    PATTERN "runstats.debug" EXCLUDE # since not installing tool
    PATTERN "run_in_bg.debug" EXCLUDE # since not installing tool
    PATTERN "*_exe.debug" EXCLUDE # since not installing tool
    REGEX "runstats.dSYM/.*" EXCLUDE
    REGEX "run_in_bg.dSYM/.*" EXCLUDE
    REGEX ".*_exe.dSYM/.*" EXCLUDE
    )
endif (BUILD_TOOLS OR WIN32)

if (BUILD_DOCS)
  file(WRITE ${PROJECT_BINARY_DIR}/docs/DynamoRIO.html "<html>\n<head>\n<meta http-equiv=\"refresh\" content=\"0; URL=./html/index.html\">\n</head><body></body>")
  DR_install(FILES ${PROJECT_BINARY_DIR}/docs/DynamoRIO.html DESTINATION docs)
endif (BUILD_DOCS)

# create empty logs dir for release package
# CPack seems to ignore empty dirs so add a README file
file(WRITE ${PROJECT_BINARY_DIR}/logs/README "Empty dir for debug-build log files.\n")
DR_install(FILES ${PROJECT_BINARY_DIR}/logs/README DESTINATION logs)

###########################################################################

# Extensions (i#277/PR 540817)
if (BUILD_EXT)
  add_subdirectory(ext)
endif (BUILD_EXT)

if (BUILD_SAMPLES OR BUILD_TESTS OR BUILD_CLIENTS)
  # api/samples is set up to function both as a separate project for
  # building samples vs a DynamoRIO installation, and to allow
  # building as a component of the whole source tree
  set(DynamoRIO_INTERNAL ON) # do not import dynamorio lib target
  set(DynamoRIO_DIR ${PROJECT_BINARY_DIR}/cmake)
endif (BUILD_SAMPLES OR BUILD_TESTS OR BUILD_CLIENTS)

if (BUILD_SAMPLES)
  add_subdirectory(api/samples)
endif (BUILD_SAMPLES)

# This must be after samples and clients, as we use those targets in tests.
if (BUILD_TESTS)
  add_subdirectory(suite/tests)
endif (BUILD_TESTS)

# We must append to this file to avoid cmake_install.cmake's diff from thinking
# the exports have changed and thus clobbering the other config's files.
# This is what is copied to ${CMAKE_INSTALL_PREFIX}/${INSTALL_CMAKE}.
DR_target_install(CODE "file(APPEND \"${PROJECT_BINARY_DIR}/CMakeFiles/Export/cmake/${exported_targets_name}.cmake\" \"${exported_targets_append}\")")
# Create the exported targets file.  This will include all targets added
# in subdirectories via DR_install(TARGETS ... EXPORT ${exported_targets_name}).
DR_target_install_dst(${INSTALL_CMAKE} EXPORT ${exported_targets_name})

###########################################################################
# packaging

# For building a full release package, we rely on an external script to
# build multiple configurations ({release,debug}x{32-bit,64-bit}) with a
# shared install tree.  In the final build, if an appropriate
# CPACK_INSTALL_CMAKE_PROJECTS variable is set to point at all 4 builds,
# "make package" will produce a package from all 4 installs.

# TODO i#74: create source tarball via 'make package_source'
# Just need to exclude exports/ (or move exports/ to ../?) and
# other files inside source dir (or run on untouched source dir)
# and update package.{sh,bat}.
# Note that CPACK_SOURCE_IGNORE_FILES may be tricky to set from here
# due to the escapes needed: may want to move to a configured file.

if (UNIX)
  # not bothering with TZ (5M!) or TBZ2 (3.0M vs 3.2M for TGZ)
  set(CPACK_GENERATOR "STGZ;TGZ")
  # not bothering with TZ or TBZ2
  set(CPACK_SOURCE_GENERATOR "TGZ")
  # We've already split out our separate .debug files and stripped the
  # originals in our build rules
  set(CPACK_STRIP_FILES OFF)
  if (APPLE)
    set(CPACK_SYSTEM_NAME "MacOS")
  elseif (ANDROID)
    set(CPACK_SYSTEM_NAME "Android")
  else ()
    set(CPACK_SYSTEM_NAME "Linux")
  endif ()
else (UNIX)
  set(CPACK_GENERATOR "ZIP")
  set(CPACK_SOURCE_GENERATOR "ZIP")
  set(CPACK_SYSTEM_NAME "Windows")
endif (UNIX)

set(CPACK_PACKAGE_NAME "DynamoRIO")
set(CPACK_PACKAGE_VENDOR "DynamoRIO community")
set(CPACK_PACKAGE_DESCRIPTION_FILE "${PROJECT_SOURCE_DIR}/README")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "DynamoRIO Dynamic Instrumentation Tool Platform")
set(CPACK_RESOURCE_FILE_LICENSE "${PROJECT_SOURCE_DIR}/License.txt")
set(CPACK_RESOURCE_FILE_README "${PROJECT_SOURCE_DIR}/README")

set(CPACK_PACKAGE_VERSION "${VERSION_NUMBER}")
set(CPACK_PACKAGE_VERSION_MAJOR "${VERSION_NUMBER_MAJOR}")
set(CPACK_PACKAGE_VERSION_MINOR "${VERSION_NUMBER_MINOR}")
string(REGEX REPLACE
  "^[0-9]+\\.[0-9]+\\.([0-9]+)" "\\1" CPACK_PACKAGE_VERSION_PATCH "${VERSION_NUMBER}")

# CPack tarballs do not allow setting a different name for the base
# directory and the file: I tried a ton of CPack variables for "install
# dir" and looked at the source code.  Most of the variables are for the
# other installers (rpm, nsis).  I can hack it via
# CPACK_TEMPORARY_PACKAGE_FILE_NAME if I hardcode the extension: but maybe
# having the full version in the base dir is a good thing, though I'm not
# sure about the caps.
# We omit the -NN suffix for the build number if it is zero.
if ("${BUILD_NUMBER}" STREQUAL "0")
  set(PACKAGE_SUFFIX "")
else ()
  set(PACKAGE_SUFFIX "-${BUILD_NUMBER}")
endif ()
set(CPACK_PACKAGE_FILE_NAME
  "DynamoRIO-${PACKAGE_PLATFORM}${CPACK_SYSTEM_NAME}${PACKAGE_SUBSYS}-${CPACK_PACKAGE_VERSION}${PACKAGE_SUFFIX}")
set(CPACK_PACKAGE_INSTALL_DIRECTORY "dynamorio")
set(CPACK_PACKAGE_INSTALL_REGISTRY_KEY "DynamoRIO")
set(CPACK_PACKAGE_RELOCATABLE "true")

include(CPack)
