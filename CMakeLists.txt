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

###########################################################################
# FIXME not yet finished:
# i#65: convert suite/ to cmake and ctest
# i#64: re-add "make review"
# i#66: re-add "make runregression"
# i#73: provide automatic suport for locating ml64.exe when using Vista SDK+WDK
# 
# future nice-to-have work:
# i#74: create source tarball via 'make package_source' 
# i#68: replace perl with cmake scripts and CTest
# i#69: re-add support for core unit tests and standalone build rules
# i#70: symbol store support in core/Makefile and tools/Makefile
# i#71: support mingw/cygwin makefile cmake target with cl compiler, for make -j
# i#75: convert clients/standalone/Makefile to CMake 
#
# Not filed:
# * move info like "build core solely from DDK" from compiler.mk into
#   HowToBuild.wiki
###########################################################################

cmake_minimum_required(VERSION 2.6)
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

project(DynamoRIO)

###########################################################################
# configuration options

# many are core-specific

# I want to override the default CMAKE_INSTALL_PREFIX, but keep
# the name of the var so CPack and other standard tools work.
# The best I can come up with is to disallow installing to the
# default location, which seems reasonable.
# If we didn't use standard tools we could set CMAKE_INSTALL_PREFIX
# to be CACHE INTERNAL FORCE to INSTALL_PREFIX.
if (UNIX)
  set(default_install "/usr/local")
else (UNIX)
  set(default_install "$ENV{SYSTEMDRIVE}/Program Files")
endif (UNIX)
if ("${CMAKE_INSTALL_PREFIX}" MATCHES "^${default_install}")
  set(CMAKE_INSTALL_PREFIX "${PROJECT_SOURCE_DIR}/exports" 
    CACHE PATH "install path" FORCE)
endif ("${CMAKE_INSTALL_PREFIX}" MATCHES "^${default_install}")
set(INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}")

# configurations that also have defines
option(DEBUG "build with asserts and logging enabled")
option(INTERNAL "for developer use")
option(DRGUI_DEMO "build DRgui with a giant font for projecting")

# target
if (UNIX)
  set(LINUX 1)
endif (UNIX)
if (WIN32)
  set(WINDOWS 1)
endif (WIN32)

if (CMAKE_SIZEOF_VOID_P EQUAL 8)
  set(X64_DEFAULT ON)
else(CMAKE_SIZEOF_VOID_P EQUAL 8)
  set(X64_DEFAULT OFF)
endif (CMAKE_SIZEOF_VOID_P EQUAL 8)
if (UNIX)
  option(X64 "target 64-bit" ${X64_DEFAULT})
else (UNIX)
  # not controllable from here: must change env vars before invoking CMake
  set(X64 ${X64_DEFAULT})
endif (UNIX)

option(VMKERNEL "target VMkernel")
option(VMKCLASSIC "target VMkernel")
option(MACOS "target MacOS")

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

# for developers
option(KSTATS "internal kstat profiling")
option(CALLPROF "internal caller profiling support")
option(STANDALONE_UNIT_TEST "internal unit tests")
option(KEEP_SYMBOLS_FOR_LIBC_BACKTRACE "keep internal symbols exported")
if (INTERNAL OR DEBUG)
  set(KSTATS 1)
endif (INTERNAL OR DEBUG)
if (INTERNAL AND DEBUG)
  set(KEEP_SYMBOLS_FOR_LIBC_BACKTRACE 1)
endif (INTERNAL AND DEBUG)
# no KSTATS for caller profiling: we want to be as close to release
# build as we can, but w/o optimizations
if (CALLPROF)
  set(KSTATS 0FF)
endif (CALLPROF)
if (WIN32)
  # xref PR 192750 - runregression uses this to avoid over-ssh pdb issues
  option(GENERATE_PDBS "generate Windows debug information" ON)
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
  VMKCLASSIC
  MACOS
  VMAP
  VMSAFE
  VPS
  KSTATS
  CALLPROF
  STANDALONE_UNIT_TEST
  KEEP_SYMBOLS_FOR_LIBC_BACKTRACE
  SET_PREFERRED_BASE
  preferred_base
  GENERATE_PDBS
  DRGUI_DEMO
  )
