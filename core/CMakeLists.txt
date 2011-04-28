# **********************************************************
# Copyright (c) 2010-2011 Google, Inc.    All rights reserved.
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

if ("${CMAKE_GENERATOR}" MATCHES "Visual Studio")
  # our CMAKE_ASM_COMPILE_OBJECT rule only applies to makefiles so we use
  # a custom command.
  # we need two versions: one for core and one for preinject+drinject.
  # we've collapsed down to a single config type so we ignore config
  # selection here (can't support multiple until cmake 2.8.4 anyway).
  function (add_asm source suffix output_out tgt_out extradefs)
    set_source_files_properties("${CMAKE_CURRENT_BINARY_DIR}/${source}${suffix}.s"
      PROPERTIES GENERATED true)
    # we don't need ${defines} (=> list) b/c we include configure.h
    set(output "${CMAKE_CURRENT_BINARY_DIR}/${source}${suffix}.obj")
    add_custom_command(OUTPUT "${output}"
      DEPENDS "${PROJECT_SOURCE_DIR}/core/x86/asm_defines.asm"
        "${PROJECT_SOURCE_DIR}/make/CMake_asm.cmake"
        "${PROJECT_BINARY_DIR}/configure.h)"
      COMMAND "${CMAKE_C_COMPILER}" ARGS ${CMAKE_CPP_FLAGS}
        /I "${PROJECT_BINARY_DIR}" ${extradefs}
        -E ${CPP_NO_LINENUM} "${CMAKE_CURRENT_SOURCE_DIR}/${source}.asm"
        ">" "${CMAKE_CURRENT_BINARY_DIR}/${source}${suffix}.s"
      COMMAND "${CMAKE_COMMAND}" ARGS
        -Dfile=${CMAKE_CURRENT_BINARY_DIR}/${source}${suffix}.s
        -P "${PROJECT_SOURCE_DIR}/make/CMake_asm.cmake"
      COMMAND "${CMAKE_ASM_COMPILER}" ARGS ${ASM_FLAGS} /c
        /Fo "${CMAKE_CURRENT_BINARY_DIR}/${source}${suffix}.obj"
        "${CMAKE_CURRENT_BINARY_DIR}/${source}${suffix}.s"
      WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
      VERBATIM # recommended: p260
      )
    get_filename_component(namebase "${source}" NAME_WE)
    add_custom_target(generate_${namebase}${suffix} DEPENDS "${output}")
    set(${output_out} "${output}" PARENT_SCOPE)
    set(${tgt_out} generate_${namebase}${suffix} PARENT_SCOPE)
  endfunction (add_asm)
  add_asm(x86/x86 _core x86_core_asm_src x86_core_asm_tgt "")
  add_asm(x86/x86 _noncore x86_noncore_asm_src x86_noncore_asm_tgt
    -DNOT_DYNAMORIO_CORE_PROPER)
  add_asm(x86/pre_inject_asm "" preinject_asm_src preinject_asm_tgt
    -DNOT_DYNAMORIO_CORE_PROPER)
else ("${CMAKE_GENERATOR}" MATCHES "Visual Studio")
  set(x86_core_asm_src "${CMAKE_CURRENT_SOURCE_DIR}/x86/x86.asm")
  set(x86_noncore_asm_src "${CMAKE_CURRENT_SOURCE_DIR}/x86/x86.asm")
  set(preinject_asm_src "${CMAKE_CURRENT_SOURCE_DIR}/x86/pre_inject_asm.asm")
  # XXX: how get cmake to analyze #includes of my asm files?
  set(asm_dependencies
    ${PROJECT_SOURCE_DIR}/core/x86/asm_defines.asm
    ${PROJECT_SOURCE_DIR}/make/CMake_asm.cmake
    ${PROJECT_BINARY_DIR}/configure.h)
  set_source_files_properties(${x86_core_asm_src} ${x86_noncore_asm_src}
    ${preinject_asm_src}
    PROPERTIES OBJECT_DEPENDS "${asm_dependencies}")
endif ("${CMAKE_GENERATOR}" MATCHES "Visual Studio")

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
  )
