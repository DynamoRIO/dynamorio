# **********************************************************
# Copyright (c) 2010-2018 Google, Inc.    All rights reserved.
# Copyright (c) 2009-2010 VMware, Inc.    All rights reserved.
# Copyright (c) 2016      ARM Limited.    All rights reserved.
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

# Plan:
# * LONG vs SHORT is configure-time
# * We build all tests at general build time: in future can try
#   ctest --build-and-test
# * "make test" runs the suite of tests for each of a set of runtime options
#   for the current build.  If not running a test suite of multiple builds
#   it runs a full set of tests; otherwise it eliminates some overlapping
#   tests on release or external builds that were run on debug internal.
# * The set of runs is in a list form with qualifiers very similar to the
#   list in suite/runregression, making it easy to change what tests are run
#   and for which builds by each option set.
# * Can set additional options at configure time via TEST_OPTIONS:
#   these are added to every options run.
# * We'll use ctest -S to build each target build config and run tests for each,
#   and optionally submit to dashboard.  Use w/o submit as "make runregression".

# We use target_include_directories() which was added in 2.8.11
cmake_minimum_required(VERSION 2.8.11)

include(../../make/policies.cmake NO_POLICY_SCOPE)

option(TEST_LONG "run long set of tests")
# if TEST_SUITE is off, we disable some flaky tests in order to have all
# tests succeed (i#389).  TEST_SUITE is now a top-level option.
set(TEST_SECONDS "90" CACHE STRING "Test time limit in seconds")
set(TEST_OPTIONS "" CACHE STRING "Extra DynamoRIO options when running tests")
set(TEST_32BIT_PATH "" CACHE STRING "32-bit test bin dir for cross-arch tests")

# FIXME: i#120: .runall support
# FIXME: i#65: add command to run natively
# FIXME: i#111: get .runall tests to run in parallel
#   and use latest cmake for "ctest -j" (and "ctest -W 60")

###########################################################################
# RUNTIME OPTIONS TO TEST

# We embed the set of runs for each test in here as separate tests.
# I considered having this CMakeLists.txt only do one runtime option
# setting, and externally invoke repeatedly, perhaps via an add_test()
# that starts w/ "xx" and runs "ctest -E ^xx" (and could use further
# regexps to select subsets of tests for certain options), but in
# cmake script to set DYNAMORIO_OPTIONS env var: but the sub-ctest's
# results aren't reported at the top level.  Better to have all
# the tests be peers.

if (UNIX)
  set(osname "linux")
else (UNIX)
  set(osname "win32")
endif (UNIX)

# Syntax:
#   [SHORT::][DEBUG::][WIN::|LIN::][ONLY::<regex>::]<DR runtime options>"
# SHORT = perform run for NOT TEST_LONG
# DEBUG = debug-build-only
# WIN = Windows-only
# LIN = Linux-only
# ONLY = only run tests that match regex

# N.B.: if short-suite tests are added to other than the debug-internal-{32,64}
# builds, update runsuite.cmake to build the tests for those builds!

# FIXME i#1807: below, we convert all long suite test subsets to ^common

set(vmap_run_list
  # our main configuration
  "SHORT::-code_api"
  # sanity check: run single app to make sure these options aren't totally broken
  # i#1575: ARM doesn't yet support -coarse_units
  "SHORT::X86::ONLY::client.events$::-code_api -opt_memory"
  # i#1551: ARM doesn't yet support indcall2direct
  "SHORT::X86::ONLY::client.events$::-code_api -opt_speed"
  # i#1884: ARM doesn't yet support thread_private
  "SHORT::X86::ONLY::client.events$::-code_api -thread_private"
  "SHORT::ONLY::client.events$::-code_api -disable_traces"
  "SHORT::X86::ONLY::client.events$::-code_api -thread_private -disable_traces"
  "SHORT::X86::LIN::ONLY::client.events$::-code_api -no_early_inject" # only early on ARM
  # maybe this should be SHORT as -coarse_units will eventually be the default?
  "X86::-code_api -opt_memory"       # i#1575: ARM -coarse_units NYI
  "X86::-code_api -opt_speed"        # i#1551: ARM indcall2direct NYI
  "X86::-code_api -thread_private"   # i#1884: ARM thread_private NYI
  "-code_api -disable_traces"
  "X86::-code_api -thread_private -disable_traces" # i#1884: ARM thread_private NYI
  "-prof_pcs"
  "X86::-prof_pcs -thread_private"                 # i#1884: ARM thread_private NYI
  "X86::LIN::ONLY::^common::-code_api -no_early_inject" # ARM only supports early
  "DEBUG::ONLY::^common::-code_api -loglevel 1"
  "ONLY::^common::-code_api -stack_size 128k"
  "WIN::ONLY::^(runall|client)::-enable_full_api"
  "DEBUG::WIN::ONLY::^(common|client)::-code_api -stack_size 128K -loglevel 1 -no_hide"
  "DEBUG::LIN::ONLY::^(common|client)::-code_api -stack_size 128K -loglevel 1"
  "ONLY::^common::"
  "X86::LIN::ONLY::^${osname}::-code_api -sysenter_is_int80"
  "X86::LIN::ONLY::^${osname}.sig::-max_pending_signals 1"

  # cover -tracedump_* options, just a couple combinations
  # XXX: how up time limit?  Used to have TEST_MINS=5 for tracedump runs.
  "ONLY::^common::-code_api -tracedump_text -tracedump_origins"
  "ONLY::^common::-code_api -tracedump_text -tracedump_origins -syntax_intel"
  "X86::ONLY::^common::-code_api -thread_private -tracedump_binary" # i#1884: ARM NYI
  "ONLY::^common::-code_api -bbdump_tags"

  # make sure we at least sometimes exercise non-default -checklevel
  "DEBUG::ONLY::^common::-checklevel 4"

  # pcache tests: per-user so each app will merge w/ previous, plus merge
  # w/ at-unload persists from earlier -desktop run
  "WIN::ONLY::^runall::-desktop -coarse_freeze_at_exit"
  # run again so each must use the merged pcaches
  # FIXME: order is alpha: need unique name sorted next
  "WIN::ONLY::^runall::-desktop -coarse_freeze_at_exit"

  # limit on shared cache size
  "ONLY::^(runall|${osname})::-finite_shared_bb_cache -cache_shared_bb_regen 80"
  "ONLY::^(runall|${osname})::-finite_shared_trace_cache -cache_shared_trace_regen 80"
  )

# This is no longer an actively supported config so we have few tests.
set(vmsafe_run_list
  # keep lightweight modes alive
  # i#24: -thin_client not working: once it works, re-enable
  #"WIN::ONLY::^runall::-thin_client"
  "WIN::ONLY::^runall::-probe_api -hotp_only"
  # We no longer actively support this build so we run only a few tests.
  "ONLY::^security::-code_api -probe_api"
  "ONLY::^security ^runall runinterface::-security_api"
  "ONLY::^security::-security_api -code_api -probe_api"
  )

# Xref the very long set of runs we used to test in
# suite/old-runregression-runs-2-6-08.
# This is no longer an actively supported config so we have few tests.
set(vps_run_list
  "ONLY::^(common|${osname}|security|runall)::-security"
  "WIN::ONLY::^runall::-security -client"
  "WIN::ONLY::^runall::-security -low" # not a default config
  # Test with -use_moduledb relaxations, (is on by default, but tests Makefile
  # adds -staged which turns it off, we re-enable by turning the option back on).
  # The tests Makefile adds different version resource information to different
  # tests in runall/ and security-{common,win32}/ to fully exercise this.
  "WIN::ONLY::^runall::-security -use_moduledb"
  )
# if building dedicated vps, want short suite of all tests
set(vps_short_list
  "SHORT::-security"
  )

if (CALLPROF AND TEST_SUITE)
  set(run_list "ONLY::^common::")
elseif (VMAP)
  if (TEST_SUITE)
    # We don't redo all tests for internal vs external, but we do for debug vs release
    if (DEBUG OR NOT INTERNAL)
      set(run_list ${vmap_run_list})
    else ()
      set(run_list "ONLY::^client::-code_api")
    endif ()
  else (TEST_SUITE)
    set(run_list ${vmap_run_list})
  endif (TEST_SUITE)
elseif (VMSAFE)
  if (TEST_SUITE)
    if (DEBUG AND INTERNAL)
      set(run_list ${vmsafe_run_list})
    else (DEBUG AND INTERNAL)
      set(run_list
        "ONLY::^client::-code_api -probe_api"
        "ONLY::^(client|security)::-security_api"
        "ONLY::^runall::-probe_api -hotp_only"
        )
    endif (DEBUG AND INTERNAL)
  else (TEST_SUITE)
    set(run_list ${vmsafe_run_list})
  endif (TEST_SUITE)
elseif (VPS)
  if (TEST_SUITE)
    if (DEBUG AND INTERNAL)
      set(run_list ${vps_run_list})
    else (DEBUG AND INTERNAL)
      set(run_list
        "ONLY::^common::"
        "ONLY::^security::-security -detect_mode -report_max 0 -no_diagnostics"
        "WIN::ONLY::^(common|${osname}|security|runall)::-security -liveshields"
        "WIN::ONLY::^(runall|security)::-security -client"
        )
    endif (DEBUG AND INTERNAL)
  else (TEST_SUITE)
    set(run_list ${vps_short_list})
  endif (TEST_SUITE)
else (CALLPROF AND TEST_SUITE)
  # no tests
  set(run_list "")
endif (CALLPROF AND TEST_SUITE)

###########################################################################
# BUILDING

if (WIN32)
  find_program(BIND_EXECUTABLE bind.exe DOC "path to bind.exe")
  if (BIND_EXECUTABLE)
    message(STATUS "Found bind.exe: ${BIND_EXECUTABLE}")
  else (BIND_EXECUTABLE)
    # else we'll use editbin via link.exe
  endif (BIND_EXECUTABLE)
  mark_as_advanced(BIND_EXECUTABLE)

  # if cygwin or mingw gcc is available, we do some extra tests
  find_program(GCC gcc.exe DOC "path to gcc.exe")
  if (NOT GCC)
    message(STATUS "gcc not found: some Windows tests will be disabled")
  else (NOT GCC)
    # check gcc target architecture
    execute_process(COMMAND ${GCC} -dumpmachine
      RESULT_VARIABLE cmd_result
      ERROR_VARIABLE cmd_err
      OUTPUT_VARIABLE cmd_out)
    if (cmd_out MATCHES "x86_64")
      set(GCC_IS64 ON)
    endif ()
    # check cygwin gcc
    if (cmd_out MATCHES "cygwin" OR
        # cmake can't launch gcc.exe as it's a symlink to /etc/alternatives/gcc --
        # we assume this error always means that it's that symlink:
        cmd_result MATCHES "Access is denied")
      set(GCC_IS_CYGWIN ON)
    endif()
  endif (NOT GCC)
else (WIN32)
  # i#215: if SELinux is enabled we'll use chcon to mark tests w/ text relocations
  # to avoid violating SELinux policies.  On Ubuntu, chcon is part of coreutils
  # so it will be there even if SELinux isn't installed or enabled.  If SELinux
  # is not enabled, chcon will give 'Operation not supported'.
  # To find out whether SELinux is enabled, we find and run the selinuxenabled
  # command and look for a zero return code.
  find_program(SEUNIXENABLED selinuxenabled DOC "path to selinuxenabled")
  if (SEUNIXENABLED)
    execute_process(COMMAND ${SEUNIXENABLED}
      RESULT_VARIABLE cmd_result)
    if (NOT cmd_result)
      find_program(CHCON chcon DOC "path to chcon")
      if (CHCON)
        message(STATUS "Detected SELnux: will chcon binaries with text relocations.")
        set(SEUNIXFOUND "yes")
      else (CHCON)
        message(WARNING "SELinux enabled, but chcon command not found.")
      endif (CHCON)
      mark_as_advanced(CHCON)
    endif (NOT cmd_result)
  endif (SEUNIXENABLED)
endif (WIN32)

# put libs in same dir for tests that need to load them
set(MAIN_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
set(MAIN_LIBRARY_OUTPUT_DIRECTORY "${DR_LIBRARY_OUTPUT_DIRECTORY}")
string(REGEX REPLACE "/lib([36])" "/ext/lib\\1"
  EXT_LIBRARY_OUTPUT_DIRECTORY "${MAIN_LIBRARY_OUTPUT_DIRECTORY}")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/bin")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/bin")
set_per_config_ouput_to_match_single_config()

include_directories(${CMAKE_CURRENT_SOURCE_DIR}) # tools.h
include_directories(${PROJECT_SOURCE_DIR}/core/arch) # asm_defines.asm

if (ANDROID AND NOT ADB)
  find_program(ADB adb)
  if (NOT adb)
    message(STATUS "Failed to find adb: tests expected to fail to run")
  endif ()
