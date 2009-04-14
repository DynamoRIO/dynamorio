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

# How to use:
# First, set these three parameters:
#
#    DynamoRIO_X64 = if client is 64-bit
#    DynamoRIO_DEBUG = if wish to link to debug DynamoRIO library
#    DynamoRIO_CXX = if client is C++
#    DynamoRIO_VISATT = if using gcc visibility attributes
#
# Then include this file, by first executing a find_package()
# as shown in this sample CMakeLists.txt file for a client:
# 
#   find_package(DynamoRIO)
#   if (DynamoRIO_FOUND)
#     set(DynamoRIO_X64 ON)
#     set(DynamoRIO_DEBUG OFF)
#     set(DynamoRIO_CXX OFF)
#     include(${DynamoRIO_USE_FILE})
#     add_library(myclient SHARED myclient.c)
#     target_link_libraries(myclient dynamorio)
#   endif(DynamoRIO_FOUND)
#
# When building your client, users will need to point at the DynamoRIO
# installation's cmake directory with the DynamoRIO_DIR variable, like this:
#
#   cmake -DDynamoRIO_DIR:PATH=/absolute/path/to/dynamorio/cmake <path to your client sources>

if (UNIX)
  if (NOT CMAKE_COMPILER_IS_GNUCC)
    # Our linker script is GNU-specific
    message(FATAL_ERROR "DynamoRIOUse.cmake only supports the GNU linker on Linux")
  endif (NOT CMAKE_COMPILER_IS_GNUCC)
else (UNIX)
  if (NOT ${COMPILER_BASE_NAME} STREQUAL "cl")
    # Our link flags are Microsoft-specific
    message(FATAL_ERROR "DynamoRIOUse.cmake only supports the Microsoft compiler on Windows")
  endif (NOT ${COMPILER_BASE_NAME} STREQUAL "cl")
endif (UNIX)

include_directories(${DynamoRIO_INCLUDE_DIRS})

if (UNIX)
  add_definitions(-DLINUX)
else (UNIX)
  add_definitions(-DWINDOWS)
endif (UNIX)

if (DynamoRIO_X64)
  add_definitions(-DX86_64)
else (DynamoRIO_X64)
  add_definitions(-DX86_32)
endif (DynamoRIO_X64)

if (DynamoRIO_VISATT)
  add_definitions(-DUSE_VISIBILITY_ATTRIBUTES)
endif (DynamoRIO_VISATT)

if (DynamoRIO_X64)
  if (DynamoRIO_DEBUG)
    set(DynamoRIO_LIBDIR ${DynamoRIO_LIBRARY64_DBG_DIRS})
  else (DynamoRIO_DEBUG)
    set(DynamoRIO_LIBDIR ${DynamoRIO_LIBRARY64_REL_DIRS})
  endif (DynamoRIO_DEBUG)
else (DynamoRIO_X64)
  if (DynamoRIO_DEBUG)
    set(DynamoRIO_LIBDIR ${DynamoRIO_LIBRARY32_DBG_DIRS})
  else (DynamoRIO_DEBUG)
    set(DynamoRIO_LIBDIR ${DynamoRIO_LIBRARY32_REL_DIRS})
  endif (DynamoRIO_DEBUG)
endif (DynamoRIO_X64)
link_directories(${DynamoRIO_LIBDIR})

if (UNIX)
  # avoid SElinux text relocation security violations by explicitly requesting PIC
  set(CLIENT_LINK_FLAGS "-fPIC -shared -nostartfiles -nodefaultlibs -lgcc")
else (UNIX)
  string(TOUPPER "${CMAKE_BUILD_TYPE}" CMAKE_BUILD_TYPE_UPPER)
  string(REGEX REPLACE "/M[TD]d?" ""
    CMAKE_C_FLAGS_${CMAKE_BUILD_TYPE_UPPER} "${CMAKE_C_FLAGS_${CMAKE_BUILD_TYPE_UPPER}}")
  string(REGEX REPLACE "/RTC." ""
    CMAKE_C_FLAGS_${CMAKE_BUILD_TYPE_UPPER} "${CMAKE_C_FLAGS_${CMAKE_BUILD_TYPE_UPPER}}")
  # Avoid bringing in libc and/or kernel32 for stack checks
  set(CMAKE_C_FLAGS_${CMAKE_BUILD_TYPE_UPPER}
    "${CMAKE_C_FLAGS_${CMAKE_BUILD_TYPE_UPPER}} /GS-")
  set(CLIENT_LINK_FLAGS "/nodefaultlib /noentry")
