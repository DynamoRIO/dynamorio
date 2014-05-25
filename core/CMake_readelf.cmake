# **********************************************************
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

# caller must set READELF_EXECUTABLE so it can be a cache variable
# caller must also set "lib" to point to target library

# PR 212290: ensure no text relocations (they violate selinux execmod policies)
# looking for dynamic section tag:
#   0x00000016 (TEXTREL)                    0x0
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

