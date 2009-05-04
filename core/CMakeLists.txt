# **********************************************************
# Copyright (c) 2009 VMware, Inc.    All rights reserved.
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

set(CORE_SRCS
  options.c
  dynamo.c
  fragment.c
  fcache.c
  link.c
  dispatch.c
  emit.c
  utils.c
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
  x86/x86.asm
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
  set(PRELOAD_SRCS linux/preload.c)
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
    win32/resources.rc
   )
  set(PRELOAD_SRCS
    win32/pre_inject.c
    win32/ntdll.c
    win32/inject_shared.c
    win32/drmarker.c
    x86/x86.asm
    x86/pre_inject_asm.asm
    win32/module_shared.c
    win32/resources.rc
    )
  set(PRELOAD_NAME drpreinject)
  set(INJECTOR_SRCS 
    win32/injector.c
    win32/inject.c
    win32/ntdll.c
    win32/inject_shared.c
    x86/x86.asm
    win32/resources.rc
    )
  # we have two drinject versions: a simpler external version that
  # relies on drdeploy having already configured the target app, and
  # a more complex version (drinjectx) that is independent of drdeploy.
  add_executable(drinject ${INJECTOR_SRCS} ${SYSLOG_SRCS} ${EVENTS_SRCS})
  add_executable(drinjectx ${INJECTOR_SRCS} ${SYSLOG_SRCS} ${EVENTS_SRCS})

  add_library(drearlyhelp1 SHARED 
    win32/early_inject_helper1.c
    win32/resources.rc
    )
  add_library(drearlyhelp2 SHARED
    win32/early_inject_helper2.c
    win32/resources.rc
    )
endif (UNIX)

# FIXME: how get cmake to analyze #includes of my asm files?
set(asm_dependencies
  ${PROJECT_SOURCE_DIR}/core/x86/asm_defines.asm
  ${PROJECT_SOURCE_DIR}/make/CMake_asm.cmake
  ${PROJECT_BINARY_DIR}/configure.h)
set_source_files_properties(x86/x86.asm x86/pre_inject_asm.asm
  PROPERTIES OBJECT_DEPENDS "${asm_dependencies}")

