# **********************************************************
# Copyright (c) 2010-2014 Google, Inc.    All rights reserved.
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

###########################################################################
# sources and targets

# we have no exe's here, and we want our dll's in the lib dir
# (could use MODULE instead of SHARED if it would let us link)
set(EXECUTABLE_OUTPUT_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}")
set_per_config_ouput_to_match_single_config()

set(asm_deps
  "${PROJECT_SOURCE_DIR}/core/x86/asm_defines.asm"
  "${PROJECT_BINARY_DIR}/configure.h")
add_asm_target(x86/x86.asm x86_core_asm_src x86_core_asm_tgt "_core" "" "${asm_deps}")
add_asm_target(x86/x86.asm x86_noncore_asm_src x86_noncore_asm_tgt "_noncore"
  "-DNOT_DYNAMORIO_CORE_PROPER" "${asm_deps}")
add_asm_target(x86/pre_inject_asm.asm preinject_asm_src preinject_asm_tgt ""
  "-DNOT_DYNAMORIO_CORE_PROPER" "${asm_deps}")

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
  instrlist.c
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
  )

if (UNIX)
  # i#46: Private string routines for libc independence.  On Windows our
  # bootstrapping loader finds these imports in ntdll.  If that becomes
  # problematic, we can find a way to compile this on Windows.
  set (CORE_SRCS ${CORE_SRCS} string.c)
endif (UNIX)

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
endif (WIN32 AND NOT X64)

set(ARCH_DIRNAME x86)
set(ARCH_SRCS
  x86/arch.c
  x86/decode.c
  x86/decode_table.c
  x86/encode.c
  x86/emit_utils.c
  x86/instr.c
  x86/interp.c
  x86/proc.c
  x86/mangle.c
  x86/steal_reg.c
  x86/x86_code.c
  x86/disassemble.c
  ${x86_core_asm_src}
  x86/decode_fast.c
  x86/optimize.c
  x86/sideline.c
  x86/instrument.c
  x86/retcheck.c
  )

if (X64)
  set(ARCH_SRCS ${ARCH_SRCS} x86/x86_to_x64.c)
endif (X64)

# These custom commands are per-subdir so can't just place at top level
include(../make/CMake_events.cmake)
include(../make/ntdll_imports.cmake)

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

# Just like i#801, we can't put custom targets in ntdll_imports.cmake
if ("${CMAKE_GENERATOR}" MATCHES "Visual Studio")
  # for correct parallel builds we need a target to avoid
  # duplicate and racy VS per-project rules
  add_custom_target(ntimp_tgt DEPENDS "${ntimp_lib}")
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
    set(OS_SRCS ${OS_SRCS} unix/memquery_linux.c)
    set(OS_SRCS ${OS_SRCS} unix/memquery.c)
    set(OS_SRCS ${OS_SRCS} unix/memcache.c)
    set(OS_SRCS ${OS_SRCS} unix/module_elf.c)
    set(OS_SRCS ${OS_SRCS} unix/ksynch_linux.c)
    set(OS_SRCS ${OS_SRCS} unix/tls_linux.c)
    set(OS_SRCS ${OS_SRCS} unix/signal_linux.c)
    set(OS_SRCS ${OS_SRCS} unix/native_elf.c)
    # XXX i#1286: should be split into nudge_linux.c and nudge_macos.c
    set(OS_SRCS ${OS_SRCS} unix/nudgesig.c)
  elseif (APPLE)
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
    unix/os.c
    ${x86_noncore_asm_src}
    # i#1334: private string routines for libc independence
    string.c
    io.c
    )
  set(INJECTOR_SRCS
    unix/injector.c
    config.c    # to read config
    unix/os.c  # for file_t interface
    unix/module.c
    string.c
    io.c
    ${x86_noncore_asm_src}  # for dynamorio_syscall
    )
  if (APPLE)
    set(INJECTOR_SRCS ${INJECTOR_SRCS} unix/module_macho.c)
  else (APPLE)
    set(INJECTOR_SRCS ${INJECTOR_SRCS} unix/module_elf.c)
  endif ()