# replaced by our own DEBUG, INTERNAL, etc.
# FIXME: or should we support and map these to ours?
# if we do support we'll need to remove flags like /MD coming from
# the CMAKE_C_FLAGS_*, which are added to the base CMAKE_C_FLAGS.
set(CMAKE_BUILD_TYPE "" CACHE INTERNAL
  "build type: unused" FORCE)

# resources when packaging
# xref cmake's supported VERSION + SOVERSION which we're not using
set(VERSION_NUMBER "1.3.2" CACHE STRING "Version number")
string(REGEX REPLACE "\\." "," VERSION_COMMA_DELIMITED "${VERSION_NUMBER}")
set(BUILD_NUMBER "18" CACHE STRING "Build number (must be <64K)")
set(UNIQUE_BUILD_NUMBER "18" CACHE STRING "Unique build number")
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

###########################################################################
# toolchain

if (UNIX)

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
  if (cpp-NOTFOUND OR NOT EXISTS "${CMAKE_CPP}")
    message(FATAL_ERROR "cpp is required to build")
  endif (cpp-NOTFOUND OR NOT EXISTS "${CMAKE_CPP}")
  mark_as_advanced(CMAKE_CPP)

else (UNIX)

  if (NOT ${COMPILER_BASE_NAME} STREQUAL "cl")
    # we use cl pragmas and intrinsics
    message(FATAL_ERROR "cl (Microsoft C++ compiler) is required to build")
  endif (NOT ${COMPILER_BASE_NAME} STREQUAL "cl")
  set(CMAKE_CPP ${CMAKE_C_COMPILER})

  # cmake doesn't even look for ml!  should file a cmake bug
  get_filename_component(cl_path ${CMAKE_C_COMPILER} PATH)
  if (X64)
    find_program(CMAKE_ASM_COMPILER ml64.exe HINTS "${cl_path}" DOC "path to assembler")
  else (X64)
    find_program(CMAKE_ASM_COMPILER ml.exe HINTS "${cl_path}" DOC "path to assembler")
  endif (X64)
  if (CMAKE_ASM_COMPILER-NOTFOUND OR NOT EXISTS "${CMAKE_ASM_COMPILER}")
    message(FATAL_ERROR "assembler not found: required to build")
  endif (CMAKE_ASM_COMPILER-NOTFOUND OR NOT EXISTS "${CMAKE_ASM_COMPILER}")

  # cmake has CMAKE_RC_COMPILER, but no message compiler
  find_program(CMAKE_MC_COMPILER mc.exe DOC "path to message compiler")
  if (CMAKE_MC_COMPILER-NOTFOUND OR NOT EXISTS "${CMAKE_MC_COMPILER}")
    message(FATAL_ERROR "message compiler not found: required to build")
  endif (CMAKE_MC_COMPILER-NOTFOUND OR NOT EXISTS "${CMAKE_MC_COMPILER}")
  message(STATUS "Found message compiler: ${CMAKE_MC_COMPILER}")
  mark_as_advanced(CMAKE_MC_COMPILER)

  # Note that we want the oldest ntdll.lib for maximum compatibility.
  # The 2003 DDK (3790.1830) or Vista WDK (6000) both have these paths:
  if (X64)
    set(NTDLL_LIB_SUBPATH lib/wnet/amd64)
  else (X64)
    set(NTDLL_LIB_SUBPATH lib/w2k/i386)
  endif (X64)
  # DDKROOT env var is recommended method in our docs
  if (NOT "$ENV{DDKROOT}" STREQUAL "")
    set(DDK_ROOT "$ENV{DDKROOT}")
  # Check default install path
  elseif (EXISTS "$ENV{SYSTEMDRIVE}/WINDDK/3790.1830/")
    set(DDK_ROOT "$ENV{SYSTEMDRIVE}/WINDDK/3790.1830/")
  elseif (EXISTS "$ENV{SYSTEMDRIVE}/WINDDK/6000/")
    set(DDK_ROOT "$ENV{SYSTEMDRIVE}/WINDDK/6000/")
  endif (NOT "$ENV{DDKROOT}" STREQUAL "")
  # Use find_library() and not find_path() in case on LIB env var or sthg
  find_library(NTDLL_LIBPATH ntdll.lib HINTS "${DDK_ROOT}/${NTDLL_LIB_SUBPATH}"
    DOC "Path to ntdll.lib from either 2003 DDK (3790.1830) or Vista WDK (6000)")
  if (NTDLL_LIBPATH-NOTFOUND OR NOT EXISTS "${NTDLL_LIBPATH}")
    message(FATAL_ERROR "ntdll.lib not found: required to build")
  endif (NTDLL_LIBPATH-NOTFOUND OR NOT EXISTS "${NTDLL_LIBPATH}")
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
  CHECK_TYPE_SIZE(int64 DR_DO_NOT_DEFINE_int32)
  CHECK_TYPE_SIZE(int64 DR_DO_NOT_DEFINE_int64)
  # we could do CHECK_SYMBOL_EXISTS for MAX and MIN but they're not
  # in standard headers so up to user to define if an issue

  # FIXME: -iquote: no way to do it in cmake
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
  set(ASM_FLAGS "-mmnemonic=intel -msyntax=intel -mnaked-reg --noexecstack")
  if (X64)
    set(BASE_CFLAGS "-m64 ${BASE_CFLAGS}")
    set(ASM_FLAGS "${ASM_FLAGS} --64")
    set(LD_FLAGS "-melf_x86_64")
  else (X64)
    set(BASE_CFLAGS "-m32 ${BASE_CFLAGS}")
    # putting --32 last so we fail on -mmnemonic=intel on older as, not --32
    set(ASM_FLAGS "${ASM_FLAGS} --32")
    set(LD_FLAGS "-melf_i386")
  endif (X64)
  # there's no cmake warning control so we hardcode it
  set(WARN "-Wall -Werror")
  set(OPT "-O3")
  set(DBG "-g3")
  # FIXME: core-specific: move to core/CMakeLists.txt?
  # We use -O to avoid stack overflow in debug build (makes a huge difference by
  # having locals/temps share stack slots) but we use -fno-omit-frame-pointer to
  # keep things more easily debuggable (though gcc claims only omits when
  # debuggable).
  # We disable strcmp intrinsic to avoid stack overflow in
  # set_dynamo_options(): case 7853.
  # FIXME: now that we have -O we may not need -fno-builtin-strcmp
  set(DBG_OPT "-O -fno-omit-frame-pointer -fno-builtin-strcmp")
  set(CPP_KEEP_COMMENTS -C)
  set(CPP_NO_LINENUM -P)
  set(CMAKE_CPP_FLAGS "")
