# **********************************************************
# Copyright (c) 2010-2022 Google, Inc.    All rights reserved.
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

# symbol access library

cmake_minimum_required(VERSION 3.7)

include(../../make/policies.cmake NO_POLICY_SCOPE)

if (X64)
  set(BITS "64")
else ()
  set(BITS "32")
endif ()

if (DR_HOST_ARM)
  if (ANDROID)
    set(ARCH "-android")
  else ()
    set(ARCH "-arm")
    # XXX i#1501: to support both arm-linux-gnueabi and arm-linux-gnueabi,
    # we rely on CMAKE_C_LIBRARY_ARCHITECTURE for libelftc libraries selection.
    # If it is not set by some system, users need manually set it to gnueabi
    # for using gnueabi build of libelftc libraries.
    if (CMAKE_C_LIBRARY_ARCHITECTURE MATCHES "gnueabi$")
      set(BITS "${BITS}-eabi")
    else ()
      set(BITS "${BITS}-eabihf")
    endif ()
  endif ()
endif()

if (DR_HOST_AARCH64)
  set(ARCH "-aarch64")
endif ()

if (DR_HOST_RISCV64)
  set(ARCH "-riscv64")
endif ()

# we need libc b/c our elftoolchain libraries use it
set(DynamoRIO_USE_LIBC ON)

# We use our own .lib file to support VS2005 whose dbghelp.lib doesn't have some
# routines we want to use.
if (WIN32)
  # XXX: if we add any more of these .lib files we should share this code
  # (currently we have make/ntdll_imports.cmake and here).
  find_program(LIB_EXECUTABLE lib.exe HINTS "${cl_path}" DOC "path to lib.exe")
  if (NOT LIB_EXECUTABLE)
    message(FATAL_ERROR "Cannot find lib.exe")
  endif (NOT LIB_EXECUTABLE)
  set(dbghelp_src "${CMAKE_CURRENT_SOURCE_DIR}/dbghelp_imports.c")
  set(dbghelp_def "${CMAKE_CURRENT_SOURCE_DIR}/dbghelp_imports.def")
  # We need a different name so we can also use the VS dbghelp.lib
  set(dbghelp_lib "${CMAKE_CURRENT_BINARY_DIR}/dbghelp_imports.lib")
  set_property(SOURCE "${dbghelp_lib}" PROPERTY GENERATED true)
  # Because the exports are stdcall we can't just use a .def file: we need
  # an .obj file built from stubs w/ the same signatures.
  # We don't need a stamp file b/c the .lib is not a source; plus a stamp
  # file causes Ninja to fail to build.
  add_custom_command(OUTPUT "${dbghelp_lib}"
    DEPENDS "${dbghelp_src}" "${dbghelp_def}"
    COMMAND "${CMAKE_C_COMPILER}" ARGS /nologo /c /Ob0 ${dbghelp_src}
    COMMAND "${LIB_EXECUTABLE}" ARGS
      /nologo /name:dbghelp.dll /def:${dbghelp_def}
      ${CMAKE_CURRENT_BINARY_DIR}/dbghelp_imports.obj
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    VERBATIM # recommended: p260 of cmake book
    )
  if ("${CMAKE_GENERATOR}" MATCHES "Visual Studio")
    set(dbghelp_dep "")
    # for correct parallel builds we need a target to avoid
    # duplicate and racy VS per-project rules
    add_custom_target(dbghelp_tgt DEPENDS "${dbghelp_lib}")
  else ()
    set(dbghelp_dep "${dbghelp_lib}")
  endif ()
  set(dbghelp_flags "${dbghelp_lib}")

  set(srcs
    drsyms_windows.c drsyms_unix_common.c drsyms_pecoff.c
    drsyms_dwarf.c demangle.cc drsyms_common.c
    ${dbghelp_dep})

  # i#1491#2: VS generators fail if static lib has resources
  set(srcs_static ${srcs})
  set(srcs ${srcs} ${PROJECT_SOURCE_DIR}/core/win32/resources.rc)

  set(dwarf_dir "${PROJECT_SOURCE_DIR}/ext/drsyms/libelftc-pecoff/lib${BITS}")
  set(dwarf_libpath "${dwarf_dir}/dwarf.lib")
  set(elftc_libpath "${dwarf_dir}/elftc.lib")
  configure_file("${dwarf_dir}/dwarf.pdb"
    "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/dwarf.pdb" COPYONLY)
  configure_file("${dwarf_dir}/elftc.pdb"
    "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/elftc.pdb" COPYONLY)

elseif (UNIX)
  set(srcs
    drsyms_unix_frontend.c drsyms_unix_common.c
    drsyms_dwarf.c demangle.cc drsyms_common.c)
  if (APPLE)
    set(srcs ${srcs} drsyms_macho.c)
    set(dwarf_libpath
      "${PROJECT_SOURCE_DIR}/ext/drsyms/libelftc-macho${ARCH}/lib${BITS}/libdwarf.a")
    set(elftc_libpath
      "${PROJECT_SOURCE_DIR}/ext/drsyms/libelftc-macho${ARCH}/lib${BITS}/libelftc.a")
  else (APPLE)
    set(srcs ${srcs} drsyms_elf.c)
    set(dwarf_libpath
      "${PROJECT_SOURCE_DIR}/ext/drsyms/libelftc${ARCH}/lib${BITS}/libdwarf.a")
    set(elftc_libpath
      "${PROJECT_SOURCE_DIR}/ext/drsyms/libelftc${ARCH}/lib${BITS}/libelftc.a")
    set(elf_libpath
      "${PROJECT_SOURCE_DIR}/ext/drsyms/libelftc${ARCH}/lib${BITS}/libelf.a")
  endif (APPLE)
  set(srcs_static ${srcs})
