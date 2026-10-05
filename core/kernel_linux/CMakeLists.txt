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

# Must match obj-m in Kbuild.
set(dynamorio_module_name dynamorio_module)

# Single source of truth for module sources. Add new files here only.
# This list drives the generated Kbuild.sources object list.
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
  string(REGEX REPLACE "\\.c$" ".o" rel_obj "${rel_src}")
  list(APPEND dynamorio_module_objs "${rel_obj}")
endforeach ()

# Kbuild writes its outputs next to the sources, so the module is built in a
# tree in the build directory that mirrors common/ and core/ with per-file
# symlinks and has the generated headers at its root. Kbuild only refers to
# paths within this tree, which keeps it relocatable.
file(GLOB_RECURSE dynamorio_module_tree_files
  RELATIVE "${PROJECT_SOURCE_DIR}"
  CONFIGURE_DEPENDS
  "${PROJECT_SOURCE_DIR}/common/*"
  "${PROJECT_SOURCE_DIR}/core/*"
)
foreach (file ${dynamorio_module_tree_files})
  get_filename_component(mirrored_dir "${CMAKE_CURRENT_BINARY_DIR}/${file}" DIRECTORY)
  file(MAKE_DIRECTORY "${mirrored_dir}")
  file(CREATE_LINK "${PROJECT_SOURCE_DIR}/${file}" "${CMAKE_CURRENT_BINARY_DIR}/${file}"
    SYMBOLIC)
endforeach ()
foreach (header configure.h configure_defines.h event_strings.h)
  file(CREATE_LINK "${PROJECT_BINARY_DIR}/${header}"
    "${CMAKE_CURRENT_BINARY_DIR}/${header}" SYMBOLIC)
endforeach ()

# Kbuild generates core/arch/x86/x86.S from x86.asm.
list(APPEND dynamorio_module_objs "core/arch/x86/x86.o")

# Kbuild adds the generated core/kernel_linux/stubs.o, which is derived from
# these objects.
string(REPLACE ";" " \\\n  " dynamorio_module_objs "${dynamorio_module_objs}")
# Only the object list is generated; any build system can supply this input to Kbuild.
# file(GENERATE) preserves the timestamp when the list is unchanged.
set(dynamorio_module_kbuild_sources "${CMAKE_CURRENT_BINARY_DIR}/Kbuild.sources")
file(GENERATE OUTPUT "${dynamorio_module_kbuild_sources}" CONTENT
  "# Generated by CMake from dynamorio_module_sources.\ndr_objs := \\\n  ${dynamorio_module_objs}\n")

file(CREATE_LINK "${CMAKE_CURRENT_SOURCE_DIR}/Kbuild" "${CMAKE_CURRENT_BINARY_DIR}/Kbuild"
  SYMBOLIC)

set(dynamorio_module_ko "${CMAKE_CURRENT_BINARY_DIR}/${dynamorio_module_name}.ko")

# Always invoke kbuild and let it decide what needs rebuilding.
add_custom_target(dynamorio_module ALL
  COMMAND ${MAKE_PROGRAM} -C ${KERNEL_DIR} M=${CMAKE_CURRENT_BINARY_DIR} modules
  # Ensure generated inputs are ready before kbuild runs.
  DEPENDS
    ${dynamorio_module_kbuild_sources}
    generate_syslog # Generates event_strings.h.
  BYPRODUCTS ${dynamorio_module_ko}
  COMMENT "Building DynamoRIO Linux kernel module (${dynamorio_module_name}.ko) against \
KERNEL_DIR=${KERNEL_DIR}"
  VERBATIM
)

# Standard `make clean` cannot remove Kbuild bookkeeping files.
# Run `make clean_dynamorio_module` to invoke Kbuild's clean target.
add_custom_target(clean_dynamorio_module
  COMMAND ${MAKE_PROGRAM} -C ${KERNEL_DIR} M=${CMAKE_CURRENT_BINARY_DIR} clean
)