else (UNIX)
  # FIXME: why isn't ${CMAKE_CL_NOLOGO} set?
  set(BASE_CFLAGS "/nologo")
  # read-only string pooling
  set(BASE_CFLAGS "${BASE_CFLAGS} /GF")
  # FIXME case 191729: we should try to enable this.
  # Currently we get "unresolved external symbol ___security_cookie"
  set(BASE_CFLAGS "${BASE_CFLAGS} /GS-")
  set(WARN "/W4 /WX")
  # Default from cmake has /W3 so remove to avoid warning about overriding
  string(REGEX REPLACE "/W3" "" CMAKE_C_FLAGS "${CMAKE_C_FLAGS}")
  if (GENERATE_PDBS)
    set(DBG "/Zi")
    set(LINK_DBG "/debug")
  else (GENERATE_PDBS)
    # xref PR 192750 - runregression uses this to avoid over-ssh pdb issues
    set(DBG "")
    set(LINK_DBG "")
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
  set(DBG_OPT "")
  set(CPP_KEEP_COMMENTS /C)
  set(CPP_NO_LINENUM /EP)
  set(CMAKE_CPP_FLAGS "/nologo")
  set(ASM_FLAGS "/nologo")
endif (UNIX)

enable_language(ASM)
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
    "<CMAKE_COMMAND> -Dfile=<OBJECT>.s -P ${PROJECT_SOURCE_DIR}/make/CMake_asm.cmake"
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
    "<CMAKE_COMMAND> -Dfile=<OBJECT>.s -P ${PROJECT_SOURCE_DIR}/make/CMake_asm.cmake"
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
  # I tried cmake's CheckCCompilerFlag but it doesn't seem to work
  execute_process(COMMAND
    ${CMAKE_C_COMPILER} -v --help
    RESULT_VARIABLE gcc_result
    ERROR_QUIET
    OUTPUT_VARIABLE gcc_out)
  if (gcc_result)
    message(FATAL_ERROR "*** ${CMAKE_C_COMPILER} failed to run ***\n")
  endif (gcc_result)
  string(REGEX MATCH "fvisibility" flag_present "${gcc_out}")
  if (NOT flag_present)
    message("${CMAKE_C_COMPILER} missing flag -fvisibility, using linker script instead")
    set(HAVE_FVISIBILITY OFF)
  else (NOT flag_present)
    set(HAVE_FVISIBILITY ON)
  endif (NOT flag_present)
  # For INTERNAL builds we keep our symbols visible, for better libc backtraces
  if (HAVE_FVISIBILITY AND NOT KEEP_SYMBOLS_FOR_LIBC_BACKTRACE)
    # PR 262458: for gcc >= 3.4 we can use -fvisibility instead of a
    # linker version script.  Note that we're still using a linker
    # script to set our preferred base (PR 253624) and will need to
    # for section ordering as well (PR 208267) but those are separate
    # scripts.
    # Only export functions so marked via attributes
    # (For older gcc we use an ugly linker script and an auto-generated export list)
    # Using "internal" instead of "hidden" b/c we don't need any indirect
    # calls to our non-exported functions
    set(BASE_CFLAGS "${BASE_CFLAGS} -fvisibility=internal")
  endif (HAVE_FVISIBILITY AND NOT KEEP_SYMBOLS_FOR_LIBC_BACKTRACE)

  # Better to use -dT when passing linker options through gcc, but ld
  # prior to 2.18 only supports -T
  # FIXME: should we duplicate this in DynamoRIOUse.cmake?
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
        # removed, so we only strip debug (-g) and local (-x), and preserve
        # file attributes (-p):
        "${CMAKE_STRIP} -g -x -p <TARGET>"
        )
      SET(CMAKE_C_LINK_EXECUTABLE
        "<CMAKE_C_COMPILER> <FLAGS> <CMAKE_C_LINK_FLAGS> <LINK_FLAGS> <OBJECTS>  -o <TARGET> <LINK_LIBRARIES>"
        "${CMAKE_OBJCOPY} --only-keep-debug <TARGET> <TARGET>.debug"
        "${CMAKE_OBJCOPY} --add-gnu-debuglink=<TARGET>.debug <TARGET>"
        "${CMAKE_STRIP} -g -x -p <TARGET>"
        )
    endif (NOT flag_present)
  endif (EXISTS ${CMAKE_OBJCOPY} AND EXISTS ${CMAKE_STRIP})