if (UNIX OR CLIENT_INTERFACE)
  # PR 219380: ntdll._vnsprintf doesn't support printing floats so we export ours
  set(CORE_SRCS ${CORE_SRCS} io.c)
endif (UNIX OR CLIENT_INTERFACE)

if (WIN32 AND NOT X64)
  # PR 219380: to avoid __ftol2_sse from libc
  # FIXME: There is no supported way to suppress a "command line" warning
  # used to do: 
  #   $(BUILD)/io.obj: FILTER_CC = 2>&1 | $(GREP) -v "D9035 : option 'QIfist'";
  # we just live with it for now:
  #   [ 20%] Building C object core/CMakeFiles/dynamorio.dir/io.c.obj
  #   cl : Command line warning D9035 : option 'QIfist' has been deprecated and will be removed in a future release
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

# These custom commands are per-subdir so can't just place at top level
include(../make/CMake_events.cmake)

if (UNIX)
  set(OSNAME linux)
  set(OS_SRCS
    linux/os.c
    linux/signal.c
    linux/module.c
    linux/pcprofile.c
    linux/stackdump.c
    linux/diagnost.c
    )
  if (VMKERNEL)
    set(VMKUW_DIR ${PROJECT_SOURCE_DIR}/../internal/core/linux)
    include_directories(${VMKUW_DIR})
    set(OS_SRCS ${OS_SRCS} ${VMKUW_DIR}/vmkuw.c)
  endif (VMKERNEL)
  set(PRELOAD_NAME drpreload)
  set(PRELOAD_SRCS
    ${SYSLOG_SRCS} 
    linux/preload.c
    config.c
    linux/os.c
    ${x86_noncore_asm_src}
    )
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
    win32/diagnost.c
    win32/aslr.c
    win32/loader.c
    win32/resources.rc
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
    )
  add_library(drinjectlib SHARED ${INJECTOR_SRCS} ${SYSLOG_SRCS} ${EVENTS_SRCS})
  if ("${CMAKE_GENERATOR}" MATCHES "Visual Studio")
    # for parallel build correctness we need a target dependence
    add_dependencies(drinjectlib ${x86_noncore_asm_tgt})
  endif ()

  add_library(drearlyhelp1 SHARED
    win32/early_inject_helper1.c
    win32/resources.rc
    )
  add_library(drearlyhelp2 SHARED
    win32/early_inject_helper2.c
    win32/resources.rc
    )
endif (UNIX)

