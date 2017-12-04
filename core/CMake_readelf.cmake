# **********************************************************
# Copyright (c) 2015-2017 Google, Inc.    All rights reserved.
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
# + READELF_EXECUTABLE so it can be a cache variable
# + "lib" to point to target library
# + "check_textrel" to ON or OFF to check to text relocations
# + "check_deps" to ON or OFF to check for zero deps
# + "check_libc" to ON or OFF to check for too-recent libc imports
# + "check_interp" to ON or OFF to check for a PT_INTERP section

# PR 212290: ensure no text relocations (they violate selinux execmod policies)
# looking for dynamic section tag:
#   0x00000016 (TEXTREL)                    0x0
if (check_textrel)
  execute_process(COMMAND
    ${READELF_EXECUTABLE} -d ${lib}
    RESULT_VARIABLE readelf_result
    ERROR_VARIABLE readelf_error
    OUTPUT_VARIABLE string
    )
  if (readelf_result OR readelf_error)
    message(FATAL_ERROR "*** ${READELF_EXECUTABLE} failed: ***\n${readelf_error}")
  endif (readelf_result OR readelf_error)
  string(REGEX MATCH
    "TEXTREL"
    has_textrel "${string}")
  if (has_textrel)
    message(FATAL_ERROR "*** Error: ${lib} has text relocations")
  endif (has_textrel)
endif ()

# also check for execstack
# looking for stack properties:
#   GNU_STACK      0x000000 0x00000000 0x00000000 0x00000 0x00000 RWE 0x4
execute_process(COMMAND
  ${READELF_EXECUTABLE} -l ${lib}
  RESULT_VARIABLE readelf_result
  ERROR_VARIABLE readelf_error
  OUTPUT_VARIABLE string
  )
if (readelf_result OR readelf_error)
  message(FATAL_ERROR "*** ${READELF_EXECUTABLE} failed: ***\n${readelf_error}")
endif (readelf_result OR readelf_error)
string(REGEX MATCH
  "STACK.*RWE"
  has_execstack "${string}")
if (has_execstack)
  message(FATAL_ERROR "*** Error: ${lib} has executable stack")
endif (has_execstack)

if (check_deps)
  # First, look for global undefined symbols:
  execute_process(COMMAND
    ${READELF_EXECUTABLE} -s ${lib}
    RESULT_VARIABLE readelf_result
    ERROR_VARIABLE readelf_error
    OUTPUT_VARIABLE string
    )
  if (readelf_result OR readelf_error)
    message(FATAL_ERROR "*** ${READELF_EXECUTABLE} failed: ***\n${readelf_error}")
  endif (readelf_result OR readelf_error)
  string(REGEX MATCH " GLOBAL [ A-Z]* UND " has_undefined "${string}")
  if (has_undefined)
    string(REGEX MATCH " GLOBAL [ A-Z]* UND *[^\n]*\n" symname "${string}")
    message(FATAL_ERROR "*** Error: ${lib} has undefined symbol: ${symname}")
  endif ()

  # Second, look for DT_NEEDED entries:
  execute_process(COMMAND
    ${READELF_EXECUTABLE} -d ${lib}
    RESULT_VARIABLE readelf_result
    ERROR_VARIABLE readelf_error
    OUTPUT_VARIABLE string
    )
  if (readelf_result OR readelf_error)
    message(FATAL_ERROR "*** ${READELF_EXECUTABLE} failed: ***\n${readelf_error}")
  endif (readelf_result OR readelf_error)
  string(REGEX MATCH "NEEDED" has_needed "${string}")
  if (has_needed)
    string(REGEX MATCH "NEEDED[^\n]*\n" libname "${string}")
    message(FATAL_ERROR "*** Error: ${lib} depends on: ${libname}")
  endif ()
endif ()

if (check_libc)
  execute_process(COMMAND
    ${READELF_EXECUTABLE} -s ${lib}
    RESULT_VARIABLE readelf_result
    ERROR_VARIABLE readelf_error
    OUTPUT_VARIABLE string
    )
  if (readelf_result OR readelf_error)
    message(FATAL_ERROR "*** ${READELF_EXECUTABLE} failed: ***\n${readelf_error}")
  endif (readelf_result OR readelf_error)
  # Avoid dependences beyond glibc 2.4 (2.17 on AArch64) for maximum backward
  # portability without going to extremes with a fixed toolchain (xref i#1504):
  execute_process(COMMAND
    ${READELF_EXECUTABLE} -h ${lib}
    OUTPUT_VARIABLE file_header_result
    )
  if (file_header_result MATCHES "AArch64")
    set (glibc_version_regexp "2\\.(1[8-9]|[2-9][0-9])")
  else ()
    set (glibc_version_regexp "2\\.([5-9]|[1-9][0-9])")
  endif ()
  string(REGEX MATCH " GLOBAL [ A-Z]* UND [^\n]*@GLIBC_${glibc_version_regexp}"
    has_recent "${string}")
  if (has_recent)
    string(REGEX MATCH " GLOBAL DEFAULT  UND [^\n]*@GLIBC_${glibc_version_regexp}[^\n]*\n"
      symname "${string}")
    message(FATAL_ERROR "*** Error: ${lib} has too-recent import: ${symname}")
  endif ()
endif ()

if (check_interp)
  execute_process(COMMAND
    ${READELF_EXECUTABLE} -l ${lib}
    RESULT_VARIABLE readelf_result
    ERROR_VARIABLE readelf_error
    OUTPUT_VARIABLE output
    )
  if (readelf_result OR readelf_error)
    message(FATAL_ERROR "*** ${READELF_EXECUTABLE} failed: ***\n${readelf_error}")
  endif (readelf_result OR readelf_error)
  if (output MATCHES " INTERP")
    message(FATAL_ERROR "*** Error: ${lib} contains an INTERP header (for Android, be sure you're using ld.bfd and not ld.gold)")
  endif ()
endif ()
