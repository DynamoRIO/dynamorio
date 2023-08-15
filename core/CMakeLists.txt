# **********************************************************
# Copyright (c) 2010-2021 Google, Inc.    All rights reserved.
# Copyright (c) 2009-2010 VMware, Inc.    All rights reserved.
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

include(../make/policies.cmake NO_POLICY_SCOPE)

###########################################################################
# Sources and targets

# Used to share exported headers with clients.
add_definitions(-DDYNAMORIO_INTERNAL)

# we have no exe's here, and we want our dll's in the lib dir
# (could use MODULE instead of SHARED if it would let us link)
set(EXECUTABLE_OUTPUT_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}")
set_per_config_ouput_to_match_single_config()

# These custom commands are per-subdir so can't just place at top level
include(../make/CMake_events.cmake)

# i#801: For generated headers, we need to use add_dependencies to ensure that
# they are generated before compiling the other sources.  Listing the header as
# a source in the target isn't enough because it's really the source files
# themselves that need the header.  We can't put this in CMake_events.cmake b/c
# we end up w/ duplicate targets then.  Note that libutil/ uses these same
# targets but core is always enabled.
add_custom_target(generate_syslog DEPENDS "${SYSLOG_SRCS}")
if (WIN32)
  add_custom_target(generate_events DEPENDS "${EVENTS_SRCS}")
endif (WIN32)
function (add_gen_events_deps target)
  add_dependencies(${target} generate_syslog)
  if (WIN32)
    add_dependencies(${target} generate_events)
  endif ()
endfunction (add_gen_events_deps)

if (AARCH64)
  # Required for opcode_api.h and codec headers, which are auto-generated on AArch64.
  include(../make/CMake_aarch64_gen_codec.cmake)
  add_custom_target(gen_aarch64_codec DEPENDS "${AARCH64_CODEC_GEN_SRCS}")
  include_directories(BEFORE ${PROJECT_BINARY_DIR})
  # Export the generated opcode header.  Use a custom target and command to avoid
  # repeated re-copying (a POST_BUILD with copy_if_different doesn't seem to work how
  # it should: it still runs the command every time, though it doesn't copy anything).
  set(aarch64_exported_opcodes ${BUILD_INCLUDE}/dr_ir_opcodes_aarch64.h)
  add_custom_target(gen_aarch64_opcodes DEPENDS ${aarch64_exported_opcodes})
  add_custom_command(OUTPUT ${aarch64_exported_opcodes}
    DEPENDS ${PROJECT_BINARY_DIR}/opcode_api.h
    COMMAND ${CMAKE_COMMAND}
    ARGS -E copy ${PROJECT_BINARY_DIR}/opcode_api.h
    ${aarch64_exported_opcodes} VERBATIM)
endif ()

if (RISCV64)
  # Required for opcode_api.h and codec headers, which are auto-generated on RISC-V.
  include(../make/CMake_riscv64_gen_codec.cmake)
  add_custom_target(gen_riscv64_codec DEPENDS "${RISCV64_CODEC_GEN_SRCS}")
  include_directories(BEFORE ${PROJECT_BINARY_DIR})
  # Export the generated headers.  Use a custom target and command to avoid
  # repeated re-copying (a POST_BUILD with copy_if_different doesn't seem to work how
  # it should: it still runs the command every time, though it doesn't copy anything).
  set(riscv64_exported_opcodes ${BUILD_INCLUDE}/dr_ir_opcodes_riscv64.h)
  set(riscv64_exported_instr ${BUILD_INCLUDE}/dr_ir_macros_riscv64.h)
  add_custom_target(gen_riscv64_headers DEPENDS
                                          ${riscv64_exported_opcodes}
                                          ${riscv64_exported_instr})
  add_custom_command(OUTPUT ${riscv64_exported_opcodes}
    DEPENDS ${PROJECT_BINARY_DIR}/opcode_api.h
    COMMAND ${CMAKE_COMMAND}
    ARGS -E copy ${PROJECT_BINARY_DIR}/opcode_api.h
    ${riscv64_exported_opcodes} VERBATIM)
  add_custom_command(OUTPUT ${riscv64_exported_instr}
    DEPENDS ${PROJECT_BINARY_DIR}/instr_create_api.h
    COMMAND ${CMAKE_COMMAND}
    ARGS -E copy ${PROJECT_BINARY_DIR}/instr_create_api.h
    ${riscv64_exported_instr} VERBATIM)
endif ()

set(asm_deps
  "${PROJECT_SOURCE_DIR}/core/arch/asm_defines.asm"
  "${PROJECT_BINARY_DIR}/configure.h")
add_asm_target(arch/${DR_HOST_ARCH_NAME}/${DR_HOST_ARCH_NAME}.asm
  arch_core_asm_src arch_core_asm_tgt "_core" "" "${asm_deps}")
if (NOT "${DR_HOST_ARCH_NAME}" STREQUAL "${DR_HOST_ARCH_NAME_SHARED}")
  add_asm_target(arch/${DR_HOST_ARCH_NAME_SHARED}/${DR_HOST_ARCH_NAME_SHARED}.asm
    archshared_core_asm_src archshared_core_asm_tgt
    "_core" "" "${asm_deps}")
endif ()
add_asm_target(arch/pre_inject_asm.asm preinject_asm_src preinject_asm_tgt ""
  "-DNOT_DYNAMORIO_CORE_PROPER" "${asm_deps}")

if (UNIX)
  # i#3315: We want our own memcpy and memset for the shared-lib DR core and
  # for drinjectlib + drfrontendlib to avoid glibc-versioned symbols in
  # our auxiliary tools (i#1504), but we do *not* want our own memcpy and
  # memset for static-lib DR core.  Thus we separate them out.  The i#1504
  # glibc versioning is only an issue on x86.
  add_asm_target(arch/${DR_HOST_ARCH_NAME}/memfuncs.asm memfuncs_asm_src memfuncs_asm_tgt
    "_memfuncs" "" "${asm_deps}")
  add_library(drmemfuncs STATIC ${memfuncs_asm_src} lib/memmove.c)
  add_gen_events_deps(drmemfuncs)
  if (APP_EXPORTS)
    add_dependencies(drmemfuncs api_headers)
  endif ()
endif ()

# i#1409: to share core libc-ish code with non-core, we use the "drlibc" library.
add_asm_target(drlibc/drlibc_xarch.asm drlibc_xarch_asm_src drlibc_xarch_asm_tgt
  "_core" "" "${asm_deps}")
add_asm_target(drlibc/drlibc_${DR_HOST_ARCH_NAME}.asm drlibc_arch_asm_src
  drlibc_arch_asm_tgt
  "" "" "${asm_deps}")
set(DRLIBC_SRCS
  ${drlibc_xarch_asm_src}
  ${drlibc_arch_asm_src}
  drlibc/drlibc.c
  drlibc/drlibc_notdr_dcxt.c
  drlibc/drlibc_notdr_error.c
  drlibc/drlibc_notdr_ignoreassert.c
  drlibc/drlibc_notdr_logfile.c
  drlibc/drlibc_notdr_printlog.c
  drlibc/drlibc_notdr_report.c
  drlibc/drlibc_notdr_saferead.c
  drlibc/drlibc_notdr_stats.c)
if (UNIX)
  set(DRLIBC_SRCS ${DRLIBC_SRCS} drlibc/drlibc_unix.c)
  if (APPLE)
    set(DRLIBC_SRCS ${DRLIBC_SRCS} drlibc/drlibc_module_macho.c)
  else ()
    set(DRLIBC_SRCS ${DRLIBC_SRCS} drlibc/drlibc_module_elf.c)
  endif ()