else (UNIX)
  set(OSNAME win32)
  set(OS_SRCS
    win32/eventlog.c
    win32/os.c
    win32/syscall.c
    win32/callback.c
    win32/drmarker.c
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
    ${ntimp_dep}
   )
  set(PRELOAD_SRCS
    win32/pre_inject.c
    win32/ntdll.c
    win32/inject_shared.c
    win32/drmarker.c
    ${x86_noncore_asm_src}
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
    win32/ntdll.c
    win32/inject_shared.c
    ${x86_noncore_asm_src}
    win32/resources.rc
    config.c
    win32/os.c
    io.c
    ${ntimp_dep}
    )

  add_library(drearlyhelp1 SHARED
    win32/early_inject_helper1.c
    win32/resources.rc
    )
  add_library(drearlyhelp2 SHARED
    win32/early_inject_helper2.c
    win32/resources.rc
    )
endif (UNIX)

add_library(dynamorio ${SHARED_OR_STATIC}
  ${CORE_SRCS} ${ARCH_SRCS} ${OS_SRCS}
  # FIXME: any other way to add a dependence?  this is for export_list,
  # where the PRE_LINK custom command doesn't support DEPENDS.
  # Apparently just files w/ certain extensions are pulled out of here
  # for automatic rules.
  ${CMAKE_CURRENT_SOURCE_DIR}/lib/genapi.pl
  # ditto: adding here for dependence
  ${CMAKE_CURRENT_SOURCE_DIR}/CMake_ldscript.cmake
  )
set_target_properties(dynamorio PROPERTIES
  LIBRARY_OUTPUT_DIRECTORY${location_suffix} "${DR_LIBRARY_OUTPUT_DIRECTORY}"
  RUNTIME_OUTPUT_DIRECTORY${location_suffix} "${DR_LIBRARY_OUTPUT_DIRECTORY}"
  ARCHIVE_OUTPUT_DIRECTORY${location_suffix} "${DR_LIBRARY_OUTPUT_DIRECTORY}")
add_gen_events_deps(dynamorio)
if ("${CMAKE_GENERATOR}" MATCHES "Visual Studio")
  # for parallel build correctness we need a target dependence
  add_dependencies(dynamorio ${x86_core_asm_tgt} ntimp_tgt)
endif ()

if (UNIX)
  # We always use a script for our own library bounds (PR 361594).
  # We could build this at configure time instead of build time as
  # it does not depend on the source files.
  # FIXME: this is duplicated in DynamoRIOConfig.cmake
  set(ld_script ${CMAKE_CURRENT_BINARY_DIR}/ldscript)
  set_directory_properties(PROPERTIES
    ADDITIONAL_MAKE_CLEAN_FILES "${ld_script}")

  function (set_preferred_base_start_and_end target)
    if (APPLE)
      set(ldflags "-image_base ${preferred_base}")
    elseif (NOT LINKER_IS_GNU_GOLD)
      set(ldflags "-Wl,${ld_script_option},\"${ld_script}\"")
      add_custom_command(TARGET ${target}
        PRE_LINK
        COMMAND ${CMAKE_COMMAND}
        # script does not inherit any vars so we must pass them all in
        # to work around i#84 be sure to put a space after -D for 1st arg at least
        ARGS -D outfile=${ld_script}
             -DCMAKE_LINKER=${CMAKE_LINKER}
             -DCMAKE_COMPILER_IS_GNUCC=${CMAKE_COMPILER_IS_GNUCC}
             -DLD_FLAGS=${LD_FLAGS}
             -Dset_preferred=${SET_PREFERRED_BASE}
             -Dreplace_maxpagesize=${REPLACE_MAXPAGESIZE}
             -Dpreferred_base=${preferred_base}
             -P ${PROJECT_SOURCE_DIR}/core/CMake_ldscript.cmake
        VERBATIM # recommended: p260
        )
    else ()
      set(ldflags "")
      if (SET_PREFERRED_BASE)
        # FIXME: This should be -Ttext-segment for bfd, but most golds want -Ttext.
        # See http://sourceware.org/ml/binutils/2013-02/msg00194.html
        set(ldflags "-Wl,-Ttext=${preferred_base}")
      endif ()
      # Add our start and end symbols for library bounds.
      set(ldflags "${ldflags} -Wl,--defsym,dynamorio_so_start=__executable_start")
      set(ldflags "${ldflags} -Wl,--defsym,dynamorio_so_end=end")
    endif ()
    append_property_string(TARGET ${target} LINK_FLAGS "${ldflags}")
  endfunction (set_preferred_base_start_and_end)

endif (UNIX)

