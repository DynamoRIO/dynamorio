# **********************************************************
# Copyright (c) 2015-2016 Google, Inc.    All rights reserved.
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
# * cmp = the file containing the expected output
#
# A "*" in any command line will be glob-expanded right before running.
# If the expansion is empty for precmd, the precmd execution is skipped.

# Intra-arg space=@@ and inter-arg space=@.
# XXX i#1327: now that we have -c and other option passing improvements we
# should be able to get rid of this @@ stuff.
macro(process_cmdline var)
  string(REGEX REPLACE "@@" " " ${var} "${${var}}")
  string(REGEX REPLACE "@" ";" ${var} "${${var}}")
  string(REGEX REPLACE "!" "\\\;" ${var} "${${var}}")

  if (${var} MATCHES "\\*")
    set(newcmd "")
    foreach (token ${${var}})
      if (token MATCHES "\\*")
        file(GLOB expand ${token})
        if (expand STREQUAL "")
          set(globempty_${var} ON)
        endif ()
        set(newcmd ${newcmd} ${expand})
      else ()
        set(newcmd ${newcmd} ${token})
      endif ()
    endforeach ()
    set(${var} ${newcmd})
  endif ()
endmacro()

process_cmdline(precmd)

if (NOT precmd STREQUAL "" AND NOT globempty_precmd)
  execute_process(COMMAND ${precmd}
    RESULT_VARIABLE cmd_result
    ERROR_VARIABLE cmd_err
    OUTPUT_VARIABLE cmd_out)
  if (cmd_result)
    message(FATAL_ERROR "*** ${precmd} failed (${cmd_result}): ${cmd_err}***\n")
  endif (cmd_result)
endif ()

process_cmdline(cmd)

execute_process(COMMAND ${cmd}
  RESULT_VARIABLE cmd_result
  ERROR_VARIABLE cmd_err
  OUTPUT_VARIABLE cmd_out
  )
if (cmd_result)
  message(FATAL_ERROR "*** ${cmd} failed (${cmd_result}): ${cmd_err}***\n")
endif (cmd_result)
set(tomatch "${cmd_err}${cmd_out}")

process_cmdline(postcmd)

execute_process(COMMAND ${postcmd}
  RESULT_VARIABLE cmd_result
  ERROR_VARIABLE cmd_err
  OUTPUT_VARIABLE cmd_out)
if (cmd_result)
  message(FATAL_ERROR "*** ${postcmd} failed (${cmd_result}): ${cmd_err}***\n")
endif (cmd_result)
set(tomatch "${tomatch}${cmd_err}${cmd_out}")

# get expected output (must already be processed w/ regex => literal, etc.)
file(READ "${cmp}" str)

if (NOT "${tomatch}" MATCHES "${str}")
  message(FATAL_ERROR "output |${tomatch}| failed to match expected output |${str}|")
endif ()
