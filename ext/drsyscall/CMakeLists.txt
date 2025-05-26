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

# TODO i#7399: Add Android, MUSL, and RISCV64 support to DrSyscall.
# TODO i#5383: Add macOS ARM64 support to DrSyscall.
if (RISCV64 OR ANDROID OR MUSL OR (AARCH64 AND APPLE))
  return()
endif ()

cmake_minimum_required(VERSION 3.14)

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

add_ext_asm_target("../drmf/common/asm_utils_arm.asm" "../drmf/common/asm_utils_aarch64.asm" ""
  "../drmf/common/asm_utils_x86.asm" asm_utils_src asm_utils_tgt)

# TODO i#7303: Use DynanmoRIO version instead of DRMF_VERSION.
# DrSyscall is currently being migrated from DrMemory to DynamoRIO. Once the
# migration is complete, DrSyscall will be removed from DrMemory. Until then,
# the DRMF version is temporarily in use.
set(DRMF_VERSION_COMPAT 9)                # Oldest compatible version
# Set DRMF_VERSION_CUR for DRMF_VERSION_USED_VAR in framework/public.h.
math(EXPR DRMF_VERSION_CUR "${VERSION_NUMBER_MAJOR}*100 + ${VERSION_NUMBER_MINOR}")

set(framework_incdir "${PROJECT_BINARY_DIR}/ext/include")
configure_file(../drmf/framework/public.h ${framework_incdir}/drmemory_framework.h)

set(external_srcs ../drmf/framework/drmf_utils.c ../drmf/common/utils_shared.c)

set(srcs
  drsyscall.c
  ../drmf/framework/version.c
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
  set(srcs ${srcs} ${PROJECT_SOURCE_DIR}/core/win32/resources.rc)
endif (UNIX)

# i#1594c#3: VS generators fail if static lib has resources
set(srcs_static ${srcs})

if (WIN32)
  set(srcs ${srcs} ${PROJECT_SOURCE_DIR}/core/win32/resources.rc)
  set(DEFINES_NO_D ${DEFINES_NO_D} RC_IS_DRSYSCALL)
  # XXX: fix warnings and up to /W4
  string(REGEX REPLACE "/W4" "/W2" CMAKE_C_FLAGS "${CMAKE_C_FLAGS}")
endif ()

macro(configure_drsyscall_target target)
  if (NOT "${CMAKE_GENERATOR}" MATCHES "Visual Studio")
    # we need to add asm_defs to target b/c add_asm_target() is not able to set flags
    # for non-VS generators
    string(REPLACE "-D" "" asm_defs_list "${asm_defs}")
    append_property_list(TARGET ${target} COMPILE_DEFINITIONS "${asm_defs_list}")
  endif ()

  if ("${CMAKE_GENERATOR}" MATCHES "Visual Studio")
    # ensure race-free parallel builds
    add_dependencies(${target} ${asm_utils_tgt})
  endif ("${CMAKE_GENERATOR}" MATCHES "Visual Studio")

  # We do not prevent duplicate builds of the same files as that would require
  # building a static library that is then linked with drmf_utils.c to
  # create a final static library, for DR_EXT_DRSYSCALL_STATIC: and that's
  # a pain to export w/ the proper link rules.  But we at least use the
  # same flags and avoid compiling the same file differently.
  _DR_append_property_list(TARGET ${target} COMPILE_DEFINITIONS "${DEFINES_NO_D}")
  target_link_libraries(${target} drfrontendlib ${ntimp_lib})

  include_directories(${PROJECT_SOURCE_DIR}/ext/drmf/common)
  include_directories(${PROJECT_SOURCE_DIR}/ext/drmf/framework)

  if (UNIX)
    include_directories(${PROJECT_SOURCE_DIR}/core/unix/include)
  endif ()

  if (WIN32)
    add_dependencies(${target} ntdll_imports)
    add_definitions(${asm_defs})
    # We can't include directly from DDK b/c the DDK include dir and VS include
    # dirs are incompatible, so we have our own copies of the headers we need.
    include_directories(${PROJECT_SOURCE_DIR}/ext/drmf/wininc/psdk
            ${PROJECT_SOURCE_DIR}/ext/drmf/wininc/dxsdk)
    target_link_libraries(${target} ntdll_imports)
  endif ()
endmacro(configure_drsyscall_target)

# For the exported version, we don't want to print to stderr or raise
# msgboxes, so we link in globals to suppress notification in drmf_utils.c.
add_library(drsyscall SHARED ${srcs} ${external_srcs})
configure_extension(drsyscall OFF OFF)

# Set a preferred base to avoid conflict if we can
set(PREFERRED_BASE 0x77000000)
configure_DynamoRIO_client(drsyscall)
use_DynamoRIO_extension(drsyscall drmgr)
use_DynamoRIO_extension(drsyscall drcontainers)
use_DynamoRIO_extension(drsyscall drsyms)

configure_drsyscall_target(drsyscall)
# Make a build-dir copy for the drmf_proj test.
install_ext_header(drsyscall.h)

# Since the license is LGPL, SHARED and not STATIC by default.
# SHARED is also required if multiple separate components all want to
# use this same extension.
# But, we also provide a static version with a different name for those
# who want it, in the style of DR's side-by-side static extensions.
add_library(drsyscall_static STATIC ${srcs_static} ${external_srcs})
configure_extension(drsyscall_static ON OFF)
use_DynamoRIO_extension(drsyscall_static drmgr_static)
use_DynamoRIO_extension(drsyscall_static drcontainers)
use_DynamoRIO_extension(drsyscall_static drsyms_static)
configure_drsyscall_target(drsyscall_static)

add_library(drsyscall_drstatic STATIC ${srcs_static})
configure_extension(drsyscall_drstatic ON ON)
use_DynamoRIO_extension(drsyscall_drstatic drcontainers_drstatuc)
use_DynamoRIO_extension(drsyscall_drstatic drmgr_static)
configure_drsyscall_target(drsyscall_drstatic)

if (LINUX)
  set(drsyscall_record_lib_srcs
    drsyscall_record_lib.c
  )

  set(drsyscall_record_lib_external_srcs
    ../drmf/framework/drmf_utils.c
  )

  add_library(drsyscall_record_lib STATIC ${drsyscall_record_lib_srcs}
    ${drsyscall_record_lib_external_srcs})
  configure_extension(drsyscall_record_lib ON ON)
  configure_DynamoRIO_client(drsyscall_record_lib)
  use_DynamoRIO_extension(drutil_static drmgr_static)

  install_ext_header(drsyscall_record.h)
  install_ext_header(drsyscall_record_lib.h)

  add_executable(drsyscall_record_viewer drsyscall_record_viewer.c)
  configure_DynamoRIO_standalone(drsyscall_record_viewer)
  configure_DynamoRIO_static(drsyscall_record_viewer)
  use_DynamoRIO_extension(drsyscall_record_viewer drsyscall_record_lib)
  set_target_properties(drsyscall_record_viewer PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY${location_suffix} "${PROJECT_BINARY_DIR}/${INSTALL_BIN}")
endif ()
