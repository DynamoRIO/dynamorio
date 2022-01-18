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
# + "READELF_EXECUTABLE" so it can be a cache variable
# + "check_textrel" to ON or OFF to check to text relocations
# + "check_deps" to ON or OFF to check for zero deps
# + "check_libc" to ON or OFF to check for too-recent libc imports
# + "check_interp" to ON or OFF to check for a PT_INTERP section

# PR 212290: ensure no text relocations (they violate selinux execmod policies)
# looking for dynamic section tag:
#   0x00000016 (TEXTREL)                    0x0

include(${lib_fileloc}.cmake)

get_filename_component(lib_file ${lib_fileloc} NAME)

if (check_textrel)
  execute_process(COMMAND
    ${READELF_EXECUTABLE} -d ${${lib_file}}
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
    message(FATAL_ERROR "*** Error: ${${lib_file}} has text relocations")
  endif (has_textrel)
endif ()

# also check for execstack
# looking for stack properties:
#   GNU_STACK      0x000000 0x00000000 0x00000000 0x00000 0x00000 RWE 0x4
execute_process(COMMAND
  ${READELF_EXECUTABLE} -l ${${lib_file}}
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
  message(FATAL_ERROR "*** Error: ${${lib_file}} has executable stack")
endif (has_execstack)

if (check_deps)
  # First, look for global undefined symbols:
  execute_process(COMMAND
    ${READELF_EXECUTABLE} -s ${${lib_file}}
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
    message(FATAL_ERROR "*** Error: ${${lib_file}} has undefined symbol: ${symname}")
  endif ()

  # Second, look for DT_NEEDED entries:
  execute_process(COMMAND
    ${READELF_EXECUTABLE} -d ${${lib_file}}
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
    message(FATAL_ERROR "*** Error: ${${lib_file}} depends on: ${libname}")
  endif ()
endif ()

if (check_libc)
  execute_process(COMMAND
    ${READELF_EXECUTABLE} -s ${${lib_file}}
    RESULT_VARIABLE readelf_result
    ERROR_VARIABLE readelf_error
    OUTPUT_VARIABLE string
    )
  if (readelf_result OR readelf_error)
    message(FATAL_ERROR "*** ${READELF_EXECUTABLE} failed: ***\n${readelf_error}")
  endif (readelf_result OR readelf_error)

  # Avoid dependences beyond the oldest well-supported version of glibc for maximum backward
  # portability without going to extremes with a fixed toolchain (xref i#1504):
  execute_process(COMMAND
    ${READELF_EXECUTABLE} -h ${${lib_file}}
    OUTPUT_VARIABLE file_header_result
    )

  # To determine the minimum version of glibc that we should support for packaging, we should
  # check the Linux distributions that are known not to be rolling releases and offer long
  # support, and then check the oldest option available that is not yet EOL. As of writing,
  # these are:
  #  * Debian Stretch, which is on glibc 2.24
  #  * CentOS 7/RHEL 7, which is on glibc 2.17
  #  * Ubuntu 16.04 LTS (Xenial), which is on glibc 2.23
  #
  # Therefore, we want to support at least glibc 2.17.
  #
  # The glibc version is independent of the architecture. For instance, RHEL 7 for AArch64 also
  # ships glibc 2.17 as of writing.
  set (glibc_version "2.17")

  set (glibc_version_regexp " GLOBAL [ A-Z]* UND [^\n]*@GLIBC_([0-9]+\\.[0-9]+)")
  string(REGEX MATCHALL "${glibc_version_regexp}" imports "${string}")

  foreach (import ${imports})
    string(REGEX MATCH "${glibc_version_regexp}" match "${import}")
    set(version ${CMAKE_MATCH_1})

    if (${version} VERSION_GREATER ${glibc_version})
      set(too_recent "${too_recent}\n  ${import}")
    endif ()
  endforeach ()

  if (too_recent)
    message(FATAL_ERROR "*** Error: ${${lib_file}} has too-recent import(s):${too_recent}")
  endif ()
endif ()

if (check_interp)
  execute_process(COMMAND
    ${READELF_EXECUTABLE} -l ${${lib_file}}
    RESULT_VARIABLE readelf_result
    ERROR_VARIABLE readelf_error
    OUTPUT_VARIABLE output
    )
  if (readelf_result OR readelf_error)
    message(FATAL_ERROR "*** ${READELF_EXECUTABLE} failed: ***\n${readelf_error}")
  endif (readelf_result OR readelf_error)
  if (output MATCHES " INTERP")
    message(FATAL_ERROR "*** Error: ${${lib_file}} contains an INTERP header (for Android, be sure you're using ld.bfd and not ld.gold)")
  endif ()
endif ()
