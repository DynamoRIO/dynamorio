# **********************************************************
# Copyright (c) 2014-2015 Google, Inc.    All rights reserved.
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

# For cross-compiling on arm64 Linux using gcc-aarch64-linux-gnu package:
# - install AArch64 tool chain:
#   $ sudo apt-get install g++-aarch64-linux-gnu
# - cross-compiling config
#   $ cmake -DCMAKE_TOOLCHAIN_FILE=../dynamorio/make/toolchain-arm64.cmake ../dynamorio
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)
set(TARGET_ABI "linux-gnu")
# specify the cross compiler
SET(CMAKE_C_COMPILER   aarch64-${TARGET_ABI}-gcc)
SET(CMAKE_CXX_COMPILER aarch64-${TARGET_ABI}-g++)

# Assuming the cross compiler is installed to /usr/bin,
# we do not need to set where the target environment is.
SET(CMAKE_FIND_ROOT_PATH)
# search for programs in the build host directories
SET(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
# for libraries and headers in the target directories
SET(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
SET(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)

# set additional variables
SET(CMAKE_LINKER        aarch64-${TARGET_ABI}-ld       CACHE FILEPATH "cmake_linker")
SET(CMAKE_ASM_COMPILER  aarch64-${TARGET_ABI}-as       CACHE FILEPATH "cmake_asm_compiler")
SET(CMAKE_OBJCOPY       aarch64-${TARGET_ABI}-objcopy  CACHE FILEPATH "cmake_objcopy")
SET(CMAKE_STRIP         aarch64-${TARGET_ABI}-strip    CACHE FILEPATH "cmake_strip")
SET(CMAKE_CPP           aarch64-${TARGET_ABI}-cpp      CACHE FILEPATH "cmake_cpp")