endif ()

# FIXME i#98: improve code so can set Wall/W4
if (UNIX)
  string(REGEX REPLACE "-Wall" "" CMAKE_C_FLAGS "${CMAKE_C_FLAGS}")
  # some tests rely on specific "nop;nop" patterns that optimization ruins
  # we should probably move the -O from top level into core/CMakeLists.txt
  string(REGEX REPLACE "-O[0-9]? " " " CMAKE_C_FLAGS "${CMAKE_C_FLAGS}")
else (UNIX)
  # W2 is default (we're using W3).  We should also replace
  # all references to unsafe functions (e.g., fopen) and
  # remove /wd4996
  string(REGEX REPLACE "/W4" "/W3" CMAKE_C_FLAGS "${CMAKE_C_FLAGS}")
  # Versions without debug, for over ssh
  string(REGEX REPLACE "/Zi" "" ORIG_C_FLAGS_NODBG "${CMAKE_C_FLAGS}")
  if (RUNNING_OVER_SSH)
    set(CMAKE_C_FLAGS "${ORIG_C_FLAGS_NODBG}")
    string(REGEX REPLACE "/debug" "" CMAKE_SHARED_LINKER_FLAGS
      "${CMAKE_SHARED_LINKER_FLAGS}")
  endif (RUNNING_OVER_SSH)
endif (UNIX)

# To avoid having separate subdirs with separate CMakeLists.txt files,
# we use DynamoRIOConfig.cmake here.  It makes global changes, though.
# We live with those changes on all our tests even though we don't
# need them everywhere for:
# - include_directories
# - link_directories
# We isolate these:
# - CMAKE_C_FLAGS
# We have to be careful to append its changes to target properties
# with changes we want to make, as well.

if (UNIX)
  if (X86)
    if (X64)
      set(ARCH_CFLAGS "-m64")
    else (X64)
      set(ARCH_CFLAGS "-m32")
    endif (X64)
  endif ()
  set(ARCH_LDFLAGS "${ARCH_CFLAGS}")
  set(ARCH_DEBUG "${ARCH_CFLAGS}") # somehow not being propagated so we set here
else (UNIX)
  set(ARCH_CFLAGS "")
  # we don't add /debug here (=> SHARED_LINKER flags) since can't remove
  set(ARCH_LDFLAGS "")
  if (RUNNING_OVER_SSH)
    set(ARCH_DEBUG "")
  else (RUNNING_OVER_SSH)
    # ensure we get proper debugging (i#567)
    set(ARCH_DEBUG "/debug")
  endif (RUNNING_OVER_SSH)
endif (UNIX)

if (NOT DEFINED DynamoRIO_DIR)
  set(DynamoRIO_DIR "${PROJECT_SOURCE_DIR}/../cmake" CACHE PATH
    "DynamoRIO installation's cmake directory")
endif (NOT DEFINED DynamoRIO_DIR)
find_package(DynamoRIO)
if (NOT DynamoRIO_FOUND)
  message(FATAL_ERROR "DynamoRIO package required to build")
endif(NOT DynamoRIO_FOUND)

# Force global changes now, and set ORIG_CMAKE_C_FLAGS
configure_DynamoRIO_global(OFF ON)

# configure_DynamoRIO_global() cleared the absolute flags for us.
# We now use PROPERTIES to set for each test.
# We do want the bitwidth flags for gcc on everything though:
set(CMAKE_C_FLAGS "${ARCH_CFLAGS}")
# DynamoRIOConfig.cmake no longer sets global CMAKE_SHARED_LINKER_FLAGS
# so we no longer need to clear this and we can just add arch:
set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} ${ARCH_LDFLAGS}")

# Must be AFTER build/include now that dr_api.h is configured.
# We need this for Windows resources.rc to include globals_shared.h, and
# for tools.c to include dr_helper.h.
include_directories(AFTER ${PROJECT_SOURCE_DIR}/core/lib) # dr_helper.h

# Since we're including configure.h we do not want any default defines.
# Note that the new DynamoRIOConfig.cmake doesn't set these so we can
# probably remove this line.  We remove from target-specific flags
# from configure_DynamoRIO_* in tobuild_api().
remove_definitions(-DX64 -DLINUX -DWINDOWS -DMACOS -DCLANG -DANDROID)
# If people want perf stress tests they'll have to manually build to avoid
# the NIGHTLY_REGRESSION define
set(test_defs "-DNIGHTLY_REGRESSION -DNOT_DYNAMORIO_CORE")
set(platform_defs "${test_defs}")
if (X64)
  set(platform_defs "${platform_defs} -DX64")
endif (X64)
if (UNIX)
  if (APPLE)
    set(platform_defs "${platform_defs} -DMACOS -DUNIX")
  else (APPLE)
    set(platform_defs "${platform_defs} -DLINUX -DUNIX")
  endif (APPLE)
else (UNIX)
  set(platform_defs "${platform_defs} -DWINDOWS")
endif (UNIX)

##################################################

set(asm_deps
  "${PROJECT_SOURCE_DIR}/core/arch/asm_defines.asm"
  "${PROJECT_BINARY_DIR}/configure.h")
set(asm_defs
  -I "${CMAKE_CURRENT_SOURCE_DIR}" # tools.h
  -I "${CMAKE_CURRENT_SOURCE_DIR}/client-interface" # for drreg-test-shared.h
  -I "${CMAKE_CURRENT_SOURCE_DIR}/api" # for detach_state_shared.h
  -I "${PROJECT_SOURCE_DIR}/core/arch") # asm_defines.asm

# some tests need to link with tools.c and its embedded asm code
set(tools_c ${CMAKE_CURRENT_SOURCE_DIR}/tools.c)
set_source_files_properties(${tools_c} PROPERTIES
  COMPILE_FLAGS "${ORIG_CMAKE_C_FLAGS}")
add_split_asm_target("${tools_c}" tools_asm generate_tools_asm "_asm"
  "${asm_defs}" "${asm_deps}")

add_library(tools STATIC ${tools_c} ${tools_asm})
if (UNIX)
  # We link tools into shared libraries, so it has to be PIC.
  append_property_string(SOURCE ${tools_c} COMPILE_FLAGS "-fPIC")
endif ()
if ("${CMAKE_GENERATOR}" MATCHES "Visual Studio")
  # for correct parallel builds we need a target
  add_dependencies(tools ${generate_tools_asm})
endif ()
target_link_libraries(tools drhelper)

if (WIN32)
  set_source_files_properties(${PROJECT_SOURCE_DIR}/core/win32/resources.rc
    PROPERTIES COMPILE_FLAGS "-DRC_IS_TEST")
endif (WIN32)

function (set_cflags source)
  if ("${source}" MATCHES "^security-win32/except-execution.c")
    # PR 229292: we want over-ssh and local builds to match the template so
    # we always build w/o pdbs.  We put this check here as it's simpler
    # than passing args all the way through, and needing extra explicit
    # sets for PARENT_SCOPE due to new helper routine layers.
    set(cflags "${ORIG_C_FLAGS_NODBG}")
  else ()
    set(cflags "${ORIG_CMAKE_C_FLAGS}")
  endif ()
  if ("${source}" MATCHES ".cpp$")
    # Our C files need -std=gnu99, but that's an invalid flag for C++.
    string(REGEX REPLACE "-std=gnu99" "" cflags "${cflags}")
    if (WIN32)
      set(cflags "${cflags} /EHsc")
    endif (WIN32)
  endif ()
  if ("${source}" MATCHES ".dll.(c|cpp)$")
    # do nothing
  else ("${source}" MATCHES ".dll.(c|cpp)$")
    # We only do it for the executable, not the dll
    # Xref i#230, removes "-fvisibility=internal" option.
    string(REGEX REPLACE "-fvisibility=internal" "" cflags "${cflags}")
    # Xref i#331, removes "/O2" to avoid optimization on the executable
    if (WIN32)
      string(REGEX REPLACE "/O2" "" cflags "${cflags}")
    endif (WIN32)
  endif ("${source}" MATCHES ".dll.(c|cpp)$")
  # We can't set the target properties COMPILE_FLAGS as that will
  # pass -g3 to cpp which ends up not expanding macros!
  # So we set properties on source files only.
  get_filename_component(abs ${source} ABSOLUTE)
  file(READ ${abs} srccode)
  set(srccode "${srccode}" PARENT_SCOPE)
  if (WIN32 AND "${srccode}" MATCHES "condvar\\.h")
    # Ensure the ConditionVariable routines are pulled in when using >VS2010.
    # Because dr_api.h includes windows.h and must be included before tools.h we do
    # this via command line flags.
    set(cflags "${cflags} -D_WIN32_WINNT=0x0600")
  endif ()
  # We support a test either getting defines from configure.h or
  # needing them passed in on cmd line.
  if ("${srccode}" MATCHES "tools\\.h" OR
      "${srccode}" MATCHES "configure\\.h")
    # getting defines from configure.h
    set_source_files_properties(${source} PROPERTIES
      COMPILE_FLAGS "${cflags} ${test_defs}")
  else ()
    set_source_files_properties(${source} PROPERTIES
      COMPILE_FLAGS "${platform_defs} ${cflags}")
  endif ()
endfunction (set_cflags)

function(append_link_flags target newflags)
  get_target_property(cur_ldflags ${target} LINK_FLAGS)
  # cmake should add an APPEND option
  set_target_properties(${target} PROPERTIES
    LINK_FLAGS "${cur_ldflags} ${newflags}")
endfunction(append_link_flags)

function(add_exe test source)
  get_filename_component(srcbase ${source} NAME_WE)
  get_filename_component(srcpath ${source} PATH)

  if (WINDOWS)
    if ("${source}" MATCHES "^security-common")
      # we add non-MS non-DR version info to some tests to better test -use_moduledb
      set(rc_srcs ${CMAKE_CURRENT_SOURCE_DIR}/oresources.rc)
    elseif ("${source}" MATCHES "^runall")
      # we add MS-like version info to some tests to better test -use_moduledb
      set(rc_srcs ${CMAKE_CURRENT_SOURCE_DIR}/mresources.rc)
    else ("${source}" MATCHES "^security-common")
      # we add dr resource version info to only some of our security tests to test both
      # sides of the -use_moduledb functionality, and ensure the matching works
      set(rc_srcs ${PROJECT_SOURCE_DIR}/core/win32/resources.rc)
    endif ("${source}" MATCHES "^security-common")
  else (WINDOWS)
    set(rc_srcs "")
  endif (WINDOWS)

  set(test_srcs ${source} ${rc_srcs})
  set_cflags(${source}) # sets srccode var

  # Some files use asm code, which must be separate for x64, but it's much
  # more convenient to have it in the same source file and auto-split.
  if ("${srccode}" MATCHES "ifndef ASM_CODE_ONLY")
    # we rely on the asm rule doing preprocessing for us, so we just make
    # a copy and set the ASM_CODE_ONLY define
    add_split_asm_target("${CMAKE_CURRENT_SOURCE_DIR}/${source}" asm_source gen_asm_tgt
      "_asm" "${asm_defs}" "${asm_deps}")
    set(test_srcs ${test_srcs} ${asm_source})
  endif ("${srccode}" MATCHES "ifndef ASM_CODE_ONLY")

  if ("${srccode}" MATCHES "include \"dr_annotations.h\"")
    use_DynamoRIO_annotations(${test} test_srcs)
  endif ("${srccode}" MATCHES "include \"dr_annotations.h\"")
  if ("${srccode}" MATCHES "include \"test.*annotation.*.h\"")
    use_DynamoRIO_test_annotations(${test} test_srcs)
  endif ("${srccode}" MATCHES "include \"test.*annotation.*.h\"")

  add_executable(${test} ${test_srcs})
  if (DEFINED ${test}_outname)
    set_target_properties(${test} PROPERTIES OUTPUT_NAME "${${test}_outname}")
  endif ()
  copy_target_to_device(${test})

  set_target_properties(${test} PROPERTIES
    LINK_FLAGS "${ARCH_DEBUG}"
    # we don't want the default rpath of <builddir>/lib as it makes
    # it hard to use other DR builds w/ these apps
    SKIP_BUILD_RPATH ON)
  if (UNIX)
    # This is kind of a hack: perhaps we should add a flag for whether to use libs.
    if (NOT ${source} MATCHES "\\.asm$" AND NOT ${test} MATCHES "static"
        AND NOT ${test} MATCHES "drdecode")
      target_link_libraries(${test} ${libmath} ${libdl})
    endif ()
    if (NOT ANDROID) # pthreads is inside Bionic on Android
      if ("${test}" MATCHES "^pthread")
        target_link_libraries(${test} ${libpthread})
      endif ()
    endif ()
  elseif (WIN32)
    append_link_flags(${test} "${ARCH_LDFLAGS}")
  endif ()
  if ("${srccode}" MATCHES "include \"tools\\.h\"")
    target_link_libraries(${test} tools)
  endif ()
  if ("${srccode}" MATCHES "ifndef ASM_CODE_ONLY" AND
      "${CMAKE_GENERATOR}" MATCHES "Visual Studio")
    add_dependencies(${test} ${gen_asm_tgt})
  endif ()
