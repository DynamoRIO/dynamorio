# **********************************************************
# Copyright (c) 2014-2015 Google, Inc.    All rights reserved.
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

# Invoked by the test suite for testing this tool

# input:
# * cmd = command to run
#     should have intra-arg space=@@ and inter-arg space=@ and ;=!
# * cmp = file containing output to compare app output to, to ensure app ran correctly
# * postcmd = post processing command to run

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

# get the real test name:
# CMake uses the first '.' to identify the longest extension, so we cannot use
# get_filename_component to get the real test name directly.
get_filename_component(test_name "${cmp}" NAME)
# tool.drcov.fib.expect => tool.drcov.fib
string(REGEX REPLACE "\\.[^.]+$" "" test_name ${test_name})
# tool.drcov.fib => fib
string(REGEX REPLACE "^.+\\.([^.]+)$" "\\1" test_name ${test_name})

FILE(GLOB drcov_logs "drcov.*${test_name}*.log")
set(cov_file "coverage.${test_name}")

file(READ ${cmp} expect)
if (WIN32)
  # our test prep turned \n into \r?\n so revert
  string(REGEX REPLACE "\r\\?" "" expect "${expect}")
endif (WIN32)

execute_process(COMMAND ${postcmd}
  -dir        ./
  -mod_filter ${test_name}
  -src_filter ${test_name}
  -output     ${cov_file}
  RESULT_VARIABLE cmd_result
  ERROR_VARIABLE cmd_err
  OUTPUT_VARIABLE cmd_out)
if (cmd_result)
  message(FATAL_ERROR "*** ${postcmd} failed (${cmd_result}): ${cmd_err} ${cmd_out}***\n")
endif (cmd_result)

file(READ ${cov_file} cov_out)

# cleanup
foreach(logfile ${drcov_logs})
  file(REMOVE ${logfile})
endforeach(logfile)
file(REMOVE ${cov_file})

if (NOT "${cov_out}" MATCHES "${expect}")
  message(FATAL_ERROR "tool output ${cov_out} failed to match expected ${expect}")
endif ()
