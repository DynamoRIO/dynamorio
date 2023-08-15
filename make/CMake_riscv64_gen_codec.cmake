# **********************************************************
# Copyright (c) 2022 Rivos, Inc.    All rights reserved.
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
# * Neither the name of Rivos, Inc. nor the names of its contributors may be
#   used to endorse or promote products derived from this software without
#   specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED. IN NO EVENT SHALL RIVOS, INC. OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
# SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
# CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
# OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
# DAMAGE.

# Commands to automatically create the codec files from
# core/arch/riscv64/isl/*.txt.
find_package(PythonInterp)

if (NOT PYTHONINTERP_FOUND)
  message(FATAL_ERROR "Python interpreter not found")
endif ()

set(RISCV64_CODEC_GEN_SRCS
  ${PROJECT_BINARY_DIR}/opcode_api.h
  ${PROJECT_BINARY_DIR}/instr_create_api.h
  ${PROJECT_BINARY_DIR}/instr_info_trie.h
)
set_source_files_properties(${RISCV64_CODEC_GEN_SRCS} PROPERTIES GENERATED true)

# Auto-generate decoder files core/arch/riscv64/isl/*.txt. files.
add_custom_command(
  OUTPUT  ${RISCV64_CODEC_GEN_SRCS}
  DEPENDS ${PROJECT_SOURCE_DIR}/core/ir/${ARCH_NAME}/codec.py
          ${PROJECT_SOURCE_DIR}/core/ir/${ARCH_NAME}/isl/rv32a.txt
          ${PROJECT_SOURCE_DIR}/core/ir/${ARCH_NAME}/isl/rv32c.txt
          ${PROJECT_SOURCE_DIR}/core/ir/${ARCH_NAME}/isl/rv32d.txt
          ${PROJECT_SOURCE_DIR}/core/ir/${ARCH_NAME}/isl/rv32f.txt
          ${PROJECT_SOURCE_DIR}/core/ir/${ARCH_NAME}/isl/rv32h.txt
          ${PROJECT_SOURCE_DIR}/core/ir/${ARCH_NAME}/isl/rv32i.txt
          ${PROJECT_SOURCE_DIR}/core/ir/${ARCH_NAME}/isl/rv32m.txt
          ${PROJECT_SOURCE_DIR}/core/ir/${ARCH_NAME}/isl/rv32q.txt
          ${PROJECT_SOURCE_DIR}/core/ir/${ARCH_NAME}/isl/rv32zba.txt
          ${PROJECT_SOURCE_DIR}/core/ir/${ARCH_NAME}/isl/rv32zbb.txt
          ${PROJECT_SOURCE_DIR}/core/ir/${ARCH_NAME}/isl/rv32zbc.txt
          ${PROJECT_SOURCE_DIR}/core/ir/${ARCH_NAME}/isl/rv32zbs.txt
          ${PROJECT_SOURCE_DIR}/core/ir/${ARCH_NAME}/isl/rv64a.txt
          ${PROJECT_SOURCE_DIR}/core/ir/${ARCH_NAME}/isl/rv64c.txt
          ${PROJECT_SOURCE_DIR}/core/ir/${ARCH_NAME}/isl/rv64d.txt
          ${PROJECT_SOURCE_DIR}/core/ir/${ARCH_NAME}/isl/rv64f.txt
          ${PROJECT_SOURCE_DIR}/core/ir/${ARCH_NAME}/isl/rv64h.txt
          ${PROJECT_SOURCE_DIR}/core/ir/${ARCH_NAME}/isl/rv64i.txt
          ${PROJECT_SOURCE_DIR}/core/ir/${ARCH_NAME}/isl/rv64m.txt
          ${PROJECT_SOURCE_DIR}/core/ir/${ARCH_NAME}/isl/rv64q.txt
          ${PROJECT_SOURCE_DIR}/core/ir/${ARCH_NAME}/isl/rv64zba.txt
          ${PROJECT_SOURCE_DIR}/core/ir/${ARCH_NAME}/isl/rv64zbb.txt
          ${PROJECT_SOURCE_DIR}/core/ir/${ARCH_NAME}/isl/rvc.txt
          ${PROJECT_SOURCE_DIR}/core/ir/${ARCH_NAME}/isl/svinval.txt
          ${PROJECT_SOURCE_DIR}/core/ir/${ARCH_NAME}/isl/system.txt
          ${PROJECT_SOURCE_DIR}/core/ir/${ARCH_NAME}/isl/zicbom.txt
          ${PROJECT_SOURCE_DIR}/core/ir/${ARCH_NAME}/isl/zicbop.txt
          ${PROJECT_SOURCE_DIR}/core/ir/${ARCH_NAME}/isl/zicboz.txt
          ${PROJECT_SOURCE_DIR}/core/ir/${ARCH_NAME}/isl/zicsr.txt
          ${PROJECT_SOURCE_DIR}/core/ir/${ARCH_NAME}/isl/zifencei.txt
  COMMAND ${PYTHON_EXECUTABLE}
  ARGS ${PROJECT_SOURCE_DIR}/core/ir/${ARCH_NAME}/codec.py
       ${PROJECT_SOURCE_DIR}/core/ir/${ARCH_NAME}/isl
       ${PROJECT_SOURCE_DIR}/core/ir/${ARCH_NAME}
       ${PROJECT_BINARY_DIR}
  VERBATIM
)