endif ()
add_library(drlibc STATIC ${DRLIBC_SRCS})
# CMake on Windows fails to link unless we tell it to use the C linker:
set_target_properties(drlibc PROPERTIES LINKER_LANGUAGE C)
if (UNIX)
  append_property_string(TARGET drlibc COMPILE_FLAGS "-fPIC")
endif ()
add_gen_events_deps(drlibc)
if (AARCH64)
  add_dependencies(drlibc gen_aarch64_codec)
elseif (RISCV64)
  add_dependencies(drlibc gen_riscv64_codec)
endif (AARCH64)
if (WIN32 AND "${CMAKE_GENERATOR}" MATCHES "Visual Studio")
  # for parallel build correctness we need a target dependence
  add_dependencies(drlibc ${drlibc_xarch_asm_tgt} ${drlibc_arch_asm_tgt})
endif ()

set(CORE_SRCS
  options.c
  dynamo.c
  fragment.c
  fcache.c
  link.c
  dispatch.c
  emit.c
  utils.c
  config.c
  stats.c
  heap.c
  monitor.c
  perfctr.c
  vmareas.c
  rct.c
  hotpatch.c
  hashtable.c
  module_list.c
  moduledb.c
  perscache.c
  nudge.c
  synch.c
  buildmark.c
  loader_shared.c
  io.c
  native_exec.c
  lib/instrument.c
  lib/module_api.c
  translate.c
  annotations.c
  jit_opt.c
  )

if (UNIX)
  # i#46: Private string routines for libc independence.  On Windows our
  # bootstrapping loader finds these imports in ntdll.  If that becomes
  # problematic, we can find a way to compile this on Windows.
  set(CORE_SRCS ${CORE_SRCS} string.c)
endif (UNIX)

if (ARM)
  # i#1566: there is no div instruction on most ARM arches including ARMv7a.
  # We use software divide emulation from libgcc, which we can link here
  # and remain BSD due to the GPL GCC Runtime Library Exception.
  #
  # The libgcc assembly file seems to need to be compiled separately for
  # each target routine set, as we get duplicate labels in the shared
  # macros otherwise.
  # XXX: change the labels to be "local" labels (like "1:") and add 'b' and 'f'
  # hints on the branches?  Or, better, figure out how glibc compiles the file.
  macro (add_libgcc_routines name)
    set(TODEFINE "L_${name}")
    set(genfile ${CMAKE_CURRENT_BINARY_DIR}/libgcc_${name}.S)
    configure_file(../third_party/libgcc/arm/lib1funcs.S ${genfile} @ONLY)
    set(CORE_SRCS ${CORE_SRCS} ${genfile})
  endmacro ()
  add_libgcc_routines(udivsi3)
  add_libgcc_routines(umodsi3)
  add_libgcc_routines(divsi3)
  add_libgcc_routines(modsi3)
  include_directories(../third_party/libgcc/arm)
endif (ARM)

if (UNIX)
  # These are needed for ARM per i#1566 above, but are also needed in some cases
  # for x86, such as 32-bit manipulation of 64-bit integers under clang where
  # __moddi3 is needed.
  if (ARM OR (X86 AND NOT X64))
    # AArch64 or x86_64 don't not use this functionality
    set(CORE_SRCS ${CORE_SRCS} ../third_party/libgcc/udivmoddi4.c)
  endif()
endif ()

if (WIN32 AND NOT X64)
  # PR 219380: to avoid __ftol2_sse from libc
  # FIXME: There is no supported way to suppress a "command line" warning
  # used to do:
  #   $(BUILD)/io.obj: FILTER_CC = 2>&1 | $(GREP) -v "D9035 : option 'QIfist'";
  # we just live with it for now:
  #   [ 20%] Building C object core/CMakeFiles/dynamorio.dir/io.c.obj
  #   cl : Command line warning D9035 : option 'QIfist' has been deprecated and will be removed in a future release
  # Note that /QIfist causes casts to round instead of truncating (i#763)
  # which we work around in our code.
  set_source_files_properties(io.c COMPILE_FLAGS "/QIfist")
  set_source_files_properties(utils.c COMPILE_FLAGS "/QIfist")
endif (WIN32 AND NOT X64)

# XXX: Originally we used _shared (inject_shared, etc.) to mean
# "shared between multiple libraries", while we've also started using
# _shared on opnd_shared.c, etc. to mean 'shared between architectures":
# perhaps we should separate the naming schemes.  "opnd_xarch.c" and
# "inject_xlib.c"?
set(DECODER_SRCS
  ir/opnd_shared.c
  ir/${ARCH_NAME_SHARED}/opnd.c
  ir/instr_shared.c
  ir/${ARCH_NAME}/instr.c
  ir/instrlist.c
  ir/decode_shared.c
  ir/${ARCH_NAME}/decode.c
  ir/encode_shared.c
  ir/${ARCH_NAME}/encode.c
  ir/disassemble_shared.c
  ir/${ARCH_NAME}/disassemble.c
  ir/ir_utils_shared.c
  ir/${ARCH_NAME_SHARED}/ir_utils.c
  )
if (X86)
  set(DECODER_SRCS ${DECODER_SRCS}
    ir/${ARCH_NAME}/decode_table.c
    ir/${ARCH_NAME}/decode_fast.c)
elseif (ARM)
  set(DECODER_SRCS ${DECODER_SRCS}
    ir/${ARCH_NAME}/table_a32_pred.c
    ir/${ARCH_NAME}/table_a32_unpred.c
    ir/${ARCH_NAME}/table_encode.c
    ir/${ARCH_NAME}/table_t32_base.c
    ir/${ARCH_NAME}/table_t32_coproc.c
    ir/${ARCH_NAME}/table_t32_16.c
    ir/${ARCH_NAME}/table_t32_16_it.c)
elseif (AARCH64)
  set(DECODER_SRCS ${DECODER_SRCS}
    ir/${ARCH_NAME}/codec.c
    ir/${ARCH_NAME}/build_ldstex.c)
elseif (RISCV64)
  set(DECODER_SRCS ${DECODER_SRCS}
    ir/${ARCH_NAME}/codec.c)
endif ()

set(ARCH_SRCS
  arch/arch.c
  arch/emit_utils_shared.c
  arch/${ARCH_NAME}/emit_utils.c
  arch/${ARCH_NAME_SHARED}/emit_utils.c
  # TODO i#1684: Link with drdecode rather than compiling all the same files.
  ${DECODER_SRCS}
  arch/interp.c
  arch/proc_shared.c
  arch/${ARCH_NAME}/proc.c
  arch/mangle_shared.c
  arch/${ARCH_NAME_SHARED}/mangle.c
  arch/clean_call_opt_shared.c
  arch/${ARCH_NAME}/clean_call_opt.c
  arch/x86_code.c
  ${arch_core_asm_src}
  ${archshared_core_asm_src}
  arch/${ARCH_NAME}/optimize.c
  arch/sideline.c
  arch/retcheck.c
  )

if (X86 AND X64)
  set(ARCH_SRCS ${ARCH_SRCS} arch/${ARCH_NAME}/x86_to_x64.c)
endif (X86 AND X64)