endif (UNIX)

# Should we be using fewer of these and using cmake's Debug vs Release?
#   Release => -O3 -NDEBUG
# Right now we only support gcc and cl but could change in future
if (DEBUG)
  set(CMAKE_C_FLAGS "${BASE_CFLAGS} ${DBG} ${DBG_OPT}")
else (DEBUG)
  if (CALLPROF)
    # no opts -- we need to avoid messing up call frame walking
    # FIXME: just disable frame ptr elim opt: but /Oy- ran into some issues
    set (OPT "")
  endif (CALLPROF)
  set(CMAKE_C_FLAGS "${BASE_CFLAGS} ${OPT}")
endif (DEBUG)
if (NOT DISABLE_WARNINGS)
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${WARN}")
endif (NOT DISABLE_WARNINGS)

###########################################################################

# use a single path for easier installation of pdbs
set(LIBRARY_OUTPUT_PATH ${PROJECT_BINARY_DIR}/lib)
set(EXECUTABLE_OUTPUT_PATH ${PROJECT_BINARY_DIR}/bin)

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
set(BUILD_INCLUDE ${PROJECT_BINARY_DIR}/${INSTALL_INCLUDE})

configure_file(
  ${PROJECT_SOURCE_DIR}/make/configure.cmake.h
  ${PROJECT_BINARY_DIR}/configure.h
  )

