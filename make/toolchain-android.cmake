# **********************************************************
# Copyright (c) 2014-2016 Google, Inc.    All rights reserved.
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

# For cross compiling for 32-bit arm Android using Android standalone toolchain:
# - Download toolchain, and install the standalone toolchain
#   https://developer.android.com/tools/sdk/ndk/index.html
#   https://developer.android.com/ndk/guides/standalone_toolchain.html
#   $/PATH/TO/ANDROID_NDK/build/tools/make-standalone-toolchain.sh --arch=arm \
#     --toolchain=arm-linux-androideabi-4.9 --platform=android-21 \
#     --install-dir=/TOOLCHAIN/INSTALL/PATH
# - cross-compiling config with ANDROID_TOOLCHAIN
#   $cmake -DCMAKE_TOOLCHAIN_FILE=../dynamorio/make/toolchain-android.cmake \
#     -DANDROID_TOOLCHAIN=/TOOLCHAIN/INSTALL/PATH ../dynamorio

# Target system
set(CMAKE_SYSTEM_NAME Android)
set(CMAKE_SYSTEM_VERSION 1)

# If using a different target, set -DTARGET_ABI=<abi> on the command line.
if (NOT DEFINED TARGET_ABI)
  set(TARGET_ABI "arm-linux-androideabi")
endif ()
if (TARGET_ABI MATCHES "^arm")
  set(CMAKE_SYSTEM_PROCESSOR arm)
endif ()

# specify the cross compiler
if (NOT DEFINED ANDROID_TOOLCHAIN)
  # Support the bin dir being on the PATH
  find_program(gcc ${TARGET_ABI}-gcc)
  if (gcc)
    get_filename_component(gcc_path ${gcc} PATH)
    set(toolchain_bin_path "${gcc_path}/")
  else ()
    set(toolchain_bin_path "")
  endif ()
else ()
  set(toolchain_bin_path "${ANDROID_TOOLCHAIN}/bin/")
endif ()
SET(CMAKE_C_COMPILER   ${toolchain_bin_path}${TARGET_ABI}-gcc
  CACHE FILEPATH "cmake_c_compiler")
SET(CMAKE_CXX_COMPILER ${toolchain_bin_path}${TARGET_ABI}-g++
  CACHE FILEPATH "cmake_cxx_compiler")
SET(CMAKE_LINKER       ${toolchain_bin_path}${TARGET_ABI}-ld.bfd
  CACHE FILEPATH "cmake_linker")
SET(CMAKE_ASM_COMPILER ${toolchain_bin_path}${TARGET_ABI}-as
  CACHE FILEPATH "cmake_asm_compiler")
SET(CMAKE_OBJCOPY      ${toolchain_bin_path}${TARGET_ABI}-objcopy
  CACHE FILEPATH "cmake_objcopy")
SET(CMAKE_STRIP        ${toolchain_bin_path}${TARGET_ABI}-strip
  CACHE FILEPATH "cmake_strip")
SET(CMAKE_CPP          ${toolchain_bin_path}${TARGET_ABI}-cpp
  CACHE FILEPATH "cmake_cpp")

# specify sysroot
if (NOT DEFINED ANDROID_SYSROOT)
  # assuming default android standalone toolchain directory layout
  find_path(compiler_path ${CMAKE_C_COMPILER})
  set(ANDROID_SYSROOT "${compiler_path}/../sysroot")
endif ()

SET(CMAKE_FIND_ROOT_PATH ${ANDROID_SYSROOT})
# search for programs in the build host directories
SET(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
# for libraries and headers in the target directories
SET(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
SET(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
