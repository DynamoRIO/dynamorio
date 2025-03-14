# **********************************************************
# Copyright (c) 2012-2020 Google, Inc.  All rights reserved.
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

cmake_minimum_required(VERSION 3.7)

include(${PROJECT_SOURCE_DIR}/make/policies.cmake NO_POLICY_SCOPE)

set_output_dirs(${framework_bindir})

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

set(external_srcs ../framework/drmf_utils.c ../common/utils_shared.c)

set(srcs
  drsyscall.c
  ../framework/version.c
  # add more here
  )

if (UNIX)
  if (APPLE)
    set(srcs ${srcs}
      drsyscall_macos.c
      table_macos_bsd.c
      ../${asm_utils_src})
  else (APPLE)
    set(srcs ${srcs}
      drsyscall_linux.c
      table_linux.c
      table_linux_ioctl.c
      ../${asm_utils_src})
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
  set(srcs ${srcs} ${PROJECT_SOURCE_DIR}/make/resources.rc)
  set(DEFINES_NO_D ${DEFINES_NO_D} RC_IS_DRSYSCALL)
endif ()

macro(configure_drsyscall_target target)
  if (UNIX)
    # Avoid relocations which tend to violate security policies
    append_property_string(TARGET ${target} COMPILE_FLAGS "-fPIC")
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
  copy_target_to_device(${target})
endmacro(configure_drsyscall_target)

macro(export_drsyscall_target target drmgr drsyms)
  # We need to clear the dependents that come from DR to avoid the prefix
  # from affecting them too.
  set_target_properties(${target} PROPERTIES INTERFACE_LINK_LIBRARIES "")
  export_target(${target})
  # Now put in our imports (w/o any namespace)
  set_target_properties(${target} PROPERTIES
    INTERFACE_LINK_LIBRARIES "dynamorio;${drmgr};${drsyms};drcontainers;drfrontendlib")
  install(TARGETS ${target} EXPORT ${exported_targets_name}
    DESTINATION ${DRMF_INSTALL_BIN})
  # Top-level installs .debug and .pdb files
endmacro(export_drsyscall_target)

# For the exported version, we don't want to print to stderr or raise
# msgboxes, so we link in globals to suppress notification in drmf_utils.c.
add_library(drsyscall SHARED ${srcs} ${external_srcs})
# Set a preferred base to avoid conflict if we can
set(PREFERRED_BASE 0x77000000)
configure_DynamoRIO_client(drsyscall)
use_DynamoRIO_extension(drsyscall drmgr)
use_DynamoRIO_extension(drsyscall drcontainers)
use_DynamoRIO_extension(drsyscall drsyms)
set_library_version(drsyscall ${DRMF_VERSION_MAJOR_MINOR})
configure_drsyscall_target(drsyscall)
export_drsyscall_target(drsyscall "drmgr" "drsyms")
# Make a build-dir copy for the drmf_proj test.
configure_file(drsyscall.h "${framework_incdir}/drsyscall.h" @ONLY)
install(FILES drsyscall.h DESTINATION ${DRMF_INSTALL_INC})

# Since the license is LGPL, SHARED and not STATIC by default.
# SHARED is also required if multiple separate components all want to
# use this same extension.
# But, we also provide a static version with a different name for those
# who want it, in the style of DR's side-by-side static extensions.
add_library(drsyscall_static STATIC ${srcs_static} ${external_srcs})
configure_DynamoRIO_client(drsyscall_static)
use_DynamoRIO_extension(drsyscall_static drmgr_static)
use_DynamoRIO_extension(drsyscall_static drcontainers)
use_DynamoRIO_extension(drsyscall_static drsyms_static)
configure_drsyscall_target(drsyscall_static)
add_static_lib_debug_info(drsyscall_static ${DRMF_INSTALL_BIN})
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