# We want a define that's set to all the options for the C code,
# and a var set to all the options for genapi.pl.
# Strategy: add -D<define> token after each #define, and then run
# cpp => left with just the -D<define> that are defined.
file(READ ${PROJECT_BINARY_DIR}/configure.h configure_h)
string(REGEX REPLACE
  "(# *define *)([^_][^ \r\n]+)( *\r?\n)"
  "\\1\\2\\3-D\\2\n" configure_h_mod "${configure_h}")
set(defines_tmpfile ${PROJECT_BINARY_DIR}/configure_temp.h)
set_directory_properties(PROPERTIES
  ADDITIONAL_MAKE_CLEAN_FILES "${defines_tmpfile}")
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
string(REGEX REPLACE
  "(#endif /\\* _CONFIGURE_H_ \\*/)"
  "#define DYNAMORIO_DEFINES \"${defines}\"\n\n\\1"
  configure_h "${configure_h}")
file(WRITE ${PROJECT_BINARY_DIR}/configure.h "${configure_h}")

include_directories(BEFORE 
  ${PROJECT_BINARY_DIR} # for configure.h
  )

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
if (WIN32 AND X64)
  message("Currently 64-bit Windows tools (other than drinject) are not supported (but 32-bit drdeploy can configure 64-bit applications)")
endif (WIN32 AND X64)
if (NOT WIN32 OR NOT X64)
  # FIXME i#28: port tools to 64-bit
  # FIXME i#28: port libutil to 64-bit
  option(BUILD_TOOLS "build tools" ON)
endif (NOT WIN32 OR NOT X64)
if (WIN32 AND NOT X64)
  # FIXME i#62: port DRgui to 64-bit
  # FIXME i#28: port libutil to 64-bit
  option(BUILD_DRGUI "build DRgui statistics viewer (requires MFC)" ON)
endif (WIN32 AND NOT X64)
option(BUILD_DOCS "build documentation" ON)

if (BUILD_CORE)
  add_subdirectory(core)
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