# rather than a separate BUILD_INCLUDES option we always build
# include headers if building core (or if building docs or samples as
# they have real dependencies)
if (CLIENT_INTERFACE OR APP_EXPORTS)
  add_dependencies(dynamorio api_headers)
endif ()

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
if ("${CMAKE_GENERATOR}" MATCHES "Visual Studio")
  add_dependencies(${PRELOAD_NAME} ${x86_noncore_asm_tgt} ${preinject_asm_tgt})
endif ()

# static decoding library
add_library(drdecode
  instrlist.c
  x86/decode.c
  x86/decode_table.c
  x86/encode.c
  x86/instr.c
  x86/disassemble.c
  x86/decode_fast.c
  x86/mangle.c
  x86/decodelib.c
  ${x86_noncore_asm_src}
  )
set_target_properties(drdecode PROPERTIES
  COMPILE_FLAGS "-DNOT_DYNAMORIO_CORE_PROPER -DSTANDALONE_DECODER")
if (UNIX)
  append_property_string(TARGET drdecode COMPILE_FLAGS "-fPIC")
endif (UNIX)
if (WIN32 AND "${CMAKE_GENERATOR}" MATCHES "Visual Studio")
  # for parallel build correctness we need a target dependence
  add_dependencies(drdecode ${x86_noncore_asm_tgt})
endif ()
add_gen_events_deps(drdecode)

###########################################################################
# linux build customization

