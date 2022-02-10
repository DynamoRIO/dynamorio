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

# get expected output
# we assume it has already been processed w/ regex => literal, etc.
file(READ "${cmp}" str)

# We do not support regex b/c ctest can't handle big regex:
#   "RegularExpression::compile(): Expression too big."
# so we use STREQUAL.

# We used to implement a "diff -b" via cmake regex on the output and expect
# file, but that gets really slow (30s for 750KB strings) so now we require
# strict matches.

set(output "${cmd_out}")
if ("${capture}" STREQUAL "stderr")
  set(output "${cmd_err}")
endif ()

if (NOT "${output}" STREQUAL "${str}")
  # make it easier to debug
  set(tmp "${cmp}-out")
  file(WRITE "${tmp}" "${output}")
  set(tmp2 "${cmp}-expect")
  file(WRITE "${tmp2}" "${str}")

  set(diffcmd "diff")
  execute_process(COMMAND ${diffcmd} ${tmp} ${tmp2}
    RESULT_VARIABLE dcmd_result
    ERROR_VARIABLE dcmd_err
    OUTPUT_VARIABLE dcmd_out)

  message(STATUS "diff: ${dcmd_out}")

  message(FATAL_ERROR "output in ${tmp} failed to match expected output in ${tmp2}")

endif ()
