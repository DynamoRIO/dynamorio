# **********************************************************
# Copyright (c) 2018-2022 Google, Inc.    All rights reserved.
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
# * pidfile = file where the pid of the background process will be written
# * nudge = arguments to drnudgeunix or drconfig
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

# Remove any stale files.
if (pidfile)
  file(REMOVE ${pidfile})
endif ()
file(REMOVE ${out})

# Run the target in the background.
execute_process(COMMAND ${cmd}
  RESULT_VARIABLE cmd_result
  ERROR_VARIABLE cmd_err
  OUTPUT_VARIABLE pid OUTPUT_STRIP_TRAILING_WHITESPACE
  TIMEOUT 90)
if (cmd_result)
  message(FATAL_ERROR "*** ${cmd} failed (${cmd_result}): ${cmd_err}***\n")
endif (cmd_result)

if (UNIX)
  find_program(SLEEP "sleep")
  if (NOT SLEEP)
    message(FATAL_ERROR "cannot find 'sleep'")
  endif ()
  set(nudge_cmd drnudgeunix)
else (UNIX)
  # We use "ping" on Windows to "sleep" :)
  find_program(PING "ping")
  if (NOT PING)
    message(FATAL_ERROR "cannot find 'ping'")
  endif ()
  set(nudge_cmd drconfig)
endif (UNIX)

if (UNIX)
  set(MAX_ITERS 50000)
else ()
  # Sleeping in longer units.
  set(MAX_ITERS 1000)
endif()

function (do_sleep ms)
  if (UNIX)
    execute_process(COMMAND "${SLEEP}" ${ms})
  else ()
    # XXX: ping's units are secs.  For now we always do 1 sec.
    # We could try to use cygwin bash or perl.
    execute_process(COMMAND ${PING} 127.0.0.1 -n 2 OUTPUT_QUIET)
  endif ()
endfunction (do_sleep)

function (kill_background_process force)
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
    # win32.infloop has a title with the pid in it so we can uniquely target it
    # for a cleaner exit than using drkill.
    execute_process(COMMAND "${toolbindir}/closewnd.exe" "Infloop pid=${pid}" 10
      RESULT_VARIABLE kill_result
      ERROR_VARIABLE kill_err
      OUTPUT_VARIABLE kill_out)
    set(kill_err "${kill_out}${kill_err}")
    if (kill_result)
      message(FATAL_ERROR "*** kill failed (${kill_result}): ${kill_err}***\n")
    endif (kill_result)
    if (force)
      # However, if infloop hung before it drew the window, it might still be up.
      # Ensure it's not.
      execute_process(COMMAND "${toolbindir}/drkill" -pid ${pid})
    endif ()
  endif (UNIX)
endfunction ()

if (pidfile)
  set(iters 0)
  while (NOT EXISTS "${pidfile}")
    do_sleep(0.1)
    math(EXPR iters "${iters}+1")
    if (${iters} GREATER ${MAX_ITERS})
      kill_background_process(ON)
      message(FATAL_ERROR "Timed out waiting for ${pidfile}")
    endif ()
  endwhile ()
  file(READ "${pidfile}" pid)
  set(iters 0)
  while ("${pid}" STREQUAL "")
    do_sleep(0.1)
    math(EXPR iters "${iters}+1")
    if (${iters} GREATER ${MAX_ITERS})
      kill_background_process(ON)
      message(FATAL_ERROR "Timed out waiting for ${pidfile} content")
    endif ()
    file(READ "${pidfile}" pid)
  endwhile ()
  string(REGEX REPLACE "\n" "" pid ${pid})
endif ()

set(iters 0)
while (NOT EXISTS "${out}")
  do_sleep(0.1)
  math(EXPR iters "${iters}+1")
  if (${iters} GREATER ${MAX_ITERS})
    kill_background_process(ON)
    message(FATAL_ERROR "Timed out waiting for ${out}")
  endif ()
endwhile ()
file(READ "${out}" output)
# we require that all runall tests write at least one line up front
set(iters 0)
while (NOT "${output}" MATCHES "\n")
  do_sleep(0.1)
  file(READ "${out}" output)
  math(EXPR iters "${iters}+1")
  if (${iters} GREATER ${MAX_ITERS})
    kill_background_process(ON)
    message(FATAL_ERROR "Timed out waiting for newline")
  endif ()
endwhile()

set(orig_nudge "${nudge}")
if ("${nudge}" MATCHES "<use-persisted>")
  # ensure using pcaches, instead of nudging
  file(READ "/proc/${pid}/maps" maps)
  if (NOT "${maps}" MATCHES "\\.dpc\n")
    set(fail_msg "no .dpc files found in ${maps}: not using pcaches!")
  endif ()
elseif ("${nudge}" MATCHES "<attach>")
  set(nudge_cmd run_in_bg)
  string(REGEX REPLACE "<attach>"
    "${toolbindir}/drrun@-attach@${pid}@-takeover_sleep@-takeovers@1000"
    nudge "${nudge}")
  string(REGEX REPLACE "@" ";" nudge "${nudge}")
  execute_process(COMMAND "${toolbindir}/${nudge_cmd}" ${nudge}
   RESULT_VARIABLE nudge_result
   ERROR_VARIABLE nudge_err
   OUTPUT_VARIABLE nudge_out
   )
  # combine out and err
  set(nudge_err "${nudge_out}${nudge_err}")
  if (nudge_result)
    kill_background_process(ON)
    message(FATAL_ERROR "*** ${nudge_cmd} failed (${nudge_result}): ${nudge_err}***\n")
  endif (nudge_result)
