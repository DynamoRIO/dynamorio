# **********************************************************
# Copyright (c) 2026 Google, Inc.    All rights reserved.
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

# Builds the DynamoRIO Linux kernel module via the kernel's own Kbuild
# system. Kernel modules can't be compiled with CMake's normal compiler
# invocation, so this shells out to `make` against KERNEL_DIR and
# MAKE_PROGRAM, both validated and cached by the parent CMakeLists.txt.

set(dynamorio_module_name dynamorio_module)

# Single source of truth for module sources. Add new files here only.
# This list drives both CMake's own rebuild-staleness tracking (DEPENDS
# below) and the generated Kbuild Makefile's object list, so the two can't
# drift out of sync as the module grows.
#
# Note: No source file may be named "${dynamorio_module_name}.c" because
# Kbuild composite targets cannot share a name with a component object file.
set(dynamorio_module_sources
  dynamorio_module_main.c
  kernel_interface.c
  os.c

  # core/
  ../buildmark.c
  ../config.c
  ../dispatch.c
  ../dynamo.c
  ../emit.c
  ../fcache.c
  ../fragment.c
  ../hashtable.c
  ../heap.c
  ../io.c
  ../link.c
  ../loader_shared.c
  ../module_list.c
  ../moduledb.c
  ../monitor.c
  ../nudge.c
  ../options.c
  ../perscache.c
  ../stats.c
  ../string.c
  ../synch.c
  ../translate.c
  ../utils.c
  ../vmareas.c

  # core/ir/
  ../ir/decode_shared.c
  ../ir/disassemble_shared.c
  ../ir/encode_shared.c
  ../ir/instr_shared.c
  ../ir/instrlist.c
  ../ir/ir_utils_shared.c
  ../ir/opnd_shared.c

  # core/ir/x86/
  ../ir/x86/decode.c
  ../ir/x86/decode_fast.c
  ../ir/x86/decode_table.c
  ../ir/x86/disassemble.c
  ../ir/x86/encode.c
  ../ir/x86/instr.c
  ../ir/x86/ir_utils.c
  ../ir/x86/opnd.c

  # core/arch/
  ../arch/arch.c
  ../arch/clean_call_opt_shared.c
  ../arch/emit_utils_shared.c
  ../arch/interp.c
  ../arch/mangle_shared.c
  ../arch/proc_shared.c

  # core/arch/x86/
  ../arch/x86/clean_call_opt.c
  ../arch/x86/emit_utils.c
  ../arch/x86/mangle.c
  ../arch/x86/proc.c

  # core/lib/
  ../lib/instrument.c
  ../lib/module_api.c
)

set(dynamorio_module_objs "")
foreach (src ${dynamorio_module_sources})
  get_filename_component(abs_src "${CMAKE_CURRENT_SOURCE_DIR}/${src}" ABSOLUTE)
  file(RELATIVE_PATH rel_src "${PROJECT_SOURCE_DIR}" "${abs_src}")
  set(mirrored_src "${CMAKE_CURRENT_BINARY_DIR}/${rel_src}")
  get_filename_component(mirrored_dir "${mirrored_src}" DIRECTORY)
  file(MAKE_DIRECTORY "${mirrored_dir}")
  file(CREATE_LINK "${abs_src}" "${mirrored_src}" SYMBOLIC)
  string(REGEX REPLACE "\\.c$" ".o" rel_obj "${rel_src}")
  list(APPEND dynamorio_module_objs "${rel_obj}")
endforeach ()

get_filename_component(core_dir "${CMAKE_CURRENT_SOURCE_DIR}/.." ABSOLUTE)
set(dynamorio_module_include_dirs
  "${CMAKE_BINARY_DIR}"
  "${PROJECT_SOURCE_DIR}/common"
  "${core_dir}"
  "${core_dir}/include"
  "${core_dir}/kernel_linux"
  "${core_dir}/arch"
  "${core_dir}/arch/x86"
  "${core_dir}/drlibc"
  "${core_dir}/ir"
  "${core_dir}/ir/x86"
  "${core_dir}/lib"
  "${core_dir}/unix"
)

set(dynamorio_module_defines
  LINUX_KERNEL
  DYNAMORIO_INTERNAL
  DR_DO_NOT_DEFINE_MAX_MIN
)

set(dynamorio_module_common_flags "")
foreach (dir ${dynamorio_module_include_dirs})
  list(APPEND dynamorio_module_common_flags "-I${dir}")
endforeach ()
foreach (define ${dynamorio_module_defines})
  list(APPEND dynamorio_module_common_flags "-D${define}")
endforeach ()

