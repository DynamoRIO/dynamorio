# **********************************************************
# Copyright (c) 2014-2024 Google, Inc.    All rights reserved.
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
# * Neither the name of Google, Inc. nor the names of its contributors may be
#   used to endorse or promote products derived from this software without
#   specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED. IN NO EVENT SHALL GOOGLE, INC. OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
# SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
# CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
# OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
# DAMAGE.

# For cross compiling for 64-bit arm Android using the Android LLVM toolchain:
# - Download the toolchain:
#   https://developer.android.com/ndk/downloads/revision_history
# - Cross-compiling config with ANDROID_NDK:
#   $ cmake -DCMAKE_TOOLCHAIN_FILE=../dynamorio/make/toolchain-android-llvm.cmake \
#     -DANDROID_NDK=/PATH/TO/android-ndk-<version> \
#     -DANDROID_API_LEVEL=<chosen-api-level> \
#     ../dynamorio

# Target system.
set(CMAKE_SYSTEM_NAME Android)
set(CMAKE_SYSTEM_VERSION 1)

# If using a different target, set -DTARGET_ABI=<abi> on the command line.
if (NOT DEFINED TARGET_ABI)
  set(TARGET_ABI "aarch64-linux-android")
endif ()
if (TARGET_ABI MATCHES "^aarch64")
  set(CMAKE_SYSTEM_PROCESSOR aarch64)
endif ()

# Specify the cross compiler.
if (NOT DEFINED ANDROID_NDK)
  set(toolchain_bin_path "")
else ()
  if(CMAKE_HOST_SYSTEM_NAME STREQUAL Linux)
    set(HOST_TAG linux-x86_64)
  elseif(CMAKE_HOST_SYSTEM_NAME STREQUAL Darwin)
    set(HOST_TAG darwin-x86_64)
  elseif(CMAKE_HOST_SYSTEM_NAME STREQUAL Windows)
    set(HOST_TAG windows-x86_64)
  endif()
  set(toolchain_bin_path "${ANDROID_NDK}/toolchains/llvm/prebuilt/${HOST_TAG}/bin/")
  set(toolchain_sysroot_path "${ANDROID_NDK}/toolchains/llvm/prebuilt/${HOST_TAG}/sysroot")
endif ()

if (NOT DEFINED ANDROID_API_LEVEL)
  set(api_level "30")
else ()
  set(api_level "${ANDROID_API_LEVEL}")
endif ()

SET(CMAKE_C_COMPILER   ${toolchain_bin_path}${TARGET_ABI}${api_level}-clang
  CACHE FILEPATH "cmake_c_compiler")
SET(CMAKE_CXX_COMPILER ${toolchain_bin_path}${TARGET_ABI}${api_level}-clang++
  CACHE FILEPATH "cmake_cxx_compiler")
SET(CMAKE_LINKER       ${toolchain_bin_path}ld.lld
  CACHE FILEPATH "cmake_linker")
SET(CMAKE_ASM_COMPILER ${toolchain_bin_path}${TARGET_ABI}${api_level}-clang
  CACHE FILEPATH "cmake_asm_compiler")
SET(CMAKE_OBJCOPY      ${toolchain_bin_path}llvm-objcopy
  CACHE FILEPATH "cmake_objcopy")
SET(CMAKE_STRIP        ${toolchain_bin_path}llvm-strip
  CACHE FILEPATH "cmake_strip")
SET(CMAKE_CPP          ${toolchain_bin_path}${TARGET_ABI}${api_level}-clang
  CACHE FILEPATH "cmake_cpp")

SET(CMAKE_FIND_ROOT_PATH ${toolchain_sysroot_path})
# Search for programs in the build host directories.
SET(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
# For libraries and headers in the target directories.
SET(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
SET(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_LIBRARY_ARCHITECTURE ${TARGET_ABI})

# Use static C++ runtime libraries.
set(CMAKE_ANDROID_STL_TYPE "c++_static")
string(APPEND CMAKE_CXX_FLAGS_INIT " -stdlib=libc++")
string(APPEND CMAKE_CXX_STANDARD_LIBRARIES " -static-libstdc++")
