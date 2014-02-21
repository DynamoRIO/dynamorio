# **********************************************************
# Copyright (c) 2013 Google, Inc.    All rights reserved.
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

# Invoked by the test suite for testing this tool

# input:
# * cmd = command to run
#     should have intra-arg space=@@ and inter-arg space=@ and ;=!
# * cmp = file containing output to compare app output to, to ensure app ran correctly

# Intra-arg space=@@ and inter-arg space=@.
# XXX i#1327: now that we have -c and other option passing improvements we
# should be able to get rid of this @@ stuff.
string(REGEX REPLACE "@@" " " cmd "${cmd}")
string(REGEX REPLACE "@" ";" cmd "${cmd}")
string(REGEX REPLACE "!" "\\\;" cmd "${cmd}")

# run the cmd
execute_process(COMMAND ${cmd}
  RESULT_VARIABLE cmd_result
  ERROR_VARIABLE cmd_err
  OUTPUT_VARIABLE cmd_out)
if (cmd_result)
  message(FATAL_ERROR "*** ${cmd} failed (${cmd_result}): ${cmd_err}***\n")
endif (cmd_result)

# DR's tests write to stderr so combine stdout and stderr.
# We distinguish drltrace's output via its prefx ~~~~.
set(app_out "${cmd_out}${cmd_err}")
string(REGEX MATCHALL "~~~~[^\n]*\n" tool_out "${app_out}")
string(REGEX REPLACE "~~~~[^\n]*\n" "" app_out "${app_out}")

# get expected app output
# we assume it has already been processed w/ regex => literal, etc.
file(READ "${cmp}" str)

if (WIN32)
  # our test prep turned \n into \r?\n so revert
  string(REGEX REPLACE "\r\\?" "" str "${str}")
endif (WIN32)

# we don't support regex in output pattern
if (NOT "${app_out}" STREQUAL "${str}")
  message(FATAL_ERROR "app output ${app_out} failed to match expected ${str}")
endif ()

# Now check tool output
if (UNIX)
  set(tomatch "~~~~ libc.so.*!.*printf")
else (UNIX)
  set(tomatch "~~~~ KERNEL32.dll!WriteFile")
endif (UNIX)
if (NOT "${tool_out}" MATCHES "${tomatch}" )
  message(FATAL_ERROR "tool output ${tool_out} failed to match expected ${tomatch}")
endif ()
