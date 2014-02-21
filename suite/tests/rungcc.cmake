# **********************************************************
# Copyright (c) 2012 Google, Inc.    All rights reserved.
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

# arguments:
# * exename = name of executable to create
# * source = name of source file to compile
# * args = extra args to gcc

# if cygwin or mingw gcc is available, we do some extra tests
# XXX: we could alternatively check in mingw-built binaries,
# or even check in a mingw toolchain
if (source MATCHES "cpp$")
  set(compiler_name "g++.exe")
else ()
  set(compiler_name "gcc.exe")
endif ()
find_program(GCC ${compiler_name} DOC "path to ${compiler_name}")
if (NOT GCC)
  message(FATAL_ERROR "gcc not found: unable to build")
endif (NOT GCC)

if (${GCC} MATCHES "cygwin")
  # cygwin sets up /c/cygwin/bin/gcc.exe -> /etc/alternatives/gcc -> /usr/bin/gcc-3.exe
  # we just hardcode -4 and -3: too lazy to go get cygwin tools and resolve
  # the chain of links.
  string(REPLACE ".exe" "-4.exe" real ${GCC})
  if (EXISTS "${real}")
    set(GCC "${real}")
  else ()
    string(REPLACE ".exe" "-3.exe" real ${GCC})
    if (EXISTS "${real}")
      set(GCC "${real}")
    endif ()
  endif ()
endif()

# -I../.. will pick up configure.h (this is run from suite/tests/ in build dir)
# we want -ggdb to get dwarf2 symbols instead of stabs, if using older gcc
execute_process(COMMAND ${GCC} -ggdb -I../.. ${args} -o ${exename} ${source}
  RESULT_VARIABLE cmd_result
  ERROR_VARIABLE cmd_err)
if (cmd_result)
  message(FATAL_ERROR "*** ${GCC} failed (${cmd_result}): ${cmd_err}***\n")
endif (cmd_result)
