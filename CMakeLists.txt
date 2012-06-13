# **********************************************************
# Copyright (c) 2010-2012 Google, Inc.    All rights reserved.
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

###########################################################################
# Missing features:
# i#77: make DRgui a subproject to support with different compiler from libutil 
# i#73: provide automatic suport for locating ml64.exe when using Vista SDK+WDK
# i#79: custom build number or label for non-release builds
# i#84: nmake adds space before 1st / in 1st arg to a command invoked with quotes
# i#74: create source tarball via 'make package_source' 
# i#68: replace perl with cmake scripts and CTest
# i#69: re-add support for core unit tests and standalone build rules
# i#70: symbol store support in core/Makefile and tools/Makefile
# i#75: convert clients/standalone/Makefile to CMake 
# i#72: RHEL3 linker script has no __executable_start
# i#60: re-add libutil/ unit tests 
#
# Not filed:
# * move info like "build core solely from DDK" from make/compiler.mk into
#   HowToBuild.wiki
###########################################################################

if ("${CMAKE_GENERATOR}" MATCHES "Visual Studio")
  # for output dir name control
  cmake_minimum_required(VERSION 2.8.2)
else ()
  cmake_minimum_required(VERSION 2.6)
endif ()

# for non-makefile-based generators (i.e., Visual Studio) there is no
# CMAKE_BUILD_TYPE and instead there are multiple configs.
# note that we would love cmake to support multi-config for makefiles too,
# but given that it doesn't, and that we don't have full control
# over the output dir names (until cmake 2.8.4), I'm collapsing
# the VS generator configs to a single choice to match the makefiles.
# this must be done prior to the project() command and the var
# must be set in the cache.
if ("${CMAKE_GENERATOR}" MATCHES "Visual Studio")
  if (DEBUG)
    set(CMAKE_CONFIGURATION_TYPES "Debug" CACHE STRING "" FORCE)
  else (DEBUG)
    set(CMAKE_CONFIGURATION_TYPES "Release" CACHE STRING "" FORCE)
  endif (DEBUG)
  # we want to use the _LOCATION_<config> property
  string(TOUPPER "${CMAKE_CONFIGURATION_TYPES}" upper)
  set(location_suffix "_${upper}")
else ("${CMAKE_GENERATOR}" MATCHES "Visual Studio")
  set(location_suffix "")
endif ("${CMAKE_GENERATOR}" MATCHES "Visual Studio")

set(cmake_ver_string
  "${CMAKE_MAJOR_VERSION}.${CMAKE_MINOR_VERSION}.${CMAKE_RELEASE_VERSION}")
if ("${cmake_ver_string}" STRLESS "2.6.4")
  # Workaround for cmake bug #8639 where CMAKE_ASM_SOURCE_FILE_EXTENSIONS
  # is not set and our asm files are not built (should be fixed in 2.6.4).
  # We can simply set the var ahead of time, luckily, since noone clears it:
  # Alternative workarounds:
  # - Use ASM-ATT instead of ASM: should work
  # - I tried puting a patched CMakeASMInformation.cmake ahead of share/Modules
  #   but enable_language() seems to not use the CMAKE_MODULE_PATH
  set(CMAKE_ASM_SOURCE_FILE_EXTENSIONS s;S;asm)
endif ("${cmake_ver_string}" STRLESS "2.6.4")

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