if (WIN32)
  # i#894: Win8 WDK ntdll.lib does not list Ki routines so we make our own .lib.
  # Because the Ki are stdcall we can't just use a .def file: we need
  # an .obj file built from stubs w/ the same signatures (in addition to
  # listing those stub routine (un-mangled) names in our .def file).
  # i#938: We expand our .lib to include everything so we don't need DDK/WDK.
  # XXX: we should make sure we don't rely on ntdll routines not in older systems!
  if (X64)
    set(ntimp_def "${PROJECT_SOURCE_DIR}/core/win32/ntdll_imports_x64.def")
  else (X64)
    set(ntimp_def "${PROJECT_SOURCE_DIR}/core/win32/ntdll_imports.def")
  endif (X64)
  # i#1588: we used to have a custom command to build this imports lib (and a
  # custom target for VS parallel builds) but we need this exported for our other
  # exported libs and we can't export custom targets, so we now use a first-class
  # lib target.  CMake supports a .def file as a source and runs lib.exe for us.
  # Although we no longer use the WDK/DDK ntdll.lib (i#938), we still use a
  # separate name "ntdll_imports" to avoid conflicts in containing projects.
  add_library(ntdll_imports SHARED win32/ntdll_imports.c ${ntimp_def})
  # i#1137: ignore 'specified multiple times' linker warnings
  append_property_string(TARGET ntdll_imports LINK_FLAGS "/ignore:4197")
  DynamoRIO_get_full_path(ntimp_flags ntdll_imports "${location_suffix}")
  string(REPLACE ".dll" ".lib" ntimp_flags ${ntimp_flags})
  DR_export_target(ntdll_imports)
  # We need separate 32-bit and 64-bit versions so we put into lib dir
  install_exported_target(ntdll_imports ${INSTALL_LIB_BASE})
endif (WIN32)

_DR_get_static_libc_list(static_libc_list)
if (WIN32)
  if (DEBUG)
    set(WIN32_C_LIB libcmtd)
  else (DEBUG)
    set(WIN32_C_LIB libcmt)
  endif (DEBUG)
  set(NOLIBC_DLL_ENTRY /entry:DllMain)
endif ()

if (UNIX)
  set(OSNAME unix)
  set(OS_SRCS
    unix/os.c
    unix/signal.c
    unix/module.c
    unix/pcprofile.c
    unix/stackdump.c
    unix/diagnost.c
    unix/loader.c
    )
  if (LINUX)
    if (ANDROID)
      set(OS_SRCS ${OS_SRCS} unix/loader_android.c)
    else ()
      set(OS_SRCS ${OS_SRCS} unix/loader_linux.c)
    endif ()
    set(OS_SRCS ${OS_SRCS} unix/memquery_linux.c)
    set(OS_SRCS ${OS_SRCS} unix/memquery.c)
    set(OS_SRCS ${OS_SRCS} unix/memcache.c)
    set(OS_SRCS ${OS_SRCS} unix/module_elf.c)
    set(OS_SRCS ${OS_SRCS} unix/ksynch_linux.c)
    if (ARM OR AARCH64 OR RISCV64)
      set(OS_SRCS ${OS_SRCS} unix/tls_linux_risc.c)
    else ()
      set(OS_SRCS ${OS_SRCS} unix/tls_linux_${ARCH_NAME}.c)
    endif ()
    set(OS_SRCS ${OS_SRCS} unix/signal_linux.c)
    set(OS_SRCS ${OS_SRCS} unix/signal_linux_${ARCH_NAME}.c)
    set(OS_SRCS ${OS_SRCS} unix/native_elf.c)
    # XXX i#1286: should be split into nudge_linux.c and nudge_macos.c
    set(OS_SRCS ${OS_SRCS} unix/nudgesig.c)
    set(OS_SRCS ${OS_SRCS} unix/rseq_linux.c)
  elseif (APPLE)
    set(OS_SRCS ${OS_SRCS} unix/loader_macos.c)
    set(OS_SRCS ${OS_SRCS} unix/memquery_macos.c)
    set(OS_SRCS ${OS_SRCS} unix/memquery.c)
    set(OS_SRCS ${OS_SRCS} unix/module_macho.c)
    set(OS_SRCS ${OS_SRCS} unix/ksynch_macos.c)
    set(OS_SRCS ${OS_SRCS} unix/tls_macos.c)
    set(OS_SRCS ${OS_SRCS} unix/signal_macos.c)
    set(OS_SRCS ${OS_SRCS} unix/native_macho.c)
    # XXX i#1286: implement nudge_macos.c
  elseif (VMKERNEL)
    set(VMKUW_DIR ${PROJECT_SOURCE_DIR}/../internal/core/linux)
    include_directories(${VMKUW_DIR})
    set(OS_SRCS ${OS_SRCS} ${VMKUW_DIR}/vmkuw.c)
    set(OS_SRCS ${OS_SRCS} unix/memquery_emulate.c)
    set(OS_SRCS ${OS_SRCS} unix/memcache.c)
    set(OS_SRCS ${OS_SRCS} unix/module_elf.c)
    set(OS_SRCS ${OS_SRCS} unix/ksynch_linux.c)
    set(OS_SRCS ${OS_SRCS} unix/signal_linux.c)
  endif ()
  set(PRELOAD_NAME drpreload)
  set(PRELOAD_SRCS
    unix/preload.c
    config.c
    # i#1334: private string routines for libc independence
    string.c
    io.c
    )
  set(INJECTOR_SRCS
    unix/injector.c
    config.c    # to read config
    string.c
    io.c
    )

else (UNIX)
  set(OSNAME win32)
  set(OS_SRCS
    win32/eventlog.c
    win32/os.c
    win32/syscall.c
    win32/callback.c
    win32/drmarker.c
    win32/ntdll_shared.c
    win32/ntdll.c
    win32/inject.c
    win32/inject_shared.c
    win32/module.c
    win32/module_shared.c
    win32/native_pe.c
    win32/diagnost.c
    win32/aslr.c
    win32/loader.c
    win32/drwinapi/drwinapi.c
    win32/drwinapi/ntdll_redir.c
    win32/drwinapi/kernel32_redir.c
    win32/drwinapi/kernel32_proc.c
    win32/drwinapi/kernel32_lib.c
    win32/drwinapi/kernel32_mem.c
    win32/drwinapi/kernel32_file.c
    win32/drwinapi/kernel32_sync.c
    win32/drwinapi/kernel32_misc.c
    win32/drwinapi/rpcrt4_redir.c
    win32/drwinapi/advapi32_redir.c
    win32/resources.rc
   )
  set(PRELOAD_SRCS
    win32/pre_inject.c
    win32/ntdll_shared.c
    win32/ntdll.c
    win32/inject_shared.c
    win32/drmarker.c
    ${preinject_asm_src}
    win32/module_shared.c
    win32/resources.rc
    config.c
    win32/os.c
    io.c
    )
  set(PRELOAD_NAME drpreinject)
  set(INJECTOR_SRCS
    win32/injector.c
    win32/inject.c
    win32/inject_shared.c
    win32/module_shared.c
    win32/ntdll_shared.c
    win32/ntdll.c
    win32/resources.rc
    config.c
    win32/os.c
    io.c
    )

  add_library(drearlyhelp1 SHARED
    win32/early_inject_helper1.c
    win32/resources.rc
    )
  set_target_properties(drearlyhelp1 PROPERTIES
    COMPILE_DEFINITIONS "RC_IS_EARLY1")
  # base = (release base - 64k)
  set_target_properties(drearlyhelp1 PROPERTIES
    # not bothering with map files
    LINK_FLAGS "${LD_FLAGS} /dll /NODEFAULTLIB /base:0x70FF0000 ${NOLIBC_DLL_ENTRY}")
  target_link_libraries(drearlyhelp1 drearlyhelp2)

  add_library(drearlyhelp2 SHARED
    win32/early_inject_helper2.c
    win32/resources.rc
    )
  set_target_properties(drearlyhelp2 PROPERTIES
    COMPILE_DEFINITIONS "RC_IS_EARLY2")
  # base = (helper1 base - 64k)
  set_target_properties(drearlyhelp2 PROPERTIES
    # not bothering with map files
    LINK_FLAGS "${LD_FLAGS} /dll /NODEFAULTLIB /base:0x71110000 ${NOLIBC_DLL_ENTRY}")
