# **********************************************************
# Copyright (c) 2012-2025 Google, Inc.  All rights reserved.
# **********************************************************

# Dr. Memory: the memory debugger
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

#FIXME Add support for RISCV64, ANDROID and MUSL
#TODO(i#7303): Add common/asm_utils_aarch64.asm for AARCH64 support.
if (RISCV64 OR AARCH64 OR ANDROID OR MUSL)
  return()
endif ()

cmake_minimum_required(VERSION 3.7)

include(${PROJECT_SOURCE_DIR}/make/policies.cmake NO_POLICY_SCOPE)

# We do not need libc, and we avoid a 10x size increase in both our
# dll and pdb (plus we avoid stressing private library isolation) by
# not using it (i#714).
set(DynamoRIO_USE_LIBC OFF)

option(SYSCALL_DRIVER "use a kernel driver to identify system call writes")
option(SYSCALL_DRIVER_SRCDIR "source dir of kernel driver")
if (SYSCALL_DRIVER)
  set(DEFINES ${DEFINES} -DSYSCALL_DRIVER)
  include_directories(${SYSCALL_DRIVER_SRCDIR})
endif (SYSCALL_DRIVER)

# for cpp2asm_defines.h
include_directories(${DynamoRIO_DIR})

# we aren't using DR's configure.h so we have to set defines:
if (UNIX)
  if (APPLE AND NOT AARCH64)
    set(asm_defs ${asm_defs} -DASSEMBLE_WITH_NASM)
  else (APPLE AND NOT AARCH64)
    set(asm_defs ${asm_defs} -DASSEMBLE_WITH_GAS)
  endif (APPLE AND NOT AARCH64)
else (UNIX)
  set(asm_defs ${asm_defs} -DASSEMBLE_WITH_MASM)
endif (UNIX)
get_DynamoRIO_defines(DR_DEFINES OFF)
# We need defines to be a list to pass as separate args to custom command.
# We assume none have spaces inside them which seems reasonable.
string(REPLACE " " ";" DR_DEFINES "${DR_DEFINES}")
set(asm_flags ${asm_defs} ${DR_DEFINES} -I "${DynamoRIO_DIR}")
set(asm_deps "${DynamoRIO_DIR}/cpp2asm_defines.h")
if (DR_HOST_X86)
  set(asm_file "../drmf/common/asm_utils_x86.asm")
else ()
  set(asm_file "../drmf/common/asm_utils_arm.asm")
endif ()
add_asm_target(${asm_file} asm_utils_src asm_utils_tgt ""
  "${asm_flags}" "${asm_deps}")

# Versioning: we use separate versioning for DRMF from the tool versioning.
# This is major*100 + minor.
set(DRMF_VERSION_DEFAULT "1.0.${VERSION_NUMBER_PATCHLEVEL}")
set(DRMF_VERSION "" CACHE STRING "DRMF version number: leave empty for default")
if ("${DRMF_VERSION}" STREQUAL "")
  set(DRMF_VERSION ${DRMF_VERSION_DEFAULT})
endif()
message(STATUS "DRMF version number: ${DRMF_VERSION}")
string(REGEX REPLACE "^([0-9]+)\\..*" "\\1" DRMF_VERSION_MAJOR "${DRMF_VERSION}")
string(REGEX REPLACE "^[0-9]+\\.([0-9]+)\\..*" "\\1" DRMF_VERSION_MINOR "${DRMF_VERSION}")
# We use just major.minor for .so versioning (and no, SOVERSION is not sufficient
# here: we have to set VERSION to control the file name and import dependence).
string(REGEX REPLACE "^([0-9]+\\.[0-9]+)\\..*" "\\1"
  DRMF_VERSION_MAJOR_MINOR "${DRMF_VERSION}")
math(EXPR DRMF_VERSION_INTEGER "${DRMF_VERSION_MAJOR}*100 + ${DRMF_VERSION_MINOR}")
set(DRMF_VERSION_COMPAT 9)                # Oldest compatible version
set(DRMF_VERSION_CUR ${DRMF_VERSION_INTEGER}) # Current version

set(framework_incdir "${PROJECT_BINARY_DIR}/ext/include")
configure_file(../drmf/framework/public.h ${framework_incdir}/drmemory_framework.h)

set(external_srcs ../dmrf/framework/drmf_utils.c ../drmf/common/utils_shared.c)

set(srcs
  drsyscall.c
  ${asm_utils_src}
  # add more here
  )

if (UNIX)
  if (APPLE)
    set(srcs ${srcs}
      drsyscall_macos.c
      table_macos_bsd.c)
  else (APPLE)
    set(srcs ${srcs}
      drsyscall_linux.c
      table_linux.c
      table_linux_ioctl.c)
  endif (APPLE)
else (UNIX)
  set(srcs ${srcs}
    drsyscall_windows.c
    table_windows_ntoskrnl.c
    table_windows_ntoskrnl_infoclass.c
    table_windows_ntuser.c
    table_windows_ntusercall.c
    table_windows_ntgdi.c
    table_windows_kernel32.c
    drsyscall_wingdi.c
    drsyscall_fileparse.c
    pdb2sysfile.cpp)
  if (SYSCALL_DRIVER)
    set(srcs ${srcs}
      drmemory/syscall_driver.c)
  endif (SYSCALL_DRIVER)
endif (UNIX)

# i#1594c#3: VS generators fail if static lib has resources
set(srcs_static ${srcs})

