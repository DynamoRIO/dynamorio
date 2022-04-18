# **********************************************************
# Copyright (c) 2015-2019 Google, Inc.    All rights reserved.
# Copyright (c) 2009 VMware, Inc.    All rights reserved.
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

# Caller must set:
# + "lib_fileloc" to /absolute/path/<name> that represents a file <name>.cmake that
#   contains a set(<name> <path>) where path is the target binary location.
# + "CMAKE_C_COMPILER" to point to CC compiler
# + "partial_link_flags" to linker flags incl. -m32/-m64
# + "disable_pie_flag" to -no-pie or ""
# + "localize_hidden" to ON or OFF
# + "CMAKE_OBJCOPY" to point to objcopy
# + "CMAKE_AR" to point to ar
# + "CMAKE_RANLIB" to point to ranlib

include(${lib_fileloc}.cmake)

get_filename_component(lib_file ${lib_fileloc} NAME)

set(dynamorio_dot_a "${${lib_file}}")
string(REGEX REPLACE "\\.a$" ".o" dynamorio_dot_o "${dynamorio_dot_a}")

execute_process(COMMAND
  ${CMAKE_C_COMPILER} ${partial_link_flags} -fPIC
  -nostartfiles -nodefaultlibs -Wl,-r ${disable_pie_flag}
  -Wl,--whole-archive "${dynamorio_dot_a}" -Wl,--no-whole-archive
  -o ${dynamorio_dot_o}
  RESULT_VARIABLE cmd_result
  ERROR_VARIABLE cmd_error
  )

if (cmd_error OR cmd_result)
  message(FATAL_ERROR "*** ${CMAKE_C_COMPILER} failed (${cmd_result}): ***\n${cmd_error}")
endif ()

if (localize_hidden)
  execute_process(COMMAND
    ${CMAKE_OBJCOPY} --localize-hidden "${dynamorio_dot_o}"
    ERROR_VARIABLE cmd_error
    )
endif ()

if (cmd_error OR cmd_result)
  message(FATAL_ERROR "*** ${CMAKE_OBJCOPY} failed (${cmd_result}): ***\n${cmd_error}")
endif ()

# Localizing PIC thunks on ia32 creates references to discarded sections.
# XXX: Hardcoding this list of symbols is fragile.  It is known to work
# with gcc 4.4 and 4.6.  gcc 4.7 uses symbols starting with __x86.
execute_process(COMMAND
  ${CMAKE_OBJCOPY}
  --globalize-symbol=__i686.get_pc_thunk.bx
  --globalize-symbol=__i686.get_pc_thunk.cx
  --globalize-symbol=__x86.get_pc_thunk.ax
  --globalize-symbol=__x86.get_pc_thunk.bx
  --globalize-symbol=__x86.get_pc_thunk.cx
  --globalize-symbol=__x86.get_pc_thunk.dx
  --globalize-symbol=__x86.get_pc_thunk.di
  --globalize-symbol=__x86.get_pc_thunk.si
  --globalize-symbol=__x86.get_pc_thunk.bp
  "${dynamorio_dot_o}"
  ERROR_VARIABLE cmd_error
  )

if (cmd_error OR cmd_result)
  message(FATAL_ERROR "*** ${CMAKE_OBJCOPY} failed (${cmd_result}): ***\n${cmd_error}")
endif ()

execute_process(COMMAND
  rm -f "${dynamorio_dot_a}"
  )

execute_process(COMMAND
  ${CMAKE_AR} cr "${dynamorio_dot_a}" "${dynamorio_dot_o}"
  ERROR_VARIABLE cmd_error
  )

if (cmd_error OR cmd_result)
  message(FATAL_ERROR "*** ${CMAKE_AR} failed (${cmd_result}): ***\n${cmd_error}")
endif ()

execute_process(COMMAND
  ${CMAKE_RANLIB} "${dynamorio_dot_a}"
  ERROR_VARIABLE cmd_error
  )

if (cmd_error OR cmd_result)
  message(FATAL_ERROR "*** ${CMAKE_RANLIB} failed (${cmd_result}): ***\n${cmd_error}")
endif ()
