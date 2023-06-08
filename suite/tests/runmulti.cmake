# **********************************************************
# Copyright (c) 2015-2023 Google, Inc.    All rights reserved.
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

# input:
# * precmd = pre processing command to run
# * cmd = command to run
#     should have intra-arg space=@@ and inter-arg space=@ and ;=!
# * postcmd = post processing command to run
# * postcmdN (for N=2+) = additional post processing commands to run
# * cmp = the file containing the expected output
# * failok = if ON then non-zero exit codes are ignored for cmd (and
#   postcmd* are still executed)
#
# A "*" in any command line will be glob-expanded right before running.
# If the command starts with "foreach@", instead of passing the glob-expansion
# to the single command, the command will be repeated for each expansion entry,
# and only one such expansion is supported (and it must be at the end).
# If the command starts with "firstglob@", only the first item in the
# glob-expansion is passed to the command.
# If the expansion is empty for precmd, the precmd execution is skipped.

# Intra-arg space=@@ and inter-arg space=@.
# XXX i#1327: now that we have -c and other option passing improvements we
# should be able to get rid of this @@ stuff.
macro(process_cmdline line skip_empty err_and_out)
  string(REGEX REPLACE "@@" " " ${line} "${${line}}")
  string(REGEX REPLACE "@" ";" ${line} "${${line}}")
  string(REGEX REPLACE "!" "\\\;" ${line} "${${line}}")
  # Clear to avoid repeating prior command if this one isn't run.
  set(cmd_err "")
  set(cmd_out "")

  if (${line} MATCHES "^foreach;")
    set(each ${${line}})
    list(REMOVE_AT each 0)
    list(LENGTH each len)
    math(EXPR len "${len} - 1")
    list(REMOVE_AT each ${len})
  endif ()

  set(globempty OFF)
  if (${line} MATCHES "\\*")
    set(newcmd "")
    foreach (token ${${line}})
      if (token MATCHES "\\*")
        file(GLOB expand ${token})
        if (expand STREQUAL "")
          set(globempty ON)
        endif ()
        if (${line} MATCHES "^foreach;")
          foreach (item ${expand})
            message("Running |${each} ${item}|")
            execute_process(COMMAND ${each} ${item}
              RESULT_VARIABLE cmd_result
              ERROR_VARIABLE cmd_err
              OUTPUT_VARIABLE cmd_out)
            if (cmd_result)
              message(FATAL_ERROR
                "*** ${${line}} failed (${cmd_result}): ${cmd_err}***\n")
            endif (cmd_result)
          endforeach ()
        elseif (${line} MATCHES "^firstglob;")
          list(GET expand 0 head)
          set(newcmd ${newcmd} ${head})
        else ()
          set(newcmd ${newcmd} ${expand})
        endif ()
      else ()
        if (NOT token STREQUAL "firstglob")
          set(newcmd ${newcmd} ${token})
        endif ()
      endif ()
    endforeach ()
    set(${line} ${newcmd})
  endif ()
  if (NOT ${line} MATCHES "^foreach;")
    if (NOT ${skip_empty} OR NOT ${line} STREQUAL "" AND NOT globempty)
      message("Running ${line} |${${line}}|")
      execute_process(COMMAND ${${line}}
        RESULT_VARIABLE cmd_result
        ERROR_VARIABLE cmd_err
        OUTPUT_VARIABLE cmd_out)
      if (cmd_result AND NOT failok)
        message(FATAL_ERROR "*** ${line} failed (${cmd_result}): ${cmd_err}***\n")
      endif ()
    endif ()
  endif ()
  set(${err_and_out} "${${err_and_out}}${cmd_err}${cmd_out}")
endmacro()

process_cmdline(precmd ON ignore)

process_cmdline(cmd OFF tomatch)

if (NOT "${postcmd}" STREQUAL "")
  process_cmdline(postcmd OFF tomatch)
  set(num 2)
  while (NOT "${postcmd${num}}" STREQUAL "")
    process_cmdline(postcmd${num} OFF tomatch)
    math(EXPR num "${num} + 1")
  endwhile ()
endif()

# get expected output (must already be processed w/ regex => literal, etc.)
file(READ "${cmp}" str)

if (NOT "${tomatch}" MATCHES "^${str}$")
  message(FATAL_ERROR "output |${tomatch}| failed to match expected output |${str}|")
endif ()
