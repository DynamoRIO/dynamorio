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

# Kbuild in this directory lists the module's sources and flags and generates
# the rest of its inputs. It expects a tree containing common/, core/, and the
# generated headers. Kbuild writes its outputs next to the sources, so build in
# a tree in the build directory that mirrors common/ and core/ with per-file
# symlinks.
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
file(CREATE_LINK "${CMAKE_CURRENT_SOURCE_DIR}/Kbuild" "${CMAKE_CURRENT_BINARY_DIR}/Kbuild"
  SYMBOLIC)

set(dynamorio_module_ko "${CMAKE_CURRENT_BINARY_DIR}/${dynamorio_module_name}.ko")

# Always invoke kbuild and let it decide what needs rebuilding.
add_custom_target(dynamorio_module ALL
  COMMAND ${MAKE_PROGRAM} -C ${KERNEL_DIR} M=${CMAKE_CURRENT_BINARY_DIR} modules
  # Ensure generated inputs are ready before kbuild runs.
  DEPENDS
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
