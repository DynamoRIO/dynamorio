# **********************************************************
# Copyright (c) 2012 Google, Inc.    All rights reserved.
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

# Ensures there are no dependencies other than ntdll.
# Caller must set:
# + DUMPBIN_EXECUTABLE
# + lib

# Ensure output goes to std{out,err} (i#951)
set(ENV{VS_UNICODE_OUTPUT} "")

execute_process(COMMAND
  ${DUMPBIN_EXECUTABLE} /dependents ${lib}
  RESULT_VARIABLE deps_result
  ERROR_VARIABLE deps_error
  OUTPUT_VARIABLE deps_out
  )
if (deps_result OR deps_error)
  message(FATAL_ERROR "*** ${DEPS_EXECUTABLE} failed: ***\n${deps_error}")
endif (deps_result OR deps_error)
string(REGEX MATCH "following dependencies:.*Summary" dlls "${deps_out}")
string(REGEX REPLACE "\r?\n" "" dlls "${dlls}")
string(REGEX REPLACE "following dependencies: *" "" dlls "${dlls}")
string(REGEX REPLACE " *Summary" "" dlls "${dlls}")
if (NOT dlls MATCHES "^ntdll.dll$")
  message(FATAL_ERROR "*** Error: ${lib} depends on more than ntdll.dll: ${dlls}")
endif ()