# FIXME: compare flags to /work/opensource/dynamorio/compilation-buffer

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
  string(REGEX REPLACE 
    "^([0-9]+\\.[0-9]+).*" "\\1" VERSION_MAJOR_MINOR "${VERSION_NUMBER}")
  set_target_properties(dynamorio PROPERTIES
    VERSION "${VERSION_MAJOR_MINOR}" SOVERSION "${VERSION_MAJOR_MINOR}")
  # Note that if we ever increase OLDEST_COMPATIBLE_VERSION in 
  # x86/instrument.c to be > 104 we need to update this var and
  # should then share a common define.
  set(COMPAT_SOVERSION 104)
  get_target_property(curver_base dynamorio LOCATION)
  get_filename_component(curver_path "${curver_base}" PATH)
  get_filename_component(curver_base "${curver_base}" NAME)
  get_target_property(curver_sover dynamorio SOVERSION)
  set(curver "${curver_base}.${curver_sover}")
  # This loop assumes still on 1.x
  # Once we go to 2.x we'll need to store somewhere the highest 1.x we released
  while (${COMPAT_SOVERSION} LESS ${VERSION_NUMBER_INTEGER})
    math(EXPR COMPAT_SUFFIX "${COMPAT_SOVERSION}-100")
    string(REGEX REPLACE "so.${VERSION_MAJOR_MINOR}"
      "so.1.${COMPAT_SUFFIX}" oldver "${curver}")
    set(COMPAT_SYMLINKS ${COMPAT_SYMLINKS} ${curver_path}/${oldver})
    add_custom_command(TARGET dynamorio POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E create_symlink ${curver} ${oldver}
      WORKING_DIRECTORY ${LIBRARY_OUTPUT_PATH}
      VERBATIM)
    math(EXPR COMPAT_SOVERSION "${COMPAT_SOVERSION}+1")
  endwhile ()

  # Note that we can't locate readelf inside CMake_readelf.cmake as that
  # would disallow pointing externally at a readelf location, so we make it
  # a proper cache variable.
  # Note that nothing depends on CMake_readelf.cmake: that's ok, we'll check
  # in the next clean build if we modify the script.
  # readelf is a binutils app just like ld
  get_filename_component(binutils_path ${CMAKE_LINKER} PATH)
  find_program(READELF_EXECUTABLE readelf HINTS "${binutils_path}" DOC "path to readelf")
  if (READELF_EXECUTABLE-NOTFOUND OR NOT EXISTS "${READELF_EXECUTABLE}")
    message("${READELF_EXECUTABLE} not found: not checking SElinux or execstack")
  else (READELF_EXECUTABLE-NOTFOUND OR NOT EXISTS "${READELF_EXECUTABLE}")
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
  endif (READELF_EXECUTABLE-NOTFOUND OR NOT EXISTS "${READELF_EXECUTABLE}")

  # we used to run size on libdynamorio.so for info purposes

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
    set(dynamorio_link_flags "${dynamorio_link_flags} /FIXED")
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
  # make sure to export dynamo_auto_start (it's now in x86.s and so
  # we can't just say __declspec(dllexport))
  set(dynamorio_link_flags 
    "${LD_FLAGS} ${dynamorio_link_flags} /dll /incremental:no")
  set(dynamorio_link_flags 
    "${dynamorio_link_flags} ${NOLIBC_DLL_ENTRY} ${FORWARD_TO_NTDLL}")
  # cmake does /out, /implib, and /pdb for us, but we do want map file
  get_target_property(drout dynamorio LOCATION)
  get_filename_component(drpath ${drout} PATH)
  get_filename_component(drname ${drout} NAME_WE)
  set(dynamorio_link_flags 
    "${dynamorio_link_flags} /map:\"${drpath}/${drname}.map\" /mapinfo:exports")
  # make sure to export dynamo_auto_start (it's now in x86.s and so
  # we can't just say __declspec(dllexport))
  set(dynamorio_link_flags 
    "${dynamorio_link_flags} /export:dynamo_auto_start /export:dynamorio_app_take_over")
  if (APP_EXPORTS)
    set(dynamorio_link_flags 
      "${dynamorio_link_flags} /export:dr_app_start /export:dr_app_take_over")
  endif (APP_EXPORTS)

  set_target_properties(dynamorio PROPERTIES LINK_FLAGS "${dynamorio_link_flags}")
  # case 4125: we link with msvcrt.lib for vc8 /O2's use of _alldvrm
  # note that _alldvrm is in ntdll.dll on xp+; but for pre-xp uses we
  # stick with the static linking of the impl in msvcrt.lib.
  target_link_libraries(dynamorio ntdll msvcrt)

  # we used to run dumpbin /headers on dynamorio.dll for info purposes


  set_target_properties(drinject PROPERTIES
    # COMPILE_DEFINITONS isn't working for me: cmake bug?
    # Set define parameters for resources.rc
    COMPILE_FLAGS "-DNOT_DYNAMORIO_CORE_PROPER -DRC_IS_DRINJECT -DEXTERNAL_INJECTOR")
  set_target_properties(drinjectx PROPERTIES
    COMPILE_FLAGS "-DNOT_DYNAMORIO_CORE_PROPER -DRC_IS_DRINJECT")
  # I'm adding libcmt to attempt to make sure we have multithreaded C lib:
  if (DEBUG)
    set(WIN32_C_LIB libcmtd)
  else (DEBUG)
    set(WIN32_C_LIB libcmt)
  endif (DEBUG)
  set(drinject_link_flags
    # not bothering with map files
    "${LD_FLAGS} /incremental:no /subsystem:console /NODEFAULTLIB")
  set_target_properties(drinject PROPERTIES LINK_FLAGS "${drinject_link_flags}")
  set_target_properties(drinjectx PROPERTIES LINK_FLAGS "${drinject_link_flags}")
  target_link_libraries(drinject ${WIN32_C_LIB} ntdll kernel32 advapi32 imagehlp)
  target_link_libraries(drinjectx ${WIN32_C_LIB} ntdll kernel32 advapi32 imagehlp)


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
if (VMKERNEL)
  # We need these only for the OS dependent files,
  # yet there are no filename conflicts so always add to include paths
  set(VMKERNEL_TREE "$ENV{VMTREE}") # path to bora
  include_directories(AFTER
    ${VMKERNEL_TREE}/public
    ${VMKERNEL_TREE}/lib/public
    ${VMKERNEL_TREE}/vmkernel/public
    ${VMKERNEL_TREE}/vmkernel/include/vmkapi
    ${VMKERNEL_TREE}/vmkernel/include/vmkapi/current
    ${VMKERNEL_TREE}/vmkernel/include/user
    ${VMKERNEL_TREE}/vmkernel/include/public
    ${VMKERNEL_TREE}/lib/vmkuser/include/public
    ${VMKERNEL_TREE}/vmkernel/distribute
    ${VMKERNEL_TREE}/vmcore/public
    )
endif (VMKERNEL)


install(TARGETS dynamorio
  EXPORT DynamoRIOTarget
  RUNTIME DESTINATION ${INSTALL_LIB} # .dll
  LIBRARY DESTINATION ${INSTALL_LIB} # .so
  ARCHIVE DESTINATION ${INSTALL_LIB} # .lib
  )
install(EXPORT DynamoRIOTarget DESTINATION ${INSTALL_CMAKE})
export(TARGETS dynamorio FILE ${PROJECT_BINARY_DIR}/cmake/DynamoRIOTarget.cmake)
if (UNIX)
  install(TARGETS ${PRELOAD_NAME} DESTINATION ${INSTALL_LIB})
  install(FILES ${COMPAT_SYMLINKS} DESTINATION ${INSTALL_LIB})
else (UNIX)
  install(TARGETS drinject DESTINATION ${INSTALL_BIN})
  # we install drinjectx for developer use; should we document it?
  install(TARGETS drinjectx DESTINATION ${INSTALL_BIN})
  install(TARGETS ${PRELOAD_NAME} RUNTIME DESTINATION ${INSTALL_LIB_BASE})
  install(TARGETS drearlyhelp1 RUNTIME DESTINATION ${INSTALL_LIB_BASE})
  install(TARGETS drearlyhelp2 RUNTIME DESTINATION ${INSTALL_LIB_BASE})
endif (UNIX)