endif (UNIX)

###########################################################################
# DynamoRIO core library shared configuration

function (configure_core_lib target)
  set_target_properties(${target} PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY${location_suffix} "${DR_LIBRARY_OUTPUT_DIRECTORY}"
    RUNTIME_OUTPUT_DIRECTORY${location_suffix} "${DR_LIBRARY_OUTPUT_DIRECTORY}"
    ARCHIVE_OUTPUT_DIRECTORY${location_suffix} "${DR_LIBRARY_OUTPUT_DIRECTORY}")
  add_gen_events_deps(${target})
  if (AARCH64)
    add_dependencies(${target} gen_aarch64_codec)
  elseif (RISCV64)
    add_dependencies(${target} gen_riscv64_codec)
  endif()
  if (WIN32)
    # Since we're forced to use link-line flags instead of target_link_libraries
    # for dynamorio, we have to add a real target dependence here for correct
    # parallel builds (i#1616: ninja also needs explicit dependency on ntdll_imports).
    add_dependencies(${target} ntdll_imports)
  endif ()
  if ("${CMAKE_GENERATOR}" MATCHES "Visual Studio")
    # For VS generators, we also need one for our asm custom commands.
    add_dependencies(${target} ${arch_core_asm_tgt} ${archshared_core_asm_tgt})
  endif ()

  target_link_libraries(${target} drlibc)
  copy_target_to_device(${target} "${location_suffix}")

  # rather than a separate BUILD_INCLUDES option we always build
  # include headers if building core (or if building docs or samples as
  # they have real dependencies)
  if (APP_EXPORTS)
    add_dependencies(${target} api_headers)
  endif ()

  if (UNIX)
    set(dynamorio_link_flags "")

    # Do non-lazy runtime binding
    if (APPLE)
      set(dynamorio_link_flags "${dynamorio_link_flags} -Xlinker -bind_at_load")
    else (APPLE)
      set(dynamorio_link_flags "${dynamorio_link_flags} -Xlinker -z -Xlinker now")
    endif (APPLE)

    if (APPLE AND NOT X64)
      # XXX i#1322: get the asm code PIC.  Until then we have to suppress the
      # error from ld about text relocs.
      set(dynamorio_link_flags "-read_only_relocs suppress")
    endif ()

    # Bind global symbols internally to avoid interposing
    if (APPLE)
      # Looks like -interposable is off by default
    else (APPLE)
      set(dynamorio_link_flags "${dynamorio_link_flags} -Xlinker -Bsymbolic")
    endif (APPLE)

    set(dynamorio_link_flags "${dynamorio_link_flags} -nostdlib")

    if (APPLE)
      # Needs explicit -init (regardless of name: _init, init, etc.)
      set(dynamorio_link_flags "${dynamorio_link_flags} -init __init")
    endif (APPLE)

    set_target_properties(${target} PROPERTIES
      LINK_FLAGS "${dynamorio_link_flags}")

    # XXX: FRAGMENT_SIZES_STUDY needs libm for sqrt but it's not supported by default

    string(REGEX REPLACE
      "^([0-9]+\\.[0-9]+).*" "\\1" VERSION_MAJOR_MINOR "${VERSION_NUMBER}")
    # We only set the version/soversion on Windows to avoid many negatives
    # (i#1374, i#2127, Android "adb push" not supporting symlinks, etc.) and
    # very few positives on UNIX platforms.  Our loader does its own version
    # checks for client compatibility.
    if (WINDOWS)
      set_target_properties(${target} PROPERTIES
        VERSION "${VERSION_MAJOR_MINOR}" SOVERSION "${VERSION_MAJOR_MINOR}")
    endif ()

  else (UNIX)
    set_target_properties(${target} PROPERTIES
      # Set define parameters for resources.rc.
      # Since it's hard to pass spaces in defines (I can get a " through using
      # \\\\\\\" but no spaces) we put strings in resources.rc and select
      # using RC_ defines.
      COMPILE_DEFINITIONS "RC_IS_CORE;INCLUDE_EVENTS"
      # i#921: we do not want a __chkstk function comparing our stack to
      # TEB.StackLimit, esp on win8 where the stack is high, so we disable
      # compiler-inserted stack probes globally in DR itself.
      COMPILE_FLAGS "/Gs65536")

    if ("${CMAKE_GENERATOR}" MATCHES "Ninja" AND
        "${CMAKE_VERSION}" VERSION_LESS "2.8.9.20120822")
      # cmake bug http://www.cmake.org/Bug/view.php?id=13486 causes rc.exe to
      # fail b/c it's passed COMPILE_FLAGS
      message(FATAL_ERROR "cmake at least version 2.8.9.20120822 is required for Ninja")
    endif ()
  endif (UNIX)

endfunction (configure_core_lib)

###########################################################################
# DynamoRIO shared core library

add_library(dynamorio SHARED
  ${CORE_SRCS} ${ARCH_SRCS} ${OS_SRCS}
  # Adding here for dependence.
  ${PROJECT_SOURCE_DIR}/make/ldscript.cmake
  )

configure_core_lib(dynamorio)
if (UNIX)
  # We need our own memcpy + memset for isolation.
  # They're separated out for sharing for i#1504.
  target_link_libraries(dynamorio drmemfuncs)
endif ()

if (UNIX)
  # i#47: set the ELF header entry point to _start for early injection.
  append_property_string(TARGET dynamorio LINK_FLAGS "-Wl,${ld_entry_flag},_start")

  if (NOT HAVE_FVISIBILITY)
    # We used to have the genapi.pl script build an linker script
    # export list for us but we no longer support that.
    message(FATAL_ERROR "-fvisibility is required to build")
  endif (NOT HAVE_FVISIBILITY)

  # This appends to LINK_FLAGS, so do it after we set them above.
  set_preferred_base_start_and_end(dynamorio ${preferred_base} ON)

  if (APPLE) # MacOS has no private loader yet.
    target_link_libraries(dynamorio dl)
  endif ()

  if (LINUX) # No checks for MacOS
    # Note that we can't locate readelf inside CMake_readelf.cmake as that
    # would disallow pointing externally at a readelf location, so we make it
    # a proper cache variable.
    # Note that nothing depends on CMake_readelf.cmake: that's ok, we'll check
    # in the next clean build if we modify the script.
    # readelf is a binutils app just like ld
    get_filename_component(binutils_path ${CMAKE_LINKER} PATH)
    if (APPLE)
      set(readelf_name "greadelf")
    else (APPLE)
      set(readelf_name "readelf")
    endif (APPLE)
    find_program(READELF_EXECUTABLE ${readelf_name} HINTS "${binutils_path}"
      DOC "path to readelf")
    if (NOT READELF_EXECUTABLE)
      message("${readelf_name} not found: not checking SElinux or execstack")
    else ()
      set(locvar_name dynamorioloc)
      file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/${locvar_name}.cmake" CONTENT
        "set(${locvar_name} \"$<TARGET_FILE:dynamorio>\")\n")
      set(check_deps OFF)
      if (NOT APPLE)
        # i#46, i#1541, i#1459: we have zero dependencies now by default, except
        # for MacOS where privlib is NYI (i#1285) and static lib.
        set(check_deps ON)
      endif ()
      # i#2632: 32-bit recent clang release builds are inserting text relocs.
      # XXX: We don't support clang for official package builds until this is fixed.
      if (X64 OR DEBUG OR NOT CLANG)
        set(check_textrel ON)
      else ()
        set(check_textrel OFF)
      endif ()
      add_custom_command(TARGET dynamorio
        POST_BUILD
        COMMAND ${CMAKE_COMMAND}
        # to work around i#84 be sure to put a space after -D for 1st arg at least
        ARGS -D lib_fileloc=${CMAKE_CURRENT_BINARY_DIR}/${locvar_name}
             -D check_textrel=${check_textrel}
             -D check_deps=${check_deps}
             -D check_libc=${BUILD_PACKAGE}
             -D check_interp=ON
             -D READELF_EXECUTABLE=${READELF_EXECUTABLE}
             -P ${CMAKE_CURRENT_SOURCE_DIR}/CMake_readelf.cmake
        VERBATIM # recommended: p260
        )
    endif ()
  endif ()

  # we used to run size on libdynamorio.so for info purposes