elseif ("${nudge}" MATCHES "<detach>")
  set(nudge_cmd run_in_bg)
  string(REGEX REPLACE "<detach>"
    "${toolbindir}/drrun@-attach@${pid}@-takeover_sleep@-takeovers@1000"
    nudge "${nudge}")
  string(REGEX REPLACE "@" ";" nudge "${nudge}")
  execute_process(COMMAND "${toolbindir}/${nudge_cmd}" ${nudge}
    RESULT_VARIABLE nudge_result
    ERROR_VARIABLE nudge_err
    OUTPUT_VARIABLE nudge_out
    )
  # Combine out and err.
  set(nudge_err "${nudge_out}${nudge_err}")
  if (nudge_result)
    kill_background_process(ON)
    message(FATAL_ERROR "*** ${nudge_cmd} failed (${nudge_result}): ${nudge_err}***\n")
  endif (nudge_result)
else ()
  # drnudgeunix and drconfig have different syntax:
  if (WIN32)
    # XXX i#120: expand beyond -client.
    string(REGEX REPLACE "-client" "-nudge_timeout;30000;-nudge_pid;${pid}"
      nudge "${nudge}")
  else ()
    set(nudge "-pid;${pid};${nudge}")
  endif ()
  execute_process(COMMAND "${toolbindir}/${nudge_cmd}" ${nudge}
    RESULT_VARIABLE nudge_result
    ERROR_VARIABLE nudge_err
    OUTPUT_VARIABLE nudge_out
    )
  # combine out and err
  set(nudge_err "${nudge_out}${nudge_err}")
  if (nudge_result)
    kill_background_process(ON)
    message(FATAL_ERROR "*** ${nudge_cmd} failed (${nudge_result}): ${nudge_err}***\n")
  endif (nudge_result)
endif ()

if ("${orig_nudge}" MATCHES "-client")
  # wait for more output to file
  string(LENGTH "${output}" prev_outlen)
  file(READ "${out}" output)
  string(LENGTH "${output}" new_outlen)
  set(iters 0)
  while (NOT ${new_outlen} GREATER ${prev_outlen})
    do_sleep(0.1)
    file(READ "${out}" output)
    string(LENGTH "${output}" new_outlen)
    math(EXPR iters "${iters}+1")
    if (${iters} GREATER ${MAX_ITERS})
      kill_background_process(ON)
      message(FATAL_ERROR "Timed out waiting for more output")
    endif ()
  endwhile()
elseif ("${orig_nudge}" MATCHES "<attach>" OR "${orig_nudge}" MATCHES "<detach>")
  # Wait until attached.
  set(iters 0)
  while (NOT "${output}" MATCHES "attach\n")
    do_sleep(0.1)
    file(READ "${out}" output)
    math(EXPR iters "${iters}+1")
    if (${iters} GREATER ${MAX_ITERS})
      kill_background_process(ON)
      message(FATAL_ERROR "Timed out waiting for attach")
    endif ()
  endwhile()
  # Wait until thread init.
  set(iters 0)
  while (NOT "${output}" MATCHES "thread init\n")
    do_sleep(0.1)
    file(READ "${out}" output)
    math(EXPR iters "${iters}+1")
    if (${iters} GREATER ${MAX_ITERS})
      kill_background_process(ON)
      message(FATAL_ERROR "Timed out waiting for attach")
    endif ()
  endwhile()
else ()
  # for reset or other DR tests there won't be further output
  # so we have to guess how long to wait.
  # FIXME: should we instead turn on stderr_mask?
  do_sleep(0.5)
endif ()

if ("${orig_nudge}" MATCHES "<detach>")
  execute_process(COMMAND "${toolbindir}/drconfig.exe" "-detach" ${pid}
    RESULT_VARIABLE detach_result
    ERROR_VARIABLE  detach_err
    OUTPUT_VARIABLE detach_out)
  set(detach_err "${detach_out}${detach_err}")
  if (detach_result)
    message(FATAL_ERROR "*** detach failed (${detach_result}): ${detach_err}***\n")
  endif (detach_result)
  # Wait until detach.
  set(iters 0)
  while (NOT "${output}" MATCHES "detach\n")
    do_sleep(0.1)
    file(READ "${out}" output)
    math(EXPR iters "${iters}+1")
    if (${iters} GREATER ${MAX_ITERS})
      kill_background_process(ON)
      message(FATAL_ERROR "Timed out waiting for attach")
    endif ()
  endwhile()
endif()

kill_background_process(OFF)

if (NOT "${fail_msg}" STREQUAL "")
  message(FATAL_ERROR "${fail_msg}")
endif ()

# we require that test print "done" as last line once done
file(READ "${out}" output)
set(iters 0)
while (NOT "${output}" MATCHES "\ndone\n")
  do_sleep(0.1)
  file(READ "${out}" output)
  if ("${output}" MATCHES "Internal Error")
    string(REGEX MATCH "Application .*Internal Error.*\n" msg "${output}")
    message(FATAL_ERROR "Found assert: |${msg}|")
  endif ()
  math(EXPR iters "${iters}+1")
  if (${iters} GREATER ${MAX_ITERS})
    message(FATAL_ERROR "Timed out waiting for \"done\"")
  endif ()
endwhile()

# message() adds a newline so removing any trailing newline
string(REGEX REPLACE "[ \n]+$" "" output "${output}")
message("${output}")

# Sometimes infloop keeps running: FIXME: figure out why.
if (UNIX)
  execute_process(COMMAND "${KILL}" -9 ${pid} ERROR_QUIET OUTPUT_QUIET)
  # we can't run pkill b/c there are other tests running infloop (i#1341)
else ()
  execute_process(COMMAND "${toolbindir}/DRkill.exe" -pid ${pid} ERROR_QUIET OUTPUT_QUIET)
endif ()
