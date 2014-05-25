# **********************************************************
# Copyright (c) 2009-2010 VMware, Inc.    All rights reserved.
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

# For testing apps that need to be running while another action is taken
# in a separate process.
# FIXME i#120: add in all the runall/runalltest.sh tests

# input:
# * cmd = command to run in background that uses run_in_bg, which will
#     print the pid to stdout.
#     should have intra-arg space=@@ and inter-arg space=@ and ;=!
# * toolbindir
# * out = file where output of background process will be sent
# * nudge = arguments to nudgeunix
# * clear = dir to clear ahead of time

# intra-arg space=@@ and inter-arg space=@
string(REGEX REPLACE "@@" " " cmd "${cmd}")
string(REGEX REPLACE "@" ";" cmd "${cmd}")
string(REGEX REPLACE "!" "\\\;" cmd "${cmd}")

# we must remove so we know when the background process has re-created it
file(REMOVE "${out}")

if (NOT "${clear}" STREQUAL "")
  # clear out dir
  file(GLOB files "${clear}/*")
  if (NOT "${files}" STREQUAL "")
    file(REMOVE_RECURSE ${files})
  endif ()
endif ()

# run in the background
execute_process(COMMAND ${cmd}
  RESULT_VARIABLE cmd_result
  ERROR_VARIABLE cmd_err
  OUTPUT_VARIABLE pid OUTPUT_STRIP_TRAILING_WHITESPACE)
if (cmd_result)
  message(FATAL_ERROR "*** ${cmd} failed (${cmd_result}): ${cmd_err}***\n")
endif (cmd_result)

if (UNIX)
  find_program(SLEEP "sleep")
  if (NOT SLEEP)
    message(FATAL_ERROR "cannot find 'sleep'")
  endif (NOT SLEEP)
else (UNIX)
  message(FATAL_ERROR "need a sleep on Windows")
  # FIXME i#120: add tools/sleep.exe?
  # or use perl?  trying to get away from perl though
endif (UNIX)

while (NOT EXISTS "${out}")
  execute_process(COMMAND "${SLEEP}" 0.1)
endwhile ()
file(READ "${out}" output)
# we require that all runall tests write at least one line up front
while (NOT "${output}" MATCHES "\n")
  execute_process(COMMAND "${SLEEP}" 0.1)
  file(READ "${out}" output)
endwhile()

if ("${nudge}" MATCHES "<use-persisted>")
  # ensure using pcaches, instead of nudging
  file(READ "/proc/${pid}/maps" maps)
  if (NOT "${maps}" MATCHES "\\.dpc\n")
    set(fail_msg "no .dpc files found in ${maps}: not using pcaches!")
  endif ()
else ()
  execute_process(COMMAND "${toolbindir}/nudgeunix" -pid ${pid} ${nudge}
    RESULT_VARIABLE nudge_result
    ERROR_VARIABLE nudge_err
    OUTPUT_VARIABLE nudge_out
    )
  # combine out and err
  set(nudge_err "${nudge_out}${nudge_err}")
  if (nudge_result)
    message(FATAL_ERROR "*** nudgeunix failed (${nudge_result}): ${nudge_err}***\n")
  endif (nudge_result)
endif ()

if ("${nudge}" MATCHES "-client")
  # wait for more output to file
  string(LENGTH "${output}" prev_outlen)
  file(READ "${out}" output)
  string(LENGTH "${output}" new_outlen)
  while (NOT ${new_outlen} GREATER ${prev_outlen})
    execute_process(COMMAND "${SLEEP}" 0.1)
    file(READ "${out}" output)
    string(LENGTH "${output}" new_outlen)
  endwhile()
else ()
  # for reset or other DR tests there won't be further output
  # so we have to guess how long to wait.
  # FIXME: should we instead turn on stderr_mask?
  execute_process(COMMAND "${SLEEP}" 0.5)
endif ()

if (UNIX)
  find_program(KILL "kill")
  if (NOT KILL)
    message(FATAL_ERROR "cannot find 'kill'")
  endif (NOT KILL)
  execute_process(COMMAND "${KILL}" ${pid}
    RESULT_VARIABLE kill_result
    ERROR_VARIABLE kill_err
    OUTPUT_VARIABLE kill_out
    )
  # combine out and err
  set(kill_err "${kill_out}${kill_err}")
  if (kill_result)
    message(FATAL_ERROR "*** kill failed (${kill_result}): ${kill_err}***\n")
  endif (kill_result)
else (UNIX)
  # FIXME i#120: for Windows, use ${toolbindir}/DRkill.exe
endif (UNIX)

if (NOT "${fail_msg}" STREQUAL "")
  message(FATAL_ERROR "${fail_msg}")
endif ()

# we require that test print "done" as last line once done
file(READ "${out}" output)
while (NOT "${output}" MATCHES "\ndone\n")
  execute_process(COMMAND "${SLEEP}" 0.1)
  file(READ "${out}" output)
endwhile()

# message() adds a newline so removing any trailing newline
string(REGEX REPLACE "[ \n]+$" "" output "${output}")
message("${output}")

if (UNIX)
  # sometimes infloop keeps running: FIXME: figure out why
  execute_process(COMMAND "${KILL}" -9 ${pid} ERROR_QUIET OUTPUT_QUIET)
  find_program(PKILL "pkill")
  # we can't run pkill b/c there are other tests running infloop (i#1341)
endif (UNIX)