else (UNIX)

  # Set up .def file.  Since there's only one and it does not depend on the
  # sources we can do this at configure time and not require a separate script.
  if (X64)
    set(DEF_SRC lib/dr_ntdll_x64.def)
  else (X64)
    set(DEF_SRC lib/dr_ntdll.def)
  endif (X64)
  set(DEF_BLD ${CMAKE_CURRENT_BINARY_DIR}/dr_ntdll.def)
  file(READ ${DEF_SRC} string)
  file(WRITE ${DEF_BLD} "${string}")
  set(FORWARD_TO_NTDLL "/def:\"${DEF_BLD}\" /ignore:4197")

  # PR 219380: we export ntdll routines from dynamorio.dll by forwarding them.
  # However, the linker tries to be smart and thinks our _snprintf forward
  # is the same as our own snprintf: it removes the latter and issues a warning.
  # We re-add our own snprintf by listing it in the .def file, and we suppress
  # the warning using the secret option "/ignore".
  set(dynamorio_link_flags /NODEFAULTLIB)
  if (X64)
    # Xref PR 215395, we currently require DynamoRIO to be loaded in a certain address
    # range.  To be double sure we compile it FIXED for now.
    set(dynamorio_link_flags "${dynamorio_link_flags} /dynamicbase:no /FIXED")
  endif (X64)
  if (DEBUG)
    set(LD_FLAGS "${LINK_DBG} /release /opt:ref")
  else (DEBUG)
    set(LD_FLAGS "${LINK_DBG} /release /opt:ref /opt:icf")
  endif (DEBUG)
  if (SET_PREFERRED_BASE)
    set(dynamorio_link_flags "${dynamorio_link_flags} /base:${preferred_base}")
  endif (SET_PREFERRED_BASE)
  if (X64)
    set(LD_FLAGS "${LD_FLAGS} /machine:X64")
  else (X64)
    set(LD_FLAGS "${LD_FLAGS} /machine:I386")
  endif (X64)
  set(dynamorio_link_flags
    "${LD_FLAGS} ${ntimp_flags} ${dynamorio_link_flags} /dll /incremental:no")
  set(dynamorio_link_flags
    "${dynamorio_link_flags} ${NOLIBC_DLL_ENTRY} ${FORWARD_TO_NTDLL}")
  # cmake does /out, /implib, and /pdb for us, but we do want map file
  # XXX i#1557: replace w/ $<TARGET_FILE:dynamorio> to satisfy CMP0026
  DynamoRIO_get_full_path(drout dynamorio "${location_suffix}")
  get_filename_component(drpath ${drout} PATH)
  get_filename_component(drname ${drout} NAME_WE)
  set(dynamorio_link_flags
    "${dynamorio_link_flags} /map:\"${drpath}/${drname}.map\" /mapinfo:exports")
  # export functions in x86.asm where we can't just say __declspec(dllexport))
  set(dynamorio_link_flags
    "${dynamorio_link_flags} /export:dynamo_auto_start /export:dynamorio_app_take_over")
  if (APP_EXPORTS)
    set(dynamorio_link_flags
      "${dynamorio_link_flags} /export:dr_app_start /export:dr_app_take_over")
    set(dynamorio_link_flags
      "${dynamorio_link_flags} /export:dr_app_running_under_dynamorio")
  endif (APP_EXPORTS)
  set(dynamorio_link_flags "${dynamorio_link_flags} /export:dr_try_start")
  set(dynamorio_link_flags "${dynamorio_link_flags} /export:dr_call_on_clean_stack")
  if (NOT X64)
    set(dynamorio_link_flags "${dynamorio_link_flags} /export:dr_invoke_x64_routine")
  endif (NOT X64)
  if (WINDOWS)
    set(dynamorio_link_flags
      "${dynamorio_link_flags} /export:dynamorio_earliest_init_takeover")
  endif (WINDOWS)

  if (NOT CMAKE_C_COMPILER_VERSION VERSION_LESS 18.0)
    # i#1376: with VS2013 we end up with an import of "_except1" from msvcr*.dll,
    # regardless of flags like QIfist, /fp:except-, /fp:fast, or /Qfast_transcendentals.
    # Our solution is to not link with msvcrt.lib and rely on ntdll having _aulldvrm.
    # This means that dynamorio.dll built with VS2013 won't run on 2K or NT.

    # case 4125: we link with msvcrt.lib for vc8 /O2's use of _alldvrm
    # note that _alldvrm is in ntdll.dll on xp+; but for pre-xp uses we
    # stick with the static linking of the impl in msvcrt.lib.
    # We must link in custom flags and not target_link_libraries() b/c we need
    # to be AFTER our ntdll_imports.lib.
    set(dynamorio_link_flags "${dynamorio_link_flags} msvcrt.lib")
  endif ()

  set_target_properties(dynamorio PROPERTIES LINK_FLAGS "${dynamorio_link_flags}")

  # ensure there are no dependencies other than ntdll
  find_program(DUMPBIN_EXECUTABLE dumpbin.exe HINTS "${cl_path}" DOC "path to dumpbin.exe")
  if (DUMPBIN_EXECUTABLE)
    add_custom_command(TARGET dynamorio
      POST_BUILD
      COMMAND ${CMAKE_COMMAND}
        ARGS -D lib=${drout}
             -D DUMPBIN_EXECUTABLE=${DUMPBIN_EXECUTABLE}
             -P ${CMAKE_CURRENT_SOURCE_DIR}/CMake_checkdeps.cmake
        VERBATIM)
  else ()
    message(STATUS "Cannot find dumpbin so not performing dependence check")
  endif ()
endif (UNIX)

###########################################################################
# DynamoRIO static core library