if (UNIX)
  # FIXME case 69/1891: -z initfirst = initialize first at runtime (before libc)
  set_target_properties(${PRELOAD_NAME} PROPERTIES
    LINK_FLAGS "-nostartfiles")

  set(dynamorio_link_flags "")

  # Do non-lazy runtime binding
  if (APPLE)
    set(dynamorio_link_flags "${dynamorio_link_flags} -Xlinker -bind_at_load")
  else (APPLE)
    set(dynamorio_link_flags "${dynamorio_link_flags} -Xlinker -z -Xlinker now")
  endif (APPLE)

  if (APPLE)
    set(dynamorio_link_flags
      "${dynamorio_link_flags} -install_name @rpath/libdynamorio.dylib")
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

  if (STATIC_LIBRARY)
    # We build static DR as PIC in case we're linked into a .so or a PIE.
    append_property_string(TARGET dynamorio COMPILE_FLAGS "-fPIC")
  else ()
    # i#47: set the ELF header entry point to _start for early injection.
    if (APPLE)
      set(dynamorio_link_flags "${dynamorio_link_flags} -Wl,-e,_start")
    else (APPLE)
      set(dynamorio_link_flags "${dynamorio_link_flags} -Wl,--entry,_start")
    endif (APPLE)
  endif ()

  if (NOT HAVE_FVISIBILITY)
    set(export_list ${CMAKE_CURRENT_BINARY_DIR}/exportlist)
    add_custom_command(TARGET dynamorio
      PRE_LINK
      COMMAND ${PERL_EXECUTABLE}
      ARGS ${CMAKE_CURRENT_SOURCE_DIR}/lib/genapi.pl
           # we assume ">" will work in all supported shells
           -filter "${defines}" > ${export_list}
      WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
      VERBATIM # recommended: p260
      )
    set(dynamorio_link_flags
      "${dynamorio_link_flags} -Xlinker --version-script -Xlinker ${export_list}")
  endif (NOT HAVE_FVISIBILITY)

  if (APPLE)
    # Needs explicit -init (regardless of name: _init, init, etc.)
    set(dynamorio_link_flags "${dynamorio_link_flags} -init __init")
  endif (APPLE)

  set_target_properties(dynamorio PROPERTIES
    LINK_FLAGS "${dynamorio_link_flags}")

  # This appends to LINK_FLAGS, so do it after we set them above.
  set_preferred_base_start_and_end(dynamorio)
  # i#46: We're pretty libc independent these days, but we still support
  # -no_private_loader when loaded by the system loader.  We also have a sqrt
  # import that we should cut.
  target_link_libraries(dynamorio dl m)

  # We need to do extra work to hide symbols in the static library build.
  # First we do a partial link with ld -r, which makes a single libdynamorio.o
  # object file.  Then we use objcopy --localize-hidden to hide all
  # non-exported symbols.
  if (STATIC_LIBRARY)
    if (NOT EXISTS ${CMAKE_OBJCOPY})
      message(FATAL_ERROR "STATIC_LIBRARY requires objcopy")
    endif ()
    get_target_property(dynamorio_dot_a dynamorio LOCATION)
    string(REGEX REPLACE "\\.a$" ".o" dynamorio_dot_o "${dynamorio_dot_a}")
    string(REPLACE " " ";" partial_link_flags "${CMAKE_C_FLAGS}")  # Get -m32/64
    set (globalize_pic_thunks)
    if (NOT X64)
      # Localizing PIC thunks on ia32 creates references to discarded sections.
      # XXX: Hardcoding this list of symbols is fragile.  It is known to work
      # with gcc 4.4 and 4.6.  gcc 4.7 uses symbols starting with __x86.
      set (globalize_pic_thunks ${CMAKE_OBJCOPY}
        --globalize-symbol=__i686.get_pc_thunk.bx
        --globalize-symbol=__i686.get_pc_thunk.cx
        --globalize-symbol=__x86.get_pc_thunk.bx
        --globalize-symbol=__x86.get_pc_thunk.cx
        "${dynamorio_dot_o}" &&
        )
    endif ()
    add_custom_command(TARGET dynamorio POST_BUILD
      COMMAND
      ${CMAKE_C_COMPILER} ${partial_link_flags} -fPIC
        -nostartfiles -nodefaultlibs -r
        -Wl,--whole-archive "${dynamorio_dot_a}" -Wl,--no-whole-archive
        -o "${dynamorio_dot_o}" &&
      ${CMAKE_OBJCOPY} --localize-hidden "${dynamorio_dot_o}" &&
      ${globalize_pic_thunks}
      rm -f "${dynamorio_dot_a}" &&
      ${CMAKE_AR} cr "${dynamorio_dot_a}" "${dynamorio_dot_o}" &&
      ${CMAKE_RANLIB} "${dynamorio_dot_a}"
      WORKING_DIRECTORY ${DR_LIBRARY_OUTPUT_DIRECTORY}
      VERBATIM)
  endif ()

  # We use .so versioning via symlinks.
  # We do not version the preload lib as it does not expose an API,
  # and drconfig is Windows-only.
  # Our releases match our API so we use major.minor as our soversion.
  # That allows both forward and backward compatibility, but means that
  # we need to provide symlinks for all backward compatible versions.
  # In the future CMake is expected to add explicit support for this.

  function (generate_soversions verstart verstop)
    while (${verstart} LESS ${verstop})
      math(EXPR COMPAT_MAJOR "${verstart} / 100")
      math(EXPR COMPAT_MINOR "${verstart} % 100")
      get_target_property(curver_base dynamorio LOCATION)
      get_filename_component(curver_path "${curver_base}" PATH)
      get_filename_component(curver_base "${curver_base}" NAME)
      get_target_property(curver_sover dynamorio SOVERSION)
      set(curver "${curver_base}.${curver_sover}")
      string(REGEX REPLACE "so.${VERSION_MAJOR_MINOR}"
        "so.${COMPAT_MAJOR}.${COMPAT_MINOR}" oldver "${curver}")
      set(COMPAT_SYMLINKS ${COMPAT_SYMLINKS} ${curver_path}/${oldver})
      add_custom_command(TARGET dynamorio POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E create_symlink ${curver} ${oldver}
        WORKING_DIRECTORY ${DR_LIBRARY_OUTPUT_DIRECTORY}
        VERBATIM)
      math(EXPR verstart "${verstart} + 1")
    endwhile ()
    set(COMPAT_SYMLINKS ${COMPAT_SYMLINKS} PARENT_SCOPE)
  endfunction (generate_soversions)

  string(REGEX REPLACE
    "^([0-9]+\\.[0-9]+).*" "\\1" VERSION_MAJOR_MINOR "${VERSION_NUMBER}")
  set_target_properties(dynamorio PROPERTIES
    VERSION "${VERSION_MAJOR_MINOR}" SOVERSION "${VERSION_MAJOR_MINOR}")
  # We assume we're still on 3.x
  if (NOT STATIC_LIBRARY)
    generate_soversions(${OLDEST_COMPATIBLE_VERSION} ${VERSION_NUMBER_INTEGER})
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
    if (STATIC_LIBRARY)
      # Skipping readelf check for STATIC_LIBRARY.
    elseif (NOT READELF_EXECUTABLE)
      message("${readelf_name} not found: not checking SElinux or execstack")
    else ()
      get_target_property(lib_to_check dynamorio LOCATION)
      add_custom_command(TARGET dynamorio
        POST_BUILD
        COMMAND ${CMAKE_COMMAND}
        # to work around i#84 be sure to put a space after -D for 1st arg at least
        ARGS -D lib=${lib_to_check}
             -D READELF_EXECUTABLE=${READELF_EXECUTABLE}
             -P ${CMAKE_CURRENT_SOURCE_DIR}/CMake_readelf.cmake
        VERBATIM # recommended: p260
        )
      get_target_property(lib_to_check ${PRELOAD_NAME} LOCATION)
      add_custom_command(TARGET ${PRELOAD_NAME}
        POST_BUILD
        COMMAND ${CMAKE_COMMAND}
        # to work around i#84 be sure to put a space after -D for 1st arg at least
        ARGS -D lib=${lib_to_check}
             -D READELF_EXECUTABLE=${READELF_EXECUTABLE}
             -P ${CMAKE_CURRENT_SOURCE_DIR}/CMake_readelf.cmake
        VERBATIM # recommended: p260
        )
    endif ()
  endif ()

  # we used to run size on libdynamorio.so for info purposes

  set_target_properties(${PRELOAD_NAME} PROPERTIES
    COMPILE_FLAGS "-DNOT_DYNAMORIO_CORE_PROPER -DRC_IS_PRELOAD")

  if (APPLE)
    # Linker complains about unresolved symbols.
    # This means we can't use preload by itself -- but these days we never do that.
    target_link_libraries(${PRELOAD_NAME} dynamorio)
    # Needs explicit -init (regardless of name: _init, init, etc.)
    append_property_string(TARGET ${PRELOAD_NAME} LINK_FLAGS "-init __init")
  endif (APPLE)

