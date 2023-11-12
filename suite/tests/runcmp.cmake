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

# Get expected output, we assume it has already been processed w/
# regex => literal, etc.
file(READ "${cmp}" expect)

# Convert file contents into a CMake list, where each element in the list is one
# line of the file.
string(REGEX MATCHALL "([^\n]+)\n" output "${output}")
string(REGEX MATCHALL "([^\n]+)\n" expect "${expect}")

if (NOT "${ignore_matching_lines}" STREQUAL "")
  # Filter out all the ignored lines.
  set(filtered_expect)
  foreach (item ${expect})
    if (NOT "${item}" MATCHES "${ignore_matching_lines}")
      list(APPEND filtered_expect ${item})
    endif ()
  endforeach ()
else ()
  set(filtered_expect ${expect})
endif ()

list(LENGTH filtered_expect filtered_expect_length)
list(LENGTH output output_length)

set(lists_identical TRUE)
if (NOT ${filtered_expect_length} EQUAL ${output_length})
  set(lists_identical FALSE)
endif ()

if (lists_identical)
  if (filtered_expect_length GREATER "1000")
    # CMake list(GET ...) operation is slow, do STREQUAL for large files, give up
    # the diff output.
    # XXX: Use ZIP_LISTS for better performance, which requires CMake 3.17.
    if(NOT filtered_expect STREQUAL output)
      set(lists_identical FALSE)
    endif ()
  else ()
    math(EXPR len "${filtered_expect_length} - 1")
    foreach (index RANGE ${len})
      list(GET filtered_expect ${index} item1)
      list(GET output ${index} item2)
      if (NOT ${item1} STREQUAL ${item2})
          set(lists_identical FALSE)
          message(STATUS "The first difference:")
          message(STATUS "< ${item1}")
          message(STATUS "> ${item2}")
          break ()
      endif ()
    endforeach ()
  endif ()
endif (lists_identical)

if (NOT lists_identical)
  find_program(DIFF_CMD diff)
  if (DIFF_CMD)
    # Detected diff command, print a full diff for better debugging.
    list(JOIN output "" output)
    list(JOIN filtered_expect "" filtered_expect)
    set(tmp "${cmp}-out")
    set(tmp2 "${cmp}-expect")
    file(WRITE "${tmp}" "${output}")
    file(WRITE "${tmp2}" "${filtered_expect}")
    execute_process(COMMAND ${DIFF_CMD} ${tmp} ${tmp2}
      OUTPUT_VARIABLE dcmd_out)
    message(STATUS "diff: ${dcmd_out}")
  endif ()
  message(FATAL_ERROR "failed to match expected output.")
endif ()
