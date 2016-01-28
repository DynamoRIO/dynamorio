# **********************************************************
# Copyright (c) 2013-2015 Google, Inc.    All rights reserved.
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

# Arguments:
# + outfile                 = path to resulting script
# + CMAKE_LINKER            = path to ld
# + CMAKE_COMPILER_IS_GNUCC = is it gcc?
# + LD_FLAGS                = extra ld flags
# + set_preferred           = whether to set preferred base
# + preferred_base          = value of preferred base
# + replace_maxpagesize     = replace the max page size?
# + add_bounds_vars         = add bounds vars (hardcoded to dynamorio name)

if (APPLE OR NOT CMAKE_COMPILER_IS_GNUCC)
  message(FATAL_ERROR "this script is only for gnu ld")
endif (APPLE OR NOT CMAKE_COMPILER_IS_GNUCC)

# In order to just tweak the default linker script we start with exactly that.
separate_arguments(LD_FLAGS)
execute_process(COMMAND
  ${CMAKE_LINKER} ${LD_FLAGS} --verbose
  RESULT_VARIABLE ld_result
  ERROR_VARIABLE ld_error
  OUTPUT_VARIABLE string
  )
if (ld_result OR ld_error)
  message(FATAL_ERROR "*** ${CMAKE_LINKER} failed: ***\n${ld_error}")
endif (ld_result OR ld_error)

# strip out just the SECTIONS{} portion
string(REGEX REPLACE
  ".*(SECTIONS.*\n\\}).*"
  "\\1"
  string "${string}")

# We need default base for older ld and for set_preferred
# We expect:
#   ld -melf_x86_64 --verbose (version 2.19.51)
#     PROVIDE (__executable_start = SEGMENT_START("text-segment", 0x400000)); . = SEGMENT_START("text-segment", 0x400000) + SIZEOF_HEADERS;
#   ld -melf_x86_64 --verbose (version 2.18.50)
#     PROVIDE (__executable_start = 0x400000); . = 0x400000 + SIZEOF_HEADERS;
#   ld -melf_i386 --verbose
#     PROVIDE (__executable_start = 0x08048000); . = 0x08048000 + SIZEOF_HEADERS;
#   rhel3 ld --verbose (version 2.14.90)
#     . = 0x08048000 + SIZEOF_HEADERS;
string(REGEX MATCH
  "= *[^{\\.\n]+(0x[0-9]+)\\)? *\\+ *SIZEOF_HEADERS"
  default_base "${string}")
string(REGEX REPLACE
  ".*(0x[0-9]+).*"
  "\\1"
  default_base "${default_base}")
# Older ld (like 2.14.90 on rhel3) doesn't have __executable_start symbol
string(REGEX MATCH
  "__executable_start" has_executable_start "${string}")
if (NOT has_executable_start)
  string(REGEX REPLACE
    "(\n{)"
    "\\1\n  PROVIDE (__executable_start = ${default_base});"
    string "${string}")
endif (NOT has_executable_start)

if (add_bounds_vars)
  # PR 361954: set up vars pointing at our own library bounds using the
  # symbols __executable_start and end defined in default script
  string(REGEX REPLACE
    "(\n})"
    "\n  PROVIDE(dynamorio_so_start = __executable_start); PROVIDE(dynamorio_so_end = end);\\1"
    string "${string}")
endif ()

# PR 253624: right now our library has to be next to our heap, and our
# heap needs to be in lower 4GB for our fallback TLS (PR 285410), so we
# set a preferred base address via a linker script.
if (set_preferred)
  string(REGEX REPLACE
    "${default_base}"
    "${preferred_base}"
    string "${string}")
  string(REGEX REPLACE
    "(\n{)"
    "\\1\n  . = ${preferred_base};"
    string "${string}")
endif (set_preferred)

if (replace_maxpagesize)
  # make drloader smaller by replacing maxpagesize in ldscript
  string(REGEX REPLACE
    "CONSTANT \\(MAXPAGESIZE\\)"
    "0x10000"
    string "${string}")
endif (replace_maxpagesize)

file(WRITE ${outfile} "${string}")