if (WIN32)
  set(srcs ${srcs} ${PROJECT_SOURCE_DIR}/core/win32/resources.rc)
  set(DEFINES_NO_D ${DEFINES_NO_D} RC_IS_DRSYSCALL)
  # FIXME: fix warnings and up to /W4
  string(REGEX REPLACE "/W4" "/W2" CMAKE_C_FLAGS "${CMAKE_C_FLAGS}")
  add_definitions(${asm_defs})
  message(STATUS "add_definitions(-DASSEMBLE_WITH_MASM)")
  # We can't include directly from DDK b/c the DDK include dir and VS include
  # dirs are incompatible, so we have our own copies of the headers we need.
  include_directories(wininc/psdk wininc/dxsdk)
endif ()

macro(configure_drsyscall_target target)
  if (UNIX)
    # Avoid relocations which tend to violate security policies
    append_property_string(TARGET ${target} COMPILE_FLAGS "-fPIC")
    # we need to add asm_defs to target b/c add_asm_target() is not able to set flags
    # for non-VS generators
    string(REPLACE "-D" "" asm_defs_list "${asm_defs}")
    append_property_list(TARGET ${target} COMPILE_DEFINITIONS "${asm_defs_list}")
  endif (UNIX)
  # We do not prevent duplicate builds of the same files as that would require
  # building a static library that is then linked with drmf_utils.c to
  # create a final static library, for DR_EXT_DRSYSCALL_STATIC: and that's
  # a pain to export w/ the proper link rules.  But we at least use the
  # same flags and avoid compiling the same file differently.
  _DR_append_property_list(TARGET ${target} COMPILE_DEFINITIONS "${DEFINES_NO_D}")
  target_link_libraries(${target} drfrontendlib ${ntimp_lib})
  if (NOT USER_SPECIFIED_DynamoRIO_DIR AND "${CMAKE_GENERATOR}" MATCHES "Visual Studio")
    add_dependencies(${target} ntdll_imports)
  endif ()
  copy_target_to_device(${target} ${DR_DEVICE_BASEDIR} "${loc_suffix}")
endmacro(configure_drsyscall_target)

macro(export_drsyscall_target target drmgr drsyms)
  # We need to clear the dependents that come from DR to avoid the prefix
  # from affecting them too.
  set_target_properties(${target} PROPERTIES INTERFACE_LINK_LIBRARIES "")
  DR_export_target(${target})
  # Now put in our imports (w/o any namespace)
  set_target_properties(${target} PROPERTIES
    INTERFACE_LINK_LIBRARIES "dynamorio;${drmgr};${drsyms};drcontainers;drfrontendlib")
  install(TARGETS ${target} EXPORT ${exported_targets_name}
    DESTINATION ${DRMF_INSTALL_BIN})
  # Top-level installs .debug and .pdb files
endmacro(export_drsyscall_target)

# For the exported version, we don't want to print to stderr or raise
# msgboxes, so we link in globals to suppress notification in drmf_utils.c.
add_library(drsyscall SHARED ${srcs})
configure_extension(drsyscall OFF OFF)

# Set a preferred base to avoid conflict if we can
set(PREFERRED_BASE 0x77000000)
configure_DynamoRIO_client(drsyscall)
use_DynamoRIO_extension(drsyscall drmgr)
use_DynamoRIO_extension(drsyscall drcontainers)
use_DynamoRIO_extension(drsyscall drsyms)

macro(set_library_version target number)
  # We only set the version/soversion on Windows to avoid many
  # negatives (DRi#1374, DRi#2127, Android "adb push" not supporting
  # symlinks, etc.) and very few positives on UNIX platforms.  DR's
  # loader and the DRMF init code perform their own version checks for
  # client compatibility.
  if (WINDOWS)
    set_target_properties(${target} PROPERTIES VERSION ${number})
  endif ()
endmacro()
set_library_version(drsyscall ${DRMF_VERSION_MAJOR_MINOR})
configure_drsyscall_target(drsyscall)
export_drsyscall_target(drsyscall "drmgr" "drsyms")
# Make a build-dir copy for the drmf_proj test.
install_ext_header(drsyscall.h)

# Since the license is LGPL, SHARED and not STATIC by default.
# SHARED is also required if multiple separate components all want to
# use this same extension.
# But, we also provide a static version with a different name for those
# who want it, in the style of DR's side-by-side static extensions.
add_library(drsyscall_static STATIC ${srcs_static})
configure_extension(drsyscall_static ON OFF)
configure_DynamoRIO_client(drsyscall_static)
use_DynamoRIO_extension(drsyscall_static drmgr_static)
use_DynamoRIO_extension(drsyscall_static drcontainers)
use_DynamoRIO_extension(drsyscall_static drsyms_static)
configure_drsyscall_target(drsyscall_static)
add_static_lib_debug_info(drsyscall_static ${INSTALL_EXT_LIB})
export_drsyscall_target(drsyscall_static "drmgr_static" "drsyms_static")

# We build a separate static target for internal use that has our
# log/assert/notify infrastructure.
add_library(drsyscall_int STATIC ${srcs_static})
configure_DynamoRIO_client(drsyscall_int)
use_DynamoRIO_extension(drsyscall_int drmgr_static)
use_DynamoRIO_extension(drsyscall_int drcontainers)
if (STATIC_DRSYMS)
  use_DynamoRIO_extension(drsyscall_int drsyms_static)
else ()
  use_DynamoRIO_extension(drsyscall_int drsyms)
endif ()
configure_drsyscall_target(drsyscall_int)

# Documentation is handled as part of the main tool docs processing.
