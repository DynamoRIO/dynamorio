# **********************************************************
# Copyright (c) 2019-2020 Google, Inc.    All rights reserved.
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

# i#3348: Avoid symbol conflicts when linking DR statically.

# Caller must set:
# + "lib_fileloc" to /absolute/path/<name> that represents a file <name>.cmake that
#   contains a set(<name> <path>) where path is the target binary location.
# + "READELF_EXECUTABLE" so it can be a cache variable
# + "CMAKE_SYSTEM_PROCESSOR"

include(${lib_fileloc}.cmake)

# Return whether binutils is version 2.30 (xref i#3450).
function (is_i3450_binutils_version var_out)
  set(readelfver ${READELF_EXECUTABLE} --version)
  execute_process(COMMAND ${readelfver}
    RESULT_VARIABLE readelf_result
    ERROR_QUIET
    OUTPUT_VARIABLE readelf_out)
  set(is_i3450 0)
  if (readelf_result)
    message("Failed to get readelf version.")
  elseif ("${readelf_out}" MATCHES "GNU readelf")
    string(REGEX MATCH "[0-9.].*$" readelf_verno "${readelf_out}")
  endif ()
  if (readelf_verno EQUAL 2.30)
    set(is_i3450 1)
  endif ()
  set(${var_out} ${is_i3450} PARENT_SCOPE)
endfunction (is_i3450_binutils_version)

get_filename_component(lib_file ${lib_fileloc} NAME)

# Get the list of symbols.
execute_process(COMMAND
  ${READELF_EXECUTABLE} -s ${${lib_file}}
  RESULT_VARIABLE readelf_result
  ERROR_VARIABLE readelf_error
  OUTPUT_VARIABLE output
  )

# Check for binutils/readelf version 2.30, and if yes, then ignore error for 32-bit
# release build. Binutils bug https://sourceware.org/bugzilla/show_bug.cgi?id=24382
# has been filed. Both earlier and later versions seem ok (xref i#3450).
is_i3450_binutils_version(is_i3450)

if (X86 AND NOT X64 AND NOT DEBUG AND is_i3450)
  set(readelf_error "")
endif ()
if (readelf_result OR readelf_error)
  message(FATAL_ERROR "*** ${READELF_EXECUTABLE} failed: ***\n${readelf_error}")
endif (readelf_result OR readelf_error)

# Limit to global defined symbols: no "UND".
string(REGEX MATCHALL "([^\n]+ GLOBAL [A-Z]+ +[^U ]+ [^\n]+)\n" globals "${output}")

# Now limit to single-word symbols, which are most likely to conflict.
string(REGEX MATCHALL " [^_\\.]+\n" oneword "${globals}")

string(REGEX MATCHALL "([^\n]+)\n" lines "${oneword}")
foreach(line ${lines})
  set(is_ok OFF)
  # We have some legacy exceptions we allow until we've renamed them all.
  if (line MATCHES "decode\n" OR
      line MATCHES "disassemble\n")
    # OK: an exception we allow for now.
    set(is_ok ON)
  endif ()
  if (NOT is_ok)
    message(FATAL_ERROR
      "*** Error: ${${lib_file}} contains a likely-to-conflict symbol: ${line}")
  endif ()
endforeach()