add_library(dynamorio SHARED 
  ${CORE_SRCS} ${ARCH_SRCS} ${OS_SRCS} ${SYSLOG_SRCS} ${EVENTS_SRCS}
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
if ("${CMAKE_GENERATOR}" MATCHES "Visual Studio")
  # for parallel build correctness we need a target dependence
  add_dependencies(dynamorio ${x86_core_asm_tgt})
endif ()

# rather than a separate BUILD_INCLUDES option we always build
# include headers if building core (or if building docs or samples as
# they have real dependencies)
add_dependencies(dynamorio api_headers)

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
if ("${CMAKE_GENERATOR}" MATCHES "Visual Studio")
  # for parallel build correctness we need a target dependence
  add_dependencies(${PRELOAD_NAME} ${x86_noncore_asm_tgt} ${preinject_asm_tgt})
endif ()

###########################################################################
# linux build customization

if (UNIX)
  # FIXME case 69/1891: -z initfirst = initialize first at runtime (before libc)
  set_target_properties(${PRELOAD_NAME} PROPERTIES
    LINK_FLAGS "-nostartfiles")

  # -z now = do non-lazy runtime binding
  set(dynamorio_link_flags
    "-nostartfiles -Xlinker -z -Xlinker now -Xlinker -Bsymbolic")

  if (KEEP_SYMBOLS_FOR_LIBC_BACKTRACE)
    set(dynamorio_link_flags 
      "${dynamorio_link_flags} -Xlinker --export-dynamic")
  endif (KEEP_SYMBOLS_FOR_LIBC_BACKTRACE)

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

  # We always use a script for our own library bounds (PR 361594).
  # We could build this at configure time instead of build time as
  # it does not depend on the source files.
  # FIXME: this is duplicated in DynamoRIOConfig.cmake
  set(ld_script ${CMAKE_CURRENT_BINARY_DIR}/ldscript)
  set_directory_properties(PROPERTIES
    ADDITIONAL_MAKE_CLEAN_FILES "${ld_script}")
  set(dynamorio_link_flags 
    "${dynamorio_link_flags} -Xlinker ${ld_script_option} -Xlinker \"${ld_script}\"")
  add_custom_command(TARGET dynamorio
    PRE_LINK
    COMMAND ${CMAKE_COMMAND}
    # script does not inherit any vars so we must pass them all in
    # to work around i#84 be sure to put a space after -D for 1st arg at least
    ARGS -D outfile=${ld_script}
         -DCMAKE_LINKER=${CMAKE_LINKER}
         -DCMAKE_COMPILER_IS_GNUCC=${CMAKE_COMPILER_IS_GNUCC}
         -DLD_FLAGS=${LD_FLAGS}
         -Dset_preferred=${SET_PREFERRED_BASE}
         -Dpreferred_base=${preferred_base}
         -P ${CMAKE_CURRENT_SOURCE_DIR}/CMake_ldscript.cmake
    VERBATIM # recommended: p260
    )

  set_target_properties(dynamorio PROPERTIES
    LINK_FLAGS "${dynamorio_link_flags}")
  target_link_libraries(dynamorio c dl m)

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
  generate_soversions(${OLDEST_COMPATIBLE_VERSION} ${VERSION_NUMBER_INTEGER})

  # Note that we can't locate readelf inside CMake_readelf.cmake as that
  # would disallow pointing externally at a readelf location, so we make it
  # a proper cache variable.
  # Note that nothing depends on CMake_readelf.cmake: that's ok, we'll check
  # in the next clean build if we modify the script.
  # readelf is a binutils app just like ld
  get_filename_component(binutils_path ${CMAKE_LINKER} PATH)
  find_program(READELF_EXECUTABLE readelf HINTS "${binutils_path}" DOC "path to readelf")
  if (NOT READELF_EXECUTABLE)
    message("readelf not found: not checking SElinux or execstack")
  else (NOT READELF_EXECUTABLE)
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
  endif (NOT READELF_EXECUTABLE)

  # we used to run size on libdynamorio.so for info purposes

  set_target_properties(${PRELOAD_NAME} PROPERTIES
    COMPILE_FLAGS "-DNOT_DYNAMORIO_CORE_PROPER -DRC_IS_PRELOAD")

else (UNIX)
###########################################################################
# windows build customization

  set_target_properties(dynamorio PROPERTIES
    # Set define parameters for resources.rc.
    # Since it's hard to pass spaces in defines (I can get a " through using
    # \\\\\\\" but no spaces) we put strings in resources.rc and select
    # using RC_ defines.
    COMPILE_FLAGS "-DRC_IS_CORE -DINCLUDE_EVENTS")

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
  if (SET_PREFERRED_BASE)
    set(dynamorio_link_flags "${dynamorio_link_flags} /base:${preferred_base}")
  endif (SET_PREFERRED_BASE)
  if (X64)
    set(LD_FLAGS "${LD_FLAGS} /machine:X64")
  else (X64)
    set(LD_FLAGS "${LD_FLAGS} /machine:I386")
  endif (X64)
  set(dynamorio_link_flags 
    "${LD_FLAGS} ${dynamorio_link_flags} /dll /incremental:no")
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
  endif (APP_EXPORTS)
  if (CLIENT_INTERFACE)
    set(dynamorio_link_flags 
      "${dynamorio_link_flags} /export:dr_try_start")
  endif (CLIENT_INTERFACE)

  set_target_properties(dynamorio PROPERTIES LINK_FLAGS "${dynamorio_link_flags}")
  # case 4125: we link with msvcrt.lib for vc8 /O2's use of _alldvrm
  # note that _alldvrm is in ntdll.dll on xp+; but for pre-xp uses we
  # stick with the static linking of the impl in msvcrt.lib.
  target_link_libraries(dynamorio ntdll msvcrt)

  # we used to run dumpbin /headers on dynamorio.dll for info purposes


  set_target_properties(drinjectlib PROPERTIES
    # COMPILE_DEFINITONS isn't working for me: cmake bug?
    # Set define parameters for resources.rc
    COMPILE_FLAGS "-DNOT_DYNAMORIO_CORE_PROPER -DRC_IS_DRINJECTLIB")
  # I'm adding libcmt to attempt to make sure we have multithreaded C lib:
  if (DEBUG)
    set(WIN32_C_LIB libcmtd)
  else (DEBUG)
    set(WIN32_C_LIB libcmt)
  endif (DEBUG)
  set(drinjectlib_link_flags
    # not bothering with map files
    "${LD_FLAGS} /incremental:no /subsystem:console /NODEFAULTLIB")
  set_target_properties(drinjectlib PROPERTIES LINK_FLAGS "${drinjectlib_link_flags}")
  target_link_libraries(drinjectlib ${WIN32_C_LIB} ntdll kernel32 advapi32 imagehlp)
  # drinject.exe needs a copy in the bin dir
  add_custom_command(TARGET drinjectlib POST_BUILD COMMAND ${CMAKE_COMMAND}
    ARGS -E copy "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/drinjectlib.dll"
    "${EXECUTABLE_OUTPUT_DIRECTORY}/" VERBATIM)


  set_target_properties(${PRELOAD_NAME} PROPERTIES
    COMPILE_FLAGS "-DNOT_DYNAMORIO_CORE_PROPER -DRC_IS_PRELOAD")
  set(pre_link_flags
    # preinjector requires no C library for its unload-self method
    # not bothering with map files
    "${LD_FLAGS} /dll /incremental:no /base:0x14000000 /NODEFAULTLIB ${NOLIBC_DLL_ENTRY}")
  set_target_properties(${PRELOAD_NAME} PROPERTIES LINK_FLAGS "${pre_link_flags}")
  # case 4125: we link with msvcrt.lib for vc8 /O2's use of _alldvrm
  target_link_libraries(${PRELOAD_NAME} ntdll kernel32 msvcrt dynamorio)


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

###########################################################################

# Do not put core/ into the include dirs as our link.h will conflict
# with /usr/include/link.h!
include_directories(BEFORE 
  ${CMAKE_CURRENT_SOURCE_DIR}/lib
  ${CMAKE_CURRENT_SOURCE_DIR}/${ARCH_DIRNAME}
  ${CMAKE_CURRENT_SOURCE_DIR}/${OSNAME}
  ${CMAKE_CURRENT_BINARY_DIR} # for event_strings.h and events.h
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
DR_install(TARGETS dynamorio
  EXPORT ${exported_targets_name}
  RUNTIME DESTINATION ${INSTALL_LIB} # .dll
  LIBRARY DESTINATION ${INSTALL_LIB} # .so
  ARCHIVE DESTINATION ${INSTALL_LIB} # .lib
  )
export(TARGETS dynamorio FILE ${PROJECT_BINARY_DIR}/cmake/${exported_targets_name}.cmake)
if (UNIX)
  DR_install(TARGETS ${PRELOAD_NAME} DESTINATION ${INSTALL_LIB})
  DR_install(FILES ${COMPAT_SYMLINKS} DESTINATION ${INSTALL_LIB})
else (UNIX)
  DR_install(TARGETS drinjectlib EXPORT ${exported_targets_name}
    DESTINATION ${INSTALL_LIB_BASE})
  export(TARGETS drinjectlib FILE
    ${PROJECT_BINARY_DIR}/cmake/${exported_targets_name}.cmake APPEND)
  # we put drinjectlib into bin for use by our tools
  DR_install(TARGETS drinjectlib DESTINATION ${INSTALL_BIN})
  DR_install(TARGETS ${PRELOAD_NAME} RUNTIME DESTINATION ${INSTALL_LIB_BASE})
  DR_install(TARGETS drearlyhelp1 RUNTIME DESTINATION ${INSTALL_LIB_BASE})
  DR_install(TARGETS drearlyhelp2 RUNTIME DESTINATION ${INSTALL_LIB_BASE})
endif (UNIX)

