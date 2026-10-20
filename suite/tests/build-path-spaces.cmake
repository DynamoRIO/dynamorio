# **********************************************************
# Copyright (c) Bryan Berns    All rights reserved.
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

# Exercise the linker with a real nested build whose import-library path contains spaces.
set(generator_args -G "${generator}")
if (NOT "${platform}" STREQUAL "")
  list(APPEND generator_args -A "${platform}")
endif ()
if (NOT "${toolset}" STREQUAL "")
  list(APPEND generator_args -T "${toolset}")
endif ()
execute_process(COMMAND "${CMAKE_COMMAND}" -S "${source}" -B "${binary}"
  ${generator_args} "-DDEBUG:BOOL=${debug}" -DBUILD_TESTS=OFF -DBUILD_EXT=OFF
  -DBUILD_TOOLS=OFF -DBUILD_CLIENTS=OFF -DBUILD_DOCS=OFF -DBUILD_SAMPLES=OFF
  "-DDISABLE_WARNINGS:BOOL=${disable_warnings}"
  RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
if (NOT result EQUAL 0)
  message(FATAL_ERROR "Spaced-path configuration failed: ${output}\n${error}")
endif ()
execute_process(COMMAND "${CMAKE_COMMAND}" --build "${binary}" --config "${config}"
  --target dynamorio --parallel 4
  RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
if (NOT result EQUAL 0)
  message(FATAL_ERROR "Spaced-path runtime build failed: ${output}\n${error}")
endif ()