# hide_symbols is only honored for UNIX.
# i#3348, i#5574: Note that hide_symbols is not safe. It leads to confusing
# behavior where different libraries may see different definitions of exported
# symbols, without obeying WEAK. E.g. the weakly defined routines in drlibc will
# not be overriden by their respective strong definitions in core DR, for the
# callsites in drlibc itself.
function (configure_static_core_lib name hide_symbols)
  add_library(${name} STATIC
    ${CORE_SRCS} ${ARCH_SRCS} ${OS_SRCS})

  configure_core_lib(${name})

  append_property_list(TARGET ${name} COMPILE_DEFINITIONS "STATIC_LIBRARY")

  if (UNIX)
    # We build static DR as PIC in case we're linked into a .so or a PIE.
    append_property_string(TARGET ${name} COMPILE_FLAGS "-fPIC")
    target_link_libraries(${name} dl)

    # We need to do extra work to hide symbols in the static library build.
    # First we do a partial link with ld -r, which makes a single libdynamorio.o
    # object file.  Then we use objcopy --localize-hidden to hide all
    # non-exported symbols, if hide_symbols is set.
    if (NOT EXISTS ${CMAKE_OBJCOPY})
      message( "${name} requires objcopy")
    endif ()

    set(locvar_name ${name}_loc)
    file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/${locvar_name}.cmake" CONTENT
      "set(${locvar_name} \"$<TARGET_FILE:${name}>\")\n")

    string(REPLACE " " "\;" partial_link_flags "${CMAKE_C_FLAGS}")  # Get -m32/64

    set(disable_pie_flag)
    # Older versions of GCC do not understand "-no-pie". See i#2083.
    CHECK_C_COMPILER_FLAG("-no-pie" no_pie_avail)
    if (no_pie_avail)
      # If the compiler's default is "-pie", we must specify "-no-pie" when using "-r"
      # to avoid the error "-r and -pie may not be used together". See i#2083.
      set(disable_pie_flag "-no-pie")
    endif ()

    add_custom_command(TARGET ${name}
      POST_BUILD
      COMMAND ${CMAKE_COMMAND}
      ARGS -D lib_fileloc=${CMAKE_CURRENT_BINARY_DIR}/${locvar_name}
           -D CMAKE_C_COMPILER=${CMAKE_C_COMPILER}
           -D partial_link_flags=${partial_link_flags}
           -D disable_pie_flag=${disable_pie_flag}
           -D localize_hidden=${hide_symbols}
           -D CMAKE_OBJCOPY=${CMAKE_OBJCOPY}
           -D CMAKE_AR=${CMAKE_AR}
           -D CMAKE_RANLIB=${CMAKE_RANLIB}
           -P ${CMAKE_CURRENT_SOURCE_DIR}/CMake_finalize_static_lib.cmake
      WORKING_DIRECTORY ${DR_LIBRARY_OUTPUT_DIRECTORY}
      VERBATIM
      )

  else (UNIX)

    # i#975: if we're building static, none of the shared lib link flags are going
    # to make it through.  But target_link_libraries will.  We name a C library
    # here to make sure things get linked in the right order to avoid duplicate
    # definitions.  See the case 4125 msvcrt comment above about why we need a C
    # library.
    target_link_libraries(${name} ${WIN32_C_LIB} ntdll_imports)

  endif (UNIX)
endfunction()

# XXX i#1997: not fully supported on Mac yet
if (NOT APPLE)
  # We do not hide symbols while building dynamorio_static, because it is unsafe
  # to do so. See comment at configure_static_core_lib for more details.
  # There are downsides to not hiding symbols that we have to live with,
  # including pushing our many global symbols into the app namespace. We have
  # renamed some of the ones that are more likely to collide in #3348.
  configure_static_core_lib(dynamorio_static OFF)
  if (LINUX AND READELF_EXECUTABLE)
    # We already located readelf above for the shared lib.
    # We also already created a loc.cmake file in configure_static_core_lib.
    set(locvar_name dynamorio_static_loc)
    add_custom_command(TARGET dynamorio_static
      POST_BUILD
      COMMAND ${CMAKE_COMMAND}
      ARGS -D lib_fileloc=${CMAKE_CURRENT_BINARY_DIR}/${locvar_name}
           -D READELF_EXECUTABLE=${READELF_EXECUTABLE}
           -D CMAKE_SYSTEM_PROCESSOR=${CMAKE_SYSTEM_PROCESSOR}
           -D X86=${X86}
           -D X64=${X64}
           -D DEBUG=${DEBUG}
           -P ${CMAKE_CURRENT_SOURCE_DIR}/CMake_symbol_check.cmake
      VERBATIM
      )
  endif ()
endif (NOT APPLE)

###########################################################################
# Preload library

add_library(${PRELOAD_NAME} SHARED ${PRELOAD_SRCS})
# We do not issue:
#   target_link_libraries(${PRELOAD_NAME} dynamorio)
# b/c preload dynamically loads dynamorio by name and does not have a
# static dependence.  Plus, by having a static dependence we hit issues
# like PR 258636.  We also don't set rpath b/c that only applies to executables.
if (UNIX)
  # we keep in {debug,release} subdir
  set(preload_dest "${DR_LIBRARY_OUTPUT_DIRECTORY}")
else (UNIX)
  # we keep in lib{32,64} base dir
  set(preload_dest "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}")
endif (UNIX)
set_target_properties(${PRELOAD_NAME} PROPERTIES
  LIBRARY_OUTPUT_DIRECTORY${location_suffix} "${preload_dest}"
  RUNTIME_OUTPUT_DIRECTORY${location_suffix} "${preload_dest}"
  ARCHIVE_OUTPUT_DIRECTORY${location_suffix} "${preload_dest}")
add_gen_events_deps(${PRELOAD_NAME})
if (AARCH64)
  add_dependencies(${PRELOAD_NAME} gen_aarch64_codec)
elseif (RISCV64)
  add_dependencies(${PRELOAD_NAME} gen_riscv64_codec)
endif()

if ("${CMAKE_GENERATOR}" MATCHES "Visual Studio")
  add_dependencies(${PRELOAD_NAME} ${preinject_asm_tgt})
endif ()
target_link_libraries(${PRELOAD_NAME} drlibc)
if (UNIX)
  # We need our own memcpy + memset for isolation.
  target_link_libraries(${PRELOAD_NAME} drmemfuncs)
endif ()
copy_target_to_device(${PRELOAD_NAME} "${location_suffix}")

# drpreinject.dll doesn't link in instr_shared.c so we can't include our inline
# functions.
set_target_properties(${PRELOAD_NAME} PROPERTIES
  COMPILE_DEFINITIONS "NOT_DYNAMORIO_CORE_PROPER;RC_IS_PRELOAD;DR_NO_FAST_IR")

if (UNIX)
  # FIXME case 69/1891: -z initfirst = initialize first at runtime (before libc)
  set_target_properties(${PRELOAD_NAME} PROPERTIES
    LINK_FLAGS "-nostartfiles")
  if (APPLE)
    # Linker complains about unresolved symbols.
    # This means we can't use preload by itself -- but these days we never do that.
    target_link_libraries(${PRELOAD_NAME} dynamorio)
    # Needs explicit -init (regardless of name: _init, init, etc.)
    append_property_string(TARGET ${PRELOAD_NAME} LINK_FLAGS "-init __init")
  endif (APPLE)
  if (LINUX AND READELF_EXECUTABLE)
    set(locvar_name drpreloadloc)
    file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/${locvar_name}.cmake" CONTENT
      "set(${locvar_name} \"$<TARGET_FILE:${PRELOAD_NAME}>\")\n")
    add_custom_command(TARGET ${PRELOAD_NAME}
      POST_BUILD
      COMMAND ${CMAKE_COMMAND}
      # to work around i#84 be sure to put a space after -D for 1st arg at least
      ARGS -D lib_fileloc=${CMAKE_CURRENT_BINARY_DIR}/${locvar_name}
           -D check_textrel=ON
           -D check_deps=OFF
           -D check_libc=${BUILD_PACKAGE}
           -D check_interp=ON
           -D READELF_EXECUTABLE=${READELF_EXECUTABLE}
           -P ${CMAKE_CURRENT_SOURCE_DIR}/CMake_readelf.cmake
      VERBATIM # recommended: p260
      )
  endif ()
else (UNIX)
  set(pre_link_flags
    # preinjector requires no C library for its unload-self method
    # not bothering with map files
    "${LD_FLAGS} /dll /incremental:no /base:0x14000000 /NODEFAULTLIB ${NOLIBC_DLL_ENTRY}")
  set_target_properties(${PRELOAD_NAME} PROPERTIES LINK_FLAGS "${pre_link_flags}")
  # case 4125: we link with msvcrt.lib for vc8 /O2's use of _alldvrm
  target_link_libraries(${PRELOAD_NAME} ${static_libc_list} ntdll_imports
    kernel32 dynamorio)
