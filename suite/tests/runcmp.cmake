# **********************************************************
# Copyright (c) 2015-2020 Google, Inc.    All rights reserved.
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

# For testing apps whose output is too large for CTest's stdout comparison

# input:
# * cmd = command to run
#     should have intra-arg space=@@ and inter-arg space=@ and ;=!
# * cmp = file containing output to compare stdout to
# * capture = if "stderr", captures stderr instead of stdout
# * ignore_matching_lines = optional regex pattern, matching lines will not be diffed

# intra-arg space=@@ and inter-arg space=@
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

set(output "${cmd_out}")
if ("${capture}" STREQUAL "stderr")
  set(output "${cmd_err}")
endif ()

set(tmp "${cmp}-out")
file(WRITE "${tmp}" "${output}")

# We do not support regex in expect file b/c ctest can't handle big regex:
#   "RegularExpression::compile(): Expression too big."

# We used to implement a "diff -b" via cmake regex on the output and expect
# file, but that gets really slow (30s for 750KB strings) so now we require
# strict matches per line.

# Use diff -I to get the ability to skip some lines.

set(diffcmd "diff")
if (ignore_matching_lines)
  execute_process(COMMAND ${diffcmd} -I ${ignore_matching_lines} ${tmp} ${cmp}
    RESULT_VARIABLE dcmd_result
    ERROR_VARIABLE dcmd_err
    OUTPUT_VARIABLE dcmd_out)
else ()
  execute_process(COMMAND ${diffcmd} ${tmp} ${cmp}
    RESULT_VARIABLE dcmd_result
    ERROR_VARIABLE dcmd_err
    OUTPUT_VARIABLE dcmd_out)
endif ()

if (dcmd_result)
  message(STATUS "diff: ${dcmd_out}")
  message(FATAL_ERROR "output in ${tmp} failed to match expected output in ${cmp}")
endif (dcmd_result)