set(dynamorio_module_cpp_flags ${dynamorio_module_common_flags} -DCPP2ASM)

set(x86_asm_stem "core/arch/x86/x86")
set(x86_asm_src "${PROJECT_SOURCE_DIR}/${x86_asm_stem}.asm")
set(x86_gen_s "${CMAKE_CURRENT_BINARY_DIR}/${x86_asm_stem}.S")

set(x86_asm_deps
  "${x86_asm_src}"
  "${PROJECT_SOURCE_DIR}/make/CMake_asm.cmake"
  "${PROJECT_SOURCE_DIR}/core/arch/asm_defines.asm"
  "${PROJECT_SOURCE_DIR}/core/arch/x86/x86_asm_defines.asm"
  "${PROJECT_SOURCE_DIR}/core/unix/os_asm_defines.asm"
  "${PROJECT_BINARY_DIR}/configure.h"
)

# Preprocess x86.asm into x86.S.
# Note: This replicates the preprocessing logic from cpp2asm_support.cmake because
# cpp2asm_support.cmake cannot be used directly with kbuild.
add_custom_command(
  OUTPUT "${x86_gen_s}"
  DEPENDS ${x86_asm_deps}
  COMMAND ${CMAKE_C_COMPILER} -E -P -x c
    ${dynamorio_module_cpp_flags}
    "${x86_asm_src}"
    -o "${x86_gen_s}"
  COMMAND ${CMAKE_COMMAND}
    -Dfile=${x86_gen_s}
    -P "${PROJECT_SOURCE_DIR}/make/CMake_asm.cmake"
  COMMENT "Preprocessing ${x86_asm_stem}.asm -> .S"
  VERBATIM
)

list(APPEND dynamorio_module_objs "${x86_asm_stem}.o")

string(REPLACE ";" " \\\n  " dynamorio_module_objs "${dynamorio_module_objs}")
set(dynamorio_module_kbuild_objs
  "${dynamorio_module_name}-y := \\\n  ${dynamorio_module_objs}")

set(dynamorio_module_kbuild_ccflags "ccflags-y += \\")
set(dynamorio_module_kbuild_asflags "asflags-y += \\")
foreach (flag ${dynamorio_module_common_flags})
  string(APPEND dynamorio_module_kbuild_ccflags "\n  ${flag} \\")
  string(APPEND dynamorio_module_kbuild_asflags "\n  ${flag} \\")
endforeach ()

string(APPEND dynamorio_module_kbuild_ccflags "\n  -Wframe-larger-than=4096")

string(APPEND dynamorio_module_kbuild_asflags "\n  -Xassembler -mmnemonic=intel \\")
string(APPEND dynamorio_module_kbuild_asflags "\n  -Xassembler -msyntax=intel \\")
string(APPEND dynamorio_module_kbuild_asflags "\n  -Xassembler -mnaked-reg \\")
string(APPEND dynamorio_module_kbuild_asflags "\n  -Xassembler --noexecstack \\")
string(APPEND dynamorio_module_kbuild_asflags "\n  -Xassembler --64")

set(dynamorio_module_ko "${CMAKE_CURRENT_BINARY_DIR}/${dynamorio_module_name}.ko")
set(dynamorio_module_kbuildfile "${CMAKE_CURRENT_BINARY_DIR}/Kbuild")

configure_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/Kbuild.in
  ${dynamorio_module_kbuildfile}
  @ONLY
)

add_custom_command(
  OUTPUT ${dynamorio_module_ko}
  # TODO i#8021: turn off KBUILD_MODPOST_WARN.
  COMMAND ${MAKE_PROGRAM} -C ${KERNEL_DIR} M=${CMAKE_CURRENT_BINARY_DIR}
    KBUILD_MODPOST_WARN=1 modules
  # dynamorio_module needs "event_strings.h" which is generated by generate_syslog.
  DEPENDS
    ${dynamorio_module_sources}
    ${x86_gen_s}
    ${dynamorio_module_kbuildfile}
    generate_syslog
  COMMENT "Building DynamoRIO Linux kernel module (${dynamorio_module_name}.ko) against \
KERNEL_DIR=${KERNEL_DIR}"
  VERBATIM
)

add_custom_target(dynamorio_module ALL DEPENDS ${dynamorio_module_ko})

# Standard `make clean` cannot remove Kbuild bookkeeping files.
# Run `make clean_dynamorio_module` to invoke Kbuild's clean target.
add_custom_target(clean_dynamorio_module
  COMMAND ${MAKE_PROGRAM} -C ${KERNEL_DIR} M=${CMAKE_CURRENT_BINARY_DIR} clean
)
