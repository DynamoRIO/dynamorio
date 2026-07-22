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
set(dynamorio_module_sources dynamorio_module_main.c)

set(dynamorio_module_ko "${CMAKE_CURRENT_SOURCE_DIR}/${dynamorio_module_name}.ko")
set(dynamorio_module_makefile "${CMAKE_CURRENT_SOURCE_DIR}/Makefile")

list(TRANSFORM dynamorio_module_sources REPLACE "\\.c$" ".o"
     OUTPUT_VARIABLE dynamorio_module_objs)
string(REPLACE ";" " " dynamorio_module_objs "${dynamorio_module_objs}")
set(dynamorio_module_kbuild_extra "${dynamorio_module_name}-y := ${dynamorio_module_objs}")

configure_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/Makefile.in
  ${dynamorio_module_makefile}
  @ONLY
)

list(TRANSFORM dynamorio_module_sources PREPEND "${CMAKE_CURRENT_SOURCE_DIR}/")

add_custom_command(
  OUTPUT ${dynamorio_module_ko}
  # TODO i#8021: turn off KBUILD_MODPOST_WARN
  COMMAND ${MAKE_PROGRAM} -C ${KERNEL_DIR} M=${CMAKE_CURRENT_SOURCE_DIR} KBUILD_MODPOST_WARN=1 modules
  DEPENDS ${dynamorio_module_sources} ${dynamorio_module_makefile}
  COMMENT "Building DynamoRIO Linux kernel module (${dynamorio_module_name}.ko) against KERNEL_DIR=${KERNEL_DIR}"
  VERBATIM
)

add_custom_target(dynamorio_module ALL DEPENDS ${dynamorio_module_ko})

# Standard `make clean` cannot remove Kbuild bookkeeping files.
# Run `make clean_dynamorio_module` to invoke Kbuild's clean target.
add_custom_target(clean_dynamorio_module
  COMMAND ${MAKE_PROGRAM} -C ${KERNEL_DIR} M=${CMAKE_CURRENT_SOURCE_DIR} clean
)