endif (UNIX)
set(CMAKE_SHARED_LINKER_FLAGS
  "${CMAKE_SHARED_LINKER_FLAGS} ${CLIENT_LINK_FLAGS}")

if (DynamoRIO_X64)
  # It's currently required on 64bit that a client library have a
  # preferred base address in the lower 2GB, to be reachable by our
  # library and code cache.  This is due to limitations in the current
  # experimental 64bit release and will be adressed in later releases.
  # Naturally for multiple clients different addresses should be used.
  # We suggest using the range 0x72000000-0x75000000.
  set(PREFERRED_BASE 0x72000000)
  if (UNIX)
    # We use a linker script to set the preferred base
    set(LD_SCRIPT ${CMAKE_CURRENT_BINARY_DIR}/ldscript)
    # We do NOT add ${LD_SCRIPT} as an ADDITIONAL_MAKE_CLEAN_FILES since it's
    # configure-time built not make-time built
    if (X64)
      set(LD_FLAGS "-melf_x86_64")
    else (X64)
      set(LD_FLAGS "-melf_i386")
    endif (X64)

    # In order to just tweak the default linker script we start with exactly that.
    execute_process(COMMAND
      ${CMAKE_LINKER} "${LD_FLAGS}" --verbose
      RESULT_VARIABLE ld_result
      ERROR_VARIABLE ld_error
      OUTPUT_VARIABLE string)
    if (ld_result OR ld_error)
      message(FATAL_ERROR "*** ${CMAKE_LINKER} failed: ***\n${ld_error}")
    endif (ld_result OR ld_error)

    # Strip out just the SECTIONS{} portion
    string(REGEX REPLACE ".*(SECTIONS.*\n\\}).*" "\\1" string "${string}")
    # Find and replace the default base
    string(REGEX MATCH "= *(0x[0-9]+) *\\+ *SIZEOF_HEADERS" default_base "${string}")
    string(REGEX REPLACE ".*(0x[0-9]+).*" "\\1" default_base "${default_base}")
    string(REGEX REPLACE "${default_base}" "${PREFERRED_BASE}" string "${string}")
    string(REGEX REPLACE "(\n{)" "\\1\n  . = ${PREFERRED_BASE};" string "${string}")
    file(WRITE ${LD_SCRIPT} "${string}")

    # -dT is preferred, available on ld 2.18+: we could check for it
    set(LD_SCRIPT_OPTION "-T")
    set(PREFERRED_BASE_FLAGS "-Xlinker ${LD_SCRIPT_OPTION} -Xlinker ${LD_SCRIPT}")
  else (UNIX)
    set(PREFERRED_BASE_FLAGS "/base:${PREFERRED_BASE} /fixed")
  endif (UNIX)
  set(CMAKE_SHARED_LINKER_FLAGS
    "${CMAKE_SHARED_LINKER_FLAGS} ${PREFERRED_BASE_FLAGS}")
endif (DynamoRIO_X64)

if (UNIX)
  # Wrapping for C++ clients
  # We set the flags even if DynamoRIO_CXX is OFF in case a user wants to
  # manually set them due to a mixture of C and C++ clients.
  set(CXX_LIBS
    -print-file-name=libstdc++.a
    -print-file-name=libgcc.a
    -print-file-name=libgcc_eh.a)
  foreach (lib ${CXX_LIBS})  
    execute_process(COMMAND
      ${CMAKE_CXX_COMPILER} ${CMAKE_CXX_FLAGS} ${lib} 
      RESULT_VARIABLE cxx_result
      ERROR_VARIABLE cxx_error
      OUTPUT_VARIABLE string)
    if (cxx_result OR cxx_error)
      message(FATAL_ERROR "*** ${CMAKE_CXX_COMPILER} failed: ***\n${cxx_error}")
    endif (cxx_result OR cxx_error)
    string(REGEX REPLACE "[\r\n]" "" string "${string}")
    set(DynamoRIO_CXXFLAGS "${DynamoRIO_CXXFLAGS} ${string}")
  endforeach (lib)
  set(DynamoRIO_CXXFLAGS
    "${DynamoRIO_CXXFLAGS} -Xlinker -wrap=malloc -Xlinker -wrap=realloc -Xlinker -wrap=free")
  if (DynamoRIO_CXX)
    set(CMAKE_SHARED_LINKER_FLAGS
      "${CMAKE_SHARED_LINKER_FLAGS} ${DynamoRIO_CXXFLAGS}")
  endif (DynamoRIO_CXX)
endif (UNIX)