# used by docs, so here instead of in core/CMakeLists.txt
if (CLIENT_INTERFACE OR APP_EXPORTS)
  add_custom_target(api_headers ALL
    DEPENDS ${BUILD_INCLUDE}/dr_app.h)
  file(MAKE_DIRECTORY ${BUILD_INCLUDE})
  # technically only the headers in the genapi.pl list
  file(GLOB header_files *.h */*.h)
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
  install(DIRECTORY ${BUILD_INCLUDE} DESTINATION .)
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
      if (NOT EXISTS ${GHOSTSCRIPT_ON_PATH})
        message("ghostscript is installed, but must be on PATH for fig2dev to find it")
        set(BUILD_DOCS OFF)
      endif (NOT EXISTS ${GHOSTSCRIPT_ON_PATH})
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

install(FILES
  ${PROJECT_SOURCE_DIR}/README 
  ${PROJECT_SOURCE_DIR}/License.txt 
  ${PROJECT_SOURCE_DIR}/ACKNOWLEDGEMENTS
  DESTINATION .)

# We put all our libs and exes in common dirs, making it easier
# to copy our symbols.  It's a pain to construct the pdb name
# counterparts for targets, so we use the install(DIRECTORY) commands
# where we can use wildcards.
# Slightly ugly to have hardcoded names: long-term CMake should
# support auto-installing pdb files.
install(DIRECTORY ${LIBRARY_OUTPUT_PATH}/
  DESTINATION ${INSTALL_LIB_BASE}
  FILE_PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE
  FILES_MATCHING
  PATTERN "*.debug"
  PATTERN "*.pdb"
  PATTERN "dynamorio.pdb" EXCLUDE # in ${INSTALL_LIB}
  PATTERN "libdynamorio.so.debug" EXCLUDE # in ${INSTALL_LIB}
  PATTERN "libdrpreload.so.debug" EXCLUDE # in ${INSTALL_LIB}
  PATTERN "policy_static.pdb" EXCLUDE
  )
install(DIRECTORY ${LIBRARY_OUTPUT_PATH}/
  DESTINATION ${INSTALL_LIB}
  FILE_PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE
  FILES_MATCHING
  PATTERN "dynamorio.pdb"
  PATTERN "libdynamorio.so.debug" 
  PATTERN "libdrpreload.so.debug" 
  )
if (BUILD_TOOLS OR WIN32)
  install(DIRECTORY ${EXECUTABLE_OUTPUT_PATH}/
    DESTINATION ${INSTALL_BIN}
    FILE_PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE
    FILES_MATCHING
    PATTERN "*.debug"
    PATTERN "*.pdb"
    PATTERN "runstats.debug" EXCLUDE # since not installing runstats
    )
endif (BUILD_TOOLS OR WIN32)

###########################################################################

# create CMake configuration files for clients to use
configure_file(
  ${PROJECT_SOURCE_DIR}/make/DynamoRIOConfig.cmake.in
  ${PROJECT_BINARY_DIR}/DynamoRIOConfig.cmake)
install(FILES
  ${PROJECT_SOURCE_DIR}/make/DynamoRIOUse.cmake
  ${PROJECT_BINARY_DIR}/DynamoRIOConfig.cmake
  DESTINATION ${INSTALL_CMAKE}
)

# should we disallow selecting samples when core is not built?
# right now the samples build will just fail
option(BUILD_SAMPLES "build client samples" ON)
if (BUILD_SAMPLES)
  # api/samples is set up to function both as a separate project for
  # building samples vs a DynamoRIO installation, and to allow
  # building as a component of the whole source tree, since
  # DynamoRIOUse.cmake will not override already-defined vars:
  set(DynamoRIO_DIR ${PROJECT_BINARY_DIR})
  set(DynamoRIO_INCLUDE_DIRS "${PROJECT_BINARY_DIR}/include")
  set(DynamoRIO_LIBRARY32_REL_DIRS "${LIBRARY_OUTPUT_PATH}")
  set(DynamoRIO_LIBRARY32_DBG_DIRS "${LIBRARY_OUTPUT_PATH}")
  set(DynamoRIO_LIBRARY64_REL_DIRS "${LIBRARY_OUTPUT_PATH}")
  set(DynamoRIO_LIBRARY64_DBG_DIRS "${LIBRARY_OUTPUT_PATH}")
  set(DynamoRIO_USE_FILE "${PROJECT_SOURCE_DIR}/make/DynamoRIOUse.cmake")
  add_subdirectory(api/samples)
endif (BUILD_SAMPLES)

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
  set(CPACK_SYSTEM_NAME "Linux")
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
string(REGEX REPLACE 
  "^([0-9]+)\\..*" "\\1" CPACK_PACKAGE_VERSION_MAJOR "${VERSION_NUMBER}")
string(REGEX REPLACE 
  "^[0-9]+\\.([0-9]+)\\..*" "\\1" CPACK_PACKAGE_VERSION_MINOR "${VERSION_NUMBER}")
string(REGEX REPLACE 
  "^[0-9]+\\.[0-9]+\\.([0-9]+)" "\\1" CPACK_PACKAGE_VERSION_PATCH "${VERSION_NUMBER}")

# CPack tarballs do not allow setting a different name for the base
# directory and the file: I tried a ton of CPack variables for "install
# dir" and looked at the source code.  Most of the variables are for the
# other installers (rpm, nsis).  I can hack it via
# CPACK_TEMPORARY_PACKAGE_FILE_NAME if I hardcode the exentsion: but maybe
# having the full version in the base dir is a good thing, though I'm not
# sure about the caps.
set(CPACK_PACKAGE_FILE_NAME "DynamoRIO-${CPACK_SYSTEM_NAME}-${CPACK_PACKAGE_VERSION}")
set(CPACK_PACKAGE_INSTALL_DIRECTORY "dynamorio")
set(CPACK_PACKAGE_INSTALL_REGISTRY_KEY "DynamoRIO")
set(CPACK_PACKAGE_RELOCATABLE "true")

include(CPack)
