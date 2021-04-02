# **********************************************************
# Copyright (c) 2020-2021 Google, Inc.   All rights reserved.
# Copyright (c) 2018 Arm Limited    All rights reserved.
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

# Commands to automatically create the codec files from core/arch/aarch64/codec.txt.
find_package(PythonInterp)

if (NOT PYTHONINTERP_FOUND)
  message(FATAL_ERROR "Python interpreter not found")
endif ()

set(AARCH64_CODEC_GEN_SRCS
  ${PROJECT_BINARY_DIR}/opcode_api.h
  ${PROJECT_BINARY_DIR}/decode_gen.h
  ${PROJECT_BINARY_DIR}/encode_gen.h
  ${PROJECT_BINARY_DIR}/opcode_names.h
)
set_source_files_properties(${AARCH64_CODEC_GEN_SRCS} PROPERTIES GENERATED true)

set(CODEC_TXT "${PROJECT_SOURCE_DIR}/core/ir/${ARCH_NAME}/codec.txt")

execute_process(
  COMMAND ${PYTHON_EXECUTABLE}
          "${PROJECT_SOURCE_DIR}/make/aarch64_check_codec_order.py"
          "${PROJECT_SOURCE_DIR}/core/ir/aarch64"
  RESULT_VARIABLE CHECK_RC
  OUTPUT_VARIABLE CHECK_MSG
)

if (CHECK_RC)
  message(FATAL_ERROR "Wrong order in codec.txt or codec.c: ${CHECK_MSG}")
endif()

# Auto-generate decoder files from codec.txt.
add_custom_command(
  OUTPUT  ${AARCH64_CODEC_GEN_SRCS}
  DEPENDS ${PROJECT_SOURCE_DIR}/core/ir/${ARCH_NAME}/codec.py
          ${PROJECT_SOURCE_DIR}/core/ir/${ARCH_NAME}/codec.txt
  COMMAND ${PYTHON_EXECUTABLE}
  ARGS ${PROJECT_SOURCE_DIR}/core/ir/${ARCH_NAME}/codec.py
       ${PROJECT_SOURCE_DIR}/core/ir/${ARCH_NAME}/codec.txt
       ${PROJECT_BINARY_DIR}
  VERBATIM # recommended: p260
)
