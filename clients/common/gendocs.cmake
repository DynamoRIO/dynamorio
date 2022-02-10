# **********************************************************
# Copyright (c) 2015-2020 Google, Inc.    All rights reserved.
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
# ARE DISCLAIMED. IN NO EVENT SHALL VMWARE, INC. OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
# SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
# CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
# OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
# DAMAGE.

# inputs:
# * src
# * dst
# * prog = the app that spits out the option list
# * prog_arg = arg to app, if any
# * CMAKE_CROSSCOMPILING = whether cross-compiling and thus this program
#   will not execute.

if (CMAKE_CROSSCOMPILING)
  # i#1730: This is better than failing.
  set(prog_out "The options list is not currently able to be generated for cross-compiled builds (https://github.com/DynamoRIO/dynamorio/issues/1730).  Please see the online documentation or run the tool with -help.")
else ()
  execute_process(COMMAND ${prog} ${prog_arg}
    RESULT_VARIABLE prog_result
    ERROR_VARIABLE prog_err
    OUTPUT_VARIABLE prog_out
    )
  if (prog_result OR prog_err)
    message(FATAL_ERROR "*** ${prog} failed: ***\n${prog_err}")
  endif ()
endif ()

file(READ "${src}" content)
string(REPLACE
  "REPLACEME_WITH_OPTION_LIST"
  "${prog_out}" content "${content}")
file(WRITE "${dst}" "${content}")