endif (UNIX)

###########################################################################
# Decoding library

# static decoding library
add_library(drdecode STATIC
  ${DECODER_SRCS}
  ir/decodelib.c
  string.c
  io.c
  )
set_target_properties(drdecode PROPERTIES
  COMPILE_DEFINITIONS "NOT_DYNAMORIO_CORE_PROPER;STANDALONE_DECODER")
if (UNIX)
  append_property_string(TARGET drdecode COMPILE_FLAGS "-fPIC")
endif (UNIX)

add_gen_events_deps(drdecode)
if (AARCH64)
  add_dependencies(drdecode gen_aarch64_codec)
elseif (RISCV64)
  add_dependencies(drdecode gen_riscv64_codec)
endif()
target_link_libraries(drdecode drlibc)

###########################################################################
# Injection library

# drinjectlib
# i#1737: we used to have drconfiglib as a shared library, but that led to a
# lot of complexity on UNIX (setting
# INSTALL_NAME_DIR to "@rpath" for Mac i#1375, calling add_rel_rpaths() for
# every executable using them, problems on Android where DT_RUNPATH isn't
# supported, etc.) so we switched to static.
# It would be nice to be static on Windows to avoid copying the lib into bin dirs,
# but duplicate symbol problems make that non-trivial: we need i#1409.
if (WIN32)
  set(inject_lib_type SHARED)
else ()
  set(inject_lib_type STATIC)
endif ()
add_library(drinjectlib ${inject_lib_type} ${INJECTOR_SRCS})
add_gen_events_deps(drinjectlib)
target_link_libraries(drinjectlib drdecode drlibc)
if (UNIX)
  # We need our own memcpy + memset to avoid glibc versioning (i#1504).
  target_link_libraries(drinjectlib drmemfuncs)
endif ()
set_target_properties(drinjectlib PROPERTIES
  # Set define parameters for resources.rc
  COMPILE_DEFINITIONS "NOT_DYNAMORIO_CORE_PROPER;RC_IS_DRINJECTLIB")
if (WIN32)
  set(drinjectlib_link_flags
    # not bothering with map files
    "${LD_FLAGS} /incremental:no /subsystem:console /NODEFAULTLIB")
  # If we use ${ntimp_flags} in the link flags and thus prior to libc from the
  # target_link_libraries() list, on VS2005 and VS2008 we end up with duplicate
  # symbols atoi and iswctype from libcmtd and ntdll.  Unlike dynamorio.dll we're
  # ok w/ libc syms so we re-order and take the static syms first.
  set_target_properties(drinjectlib PROPERTIES LINK_FLAGS "${drinjectlib_link_flags}")
  # I'm adding libcmt to attempt to make sure we have multithreaded C lib:
  target_link_libraries(drinjectlib drdecode ${static_libc_list}
    ntdll_imports kernel32 advapi32 imagehlp)
  # drinject.exe needs a copy in the bin dir (i#1737 would eliminate this)
  add_custom_command(TARGET drinjectlib POST_BUILD COMMAND ${CMAKE_COMMAND}
    ARGS -E copy "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/drinjectlib.dll"
    "${EXECUTABLE_OUTPUT_DIRECTORY}/" VERBATIM)
endif (WIN32)

###########################################################################
# Annotations

if (ANNOTATIONS)
  set(BUILD_ANNOTATION "${BUILD_INCLUDE}/annotations")
  set(BUILD_ANNOTATION "${BUILD_INCLUDE}/annotations" PARENT_SCOPE)
  file(MAKE_DIRECTORY ${BUILD_ANNOTATION})
  if (NOT WINDOWS OR NOT X64)
    configure_file("${PROJECT_SOURCE_DIR}/third_party/valgrind/valgrind.h"
      "${BUILD_ANNOTATION}/valgrind.h" COPYONLY)
    configure_file("${PROJECT_SOURCE_DIR}/third_party/valgrind/memcheck.h"
      "${BUILD_ANNOTATION}/memcheck.h" COPYONLY)
  endif (NOT WINDOWS OR NOT X64)
  configure_file("${PROJECT_SOURCE_DIR}/core/lib/dr_annotations_asm.h"
    "${BUILD_ANNOTATION}/dr_annotations_asm.h" COPYONLY)
  configure_file("${PROJECT_SOURCE_DIR}/core/lib/dr_annotations.h"
    "${BUILD_ANNOTATION}/dr_annotations.h" COPYONLY)
  configure_file("${PROJECT_SOURCE_DIR}/core/lib/dr_annotations.c"
    "${BUILD_ANNOTATION}/dr_annotations.c" COPYONLY)

  include_directories("${BUILD_ANNOTATION}")
endif ()

###########################################################################
# Unit tests

# We can't run core unit tests when the target is not the host, just like we can't
# run DR itself managing an app: DR is a same-arch system for code cache operation
# and only supports target!=host for decoding and drmemtrace analysis (i#1684).
if (BUILD_TESTS AND
    ((CMAKE_CROSSCOMPILING AND DEFINED CMAKE_FIND_ROOT_PATH) OR
      (NOT DR_HOST_NOT_TARGET)))
  add_executable(unit_tests unit_tests.c
    # These unit tests have been moved from the x86_code module into a new x86_code
    # test module that gets its own clang/gcc options for testing (-mno-vzeroupper).
    # We want this option to apply only to code compiled for unit_test. Also clang
    # specific target options break the clang build (xref i#3458).
    ${CORE_SRCS} ${ARCH_SRCS} ${OS_SRCS} arch/x86_code_test.c)
  add_gen_events_deps(unit_tests)
  if ("${CMAKE_GENERATOR}" MATCHES "Visual Studio")
    # for parallel build correctness we need a target dependence
    add_dependencies(unit_tests ${arch_core_asm_tgt} ${archshared_core_asm_tgt}
      ntdll_imports)
  endif ()
  set(unit_tests_extra_flags "")
  if (UNIX AND NOT DEBUG)
    # -Wno-array-bounds to work around compiler bug (i#1796)
    set(unit_tests_extra_flags "${unit_tests_extra_flags} -Wno-array-bounds")
  endif ()
  if (proc_supports_avx512)
    if (UNIX)
      # XXX i#3459: add Windows support.
      set(unit_tests_extra_flags
        "${unit_tests_extra_flags} ${CFLAGS_AVX512}")
    endif ()
  elseif (proc_supports_avx)
    if (UNIX)
      # XXX i#3459: add Windows support.
      set(unit_tests_extra_flags
        "${unit_tests_extra_flags} ${CFLAGS_AVX}")
    endif ()
  endif ()
  if (proc_supports_avx OR proc_supports_avx512)
    if (UNIX)
      # XXX i#3459: add Windows support.
      # Force the compiler to not apply vzeroupper HW performance optimizations
      # that applies to most Intel processors. In the unit test, this would
      # destroy the upper halfs of the AVX registers we want to read.
      if (CLANG)
        set_source_files_properties(arch/x86_code_test.c PROPERTIES COMPILE_FLAGS
          "-mllvm -x86-use-vzeroupper=0")
      else ()
        set_source_files_properties(arch/x86_code_test.c PROPERTIES COMPILE_FLAGS
          "-mno-vzeroupper")
      endif ()
    endif ()
  endif ()
  set_target_properties(unit_tests PROPERTIES
    COMPILE_DEFINITIONS "RC_IS_TEST;STANDALONE_UNIT_TEST"
    COMPILE_FLAGS "${unit_tests_extra_flags}"
    RUNTIME_OUTPUT_DIRECTORY${location_suffix} "${EXECUTABLE_OUTPUT_DIRECTORY}")
  if (UNIX)
    if (NOT APPLE)
      # i#1228 make sure entry point of the elf binary pointing to _start.
      # Otherwise, the entry point will point to the start of .text section,
      # which might not be _start.
      append_property_string(TARGET unit_tests LINK_FLAGS "-Wl,${ld_entry_flag},_start")
    endif ()
    if (NOT ANDROID) # everything is inside Bionic on Android
      target_link_libraries(unit_tests c dl m pthread)
    endif ()
    set_preferred_base_start_and_end(unit_tests ${preferred_base} ON)
  else (UNIX)
    # Just like drinjectlib (see above) we need libc before ntdll
    target_link_libraries(unit_tests ${static_libc_list} ntdll_imports)
    set_target_properties(unit_tests PROPERTIES
      LINK_FLAGS "/base:${preferred_base} ${LD_FLAGS}")
  endif (UNIX)
  target_link_libraries(unit_tests drlibc)
  get_target_path_for_execution(unit_relpath unit_tests "${location_suffix}")
  prefix_cmd_if_necessary(unit_relpath OFF ${unit_relpath})
  add_test(unit_tests ${unit_relpath})
  if (APPLE)
    set_tests_properties(unit_tests PROPERTIES LABELS OSX)
  endif ()
  if (AARCHXX)
    set_tests_properties(unit_tests PROPERTIES LABELS RUNS_ON_QEMU)
  endif ()
  copy_target_to_device(unit_tests "${location_suffix}")
