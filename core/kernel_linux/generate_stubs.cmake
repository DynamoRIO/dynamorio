# **********************************************************
# Copyright (c) 2026 Google, Inc.  All rights reserved.
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

# Generates weak stub definitions for unported symbols to allow
# the kernel module to load during incremental development.
#
# Arguments expected via -D flags:
#   MODULE_OBJECT - Path to dynamorio_module.o
#   KERNEL_DIR    - Path to KERNEL_DIR (containing Module.symvers)
#   OUTPUT_FILE   - Target path for generated stubs.c
#   NM_PROGRAM    - Path to nm binary (optional, defaults to "nm")

if (NOT DEFINED MODULE_OBJECT OR NOT DEFINED OUTPUT_FILE)
  message(FATAL_ERROR "MODULE_OBJECT and OUTPUT_FILE must be defined")
endif ()

if (NOT NM_PROGRAM)
  set(NM_PROGRAM "nm")
endif ()

# 1. Read exported kernel symbols from Module.symvers to avoid stubbing them
if (EXISTS "${KERNEL_DIR}/Module.symvers")
  file(STRINGS "${KERNEL_DIR}/Module.symvers" symvers_lines)
  foreach (line ${symvers_lines})
    # Line format: <crc>\t<symname>\t<module>\t<export_type>
    string(REGEX MATCH "^[^\t]+\t([^\t]+)" _match "${line}")
    if (CMAKE_MATCH_1)
      set(k_${CMAKE_MATCH_1} 1)
    endif ()
  endforeach ()
else ()
  message(WARNING
    "Module.symvers not found in KERNEL_DIR (${KERNEL_DIR}). "
    "Exported kernel symbols may be stubbed.")
endif ()

# 2. Extract undefined symbols from the composite object
execute_process(
  COMMAND ${NM_PROGRAM} -u "${MODULE_OBJECT}"
  OUTPUT_VARIABLE nm_output
  RESULT_VARIABLE nm_result
)

if (NOT nm_result EQUAL 0)
  message(FATAL_ERROR "Failed to run ${NM_PROGRAM} on ${MODULE_OBJECT}")
endif ()

string(REPLACE "\n" ";" nm_lines "${nm_output}")
set(missing_symbols "")
foreach (line ${nm_lines})
  string(REGEX MATCH "U ([a-zA-Z0-9_]+)" _match "${line}")
  if (CMAKE_MATCH_1)
    set(sym "${CMAKE_MATCH_1}")
    # Filter __this_module (defined in .mod.c by Kbuild) and kernel exports.
    if (NOT sym STREQUAL "__this_module" AND NOT DEFINED k_${sym})
      list(APPEND missing_symbols "${sym}")
    endif ()
  endif ()
endforeach ()

list(REMOVE_DUPLICATES missing_symbols)
list(SORT missing_symbols)

# 3. Generate stubs.c content
set(stub_content "/* Auto-generated weak stubs for unported DynamoRIO symbols */\n")
string(APPEND stub_content "#include <linux/bug.h>\n\n")

foreach (sym ${missing_symbols})
  string(APPEND stub_content "void __attribute__((weak)) ${sym}(void) { BUG(); }\n")
endforeach ()

file(WRITE "${OUTPUT_FILE}" "${stub_content}")
message(STATUS "Auto-generated weak stubs for ${OUTPUT_FILE}")