else (UNIX)
###########################################################################
# windows build customization

  set_target_properties(dynamorio PROPERTIES
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

  set(NOLIBC_DLL_ENTRY /entry:DllMain)

  # Set up .def file.  Since there's only one and it does not depend on the
  # sources we can do this at configure time and not require a separate script.
  if (X64)
    set(DEF_SRC lib/dr_ntdll_x64.def)
  else (X64)
    set(DEF_SRC lib/dr_ntdll.def)
  endif (X64)
  set(DEF_BLD ${CMAKE_CURRENT_BINARY_DIR}/dr_ntdll.def)
  file(READ ${DEF_SRC} string)
  if (NOT CLIENT_INTERFACE)
    # we're not exporting snprintf() so remove from def file */
    string(REGEX REPLACE
      " snprintf"
      "" string "${string}")
  endif (NOT CLIENT_INTERFACE)
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
  # XXX i#893: VS2012 won't build any target using x86.asm w/ safeseh
  set(LD_FLAGS "${LD_FLAGS} /safeseh:no")
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
  get_target_property(drout dynamorio LOCATION${location_suffix})
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
  if (CLIENT_INTERFACE)
    set(dynamorio_link_flags "${dynamorio_link_flags} /export:dr_try_start")
    if (NOT X64)
      set(dynamorio_link_flags "${dynamorio_link_flags} /export:dr_invoke_x64_routine")
    endif (NOT X64)
  endif (CLIENT_INTERFACE)
  if (WINDOWS)
    set(dynamorio_link_flags
      "${dynamorio_link_flags} /export:dynamorio_earliest_init_takeover")
  endif (WINDOWS)

  # case 4125: we link with msvcrt.lib for vc8 /O2's use of _alldvrm
  # note that _alldvrm is in ntdll.dll on xp+; but for pre-xp uses we
  # stick with the static linking of the impl in msvcrt.lib.
  # We must link in custom flags and not target_link_libraries() b/c we need
  # to be AFTER our ntdll_imports.lib.
  set(dynamorio_link_flags "${dynamorio_link_flags} msvcrt.lib")

  set_target_properties(dynamorio PROPERTIES LINK_FLAGS "${dynamorio_link_flags}")

  if (DEBUG)
    set(WIN32_C_LIB libcmtd)
  else (DEBUG)
    set(WIN32_C_LIB libcmt)
  endif (DEBUG)

  if (NOT STATIC_LIBRARY)
    # ensure there are no dependencies other than ntdll
    find_program(DUMPBIN_EXECUTABLE dumpbin.exe DOC "path to dumpbin.exe")
    if (DUMPBIN_EXECUTABLE)
      add_custom_command(TARGET dynamorio POST_BUILD
        COMMAND ${CMAKE_COMMAND}
        ARGS -D lib=${drout}
             -D DUMPBIN_EXECUTABLE=${DUMPBIN_EXECUTABLE}
             -P ${CMAKE_CURRENT_SOURCE_DIR}/CMake_checkdeps.cmake
        VERBATIM)
    else ()
      message(STATUS "Cannot find dumpbin so not performing dependence check")
    endif ()
  else ()
    # i#975: if we're building static, none of those link flags above are going
    # to make it through.  But target_link_libraries will.  We name a C library
    # here to make sure things get linked in the right order to avoid duplicate
    # definitions.  See the case 4125 msvcrt comment above about why we need a C
    # library.
    target_link_libraries(dynamorio ${WIN32_C_LIB} ${ntimp_lib})
  endif ()


  set_target_properties(${PRELOAD_NAME} PROPERTIES
    COMPILE_FLAGS "-DNOT_DYNAMORIO_CORE_PROPER -DRC_IS_PRELOAD")
  set(pre_link_flags
    # preinjector requires no C library for its unload-self method
    # not bothering with map files
    "${LD_FLAGS} /dll /incremental:no /base:0x14000000 /NODEFAULTLIB ${NOLIBC_DLL_ENTRY}")
  set_target_properties(${PRELOAD_NAME} PROPERTIES LINK_FLAGS "${pre_link_flags}")
  # case 4125: we link with msvcrt.lib for vc8 /O2's use of _alldvrm
  target_link_libraries(${PRELOAD_NAME} ${ntimp_lib} kernel32 msvcrt dynamorio)


  set_target_properties(drearlyhelp1 PROPERTIES
    COMPILE_FLAGS "-DRC_IS_EARLY1")
  # base = (release base - 64k)
  set_target_properties(drearlyhelp1 PROPERTIES
    # not bothering with map files
    LINK_FLAGS "${LD_FLAGS} /dll /NODEFAULTLIB /base:0x70FF0000 ${NOLIBC_DLL_ENTRY}")
  target_link_libraries(drearlyhelp1 drearlyhelp2)

  set_target_properties(drearlyhelp2 PROPERTIES
    COMPILE_FLAGS "-DRC_IS_EARLY2")
  # base = (helper1 base - 64k)
  set_target_properties(drearlyhelp2 PROPERTIES
    # not bothering with map files
    LINK_FLAGS "${LD_FLAGS} /dll /NODEFAULTLIB /base:0x71110000 ${NOLIBC_DLL_ENTRY}")

endif (UNIX)

# drinjectlib
add_library(drinjectlib SHARED ${INJECTOR_SRCS})
add_gen_events_deps(drinjectlib)
target_link_libraries(drinjectlib drdecode)
set_target_properties(drinjectlib PROPERTIES
  # COMPILE_DEFINITONS isn't working for me: cmake bug?
  # Set define parameters for resources.rc
  COMPILE_FLAGS "-DNOT_DYNAMORIO_CORE_PROPER -DRC_IS_DRINJECTLIB")
if (WIN32)
  if ("${CMAKE_GENERATOR}" MATCHES "Visual Studio")
    # for parallel build correctness we need a target dependence
    add_dependencies(drinjectlib ${x86_noncore_asm_tgt} ntimp_tgt)
  endif ()
  set(drinjectlib_link_flags
    # not bothering with map files
    "${LD_FLAGS} /incremental:no /subsystem:console /NODEFAULTLIB")
  # If we use ${ntimp_flags} in the link flags and thus prior to the
  # target_link_libraries() list, on VS2005 and VS2008 we end up with duplicate
  # symbols atoi and iswctype from libcmtd and ntdll.  Unlike dynamorio.dll we're
  # ok w/ libc syms so we re-order and take the static syms first.
  set_target_properties(drinjectlib PROPERTIES LINK_FLAGS "${drinjectlib_link_flags}")
  # I'm adding libcmt to attempt to make sure we have multithreaded C lib:
  target_link_libraries(drinjectlib drdecode ${WIN32_C_LIB} ${ntimp_lib}
    kernel32 advapi32 imagehlp)
  # drinject.exe needs a copy in the bin dir
  add_custom_command(TARGET drinjectlib POST_BUILD COMMAND ${CMAKE_COMMAND}
    ARGS -E copy "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/drinjectlib.dll"
    "${EXECUTABLE_OUTPUT_DIRECTORY}/" VERBATIM)
endif (WIN32)
if (APPLE)
  set_target_properties(drinjectlib PROPERTIES
    LINK_FLAGS "-install_name @rpath/libdrinjectlib.dylib")
endif (APPLE)

if (BUILD_TESTS)
  add_executable(unit_tests unit_tests.c
    ${CORE_SRCS} ${ARCH_SRCS} ${OS_SRCS})
  add_gen_events_deps(unit_tests)
  if ("${CMAKE_GENERATOR}" MATCHES "Visual Studio")
    # for parallel build correctness we need a target dependence
    add_dependencies(unit_tests ${x86_core_asm_tgt} ntimp_tgt)
  endif ()
  set_target_properties(unit_tests PROPERTIES
    COMPILE_FLAGS "-DRC_IS_TEST -DSTANDALONE_UNIT_TEST"
    RUNTIME_OUTPUT_DIRECTORY${location_suffix} "${EXECUTABLE_OUTPUT_DIRECTORY}")
  if (UNIX)
    # i#1228 make sure entry point of the elf binary pointing to _start.
    # Otherwise, the entry point will point to the start of .text section,
    # which might not be _start.
    append_property_string(TARGET unit_tests LINK_FLAGS "-Wl,--entry,_start")
    target_link_libraries(unit_tests c dl m)
    set_preferred_base_start_and_end(unit_tests)
  else (UNIX)
    # Just like drinjectlib (see above) we need libc before ntimp_lib
    target_link_libraries(unit_tests ${WIN32_C_LIB} ${ntimp_lib})
    set_target_properties(unit_tests PROPERTIES
      LINK_FLAGS "/base:${preferred_base} ${LD_FLAGS}")
  endif (UNIX)
  add_test(unit_tests "${EXECUTABLE_OUTPUT_DIRECTORY}/unit_tests")
endif (BUILD_TESTS)

###########################################################################

# Do not put core/ into the include dirs as our link.h will conflict
# with /usr/include/link.h!
include_directories(BEFORE
  ${CMAKE_CURRENT_SOURCE_DIR}/lib
  ${CMAKE_CURRENT_SOURCE_DIR}/${ARCH_DIRNAME}
  ${CMAKE_CURRENT_SOURCE_DIR}/${OSNAME}
  )

# Avoid transitive linking for imported target.
# This is a shared library, after all, and ntdll.lib is typically not available.
set_target_properties(dynamorio PROPERTIES
  # I seem to have to set these to remove the
  # IMPORTED_LINK_INTERFACE_LIBRARIES from the resulting .cmake file.
  LINK_INTERFACE_LIBRARIES ""
  LINK_INTERFACE_LIBRARIES_NOCONFIG ""
  # Setting these isn't affecting anything but doing so anyway.
  IMPORTED_LINK_INTERFACE_LIBRARIES ""
  IMPORTED_LINK_INTERFACE_LIBRARIES_NOCONFIG "")
install_exported_target(dynamorio ${INSTALL_LIB}
  # CMake doesn't set +x on shared libraries, so we have to ask for it.
  PERMISSIONS ${owner_access} OWNER_EXECUTE GROUP_READ GROUP_EXECUTE)
DR_export_target(dynamorio)
install_exported_target(drinjectlib ${INSTALL_LIB_BASE})
DR_export_target(drinjectlib)
if (UNIX)
  DR_install(TARGETS ${PRELOAD_NAME} DESTINATION ${INSTALL_LIB})
  DR_install(FILES ${COMPAT_SYMLINKS} DESTINATION ${INSTALL_LIB})
else (UNIX)
  # we put drinjectlib into bin for use by our tools
  DR_install(TARGETS drinjectlib DESTINATION ${INSTALL_BIN})
  DR_install(TARGETS ${PRELOAD_NAME} RUNTIME DESTINATION ${INSTALL_LIB_BASE})
  DR_install(TARGETS drearlyhelp1 RUNTIME DESTINATION ${INSTALL_LIB_BASE})
  DR_install(TARGETS drearlyhelp2 RUNTIME DESTINATION ${INSTALL_LIB_BASE})
endif (UNIX)

install_exported_target(drdecode ${INSTALL_LIB})

# We need separate 32-bit and 64-bit versions so we put into lib dir
DR_install(FILES ${ntimp_lib} DESTINATION ${INSTALL_LIB_BASE})