endif ()

###########################################################################

# Do not put core/ into the include dirs as our link.h will conflict
# with /usr/include/link.h!
include_directories(BEFORE
  ${CMAKE_CURRENT_SOURCE_DIR}/lib
  ${CMAKE_CURRENT_SOURCE_DIR}/arch
  ${CMAKE_CURRENT_SOURCE_DIR}/ir
  ${CMAKE_CURRENT_SOURCE_DIR}/${OSNAME}
  ${CMAKE_CURRENT_SOURCE_DIR}/arch/${ARCH_NAME}
  ${CMAKE_CURRENT_SOURCE_DIR}/ir/${ARCH_NAME}
  ${CMAKE_CURRENT_SOURCE_DIR}/drlibc
  )

# Avoid transitive linking for imported target.
# This is a shared library, after all, and ntdll.lib is typically not available.
set_target_properties(dynamorio PROPERTIES INTERFACE_LINK_LIBRARIES "")
install_exported_target(dynamorio ${INSTALL_LIB}
  # CMake doesn't set +x on shared libraries, so we have to ask for it.
  PERMISSIONS ${owner_access} OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)
DR_export_target(dynamorio)

# XXX i#1997: not fully supported on Mac yet
if (NOT APPLE)
  install_exported_target(dynamorio_static ${INSTALL_LIB})
  DR_export_target(dynamorio_static)
endif ()

DR_export_header(${CMAKE_CURRENT_SOURCE_DIR}/lib/globals_api.h dr_defines.h)
DR_export_header(${CMAKE_CURRENT_SOURCE_DIR}/lib/mcxtx_api.h dr_mcxtx.h)
DR_export_header(${CMAKE_CURRENT_SOURCE_DIR}/lib/dr_events.h dr_events.h)
DR_export_header(${CMAKE_CURRENT_SOURCE_DIR}/ir/encode_api.h dr_ir_encode.h)
DR_export_header(${CMAKE_CURRENT_SOURCE_DIR}/ir/decode_api.h dr_ir_decode.h)
DR_export_header(${CMAKE_CURRENT_SOURCE_DIR}/ir/disassemble_api.h dr_ir_disassemble.h)
DR_export_header(${CMAKE_CURRENT_SOURCE_DIR}/ir/opnd_api.h dr_ir_opnd.h)
DR_export_header(${CMAKE_CURRENT_SOURCE_DIR}/ir/instr_api.h dr_ir_instr.h)
DR_export_header(${CMAKE_CURRENT_SOURCE_DIR}/ir/instr_inline_api.h dr_ir_instr_inline.h)
DR_export_header(${CMAKE_CURRENT_SOURCE_DIR}/ir/instrlist_api.h dr_ir_instrlist.h)
DR_export_header(${CMAKE_CURRENT_SOURCE_DIR}/ir/dr_ir_opcodes.h dr_ir_opcodes.h)
DR_export_header(${CMAKE_CURRENT_SOURCE_DIR}/ir/x86/opcode_api.h dr_ir_opcodes_x86.h)
DR_export_header(${CMAKE_CURRENT_SOURCE_DIR}/ir/arm/opcode_api.h dr_ir_opcodes_arm.h)
DR_export_header(${CMAKE_CURRENT_SOURCE_DIR}/ir/instr_create_shared_api.h dr_ir_macros.h)
DR_export_header(${CMAKE_CURRENT_SOURCE_DIR}/ir/x86/instr_create_api.h dr_ir_macros_x86.h)
DR_export_header(${CMAKE_CURRENT_SOURCE_DIR}/ir/arm/instr_create_api.h dr_ir_macros_arm.h)
DR_export_header(${CMAKE_CURRENT_SOURCE_DIR}/ir/aarch64/instr_create_api.h
  dr_ir_macros_aarch64.h)
DR_export_header(${CMAKE_CURRENT_SOURCE_DIR}/lib/dr_ir_utils.h dr_ir_utils.h)
DR_export_header(${CMAKE_CURRENT_SOURCE_DIR}/lib/dr_tools.h dr_tools.h)
DR_export_header(${CMAKE_CURRENT_SOURCE_DIR}/annotations_api.h dr_annotation.h)
DR_export_header(${CMAKE_CURRENT_SOURCE_DIR}/lib/dr_inject.h dr_inject.h)
DR_export_header(${CMAKE_CURRENT_SOURCE_DIR}/module_api.h dr_modules.h)
DR_export_header(${CMAKE_CURRENT_SOURCE_DIR}/os_api.h dr_os_utils.h)
DR_export_header(${CMAKE_CURRENT_SOURCE_DIR}/fragment_api.h dr_tracedump.h)
DR_export_header(${CMAKE_CURRENT_SOURCE_DIR}/arch/proc_api.h dr_proc.h)
if (HOT_PATCHING_INTERFACE)
  DR_export_header(${CMAKE_CURRENT_SOURCE_DIR}/lib/dr_probe.h dr_probe.h)
endif ()

install_exported_target(drinjectlib ${INSTALL_LIB_BASE})
DR_export_target(drinjectlib)
if (UNIX)
  DR_install(TARGETS ${PRELOAD_NAME} DESTINATION ${INSTALL_LIB})
  DR_install(FILES ${COMPAT_SYMLINKS} DESTINATION ${INSTALL_LIB})
else (UNIX)
  # we put drinjectlib into bin for use by our tools (i#1737 would eliminate this)
  DR_install(TARGETS drinjectlib DESTINATION ${INSTALL_BIN})
  DR_install(TARGETS ${PRELOAD_NAME} RUNTIME DESTINATION ${INSTALL_LIB_BASE})
  DR_install(TARGETS drearlyhelp1 RUNTIME DESTINATION ${INSTALL_LIB_BASE})
  DR_install(TARGETS drearlyhelp2 RUNTIME DESTINATION ${INSTALL_LIB_BASE})
endif (UNIX)

DR_export_target(drdecode)
install_exported_target(drdecode ${INSTALL_LIB})

# We have to export drlibc as static libs we're exporting depend on it.
DR_export_target(drlibc)
install_exported_target(drlibc ${INSTALL_LIB_BASE})
if (UNIX)
  DR_export_target(drmemfuncs)
  install_exported_target(drmemfuncs ${INSTALL_LIB_BASE})
endif ()