project(DynamoRIO NONE)
if (DEFINED GENERATE_PDBS AND NOT GENERATE_PDBS)
  # i#310: support building over cygwin ssh where we cannot build pdbs.
  # To prevent cmake's try-compile for its working compiler test and
  # its ABI determination test we request a Release build config
  # via a custom Plaform/Windows-cl.cmake in our make/ dir.
  set(CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/make")
endif ()
enable_language(C)
enable_language(CXX)

include(CheckCCompilerFlag)

###########################################################################
# configuration options

# many are core-specific

# configurations that also have defines
option(DEBUG "build with asserts and logging enabled")
option(DRGUI_DEMO "build DRgui without the no-longer-supported Run, etc. controls" ON)

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

# target
if (UNIX)
  set(LINUX 1)
endif (UNIX)
if (WIN32)
  set(WINDOWS 1)
endif (WIN32)

# Whether 64-bit is expected to be selected by user setting up compiler
# prior to invoking CMake: it has to be that way for Windows, and for
# Linux the user should set CFLAGS to -m32 or -m64
# to override gcc's default.  To simplify matters we only look at
# CMAKE_C_SIZEOF_DATA_PTR, controlled by CFLAGS, so the user doesn't
# have to also set CXXFLAGS (CMAKE_SIZEOF_VOID_P happens
# to come from CXXFLAGS).  (CMAKE_C_SIZEOF_DATA_PTR can be relied on
# to be set in all CMake versions we support.)
if (CMAKE_C_SIZEOF_DATA_PTR EQUAL 8)
  set(X64 ON)
else(CMAKE_C_SIZEOF_DATA_PTR EQUAL 8)
  set(X64 OFF)
endif (CMAKE_C_SIZEOF_DATA_PTR EQUAL 8)

option(VMKERNEL "target VMkernel (not officially supported yet)")
option(MACOS "target MacOS (not officially supported yet)")

# high-level configurations
option(VMAP "client support" ON)
option(VPS "security support but no client support")
option(VMSAFE "client support including security API")
if (VMAP)
  set(CLIENT_INTERFACE 1)
  set(APP_EXPORTS 1)
endif (VMAP)
if (VMSAFE)
  set(PROGRAM_SHEPHERDING 1)
  set(CLIENT_INTERFACE 1)
  set(APP_EXPORTS 1)
  set(HOT_PATCHING_INTERFACE 1)
endif (VMSAFE)
if (VPS)
  set(PROGRAM_SHEPHERDING 1)
  set(HOT_PATCHING_INTERFACE 1)
  set(PROCESS_CONTROL 1)
  if (WIN32)
    set(GBOP 1)
  endif (WIN32)
endif (VPS)

# temporary: once build+support issues fixed, remove and add HOT_PATCHING_INTERFACE
# to VMAP by default
option(PROBE "enable not-yet-supported Probe API")
mark_as_advanced(PROBE)
if (PROBE)
  set(HOT_PATCHING_INTERFACE 1)
endif (PROBE)

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

# PR 403006 - We want to keep symbols to get glibc backtrace for DR errors, but
# that causes a collision of app and dr symbols on visor (DR's crc32() was
# linked to imports of crc32() in libz.so in hostd).  So, we just keep symbol
# support for glibc backtrace only on linux - hope we don't run into any
# collisions on linux!  Longer term we do our own backtrace with symbols.
#
if (INTERNAL AND DEBUG AND NOT VMKERNEL)
  # Now that INTERNAL is the exported debug build, turning off by default
  set(KEEP_SYMBOLS_DEFAULT OFF)
else (INTERNAL AND DEBUG AND NOT VMKERNEL)
  set(KEEP_SYMBOLS_DEFAULT OFF)
endif (INTERNAL AND DEBUG AND NOT VMKERNEL)
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
set(KEEP_SYMBOLS_FOR_LIBC_BACKTRACE "" CACHE STRING
  "keep internal symbols exported: ON or OFF overrides default")
# FIXME i#170: ditto
if ("${KEEP_SYMBOLS_FOR_LIBC_BACKTRACE}" STREQUAL "")
  set(KEEP_SYMBOLS_FOR_LIBC_BACKTRACE ${KEEP_SYMBOLS_DEFAULT})
endif ()

option(CALLPROF "internal caller profiling support")
option(LINKCOUNT "internal linkcount profiling")
option(STANDALONE_UNIT_TEST "internal unit tests")
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
else (WIN32 AND DEBUG)
  set(preferred_base "0x71000000" CACHE STRING "Preferred library base address")
endif (WIN32 AND DEBUG)
if (WIN32 OR X64)
  # for x64: PR 253624: we need our library to be next to our heap
  # for win32: not PIC so need a base
  # we studied existing exe + dll bases and tried to pick non-conflicting addresses
  set(SET_PREFERRED_BASE 1)
endif (WIN32 OR X64)
if (VMKERNEL)
  # we end up with the default executable base (0x08*) so go back to 0 base
  # (else we fail to load on esx)
  set(SET_PREFERRED_BASE 1)
  set(preferred_base "0x00000000")
endif (VMKERNEL)

# we do not support most users choosing these
mark_as_advanced(
  VMKERNEL
  MACOS
  VMAP
  VMSAFE
  VPS
  KSTATS
  CALLPROF
  LINKCOUNT
  STANDALONE_UNIT_TEST
  KEEP_SYMBOLS_FOR_LIBC_BACKTRACE
  SET_PREFERRED_BASE
  preferred_base
  GENERATE_PDBS
  DRGUI_DEMO
  )

# we ignore CMAKE_BUILD_TYPE (replaced by our own DEBUG, INTERNAL, etc.)
# we clear both base and build-type cflags and then use just base.
# XXX: or should we support build type and auto-map to ours?
# if we do support we'll need to remove flags like /MD coming from
# the CMAKE_C_FLAGS_*, which are added to the base CMAKE_C_FLAGS.
# XXX: could use CMAKE_USER_MAKE_RULES_OVERRIDE to set these but not any cleaner
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

##################################################
# resources when packaging

# i#83: use svn revision of last commit as patchlevel ver# to distinguish
# mid-release builds.
# XXX: if we later switch to a non-linear version control system we'll
# want to switch to a date or something, but be aware of timezone issues
# so we may want a cron job that updates an official date in the sources.
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
      execute_process(COMMAND ${GIT} log
        WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
        RESULT_VARIABLE git_result
        ERROR_VARIABLE git_err
        OUTPUT_VARIABLE git_out)
      if (git_result OR git_err)
        message(FATAL_ERROR "*** ${GIT} show failed: ***\n${git_err}")
      endif (git_result OR git_err)
      # look for first commit with git-svn-id
      string(REGEX MATCH "git-svn-id: http[^@]*@[0-9]+" git_out "${git_out}")
      string(REGEX REPLACE "git-svn-id: http[^@]*@" "" git_out "${git_out}")
      set(VERSION_NUMBER_PATCHLEVEL "${git_out}")
    endif (GIT)
  endif (EXISTS "${PROJECT_SOURCE_DIR}/.git")
endif (EXISTS "${PROJECT_SOURCE_DIR}/.svn")

# N.B.: once VERSION_NUMBER crosses 4.x we'll need to add another
# symlink loop in core/CMakeLists.txt
set(VERSION_NUMBER_DEFAULT "3.2.${VERSION_NUMBER_PATCHLEVEL}")
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
mark_as_advanced(
  VERSION_NUMBER
  VERSION_COMMA_DELIMITED
  BUILD_NUMBER
  UNIQUE_BUILD_NUMBER
  CUSTOM_PRODUCT_NAME
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

if (UNIX)

  # Assume clang behaves like gcc.  CMake 2.6 won't detect clang and will set
  # CMAKE_COMPILER_IS_GNUCC to TRUE, but 2.8 does not.  We prefer the 2.6
  # behavior.
  string(REGEX MATCH "clang" CMAKE_COMPILER_IS_CLANG "${CMAKE_C_COMPILER}")
  if (CMAKE_COMPILER_IS_CLANG)
    set(CMAKE_COMPILER_IS_GNUCC TRUE)
  endif ()

  if (CYGWIN)
    message(FATAL_ERROR "building using gcc within cygwin is not supported")
  endif (CYGWIN)
  if (NOT CMAKE_COMPILER_IS_GNUCC)
    # we use gcc extensions
    message(FATAL_ERROR "gcc is required to build")
  endif (NOT CMAKE_COMPILER_IS_GNUCC)
  # "gcc -E" on a non-.c-extension file gives message:
  #   "linker input file unused because linking not done"
  # and doesn't produce any output, so we must use cpp for our .asm files.
  # we assume it's in the same dir.
  get_filename_component(compiler_path ${CMAKE_C_COMPILER} PATH)
  find_program(CMAKE_CPP cpp HINTS "${compiler_path}" DOC "path to C preprocessor")
  if (NOT CMAKE_CPP)
    message(FATAL_ERROR "cpp is required to build")
  endif (NOT CMAKE_CPP)
  mark_as_advanced(CMAKE_CPP)

  # Detect clang, which masquerades as gcc.  CMake 2.6 doesn't know how to
  # detect it.
  string(REGEX MATCH "clang" CMAKE_COMPILER_IS_CLANG "${CMAKE_C_COMPILER}")

else (UNIX)

  if (NOT ${COMPILER_BASE_NAME} STREQUAL "cl")
    # we use cl pragmas and intrinsics
    message(FATAL_ERROR "cl (Microsoft C++ compiler) is required to build")
  endif (NOT ${COMPILER_BASE_NAME} STREQUAL "cl")

  # CMake older than 2.6.3 does not automatically look for ml or ml64
  if ("${CMAKE_GENERATOR}" MATCHES "Visual Studio")
    # CMAKE_C_COMPILER doesn't have full path, unless user has in PATH
    if (MSVC10)
      get_filename_component(vc_dir [HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\VisualStudio\\10.0\\Setup\\VC;ProductDir] REALPATH)
    elseif (MSVC90)
      get_filename_component(vc_dir [HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\VisualStudio\\9.0\\Setup\\VC;ProductDir] REALPATH)
    elseif (MSVC80)
      get_filename_component(vc_dir [HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\VisualStudio\\8.0\\Setup\\VC;ProductDir] REALPATH)
    endif()
    if (X64)
      set(cl_path "${vc_dir}/bin/amd64")
    else (X64)
      set(cl_path "${vc_dir}/bin")
    endif (X64)
    # while we can get the cl path, we can't run cl b/c mspdb80.dll must
    # be on the PATH: so even w/ VS generators we require PATH to be set.
    if ("${CMAKE_C_COMPILER}" STREQUAL "cl")
      set(CMAKE_C_COMPILER "${cl_path}/${CMAKE_C_COMPILER}")
    endif ()
  else ()
    get_filename_component(cl_path ${CMAKE_C_COMPILER} PATH)
  endif ()
  if (X64)
    find_program(CMAKE_ASM_COMPILER ml64.exe HINTS "${cl_path}" DOC "path to assembler")
  else (X64)
    find_program(CMAKE_ASM_COMPILER ml.exe HINTS "${cl_path}" DOC "path to assembler")
  endif (X64)
  if (NOT CMAKE_ASM_COMPILER)
    message(FATAL_ERROR "assembler not found: required to build")
  endif (NOT CMAKE_ASM_COMPILER)
  message(STATUS "Found assembler: ${CMAKE_ASM_COMPILER}")

  set(CMAKE_CPP ${CMAKE_C_COMPILER})

  # cmake has CMAKE_RC_COMPILER, but no message compiler
  if ("${CMAKE_GENERATOR}" MATCHES "Visual Studio")
    # this path is only present for 2008+, but we currently require PATH to
    # be set up anyway
    get_filename_component(sdk_dir "[HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Microsoft SDKs\\Windows;CurrentInstallFolder]" REALPATH)
    if (X64)
      set(sdk_bindir "${sdk_dir}/bin/x64")
    else (X64)
      set(sdk_bindir "${sdk_dir}/bin")
    endif (X64)
  endif ()
  find_program(CMAKE_MC_COMPILER mc.exe HINTS "${sdk_bindir}" 
    DOC "path to message compiler")
  if (NOT CMAKE_MC_COMPILER)
    message(FATAL_ERROR "message compiler not found: required to build")
  endif (NOT CMAKE_MC_COMPILER)
  message(STATUS "Found message compiler: ${CMAKE_MC_COMPILER}")
  mark_as_advanced(CMAKE_MC_COMPILER)

  # DDKROOT env var is recommended method in our docs
  if (NOT "$ENV{DDKROOT}" STREQUAL "")
    set(DDK_ROOT "$ENV{DDKROOT}")
  # Check default install path
  elseif (EXISTS "$ENV{SYSTEMDRIVE}/WINDDK/3790.1830/")
    set(DDK_ROOT "$ENV{SYSTEMDRIVE}/WINDDK/3790.1830/")
  elseif (EXISTS "$ENV{SYSTEMDRIVE}/WINDDK/6000/")
    set(DDK_ROOT "$ENV{SYSTEMDRIVE}/WINDDK/6000/")
  elseif (EXISTS "$ENV{SYSTEMDRIVE}/WINDDK/7600.16385.1/")
    set(DDK_ROOT "$ENV{SYSTEMDRIVE}/WINDDK/7600.16385.1/")
  endif (NOT "$ENV{DDKROOT}" STREQUAL "")
  # Note that we want the oldest ntdll.lib for maximum compatibility.
  # The 2003 DDK (3790.1830) or Vista WDK (6000) both have these paths:
  if (X64)
    set(NTDLL_LIB_SUBPATH lib/wnet/amd64)
  else (X64)
    # Newer WDK (7600.16385.1) does not have w2k so use wxp: if using
    # that WDK, won't be able to run on 2K or NT.
    if (EXISTS "${DDK_ROOT}/lib/w2k/i386")
      set(NTDLL_LIB_SUBPATH lib/w2k/i386)
    else (EXISTS "${DDK_ROOT}/lib/w2k/i386")
      message(STATUS "DDK/WDK is too recent: no support for Windows 2000 or earlier")
      set(NTDLL_LIB_SUBPATH lib/wxp/i386)
    endif (EXISTS "${DDK_ROOT}/lib/w2k/i386")
  endif (X64)
  # Use find_library() and not find_path() in case on LIB env var or sthg
  find_library(NTDLL_LIBPATH ntdll.lib HINTS "${DDK_ROOT}/${NTDLL_LIB_SUBPATH}"
    DOC "Path to ntdll.lib from either 2003 DDK (3790.1830) or Vista WDK (6000)")
  if (NOT NTDLL_LIBPATH)
    message(FATAL_ERROR "ntdll.lib not found: required to build")
  endif (NOT NTDLL_LIBPATH)
  message(STATUS "Found ntdll.lib: ${NTDLL_LIBPATH}")
  get_filename_component(NTDLL_LIBPATH "${NTDLL_LIBPATH}" PATH)
  link_directories(${NTDLL_LIBPATH})

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

# compiler flags
if (UNIX)
  # -std=c99 doesn't quite work
  # FIXME case 191480: we used to pass -pedantic just to cpp;
  # now w/ no separate cpp step we should eliminate the
  # warnings and pass -pedantic to main gcc
  set(BASE_CFLAGS "-std=gnu99")
  # disable strict aliasing opt in gcc 3.3.3 -- gives warnings and makes bad assumptions
  set(BASE_CFLAGS "${BASE_CFLAGS} -fno-strict-aliasing")
  # Ubuntu defaults to -fstack-protector these days, which depends on app TLS.
  CHECK_C_COMPILER_FLAG("-fno-stack-protector" no_stack_protector_avail)
  if (no_stack_protector_avail)
    set(BASE_CFLAGS "${BASE_CFLAGS} -fno-stack-protector")
  endif (no_stack_protector_avail)
  set(ASM_FLAGS "-mmnemonic=intel -msyntax=intel -mnaked-reg --noexecstack")
  # now that the user must set non-default -mNN we don't nec. need these:
  if (X64)
    set(BASE_CFLAGS "-m64 ${BASE_CFLAGS}")
    set(BASE_CXXFLAGS "-m64")
    set(ASM_FLAGS "${ASM_FLAGS} --64")
    set(LD_FLAGS "-melf_x86_64")
  else (X64)
    set(BASE_CFLAGS "-m32 ${BASE_CFLAGS}")
    set(BASE_CXXFLAGS "-m32")
    # putting --32 last so we fail on -mmnemonic=intel on older as, not --32
    set(ASM_FLAGS "${ASM_FLAGS} --32")
    set(LD_FLAGS "-melf_i386")
  endif (X64)
  if (DEBUG)
    set(ASM_FLAGS "${ASM_FLAGS} -g")
  endif (DEBUG)
  # there's no cmake warning control so we hardcode it
  set(WARN "-Wall -Werror")
  if (NOT CMAKE_COMPILER_IS_CLANG)
    # Old gcc's ignore unknown -W flags, but -Wall -Werror causes clang to
    # complain that it doesn't recognize it.
    # Actually this is not true: gcc 4.1.2 aborts on unknown -W so we check
    CHECK_C_COMPILER_FLAG("-Wno-unused-but-set-variable" nounused_avail)
    if (nounused_avail)
      set(WARN "${WARN} -Wno-unused-but-set-variable")
    endif (nounused_avail)
  endif ()
  set(DBG "-g3")
  # gcc doesn't change its optimizations based on -g
  set(OPT "-O3 ${DBG}")

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
  #
  # We disable strcmp intrinsic to avoid stack overflow in
  # set_dynamo_options(): case 7853.
  set(DBG_OPT "-fno-omit-frame-pointer -fno-builtin-strcmp")

  set(CPP_KEEP_COMMENTS -C)
  set(CPP_NO_LINENUM -P)
  set(CPP_KEEP_WHITESPACE -traditional-cpp)
  set(CMAKE_CPP_FLAGS "")
else (UNIX)
  # FIXME: why isn't ${CMAKE_CL_NOLOGO} set?
  set(BASE_CFLAGS "/nologo")
  # build in parallel, always.
  # note that /MP is not officially supported on VS 2005 and others
  # have seen occasional problems: we'll risk it.  we could check for
  # "MSVC10 OR MSVC90".
  set(BASE_CFLAGS "${BASE_CFLAGS} /MP")
  # read-only string pooling
  set(BASE_CFLAGS "${BASE_CFLAGS} /GF")
  # FIXME case 191729: we should try to enable this.
  # Currently we get "unresolved external symbol ___security_cookie"
  set(BASE_CFLAGS "${BASE_CFLAGS} /GS-")
  # VS2010 has /MD as the default (and does declspec(dllimport) for
  # libc routines) so make sure we explicitly request /MT
  set(BASE_CFLAGS "${BASE_CFLAGS} /MT")
  set(WARN "/W4 /WX")
  # Default from cmake has /W3 so remove to avoid warning about overriding
  string(REGEX REPLACE "/W[0-9]" "" CMAKE_C_FLAGS "${CMAKE_C_FLAGS}")
  string(REGEX REPLACE "/W[0-9]" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
  if (GENERATE_PDBS)
    set(DBG "/Zi")
    set(LINK_DBG "/debug")
    set(ASM_DBG "/Zi /Zd")
  else (GENERATE_PDBS)
    # xref PR 192750 - runregression uses this to avoid over-ssh pdb issues
    set(DBG "")
    set(LINK_DBG "")
    set(ASM_DBG "")
    # Default from cmake in DEBUG and RELWITHDEBINFO has /debug
    string(REGEX REPLACE "/debug" "" CMAKE_EXE_LINKER_FLAGS
      "${CMAKE_EXE_LINKER_FLAGS}")
    string(REGEX REPLACE "/debug" "" CMAKE_MODULE_LINKER_FLAGS
      "${CMAKE_MODULE_LINKER_FLAGS}")
    string(REGEX REPLACE "/debug" "" CMAKE_SHARED_LINKER_FLAGS
      "${CMAKE_SHARED_LINKER_FLAGS}")
  endif (GENERATE_PDBS)
  # w/ cl, using DBG here won't mess up the optimizations, debug line number info
  # gets a little messed up, but is better than nothing
  set(OPT "/O2 ${DBG}")
  # do not use /O2 on windows, it messes up debug info!
  # explicitly request /Od even though it's cl's default b/c VS has /O2 as default
  # and cl 14.00 hangs compiling core/fragment.c with /Od and DEBUG.
  set(DBG_OPT "/Od")
  set(CPP_KEEP_COMMENTS /C)
  set(CPP_NO_LINENUM /EP)
  set(CPP_KEEP_WHITESPACE "")
  set(CMAKE_CPP_FLAGS "/nologo")
  set(ASM_FLAGS "/nologo ${ASM_DBG}")
  set(BASE_CXXFLAGS "${BASE_CFLAGS}")
endif (UNIX)
string(REPLACE " " ";" ASM_FLAGS_LIST "${ASM_FLAGS}")

if (UNIX)
 # i#646: cmake 2.8.5 requires ASM-ATT to find as: but we don't want to change
 # all our vars to have -ATT suffix so we set the search list instead
 set(CMAKE_ASM_COMPILER_INIT gas as)
endif (UNIX)
if (NOT "${CMAKE_GENERATOR}" MATCHES "Visual Studio")
  # CMake does not support assembly with VS generators
  # (http://public.kitware.com/Bug/view.php?id=11536)
  # so we have to add our own custom commands and targets
  enable_language(ASM)
endif ()
# note that I had to fix a bug in /usr/share/cmake/Modules/CMakeASMInformation.cmake
# where @VAR@ expansion was used, which only works for configure_file()
#   now fixed in CMake/Modules/CMakeASMInformation.cmake:1.5
# see above for our workaround
if (UNIX)
  # we used to have ".ifdef FOO" and to not have it turn into ".ifdef 1" we'd say
  # "-DFOO=FOO", but we now use exclusively preprocessor defines, which is good
  # since our defines are mostly in configure.h where we can't as easily tweak them
  # (update: I do have top-level defines gathered up in ${defines}.
  # so, we don't bother transforming -DFOO into -DFOO=FOO, nor with setting
  # up the --defsym args.
  set(CMAKE_ASM_COMPILE_OBJECT
    "${CMAKE_CPP} ${CMAKE_CPP_FLAGS} <FLAGS> <DEFINES> -E <SOURCE> -o <OBJECT>.s"
    "<CMAKE_COMMAND> -Dfile=<OBJECT>.s -P \"${PROJECT_SOURCE_DIR}/make/CMake_asm.cmake\""
    # not using <FLAGS> b/c of cmake bug #8107 where -Ddynamorio_EXPORTS
    # is passed in: we don't need the include dirs b/c of the cpp step.
    # update: Brad fixed bug #8107: moved -Ddynamorio_EXPORTS from <FLAGS> to <DEFINES> 
    # in CMake/Source/cmMakefileTargetGenerator.cxx:1.115 (will be in 2.6.4).
    #
    # we also aren't passing any <DEFINES> since for one thing
    # there's no way to transform to --defsym: luckily we don't need them
    # since using cpp now (see above).
    # FIXME: I tried setting CMAKE_ASM_DEFINE_FLAG to "--defsym " (not clear
    # how to get =1 in there :should verify it's needed) but <DEFINES>
    # comes up empty for me.
    "<CMAKE_ASM_COMPILER> ${ASM_FLAGS} -o <OBJECT> <OBJECT>.s"
    )
else (UNIX)
  # Even if we didn't preprocess we'd need our own rule since cmake doesn't
  # support ml.
  set(CMAKE_ASM_COMPILE_OBJECT
    # There's no way to specify a non-default name with /P: writes to
    # cwd/sourcebase.i.  Could copy with "cmake -E copy" but no way to
    # run get_filename_component on a tag var.  So going with
    # redirection operator which should work in all supported shells.
    #
    # ml can't handle line number markers so using ${CPP_NO_LINENUM}.
    "<CMAKE_C_COMPILER> ${CMAKE_CPP_FLAGS} <FLAGS> <DEFINES> -E ${CPP_NO_LINENUM} <SOURCE> > <OBJECT>.s"
    # cmake does add quotes in custom commands, etc. but not in this rule so we add
    # them to handle paths with spaces:
    "<CMAKE_COMMAND> -Dfile=<OBJECT>.s -P \"${PROJECT_SOURCE_DIR}/make/CMake_asm.cmake\""
    "<CMAKE_ASM_COMPILER> ${ASM_FLAGS} /c /Fo<OBJECT> <OBJECT>.s"
    )
endif (UNIX)

if (UNIX)
  # We require gas >= 2.18.50 for --32, --64, and the new -msyntax=intel, etc.
  # Since this is pretty recent we include a copy (built vs as old a glibc
  # as was convenient)
  execute_process(COMMAND
    ${CMAKE_ASM_COMPILER} --help
    RESULT_VARIABLE asm_result
    ERROR_VARIABLE asm_error
    OUTPUT_VARIABLE asm_out)
  if (asm_result OR asm_error)
    message(FATAL_ERROR "*** ${CMAKE_ASM_COMPILER} failed: ***\n${asm_error}")
  endif (asm_result OR asm_error)
  # turn the flags into a vector
  string(REGEX REPLACE " " ";" flags_needed "${ASM_FLAGS}")
  # we want "-mmnemonic=intel" to match "-mmnemonic=[att|intel]"
  string(REGEX REPLACE "=" ".*" flags_needed "${flags_needed}")
  set(flag_present 1)
  foreach (flag ${flags_needed})
    if (flag_present)
      string(REGEX MATCH "${flag}" flag_present "${asm_out}")
      if (NOT flag_present)
        message("${CMAKE_ASM_COMPILER} missing flag \"${flag}\"")
      endif (NOT flag_present)
    endif (flag_present)
  endforeach (flag)
  if (NOT flag_present)
    message("${CMAKE_ASM_COMPILER} too old: using ${PROJECT_SOURCE_DIR}/make/as-2.18.50")
    set(CMAKE_ASM_COMPILER "${PROJECT_SOURCE_DIR}/make/as-2.18.50")
  endif (NOT flag_present)

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
  # For INTERNAL builds we keep our symbols visible, for better libc backtraces
  if (NOT KEEP_SYMBOLS_FOR_LIBC_BACKTRACE)
    CHECK_C_COMPILER_FLAG("-fvisibility=internal" HAVE_FVISIBILITY_INTERNAL)
    CHECK_C_COMPILER_FLAG("-fvisibility=hidden" HAVE_FVISIBILITY_HIDDEN)

    if (HAVE_FVISIBILITY_INTERNAL)
      set(VISIBILITY "internal")
    elseif (HAVE_FVISIBILITY_HIDDEN)
      set(VISIBILITY "hidden")
    else (HAVE_FVISIBILITY_INTERNAL)
      message("${CMAKE_C_COMPILER} missing flag -fvisibility, using linker "
              "script instead")
      set(VISIBILITY " ")
    endif (HAVE_FVISIBILITY_INTERNAL)

    if (VISIBILITY)
      set(BASE_CFLAGS "${BASE_CFLAGS} -fvisibility=${VISIBILITY}")
      set(BASE_CXXFLAGS "${BASE_CXXFLAGS} -fvisibility=${VISIBILITY}")
      set(HAVE_FVISIBILITY ON)
    endif (VISIBILITY)
  endif (NOT KEEP_SYMBOLS_FOR_LIBC_BACKTRACE)

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

  # We want separate .debug files for all shared libraries
  if (NOT DEFINED CMAKE_OBJCOPY)
    find_package(BinUtils)
  endif (NOT DEFINED CMAKE_OBJCOPY)
  if (EXISTS ${CMAKE_OBJCOPY} AND EXISTS ${CMAKE_STRIP})
    # Check for --only-keep-debug support: added ~2.15
    execute_process(COMMAND
      ${CMAKE_OBJCOPY} --help
      RESULT_VARIABLE objcopy_result
      ERROR_QUIET
      OUTPUT_VARIABLE objcopy_out)
    if (objcopy_result)
      message(FATAL_ERROR "*** ${CMAKE_OBJCOPY} failed to run ***\n")
    endif (objcopy_result)
    string(REGEX MATCH "only-keep-debug" flag_present "${objcopy_out}")
    if (NOT flag_present)
      message("${CMAKE_OBJCOPY} missing flag --only-keep-debug: leaving debug info in .so files")
    else (NOT flag_present)
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
        "${CMAKE_STRIP} -g -x <TARGET>"
        )
      SET(CMAKE_C_LINK_EXECUTABLE
        "<CMAKE_C_COMPILER> <FLAGS> <CMAKE_C_LINK_FLAGS> <LINK_FLAGS> <OBJECTS>  -o <TARGET> <LINK_LIBRARIES>"
        "${CMAKE_OBJCOPY} --only-keep-debug <TARGET> <TARGET>.debug"
        "${CMAKE_OBJCOPY} --add-gnu-debuglink=<TARGET>.debug <TARGET>"
        "${CMAKE_STRIP} -g -x <TARGET>"
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
        "${CMAKE_STRIP} -g -x <TARGET>"
        )
      SET(CMAKE_CXX_LINK_EXECUTABLE
        "<CMAKE_CXX_COMPILER> <FLAGS> <CMAKE_CXX_LINK_FLAGS> <LINK_FLAGS> <OBJECTS>  -o <TARGET> <LINK_LIBRARIES>"
        "${CMAKE_OBJCOPY} --only-keep-debug <TARGET> <TARGET>.debug"
        "${CMAKE_OBJCOPY} --add-gnu-debuglink=<TARGET>.debug <TARGET>"
        "${CMAKE_STRIP} -g -x <TARGET>"
        )
    endif (NOT flag_present)
  endif (EXISTS ${CMAKE_OBJCOPY} AND EXISTS ${CMAKE_STRIP})

endif (UNIX)

# Should we be using fewer of these and using cmake's Debug vs Release?
#   Release => -O3 -NDEBUG
# Right now we only support gcc and cl but could change in future
if (DEBUG)
  set(CMAKE_C_FLAGS "${BASE_CFLAGS} ${DBG} ${DBG_OPT}")
  set(CMAKE_CXX_FLAGS "${BASE_CXXFLAGS} ${DBG} ${DBG_OPT}")
else (DEBUG)
  if (CALLPROF)
    # no opts -- we need to avoid messing up call frame walking
    # FIXME: just disable frame ptr elim opt: but /Oy- ran into some issues
    set (OPT "")
  endif (CALLPROF)
  set(CMAKE_C_FLAGS "${BASE_CFLAGS} ${OPT}")
  set(CMAKE_CXX_FLAGS "${BASE_CXXFLAGS} ${OPT}")
endif (DEBUG)
if (NOT DISABLE_WARNINGS)
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${WARN}")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${WARN}")
endif (NOT DISABLE_WARNINGS)

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
# samples are installed via api/samples/ separate CMake project
set(INSTALL_CMAKE cmake)
set(BUILD_INCLUDE "${PROJECT_BINARY_DIR}/${INSTALL_INCLUDE}")

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

# mirror install path for easier use from build dir
# most libs go in base; DR and preload go in {debug,release} subdir
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/${INSTALL_LIB_BASE}")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}")
set(DR_LIBRARY_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/${INSTALL_LIB}")
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
# 3.0 broke backcompat and 3.1 was first release so it's now the oldest compat
set(OLDEST_COMPATIBLE_VERSION "301")

# This does a copy-if-different, so it won't trigger recompilation of
# every single source file if no changes
configure_file(
  ${PROJECT_SOURCE_DIR}/make/configure.cmake.h
  ${PROJECT_BINARY_DIR}/configure.h
  )

# We want a define that's set to all the options for the C code,
# and a var set to all the options for genapi.pl.
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
# To support packaging both together we need different names
# FIXME: should we also append Debug, and have an optional param
# to DynamoRIOConfig.cmake to link to it (only really makes sense
# if using rpath)?  If we did that we'd need to set the debug
# param for our internal builds.
if (X64)
  set(exported_targets_name "DynamoRIOTarget64")
else (X64)
  set(exported_targets_name "DynamoRIOTarget32")
endif (X64)

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
if (WIN32 AND NOT X64)
  # FIXME i#62: port DRgui to 64-bit
  option(BUILD_DRGUI "build DRgui statistics viewer (requires MFC)" ON)
endif (WIN32 AND NOT X64)
option(BUILD_EXT "build Extension libraries" ON)
option(BUILD_DOCS "build documentation" ON)

if (VMSAFE)
  # I did not preserve VMSAFE docs building: to reactivate it, need to
  # add the missing transformations mentioned in api/docs/CMakeLists.txt
  # and fix genapi.pl on lib/dr_config.h to handle the outer ifdef WINDOWS
  # (ends up keeping 1st endif in 1st enum).
  set(BUILD_DOCS OFF)
endif (VMSAFE)

# should we disallow selecting samples when core is not built?
# right now the samples build will just fail
option(BUILD_SAMPLES "build client samples" ON)
if (NOT CLIENT_INTERFACE)
  set(BUILD_SAMPLES OFF)
  set(BUILD_EXT OFF)
endif (NOT CLIENT_INTERFACE)

# We include all tests and build them at configure time if requested.
# An alternative is to keep them in separate projects and only build
# when we run them via --build-and-test.
option(BUILD_TESTS "build tests" OFF)
if (BUILD_TESTS)
  # Tests require tools
  # FIXME i#65 NOT YET COMPLETED: what about x64 Windows: we need 32-bit drdeploy.exe!
  set(BUILD_TOOLS ON)
endif (BUILD_TESTS)

if (BUILD_CORE)
  if (STANDALONE_UNIT_TEST)
    enable_testing()
  endif (STANDALONE_UNIT_TEST)
  add_subdirectory(core)
  if (STANDALONE_UNIT_TEST)
    include(CTest)
  endif (STANDALONE_UNIT_TEST)
endif (BUILD_CORE)

if (BUILD_TOOLS OR BUILD_DRGUI)
  if (WIN32)
    add_subdirectory(libutil)
  endif (WIN32)
endif (BUILD_TOOLS OR BUILD_DRGUI)
  
if (BUILD_TOOLS)
  add_subdirectory(tools)
endif (BUILD_TOOLS)

if (BUILD_DRGUI)
  add_subdirectory(tools/DRgui)
endif (BUILD_DRGUI)

# create empty logs dir for running out of build dir
file(MAKE_DIRECTORY "${PROJECT_BINARY_DIR}/logs")

###########################################################################

# used by docs, samples, and tests, so here instead of in core/CMakeLists.txt
if (CLIENT_INTERFACE OR APP_EXPORTS)
  # set VERSION_NUMBER_INTEGER in dr_api.h
  configure_file(${PROJECT_SOURCE_DIR}/core/lib/dr_api.h
    ${BUILD_INCLUDE}/dr_api.h)
  add_custom_target(api_headers
    DEPENDS ${BUILD_INCLUDE}/dr_app.h
            ${BUILD_INCLUDE}/dr_api.h)
  file(MAKE_DIRECTORY ${BUILD_INCLUDE})
  # technically only the headers in the genapi.pl list
  file(GLOB header_files core/*.h core/*/*.h libutil/*.h core/hotpatch.c)
  add_custom_command(
    OUTPUT ${BUILD_INCLUDE}/dr_app.h # among other files
    DEPENDS ${PROJECT_SOURCE_DIR}/core/lib/genapi.pl
            ${header_files}
    COMMAND ${PERL_EXECUTABLE}
    ARGS ${PROJECT_SOURCE_DIR}/core/lib/genapi.pl
         -header ${BUILD_INCLUDE} "${defines} -DAPI_EXPORT_ONLY"
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}/core
    VERBATIM # recommended: p260
    )
  DR_install(DIRECTORY ${BUILD_INCLUDE} DESTINATION .)
endif (CLIENT_INTERFACE OR APP_EXPORTS)

if (CLIENT_INTERFACE OR APP_EXPORTS)
  if (BUILD_DOCS)
    find_package(ImageMagick COMPONENTS mogrify)
    if (NOT ImageMagick_mogrify_FOUND)
      # cmake 2.6.0 does not set the _FOUND vars, just the exec path,
      # and it uses all caps
      if (EXISTS ${IMAGEMAGICK_MOGRIFY_EXECUTABLE})
        set(ImageMagick_mogrify_EXECUTABLE ${IMAGEMAGICK_MOGRIFY_EXECUTABLE})
      else (EXISTS ${IMAGEMAGICK_MOGRIFY_EXECUTABLE})
        message("ImageMagick is required to build the documentation")
        set(BUILD_DOCS OFF)
      endif (EXISTS ${IMAGEMAGICK_MOGRIFY_EXECUTABLE})
    endif (NOT ImageMagick_mogrify_FOUND)
    mark_as_advanced(ImageMagick_EXECUTABLE_DIR)

    find_package(Doxygen)
    if (NOT DOXYGEN_FOUND)
      message("doxygen is required to build the documentation")
      set(BUILD_DOCS OFF)
    endif (NOT DOXYGEN_FOUND)

    include(${PROJECT_SOURCE_DIR}/make/FindFig2Dev.cmake)
    if (NOT FIG2DEV_FOUND)
      message("fig2dev (from transfig) is required to build the documentation")
      set(BUILD_DOCS OFF)
    endif (NOT FIG2DEV_FOUND)

    # fig2dev needs ghostscript to produce tiff files (not to produce eps)
    include(${PROJECT_SOURCE_DIR}/make/FindGhostscript.cmake)
    if (NOT GHOSTSCRIPT_FOUND)
      message("ghostscript is required to build the documentation")
      set(BUILD_DOCS OFF)
    else (NOT GHOSTSCRIPT_FOUND)
      # must be on path.  we can't just literally check the path
      # b/c of cygwin issues: c:/cygwin/bin vs /usr/bin, etc.
      get_filename_component(gs_name ${GHOSTSCRIPT_EXECUTABLE} NAME)
      find_program(GHOSTSCRIPT_ON_PATH ${gs_name} PATHS ENV PATH NO_DEFAULT_PATH)
      if (NOT GHOSTSCRIPT_ON_PATH)
        message("ghostscript is installed, but must be on PATH for fig2dev to find it")
        set(BUILD_DOCS OFF)
      endif (NOT GHOSTSCRIPT_ON_PATH)
      mark_as_advanced(GHOSTSCRIPT_ON_PATH)
    endif (NOT GHOSTSCRIPT_FOUND)

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
else (CLIENT_INTERFACE OR APP_EXPORTS)
  message("no documentation will be built for this target")
endif (CLIENT_INTERFACE OR APP_EXPORTS)

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
  FILE_PERMISSIONS OWNER_READ OWNER_EXECUTE GROUP_READ GROUP_EXECUTE
  WORLD_READ WORLD_EXECUTE
  FILES_MATCHING
  PATTERN "*.debug"
  PATTERN "*.pdb"
  PATTERN "dynamorio.pdb" EXCLUDE # in ${INSTALL_LIB}
  PATTERN "libdynamorio.so.*debug" EXCLUDE # in ${INSTALL_LIB}
  PATTERN "libdrpreload.so.debug" EXCLUDE # in ${INSTALL_LIB}
  PATTERN "policy_static.pdb" EXCLUDE
  )
DR_install(DIRECTORY ${DR_LIBRARY_OUTPUT_DIRECTORY}/
  DESTINATION ${INSTALL_LIB}
  FILE_PERMISSIONS OWNER_READ OWNER_EXECUTE GROUP_READ GROUP_EXECUTE
  WORLD_READ WORLD_EXECUTE
  FILES_MATCHING
  PATTERN "dynamorio.pdb"
  PATTERN "libdynamorio.so.*debug" 
  PATTERN "libdrpreload.so.debug" 
  )
if (BUILD_TOOLS OR WIN32)
  DR_install(DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/
    DESTINATION ${INSTALL_BIN}
    FILE_PERMISSIONS OWNER_READ OWNER_EXECUTE GROUP_READ GROUP_EXECUTE
    WORLD_READ WORLD_EXECUTE
    FILES_MATCHING
    PATTERN "*.debug"
    PATTERN "*.pdb"
    PATTERN "runstats.debug" EXCLUDE # since not installing tool
    PATTERN "run_in_bg.debug" EXCLUDE # since not installing tool
    )
endif (BUILD_TOOLS OR WIN32)

if (DO_DR_INSTALL AND BUILD_DOCS)
  # cannot use DR_install because it breaks at ; 
  install(CODE "file(WRITE \"\${CMAKE_INSTALL_PREFIX}/docs/DynamoRIO.html\" \"<html>\n<head>\n<meta http-equiv=\\\"refresh\\\" content=\\\"0; URL=./html/index.html\\\">\n</head><body></body>\")")
endif (DO_DR_INSTALL AND BUILD_DOCS)

# create empty logs dir for release package
# be sure to escape ",$ since evaluated at install time not configure time
# with DR_install macro we need double-escape for $
DR_install(CODE "file(MAKE_DIRECTORY \"\\\${CMAKE_INSTALL_PREFIX}/logs\")")
# CPack seems to ignore empty dirs so add a README file
DR_install(CODE "file(WRITE \"\\\${CMAKE_INSTALL_PREFIX}/logs/README\" \"Empty dir for debug-build log files.\n\")")

###########################################################################

# create CMake configuration files for clients and tests to use
# we also create cmake/DynamoRIOTarget.cmake in core/CMakeLists.txt
configure_file(
  ${PROJECT_SOURCE_DIR}/make/DynamoRIOConfig.cmake.in
  ${PROJECT_BINARY_DIR}/cmake/DynamoRIOConfig.cmake
  @ONLY)
configure_file(
  ${PROJECT_SOURCE_DIR}/make/DynamoRIOConfigVersion.cmake.in
  ${PROJECT_BINARY_DIR}/cmake/DynamoRIOConfigVersion.cmake
  @ONLY)
DR_install(FILES
  ${PROJECT_BINARY_DIR}/cmake/DynamoRIOConfig.cmake
  ${PROJECT_BINARY_DIR}/cmake/DynamoRIOConfigVersion.cmake
  DESTINATION ${INSTALL_CMAKE}
)
# export suite scripts even if not building tests
DR_install(FILES
  ${PROJECT_SOURCE_DIR}/suite/runsuite_common_pre.cmake
  ${PROJECT_SOURCE_DIR}/suite/runsuite_common_post.cmake
  DESTINATION ${INSTALL_CMAKE}
)

# Extensions (i#277/PR 540817)
if (BUILD_EXT)
  add_subdirectory(ext)
endif (BUILD_EXT)

if (BUILD_SAMPLES OR BUILD_TESTS)
  # api/samples is set up to function both as a separate project for
  # building samples vs a DynamoRIO installation, and to allow
  # building as a component of the whole source tree
  set(DynamoRIO_INTERNAL ON) # do not import dynamorio lib target
  set(DynamoRIO_DIR ${PROJECT_BINARY_DIR}/cmake)
endif (BUILD_SAMPLES OR BUILD_TESTS)

if (BUILD_SAMPLES)
  add_subdirectory(api/samples)
endif (BUILD_SAMPLES)

if (BUILD_TESTS)
  enable_testing()
  add_subdirectory(suite/tests)
  # add Dashboard support
  include(CTest)
endif (BUILD_TESTS)

# Create the exported targets file.  This will include all targets added
# in subdirectories via DR_install(TARGETS ... EXPORT ${exported_targets_name}).
DR_install(EXPORT ${exported_targets_name} DESTINATION ${INSTALL_CMAKE})

###########################################################################
# packaging

# For building a full release package, we rely on an external script to
# build multiple configurations ({release,debug}x{32-bit,64-bit}) with a
# shared install tree.  In the final build, if an appropriate
# CPACK_INSTALL_CMAKE_PROJECTS variable is set to point at all 4 builds,
# "make package" will produce a package from all 4 installs.

# FIXME i#74: create source tarball via 'make package_source' 
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
  if (VMKERNEL)
    set(CPACK_SYSTEM_NAME "ESXi")
  else (VMKERNEL)
    set(CPACK_SYSTEM_NAME "Linux")
  endif (VMKERNEL)
else (UNIX)
  # FIXME: try Windows NSIS
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
set(CPACK_PACKAGE_FILE_NAME
  "DynamoRIO-${CPACK_SYSTEM_NAME}-${CPACK_PACKAGE_VERSION}-${BUILD_NUMBER}")
set(CPACK_PACKAGE_INSTALL_DIRECTORY "dynamorio")
set(CPACK_PACKAGE_INSTALL_REGISTRY_KEY "DynamoRIO")
set(CPACK_PACKAGE_RELOCATABLE "true")

include(CPack)