endfunction(add_exe)

# normal app
function(tobuild test source)
  add_exe(${test} ${source})
  set(testlist_normal ${testlist_normal} ${test} PARENT_SCOPE)
  set(${test}_source ${source} PARENT_SCOPE)
endfunction(tobuild)

# normal app w/ options
function(tobuild_ops test source dr_ops exe_ops)
  set(srcs ${source})
  get_filename_component(abs ${source} ABSOLUTE)
  file(READ ${abs} srccode)
  if ("${srccode}" MATCHES "include \"dr_annotations.h\"")
    use_DynamoRIO_annotations(${test} srcs)
  endif ()
  add_exe(${test} ${srcs})
  set(testlist_normal ${testlist_normal} ${test} PARENT_SCOPE)
  set(${test}_source ${source} PARENT_SCOPE)
  set(${test}_dr_ops ${dr_ops} PARENT_SCOPE)
  set(${test}_exe_ops ${exe_ops} PARENT_SCOPE)
endfunction(tobuild_ops)

# normal app that has a dll and an executable
function(tobuild_dll test source dr_ops)
  string(REGEX REPLACE "\\.(c|cpp)$" ".dll.c" dll_source "${source}")

  add_exe(${test} ${source})
  # On Linux, we need the rpath to load ${test}.dll at initialization.
  set_target_properties(${test} PROPERTIES SKIP_BUILD_RPATH OFF)

  set(lib_srcs ${dll_source})
  set_cflags(${dll_source}) # sets srccode var
  add_library(${test}.dll SHARED ${lib_srcs})
  set_target_properties(${test}.dll PROPERTIES LINK_FLAGS "${ARCH_DEBUG}")
  copy_target_to_device(${test}.dll)
  target_link_libraries(${test} ${test}.dll)
  if ("${srccode}" MATCHES "include \"tools\\.h\"")
    target_link_libraries(${test}.dll tools)
  endif ()

  set(testlist_normal ${testlist_normal} ${test} PARENT_SCOPE)
  set(${test}_source ${source} PARENT_SCOPE)
  set(${test}_dr_ops ${dr_ops} PARENT_SCOPE)
endfunction(tobuild_dll)

function(get_client_path client_path_out realclient realexe)
  get_target_path_for_execution(client_path ${realclient})
  set(${client_path_out} "${client_path}" PARENT_SCOPE)
endfunction(get_client_path)

# A shared app for use with multiple ci tests
set(ci_shared_app simple_app)
set(ci_shared_app_src client-interface/${ci_shared_app}.c)
add_exe(${ci_shared_app} ${ci_shared_app_src})

# A client interface app with client.
# If ${source} does not exist, ${ci_shared_app} is used as the target app.
# Note that we can't easily have a routine that takes no ops
# and calls this one w/ "" "" "" b/c the PARENT_SCOPE won't make
# it all the way back then: would need to chain.
function(tobuild_ci test source client_ops dr_ops exe_ops)
  string(REGEX REPLACE "\\.(c|cpp)$" ".dll.\\1" client_source "${source}")
  string(REGEX REPLACE "\\.runall$" ".dll.c" client_source "${client_source}")

  if (NOT "${source}" MATCHES "\\.runall$")
    # Many client tests don't care about the app that's run.  To save time
    # and space with a ton of duplicated hello,world apps, we support using
    # a single one, "simple_app".
    if (EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${source})
      add_exe(${test} ${source})
      # Make sure to avoid using the modified flags which can break msbuild (i#1748).
      # On Linux this results in some build issues which I did not want to track
      # down, so we do this only on Windows.
      if (WIN32)
        get_source_file_property(src_lang ${source} LANGUAGE)
        if (src_lang MATCHES CXX)
          set(tst_flags "${ORIG_CMAKE_CXX_FLAGS}")
        else ()
          set(tst_flags "${ORIG_CMAKE_C_FLAGS}")
        endif ()
        set_target_properties(${test} PROPERTIES COMPILE_FLAGS "${tst_flags}")
      endif ()
    else ()
      set(${test}_realtest ${ci_shared_app} PARENT_SCOPE)
    endif ()
  endif ()

  add_library(${test}.dll SHARED ${client_source})
  if (NOT "${source}" MATCHES "cpp\\.cpp")
    # to avoid changing all the REG_ constants we ask for compatibility
    set(DynamoRIO_REG_COMPATIBILITY ON)
  endif (NOT "${source}" MATCHES "cpp\\.cpp")
  configure_DynamoRIO_client(${test}.dll)
  append_property_string(TARGET ${test}.dll LINK_FLAGS "${ARCH_DEBUG}")
  add_dependencies(${test}.dll api_headers)
  copy_target_to_device(${test}.dll)
  get_client_path(client_path ${test}.dll ${test})

  set(testlist_ci ${testlist_ci} ${test} PARENT_SCOPE)
  set(${test}_source ${source} PARENT_SCOPE)
  set(${test}_client_ops ${client_ops} PARENT_SCOPE)
  set(${test}_dr_ops ${dr_ops} PARENT_SCOPE)
  set(${test}_exe_ops ${exe_ops} PARENT_SCOPE)
  set(${test}_client_path ${client_path} PARENT_SCOPE)
endfunction(tobuild_ci)

function(configure_app_api_build_flags test decoder use_static_DR)
  # save property since configure_DynamoRIO_standalone will clobber it
  get_target_property(pre_lflags ${test} LINK_FLAGS)
  # disable the default rpath for standalone apps
  set(DynamoRIO_RPATH OFF)
  # to avoid changing all the REG_ constants we ask for compatibility
  set(DynamoRIO_REG_COMPATIBILITY ON)
  if (decoder)
    configure_DynamoRIO_decoder(${test})
    set(extra_flags "-DSTANDALONE_DECODER")
  elseif (use_static_DR)
    configure_DynamoRIO_static(${test})
    set(extra_flags "-DSTATIC_LIBRARY")
  else ()
    configure_DynamoRIO_standalone(${test})
    set(extra_flags "-DSTANDALONE")
  endif ()
  # append to properties set by configure_DynamoRIO_standalone
  get_target_property(prop_cflags ${test} COMPILE_FLAGS)
  if (NOT prop_cflags)
    set(prop_cflags "")
  endif ()
  set(prop_cflags "${prop_cflags} ${extra_flags}")
  set(prop_cflags "${prop_cflags} -DUSE_DYNAMO")
  # since we're including configure.h we do not want any default defines
  string(REGEX REPLACE "-DX64" "" prop_cflags "${prop_cflags}")
  string(REGEX REPLACE "-DLINUX" "" prop_cflags "${prop_cflags}")
  string(REGEX REPLACE "-DWINDOWS" "" prop_cflags "${prop_cflags}")
  string(REGEX REPLACE "-DMACOS" "" prop_cflags "${prop_cflags}")
  string(REGEX REPLACE "-DCLANG" "" prop_cflags "${prop_cflags}")
  string(REGEX REPLACE "-DANDROID" "" prop_cflags "${prop_cflags}")
  string(REGEX REPLACE "-DDR_APP_EXPORTS" "" prop_cflags "${prop_cflags}")
  # FIXME: apparently moving cflags to a target property makes it show up
  # in the cpp run for asm targets, where it doesn't if in global var.
  # This causes compilation failure.  Removing -g3 manually as a workaround.
  string(REGEX REPLACE "-g3" "" prop_cflags "${prop_cflags}")
  get_target_property(prop_lflags ${test} LINK_FLAGS)
  set_target_properties(${test} PROPERTIES
    COMPILE_FLAGS "${prop_cflags}"
    LINK_FLAGS "${pre_lflags} ${prop_lflags}")
endfunction (configure_app_api_build_flags)

# adds a library target ${test}.appdll built from ${source-without-extension}.appdll.c
function(tobuild_appdll test source)
  # support a test with a library
  get_filename_component(srcpath ${source} ABSOLUTE)
  string(REGEX REPLACE "\\.(c|cpp)$" ".appdll.\\1" dll_srcpath "${srcpath}")
  if (EXISTS "${dll_srcpath}")
    string(REGEX REPLACE "\\.(c|cpp)$" ".appdll.\\1" dll_source "${source}")
    set(lib_srcs ${dll_source})
    set_cflags(${dll_source}) # sets srccode var

    if ("${srccode}" MATCHES "ifndef ASM_CODE_ONLY")
      # we rely on the asm rule doing preprocessing for us, so we just make
      # a copy and set the ASM_CODE_ONLY define
      get_filename_component(srcbase ${dll_srcpath} NAME_WE)
      set(srcbase "${srcbase}.appdll") # NAME_WE took this off
      get_filename_component(srcpath_rel ${source} PATH)
      add_split_asm_target("${dll_srcpath}" asm_source gen_asm_tgt
        "_asm" "${asm_defs}" "${asm_deps}")
      set(lib_srcs ${lib_srcs} ${asm_source})
    endif ("${srccode}" MATCHES "ifndef ASM_CODE_ONLY")

    if ("${srccode}" MATCHES "include \"dr_annotations.h\"")
      use_DynamoRIO_annotations(${test}.appdll lib_srcs)
    endif ("${srccode}" MATCHES "include \"dr_annotations.h\"")
    if ("${srccode}" MATCHES "include \"test.*annotation.*.h\"")
      use_DynamoRIO_test_annotations(${test}.appdll lib_srcs)
    endif ("${srccode}" MATCHES "include \"test.*annotation.*.h\"")

    add_library(${test}.appdll SHARED ${lib_srcs})
    set_target_properties(${test}.appdll PROPERTIES LINK_FLAGS "${ARCH_DEBUG}")
    # so far these are all dynamically loaded so we don't bother
    # with "target_link_libraries(${test} ${test}.appdll)"
    if ("${srccode}" MATCHES "include \"tools\\.h\"")
      target_link_libraries(${test}.appdll tools)
    endif ()
    if ("${srccode}" MATCHES "ifndef ASM_CODE_ONLY" AND
        "${CMAKE_GENERATOR}" MATCHES "Visual Studio")
      add_dependencies(${test}.appdll ${gen_asm_tgt})
    endif ()
    if ("${test}" MATCHES "nativeexec")
      # Link DynamoRIO for building nativeexec.appdll for using
      # dr_running_under_dynamorio and dr_native_handle_mbr_target.
      configure_app_api_build_flags(${test}.appdll OFF OFF)
      add_dependencies(${test}.appdll api_headers)
    endif()
  copy_target_to_device(${test}.appdll)
  endif (EXISTS "${dll_srcpath}")
endfunction(tobuild_appdll)