endif (WIN32)

# while private loader means preferred base is not required, more efficient
# to avoid rebase so we avoid conflict w/ client and other exts
set(PREFERRED_BASE 0x76000000)

add_library(drsyms SHARED ${srcs})
configure_extension(drsyms OFF)

macro(configure_drsyms_target target)
  if (WIN32)
    target_link_libraries(${target} dbghelp ${dbghelp_flags})
    if ("${CMAKE_GENERATOR}" MATCHES "Visual Studio")
      # for parallel build correctness we need a target dependence
      add_dependencies(${target} dbghelp_tgt)
    endif ()
  endif ()
  # we always use the elftoolchain library when building with cmake
  append_property_list(TARGET ${target} COMPILE_DEFINITIONS "DRSYM_HAVE_LIBELFTC")
endmacro(configure_drsyms_target)

configure_drsyms_target(drsyms)

use_DynamoRIO_extension(drsyms drcontainers)

include_directories("${PROJECT_SOURCE_DIR}/ext/drsyms/libelftc/include")

# We require DynamoRIO_USE_LIBC, as there's no simple automated
# solution to get clients using a static drsyms to omit /noentry and
# link with libcmt.lib prior to linking w/ DR, unless we change
# configure_DynamoRIO_client() to take in a list of all extensions
# that will be used -- but we now have DynamoRIO_USE_LIBC ON by
# default.

# Listing the dependent libs as sources does get them combined into
# drsyms static lib for pre-VS2010, but VS2010 won't do that, so we go
# with exported separate libs to match Linux (alternative would be a custom
# command to combine via lib.exe).
add_library(drsyms_static STATIC ${srcs_static})
configure_extension(drsyms_static ON)
configure_drsyms_target(drsyms_static)
use_DynamoRIO_extension(drsyms_static drcontainers)

target_link_libraries(drsyms dwarf elftc)
if (LINUX)
  target_link_libraries(drsyms elf)
endif ()
# i#693: CMake will try to export the path to the static libs we use via
# IMPORTED_LINK_INTERFACE_LIBRARIES_NOCONFIG, but they won't exist on the
# user's machine.  Clearing this property prevents that.
# i#3474: LINK_INTERFACE_LIBRARIES is deprecated, which is overridden by
# INTERFACE_LINK_LIBRARIES property if policy CMP0022 is NEW.
set_target_properties(drsyms PROPERTIES INTERFACE_LINK_LIBRARIES "")

# If drsyms is built static we need to include these with an exports path
# in DynamoRIOTarget*.cmake and not with the source path here:
add_library(dwarf STATIC IMPORTED)
set_property(TARGET dwarf PROPERTY IMPORTED_LOCATION "${dwarf_libpath}")
add_library(elftc STATIC IMPORTED)
set_property(TARGET elftc PROPERTY IMPORTED_LOCATION "${elftc_libpath}")
target_link_libraries(drsyms_static dwarf elftc)
if (LINUX)
  add_library(elf STATIC IMPORTED)
  set_property(TARGET elf PROPERTY IMPORTED_LOCATION "${elf_libpath}")
  target_link_libraries(drsyms_static elf)
endif (LINUX)
if (UNIX)
  # Avoid missing symbols in static library build from g++ libs when
  # drsyms_bench is linked with gcc instead of g++ (i#715, happens w/ cmake
  # < 2.8.0 where demangle.cc is not propagated through libdrsyms.a)
  append_property_string(TARGET drsyms_static COMPILE_FLAGS "-fno-exceptions")
  # On ARM cross-compilation I'm seeing the same __gxx_personality_v0 and
  # __cxa_end_cleanup errors, so we pass this for the .so as well:
  append_property_string(TARGET drsyms COMPILE_FLAGS "-fno-exceptions")
endif (UNIX)

if (WIN32)
  append_property_string(TARGET drsyms_static COMPILE_FLAGS "/DSTATIC_LIB")
endif (WIN32)
DR_install(FILES "${dwarf_libpath}" "${elf_libpath}" "${elftc_libpath}"
  DESTINATION ${INSTALL_EXT_LIB})
if (WIN32)
 DR_install(FILES "${dwarf_dir}/dwarf.pdb" "${dwarf_dir}/elftc.pdb"
    DESTINATION ${INSTALL_EXT_LIB}
    PERMISSIONS ${owner_access} OWNER_EXECUTE GROUP_READ GROUP_EXECUTE
    WORLD_READ WORLD_EXECUTE)
endif (WIN32)

add_executable(drsyms_bench drsyms_bench.c)
configure_DynamoRIO_standalone(drsyms_bench)
use_DynamoRIO_extension(drsyms_bench drsyms)
# we don't want drsyms_bench installed so we avoid the standard location
set_target_properties(drsyms_bench PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY${location_suffix} "${PROJECT_BINARY_DIR}/ext")

# documentation is put into main DR docs/ dir
install_ext_header(drsyms.h)

if (WIN32)
  # We need separate 32-bit and 64-bit versions so we put into lib dir.
  # We put into base dir and not ext for easy sharing w/ including clients.
  DR_install(FILES ${dbghelp_lib} DESTINATION ${INSTALL_LIB_BASE})
  # i#1344: Include a copy of a recent (> 6.0) redistributable version of dbghelp,
  # if we found one, so our drsyms works pre-Vista.
  if (dbghelp_path)
    configure_file(${dbghelp_path} ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/dbghelp.dll
      COPYONLY)
    DR_install(FILES "${dbghelp_path}" DESTINATION ${INSTALL_EXT_LIB})
  endif ()
endif (WIN32)