function(tobuild_gcc exe source depender extra_args)
  add_custom_target(target_${exe} DEPENDS ${exe})
  add_custom_command(OUTPUT ${exe} DEPENDS ${source}
    COMMAND ${CMAKE_COMMAND}
    # to work around i#84 be sure to put a space after -D for 1st arg at least
    ARGS -D exename=${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${exe}
       -D source=${CMAKE_CURRENT_SOURCE_DIR}/${source}
       -D args=${extra_args}
       -P ${CMAKE_CURRENT_SOURCE_DIR}/rungcc.cmake
    VERBATIM # recommended: p260
    )
  add_dependencies(${depender} target_${exe})
endfunction(tobuild_gcc)

# If the client is in suite/tests/client-interface, caller must append ".dll".
function(torunonly_ci test realexe realclient source client_ops dr_ops exe_ops)
  get_client_path(client_path ${realclient} ${realexe})

  # note that b/c of cmake's ridiculous scoping we can't easily share this code
  # w/ tobuild_ci: still have to propagate out of there, so we dup.
  set(testlist_ci ${testlist_ci} ${test} PARENT_SCOPE)
  set(${test}_realtest ${realexe} PARENT_SCOPE)
  set(${test}_source ${source} PARENT_SCOPE)
  set(${test}_client_ops ${client_ops} PARENT_SCOPE)
  set(${test}_dr_ops ${dr_ops} PARENT_SCOPE)
  set(${test}_exe_ops ${exe_ops} PARENT_SCOPE)
  set(${test}_client_path ${client_path} PARENT_SCOPE)
endfunction(torunonly_ci)

function(torunonly_api test realexe source dr_ops exe_ops use_decoder)
  set(testlist_api ${testlist_api} ${test} PARENT_SCOPE)
  if (realexe)
    set(${test}_realtest ${realexe} PARENT_SCOPE)
  endif (realexe)
  set(${test}_source ${source} PARENT_SCOPE)
  set(${test}_dr_ops ${dr_ops} PARENT_SCOPE)
  set(${test}_exe_ops ${exe_ops} PARENT_SCOPE)
  # If using drdecodelib, the app does not load DR, so no reason to use drrun
  # to set options (plus drrun will create a never-deleted .1config file).
  if (use_decoder)
    set(${test}_standalone_dr OFF PARENT_SCOPE)
  else ()
    set(${test}_standalone_dr ON PARENT_SCOPE)
  endif ()
endfunction(torunonly_api)

# client interface standalone app
function(add_api_exe test source use_decoder use_static_DR)
  add_exe(${test} ${source})
  configure_app_api_build_flags(${test} ${use_decoder} ${use_static_DR})
  add_dependencies(${test} api_headers)
  if (WIN32 AND NOT use_decoder AND NOT use_static_DR)
    # Since we can't set the working dir w/o making an external script,
    # we copy dynamorio.dll into the default dir.  If we just do a POST_BUILD
    # custom command we have races (i#810) so we use a globally unique
    # custom target (alternative of just adding POST_BUILD once has the downside
    # that building just one target will not work if it's not the one w/ the
    # POST_BUILD).
    if (NOT created_drcopy_target)
      set(drcopy_stamp "${CMAKE_CURRENT_BINARY_DIR}/DRcopy.stamp")
      add_custom_command(OUTPUT "${drcopy_stamp}"
        DEPENDS dynamorio drsyms
        COMMAND ${CMAKE_COMMAND} ARGS
          -E touch "${drcopy_stamp}"
        COMMAND ${CMAKE_COMMAND} ARGS
          -E copy "${MAIN_LIBRARY_OUTPUT_DIRECTORY}/dynamorio.dll"
          "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/dynamorio.dll"
        # We copy drsyms.dll too, for api.symtest
        COMMAND ${CMAKE_COMMAND} ARGS
          -E copy "${PROJECT_BINARY_DIR}/ext/${INSTALL_LIB}/drsyms.dll"
          "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/drsyms.dll"
        VERBATIM)
      add_custom_target(drcopy DEPENDS "${drcopy_stamp}")
      set(created_drcopy_target ON PARENT_SCOPE)
    endif ()
    add_dependencies(${test} drcopy)
  endif ()
endfunction(add_api_exe)

# macro so the PARENT_SCOPE in torunonly_api will reach caller of this
macro(tobuild_api test source dr_ops exe_ops use_decoder use_static_DR)
  add_api_exe(${test} ${source} ${use_decoder} ${use_static_DR})
  torunonly_api(${test} OFF ${source} "${dr_ops}" "${exe_ops}" ${use_decoder})
endmacro(tobuild_api)

# run that uses a different build
function(torunonly test realtest source dr_ops exe_ops)
  set(testlist_normal ${testlist_normal} ${test} PARENT_SCOPE)
  set(${test}_realtest ${realtest} PARENT_SCOPE)
  set(${test}_source ${source} PARENT_SCOPE)
  set(${test}_dr_ops ${dr_ops} PARENT_SCOPE)
  set(${test}_exe_ops ${exe_ops} PARENT_SCOPE)
endfunction(torunonly)

function(tobind target)
  get_target_property(binary ${target} LOCATION${location_suffix})
  if (BIND_EXECUTABLE)
    add_custom_command(TARGET ${target}
      POST_BUILD
      COMMAND ${BIND_EXECUTABLE}
      ARGS -u ${binary}
      VERBATIM # recommended: p260
      )
  else (BIND_EXECUTABLE)
    add_custom_command(TARGET ${target}
      POST_BUILD
      COMMAND ${CMAKE_LINKER}
      ARGS /edit /bind ${binary}
      VERBATIM # recommended: p260
      )
  endif (BIND_EXECUTABLE)
endfunction(tobind)

# i#215: mark tests w/ text relocations to avoid violating selinux policies:
# for that, pass in "textrel_shlib_t" as the type.
# We also need to mark tests that make their stack +x: "execmem_exec_t"
# for the type.
# If both are needed, "wine_exec_t" seems to do the trick.
function(tochcon target type)
  if (UNIX AND SEUNIXFOUND)
    get_target_property(binary ${target} LOCATION${location_suffix})
    add_custom_command(TARGET ${target}
      POST_BUILD
      COMMAND ${CHCON}
      ARGS -t ${type} ${binary}
      VERBATIM # recommended: p260
      )
  endif (UNIX AND SEUNIXFOUND)
endfunction(tochcon)

function(mark_execstack target)
  if (UNIX AND NOT APPLE)
    # Use -Wl escaping because CMake invokes the compiler to link.
    get_target_property(existing_flags ${target} LINK_FLAGS)
    set_target_properties(${target} PROPERTIES LINK_FLAGS
      "-Wl,-z,execstack ${existing_flags}")
  endif ()
endfunction(mark_execstack)

# XXX i#1468: this function is added as part of an incremental commit. It is not
# currently used, but will be used when the annotation tests are added.
function (use_DynamoRIO_test_annotations target target_srcs)
  set(dr_annotation_test_dir "${DynamoRIO_cwd}/../suite/tests/annotations")
  set(dr_annotation_test_srcs
    "${dr_annotation_test_dir}/test_mode_annotations.c"
    "${dr_annotation_test_dir}/test_annotation_arguments.c")
  configure_DynamoRIO_annotation_sources("${dr_annotation_test_srcs}")
  set(${target_srcs} ${${target_srcs}} ${dr_annotation_test_srcs} PARENT_SCOPE)
endfunction (use_DynamoRIO_test_annotations target target_srcs)

###########################################################################
# RUNNING

set(PCACHE_DIR "${PROJECT_BINARY_DIR}/pcache")
set(PCACHE_SHARED_DIR "${PROJECT_BINARY_DIR}/pcache/shared")

function(template2expect outexpect template runops key)
  # We used to use a custom perl script to match defines and runtime
  # options but we've switched to cpp.  We convert -foo to -Dfoo and
  # -foo value to -Dfoo___value.
  # First we strip out the client lib and client args
  # (this was easier when we passed a single single-quoted client_lib arg).
  string(REGEX REPLACE
    "-c;.*"
    "" rundefs "${runops}")
  # We now pass a list, so convert ; separators to spaces:
  string(REPLACE
    ";"
    " " rundefs "${rundefs}")
  # We replace bad define-name chars in client_lib with _.
  string(REGEX REPLACE
    "[;/\\.!]"
    "_" rundefs "${rundefs}")
  string(REGEX REPLACE
    "([^ ])-"
    "\\1_" rundefs "${rundefs}")
  string(REGEX REPLACE
    "(^| +)-([^ ]+)"
    " -D\\2" rundefs "${rundefs}")
  # allow matching * via XX (since * can't be in define)
  string(REGEX REPLACE
    "\\*"
    "XX" rundefs "${rundefs}")
  # allow matching = via YY (since = can't be in define)
  string(REGEX REPLACE
    "="
    "YY" rundefs "${rundefs}")
  # allow matching runtime option values via -Dfoo___value
  string(REGEX REPLACE
    "(-D[^ ]+) +([^-][^ ]*)"
    "\\1___\\2" rundefs "${rundefs}")

  if ("${CMAKE_SYSTEM_VERSION}" STRLESS "5.0")
    set(rundefs "${rundefs} -DRUNREGRESSION_NT")
  elseif ("${CMAKE_SYSTEM_VERSION}" STRLESS "5.1")
    set(rundefs "${rundefs} -DRUNREGRESSION_2000")
  elseif ("${CMAKE_SYSTEM_VERSION}" STRLESS "5.2")
    set(rundefs "${rundefs} -DRUNREGRESSION_XP")
  elseif ("${CMAKE_SYSTEM_VERSION}" STRLESS "6.0")
    set(rundefs "${rundefs} -DRUNREGRESSION_2003")
  elseif ("${CMAKE_SYSTEM_VERSION}" STRLESS "6.1")
    set(rundefs "${rundefs} -DRUNREGRESSION_VISTA")
  elseif ("${CMAKE_SYSTEM_VERSION}" STRLESS "6.2")
    set(rundefs "${rundefs} -DRUNREGRESSION_WIN7")
  elseif ("${CMAKE_SYSTEM_VERSION}" STRLESS "6.3")
    set(rundefs "${rundefs} -DRUNREGRESSION_WIN8")
  endif ()
  if (${key}_is_cygwin)
    # i#1478 option used to avoid cygwin messes up output issue: though
    # whether the output is messed up seems to depend on the cygwin version,
    # so currently this is unused.
    set(rundefs "${rundefs} -DCYGWIN")
  endif ()

  string(STRIP "${rundefs}" rundefs)

  # relies on ${defines} from top level
  # we need a list: else passed as a single arg in quotes
  string(REGEX REPLACE " " ";" deflist "${defines} ${rundefs}")
  execute_process(COMMAND
    ${CMAKE_CPP} ${CMAKE_CPP_FLAGS} ${deflist} -E ${CPP_NO_LINENUM}
      ${CPP_KEEP_WHITESPACE} ${template}
    RESULT_VARIABLE cpp_result
    ERROR_VARIABLE cpp_err
    OUTPUT_VARIABLE cpp_out
    )
  if (WIN32)
    # cl prints out name of file: no way to quiet it
    get_filename_component(file_nm ${template} NAME)
    string(REGEX REPLACE "${file_nm}[ \r\n]*" "" cpp_err "${cpp_err}")
    string(STRIP "${cpp_err}" cpp_err)
  endif (WIN32)
  if (cpp_result OR cpp_err)
    message(FATAL_ERROR "*** ${CMAKE_CPP} failed: ***\n${cpp_err}")
  endif (cpp_result OR cpp_err)

  # remove // comments since we use -traditional-cpp to preserve whitespace
  # on linux
  string(REGEX REPLACE "(\r?\n)//[^\n]*" "" cpp_out "${cpp_out}")
  string(REGEX REPLACE "^//[^\n]*" "" cpp_out "${cpp_out}")

  # remove blank lines left by cpp directives
  string(REGEX REPLACE
    "(\r?\n)+"
    "\\1" cpp_out "${cpp_out}")
  string(REGEX REPLACE
    "^(\r?\n)+"
    "" cpp_out "${cpp_out}")
  # allow trailing space on a line via @&
  string(REGEX REPLACE
    "@&"
    " " cpp_out "${cpp_out}")
  # allow matching blank lines via @@ => we strip it out after stripping blank
  string(REGEX REPLACE
    "@@"
    "" cpp_out "${cpp_out}")

  # support macros from our security days
  string(REGEX MATCHALL "-throw_exception" op_throw "${runops}")
  string(REGEX MATCHALL "-kill_thread" op_kill "${runops}")
  set(msg_vio "<Execution security violation was intercepted!\nContact your vendor for a security vulnerability fix.\n")
  set(msg_cont "Program continuing!")
  set(msg_term "Program terminated.")
  set(msg_thread "Program continuing after terminating thread.")
  set(msg_throw "Program continuing after throwing an exception.")
  set(msg_mem "Out of memory.  Program aborted.")
  set(msg_unhand "Unhandled exception caught.\n")
  # FIXME: add in \r? and test on Windows
  string(REGEX REPLACE "(^|\n) *SEC_VIO_CONT\n"        "\\1${msg_vio}${msg_cont}>\n"
    cpp_out "${cpp_out}")
  string(REGEX REPLACE "(^|\n) *SEC_VIO_STOP\n"        "\\1${msg_vio}${msg_term}>\n"
    cpp_out "${cpp_out}")
  string(REGEX REPLACE "(^|\n) *SEC_VIO_EXCEPTION\n"   "\\1${msg_vio}${msg_throw}>\n"
    cpp_out "${cpp_out}")
  string(REGEX REPLACE "(^|\n) *SEC_VIO_THREAD\n"      "\\1${msg_vio}${msg_thread}>\n"
    cpp_out "${cpp_out}")
  string(REGEX REPLACE "\n *OUT_OF_MEMORY\n"       "\n<${msg_mem}>\n"
    cpp_out "${cpp_out}")
  if (WIN32)
    string(REGEX REPLACE "\n *UNHANDLED_EXCEPTION\n" "\n${msg_unhand}"
      cpp_out "${cpp_out}")
  endif (WIN32)
  if (op_throw)
    if (WIN32)
      string(REGEX REPLACE "\n *SEC_VIO_AUTO_STOP\n"
        "\n${msg_vio}${msg_throw}>\n${msg_unhand}"
        cpp_out "${cpp_out}")
    else (WIN32)
      string(REGEX REPLACE "\n *SEC_VIO_AUTO_STOP\n" "\n${msg_vio}${msg_throw}>\n"
        cpp_out "${cpp_out}")
    endif (WIN32)
  elseif (op_kill)
    string(REGEX REPLACE "\n *SEC_VIO_AUTO_STOP\n"   "\n${msg_vio}${msg_thread}>\n"
      cpp_out "${cpp_out}")
  else (op_throw)
    string(REGEX REPLACE "\n *SEC_VIO_AUTO_STOP\n"   "\n${msg_vio}${msg_term}>\n"
      cpp_out "${cpp_out}")
  endif (op_throw)
  string(REGEX REPLACE "\n *STOP\n.*$"   "\n"
    cpp_out "${cpp_out}")

  set(${outexpect} ${cpp_out} PARENT_SCOPE)
endfunction(template2expect)

##################################################

if (UNIX)
  set(dr_os_ops -dumpcore_mask 0)
else ()
  # Turn on core dumps for most fatal errors.  Don't dump for curiosities or
  # security violations.
  set(dr_os_ops -msgbox_mask 0 -dumpcore_mask 0x7d -staged)
endif ()
set(dr_test_ops -stderr_mask 0xC ${dr_os_ops})

function(runtest_cmd outcmd outops key native standalone_dr dr_ops_aux separate_script)
  # assumes tools have been built: enforced at top level
  set(dr_ops ${dr_test_ops} ${dr_ops_aux} ${TEST_OPTIONS})
  if (NOT CLIENT_INTERFACE)
    # XXX i#1802: non-CI builds have a bug w/ early injection which we work around:
    set(dr_ops ${dr_ops} -no_early_inject)
  endif ()

  if (separate_script)
    # eliminate double-spaces, to help w/ passing strings to runall.cmake
    string(REGEX REPLACE "  +" " " dr_ops "${dr_ops}")
    string(REGEX REPLACE " $" "" dr_ops "${dr_ops}")
    # We assume there is no ; inside an option: only there as cmake list separtors
    # (we use -c instead of -client_lib).
  endif (separate_script)

  set(timeout ${TEST_SECONDS})
  if (DEFINED ${key}_timeout)
    set(timeout ${${key}_timeout})
  endif ()

  if (WIN32 OR NOT native)
    # Both Windows and non-native Unix tests now use the native drrun frontend.
    # Since the creation of CTestTestfile.cmake ends up replacing \ with / we
    # can't escape quotes, so we have to use a list variable
    if (standalone_dr OR NOT native)
      get_target_path_for_execution(drrun_path drrun)
    endif (standalone_dr OR NOT native)
    # CMake 3.x seems to put double slashes in there (xref i#1559)
    string(REGEX REPLACE "//" "/" drrun_path "${drrun_path}")
    set(cmd "${drrun_path}" -s ${timeout} -quiet)
    prefix_cmd_if_necessary(cmd OFF ${cmd})
    if (VMSAFE)
      set(cmd ${cmd} -mode code)
    endif ()
    if (DEBUG)
      set(cmd ${cmd} -debug)
    endif ()
    if (UNIX)
      set(cmd ${cmd} -killpg)
    endif ()
    if (UNIX AND TEST_SUITE AND TEST_32BIT_PATH)
      # In the Unix suite, use the DR install dir for cross-arch execve.
      set(cmd ${cmd} -dr_home "${CMAKE_INSTALL_PREFIX}")
    endif ()
    if (WIN32)
      file(TO_CMAKE_PATH "${MAIN_LIBRARY_OUTPUT_DIRECTORY}/dynamorio.dll" dllpath)
      # forward slashes work fine: we just need drive-letter path
      set(cmd ${cmd} -use_dll ${dllpath})
      # we pass -exit0 to avoid ctest from considering our segfault and other tests
      # to have failed (also xref PR 548997).
      set(cmd ${cmd} -exit0)
      if (VPS AND VMAP OR VMSAFE)
        set(cmd ${cmd} -mode code)
      endif (VPS AND VMAP OR VMSAFE)
      if (native)
        set(cmd ${cmd} -noinject)
      endif ()
    endif (WIN32)
    if (standalone_dr OR NOT native)
      # add dr_ops last to avoid needing extra \ on ;
      set(${outcmd} ${cmd} ${dr_ops} "--" PARENT_SCOPE)
    else (standalone_dr OR NOT native)
      # the test is pure native, so we rely on ctest for job control
      set(${outcmd} "" PARENT_SCOPE)
    endif (standalone_dr OR NOT native)

  elseif (UNIX AND native)
    get_target_path_for_execution(runstats_path runstats)
    set(cmd ${runstats_path} -s ${timeout} -killpg -silent)
    prefix_cmd_if_necessary(cmd OFF ${cmd})
    # We set LD_LIBRARY_PATH for native api/ apps since we're not setting
    # rpath (to make it easier to try different trees)
    set(cmd ${cmd} -env LD_LIBRARY_PATH
      "${MAIN_LIBRARY_OUTPUT_DIRECTORY}:${EXT_LIBRARY_OUTPUT_DIRECTORY}:$ENV{LD_LIBRARY_PATH}")
    # add dr_ops last to avoid needing extra \ on ;
    string(REPLACE ";" " " dr_ops_string "${dr_ops}")
    set(${outcmd} ${cmd} -env DYNAMORIO_OPTIONS "${dr_ops_string}" PARENT_SCOPE)
  else ()
    # clear the caller's `outcmd`
    set(${outcmd} "" PARENT_SCOPE)
  endif ()

  set(${outops} "${dr_ops}" PARENT_SCOPE)
endfunction(runtest_cmd)

# if test != exe we assume exe is already added as a build target
# key is the test name without the options, and is used to look up ${key}_expectbase
# and ${key}_postcmd, which points to the post-processing executable if necessary.
function(torun test key source native standalone_dr dr_ops exe_ops added_out)
  # Avoid running some tests with invalid option combinations.  For example,
  # coarse fragments are incompatible with thread private code caches.
  if ("${dr_ops}" MATCHES "-thread_private" AND
      "${dr_ops}" MATCHES "-opt_memory")
    set(${added_out} OFF PARENT_SCOPE)
    return()
  endif ()
  if (NOT "${dr_ops}" MATCHES "-no_early_inject" AND
      "${dr_ops}" MATCHES "-no_private_loader")
    set(${added_out} OFF PARENT_SCOPE)
    return()
  endif ()

  if (DEFINED ${key}_realtest)
    set(exe ${${key}_realtest})
  else (DEFINED ${key}_realtest)
    set(exe ${key})
  endif (DEFINED ${key}_realtest)

  get_filename_component(srcbase ${source} NAME_WE)
  if (DEFINED ${key}_basedir)
    set(testpath ${${key}_basedir})
  else ()
    get_filename_component(srcpath ${source} PATH)
    set(testpath ${CMAKE_CURRENT_SOURCE_DIR}/${srcpath})
  endif ()
  if (EXISTS "${testpath}/${srcbase}.runall")
    set(is_runall ON)
  else ()
    set(is_runall OFF)
  endif ()
  if (EXISTS "${testpath}/${srcbase}.runcmp" OR
      DEFINED ${key}_runcmp)
    set(is_runcmp ON)
  else ()
    set(is_runcmp OFF)
  endif ()
  if (is_runall OR is_runcmp)
    set(separate_script ON)
  else ()
    set(separate_script OFF)
  endif ()

  set(ALREADY_REGEX OFF)

  runtest_cmd(rundr runops ${key} ${native} ${standalone_dr}
    "${dr_ops}" ${separate_script})

  if (TARGET ${exe})
    get_target_path_for_execution(exepath ${exe})
  else ()
    # support running binaries that are not targets of this build
    set(exepath ${exe})
  endif ()

  if (DEFINED ${key}_expectbase)
    set(expectbase ${${key}_expectbase})
  else ()
    set(expectbase ${srcbase})
  endif ()

  if (TEST_SUITE AND TEST_32BIT_PATH AND ${test} MATCHES "linux.execve32")
    # We're an x64 build, but we need to launch a 32-bit app under DR.  The
    # suite sets -dr_home to the full install dir with both archs, but we're
    # invoking bin64/drrun.  Ideally drrun would detect which ELF class we're
    # about to exec, but until then we have to add -32 for 32-bit apps.
    string(REGEX REPLACE "/drrun;" "/drrun;-32;" rundr "${rundr}")
  endif ()

  if (is_runall)
    # To run the test we use a series of commands in runall.cmake.
    file(READ "${testpath}/${srcbase}.runall" runall)
    if (UNIX)
      # We always run infloop.
      get_target_path_for_execution(app_path linux.infloop)
    else ()
      # It's easier to run our own infloop, which prints so we know when it's up,
      # than to run notepad or sthg.
      get_target_path_for_execution(app_path win32.infloop)
    endif ()
    # Swap from drrun to run_in_bg to run the test in the background and print the pid.
    # XXX i#1874: add support for running on Android
    set(tmpfile "${CMAKE_CURRENT_BINARY_DIR}/${key}-out")
    if (UNIX)
      set(pidfile "")
      string(REGEX REPLACE "^(.*)/drrun" "\\1/run_in_bg;-out;${tmpfile};\\1/drrun"
        rundr "${rundr}")
    else ()
      set(pidfile "${CMAKE_CURRENT_BINARY_DIR}/${key}-pid")
      # We want the pid not of drrun but its child so we pass -pidfile to drrun rather
      # than -pid to run_in_bg.
      # XXX i#120: add systemwide injection testing.  For now we only test drrun.
      string(REGEX REPLACE "^(.*)/drrun.exe"
        "\\1/run_in_bg;-out;${tmpfile};\\1/drrun.exe;-pidfile;${pidfile}"
        rundr "${rundr}")
    endif ()
    # Remove timeout args to drrun to ensure that it doesn't fork the app.
    string(REGEX REPLACE ";-s;[0-9]+" "" rundr "${rundr}")
    string(REGEX REPLACE ";-killpg" "" rundr "${rundr}")
    set(clear_arg "")
    if ("${runall}" MATCHES "<reset>")
      set(nudge_arg "-type\;reset")
    elseif ("${runall}" MATCHES "<freeze>")
      set(nudge_arg "-type\;freeze")
    elseif ("${runall}" MATCHES "<persist>")
      set(nudge_arg "-type\;persist")
      # clear out pcache dir prior to run
      set(clear_arg "${PCACHE_SHARED_DIR}")
    elseif ("${runall}" MATCHES "<use-persisted>")
      set(nudge_arg "<use-persisted>")
    elseif ("${runall}" MATCHES "<client")
      string(REGEX MATCHALL "<client_nudge[^>]+" nudge_arg "${runall}")
      string(REGEX REPLACE "<client_nudge" "-client" nudge_arg "${nudge_arg}")
      string(REGEX REPLACE " " "\\\\;" nudge_arg "${nudge_arg}")
    endif ()
    set(cmd_with_at ${rundr} ${app_path} ${exe_ops})
    # we pass intra-arg spaces via @@ and inter-arg via @ and ; via !
    # to get around the pain of trying to quote everything just right:
    # much simpler this way.
    string(REGEX REPLACE " " "@@" cmd_with_at "${cmd_with_at}")
    string(REGEX REPLACE ";" "@" cmd_with_at "${cmd_with_at}")
    if (NOT APPLE) # XXX i#1286: implement nudge for MacOS
      if (UNIX)
        get_target_path_for_execution(toolbindir nudgeunix)
      else ()
        get_target_path_for_execution(toolbindir drconfig)
      endif ()
    else ()
      set(toolbindir "i#1286-not-implemented-for-Mac")
    endif ()
    get_filename_component(toolbindir ${toolbindir} DIRECTORY)
    add_test(${test} ${CMAKE_COMMAND} -D toolbindir=${toolbindir}
      -D cmd=${cmd_with_at} -D out=${tmpfile} -D nudge=${nudge_arg}
      -D clear=${clear_arg} -D pidfile=${pidfile}
      -P ${CMAKE_CURRENT_SOURCE_DIR}/runall.cmake)
  elseif (is_runcmp)
    # Some apps have enough output that ctest runs out of memory when
    # doing its built-in regex cmp so we use a separate script.
    # We also use this approach for tests that need custom success tests.
    if (DEFINED ${key}_nodr)
      set(cmd_with_at ${exepath} ${exe_ops})
    else ()
      set(cmd_with_at ${rundr} ${exepath} ${exe_ops})
    endif ()
    # we pass intra-arg spaces via @@ and inter-arg via @ and ; via !
    # to get around the pain of trying to quote everything just right:
    # much simpler this way.
    # XXX i#1327: now that we have -c and other option passing improvements we
    # should be able to get rid of this @@ stuff.
    string(REGEX REPLACE " " "@@" cmd_with_at "${cmd_with_at}")
    string(REGEX REPLACE ";" "@" cmd_with_at "${cmd_with_at}")
    if (DEFINED ${key}_runcmp)
      set(runcmp_script ${${key}_runcmp})
    else ()
      set(runcmp_script ${CMAKE_CURRENT_SOURCE_DIR}/runcmp.cmake)
    endif ()
    if (NOT DEFINED ${key}_postcmd)
      set(${key}_postcmd "")
    endif ()
    add_test(${test} ${CMAKE_COMMAND}
      -D precmd=${${key}_precmd}
      -D cmd=${cmd_with_at}
      -D postcmd=${${key}_postcmd}
      -D postcmd2=${${key}_postcmd2}
      -D postcmd3=${${key}_postcmd3}
      -D cmp=${CMAKE_CURRENT_BINARY_DIR}/${expectbase}.expect
      -D code=${${key}_code}
      -P ${runcmp_script})
    # No support for regex here (ctest can't handle large regex)
    set(ALREADY_REGEX ON)
  else (is_runcmp)
    add_test(${test} ${rundr} ${exepath} ${exe_ops})
  endif (is_runall)

  if (EXISTS ${testpath}/${expectbase}.expect)
    file(READ ${testpath}/${expectbase}.expect expect)
    # add dependence so cmake will reconfigure if file changes
    configure_file(${testpath}/${expectbase}.expect
      ${CMAKE_CURRENT_BINARY_DIR}/ignoreme_for_dep)
  elseif (EXISTS ${testpath}/${expectbase}.template)
    # We convert .template at configure time and use CTest's built-in
    # PASS_REGULAR_EXPRESSION to diff the output.
    #
    # Advantages of this approach:
    # * More efficient: no separate script to launch that then launches
    #   multiple sub-processes to diff or cmake -E compare_files.
    #
    # Advantages of instead using a script at runtime to convert:
    # * CMake can't handle giant files as regexps (api/dis.template in particular)
    #   so we wouldn't need to special-case it
    # * Large regexps that don't match are printed in their entirety, so
    #   if we did our own diff then the CTest output would be more readable.
    #   Note that we can't output, say, the diff head, but we can be quieter.
    # * Changes in .template don't require reconfiguring: but it's automatic
    #   (thanks to configure_file() hack) and pretty quick
    # * We could support extra runtime options via env var DYNAMORIO_OPTIONS,
    #   but while that might be useful it could also be confusing.
    template2expect(expect
      ${testpath}/${expectbase}.template
      "${runops}"
      ${key})
    # add dependence so cmake will reconfigure if file changes
    configure_file(${testpath}/${expectbase}.template
      ${CMAKE_CURRENT_BINARY_DIR}/ignoreme_for_dep)
  elseif (EXISTS ${testpath}/${expectbase}.templatex)
    # A .templatex is a .template that is treated directly as a regex:
    # so any regex chars inside are not converted to literals.
    # We separate the two so that we don't have to escape everything in
    # regular templates where we want a literal match.
    if (DEFINED ${key}_rawtemp)
      # No preprocessor
      file(READ ${testpath}/${expectbase}.templatex expect)
    else ()
      template2expect(expect
        ${testpath}/${expectbase}.templatex
        "${runops}"
        ${key})
    endif ()
    # add dependence so cmake will reconfigure if file changes
    configure_file(${testpath}/${expectbase}.templatex
      ${CMAKE_CURRENT_BINARY_DIR}/ignoreme_for_dep)
    set(ALREADY_REGEX ON)
  else (EXISTS ${testpath}/${expectbase}.expect)
    message(FATAL_ERROR
      "no .expect or .template or .templatex for ${testpath}/${expectbase} for ${key}")
  endif (EXISTS ${testpath}/${expectbase}.expect)

  if (NOT ALREADY_REGEX)
    # turn regex chars into literals
    # \\] somehow messes up the match though $expect looks identical
    # some tests do have ']' so we replace it in a separate step below
    string(REGEX REPLACE "([\\^\\$\\.\\*\\+\\?\\|\\(\\)\\[\\;])"
      "\\\\\\1" expect "${expect}")
    string(REGEX REPLACE "\\]"
      "\\\\]" expect "${expect}")
  endif (NOT ALREADY_REGEX)

  if (WIN32 AND NOT is_runcmp) # for runcmp it's up to the test
    # libc print => CRLF, dr_printf => plain newline: match both
    string(REGEX REPLACE "\n" "\r?\n" expect "${expect}")
  endif ()

  if (is_runcmp)
    file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/${expectbase}.expect" "${expect}")
  else ()
    # match whole output
    set(expect "^${expect}$")
    set_tests_properties(${test} PROPERTIES PASS_REGULAR_EXPRESSION "${expect}")
  endif ()

  if (DEFINED ${key}_timeout)
    # Though we mostly use drrun's -s we set this too in case the requested
    # value is higher than ctest's default.
    set_tests_properties(${test} PROPERTIES TIMEOUT ${${key}_timeout})
  endif ()

  if (WIN32)
    # With i#265/PR 486139 we use local one-time config files instead of
    # global reg keys and can thus run multiple instances of the same
    # app name simultaneously so we do not need to mark RUN_SERIAL
  endif (WIN32)
  set(${added_out} ON PARENT_SCOPE)
endfunction(torun)

macro (string2list var)
  string(REPLACE " " ";" ${var} ${${var}})
endmacro ()

macro(strings2lists test ops_var)
  # We now use a list instead of a string for ops.  We convert here where we can
  # assume there are no spaces (from paths) in the options.
  if (NOT ${${ops_var}} STREQUAL "")
    string2list(${ops_var})
  endif ()
  if (DEFINED ${test}_dr_ops)
    string2list(${test}_dr_ops)
  endif ()
  if (DEFINED ${test}_client_ops AND NOT DEFINED ${test}_client_ops_islist)
    string2list(${test}_client_ops)
  endif ()
endmacro()

function(torun_normal name test ops)
  strings2lists(${test} ops)
  torun("${name}" ${test} ${${test}_source} OFF OFF "${${test}_dr_ops};${ops}"
    "${${test}_exe_ops}" added)
  if (DEFINED ${test}_depends)
    ops2testprefix(prefix "${ops}")
    set_property(TEST ${name} APPEND PROPERTY DEPENDS "${prefix}${${test}_depends}")
  endif (DEFINED ${test}_depends)
  if (DEFINED ${test}_env)
    ops2testprefix(prefix "${ops}")
    set_property(TEST ${name} APPEND PROPERTY ENVIRONMENT "${prefix}${${test}_env}")
  endif ()
endfunction(torun_normal)

function(torun_ci name test ops)
  strings2lists(${test} ops)
  set(cli_ops "${${test}_client_ops}")
  if ("${test}" MATCHES "client\\.flush")
    # See PR 244090 - currently there is no way for a client to see what
    # options DR was run with.  The flush tests needs to know whether it
    # can use the unlink flush routines or not which depends on having
    # either -thread_private or -enable_full_api We check here and pass
    # a client option if the unlink flush is safe to use.
    if ("${ops} ${TEST_OPTIONS}" MATCHES "(-thread_private|-enable_full_api)")
      set(cli_ops "use_unlink")
    endif ()
  endif ("${test}" MATCHES "client\\.flush")
  # note that we cannot use double quotes as they'll be stripped out
  if (DEFINED ${test}_toolname)
    # end-user tool
    torun("${name}" ${test} ${${test}_source} OFF OFF
      "${${test}_dr_ops};${ops};-t;${${test}_toolname};${cli_ops}"
      "${${test}_exe_ops}" added)
  else ()
    # simple client
    torun("${name}" ${test} ${${test}_source} OFF OFF
      "${${test}_dr_ops};${ops};-c;${${test}_client_path};${cli_ops}"
      "${${test}_exe_ops}" added)
  endif ()
  if (added)
    if (DEFINED ${test}_depends)
      ops2testprefix(prefix "${ops}")
      set_property(TEST ${name} APPEND PROPERTY DEPENDS "${prefix}${${test}_depends}")
    endif (DEFINED ${test}_depends)
    if (DEFINED ${test}_env)
      ops2testprefix(prefix "${ops}")
      set_property(TEST ${name} APPEND PROPERTY ENVIRONMENT "${prefix}${${test}_env}")
    endif ()
  endif (added)
endfunction(torun_ci)

function(torun_api name test ops)
  strings2lists(${test} ops)
  torun("${name}" ${test} ${${test}_source} ON ${${test}_standalone_dr}
    "${${test}_dr_ops};${ops}" "${${test}_exe_ops}" added)
endfunction(torun_api)

function(torunonly_native name realexe expect source exe_ops)
  strings2lists(${name} ops)
  set(${name}_expectbase ${expect})
  set(${name}_realtest ${realexe})
  torun(${name} ${name} ${source} ON OFF "" "${exe_ops}" added)
endfunction(torunonly_native)

function(torun_runall name test ops)
  strings2lists(${test} ops)
  # FIXME NYI i#120
endfunction(torun_runall)

function(disable_optimizations_for_file source_file)
  get_source_file_property(cflags "${source_file}" COMPILE_FLAGS)
  string(REGEX REPLACE "[/-]O[1-9]" "" cflags "${cflags}")
  set_source_files_properties("${source_file}" PROPERTIES
    COMPILE_FLAGS "${cflags}")
endfunction(disable_optimizations_for_file)

# optimize the target
function(optimize target)
  if (UNIX)
    append_property_string(TARGET ${target} COMPILE_FLAGS "-O3")
  else ()
    append_property_string(TARGET ${target} COMPILE_FLAGS "/Ox /GL")
  endif (UNIX)
endfunction(optimize)

###########################################################################
# tests

# This step takes a while so we provide a status message:
message(STATUS "Processing tests and generating expected output patterns")

# Make sure we test running a path with spaces (to avoid regressions like i#2538).
#set(common.broadfun_outname "common.broadfun spaces")
#tobuild(common.broadfun common/broadfun.c)
#if (DEBUG)
#  torunonly(common.logstderr common.broadfun common/logstderr.c
#    "-log_to_stderr -loglevel 1 -logmask 2" "")
#endif ()
#if (NOT ANDROID) # We do not support -no_early_inject on Android (i#1873).
#  tobuild_ops(common.fib common/fib.c "-no_early_inject" "")
#endif ()
#if (X86) # FIXME i#1551, i#1569: port asm to ARM and AArch64
#  tobuild(common.decode-bad common/decode-bad.c)
#  # FIXME i#105: get this working for 32-bit linux
#  if (X64 OR WIN32)
#    tobuild(common.decode common/decode.c)
#  endif (X64 OR WIN32)
#endif (X86)
## FIXME i#1025: get working on Linux
#if (WIN32)
#  # We build all tests before running so decode will be built before
#  # ctest -j ever ran this.  If we switch to --build-and-test then we'll
#  # make these runonly tests make sure their exe is built.
#  torunonly(common.decode-stress common.decode common/decode.c
#    "-stress_recreate_state" "")
##endif (WIN32)

if (X86) # FIXME i#1551, i#1569: port asm to ARM and AArch64
  # nativeexec tests are under CLIENT_INTERFACE b/c they link w/ DR and need api_headers.
  # We do not run for VMSAFE as they fail there and we just don't support that
  # combination now (i#1793).
  # XXX i#1287: implement for MacOS
  # We do not support -no_early_inject on Android (i#1873).
  # FIXME i#1799: clang does not support "asm goto".
  if (CLIENT_INTERFACE AND NOT VMSAFE AND NOT APPLE AND NOT ANDROID
      AND ANNOTATIONS AND NOT CMAKE_COMPILER_IS_CLANG)
    # XXX i#1651: we don't support -native_exec combined with -early_inject so
    # for now we pass -no_early_inject to all of these tests.
    tobuild_appdll(common.nativeexec common/nativeexec.c)
    get_target_property(native_dll_name common.nativeexec.appdll
      LOCATION${location_suffix})
    get_filename_component(native_dll_name "${native_dll_name}" NAME)
    tobuild_ops(common.nativeexec common/nativeexec.c
      "-no_early_inject -native_exec_list ${native_dll_name}" "")
    target_link_libraries(common.nativeexec common.nativeexec.appdll)
    # We want rpath on Linux so we can load the appdll.
    set_target_properties(common.nativeexec PROPERTIES SKIP_BUILD_RPATH OFF)
    if (UNIX)
      # FIXME i#978: Windows support NYI
      #torunonly(common.nativeexec_retakeover common.nativeexec common/nativeexec.c
      #  "-no_early_inject -native_exec_list ${native_dll_name} -native_exec_retakeover"
      #  "")
      torunonly(common.nativeexec_exe common.nativeexec
        common/nativeexec_exenative.c
        "-log_to_stderr -loglevel 4 -no_early_inject -native_exec_list common.nativeexec -native_exec_retakeover" "")
      #torunonly(common.nativeexec_bindnow common.nativeexec common/nativeexec_exenative.c
      #  "-no_early_inject -native_exec_list common.nativeexec -native_exec_retakeover"
      #  "-bind_now")
      # XXX: the next 3 tests, and perhaps all of these native_exec tests,
      # really want a mode with -no_private_loader -no_mangle_app_seg.  With
      # our reluctance to add lib deps back to DR, that's going to require a
      # sepaparate build.  Until someone really focuses on hybrid mode again
      # (in which case a new build param should be added to re-enable libdl),
      # we leave the tests here as regression tests but w/ private loader in place.
      #torunonly(common.nativeexec_retakeover_opt common.nativeexec common/nativeexec.c
      #  "-no_early_inject -native_exec_list ${native_dll_name} -native_exec_retakeover  -native_exec_opt -no_kstats" "")
      # No swap TLS and no kstats for optimized native_exec
      #torunonly(common.nativeexec_exe_opt common.nativeexec common/nativeexec_exenative.c
      #  "-no_early_inject -native_exec_list common.nativeexec -native_exec_retakeover -native_exec_opt -no_kstats" "")
      #torunonly(common.nativeexec_bindnow_opt common.nativeexec
      #  common/nativeexec_exenative.c
      #  "-no_early_inject -native_exec_list common.nativeexec -native_exec_retakeover -native_exec_opt -no_kstats" "-bind_now")
    endif ()
  endif ()

  # App seg mangling makes dstack test fail, and our default build does not
endif (X86)

# FIXME i#1360: implement Mach-O export iterator
  # FIXME i#2008: fix bugs on Android
#  if (NOT MACOS AND NOT ANDROID)
#    tobuild_appdll(client.modules client-interface/modules.c)
#    get_target_property(modules_libname client.modules.appdll
#      LOCATION${location_suffix})
#    tobuild_ci(client.modules client-interface/modules.c "" "" "${modules_libname}")
#  endif ()
#  if (X86) # FIXME i#1551, i#1569: port to ARM and AArch64
#    tobuild_ci(client.alloc client-interface/alloc.c "" "" "")
#    tobuild_ci(client.cleancall client-interface/cleancall.c "" "" "")
#    if (UNIX)
#      torunonly_ci(client.cleancall.prof-pcs client.cleancall client.cleancall.dll
#        client-interface/cleancall.c "" "-prof_pcs -opt_cleancall 0" "")
#      if (NOT X64)
#        torunonly_ci(client.cleancall.prof-pcs.thread-private
#          client.cleancall client.cleancall.dll
#          client-interface/cleancall.c "" "-thread_private -prof_pcs -opt_cleancall 0" "")
#      endif (NOT X64)
#    endif (UNIX)
#    tobuild_ci(client.count-ctis client-interface/count-ctis.c "" "" "")
#    # check dr_insert_cbr_instrumentation with out-of-line clean call
#    torunonly_ci(client.count-ctis-noopt client.count-ctis client.count-ctis.dll
#      client-interface/count-ctis.c "" "-opt_cleancall 0" "")
#    tobuild_ci(client.syscall client-interface/syscall.c "" "-no_follow_children" "")
#    tobuild_ci(client.count-bbs client-interface/count-bbs.c "" "" "")
#  endif (X86)
#  tobuild_ci(client.cleancallparams client-interface/cleancallparams.c "" "" "")
#  tobuild_ci(client.app_inscount client-interface/app_inscount.c "" "" "")
#  if (NOT WIN32) # FIXME i#1717: add Windows client C++ EH support
#    if (NOT ANDROID) # XXX i#1874: get working on Android
#      tobuild_ci(client.exception client-interface/exception.cpp "" "" "")
#    endif ()
##  endif ()

#  # FIXME i#1799: clang does not support "asm goto"
#  if (ANNOTATIONS AND NOT CMAKE_COMPILER_IS_CLANG)
#    set(DynamoRIO_USE_LIBC OFF)
#    if (UNIX)
#      set(annotation_test_args "libclient.annotation-concurrency.appdll.so" "A" "4")
#    else (UNIX)
#      set(annotation_test_args "client.annotation-concurrency.appdll.dll" "A" "4")
#    endif (UNIX)
#    tobuild_ci(client.annotation-concurrency client-interface/annotation-concurrency.c
#      "" "" "${annotation_test_args}")
#    tobuild_appdll(client.annotation-concurrency
#      client-interface/annotation-concurrency.c)
#    if (UNIX)
#      target_link_libraries(client.annotation-concurrency ${libpthread})
#    endif (UNIX)
#    # i#1681: on a many-core machine this test can hit the 1-min timeout
#    set(client.annotation-concurrency_timeout 120)
#    set(client.annotation-concurrency.full-decode_expectbase
#      "annotation-concurrency.full-decode")
#    torunonly_ci(client.annotation-concurrency.full-decode client.annotation-concurrency
#      client.annotation-concurrency.dll client-interface/annotation-concurrency.c
#      "full-decode" "" "${annotation_test_args}")
#    # i#1681: on a many-core machine this test can hit the 1-min timeout
#    set(client.annotation-concurrency.full-decode_timeout 120)
#    set(client.annotation-concurrency.full-decode.tiny-bb_expectbase
#      "annotation-concurrency.full-decode")
#    torunonly_ci(client.annotation-concurrency.full-decode.tiny-bb
#      client.annotation-concurrency client.annotation-concurrency.dll
#      client-interface/annotation-concurrency.c "full-decode" "-max_bb_instrs 2"
#      "${annotation_test_args}")
#    # i#1681: on a many-core machine this test can hit the 1-min timeout
#    set(client.annotation-concurrency.full-decode.tiny-bb_timeout 120)
#    set(client.annotation-concurrency.bb-truncate-1_expectbase
#      "annotation-concurrency.bb-truncate")
#    torunonly_ci(client.annotation-concurrency.bb-truncate-1 client.annotation-concurrency
#      client.annotation-concurrency.dll client-interface/annotation-concurrency.c
#      "truncate@1" "" "${annotation_test_args}")
#    # i#1681: on a many-core machine this test can hit the 1-min timeout
#    set(client.annotation-concurrency.bb-truncate-1_timeout 120)
#    set(client.annotation-concurrency.bb-truncate-2_expectbase
#      "annotation-concurrency.bb-truncate")
#    torunonly_ci(client.annotation-concurrency.bb-truncate-2 client.annotation-concurrency
#      client.annotation-concurrency.dll client-interface/annotation-concurrency.c
#      "truncate@2" "" "${annotation_test_args}")
#    # i#1681: on a many-core machine this test can hit the 1-min timeout
#####    set(client.annotation-concurrency.bb-truncate-2_timeout 120)

#    if (UNIX)
#      set(client.annotation-concurrency.prof-pcs_timeout 120)
#      set(client.annotation-concurrency.prof-pcs_expectbase
#        "annotation-concurrency.full-decode")
#      torunonly_ci(client.annotation-concurrency.prof-pcs client.annotation-concurrency
#        client.annotation-concurrency.dll client-interface/annotation-concurrency.c
#        "full-decode" "-prof_pcs" "${annotation_test_args}")
#      if (NOT X64)
#        set(client.annotation-concurrency.prof-pcs.thread-private_timeout 120)
#        set(client.annotation-concurrency.prof-pcs.thread-private_expectbase
#          "annotation-concurrency.full-decode")
#        torunonly_ci(client.annotation-concurrency.prof-pcs.thread-private
#          client.annotation-concurrency
#          client.annotation-concurrency.dll client-interface/annotation-concurrency.c
#          "full-decode" "-thread_private -prof_pcs" "${annotation_test_args}")
#      endif (NOT X64)
##############    endif (UNIX)

#    if (UNIX)
#      set(annotation_opt_args "libclient.annotation-concurrency-opt.appdll.so" "A" "4")
#    else (UNIX)
#      set(annotation_opt_args "client.annotation-concurrency-opt.appdll.dll" "A" "4")
#    endif (UNIX)
#    tobuild_ci(client.annotation-concurrency-opt client-interface/annotation-concurrency.c
#      "" "" "${annotation_opt_args}")
#    optimize(client.annotation-concurrency-opt)
#    tobuild_appdll(client.annotation-concurrency-opt
#      client-interface/annotation-concurrency.c)
#    optimize(client.annotation-concurrency-opt.appdll)
#    if (UNIX)
#      target_link_libraries(client.annotation-concurrency-opt ${libpthread})
#    endif (UNIX)
#    # i#1681: on a many-core machine this test can hit the 1-min timeout
#    set(client.annotation-concurrency-opt_timeout 120)
#    set(client.annotation-concurrency-opt.full-decode_expectbase
#      "annotation-concurrency.full-decode")
#    torunonly_ci(client.annotation-concurrency-opt.full-decode
#      client.annotation-concurrency-opt client.annotation-concurrency-opt.dll
#      client-interface/annotation-concurrency.c "full-decode" "" "${annotation_opt_args}")
#    # i#1681: on a many-core machine this test can hit the 1-min timeout
##################    set(client.annotation-concurrency-opt.full-decode_timeout 120)

#    tobuild_ci(client.annotation-detection client-interface/annotation-detection.cpp
#      "" "" "")
#    set(client.annotation-detection.full-decode_expectbase
#      "annotation-detection.full-decode")
#    torunonly_ci(client.annotation-detection.full-decode client.annotation-detection
#      client.annotation-detection.dll client-interface/annotation-detection.c
#      "full-decode" "" "")
#    set(client.annotation-detection.full-decode.tiny-bb_expectbase
#      "annotation-detection.full-decode")
#    torunonly_ci(client.annotation-detection.full-decode.tiny-bb
#      client.annotation-detection client.annotation-detection.dll
#      client-interface/annotation-detection.c "full-decode" "-max_bb_instrs 2" "")
#    set(client.annotation-detection.bb-truncate-1_expectbase
#      "annotation-detection.bb-truncate")
#    torunonly_ci(client.annotation-detection.bb-truncate-1 client.annotation-detection
#      client.annotation-detection.dll client-interface/annotation-detection.c
#      "truncate@1" "" "")
#    set(client.annotation-detection.bb-truncate-2_expectbase
#      "annotation-detection.bb-truncate")
#    torunonly_ci(client.annotation-detection.bb-truncate-2 client.annotation-detection
#      client.annotation-detection.dll client-interface/annotation-detection.c
#      "truncate@2" "" "")
#    torunonly_native(client.annotation-detection.native client.annotation-detection
#################      annotation-detection.native client-interface/annotation-detection.c "")

#    set(client.annotation-detection-opt_expectbase "annotation-detection")
#    tobuild_ci(client.annotation-detection-opt client-interface/annotation-detection.cpp
#      "" "" "")
#    optimize(client.annotation-detection-opt)
#    torunonly_native(client.annotation-detection-opt.native
#      client.annotation-detection-opt annotation-detection.native
#      client-interface/annotation-detection.c "")
#    set(DynamoRIO_USE_LIBC ON)
############  endif (ANNOTATIONS AND NOT CMAKE_COMPILER_IS_CLANG)

#  if (UNIX)
#    # XXX i#1246: Make partial_module_map work for Windows as well.
#    tobuild_ci(client.partial_module_map client-interface/partial_module_map.c "" "" "")
#  endif ()
#  if (LINUX AND X86)
#    # XXX i#1833: once i#1833 is fixed, enable code in mbr_instrumentation_segment.dll.c
#    tobuild_ci(client.mbr_instrumentation_segment
#      client-interface/mbr_instrumentation_segment.c "" "" "")
#    use_DynamoRIO_extension(client.mbr_instrumentation_segment.dll drsyms)
#    use_DynamoRIO_extension(client.mbr_instrumentation_segment.dll drmgr)
#  endif()
#  if (NOT ARM) # FIXME i#1551: fix bugs on ARM
#    tobuild_ci(client.segfault client-interface/segfault.c "" "" "")
#    tobuild_ci(client.execfault client-interface/execfault.c "" "" "")
#    tobuild_appdll(client.events client-interface/events.c)
#    get_target_property(events_appdll_path client.events.appdll LOCATION${location_suffix})
#  endif (NOT ARM)
#  if (ANNOTATIONS AND NOT ARM)
#    if (UNIX OR NOT X64) # the Valgrind annotation structure cannot support Windows x64
#      tobuild_ci(client.vg-annot client-interface/vg-annot.c "" "" "")
#      set(client.vg-annot.full-decode_expectbase "vg-annot.full-decode")
#      torunonly_ci(client.vg-annot.full-decode client.vg-annot client.vg-annot.dll
#        client-interface/vg-annot.c "full-decode" "" "")
#      set(client.vg-annot.full-decode.tiny-bb_expectbase "vg-annot.full-decode")
#      torunonly_ci(client.vg-annot.full-decode.tiny-bb client.vg-annot client.vg-annot.dll
#        client-interface/vg-annot.c "full-decode" "-max_bb_instrs 2" "")
#      set(client.vg-annot.bb-truncate-1_expectbase "vg-annot.bb-truncate")
#      torunonly_ci(client.vg-annot.bb-truncate-1 client.vg-annot client.vg-annot.dll
#        client-interface/vg-annot.c "truncate@1" "" "")
#      set(client.vg-annot.bb-truncate-2_expectbase "vg-annot.bb-truncate")
#      torunonly_ci(client.vg-annot.bb-truncate-2 client.vg-annot client.vg-annot.dll
#        client-interface/vg-annot.c "truncate@2" "" "")
#      tobuild_ci(client.vg-annot-opt client-interface/vg-annot.c "" "" "")
#      optimize(client.vg-annot-opt)
#      set(client.vg-annot-opt.full-decode_expectbase "vg-annot.full-decode")
#      torunonly_ci(client.vg-annot-opt.full-decode client.vg-annot-opt client.vg-annot.dll
#        client-interface/vg-annot.c "full-decode" "" "")
#      set(client.vg-annot-opt.full-decode.tiny-bb_expectbase "vg-annot.full-decode")
#      torunonly_ci(client.vg-annot-opt.full-decode.tiny-bb client.vg-annot-opt
#        client.vg-annot.dll client-interface/vg-annot.c
#        "full-decode" "-max_bb_instrs 3" "")
#      set(client.vg-annot-opt.bb-truncate_expectbase "vg-annot.bb-truncate")
#      torunonly_ci(client.vg-annot-opt.bb-truncate client.vg-annot-opt client.vg-annot.dll
#        client-interface/vg-annot.c "truncate@2" "" "")
#    endif (UNIX OR NOT X64)
#  endif (ANNOTATIONS AND NOT ARM)
#  if (UNIX)
#    if (NOT ARM) # FIXME i#1551: fix bugs on ARM
#      if (AARCH64)
#        # FIXME i#1569: Reenable when trace construction is implemented.
#        set(trace_arg "-disable_traces")
#      else ()
#        # Just happens to not make as many traces.
#        set(trace_arg "-trace_threshold 5")
#      endif ()
#      tobuild_ci(client.events client-interface/events.c
#        "" "${trace_arg}" "${events_appdll_path}")
#      tobuild_ci(client.events_cpp client-interface/events_cpp.cpp
#        "" "${trace_arg}" "${events_appdll_path}")
#      tobuild_ci(client.nudge_test client-interface/nudge_test.runall "" "" "")
#      tobuild_ci(client.timer client-interface/timer.c "" "" "")
#    endif (NOT ARM)
#    if (X86 OR AARCH64) # FIXME i#1551: port asm to AArch32
#      tobuild_ci(client.syscall-mod client-interface/syscall-mod.c "" "" "")
#    endif (X86 OR AARCH64)
#    if (X86) # FIXME i#1551, i#1569: port asm to ARM and AArch64
#      tobuild_ci(client.signal client-interface/signal.c "" "" "")
#      target_link_libraries(client.signal ${libpthread})
#      tobuild_ci(client.cbr-retarget client-interface/cbr-retarget.c "" "" "")
#    endif (X86)
#    tobuild_ci(client.cleancallsig client-interface/cleancallsig.c "" "" "")
#    if (AARCH64) # XXX i#3173 Improve testing of emulation API functions
#      tobuild_ci(client.emulation_api_simple client-interface/emulation_api_simple.c "" "" "")
#      use_DynamoRIO_extension(client.emulation_api_simple.dll drmgr)
#    endif (AARCH64)
#  else (UNIX)
#    tobuild_ci(client.events client-interface/events.c
#      "" "" "${events_appdll_path}")
#    tobuild_ci(client.events_cpp client-interface/events_cpp.cpp
#      "" "" "${events_appdll_path}")
#    tobuild_ci(client.cbr3 client-interface/cbr3.c "" "" "")
#    tobuild_ci(client.cbr4 client-interface/cbr4.c "" "-thread_private" "")
#    # FIXME i#120: enable these .runall tests once have .runall support
#    #tobuild_ci_runall(client.cbr2 client-interface/cbr2.runall "" "" "")
#    #tobuild_ci_runall(client.cbr client-interface/cbr.runall "" "" "")
#    #tobuild_ci_runall(client.custom_traces client-interface/custom_traces.runall
#    #  "" "" "")
#    #tobuild_ci_runall(client.decode-bb client-interface/decode-bb.runall "" "" "")
#    # I reworked Windows .runall support to run the new win32.infloop but have not yet
#    # looked at other tests:
#    tobuild_ci(client.nudge_test client-interface/nudge_test.runall "" "" "")
#    #tobuild_ci_runall(client.pc-check client-interface/pc-check.runall "" "" "")
#    if (NOT X64)
#      # FIXME i#16, does not work in Win-X64 because of inline assembly code.
#      tobuild_ci(client.cbr-retarget client-interface/cbr-retarget.c "" "" "")
#########################    endif (NOT X64)

#    add_exe(client.fibers client-interface/fibers.c)
#    torunonly_ci(client.fibers client.fibers client.events_cpp.dll
#      client-interface/fibers.c "" "" "")

    # i#1866: stress our private loader
#    tobuild_ci(client.loader client-interface/loader.c "" "" "")
#    target_link_libraries(client.loader.dll "shlwapi")

#    tobuild_ci(client.winxfer client-interface/winxfer.c "" "" "")
#  endif (UNIX)
#  if (X86) # FIXME i#1551, i#1569: port asm to ARM and AArch64
#    tobuild_ci(client.file_io client-interface/file_io.c
#      "${CMAKE_CURRENT_SOURCE_DIR}/client-interface/file_io_data.txt" "" "")
#    if (DEBUG) # FIXME i#1806: fails in release; also in OSX list below.
#      # we add custom option to flush test based on dr ops in torun_ci()
#      tobuild_ci(client.flush client-interface/flush.c "" "" "")
#    endif ()
#    tobuild_ci(client.thread client-interface/thread.c "-paramx -paramy" "" "")
#    tobuild_appdll(client.thread client-interface/thread.c)
#    tobuild_ci(client.strace client-interface/strace.c "" "" "")
#    use_DynamoRIO_extension(client.strace.dll drmgr)
#  endif (X86)
#  # FIXME: PR 199115 to re-enable fragdel, get some more of the UNIX tests working
#  #tobuild_ci(client.fragdel client-interface/fragdel.c "" "" "")
#  if (PROGRAM_SHEPHERDING)
#    tobuild_ci(client.security client-interface/security.c "" "-security_api" "")
#  endif (PROGRAM_SHEPHERDING)
#  if (NOT ARM) # FIXME i#1551: fix bugs on ARM
#    tobuild_ci(client.truncate client-interface/truncate.c "" "" "")
#    if (WIN32)
#      set(client.truncate.thread-churn-1_expectbase "truncate.thread-churn")
#      torunonly_ci(client.truncate.thread-churn-1 win32.threadchurn client.truncate.dll
#        client-interface/truncate.c "1" "" "")
#      set(client.truncate.thread-churn-2_expectbase "truncate.thread-churn")
#      torunonly_ci(client.truncate.thread-churn-2 win32.threadchurn client.truncate.dll
#        client-interface/truncate.c "2" "" "")
#    endif (WIN32)
#    if (NOT AARCH64) # FIXME i#1569: get working on AArch64
#      tobuild_ci(client.dr_options client-interface/dr_options.c
#        "" "-native_exec_list foo.dll,bar.dll -opt_cleancall 3 -thread_private" "")
#    endif (NOT AARCH64)
#  endif (NOT ARM)
#  tobuild_ci(client.unregister client-interface/unregister.c "" "" "")
#  if (NOT ARM) # FIXME i#2094: implement cleancall optimizations on ARM
#    tobuild_ci(client.cleancall-opt-1 client-interface/cleancall-opt-1.c "" "-opt_cleancall 1" "")
#    tobuild_ci(client.inline client-interface/inline.c "" "-opt_cleancall 3" "")
#    if (CMAKE_COMPILER_IS_CLANG)
#      optimize(client.inline.dll)
#    endif ()
#  endif (NOT ARM)
#  if (NOT ANDROID) # XXX i#1874: get working on Android
#    tobuild_ci(client.null_instrument client-interface/null_instrument.c "" "" "")
#    tobuild_appdll(client.null_instrument client-interface/null_instrument.c)
#    target_link_libraries(client.null_instrument client.null_instrument.appdll)
#    # We want rpath on Linux so we can load the appdll.
#    set_target_properties(client.null_instrument PROPERTIES SKIP_BUILD_RPATH OFF)
#####  endif ()

  # Test DR as a static library
  # XXX i#1996: not fully supported on Android yet
  # XXX i#1997: not fully supported on Mac yet
  # XXX i#1551: start/stop API NYI on ARM
  # XXX i#1569: start/stop API NYI on AArch64

# We'll want the latest CTest (2.6.4) so we can use the -W parameter
# for wider test name fields as ours include the runtime options and
# app name.

function(ops2testprefix prefix ops)
  string(REGEX REPLACE " [^-][^ ]*" "" name "${ops}")
  string(REGEX REPLACE " +-" "," name "${name}")
  string(REGEX REPLACE "^-" "" name "${name}")
  string(STRIP "${name}" name)
  # want it grouped by run, not by test, and it auto-alpha-sorts
  set(${prefix} "${name}|" PARENT_SCOPE)
endfunction(ops2testprefix)

function(run2testname testname ops test)
  ops2testprefix(prefix "${ops}")
  # want it grouped by run, not by test, and it auto-alpha-sorts
  set(${testname} "${prefix}${test}" PARENT_SCOPE)
endfunction(run2testname)

foreach (run ${run_list})
  set(enabled OFF)

  string(REGEX MATCHALL "^SHORT::" is_short "${run}")
  string(REGEX REPLACE "^SHORT::" "" run "${run}")
  if (is_short OR TEST_LONG)
    set(enabled ON)
  endif (is_short OR TEST_LONG)

  string(REGEX MATCHALL "^DEBUG::" is_debug "${run}")
  string(REGEX REPLACE "^DEBUG::" "" run "${run}")
  if (is_debug AND NOT DEBUG)
    set(enabled OFF)
  endif (is_debug AND NOT DEBUG)

  string(REGEX MATCHALL "^X86::" is_x86 "${run}")
  string(REGEX REPLACE "^X86::" "" run "${run}")
  if (is_x86 AND NOT X86)
    set(enabled OFF)
  endif ()

  string(REGEX MATCHALL "^WIN::" is_win "${run}")
  string(REGEX REPLACE "^WIN::" "" run "${run}")
  if (is_win AND UNIX)
    set(enabled OFF)
  endif (is_win AND UNIX)

  string(REGEX MATCHALL "^LIN::" is_lin "${run}")
  string(REGEX REPLACE "^LIN::" "" run "${run}")
  if (is_lin AND WIN32)
    set(enabled OFF)
  endif (is_lin AND WIN32)

  if (enabled)

    string(REGEX MATCHALL "^ONLY::.*::" regex "${run}")
    string(REGEX REPLACE "^ONLY::.*::" "" run "${run}")
    string(REGEX REPLACE "^ONLY::(.*)::" "\\1" regex "${regex}")

    if (NOT is_short)
      # FIXME i#1807: fix the failing long suite tests so we can enable them once again
      # in our nightly regression runs.  For now we disable the extra tests (but
      # we leave the extra builds selected in runsuite.cmake) by throttling
      # all long tests to only run ^common.
      set(regex "^common")
    endif ()

    foreach (test ${testlist_normal})
      if (NOT regex OR "${test}" MATCHES "${regex}")
        run2testname(test_name "${run}" ${test})
        torun_normal("${test_name}" ${test} "${run}")
      endif (NOT regex OR "${test}" MATCHES "${regex}")
    endforeach (test)
    foreach (test ${testlist_ci})
      if (NOT regex OR "${test}" MATCHES "${regex}")
        run2testname(test_name "${run}" ${test})
        torun_ci("${test_name}" ${test} "${run}")
      endif (NOT regex OR "${test}" MATCHES "${regex}")
    endforeach (test)
    foreach (test ${testlist_api})
      if (NOT regex OR "${test}" MATCHES "${regex}")
        run2testname(test_name "${run}" ${test})
        torun_api("${test_name}" ${test} "${run}")
      endif (NOT regex OR "${test}" MATCHES "${regex}")
    endforeach (test)
    foreach (test ${testlist_runall})
      if (NOT regex OR "${test}" MATCHES "${regex}")
        run2testname(test_name "${run}" ${test})
        torun_runall("${test_name}" ${test} "${run}")
      endif (NOT regex OR "${test}" MATCHES "${regex}")
    endforeach (test)

  endif (enabled)
endforeach(run)
