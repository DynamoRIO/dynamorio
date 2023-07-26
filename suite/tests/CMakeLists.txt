# **********************************************************
# Copyright (c) 2010-2023 Google, Inc.    All rights reserved.
# Copyright (c) 2009-2010 VMware, Inc.    All rights reserved.
# Copyright (c) 2016-2022 ARM Limited.    All rights reserved.
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
cmake_minimum_required(VERSION 3.7)

include(../../make/policies.cmake NO_POLICY_SCOPE)
include(../runsuite_common_pre.cmake) # For _DR_set_VS_bitwidth_env_vars.

option(RUN_LONG_SUITE "run long set of tests")
# We have some tests that need sudo, which is problematic if it requires
# an interactive password.  Thus, we disable unless explicitly turned on.
option(RUN_SUDO_TESTS "run tests that require sudo")
option(SKIP_FLAKY_TESTS "do not run tests named *_FLAKY")
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

set(main_run_list
  # our main configuration
  "SHORT::-code_api"
  # no_intercept_all_signals tests.
  # TODO i#5261: Remove this suite when -intercept_all_signals is deprecated.
  "SHORT::LIN::ONLY::sigaction$::-no_code_api -no_intercept_all_signals"
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
  # XXX i#3556: NYI on Windows and Mac (and not supported on 32-bit).
  "SHORT::X64::LIN::ONLY::drcache.*\\.simple$|selfmod2|racesys|reachability|linux.*fork|file_io|loglevel$::-code_api -satisfy_w_xor_x"
  # maybe this should be SHORT as -coarse_units will eventually be the default?
  "X86::-code_api -opt_memory"       # i#1575: ARM -coarse_units NYI
  "X86::-code_api -opt_speed"        # i#1551: ARM indcall2direct NYI
  "X86::-code_api -thread_private"   # i#1884: ARM thread_private NYI
  "-code_api -disable_traces"
  "X86::-code_api -thread_private -disable_traces" # i#1884: ARM thread_private NYI
  # i#1807: -prof_pcs fails on Windows currently.
  "LIN::-prof_pcs"
  "X86::LIN::-prof_pcs -thread_private"                 # i#1884: ARM thread_private NYI
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
  # i#1807: This Windows suite fails currently.
  # "WIN::ONLY::^runall::-desktop -coarse_freeze_at_exit"
  # run again so each must use the merged pcaches
  # FIXME: order is alpha: need unique name sorted next
  # i#1807: This Windows suite fails currently.
  # "WIN::ONLY::^runall::-desktop -coarse_freeze_at_exit"

  # limit on shared cache size
  "ONLY::^(runall|${osname})::-finite_shared_bb_cache -cache_shared_bb_regen 80"
  "ONLY::^(runall|${osname})::-finite_shared_trace_cache -cache_shared_trace_regen 80"
  )

if (CALLPROF AND TEST_SUITE)
  set(run_list "ONLY::^common::")
else ()
  if (TEST_SUITE)
    # We don't redo all tests for internal vs external, but we do for debug vs release
    if (DEBUG OR NOT INTERNAL)
      set(run_list ${main_run_list})
    else ()
      set(run_list "ONLY::^client::-code_api")
    endif ()
  else (TEST_SUITE)
    set(run_list ${main_run_list})
  endif (TEST_SUITE)
endif (CALLPROF AND TEST_SUITE)

# i#4059: Reduce Appveyor time by reducing the # of tests run for 32-bit.
# This assumes that 32-bit Windows is not as important these days.
# We remove some of the less differentiated tests.
# We still run the full set for merges to master.
if (WIN32 AND NOT X64 AND "$ENV{CI_TRIGGER}" STREQUAL "pull_request")
  set(SKIP_LESS_DIFFERENTIATED_TESTS_FOR_PULL_REQUEST ON)
else ()
  set(SKIP_LESS_DIFFERENTIATED_TESTS_FOR_PULL_REQUEST OFF)
endif ()

# i#4800: Run long test suite on push-to-master events.
if (RUN_LONG_SUITE OR ("$ENV{CI_TRIGGER}" STREQUAL "push" AND
                       "$ENV{CI_BRANCH}" STREQUAL "refs/heads/master"))
  set(TEST_LONG ON)
endif ()

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

  # If we up our cmake minimum to 3.8.2 we can use enable_language(CSharp).
  find_program(CSC csc.exe DOC "Path to CSharp compiler csc.exe")
  if (NOT CSC)
    message(STATUS "csc not found: .NET Windows tests will be disabled")
  endif ()

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

CHECK_C_COMPILER_FLAG("-no-pie" no_pie_avail)

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
# for tools.c to include drlibc.h.
include_directories(AFTER
  ${PROJECT_SOURCE_DIR}/core/drlibc # drlibc.h
  ${PROJECT_SOURCE_DIR}/core/lib) # c_defines.h

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
target_link_libraries(tools drlibc)

if (WIN32)
  set_source_files_properties(${PROJECT_SOURCE_DIR}/core/win32/resources.rc
    PROPERTIES COMPILE_DEFINITIONS "RC_IS_TEST")
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
    # configure_DynamoRIO_global removes unfavorable options for clients,
    # re-adding -std=c++11.
    string(REGEX REPLACE "-std=gnu99" "-std=c++11" cflags "${cflags}")
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
    if (DR_HOST_NOT_TARGET)
      # We just don't support such tests.
      message(FATAL_ERROR "No asm-containing tests supported for host!=target")
    endif ()
    # we rely on the asm rule doing preprocessing for us, so we just make
    # a copy and set the ASM_CODE_ONLY define
    get_filename_component(source_abs "${source}" ABSOLUTE)
    add_split_asm_target("${source_abs}" asm_source gen_asm_tgt
      "_asm" "${asm_defs}" "${asm_deps}")
    set(test_srcs ${test_srcs} ${asm_source})
  endif ("${srccode}" MATCHES "ifndef ASM_CODE_ONLY")

  if (ANNOTATIONS AND "${srccode}" MATCHES "include \"dr_annotations.h\"")
    use_DynamoRIO_annotations(${test} test_srcs)
  endif ()
  if ("${srccode}" MATCHES "include \"test.*annotation.*.h\"")
    use_DynamoRIO_test_annotations(${test} test_srcs)
  endif ()

  add_executable(${test} ${test_srcs})
  if (DEFINED ${test}_outname)
    set_target_properties(${test} PROPERTIES OUTPUT_NAME "${${test}_outname}")
  endif ()
  copy_target_to_device(${test} "${location_suffix}")

  set_target_properties(${test} PROPERTIES
    LINK_FLAGS "${ARCH_DEBUG}"
    # we don't want the default rpath of <builddir>/lib as it makes
    # it hard to use other DR builds w/ these apps
    SKIP_BUILD_RPATH ON)
  if (UNIX)
    # Older CMake would add this for us, but no longer.  We want global syms
    # in .dynsym for all our executables, so our clients can use dr_get_proc_address()
    # to find them.
    append_link_flags(${test} "-rdynamic")
    # This is kind of a hack: perhaps we should add a flag for whether to use libs.
    if (NOT ${source} MATCHES "\\.asm$" AND NOT ${test} MATCHES "static"
        AND NOT ${test} MATCHES "drdecode")
      target_link_libraries(${test} ${libmath} ${libdl})
    endif ()
    if ("${test}" MATCHES "^pthread")
      link_with_pthread(${test})
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
  copy_target_to_device(${test}.dll "${location_suffix}")
  target_link_libraries(${test} ${test}.dll)
  if ("${srccode}" MATCHES "include \"tools\\.h\"")
    target_link_libraries(${test}.dll tools)
  endif ()

  set(testlist_normal ${testlist_normal} ${test} PARENT_SCOPE)
  set(${test}_source ${source} PARENT_SCOPE)
  set(${test}_dr_ops ${dr_ops} PARENT_SCOPE)
endfunction(tobuild_dll)

function(get_client_path client_path_out realclient realexe)
  get_target_path_for_execution(client_path ${realclient} "${location_suffix}")
  set(${client_path_out} "${client_path}" PARENT_SCOPE)
endfunction(get_client_path)

# A shared app for use with multiple ci tests
set(ci_shared_app simple_app)
set(ci_shared_app_src client-interface/${ci_shared_app}.c)
add_exe(${ci_shared_app} ${ci_shared_app_src})

# A shared tiny app.
if (DR_HOST_X86 AND DR_HOST_X64 AND LINUX)
  add_exe(allasm_x86_64 common/allasm_x86_64.asm)
  set_target_properties(allasm_x86_64 PROPERTIES LINKER_LANGUAGE C)
  append_link_flags(allasm_x86_64 "-nostartfiles -nodefaultlibs -static")
endif ()

function(setup_test_client_dll_basics test)
  configure_DynamoRIO_client(${test})
  append_property_string(TARGET ${test} LINK_FLAGS "${ARCH_DEBUG}")
  add_dependencies(${test} api_headers)
  copy_target_to_device(${test} "${location_suffix}")
endfunction (setup_test_client_dll_basics)

# A client interface app with client.
# If ${source} does not exist, ${ci_shared_app} is used as the target app.
# Note that we can't easily have a routine that takes no ops
# and calls this one w/ "" "" "" b/c the PARENT_SCOPE won't make
# it all the way back then: would need to chain.
function(tobuild_ci test source client_ops dr_ops exe_ops)
  string(REGEX REPLACE "\\.(c|cpp)$" ".dll.\\1" client_source "${source}")
  string(REGEX REPLACE "\\.runall$" ".dll.c" client_source "${client_source}")
  if (NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${client_source}")
    string(REGEX REPLACE "\\.c$" ".cpp" client_source "${client_source}")
  endif ()

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
      if (NOT DEFINED ${test}_realtest)
        set(${test}_realtest ${ci_shared_app} PARENT_SCOPE)
      endif ()
    endif ()
  endif ()

  add_library(${test}.dll SHARED ${client_source})
  if (NOT DEFINED ${test}_no_reg_compat AND
      NOT "${source}" MATCHES "cpp\\.cpp")
    # to avoid changing all the REG_ constants we ask for compatibility
    set(DynamoRIO_REG_COMPATIBILITY ON)
  endif ()
  setup_test_client_dll_basics(${test}.dll)
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
  # To avoid changing all the REG_ constants we ask for compatibility.
  if (DEFINED ${test}_no_reg_compat)
    # We try but it's already in the flags so we unset in drwrap-test-detach.cpp.
    set(DynamoRIO_REG_COMPATIBILITY OFF)
  else ()
    set(DynamoRIO_REG_COMPATIBILITY ON)
  endif ()
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
  # Since we're including configure.h we do not want any default defines.
  get_target_property(prop_defs ${test} COMPILE_DEFINITIONS)
  if (NOT prop_defs)
    set(prop_defs "")
  endif ()
  string(REGEX REPLACE "X64" "" prop_defs "${prop_defs}")
  string(REGEX REPLACE "LINUX" "" prop_defs "${prop_defs}")
  string(REGEX REPLACE "WINDOWS" "" prop_defs "${prop_defs}")
  string(REGEX REPLACE "MACOS" "" prop_defs "${prop_defs}")
  string(REGEX REPLACE "CLANG" "" prop_defs "${prop_defs}")
  string(REGEX REPLACE "ANDROID" "" prop_defs "${prop_defs}")
  string(REGEX REPLACE "DR_APP_EXPORTS" "" prop_defs "${prop_defs}")
  string(REGEX REPLACE "DR_HOST_ARM" "" prop_defs "${prop_defs}")
  string(REGEX REPLACE "DR_HOST_AARCH64" "" prop_defs "${prop_defs}")
  string(REGEX REPLACE "DR_HOST_X86" "" prop_defs "${prop_defs}")
  string(REGEX REPLACE "DR_HOST_X64" "" prop_defs "${prop_defs}")
  string(REGEX REPLACE "DR_HOST_RISCV64" "" prop_defs "${prop_defs}")
  string(REGEX REPLACE "DR_HOST_NOT_TARGET" "" prop_defs "${prop_defs}")
  # Append to properties set by configure_DynamoRIO_standalone.
  get_target_property(prop_cflags ${test} COMPILE_FLAGS)
  if (NOT prop_cflags)
    set(prop_cflags "")
  endif ()
  set(prop_cflags "${prop_cflags} ${extra_flags}")
  set(prop_cflags "${prop_cflags} -DUSE_DYNAMO")
  # FIXME: apparently moving cflags to a target property makes it show up
  # in the cpp run for asm targets, where it doesn't if in global var.
  # This causes compilation failure.  Removing -g3 manually as a workaround.
  string(REGEX REPLACE "-g3" "" prop_cflags "${prop_cflags}")
  get_target_property(prop_lflags ${test} LINK_FLAGS)
  set_target_properties(${test} PROPERTIES
    COMPILE_FLAGS "${prop_cflags}"
    COMPILE_DEFINITIONS "${prop_defs}"
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

    if (ANNOTATIONS AND "${srccode}" MATCHES "include \"dr_annotations.h\"")
      use_DynamoRIO_annotations(${test}.appdll lib_srcs)
    endif ()
    if ("${srccode}" MATCHES "include \"test.*annotation.*.h\"")
      use_DynamoRIO_test_annotations(${test}.appdll lib_srcs)
    endif ()

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
  copy_target_to_device(${test}.appdll "${location_suffix}")
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
  set(${test}_pass_opts_via_env OFF PARENT_SCOPE)
endfunction(torunonly_ci)

function(torunonly_api test realexe source dr_ops exe_ops use_decoder pass_opts_via_env)
  set(testlist_api ${testlist_api} ${test} PARENT_SCOPE)
  if (realexe)
    set(${test}_realtest ${realexe} PARENT_SCOPE)
  endif (realexe)
  set(${test}_source ${source} PARENT_SCOPE)
  set(${test}_dr_ops ${dr_ops} PARENT_SCOPE)
  set(${test}_exe_ops ${exe_ops} PARENT_SCOPE)
  set(${test}_pass_opts_via_env ${pass_opts_via_env} PARENT_SCOPE)
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
        # libutil.drconfig_test needs drconfiglib.
        COMMAND ${CMAKE_COMMAND} ARGS
          -E copy "${MAIN_LIBRARY_OUTPUT_DIRECTORY}/../drconfiglib.dll"
          "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/drconfiglib.dll"
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

# tobuild_api builds an API test which uses the either DR as a dynamic or
# static library, depending on use_static_DR. It is a macro so the
# PARENT_SCOPE in torunonly_api will reach caller of this.
macro(tobuild_api test source dr_ops exe_ops use_decoder use_static_DR pass_opts_via_env)
  add_api_exe(${test} ${source} ${use_decoder} ${use_static_DR})
  torunonly_api(${test} OFF ${source} "${dr_ops}" "${exe_ops}" ${use_decoder}
    ${pass_opts_via_env})
endmacro(tobuild_api)

# run that uses a different build
function(torunonly test realtest source dr_ops exe_ops)
  set(testlist_normal ${testlist_normal} ${test} PARENT_SCOPE)
  set(${test}_realtest ${realtest} PARENT_SCOPE)
  set(${test}_source ${source} PARENT_SCOPE)
  set(${test}_dr_ops ${dr_ops} PARENT_SCOPE)
  set(${test}_exe_ops ${exe_ops} PARENT_SCOPE)
endfunction(torunonly)

# macro so the PARENT_SCOPE in torunonly will reach caller of this
macro(tobuild_csharp test source)
  if (CSC)
    set(exe "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${test}.exe")
    add_custom_target(${test} ALL DEPENDS ${exe})
    if (X64)
      set(platform "x64")
    else ()
      set(platform "x86")
    endif ()
    set(source_winpath "${CMAKE_CURRENT_SOURCE_DIR}/${source}")
    string(REGEX REPLACE "/" "\\\\" source_winpath "${source_winpath}")
    add_custom_command(OUTPUT ${exe} DEPENDS ${source}
      COMMAND ${CSC} ARGS /debug /platform:${platform} /t:exe /out:${exe}
      "${source_winpath}" VERBATIM)
    torunonly(${test} ${exe} ${source} "" "")
  endif ()
endmacro(tobuild_csharp)

macro(append_pure_asm_app_link_flags target)
  set_target_properties(${target} PROPERTIES LINKER_LANGUAGE C)
  append_link_flags(${target} "-nostartfiles -nodefaultlibs -static")
  if (APPLE)
    append_property_string(TARGET ${target} LINK_FLAGS "-e _start")
  else ()
    append_property_string(TARGET ${target} LINK_FLAGS
      # The default value of CMAKE_SHARED_LIBRARY_LINK_C_FLAGS has -rdynamic, which
      # with some tool chains (such as SUSE GCC 6.2) results in an executable that
      # is not properly static. Adding --no-export-dynamic avoids the problem.
      "-Wl,--no-export-dynamic")
  endif ()
endmacro(append_pure_asm_app_link_flags)

function(tobind target)
  if (BIND_EXECUTABLE)
    add_custom_command(TARGET ${target}
      POST_BUILD
      COMMAND ${BIND_EXECUTABLE}
      ARGS -u $<TARGET_FILE:${target}>
      VERBATIM # recommended: p260
      )
  else (BIND_EXECUTABLE)
    add_custom_command(TARGET ${target}
      POST_BUILD
      COMMAND ${CMAKE_LINKER}
      ARGS /edit /bind $<TARGET_FILE:${target}>
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
    add_custom_command(TARGET ${target}
      POST_BUILD
      COMMAND ${CHCON}
      ARGS -t ${type} $<TARGET_FILE:${target}>
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
  if (ANNOTATIONS)
    configure_DynamoRIO_annotation_sources("${dr_annotation_test_srcs}")
  endif ()
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
    "[;/\\.!,]"
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
  if (DEFINED ${key}_runavx512)
    set(rundefs "${rundefs} -D__AVX__ -D__AVX512F__")
  elseif (DEFINED ${key}_runavx)
    set(rundefs "${rundefs} -D__AVX__")
  endif ()

  if (DEFINED ${key}_test_sample_client)
    set(rundefs "${rundefs} -DTEST_SAMPLE_CLIENT")
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
if (CMAKE_CROSSCOMPILING AND DEFINED CMAKE_FIND_ROOT_PATH)
  set(dr_test_ops -xarch_root ${CMAKE_FIND_ROOT_PATH} ${dr_test_ops})
endif ()

function(runtest_cmd outcmd outops key native standalone_dr dr_ops_aux separate_script)
  # assumes tools have been built: enforced at top level
  set(dr_ops ${dr_test_ops} ${dr_ops_aux} ${TEST_OPTIONS})

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
  if (CMAKE_CROSSCOMPILING)
    # Everything is slower under emulation.
    math(EXPR timeout "${timeout}*4")
  endif ()

  if (WIN32 OR NOT native)
    # Both Windows and non-native Unix tests now use the native drrun frontend.
    # Since the creation of CTestTestfile.cmake ends up replacing \ with / we
    # can't escape quotes, so we have to use a list variable
    if (standalone_dr OR NOT native)
      get_target_path_for_execution(drrun_path drrun "${location_suffix}")
    endif (standalone_dr OR NOT native)
    # CMake 3.x seems to put double slashes in there (xref i#1559)
    string(REGEX REPLACE "//" "/" drrun_path "${drrun_path}")
    set(cmd "${drrun_path}" -s ${timeout} -quiet)
    prefix_cmd_if_necessary(cmd OFF ${cmd})
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
    get_target_path_for_execution(runstats_path runstats "${location_suffix}")
    set(cmd ${runstats_path} -s ${timeout} -killpg -silent)
    prefix_cmd_if_necessary(cmd OFF ${cmd})
    # We set LD_LIBRARY_PATH for native api/ apps since we're not setting
    # rpath (to make it easier to try different trees)
    if (APPLE)
      set(libvar "DYLD_LIBRARY_PATH")
    else ()
      set(libvar "LD_LIBRARY_PATH")
    endif ()
    set(cmd ${cmd} -env ${libvar}
      "${MAIN_LIBRARY_OUTPUT_DIRECTORY}:${EXT_LIBRARY_OUTPUT_DIRECTORY}:$ENV{${libvar}}")
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
# If pass_opts_via_env is ON, we pass the computed DynamoRIO options as the
# DYNAMORIO_OPTIONS environment variable for the test run.
function(torun test key source native standalone_dr dr_ops exe_ops added_out pass_opts_via_env)
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
    get_target_path_for_execution(exepath ${exe} "${location_suffix}")
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

  if (DEFINED ${key}_sudo)
    set(rundr "sudo;${rundr}")
  endif ()

  if (is_runall)
    # To run the test we use a series of commands in runall.cmake.
    file(READ "${testpath}/${srcbase}.runall" runall)
    if (UNIX)
      # We always run infloop.
      get_target_path_for_execution(app_path linux.infloop "${location_suffix}")
      if ("${runall}" MATCHES "<attach>")
        # Add a starting message with -v, and avoid mprotect for now (i#38: mprotect
        # is failing).
        set(exe_ops "${exe_ops};-v;-attach")
      endif ()
      if ("${runall}" MATCHES "<block>")
        set(exe_ops "${exe_ops};-block")
      endif ()
    else ()
      if ("${runall}" MATCHES "<attach>" OR "${runall}" MATCHES "<detach>")
        # For detach we use attachee to attach firstly, and "-detach" option is used
        # for differentiating between attach and detach.
        if ("${runall}" MATCHES "<detach>")
          set(exe_ops "${exe_ops};-detach")
        endif ()
        # For attach we use attachee, which is identical to infloop, aside for writing
        # a starting message to the output file. This allows us to detect when it is
        # up and running and we can start the attach process.
        get_target_path_for_execution(app_path win32.attachee "${location_suffix}")
      else ()
        # It's easier to run our own infloop, which prints so we know when it's up,
        # than to run notepad or sthg.
        get_target_path_for_execution(app_path win32.infloop "${location_suffix}")
      endif ()
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
    elseif ("${runall}" MATCHES "<attach>")
      # move params to nudge
      string(REGEX MATCH ";-quiet;(.*);--" nudge_arg "${rundr}")
      string(REGEX REPLACE ";--" "" nudge_arg "${nudge_arg}")
      string(PREPEND nudge_arg "<attach>")
      string(REGEX REPLACE ";" "@" nudge_arg "${nudge_arg}")
      # clear client from dr command and run native
      string(REGEX REPLACE ";-quiet;(.*);--" ";-no_inject;--;" rundr "${rundr}")
    elseif ("${runall}" MATCHES "<detach>")
      # Move params to nudge.
      string(REGEX MATCH ";-quiet;(.*);--" nudge_arg "${rundr}")
      string(REGEX REPLACE ";--" "" nudge_arg "${nudge_arg}")
      string(PREPEND nudge_arg "<detach>")
      string(REGEX REPLACE ";" "@" nudge_arg "${nudge_arg}")
      # Clear client from dr command and run native.
      string(REGEX REPLACE ";-quiet;(.*);--" ";-no_inject;--;" rundr "${rundr}")
    endif ()
    set(cmd_with_at ${rundr} ${app_path} ${exe_ops})
    # we pass intra-arg spaces via @@ and inter-arg via @ and ; via !
    # to get around the pain of trying to quote everything just right:
    # much simpler this way.
    string(REGEX REPLACE " " "@@" cmd_with_at "${cmd_with_at}")
    string(REGEX REPLACE ";" "@" cmd_with_at "${cmd_with_at}")
    if (NOT APPLE) # XXX i#1286: implement nudge for MacOS
      if (UNIX)
        get_target_path_for_execution(toolbindir drnudgeunix "${location_suffix}")
      else ()
        get_target_path_for_execution(toolbindir drconfig "${location_suffix}")
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
      -D failok=${${key}_failok}
      -D cmp=${CMAKE_CURRENT_BINARY_DIR}/${expectbase}.expect
      -D code=${${key}_code}
      -D capture=${${key}_runcmp_capture}
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
  elseif (is_runcmp)
    # Runcmp generally doesn't use drrun or runstats so we need a ctest timeout.
    set_tests_properties(${test} PROPERTIES TIMEOUT ${TEST_SECONDS})
  endif ()

  # Though we use drrun and runstats -s timeout parameters, we have
  if (pass_opts_via_env)
    string(REPLACE ";" " " runops_string "${runops}")
    set_tests_properties(${test} PROPERTIES ENVIRONMENT "DYNAMORIO_OPTIONS=${runops_string}")
  endif (pass_opts_via_env)

  if (WIN32)
    # With i#265/PR 486139 we use local one-time config files instead of
    # global reg keys and can thus run multiple instances of the same
    # app name simultaneously so we do not need to mark RUN_SERIAL
  endif (WIN32)

  # A subset of release tests has been added and labeled with RUN_IN_RELEASE. It focuses
  # on Google's use of the drcachesim client. We do not want to extend the Travis
  # regression time by too much and keep the following list concise.
  if (NOT DEBUG AND UNIX AND X64 AND NOT CMAKE_COMPILER_IS_CLANG)
    if (${test} MATCHES "common" OR
        ${test} MATCHES "drcachesim" OR
        ${test} MATCHES "drcacheoff")
      set_tests_properties(${test} PROPERTIES LABELS RUN_IN_RELEASE)
    endif()
  endif ()
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

function(set_properties name test myprefix)
  if (DEFINED ${test}_depends)
    set_property(TEST ${name} APPEND PROPERTY DEPENDS "${myprefix}${${test}_depends}")
  endif (DEFINED ${test}_depends)
  if (DEFINED ${test}_self_serial)
    # We blindly add a dep to every possible cross-cutting same test w/ diff ops.
    # CTest seems ok with a dep on a non-existent test.
    foreach (prefix ${prior_prefixes})
      if (NOT "${prefix}" STREQUAL "${myprefix}")
        set_property(TEST ${name} APPEND PROPERTY DEPENDS "${prefix}${test}")
      endif ()
    endforeach ()
  endif ()
  if (DEFINED ${test}_env)
    set_property(TEST ${name} APPEND PROPERTY ENVIRONMENT "${myprefix}${${test}_env}")
  endif ()
endfunction ()

function(torun_normal name test ops)
  strings2lists(${test} ops)
  torun("${name}" ${test} ${${test}_source} OFF OFF "${${test}_dr_ops};${ops}"
    "${${test}_exe_ops}" added OFF)
  ops2testprefix(myprefix "${ops}")
  set_properties(${name} ${test} ${myprefix})
endfunction(torun_normal)

function(torun_ci name test ops)
  ops2testprefix(myprefix "${ops}")
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
      "${${test}_exe_ops}" added OFF)
  else ()
    # simple client
    torun("${name}" ${test} ${${test}_source} OFF OFF
      "${${test}_dr_ops};${ops};-c;${${test}_client_path};${cli_ops}"
      "${${test}_exe_ops}" added OFF)
  endif ()
  if (added)
    set_properties(${name} ${test} ${myprefix})
  endif (added)
endfunction(torun_ci)

function(torun_api name test ops)
  strings2lists(${test} ops)
  torun("${name}" ${test} ${${test}_source} ON ${${test}_standalone_dr}
    "${${test}_dr_ops};${ops}" "${${test}_exe_ops}" added ${${test}_pass_opts_via_env})
endfunction(torun_api)

function(torunonly_native name realexe expect source exe_ops)
  strings2lists(${name} ops)
  set(${name}_expectbase ${expect})
  set(${name}_realtest ${realexe})
  if (NOT DEFINED ${test}_timeout)
    # We're not going through drrun with its timeout, so set the ctest timeout.
    set(${test}_timeout ${TEST_SECONDS})
  endif ()
  torun(${name} ${name} ${source} ON OFF "" "${exe_ops}" added OFF)
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

# Some of our tests were created for /MT which was the top-level flag even
# in debug build until recently, so we go back to it to avoid breaking them.
function(use_MT_not_MTd source_file)
  if (WIN32)
    # Appending to the target flags goes in too early: we need source to
    # override the /MTd from the top level.
    append_property_string(SOURCE ${source_file} COMPILE_FLAGS "/MT /DNO_DBG_CRT")
  endif ()
endfunction()

function(use_MD_not_MTd source_file)
  if (WIN32)
    # Appending to the target flags goes in too early: we need source to
    # override the /MTd from the top level.
    append_property_string(SOURCE ${source_file} COMPILE_FLAGS "/MD")
  endif ()
endfunction()

# optimize the target
function(optimize target)
  if (UNIX)
    append_property_string(TARGET ${target} COMPILE_FLAGS "-O3")
  else ()
    append_property_string(TARGET ${target} COMPILE_FLAGS "/Ox /GL")
  endif (UNIX)
endfunction(optimize)

macro(set_avx_flags target)
  if (proc_supports_avx512)
    if (TARGET ${target}) # Support calling on non-exe target.
      append_property_string(TARGET ${target} COMPILE_FLAGS "${CFLAGS_AVX512}")
    endif ()
    set(${target}_runavx512 1)
  elseif (proc_supports_avx)
    if (TARGET ${target}) # Support calling on non-exe target.
      append_property_string(TARGET ${target} COMPILE_FLAGS "${CFLAGS_AVX}")
    endif ()
    set(${target}_runavx 1)
  endif ()
endmacro(set_avx_flags)

###########################################################################

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

function(is_test_enabled_by_name output name regex)
  set(enabled OFF)
  if (NOT SKIP_FLAKY_TESTS OR NOT "${name}" MATCHES "_FLAKY")
    if (TEST_LONG OR NOT "${name}" MATCHES "_LONG")
      if (RUN_SUDO_TESTS OR NOT "${name}" MATCHES "_SUDO")
        if (NOT regex OR "${name}" MATCHES "${regex}")
          if (SKIP_LESS_DIFFERENTIATED_TESTS_FOR_PULL_REQUEST AND
              # See above: for particular builds we skip some of the similar tests from
              # some of the test groups.
              (("${name}" MATCHES "tool.drcache" AND
                  NOT "${name}" MATCHES "(simple|invariants|burst_malloc|burst_replaceall)") OR
                ("${name}" MATCHES "tool.drcpusim" AND
                  NOT "${name}" MATCHES "Pentium3") OR
                ("${name}" MATCHES "tool.[^d]" AND
                  NOT "${name}" MATCHES "(basic_counts|reuse_offline)") OR
                # instrcalls is the slowest of the samples.
                "${name}" MATCHES "sample.instrcalls" OR
                ("${name}" MATCHES "client.annotation-" AND
                  NOT "${name}" MATCHES "(-opt|tiny-bb)")))
            set(enabled OFF)
          else ()
            set(enabled ON)
          endif ()
        endif ()
      endif ()
    endif ()
  endif ()
  set(${output} ${enabled} PARENT_SCOPE)
endfunction ()

macro (set_up_test_lists)
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

    string(REGEX MATCHALL "^X64::" is_x64 "${run}")
    string(REGEX REPLACE "^X64::" "" run "${run}")
    if (is_x64 AND NOT X64)
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

      ops2testprefix(prefix "${run}")
      list(FIND prior_prefixes "${prefix}" index)
      if (${index} EQUAL -1)
        list(APPEND prior_prefixes "${prefix}")
      endif ()
      foreach (test ${testlist_normal})
        is_test_enabled_by_name(name_ok ${test} "${regex}")
        if (name_ok)
          run2testname(test_name "${run}" ${test})
          torun_normal("${test_name}" ${test} "${run}")
        endif ()
      endforeach (test)
      foreach (test ${testlist_ci})
        is_test_enabled_by_name(name_ok ${test} "${regex}")
        if (name_ok)
          run2testname(test_name "${run}" ${test})
          torun_ci("${test_name}" ${test} "${run}")
        endif ()
      endforeach (test)
      foreach (test ${testlist_api})
        is_test_enabled_by_name(name_ok ${test} "${regex}")
        if (name_ok)
          run2testname(test_name "${run}" ${test})
          torun_api("${test_name}" ${test} "${run}")
        endif ()
      endforeach (test)
      foreach (test ${testlist_runall})
        is_test_enabled_by_name(name_ok ${test} "${regex}")
        if (name_ok)
          run2testname(test_name "${run}" ${test})
          torun_runall("${test_name}" ${test} "${run}")
        endif ()
      endforeach (test)

    endif (enabled)
  endforeach(run)
endmacro ()

###########################################################################
# tests

# This step takes a while so we provide a status message:
message(STATUS "Processing tests and generating expected output patterns")

# Tests enabled when DR_HOST_NOT_TARGET go first:
if (ARM)
  set(sfx "arm")
elseif (AARCH64)
  set(sfx "aarch64")
elseif (RISCV64)
  set(sfx "riscv64")
else ()
  set(sfx "x86")
endif ()
if (NOT ANDROID)
  # XXX i#1874: get working on Android
  tobuild_api(api.ir api/ir_${sfx}.c "" "" OFF OFF OFF)
  if (AARCH64 OR RISCV64)
    # The ir_aarch64.expect file is too large for CMake's regexes.
    set(api.ir_runcmp "${CMAKE_CURRENT_SOURCE_DIR}/runcmp.cmake")
    set(api.ir_runcmp_capture "stderr")
  endif ()
  # XXX i#1686: add ARM's headers too once they are all created
  if (NOT ARM AND NOT AARCH64 AND NOT RISCV64 AND "${CMAKE_GENERATOR}" MATCHES "Unix Makefiles")
    # CMake's Unix Makefiles dependence analysis doesn't run the preprocessor
    # and so doesn't find headers named via macros
    # (http://www.cmake.org/Bug/view.php?id=13718)
    set(api_ir_headers # must be full paths:
      ${CMAKE_CURRENT_SOURCE_DIR}/api/ir_${sfx}_0args.h
      ${CMAKE_CURRENT_SOURCE_DIR}/api/ir_${sfx}_1args.h
      ${CMAKE_CURRENT_SOURCE_DIR}/api/ir_${sfx}_2args.h
      ${CMAKE_CURRENT_SOURCE_DIR}/api/ir_${sfx}_2args_mm.h
      ${CMAKE_CURRENT_SOURCE_DIR}/api/ir_${sfx}_3args.h
      ${CMAKE_CURRENT_SOURCE_DIR}/api/ir_${sfx}_4args.h
      # The following headers are x86 specific.
      ${CMAKE_CURRENT_SOURCE_DIR}/api/ir_${sfx}_3args_avx.h
      ${CMAKE_CURRENT_SOURCE_DIR}/api/ir_${sfx}_2args_avx512_evex.h
      ${CMAKE_CURRENT_SOURCE_DIR}/api/ir_${sfx}_2args_avx512_vex.h
      ${CMAKE_CURRENT_SOURCE_DIR}/api/ir_${sfx}_3args_avx512_evex.h
      ${CMAKE_CURRENT_SOURCE_DIR}/api/ir_${sfx}_3args_avx512_evex_mask.h
      ${CMAKE_CURRENT_SOURCE_DIR}/api/ir_${sfx}_3args_avx512_vex.h
      ${CMAKE_CURRENT_SOURCE_DIR}/api/ir_${sfx}_4args_avx512_evex_mask_A.h
      ${CMAKE_CURRENT_SOURCE_DIR}/api/ir_${sfx}_4args_avx512_evex_mask_B.h
      ${CMAKE_CURRENT_SOURCE_DIR}/api/ir_${sfx}_4args_avx512_evex_mask_C.h
      ${CMAKE_CURRENT_SOURCE_DIR}/api/ir_${sfx}_5args_avx512_evex_mask.h)
    append_property_string(SOURCE api/ir_${sfx}.c OBJECT_DEPENDS "${api_ir_headers}")
  endif ()

  if (AARCH64)
    tobuild_api(api.ir_negative api/ir_aarch64_negative.c "" "" OFF OFF OFF)
    tobuild_api(api.ir_v81 api/ir_aarch64_v81.c "" "" OFF OFF OFF)
    tobuild_api(api.ir_v82 api/ir_aarch64_v82.c "" "" OFF OFF OFF)
    tobuild_api(api.ir_v83 api/ir_aarch64_v83.c "" "" OFF OFF OFF)
    tobuild_api(api.ir_v84 api/ir_aarch64_v84.c "" "" OFF OFF OFF)
    tobuild_api(api.ir_v86 api/ir_aarch64_v86.c "" "" OFF OFF OFF)
    tobuild_api(api.ir_sve api/ir_aarch64_sve.c "" "" OFF OFF OFF)
    tobuild_api(api.ir_sve2 api/ir_aarch64_sve2.c "" "" OFF OFF OFF)
  endif (AARCH64)
endif ()

# test static decoder library
tobuild_api(api.ir-static api/ir_${sfx}.c "" "" ON OFF OFF)
if (AARCH64 OR RISCV64)
  # The ir_aarch64.expect file is too large for CMake's regexes.
  set(api.ir-static_runcmp "${CMAKE_CURRENT_SOURCE_DIR}/runcmp.cmake")
  set(api.ir-static_runcmp_capture "stderr")
endif ()

if (ARM)
  if (NOT ANDROID) # XXX i#1874: get working on Android
    tobuild_api(api.drdecode api/drdecode_arm.c "" "" ON OFF OFF)
  endif ()
elseif (AARCH64)
  tobuild_api(api.drdecode api/drdecode_aarch64.c "" "" ON OFF OFF)
else ()
  if (NOT RISCV64) # TODO i#3544: Port tests to RISC-V 64
    tobuild_api(api.drdecode api/drdecode_x86.c "" "" ON OFF OFF)
  endif (NOT RISCV64)
endif ()

if (BUILD_CLIENTS)
  ###########################################################################
  # drdisas tests
  # XXX: We should try to export or share the test list, filtering, matching,
  # and other code here to support tests being declared locally rather than
  # in one central place.
  if (AARCH64)
    set(drdisas_args "-vl" "256" "00000000" "f94017a0" "a9be7bfd" "e58057a1" "85865e6b" "88")
  elseif (ARM)
    set(drdisas_args "6813" "f243" "42c8" "b2db" "88")
  else ()
    set(drdisas_args "-syntax" "dr" "66" "90" "c4" "e2" "65" "90" "14" "80" "88")
  endif ()
  torunonly_api(tool.drdisas drdisas
    "../../clients/drdisas/test_simple.template" "" "${drdisas_args}" ON OFF)
  if (ARM)
    torunonly_api(tool.drdisas_thumb drdisas
      # Pass -thumb as a "DR op" to define it in the .template file.
      "../../clients/drdisas/test_simple.template"
      "-thumb" "-mode;thumb;${drdisas_args}" ON OFF)
  endif ()
  # Test syntax
  # XXX i#:4021: ARM/AARCH64 not yet supported.
  if (X86)
      set(drdisas_args "66" "90" "c4" "e2" "65" "90" "14" "80")
      torunonly_api(tool.drdisas_syntax drdisas
          "../../clients/drdisas/test_syntax.template" "" "${drdisas_args}" ON OFF)
  endif ()
endif ()

if (DR_HOST_NOT_TARGET)
  # i#1684,i#4318: Test postprocessing and analysis with an alternate binary directory.
  # Limited to UNIX because the checked-in modules.log is in UNIX format
  # (Windows needs extra fields per library).
  # Limited to ZLIB_FOUND because we test gzipped .raw files here as well.
  if (BUILD_CLIENTS AND UNIX AND ZLIB_FOUND)
    # We make a writable copy so we can both write the final trace there and run our
    # analyzer in a single command line.
    set(srcdir
      ${PROJECT_SOURCE_DIR}/clients/drcachesim/tests/drmemtrace.threadsig.aarch64.raw)
    set(locdir ${PROJECT_BINARY_DIR}/drmemtrace.threadsig.aarch64)
    file(MAKE_DIRECTORY ${locdir})
    file(COPY ${srcdir}/raw DESTINATION ${locdir}/)
    get_target_path_for_execution(drcachesim_path drcachesim "${location_suffix}")
    prefix_cmd_if_necessary(drcachesim_path ON ${drcachesim_path})
    torunonly_api(tool.drcacheoff.altbindir "${drcachesim_path}"
      "offline-altbindir.c" ""
      "-indir;${locdir};-simulator_type;opcode_mix;-alt_module_dir;${srcdir};-module_file;${locdir}/raw/modules.log"
      OFF OFF)
    set(tool.drcacheoff.altbindir_basedir
      "${PROJECT_SOURCE_DIR}/clients/drcachesim/tests")
  endif ()

  # The tests are not ported to build with host!=target.  Most will never work there,
  # so rather than spend effort on making everything build when we can only enable
  # a few tests, we simply enable the few tests here and return, skipping the rest of
  # the file and avoiding a giant if().
  set_up_test_lists()
  return ()
endif (DR_HOST_NOT_TARGET)

# Make sure we test running a path with spaces (to avoid regressions like i#2538).
set(common.broadfun_outname "common.broadfun spaces")
tobuild(common.broadfun common/broadfun.c)
if (DEBUG)
  torunonly(common.logstderr common.broadfun common/logstderr.c
    "-log_to_stderr -loglevel 1 -logmask 2" "")
endif ()
if (NOT ANDROID) # We do not support -no_early_inject on Android (i#1873).
  tobuild_ops(common.fib common/fib.c "-no_early_inject" "")
endif ()
if (X86) # TODO i#1551, i#1569: port asm to ARM and AArch64
  tobuild(common.decode-bad common/decode-bad.c)
  tobuild(common.decode common/decode.c)
  if (proc_supports_avx512)
    set_target_properties(common.decode PROPERTIES COMPILE_FLAGS "${CFLAGS_AVX512}")
  endif ()
endif (X86)
if (X86)
  torunonly(common.decode-stress common.decode common/decode.c
    "-stress_recreate_state" "")
elseif (AARCHXX)
  # The common.decode test is very x86-centric and may never be ported.
  # For now we simply run a simple app which still exercises quite a bit.
  torunonly(common.broadfun-stress common.broadfun common/broadfun.c
    "-stress_recreate_state" "")
endif ()

if (X86)
  set(control_flags "eflags")
elseif (AARCHXX)
  set(control_flags "nzcv")
endif ()
if (NOT RISCV64) # TODO i#3544: Port tests to RISC-V 64
  tobuild(common.${control_flags} common/${control_flags}.c)
endif ()

if (X86) # FIXME i#1551, i#1569: port asm to ARM and AArch64
  tobuild(common.floatpc common/floatpc.c)
  torunonly(common.floatpc_xl8all common.floatpc common/floatpc.c "-translate_fpu_pc" "")
endif (X86)
tobuild(common.getretaddr common/getretaddr.c)

if (X86) # FIXME i#1551, i#1569: port asm to ARM and AArch64
  # XXX i#1287: implement for MacOS
  # We do not support -no_early_inject on Android (i#1873).
  # FIXME i#1799: clang does not support "asm goto".
  if (NOT APPLE AND NOT ANDROID AND ANNOTATIONS AND NOT CMAKE_COMPILER_IS_CLANG)
    # XXX i#1651: we don't support -native_exec combined with -early_inject so
    # for now we pass -no_early_inject to all of these tests.
    tobuild_appdll(common.nativeexec common/nativeexec.c)
    DynamoRIO_get_full_path(native_dll_name common.nativeexec.appdll "${location_suffix}")
    get_filename_component(native_dll_name "${native_dll_name}" NAME)
    tobuild_ops(common.nativeexec common/nativeexec.c
      "-no_early_inject -native_exec_list ${native_dll_name}" "")
    target_link_libraries(common.nativeexec common.nativeexec.appdll)
    # We want rpath on Linux so we can load the appdll.
    set_target_properties(common.nativeexec PROPERTIES SKIP_BUILD_RPATH OFF)
    # For common.nativeexec takeover tests, we do not support PIE. Newer (unix-) distri-
    # butions with toolchains may enable PIE by default. In order to be able take over the
    # "PLT style" dll call (test with executable in native exec list), we expect the call
    # to jump to a PLT stub first. However with PIE enabled, the compiler will generate a
    # rip-relative gotplt table call instead.
    if (UNIX AND no_pie_avail)
      set_source_files_properties(common/nativeexec.c PROPERTIES COMPILE_FLAGS
        "${CMAKE_C_FLAGS} -fno-pie")
      append_link_flags(common.nativeexec "-no-pie")
    endif()
    if (UNIX)
      # FIXME i#978: Windows support NYI
      torunonly(common.nativeexec_retakeover common.nativeexec common/nativeexec.c
        "-no_early_inject -native_exec_list ${native_dll_name} -native_exec_retakeover"
        "")
      torunonly(common.nativeexec_exe common.nativeexec
        common/nativeexec_exenative.c
        "-no_early_inject -native_exec_list common.nativeexec -native_exec_retakeover" "")
      torunonly(common.nativeexec_bindnow common.nativeexec common/nativeexec_exenative.c
        "-no_early_inject -native_exec_list common.nativeexec -native_exec_retakeover"
        "-bind_now")
      # XXX: the next 3 tests, and perhaps all of these native_exec tests,
      # really want a mode with -no_private_loader -no_mangle_app_seg.  With
      # our reluctance to add lib deps back to DR, that's going to require a
      # sepaparate build.  Until someone really focuses on hybrid mode again
      # (in which case a new build param should be added to re-enable libdl),
      # we leave the tests here as regression tests but w/ private loader in place.
      torunonly(common.nativeexec_retakeover_opt common.nativeexec common/nativeexec.c
        "-no_early_inject -native_exec_list ${native_dll_name} -native_exec_retakeover  -native_exec_opt -no_kstats" "")
      # No swap TLS and no kstats for optimized native_exec
      torunonly(common.nativeexec_exe_opt common.nativeexec common/nativeexec_exenative.c
        "-no_early_inject -native_exec_list common.nativeexec -native_exec_retakeover -native_exec_opt -no_kstats" "")
      torunonly(common.nativeexec_bindnow_opt common.nativeexec
        common/nativeexec_exenative.c
        "-no_early_inject -native_exec_list common.nativeexec -native_exec_retakeover -native_exec_opt -no_kstats" "-bind_now")
    endif ()
  endif ()

  # App seg mangling makes dstack test fail, and our default build does not
  # support disabling such mangling, so this test is Windows-only.
  if (WIN32)
    tobuild_ops(common.protect-dstack common/protect-dstack.c
      "-no_mangle_app_seg" "")
  endif ()
endif (X86)

if (NOT ANDROID) # FIXME i#1874: failing on Android
  tobuild(common.segfault common/segfault.c)
endif ()

# PR 217255: these 4 removed to shorten the regression suite since not
# adding much value in terms of testing corner cases
#tobuild(common.hello common/hello.c)
#tobuild(common.conflict common/conflict.c)
#tobuild(common.ops common/ops.c)
#tobuild(common.recurse common/recurse.c)

# XXX i#1874: Android loader won't run our static asm apps
if (DR_HOST_ARM AND NOT ANDROID)
  tobuild_ops(common.allasm_thumb common/allasm_thumb.asm
    # set small -max_bb_instrs to test bb_safe_to_stop()
    "-early_inject -max_bb_instrs 6" "")
  set_target_properties(common.allasm_thumb PROPERTIES LINKER_LANGUAGE C)
  append_link_flags(common.allasm_thumb
    "-nostartfiles -nodefaultlibs -static")

  tobuild_ops(common.allasm_arm common/allasm_arm.asm "-early_inject" "")
  set_target_properties(common.allasm_arm PROPERTIES LINKER_LANGUAGE C)
  append_link_flags(common.allasm_arm "-nostartfiles -nodefaultlibs -static")
endif ()

if (DR_HOST_AARCH64)
  if (NOT APPLE) # TODO i#5383: Port to Mac M1.
    tobuild_ops(common.allasm_aarch64_isa common/allasm_aarch64_isa.asm
      "-early_inject" "") # FIXME i#1569: -enable_traces here
    set_target_properties(common.allasm_aarch64_isa PROPERTIES LINKER_LANGUAGE C)
    append_link_flags(common.allasm_aarch64_isa
      "-nostartfiles -nodefaultlibs -static")
  endif ()

  tobuild_ops(common.allasm_aarch64_cache common/allasm_aarch64_cache.asm
    "-early_inject" "")
  append_pure_asm_app_link_flags(common.allasm_aarch64_cache)

  add_exe(allasm_aarch64_prefetch
    ${PROJECT_SOURCE_DIR}/clients/drcachesim/tests/allasm_aarch64_prefetch.asm
    "-early_inject" "")
  append_pure_asm_app_link_flags(allasm_aarch64_prefetch)

  add_exe(allasm_aarch64_flush
    ${PROJECT_SOURCE_DIR}/clients/drcachesim/tests/allasm_aarch64_flush.asm "" "")
  append_pure_asm_app_link_flags(allasm_aarch64_flush)
endif ()

if (DEBUG)
  # A high-loglevel test.
  if (DR_HOST_X86 AND DR_HOST_X64)
    # Unfortunately it's not possible to create a tiny app on Windows, and hello,world
    # at -loglevel 19 produces 11GB of data and takes >17 minutes to run (vs <10MB and
    # <1s for Linux).  We live w/o a regression test on Windows for high loglevels.
    if (LINUX)
      torunonly(common.loglevel allasm_x86_64 common/allasm_x86_64.asm "-loglevel 19" "")
    endif ()
  elseif (DR_HOST_AARCH64)
    torunonly(common.loglevel common.allasm_aarch64_cache
      common/allasm_aarch64_cache.asm "-loglevel 19" "")
  elseif (DR_HOST_ARM AND NOT ANDROID)
    torunonly(common.loglevel common.allasm_thumb
      common/allasm_thumb.asm "-loglevel 19" "")
  endif ()
endif ()

if (DEBUG)
  torunonly(common.no_global_stats_LONG ${ci_shared_app} ${ci_shared_app_src} "-no_global_stats" "")
else (DEBUG)
  torunonly(common.no_global_stats_LONG ${ci_shared_app} ${ci_shared_app_src} "-no_global_rstats" "")
endif (DEBUG)

if (NOT ANDROID) # XXX i#1874: get working on Android
  # We don't want to link with DR.  We hack that for now by requesting drdecode so
  # we'll get the arch defs.  XXX: Add a better way!
  tobuild_api(libutil.frontend_test libutil/frontend_test.c "" "" ON OFF OFF)
  target_link_libraries(libutil.frontend_test drfrontendlib)
endif ()

set(dr_root ${PROJECT_BINARY_DIR})
# Just like frontend_test we request drdecode.
tobuild_api(libutil.drconfig_test libutil/drconfig_test.c "" "${dr_root}" ON OFF OFF)
target_link_libraries(libutil.drconfig_test drconfiglib)
if (WIN32)
  # We have to copy drconfiglib.dll into the bin dir.
  add_dependencies(libutil.drconfig_test drcopy)
endif ()

set(BUILD_TEST_ANNOTATION "${PROJECT_BINARY_DIR}/suite/tests/annotations")
configure_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/client-interface/annotation/test_mode_annotations.h"
  "${BUILD_TEST_ANNOTATION}/test_mode_annotations.h" COPYONLY)
configure_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/client-interface/annotation/test_mode_annotations.c"
  "${BUILD_TEST_ANNOTATION}/test_mode_annotations.c" COPYONLY)
configure_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/client-interface/annotation/test_annotation_arguments.h"
  "${BUILD_TEST_ANNOTATION}/test_annotation_arguments.h" COPYONLY)
configure_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/client-interface/annotation/test_annotation_arguments.c"
  "${BUILD_TEST_ANNOTATION}/test_annotation_arguments.c" COPYONLY)

include_directories("${PROJECT_BINARY_DIR}/include/annotations")
include_directories("${PROJECT_BINARY_DIR}/suite/tests/annotations")
if (NOT RISCV64) # TODO i#3544: Port tests to RISC-V 64
  tobuild_ci(client.call-retarget client-interface/call-retarget.c "" "" "")
  tobuild_ci(client.int64_overrides client-interface/int64_overrides.c "" "" "")
endif ()
if (X86) # FIXME i#1551, i#1569: port to ARM and AArch64
  set(client.abort_runcmp "${CMAKE_CURRENT_SOURCE_DIR}/runcode.cmake")
  set(client.abort_code "8")
  tobuild_ci(client.abort client-interface/abort.c "" "" "")
endif (X86)
tobuild_ci(client.crashmsg client-interface/crashmsg.c "" "" "")
if (UNIX) # XXX i#2595: NYI on Windows
  tobuild_ci(client.stack-overflow client-interface/stack-overflow.c ""
    # To avoid flakiness on different systems we nail down the stack size:
    "-stack_size 64K" "")
endif ()
# FIXME i#1360: implement Mach-O export iterator
# FIXME i#2008: fix bugs on Android
if (NOT MACOS AND NOT ANDROID)
  tobuild_appdll(client.modules client-interface/modules.c)
  DynamoRIO_get_full_path(modules_libname client.modules.appdll "${location_suffix}")
  tobuild_ci(client.modules client-interface/modules.c "" "" "${modules_libname}")
endif ()
if (X86) # FIXME i#1551, i#1569: port to ARM and AArch64
  tobuild_ci(client.alloc client-interface/alloc.c ""
    # For the subtest "targeting upper 2GB of low 4GB" we move -vm_base out of the
    # way.  We also raise the checklevel so we get memory filling for client allocs.
    # We keep vmheap_size at 2G to more easily trigger OOM.
    "-no_vm_base_near_app -vm_base 0x120000000 -vmheap_size 2G -checklevel 3" "")
  # A second test for i#4335's OOM despite a reset.  We leave checklevel for CI speed.
  tobuild_ci(client.alloc-noreset client-interface/alloc.c ""
    "-no_enable_reset -no_vm_base_near_app -vm_base 0x120000000 -vmheap_size 2G" "")
  # XXX i#1312, i#3504: The test doesn't currently compile with CFLAGS_AVX512 but
  # attempts to test features based on it. The compile flag needs to be added.
  tobuild_ci(client.cleancall client-interface/cleancall.c "" "" "")
  if (proc_supports_avx512)
    append_property_string(TARGET client.cleancall.dll COMPILE_FLAGS "${CFLAGS_AVX512}")
  endif ()
  if (UNIX)
    torunonly_ci(client.cleancall.prof-pcs client.cleancall client.cleancall.dll
      client-interface/cleancall.c "" "-prof_pcs -opt_cleancall 0" "")
    if (NOT X64)
      torunonly_ci(client.cleancall.prof-pcs.thread-private
        client.cleancall client.cleancall.dll
        client-interface/cleancall.c "" "-thread_private -prof_pcs -opt_cleancall 0" "")
    endif (NOT X64)
  endif (UNIX)
  tobuild_ci(client.count-ctis client-interface/count-ctis.c "" "" "")
  # check dr_insert_cbr_instrumentation with out-of-line clean call
  torunonly_ci(client.count-ctis-noopt client.count-ctis client.count-ctis.dll
    client-interface/count-ctis.c "" "-opt_cleancall 0" "")
  tobuild_ci(client.syscall client-interface/syscall.c "" "-no_follow_children" "")
  tobuild_ci(client.count-bbs client-interface/count-bbs.c "" "" "")
endif (X86)
if (NOT RISCV64) # TODO i#3544: Port tests to RISC-V 64
  tobuild_ci(client.cleancallparams client-interface/cleancallparams.c "" "" "")
  tobuild_ci(client.app_inscount client-interface/app_inscount.c "" "" "")
endif (NOT RISCV64)
if (NOT WIN32) # FIXME i#1717: add Windows client C++ EH support
  if (NOT ANDROID) # XXX i#1874: get working on Android
    tobuild_ci(client.exception client-interface/exception.cpp "" "" "")
    if (UNIX)
      # This test deliberately crashes: disable the compiler warning about it.
      CHECK_C_COMPILER_FLAG("-Wno-array-bounds" noarray_avail)
      if (noarray_avail)
        append_property_string(TARGET client.exception.dll
          COMPILE_FLAGS "-Wno-array-bounds")
      endif ()
    endif ()
  endif ()
endif ()

if (UNIX)
  set(annotation_test_args "libclient.annotation-concurrency.appdll.so" "A" "4")
  set(annotation_test_args_shorter
    "libclient.annotation-concurrency.appdll.so" "A" "4" "64" "3")
else (UNIX)
  set(annotation_test_args "client.annotation-concurrency.appdll.dll" "A" "4")
  set(annotation_test_args_shorter
    "client.annotation-concurrency.appdll.dll" "A" "4" "64" "3")
endif (UNIX)
# FIXME i#1799: clang does not support "asm goto"
if (ANNOTATIONS AND NOT CMAKE_COMPILER_IS_CLANG)
  set(DynamoRIO_USE_LIBC OFF)
  tobuild_ci(client.annotation-concurrency client-interface/annotation-concurrency.c
    "" "" "${annotation_test_args}")
  tobuild_appdll(client.annotation-concurrency
    client-interface/annotation-concurrency.c)
  link_with_pthread(client.annotation-concurrency)
  # i#1681: on a many-core machine this test can hit the 1-min timeout
  set(client.annotation-concurrency_timeout 120)
  set(client.annotation-concurrency.full-decode_expectbase
    "annotation-concurrency.full-decode")
  torunonly_ci(client.annotation-concurrency.full-decode client.annotation-concurrency
    client.annotation-concurrency.dll client-interface/annotation-concurrency.c
    "full-decode" "" "${annotation_test_args}")
  # i#1681: on a many-core machine this test can hit the 1-min timeout
  set(client.annotation-concurrency.full-decode_timeout 120)
  set(client.annotation-concurrency.full-decode.tiny-bb_expectbase
    "annotation-concurrency.full-decode")
  torunonly_ci(client.annotation-concurrency.full-decode.tiny-bb
    client.annotation-concurrency client.annotation-concurrency.dll
    client-interface/annotation-concurrency.c "full-decode" "-max_bb_instrs 2"
    "${annotation_test_args}")
  # i#1681: on a many-core machine this test can hit the 1-min timeout
  set(client.annotation-concurrency.full-decode.tiny-bb_timeout 120)
  set(client.annotation-concurrency.bb-truncate-1_expectbase
    "annotation-concurrency.bb-truncate")
  torunonly_ci(client.annotation-concurrency.bb-truncate-1 client.annotation-concurrency
    client.annotation-concurrency.dll client-interface/annotation-concurrency.c
    "truncate@1" "" "${annotation_test_args}")
  # i#1681: on a many-core machine this test can hit the 1-min timeout
  set(client.annotation-concurrency.bb-truncate-1_timeout 120)
  set(client.annotation-concurrency.bb-truncate-2_expectbase
    "annotation-concurrency.bb-truncate")
  torunonly_ci(client.annotation-concurrency.bb-truncate-2 client.annotation-concurrency
    client.annotation-concurrency.dll client-interface/annotation-concurrency.c
    "truncate@2" "" "${annotation_test_args}")
  # i#1681: on a many-core machine this test can hit the 1-min timeout
  set(client.annotation-concurrency.bb-truncate-2_timeout 120)

  if (UNIX)
    set(client.annotation-concurrency.prof-pcs_timeout 120)
    set(client.annotation-concurrency.prof-pcs_expectbase
      "annotation-concurrency.full-decode")
    torunonly_ci(client.annotation-concurrency.prof-pcs client.annotation-concurrency
      client.annotation-concurrency.dll client-interface/annotation-concurrency.c
      "full-decode" "-prof_pcs" "${annotation_test_args}")
    if (NOT X64)
      set(client.annotation-concurrency.prof-pcs.thread-private_timeout 120)
      set(client.annotation-concurrency.prof-pcs.thread-private_expectbase
        "annotation-concurrency.full-decode")
      torunonly_ci(client.annotation-concurrency.prof-pcs.thread-private
        client.annotation-concurrency
        client.annotation-concurrency.dll client-interface/annotation-concurrency.c
        "full-decode" "-thread_private -prof_pcs" "${annotation_test_args}")
    endif (NOT X64)
  endif (UNIX)

  if (UNIX)
    set(annotation_opt_args "libclient.annotation-concurrency-opt.appdll.so" "A" "4")
  else (UNIX)
    set(annotation_opt_args "client.annotation-concurrency-opt.appdll.dll" "A" "4")
  endif (UNIX)
  tobuild_ci(client.annotation-concurrency-opt client-interface/annotation-concurrency.c
    "" "" "${annotation_opt_args}")
  optimize(client.annotation-concurrency-opt)
  tobuild_appdll(client.annotation-concurrency-opt
    client-interface/annotation-concurrency.c)
  optimize(client.annotation-concurrency-opt.appdll)
  link_with_pthread(client.annotation-concurrency-opt)
  # i#1681: on a many-core machine this test can hit the 1-min timeout
  set(client.annotation-concurrency-opt_timeout 120)
  set(client.annotation-concurrency-opt.full-decode_expectbase
    "annotation-concurrency.full-decode")
  torunonly_ci(client.annotation-concurrency-opt.full-decode
    client.annotation-concurrency-opt client.annotation-concurrency-opt.dll
    client-interface/annotation-concurrency.c "full-decode" "" "${annotation_opt_args}")
  # i#1681: on a many-core machine this test can hit the 1-min timeout
  set(client.annotation-concurrency-opt.full-decode_timeout 120)

  tobuild_ci(client.annotation-detection client-interface/annotation-detection.cpp
    "" "" "")
  use_DynamoRIO_extension(client.annotation-detection.dll drmgr)
  use_DynamoRIO_extension(client.annotation-detection.dll drreg)
  set(client.annotation-detection.full-decode_expectbase
    "annotation-detection.full-decode")
  torunonly_ci(client.annotation-detection.full-decode client.annotation-detection
    client.annotation-detection.dll client-interface/annotation-detection.c
    "full-decode" "" "")
  set(client.annotation-detection.full-decode.tiny-bb_expectbase
    "annotation-detection.full-decode")
  torunonly_ci(client.annotation-detection.full-decode.tiny-bb
    client.annotation-detection client.annotation-detection.dll
    client-interface/annotation-detection.c "full-decode" "-max_bb_instrs 2" "")
  set(client.annotation-detection.bb-truncate-1_expectbase
    "annotation-detection.bb-truncate")
  torunonly_ci(client.annotation-detection.bb-truncate-1 client.annotation-detection
    client.annotation-detection.dll client-interface/annotation-detection.c
    "truncate@1" "" "")
  set(client.annotation-detection.bb-truncate-2_expectbase
    "annotation-detection.bb-truncate")
  torunonly_ci(client.annotation-detection.bb-truncate-2 client.annotation-detection
    client.annotation-detection.dll client-interface/annotation-detection.c
    "truncate@2" "" "")
  torunonly_native(client.annotation-detection.native client.annotation-detection
    annotation-detection.native client-interface/annotation-detection.c "")

  set(client.annotation-detection-opt_expectbase "annotation-detection")
  tobuild_ci(client.annotation-detection-opt client-interface/annotation-detection.cpp
    "" "" "")
  use_DynamoRIO_extension(client.annotation-detection-opt.dll drmgr)
  use_DynamoRIO_extension(client.annotation-detection-opt.dll drreg)
  optimize(client.annotation-detection-opt)
  torunonly_native(client.annotation-detection-opt.native
    client.annotation-detection-opt annotation-detection.native
    client-interface/annotation-detection.c "")
  set(DynamoRIO_USE_LIBC ON)
else (ANNOTATIONS AND NOT CMAKE_COMPILER_IS_CLANG)
  # We build the client.annotation-concurrency app even if we have no annotation
  # support, since it is used in drcachesim tests.
  add_exe(client.annotation-concurrency
    client-interface/annotation-concurrency.c)
  append_property_string(TARGET client.annotation-concurrency COMPILE_FLAGS
    "-DANNOTATIONS_DISABLED")
  link_with_pthread(client.annotation-concurrency)
  tobuild_appdll(client.annotation-concurrency
    client-interface/annotation-concurrency.c)
  append_property_string(TARGET client.annotation-concurrency.appdll COMPILE_FLAGS
    "-DANNOTATIONS_DISABLED")
endif (ANNOTATIONS AND NOT CMAKE_COMPILER_IS_CLANG)

if (UNIX)
  # XXX i#1246: Make partial_module_map work for Windows as well.
  tobuild_ci(client.partial_module_map client-interface/partial_module_map.c "" "" "")
endif ()
if (LINUX AND X86)
  # XXX i#1833: once i#1833 is fixed, enable code in mbr_instrumentation_segment.dll.c
  tobuild_ci(client.mbr_instrumentation_segment
    client-interface/mbr_instrumentation_segment.c "" "" "")
  use_DynamoRIO_extension(client.mbr_instrumentation_segment.dll drsyms)
  use_DynamoRIO_extension(client.mbr_instrumentation_segment.dll drmgr)
endif()
if (NOT ARM) # FIXME i#1551: fix bugs on ARM
  tobuild_ci(client.segfault client-interface/segfault.c "" "" "")
  tobuild_ci(client.execfault client-interface/execfault.c "" "" "")
  tobuild_appdll(client.events client-interface/events.c)
  DynamoRIO_get_full_path(events_appdll_path client.events.appdll "${location_suffix}")
endif (NOT ARM)
if (ANNOTATIONS AND NOT ARM)
  if (UNIX OR NOT X64) # the Valgrind annotation structure cannot support Windows x64
    tobuild_ci(client.vg-annot client-interface/vg-annot.c "" "" "")
    set(client.vg-annot.full-decode_expectbase "vg-annot.full-decode")
    torunonly_ci(client.vg-annot.full-decode client.vg-annot client.vg-annot.dll
      client-interface/vg-annot.c "full-decode" "" "")
    set(client.vg-annot.full-decode.tiny-bb_expectbase "vg-annot.full-decode")
    torunonly_ci(client.vg-annot.full-decode.tiny-bb client.vg-annot client.vg-annot.dll
      client-interface/vg-annot.c "full-decode" "-max_bb_instrs 2" "")
    set(client.vg-annot.bb-truncate-1_expectbase "vg-annot.bb-truncate")
    torunonly_ci(client.vg-annot.bb-truncate-1 client.vg-annot client.vg-annot.dll
      client-interface/vg-annot.c "truncate@1" "" "")
    set(client.vg-annot.bb-truncate-2_expectbase "vg-annot.bb-truncate")
    torunonly_ci(client.vg-annot.bb-truncate-2 client.vg-annot client.vg-annot.dll
      client-interface/vg-annot.c "truncate@2" "" "")
    tobuild_ci(client.vg-annot-opt client-interface/vg-annot.c "" "" "")
    optimize(client.vg-annot-opt)
    set(client.vg-annot-opt.full-decode_expectbase "vg-annot.full-decode")
    torunonly_ci(client.vg-annot-opt.full-decode client.vg-annot-opt client.vg-annot.dll
      client-interface/vg-annot.c "full-decode" "" "")
    set(client.vg-annot-opt.full-decode.tiny-bb_expectbase "vg-annot.full-decode")
    torunonly_ci(client.vg-annot-opt.full-decode.tiny-bb client.vg-annot-opt
      client.vg-annot.dll client-interface/vg-annot.c
      "full-decode" "-max_bb_instrs 3" "")
    set(client.vg-annot-opt.bb-truncate_expectbase "vg-annot.bb-truncate")
    torunonly_ci(client.vg-annot-opt.bb-truncate client.vg-annot-opt client.vg-annot.dll
      client-interface/vg-annot.c "truncate@2" "" "")
  endif (UNIX OR NOT X64)
endif (ANNOTATIONS AND NOT ARM)

if (NOT ANDROID) # TODO i#38: Port test to Android.
  tobuild_ci(client.attach_test client-interface/attach_test.runall "" "" "")
  if (WIN32)
    tobuild_ci(client.detach_test client-interface/detach_test.runall "" "" "")
  endif (WIN32)
  if (UNIX)
    # Test attaching during a blocking syscall.
    torunonly_ci(client.attach_blocking linux.infloop client.attach_test.dll
      client-interface/attach_blocking.runall "" "" "")
  endif ()
endif ()

if (UNIX)
  if (NOT ARM) # FIXME i#1551: fix bugs on ARM
    if (AARCH64)
      # FIXME i#1569: Reenable when trace construction is implemented.
      set(trace_arg "-disable_traces")
    else ()
      # Just happens to not make as many traces.
      set(trace_arg "-trace_threshold 5")
    endif ()
    tobuild_ci(client.events client-interface/events.c
      "" "${trace_arg}" "${events_appdll_path}")
    tobuild_ci(client.events_cpp client-interface/events_cpp.cpp
      "" "${trace_arg}" "${events_appdll_path}")
    tobuild_ci(client.nudge_test client-interface/nudge_test.runall "" "" "")
    # We disable traces to get bb prefixes for testing i#4669.
    tobuild_ci(client.timer client-interface/timer.c "" "-disable_traces" "")
    if (X64)
      tobuild_ci(client.mangle_suspend client-interface/mangle_suspend.c ""
        "-vm_base 0x100000000 -no_vm_base_near_app" "")
      use_DynamoRIO_extension(client.mangle_suspend.dll drmgr)
      target_include_directories(client.mangle_suspend PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/client-interface)
      link_with_pthread(client.mangle_suspend)
    endif ()
  endif (NOT ARM)
  if (X86 OR AARCH64) # FIXME i#1551: port asm to AArch32
    tobuild_ci(client.syscall-mod client-interface/syscall-mod.c "" "" "")
  endif (X86 OR AARCH64)
  if (NOT RISCV64) # TODO i#3544: Port tests to RISC-V 64
    tobuild_ci(client.signal client-interface/signal.c "" "" "")
    link_with_pthread(client.signal)
  endif ()
  if (X86) # FIXME i#1551, i#1569: port asm to ARM and AArch64
    tobuild_ci(client.cbr-retarget client-interface/cbr-retarget.c "" "" "")
  endif (X86)
  tobuild_ci(client.cleancallsig client-interface/cleancallsig.c "" "" "")
  if (X64 AND NOT RISCV64) # XXX i#3173 Improve testing of emulation API functions
    # TODO i#3544: Port tests to RISC-V 64
    tobuild_ci(client.emulation_api_simple client-interface/emulation_api_simple.c
      "" "" "")
    use_DynamoRIO_extension(client.emulation_api_simple.dll drmgr)
  endif ()
else (UNIX)
  tobuild_ci(client.events client-interface/events.c
    "" "" "${events_appdll_path}")
  tobuild_ci(client.events_cpp client-interface/events_cpp.cpp
    "" "" "${events_appdll_path}")
  tobuild_ci(client.cbr3 client-interface/cbr3.c "" "" "")
  tobuild_ci(client.cbr4 client-interface/cbr4.c "" "-thread_private" "")
  # FIXME i#120: enable these .runall tests once have .runall support
  #tobuild_ci_runall(client.cbr2 client-interface/cbr2.runall "" "" "")
  #tobuild_ci_runall(client.cbr client-interface/cbr.runall "" "" "")
  #tobuild_ci_runall(client.custom_traces client-interface/custom_traces.runall
  #  "" "" "")
  #tobuild_ci_runall(client.decode-bb client-interface/decode-bb.runall "" "" "")
  # I reworked Windows .runall support to run the new win32.infloop but have not yet
  # looked at other tests:
  tobuild_ci(client.nudge_test client-interface/nudge_test.runall "" "" "")
  #tobuild_ci_runall(client.pc-check client-interface/pc-check.runall "" "" "")
  if (NOT X64)
    # FIXME i#16, does not work in Win-X64 because of inline assembly code.
    tobuild_ci(client.cbr-retarget client-interface/cbr-retarget.c "" "" "")
  endif (NOT X64)

  add_exe(client.fibers client-interface/fibers.c)
  torunonly_ci(client.fibers client.fibers client.events_cpp.dll
    client-interface/fibers.c "" "" "")

  # i#1866: stress our private loader
  tobuild_ci(client.loader client-interface/loader.c "" "" "")
  target_link_libraries(client.loader.dll "shlwapi")

  tobuild_ci(client.winxfer client-interface/winxfer.c "" "" "")
endif (UNIX)
if (NOT APPLE OR NOT AARCH64) # TODO i#5383: Port to Mac M1.
  tobuild_ci(client.file_io client-interface/file_io.c
    "${CMAKE_CURRENT_SOURCE_DIR}/client-interface/file_io_data.txt" "" "")
endif ()
if (X86) # FIXME i#1551, i#1569: port asm to ARM and AArch64
  if (DEBUG) # FIXME i#1806: fails in release; also in OSX list below.
    # we add custom option to flush test based on dr ops in torun_ci()
    tobuild_ci(client.flush client-interface/flush.c "" "" "")
  endif ()
  tobuild_ci(client.thread client-interface/thread.c "-paramx -paramy" "" "")
  tobuild_appdll(client.thread client-interface/thread.c)
  tobuild_ci(client.strace client-interface/strace.c "" "" "")
  use_DynamoRIO_extension(client.strace.dll drmgr)
endif (X86)
# FIXME: PR 199115 to re-enable fragdel, get some more of the UNIX tests working
#tobuild_ci(client.fragdel client-interface/fragdel.c "" "" "")
if (PROGRAM_SHEPHERDING)
  tobuild_ci(client.security client-interface/security.c "" "-security_api" "")
endif (PROGRAM_SHEPHERDING)
if (NOT ARM) # FIXME i#1551: fix bugs on ARM
  tobuild_ci(client.truncate client-interface/truncate.c "" "" "")
  if (WIN32)
    set(client.truncate.thread-churn-1_expectbase "truncate.thread-churn")
    torunonly_ci(client.truncate.thread-churn-1 win32.threadchurn client.truncate.dll
      client-interface/truncate.c "1" "" "")
    set(client.truncate.thread-churn-2_expectbase "truncate.thread-churn")
    torunonly_ci(client.truncate.thread-churn-2 win32.threadchurn client.truncate.dll
      client-interface/truncate.c "2" "" "")
  endif (WIN32)
  if (NOT AARCH64) # FIXME i#1569: get working on AArch64
    tobuild_ci(client.dr_options client-interface/dr_options.c
      "" "-native_exec_list foo.dll,bar.dll -opt_cleancall 3 -thread_private" "")
  endif (NOT AARCH64)
endif (NOT ARM)
tobuild_ci(client.unregister client-interface/unregister.c "" "" "")
if (NOT ARM AND NOT RISCV64) # FIXME i#2094: implement cleancall optimizations on ARM
  # TODO i#3544: Port tests to RISC-V 64
  tobuild_ci(client.cleancall-opt-1 client-interface/cleancall-opt-1.c ""
    "-opt_cleancall 1" "")
  if (proc_supports_avx512)
    # XXX i#1312: This is untested on Windows.
    tobuild_ci(client.avx512cleancall-opt-1 client-interface/cleancall-opt-1.c ""
      "-opt_cleancall 1" "")
    append_property_string(TARGET client.avx512cleancall-opt-1 COMPILE_FLAGS
      "${CFLAGS_AVX512}")
    append_property_string(TARGET client.avx512cleancall-opt-1.dll COMPILE_FLAGS
      "${CFLAGS_AVX512}")
  endif ()

  tobuild_ci(client.inline client-interface/inline.c "" "-opt_cleancall 3" "")
  if (CMAKE_COMPILER_IS_CLANG)
    optimize(client.inline.dll)
  endif ()
endif (NOT ARM AND NOT RISCV64)
if (NOT ANDROID) # XXX i#1874: get working on Android
  tobuild_ci(client.null_instrument client-interface/null_instrument.c "" "" "")
  tobuild_appdll(client.null_instrument client-interface/null_instrument.c)
  target_link_libraries(client.null_instrument client.null_instrument.appdll)
  # We want rpath on Linux so we can load the appdll.
  set_target_properties(client.null_instrument PROPERTIES SKIP_BUILD_RPATH OFF)
endif ()

if (NOT ARM) # FIXME i#1551: fix bugs on ARM
  # Test passing a really long (600 chars) client option string.
  tobuild_ci(client.large_options client-interface/large_options.c
    "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
    "" "")
endif (NOT ARM)

add_library(client.blackbox.dll.A SHARED client-interface/blackbox.dll.A.c)
setup_test_client_dll_basics(client.blackbox.dll.A)
add_library(client.blackbox.dll.B SHARED client-interface/blackbox.dll.B.c)
setup_test_client_dll_basics(client.blackbox.dll.B)
target_link_libraries(client.blackbox.dll.A client.blackbox.dll.B)
torunonly_ci(client.blackbox ${ci_shared_app} client.blackbox.dll.A
  client-interface/${ci_shared_app} # for .expect
  "" "" "")

tobuild_ci(client.option_parse client-interface/option_parse.cpp
  "-l;-4;-ll;-3220721071790640321;-ul;4;-ull;1384772493926445887;-x;3;-x_alias;4;-y;quoted string;-z;first;-z_alias;single quotes -dash --dashes;-front;value;-y;accum;-front2;value2;-flag;-flag_alias1;-no_flag_alias2;-takes2;1_of_4;2_of_4;-takes2;3_of_4;4_of_4;-val_sep;v1.1 v1.2;-val_sep;v2.1 v2.2;-val_sep2;v1;v2;-val_sep2;v3;v4;-large_bytesize;9999999999;-oi;-012;-ol;-012;-oll;-012;-ou;012;-oul;012;-oull;012;-xi;-0xa;-xl;-0xa;-xll;-0xa;-xu;0xa;-xul;0xa;-xull;0xa;"
  "" "")
use_DynamoRIO_extension(client.option_parse.dll droption)
set(client.option_parse_client_ops_islist ON)

if (AARCH64)
  tobuild_ci(client.features client-interface/features.c "" "" "")

  tobuild_ci(client.stolen-reg-index client-interface/stolen-reg-index.c "" "" "")
  use_DynamoRIO_extension(client.stolen-reg-index.dll drmgr)
  use_DynamoRIO_extension(client.stolen-reg-index.dll drreg)
  use_DynamoRIO_extension(client.stolen-reg-index.dll drutil)
endif ()

if (NOT RISCV64) # TODO i#3544: Port tests to RISC-V 64
  tobuild_ci(client.reg_size_test client-interface/reg_size_test.c "" "" "")

  tobuild_ci(client.thread_exit_xl8 client-interface/thread_exit_xl8.c "" "" "")
endif (NOT RISCV64)

if (UNIX AND NOT RISCV64) # The test uses signals.
  # TODO i#3544: Port tests to RISC-V 64
  tobuild_ci(client.gonative client-interface/gonative.c "" "" "")
  link_with_pthread(client.gonative)
  # Configure for dr_app_running_under_dynamorio().
  configure_app_api_build_flags(client.gonative OFF OFF)
  add_dependencies(client.gonative api_headers)
  set_target_properties(client.gonative PROPERTIES SKIP_BUILD_RPATH OFF)
endif ()

if (NOT RISCV64) # TODO i#3544: Port tests to RISC-V 64
  tobuild_ci(client.drcontainers-test client-interface/drcontainers-test.c "" "" "")
  use_DynamoRIO_extension(client.drcontainers-test.dll drcontainers)

  tobuild_ci(client.drmgr-test client-interface/drmgr-test.c "" "" "${events_appdll_path}")
  use_DynamoRIO_extension(client.drmgr-test.dll drmgr)
  link_with_pthread(client.drmgr-test)
endif (NOT RISCV64)

if (proc_supports_pt)
  tobuild_ci(client.drpttracer_SUDO-test client-interface/drpttracer_SUDO-test.c "" "" "")
  use_DynamoRIO_extension(client.drpttracer_SUDO-test.dll drpttracer)
  use_DynamoRIO_extension(client.drpttracer_SUDO-test.dll drmgr)
  set(client.drpttracer_SUDO-test_sudo ON)
endif ()

if (NOT RISCV64) # TODO i#3544: Port tests to RISC-V 64
  tobuild_ci(client.drx_buf-test client-interface/drx_buf-test.c "" "" "")
  use_DynamoRIO_extension(client.drx_buf-test.dll drmgr)
  use_DynamoRIO_extension(client.drx_buf-test.dll drx)
  link_with_pthread(client.drx_buf-test)
  target_include_directories(client.drx_buf-test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/client-interface)
endif (NOT RISCV64)

if (NOT RISCV64) # TODO i#3544: Port tests to RISC-V 64
  tobuild_ci(client.drbbdup-test client-interface/drbbdup-test.c "" "" "")
  use_DynamoRIO_extension(client.drbbdup-test.dll drmgr)
  use_DynamoRIO_extension(client.drbbdup-test.dll drbbdup)
endif (NOT RISCV64)

# XXX i#1884: The thread-private option is not yet available for ARM.
if (X86)
  # Same drbbdup test but with thread-private code caches.
  torunonly_ci(client.drbbdup-thread-private-test ${ci_shared_app}
    client.drbbdup-test.dll client-interface/drbbdup-test.c "" "-thread_private" "")
endif (X86)

macro(drbbdup_bb_test test)
  add_exe(client.${test}-exe client-interface/${test}.asm)
  set_target_properties(client.${test}-exe PROPERTIES LINKER_LANGUAGE C)
  append_link_flags(client.${test}-exe "-nostartfiles -nodefaultlibs -static")

  add_library(client.${test}.dll SHARED  client-interface/${test}.dll.c)
  setup_test_client_dll_basics(client.${test}.dll)
  use_DynamoRIO_extension(client.${test}.dll drmgr)
  use_DynamoRIO_extension(client.${test}.dll drbbdup)

  torunonly_ci(client.${test} client.${test}-exe
    client.${test}.dll client-interface/${test}.asm "" "-max_bb_instrs 0" "")
endmacro(drbbdup_bb_test)

if (X86 AND X64 AND LINUX)
  drbbdup_bb_test(drbbdup-meta-label-bb-test)
  drbbdup_bb_test(drbbdup-meta-nop-bb-test)
  drbbdup_bb_test(drbbdup-empty-bb-test)
endif (X86 AND X64 AND LINUX)

if (NOT RISCV64) # TODO i#3544: Port tests to RISC-V 64
  tobuild_ci(client.drbbdup-no-encode-test client-interface/drbbdup-no-encode-test.c "" "" "")
  use_DynamoRIO_extension(client.drbbdup-no-encode-test.dll drmgr)
  use_DynamoRIO_extension(client.drbbdup-no-encode-test.dll drbbdup)

  tobuild_ci(client.drbbdup-no-dup-test client-interface/drbbdup-no-dup-test.c "" "" "")
  use_DynamoRIO_extension(client.drbbdup-no-dup-test.dll drmgr)
  use_DynamoRIO_extension(client.drbbdup-no-dup-test.dll drbbdup)

  tobuild_ci(client.drbbdup-nonzero-test client-interface/drbbdup-nonzero-test.c "" "" "")
  use_DynamoRIO_extension(client.drbbdup-nonzero-test.dll drmgr)
  use_DynamoRIO_extension(client.drbbdup-nonzero-test.dll drbbdup)

  tobuild_ci(client.drbbdup-analysis-test client-interface/drbbdup-analysis-test.c "" "" "")
  use_DynamoRIO_extension(client.drbbdup-analysis-test.dll drmgr)
  use_DynamoRIO_extension(client.drbbdup-analysis-test.dll drbbdup)

  tobuild_appdll(client.drbbdup-drwrap-test client-interface/drbbdup-drwrap-test.c)
  get_target_path_for_execution(bbdupwrap_libpath
    client.drbbdup-drwrap-test.appdll "${location_suffix}")
  tobuild_ci(client.drbbdup-drwrap-test client-interface/drbbdup-drwrap-test.c "" ""
    "${bbdupwrap_libpath}")
  use_DynamoRIO_extension(client.drbbdup-drwrap-test.dll drmgr)
  use_DynamoRIO_extension(client.drbbdup-drwrap-test.dll drbbdup)
  use_DynamoRIO_extension(client.drbbdup-drwrap-test.dll drwrap)

  tobuild_ci(client.drbbdup-emul-test client-interface/drbbdup-emul-test.c "" "" "")
  use_DynamoRIO_extension(client.drbbdup-emul-test.dll drmgr)
  use_DynamoRIO_extension(client.drbbdup-emul-test.dll drutil)
  use_DynamoRIO_extension(client.drbbdup-emul-test.dll drbbdup)
  use_DynamoRIO_extension(client.drbbdup-emul-test.dll drreg)
endif (NOT RISCV64)

if (X86)
  tobuild_ci(client.drbbdup-emul-reg-clobber-test
      client-interface/drbbdup-emul-reg-clobber-test.c "" "" "")
  use_DynamoRIO_extension(client.drbbdup-emul-reg-clobber-test.dll drmgr)
  use_DynamoRIO_extension(client.drbbdup-emul-reg-clobber-test.dll drutil)
  use_DynamoRIO_extension(client.drbbdup-emul-reg-clobber-test.dll drbbdup)
  use_DynamoRIO_extension(client.drbbdup-emul-reg-clobber-test.dll drreg)
endif (X86)

if (ARM)
  tobuild_ci(client.predicate-test client-interface/predicate-test.c "" "" "")
  use_DynamoRIO_extension(client.predicate-test.dll drmgr)
  use_DynamoRIO_extension(client.predicate-test.dll drutil)
  use_DynamoRIO_extension(client.predicate-test.dll drreg)
  target_include_directories(client.predicate-test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/client-interface)
endif (ARM)

if (UNIX)
  tobuild_ci(client.process-id client-interface/process-id.c "" "" "")
  link_with_pthread(client.process-id)
endif (UNIX)

if (NOT RISCV64) # TODO i#3544: Port tests to RISC-V 64
  if (NOT APPLE OR NOT AARCH64) # TODO i#5383: Port to Mac M1.
  tobuild_ci(client.drreg-test client-interface/drreg-test.c "" "" "")
  use_DynamoRIO_extension(client.drreg-test.dll drmgr)
  use_DynamoRIO_extension(client.drreg-test.dll drreg)
  use_DynamoRIO_extension(client.drreg-test.dll drx)
  target_include_directories(client.drreg-test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/client-interface)
  endif ()

  tobuild_ci(client.drreg-end-restore client-interface/drreg-end-restore.c "" "" "")
  use_DynamoRIO_extension(client.drreg-end-restore.dll drmgr)
  use_DynamoRIO_extension(client.drreg-end-restore.dll drreg)

  tobuild_ci(client.drreg-flow client-interface/drreg-flow.c "" "" "")
  use_DynamoRIO_extension(client.drreg-flow.dll drmgr)
  use_DynamoRIO_extension(client.drreg-flow.dll drreg)
  use_DynamoRIO_extension(client.drreg-flow.dll drutil)

  tobuild_ci(client.drreg-cross client-interface/drreg-cross.c "" "" "")
  use_DynamoRIO_extension(client.drreg-cross.dll drmgr)
  use_DynamoRIO_extension(client.drreg-cross.dll drreg)
  use_DynamoRIO_extension(client.drreg-cross.dll drutil)

  tobuild_ci(client.drx-test client-interface/drx-test.c "" "" "")
  use_DynamoRIO_extension(client.drx-test.dll drx)

  tobuild_ci(client.drxmgr-test client-interface/drxmgr-test.c "" "" "")
  use_DynamoRIO_extension(client.drxmgr-test.dll drmgr)
  use_DynamoRIO_extension(client.drxmgr-test.dll drx)
  use_DynamoRIO_extension(client.drxmgr-test.dll drreg)

  # Declare reset params due to i#1674 and i#3912
  tobuild_ci(client.low_on_memory client-interface/low_on_memory.c ""
      "-enable_reset -reset_at_vmm_percent_free_limit 10 -vm_size 4M" "")
  use_DynamoRIO_extension(client.low_on_memory.dll drmgr)
  use_DynamoRIO_extension(client.low_on_memory.dll drwrap)

  tobuild_ci(client.tls client-interface/tls.c "" "" "")
  link_with_pthread(client.tls)
  use_DynamoRIO_extension(client.tls.dll drmgr)
endif (NOT RISCV64)

if (X86)
  if (NOT MACOS)
    # XXX i#2985: The test's asm doesn't build with MacOS's clang.
    tobuild_ci(client.drx-scattergather client-interface/drx-scattergather.c "" "" "")
    target_include_directories(client.drx-scattergather PRIVATE
      ${CMAKE_CURRENT_SOURCE_DIR}/client-interface)
    use_DynamoRIO_extension(client.drx-scattergather.dll drmgr)
    use_DynamoRIO_extension(client.drx-scattergather.dll drx)
    use_DynamoRIO_extension(client.drx-scattergather.dll drreg)
    set_avx_flags(client.drx-scattergather)

    # Our scattergather drbbdup test uses the same app as the base test.
    set(client.drx-scattergather-bbdup_realtest client.drx-scattergather)
    set(client.drx-scattergather-bbdup_expectbase "drx-scattergather")
    tobuild_ci(client.drx-scattergather-bbdup client-interface/drx-scattergather-bbdup.c
      "" "" "")
    target_include_directories(client.drx-scattergather PRIVATE
      ${CMAKE_CURRENT_SOURCE_DIR}/client-interface)
    use_DynamoRIO_extension(client.drx-scattergather-bbdup.dll drmgr)
    use_DynamoRIO_extension(client.drx-scattergather-bbdup.dll drx)
    use_DynamoRIO_extension(client.drx-scattergather-bbdup.dll drreg)
    use_DynamoRIO_extension(client.drx-scattergather-bbdup.dll drbbdup)
    set_avx_flags(client.drx-scattergather-bbdup)

    if (X64 AND UNIX)
      if (proc_supports_avx)
        add_exe(allasm_scattergather
          ${PROJECT_SOURCE_DIR}/clients/drcachesim/tests/allasm_scattergather.asm
          "-early_inject" "")
        append_pure_asm_app_link_flags(allasm_scattergather)
        set_avx_flags(allasm_scattergather)
      endif (proc_supports_avx)
      add_exe(allasm_repstr
        ${PROJECT_SOURCE_DIR}/clients/drcachesim/tests/allasm_repstr.asm
        "-early_inject" "")
      append_pure_asm_app_link_flags(allasm_repstr)
    endif (X64 AND UNIX)
  endif()
endif ()

if (NOT RISCV64)
  # TODO i#3544: Port tests to RISC-V 64
  tobuild_appdll(client.drwrap-test client-interface/drwrap-test.c)
  get_target_path_for_execution(drwrap_libpath client.drwrap-test.appdll
    "${location_suffix}")
  tobuild_ci(client.drwrap-test client-interface/drwrap-test.c "" "" "${drwrap_libpath}")
  use_DynamoRIO_extension(client.drwrap-test.dll drwrap)
  tochcon(client.drwrap-test.appdll textrel_shlib_t)
endif (NOT RISCV64)

if (WIN32)
  # export from asm code
  append_link_flags(client.drwrap-test.appdll
    "/export:makes_tailcall /export:tailcall_test2 /export:tailcall_tail")
endif (WIN32)
if (NOT ANDROID) # XXX i#1874: get working on Android
  tobuild_ci(client.drwrap-test-callconv client-interface/drwrap-test-callconv.cpp
    "" "" "")
  use_DynamoRIO_extension(client.drwrap-test-callconv.dll drwrap)
endif ()
if (NOT APPLE AND NOT RISCV64) # XXX i#1997: static DR not fully supported on Mac yet
  # TODO i#3544: Port tests to RISC-V 64
  set(client.drwrap-test-detach_no_reg_compat)
  tobuild_api(client.drwrap-test-detach client-interface/drwrap-test-detach.cpp
    "" "" OFF ON OFF)
  use_DynamoRIO_extension(client.drwrap-test-detach drwrap_static)
  link_with_pthread(client.drwrap-test-detach)
endif ()

if (NOT RISCV64) # TODO i#3544: Port tests to RISC-V 64
  tobuild_appdll(client.drwrap-drreg-test client-interface/drwrap-drreg-test.c)
  get_target_path_for_execution(drwrap_drreg_libpath client.drwrap-drreg-test.appdll
    "${location_suffix}")
  tobuild_ci(client.drwrap-drreg-test client-interface/drwrap-drreg-test.c "" ""
    "${drwrap_drreg_libpath}")
  use_DynamoRIO_extension(client.drwrap-drreg-test.dll drwrap)
  use_DynamoRIO_extension(client.drwrap-drreg-test.dll drreg)
  use_DynamoRIO_extension(client.drwrap-drreg-test.dll drmgr)
endif (NOT RISCV64)

if (AARCH64 AND NOT APPLE) # TODO i#5383: Port to Mac M1.
  # Create a fuzzing application for stress-testing via drstatecmp.
  add_api_exe(drstatecmp-fuzz-app client-interface/drstatecmp-fuzz-app.c ON OFF)
endif ()

# We rely on dbghelp >= 6.0 for our drsyms and sample.instrcalls tests,
# but the system dbghelp pre-Vista is too old, so we copy one from VS.
if ("${CMAKE_SYSTEM_VERSION}" STRLESS "6.0")
  if (dbghelp_path) # set at top level
    configure_file(${dbghelp_path} ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/dbghelp.dll
      COPYONLY)
    if (BUILD_SAMPLES)
      get_property(sample_list GLOBAL PROPERTY DynamoRIO_sample_list)
      list(GET sample_list 1 sample_one) # 0 is empty
      DynamoRIO_get_full_path(sample_loc ${sample_one} "${location_suffix}")
      get_filename_component(sample_dir ${sample_loc} PATH)
      configure_file(${dbghelp_path} ${sample_dir}/dbghelp.dll COPYONLY)
    endif (BUILD_SAMPLES)
    message(STATUS "Using ${dbghelp_path} for drsyms tests")
  endif (dbghelp_path)
endif ()

if (NOT ANDROID AND NOT RISCV64) # TODO i#3544: Port tests to RISC-V 64
  # XXX i#1874: get working on Android
  tobuild_appdll(client.drsyms-test client-interface/drsyms-test.cpp)
  get_target_path_for_execution(drsyms_libpath client.drsyms-test.appdll "${location_suffix}")
  tobuild_ci(client.drsyms-test client-interface/drsyms-test.cpp ""
    "" "${drsyms_libpath}")
  # Disable optimizations for the exe and appdll to allow stack tracing.
  disable_optimizations_for_file(client-interface/drsyms-test.cpp)
  disable_optimizations_for_file(client-interface/drsyms-test.appdll.cpp)
  # Debug libc seems to mess up the test.
  use_MT_not_MTd(client-interface/drsyms-test.appdll.cpp)
  use_DynamoRIO_extension(client.drsyms-test.dll drsyms)
  use_DynamoRIO_extension(client.drsyms-test.dll drwrap)  # Makes testing easy

  # TODO i#2414: Port to Windows, Mac, and Android.
  if (LINUX AND HAVE_LIBUNWIND_H)
    tobuild_ci(client.drcallstack-test client-interface/drcallstack-test.c "" "" "")
    use_DynamoRIO_extension(client.drcallstack-test.dll drsyms)
    use_DynamoRIO_extension(client.drcallstack-test.dll drwrap)
    use_DynamoRIO_extension(client.drcallstack-test.dll drcallstack)
  endif ()
endif ()

# We check these two statements here b/c not all gcc compilers support
# cross - architecture compilation (e.g. perl mingw x64 doesn't support
# x32 app compilation).
if (WIN32 AND GCC AND (GCC_IS64 AND X64) OR (NOT GCC_IS64 AND NOT X64))
  # test drsyms dwarf-on-windows capabilities.
  # cmake doesn't support a new compiler so we do a simple build here.
  tobuild_gcc(client.drsyms-testgcc.exe
    client-interface/drsyms-test.cpp client.drsyms-test "")
  tobuild_gcc(client.drsyms-testgcc.appdll.dll
    client-interface/drsyms-test.appdll.cpp client.drsyms-test "-shared")
  torunonly_ci(client.drsyms-testgcc
    ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/client.drsyms-testgcc.exe
    client.drsyms-test.dll client-interface/drsyms-test.cpp "" "-stack_size 36K"
    "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/client.drsyms-testgcc.appdll.dll")
  set(client.drsyms-testgcc_expectbase "drsyms-testgcc")
  if (GCC_IS_CYGWIN)
    set(client.drsyms-testgcc_is_cygwin ON)
  endif()
endif (WIN32 AND GCC AND (GCC_IS64 AND X64) OR (NOT GCC_IS64 AND NOT X64))
if (NOT RISCV64) # TODO i#3544: Port tests to RISC-V 64
  tobuild_ci(client.drutil-test client-interface/drutil-test.c "" "" "")
  use_DynamoRIO_extension(client.drutil-test.dll drutil)
  use_DynamoRIO_extension(client.drutil-test.dll drmgr)
  link_with_pthread(client.drutil-test)

  tobuild_ci(client.drmodtrack-test client-interface/drmodtrack-test.cpp "" "" "")
  use_DynamoRIO_extension(client.drmodtrack-test.dll drcovlib)
  use_DynamoRIO_extension(client.drmodtrack-test.dll drx)
  use_DynamoRIO_extension(client.drmodtrack-test.dll drmgr)
endif (NOT RISCV64)
if (X86) # FIXME i#1551, i#1569: port to ARM and AArch64
  # We need to load w/ the same base so the test passes
  set(DynamoRIO_SET_PREFERRED_BASE ON)
  if (X64)
    # We need this higher than DR's vmm.
    set(PREFERRED_BASE 0xabc00000)
  else ()
    set(PREFERRED_BASE 0x6f000000)
  endif ()
  # Pass extra options to first run so this will pass when run repeatedly in local
  # build dir w/o loading any pcache
  tobuild_ci(client.pcache client-interface/pcache.c ""
    "-persist -no_use_persisted -no_coarse_disk_merge -no_coarse_lone_merge" "")
  use_DynamoRIO_extension(client.pcache.dll drcontainers)
  if (WIN32)
    append_link_flags(client.pcache "/dynamicbase:no")
  endif ()
  if (UNIX AND no_pie_avail)
    # i#2868: pcache tests are failing with ET_DYN so we disable PIE.
    set_source_files_properties(client-interface/pcache.c PROPERTIES COMPILE_FLAGS
      "${CMAKE_C_FLAGS} -fno-pie")
    append_link_flags(client.pcache "-no-pie")
  endif ()
  torunonly_ci(client.pcache-use client.pcache client.pcache.dll
    client-interface/pcache.c "" "-persist" "")
  # XXX: we should have the key be the default for the .expect but
  # currently that's not the case (thread-reset, etc.).
  set(client.pcache-use_expectbase "pcache-use")
  # when running tests in parallel: have to generate pcaches first
  set(client.pcache-use_depends client.pcache)
  set(DynamoRIO_SET_PREFERRED_BASE OFF)
endif (X86)

if (AARCHXX)
  tobuild_ci(client.stolen-reg client-interface/stolen-reg.c "" "" "")
  link_with_pthread(client.stolen-reg)

  tobuild_ci(client.ldstex client-interface/ldstex.c "" "" "")
  link_with_pthread(client.ldstex)
endif ()

if (ARM AND NOT ANDROID)
  # i#2580: DT_RUNPATH is not yet supported on android so we disable this test on
  # android
  tobuild_api(api.it api/it_arm.c "" "" OFF OFF OFF)
endif(ARM AND NOT ANDROID)

if (ARM)
  if (NOT ANDROID) # XXX i#1874: get working on Android
    # Helper exe, used offline to build smaller input binary to test itself:
    add_api_exe(api.dis-create api/dis-create.c OFF OFF)
    # XXX i#1686: get -syntax_arm working and test it further here.
    # For now our template is DR syntax.
    add_api_exe(api.dis api/dis.c OFF OFF)
    torunonly_api(api.disA32 api.dis api/dis.c ""
      "${CMAKE_CURRENT_SOURCE_DIR}/api/dis-armA32-randtest.raw;-arm" OFF OFF)
    set(api.disA32_expectbase "dis-armA32")
    torunonly_api(api.disT32 api.dis api/dis.c ""
      "${CMAKE_CURRENT_SOURCE_DIR}/api/dis-armT32-randtest.raw;-thumb" OFF OFF)
    set(api.disT32_expectbase "dis-armT32")
  endif ()
elseif (AARCH64)
  add_api_exe(api.dis-a64 api/dis-a64.c OFF OFF)
  torunonly_api(api.dis-a64 api.dis-a64 api/dis-a64.c ""
    "-q;${CMAKE_CURRENT_SOURCE_DIR}/api/dis-a64.txt" OFF OFF)
  torunonly_api(api.dis-a64-v82 api.dis-a64 api/dis-a64.c ""
    "-q;${CMAKE_CURRENT_SOURCE_DIR}/api/dis-a64-v82.txt" OFF OFF)
  torunonly_api(api.dis-a64-v83 api.dis-a64 api/dis-a64.c ""
    "-q;${CMAKE_CURRENT_SOURCE_DIR}/api/dis-a64-v83.txt" OFF OFF)
  torunonly_api(api.dis-a64-v84 api.dis-a64 api/dis-a64.c ""
    "-q;${CMAKE_CURRENT_SOURCE_DIR}/api/dis-a64-v84.txt" OFF OFF)
  torunonly_api(api.dis-a64-v86 api.dis-a64 api/dis-a64.c ""
    "-q;${CMAKE_CURRENT_SOURCE_DIR}/api/dis-a64-v86.txt" OFF OFF)
  torunonly_api(api.dis-a64-sve api.dis-a64 api/dis-a64.c ""
    "-q;${CMAKE_CURRENT_SOURCE_DIR}/api/dis-a64-sve.txt" OFF OFF)
  torunonly_api(api.dis-a64-sve2 api.dis-a64 api/dis-a64.c ""
    "-q;${CMAKE_CURRENT_SOURCE_DIR}/api/dis-a64-sve2.txt" OFF OFF)
  add_api_exe(api.reenc-a64 api/reenc-a64.c OFF OFF)
  tobuild_api(api.opnd api/opnd-a64.c "" "" OFF OFF OFF)
elseif (X86)
  tobuild_api(api.dis api/dis.c "-syntax_intel"
    "${CMAKE_CURRENT_SOURCE_DIR}/api/dis-udis86-randtest.raw" OFF OFF OFF)
  if (X64)
    set(api.dis_expectbase "dis-x64")
  else ()
    set(api.dis_expectbase "dis-x86")
  endif ()
endif (ARM)

if (NOT RISCV64) # TODO i#3544: Port tests to RISC-V 64
  tobuild_api(api.startstop api/startstop.c "" "" OFF OFF OFF)
  link_with_pthread(api.startstop)
  tobuild_api(api.detach api/detach.c "" "" OFF OFF OFF)
  link_with_pthread(api.detach)
endif (NOT RISCV64)
if (LINUX AND X64 AND NOT RISCV64) # Will take extra work to port to 32-bit.
  # TODO i#3544: Port tests to RISC-V 64
  tobuild_api(api.detach_state api/detach_state.c "" "" OFF ON OFF)
  target_include_directories(api.detach_state PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/api)
  link_with_pthread(api.detach_state)
  set_source_files_properties(detach_state.c_asm.asm APPEND_LIST PROPERTIES
    OBJECT_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/api/detach_state_shared.h")
  set_avx_flags(api.detach_state)
endif ()
if (UNIX AND NOT RISCV64)
  if (X64) # TODO i#4664: Fix crash in vsyscall hook under native threads.
    tobuild_api(api.detach_signal api/detach_signal.cpp "" "" OFF OFF OFF)
    link_with_pthread(api.detach_signal)
  endif ()
endif ()
if (NOT WIN32 AND NOT RISCV64) # XXX i#2611: fix for Windows
  # TODO i#3544: Port tests to RISC-V 64
  if (X64)
    set(detach_spawn_name api.detach_spawn)
  else ()
    # FIXME i#2694: failing sometimes on 32-bit Linux.
    set(detach_spawn_name api.detach_spawn_FLAKY)
  endif ()
  set(detach_spawn_stress_name api.detach_spawn_stress_FLAKY)
  set(detach_spawn_quick_exit_name api.detach_spawn_quick_exit)
  tobuild_api(${detach_spawn_name} api/detach_spawn.c "" "" OFF OFF OFF)
  link_with_pthread(${detach_spawn_name})
  tobuild_api(${detach_spawn_stress_name} api/detach_spawn_stress.c "" "" OFF OFF OFF)
  link_with_pthread(${detach_spawn_stress_name})
  tobuild_api(${detach_spawn_quick_exit_name} api/detach_spawn_quick_exit.c "" ""
    OFF OFF OFF)
  link_with_pthread(${detach_spawn_quick_exit_name})
endif ()
if (X86)
  if (X64)
    tobuild_api(decenc.drdecode_decenc_x86_64
      ../../third_party/binutils/test_decenc/drdecode_decenc_x86_64.c "" "" OFF OFF OFF)
    target_include_directories(decenc.drdecode_decenc_x86_64 PRIVATE
      ${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/binutils/test_decenc/)
    set(decenc.drdecode_decenc_x86_64_runcmp "${CMAKE_CURRENT_SOURCE_DIR}/runcmp.cmake")
  else ()
    tobuild_api(decenc.drdecode_decenc_x86
      ../../third_party/binutils/test_decenc/drdecode_decenc_x86.c "" "" OFF OFF OFF)
    target_include_directories(decenc.drdecode_decenc_x86 PRIVATE
      ${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/binutils/test_decenc/)
    set(decenc.drdecode_decenc_x86_runcmp "${CMAKE_CURRENT_SOURCE_DIR}/runcmp.cmake")
  endif ()
endif ()
if (X86 OR AARCH64) # Generated code is x86- or AArch64-specific.
  set(checklevel "")
  if (DEBUG)
    # This tests indirect branches between blocks.
    # We use -checklevel 0 to disable the DOCHECK in check_thread_vm_area
    # which makes building 150K bbs very slow.
    set(checklevel "-checklevel 0")
  endif ()

  tobuild_api(api.ibl-stress api/ibl-stress.c
    "-disable_traces -shared_bb_ibt_tables ${checklevel}" "" OFF OFF OFF)
  use_DynamoRIO_extension(api.ibl-stress drcontainers)
  link_with_pthread(api.ibl-stress)

  if (WIN32)
    # On Windows, somehow linking with drcontainers inserts it and drlibc
    # in front of dynamorio.lib, despite being added to the link list later,
    # which results in unresolved symbols.
    # The only fix I could come up with is to force dynamorio.lib earlier
    # using the link flags:
    DynamoRIO_get_full_path(drpath dynamorio "${location_suffix}")
    get_filename_component(drdir ${drpath} DIRECTORY)
    get_filename_component(drname ${drpath} NAME_WE)
    append_link_flags(api.ibl-stress "${drdir}/${drname}.lib")
  endif ()

  if (AARCH64)
    tobuild_api(api.ibl-stress-aarch64-far-link_LONG api/ibl-stress.c
      "-disable_traces -shared_bb_ibt_tables ${checklevel}" "" OFF OFF OFF)
    append_property_string(TARGET api.ibl-stress-aarch64-far-link_LONG COMPILE_FLAGS
      "-DTEST_FAR_LINK_AARCH64")
    set(api.ibl-stress-aarch64-far-link_LONG_timeout 900)
    use_DynamoRIO_extension(api.ibl-stress-aarch64-far-link_LONG drcontainers)
    link_with_pthread(api.ibl-stress-aarch64-far-link_LONG)
  endif ()
endif ()

# XXX: we should expand this test of drsyms standalone to be cross-platform and
# not just check libstdc++-6.dll.
if (WIN32)
  find_program(mingwlib libstdc++-6.dll
    HINTS "c:/cygwin/usr/i686-pc-mingw32/sys-root/mingw/bin"
    DOC "path to MinGW libstdc++-6.dll")
  if (mingwlib)
    message(STATUS "Found ${mingwlib}, enabling api.symtest")
    tobuild_api(api.symtest api/symtest.c "" "${mingwlib}" OFF OFF OFF)
    use_DynamoRIO_extension(api.symtest drsyms)
  endif ()
endif ()

# Test DR as a static library
# XXX i#1996: not fully supported on Android yet
# XXX i#1997: not fully supported on Mac yet
# TODO i#3544: Port tests to RISC-V 64
if (NOT ANDROID AND NOT APPLE AND NOT RISCV64)
  tobuild_api(api.static_startstop api/static_startstop.c "" "" OFF ON OFF)
  target_link_libraries(api.static_startstop ${libmath})
  tobuild_api(api.static_noclient api/static_noclient.c "" "" OFF ON OFF)
  target_link_libraries(api.static_noclient ${libmath})
  tobuild_api(api.static_noinit api/static_noinit.c "" "" OFF ON OFF)
  target_link_libraries(api.static_noinit ${libmath})
  tobuild_api(api.static_detach api/static_detach.c "" "" OFF ON OFF)
  target_link_libraries(api.static_detach ${libmath})
  tobuild_api(api.static_prepop api/static_prepop.c "" "" OFF ON OFF)
  target_link_libraries(api.static_prepop ${libmath})
  tobuild_api(api.static_reattach_client_flags api/static_reattach_client_flags.c
    "" "" OFF ON ON)
  target_link_libraries(api.static_reattach_client_flags ${libmath})

  if (NOT WIN32)
    tobuild_api(api.static_signal api/static_signal.c "" "" OFF ON OFF)
    target_link_libraries(api.static_signal ${libmath})
    link_with_pthread(api.static_signal)
    # i#2119: test invoking the app's handler on a DR fault.
    tobuild_api(api.static_crash api/static_crash.c "-unsafe_crash_process" "" OFF ON OFF)
    target_link_libraries(api.static_crash ${libmath})
    # XXX i#2346: add delayed sideline thread exit on Windows
    # FIXME i#297: static_sideline is flaky and sometimes hangs on exit.
    tobuild_api(api.static_sideline_FLAKY api/static_sideline.c "" "" OFF ON OFF)
    target_link_libraries(api.static_sideline_FLAKY ${libmath})
    link_with_pthread(api.static_sideline_FLAKY)

    # i#3348: Ensure that DR without local symbols hidden is linkable and
    # avoids conflicts (though CMake_symbol_check does most of the conflict
    # checking).

    tobuild_api(api.static_symbols api/static_symbols.c "" "" OFF ON OFF)
    append_link_flags(api.static_symbols "-Wl,--warn-common -Wl,--fatal-warnings")

    # To ensure that the dynamorio_so_start/_end linker variables are actually
    # used in static binaries, we have two tests which run the same "map mixup"
    # logic: one which exits successfully because we provide the linker
    # variables and another which exits with an assertion because we don't.
    set(api.static_maps_mixup_yesvars_expectbase
       "static_maps_mixup_yesvars")
    tobuild_api(api.static_maps_mixup_yesvars
       api/static_maps_mixup.c "" "" OFF ON OFF)
    append_link_flags(api.static_maps_mixup_yesvars
        "-Wl,--defsym,dynamorio_so_start=__executable_start -Wl,--defsym,dynamorio_so_end=end")

    set(api.static_maps_mixup_novars_FLAKY_expectbase
       "static_maps_mixup_novars")
    tobuild_api(api.static_maps_mixup_novars_FLAKY
       api/static_maps_mixup.c "" "" OFF ON OFF)
  endif ()

  if (NOT WIN32 AND NOT RISCV64) # TODO i#4349: Fix re-attach issues to enable.
    # TODO i#3544: Port tests to RISC-V 64
    tobuild_api(api.thread_churn api/thread_churn.c "" "" OFF OFF OFF)
    link_with_pthread(api.thread_churn)
  endif ()
endif ()

if (NOT X64 AND NOT ARM) # FIXME i#1551: port to ARM
  # i#696: Use -thread_private and small fcache units to trigger shifts.  x64
  # does not support fcache unit resizing and won't allow 4k unit sizes, so
  # skip it.
  tobuild_ci(client.fcache_shift client-interface/fcache_shift.c
    "" "-thread_private -cache_bb_unit_init 4K" "")
endif ()

if (X86)
  if (X64) # Tests x86 reachability.
    tobuild_ci(client.reachability client-interface/reachability.c ""
      # The vm_base options were added in order to expose bugs that fail to xl8 reachable
      # addresses to the executable view within the scheme of satisfy_w_xor_x.
      "-no_vm_base_near_app -vm_base 0x100000000" "")
  else ()
    tobuild_ci(client.reachability client-interface/reachability.c "" "" "")
  endif ()
endif ()

if (X86) # FIXME i#1551, i#1569: fix bugs on ARM and AArch64
  tobuild_ci(client.nudge_ex client-interface/nudge_ex.c "" "" "")
  use_DynamoRIO_extension(client.nudge_ex.dll drmgr)
endif (X86)

tobuild_ci(client.app_args client-interface/app_args.c "" "" "Test;Test2;Test3")

if (X86) # FIXME i#1551, i#1569: port asm to ARM and AArch64
  tobuild_ci(client.retaddr client-interface/retaddr.c "" "" "")
  # Debug libc seems to mess up the test.
  use_MT_not_MTd(client-interface/retaddr.c)
endif (X86)

if (X86 AND WIN32) # FIXME port test application to other systems
  tobuild_ci(client.translate_sandbox client-interface/translate_sandbox.c "" "" "")
endif ()

if (NOT ANDROID) # XXX i#1874: get working on Android
  tobuild_ci(client.destructor client-interface/destructor.cpp "" "" "")
endif ()

if (proc_supports_avx512)
  # XXX i#1312: This is untested on Windows.
  tobuild_ci(client.avx512lazy client-interface/avx512lazy.c "" "" "")
  set_target_properties(client.avx512lazy PROPERTIES COMPILE_FLAGS "${CFLAGS_AVX512}")

  # XXX i#1312: This is untested on Windows.
  tobuild_ci(client.avx512lazy-initial client-interface/avx512lazy.c "" "" "")
  set_target_properties(client.avx512lazy-initial PROPERTIES COMPILE_FLAGS
    "${CFLAGS_AVX512}")
  append_property_string(TARGET client.avx512lazy-initial.dll COMPILE_FLAGS
    "${CFLAGS_AVX512} -DCLIENT_COMPILED_WITH_AVX512")

  if (UNIX)
    # XXX i#1312: we do not yet support AVX-512 Windows context switching.
    tobuild_ci(client.avx512ctx client-interface/avx512ctx.c "" "" "")
    set_target_properties(client.avx512ctx PROPERTIES COMPILE_FLAGS "${CFLAGS_AVX512}")
    append_property_string(TARGET client.avx512ctx.dll COMPILE_FLAGS
      "${CFLAGS_AVX512}")
    target_include_directories(client.avx512ctx PRIVATE
      ${CMAKE_CURRENT_SOURCE_DIR}/client-interface)

    # This could be generalized in a new function. Currently this is the only api-style
    # test that also has a client library.
    add_library(api.startstop_avx512lazy.dll SHARED api/startstop_avx512lazy.dll.c)
    setup_test_client_dll_basics(api.startstop_avx512lazy.dll)
    append_property_string(TARGET api.startstop_avx512lazy.dll COMPILE_FLAGS
      "${CFLAGS_AVX512}")
    get_client_path(client_path api.startstop_avx512lazy.dll api.startstop_avx512lazy)
    tobuild_api(api.startstop_avx512lazy api/startstop_avx512lazy.c
      "-client_lib ${client_path}" "" OFF OFF OFF)
    append_property_string(TARGET api.startstop_avx512lazy COMPILE_FLAGS
      "${CFLAGS_AVX512}")
    append_property_string(TARGET api.startstop_avx512lazy.dll COMPILE_FLAGS
      "${CFLAGS_AVX512}")
  endif (UNIX)
endif ()

if (BUILD_SAMPLES)
  # Sanity tests: we run the samples without SHOW_RESULTS (turned off
  # if BUILD_TESTS is on) so we're only ensuring the client doesn't crash.
  # This requires that each sample does not affect stdout and works
  # when given no arguments.
  # We run common.eflags mainly for memtrace and other big clients which
  # are quite slow on common.fib or common.broadfun: just a sanity check
  # after all.
  if (NOT ANDROID) # FIXME i#2130: On Android, copy samples to device and test.
    get_property(sample_list GLOBAL PROPERTY DynamoRIO_sample_list)
    foreach (sample ${sample_list})
      # FIXME i#4392: Remove sanity checks and replace with functioning tests like
      # that done for the opcode_count and memtrace_simple samples.
      torunonly_ci(sample.${sample} common.${control_flags}
        ${sample} common/${control_flags}.c "" "" "")
      if (sample STREQUAL "inscount")
        # test out-of-line clean call
        torunonly_ci(sample.${sample}.cleancall common.${control_flags} ${sample}
          common/${control_flags}.c "" "-opt_cleancall 0" "")
        if (UNIX)
          torunonly_ci(sample.${sample}.prof-pcs.cleancall common.${control_flags}
            ${sample} common/${control_flags}.c "" "-prof_pcs -opt_cleancall 0" "")
          if (NOT X64 AND NOT ARM) # FIXME i#2160: "-thread_private -prof_pcs" support on AArch64
                                   # FIXME i#1551: "-thread_private -prof_pcs" support on ARM
            torunonly_ci(sample.${sample}.prof-pcs.thread-private.cleancall
              common.${control_flags} ${sample} common/${control_flags}.c ""
              "-thread_private -prof_pcs -opt_cleancall 0" "")
          endif (NOT X64 AND NOT ARM)
        endif (UNIX)
      elseif (sample STREQUAL "memval_simple")
        if (X86 AND UNIX)
          tobuild(client.memval-test client-interface/memval-test.c)
          torunonly_ci(sample.${sample} client.memval-test ${sample}
              client-interface/memval-test.c "" "" "")

          torunonly_ci(sample.${sample}_scattergather client.drx-scattergather ${sample}
              client-interface/drx-scattergather.c "" "" "")
          set(sample.${sample}_scattergather_test_sample_client 1)
          if (proc_supports_avx512)
            set(sample.${sample}_scattergather_runavx512 1)
          elseif (proc_supports_avx)
            set(sample.${sample}_scattergather_runavx 1)
          endif ()
        endif (X86 AND UNIX)
      elseif (sample STREQUAL "opcode_count")
        # We do not do a simple sanity test for opcode_count, but a normal test
        # that checks the sample's output.
        # FIXME i#4372: The test is currently only supported on X86 32-bit.
        # The test program is implemented in pure asm (for UNIX).
        if (X86 AND (NOT X64) AND UNIX)
          add_exe(sample.opcode_count-test samples/opcode_count-test.asm)
          set_target_properties(sample.opcode_count-test PROPERTIES LINKER_LANGUAGE C)
          append_link_flags(sample.opcode_count-test "-nostartfiles -nodefaultlibs -static")

          torunonly_ci(sample.${sample} sample.opcode_count-test ${sample}_test
            samples/opcode_count-test.asm "-opcode 5" "" "")
        endif (X86 AND (NOT X64) AND UNIX)
      elseif (sample STREQUAL "memtrace_simple")
        if (X86 AND X64 AND UNIX)
          torunonly_ci(sample.${sample}_repstr allasm_repstr ${sample}_test
            samples/${sample}_repstr "-log_to_stderr" "" "")
        endif ()
      elseif (sample STREQUAL "callstack")
        set(sample.${sample}_expectbase "sample.callstack.control_flags")
      endif()
    endforeach ()
  endif (NOT ANDROID)

  # Test building the samples as an external project (i#1586).
  # This is not a perfect test as the build dir is not an install dir,
  # but this should catch errors in the public samples/CMakeLists.txt.
  # To set up for this, we copied all the sample source files into the build
  # dir, and we also copied the extension headers into the build dir.
  # XXX: we could also execute the resulting clients.
  # XXX i#1588: this does not catch exported-target issues, such as the absolute
  # path problem.  Such things are not easy to test.  Also note that with
  # cmake < 3.x our build-dir IMPORTED_LINK_INTERFACE_LIBRARIES does not
  # list the transitive closure of the dependents, so we end up with link errors
  # if we make a client link drx_static.
  if (NOT ANDROID) # XXX i#1874: get working on Android
    get_property(sample_proj_dir GLOBAL PROPERTY DynamoRIO_sample_proj_dir)
    add_test(samples_proj ${CMAKE_CTEST_COMMAND}
      --build-and-test "${sample_proj_dir}" "${sample_proj_dir}/build_and_test"
      --build-generator ${CMAKE_GENERATOR}
      --build-project DynamoRIO_samples # needed for VS generators
      --build-makeprogram ${CMAKE_MAKE_PROGRAM}
      # If we don't set Debug, we only have RelWithDebInfo available (b/c we
      # collapse configs) but --build-and-test still tries to build Debug:
      --build-options -DDEBUG=ON -DDynamoRIO_DIR:PATH=${public_config_dir})
    if (UNIX AND X86 AND NOT X64)
      set_tests_properties(samples_proj PROPERTIES ENVIRONMENT
        "CFLAGS=-m32;CXXFLAGS=-m32")
    endif ()
  endif ()
endif (BUILD_SAMPLES)

if (BUILD_CLIENTS)

  if (NOT ANDROID) # XXX i#1874: get working on Android
    torunonly_ci(tool.drcov.fib common.fib drcov common/fib.c "" "" "")
    set(tool.drcov.fib_runcmp "${PROJECT_SOURCE_DIR}/clients/drcov/runtest.cmake")
    set(tool.drcov.fib_expectbase "tool.drcov.fib")
    DynamoRIO_get_full_path(tool.drcov.fib_postcmd drcov2lcov "${location_suffix}")

    if (UNIX AND NOT RISCV64) # TODO i#3544: Port tests to RISC-V 64
      # Test an app that executes a pipe syscall for i#5981.
      torunonly_ci(tool.drcov.eintr linux.eintr drcov linux/eintr.c "" "" "")
      set(tool.drcov.eintr_runcmp "${PROJECT_SOURCE_DIR}/clients/drcov/runtest.cmake")
      set(tool.drcov.eintr_expectbase "tool.drcov.eintr")
      DynamoRIO_get_full_path(tool.drcov.eintr_postcmd drcov2lcov "${location_suffix}")
    endif ()
  endif ()

  ###########################################################################
  # drcachesim tests

  if (NOT ANDROID) # Pipes not working on Android yet (i#1874)
    # i#2023: to avoid stale pipe files in /tmp from prior suite runs, we place them
    # in the current build dir where they will be blown away on the next suite run.
    if (UNIX)
      set(IPC_PREFIX "${CMAKE_CURRENT_BINARY_DIR}/")
    else ()
      set(IPC_PREFIX "")
    endif ()

    macro (torunonly_drcachesim testname exetgt sim_ops app_args)
      torunonly_ci(tool.drcachesim.${testname} ${exetgt} drcachesim
        "${testname}.c" # for templatex basename
        "-ipc_name ${IPC_PREFIX}drtestpipe_${testname} ${sim_ops}"
        "" "${app_args}")
      set(tool.drcachesim.${testname}_toolname "drcachesim")
      set(tool.drcachesim.${testname}_basedir
        "${PROJECT_SOURCE_DIR}/clients/drcachesim/tests")
      set(tool.drcachesim.${testname}_rawtemp ON) # no preprocessor
      set(tool.drcachesim.${testname}_self_serial ON)
    endmacro (torunonly_drcachesim)

    set(config_files_dir ${PROJECT_SOURCE_DIR}/clients/drcachesim/tests)

    # We have a couple of sanity checks that at least nothing crashes.
    # We also test some runtime parameters.
    torunonly_drcachesim(simple ${ci_shared_app} "" "")

    # Simple test that reads the cache configuration from a config file.
    torunonly_drcachesim(simple-config-file ${ci_shared_app}
      "-config_file ${config_files_dir}/cores-1-levels-3-no-missfile.conf"
      "")

    # TLB simulator's single-thread sanity check
    torunonly_drcachesim(TLB-simple ${ci_shared_app} "-simulator_type TLB" "")

    # Test that -LL_miss_file at least doesn't crash.  It's not easy to test
    # much further.
    torunonly_drcachesim(missfile ${ci_shared_app}
      "-LL_miss_file ${CMAKE_CURRENT_BINARY_DIR}/drtestmf.gz" "")
    set(tool.drcachesim.missfile_source simple) # Share simple template.

    # Test miss file production. Test reads the cache configuration from a config file.
    torunonly_drcachesim(missfile-config-file ${ci_shared_app}
      "-config_file ${config_files_dir}/cores-1-levels-3-with-missfile.conf"
      "")

    if (LINUX) # Physaddr access limited to Linux.
      # XXX i#4014: We should verify that we're actually getting physical addresses:
      # but A) there may not be pagemap access for automated tests and B) we
      # would need an analyzer to go examine addresses, or to use the view tool.
      # For now these are just sanity tests that flipping on the option doesn't
      # cause outright failure.
      set(tool.drcachesim.phys_SUDO_sudo ON)
      set(tool.drcachesim.phys_SUDO_expectbase "phys")
      torunonly_drcachesim(phys_SUDO ${ci_shared_app} "-use_physical" "")
      set(tool.drcachesim.phys-threads_SUDO_sudo ON)
      set(tool.drcachesim.phys-threads_SUDO_expectbase "phys-threads")
      torunonly_drcachesim(phys-threads_SUDO client.annotation-concurrency "-use_physical"
        "${annotation_test_args_shorter}")
    endif ()

    set(test_mode_flag "-test_mode")
    torunonly_drcachesim(filter-simple ${ci_shared_app}
      "-L0_filter ${test_mode_flag}" "")
    if (DEBUG AND DR_HOST_X86 AND DR_HOST_X64 AND LINUX)
      # If we have a pure-asm app with a constant instr count, check that count.
      torunonly_drcachesim(filter-asm allasm_x86_64
        "-L0_filter -test_mode -test_mode_name filter_asm_instr_count" "")
    endif ()
    torunonly_drcachesim(filter-no-i ${ci_shared_app}
      "-L0I_filter -L0I_size 0 -simulator_type basic_counts ${test_mode_flag}" "")
    torunonly_drcachesim(filter-i ${ci_shared_app}
      "-L0I_filter -L0I_size 1024 -simulator_type basic_counts ${test_mode_flag}" "")
    set(tool.drcachesim.filter-i_expectbase "basic-counts-generic")
    torunonly_drcachesim(filter-no-d ${ci_shared_app}
      "-L0D_filter -L0D_size 0 -simulator_type basic_counts ${test_mode_flag}" "")
    torunonly_drcachesim(filter-d ${ci_shared_app}
      "-L0D_filter -L0D_size 1024 -simulator_type basic_counts ${test_mode_flag}" "")
    set(tool.drcachesim.filter-d_expectbase "basic-counts-generic")

    torunonly_drcachesim(instr-only-trace ${ci_shared_app} "-instr_only_trace" "")

    # __builtin_prefetch used in the test is not defined on MSVC.
    if (NOT MSVC)
      add_exe(builtin_prefetch ${PROJECT_SOURCE_DIR}/clients/drcachesim/tests/builtin_prefetch.c)
      torunonly_drcachesim(builtin-prefetch-basic-counts builtin_prefetch "-simulator_type basic_counts" "")
      unset(tool.drcachesim.builtin-prefetch-basic-counts_rawtemp) # use preprocessor
    endif ()

    torunonly_drcachesim(delay-simple ${ci_shared_app}
      "-trace_after_instrs 20000 -exit_after_tracing 10000" "")

    # We use a many-threaded test with a small max and test that we only see
    # 1 thread, testing the thread ignore logic.  The max should be small enough
    # to not be flaky on any platform.
    torunonly_drcachesim(delay-global client.annotation-concurrency
      "-simulator_type basic_counts -trace_after_instrs 20K -max_global_trace_refs 10K"
      "${annotation_test_args_shorter}")

    torunonly_drcachesim(windows-simple ${ci_shared_app}
      "-trace_after_instrs 20K -trace_for_instrs 5K -retrace_every_instrs 35K -simulator_type basic_counts" "")

    # Test that "Warmup hits" and "Warmup misses" are printed out
    torunonly_drcachesim(warmup-valid ${ci_shared_app} "-warmup_refs 1" "")

    # Test that warmup was enabled but not triggered.
    torunonly_drcachesim(warmup-zeros ${ci_shared_app} "-warmup_refs 1000000000" "")

    # Our pthreads tests don't have many threads so we run this annot test,
    # though it is a little slow under drcachesim.
    # XXX i#1703: this may be too flaky: we may want to remove this once
    # we add some dedicated multi-thread (and multi-process) tests with
    # more deterministic output.
    # We test -cpu_scheduling on several tests with many threads.
    # We use a smaller test (_shorter) to avoid taking multiple minutes on our
    # CI (i#4059; xref i#2063).
    torunonly_drcachesim(threads client.annotation-concurrency "-cpu_scheduling"
      "${annotation_test_args_shorter}")

    # Threads test that reads the cache configuration from a config file.
    torunonly_drcachesim(threads-with-config-file client.annotation-concurrency
      "-config_file ${config_files_dir}/cores-1-levels-3-no-missfile.conf"
      "${annotation_test_args_shorter}")
    set(tool.drcachesim.threads_timeout 150) # This test is long.

    torunonly_drcachesim(coherence client.annotation-concurrency "-coherence"
      "${annotation_test_args_shorter}")
    set(tool.drcachesim.coherence_timeout 150) # This test is long.

    # TLB simulator's multi-thread sanity check
    torunonly_drcachesim(TLB-threads client.annotation-concurrency
      "-simulator_type TLB -cpu_scheduling" "${annotation_test_args_shorter}")
    # i#2063: this test can time out.
    set(tool.drcachesim.TLB-threads_timeout 150)

    if (ARM)
      torunonly_drcachesim(allasm-thumb common.allasm_thumb "" "")
      torunonly_drcachesim(allasm-arm common.allasm_arm "" "")
    endif ()

    if (AARCH64)
      torunonly_drcachesim(allasm-aarch64-cache common.allasm_aarch64_cache
        # XXX i#2676: We disable the data prefetcher until someone with A64
        # hardware gets a chance to update the expected output.
        "-data_prefetcher none" "")

      torunonly_drcachesim(allasm-aarch64-prefetch-basic-counts allasm_aarch64_prefetch
        "-simulator_type basic_counts" "")

      torunonly_drcachesim(allasm-aarch64-flush-basic-counts allasm_aarch64_flush
        "-simulator_type basic_counts" "")
    endif ()

    # XXX i#1703: add more dedicated tests (in suite/tests/clients/) for which
    # we know the results, and check the output.
    # We have one so far:
    if (UNIX) # FIXME i#1727: on Windows the child fails to open the pipe.
      add_exe(tool.multiproc
        ${PROJECT_SOURCE_DIR}/clients/drcachesim/tests/multiproc.c)
      get_target_path_for_execution(tool.multiproc_path tool.multiproc "${location_suffix}")
      torunonly_drcachesim(multiproc tool.multiproc "" "${tool.multiproc_path}")
    endif ()

    # Test the cache miss analyzer.
    if (UNIX)
      add_exe(stride_benchmark
        ${PROJECT_SOURCE_DIR}/clients/drcachesim/tests/stride_benchmark.cpp)
      torunonly_drcachesim(miss_analyzer stride_benchmark
        "-simulator_type miss_analyzer -miss_count_threshold 5000 -miss_frac_threshold 0.25" "")
    endif ()

    # Test other analysis tools
    macro (torunonly_simtool testname exetgt sim_ops app_args)
      torunonly_ci(tool.${testname} ${exetgt} drcachesim
        "${testname}.c" # for templatex basename
        "-ipc_name ${IPC_PREFIX}drtestpipe_${testname} ${sim_ops}" "" "${app_args}")
      set(tool.${testname}_toolname "drcachesim")
      set(tool.${testname}_basedir "${PROJECT_SOURCE_DIR}/clients/drcachesim/tests")
    endmacro (torunonly_simtool)

    torunonly_simtool(histogram ${ci_shared_app}
      "-simulator_type histogram -report_top 20" "")

    torunonly_simtool(reuse_distance ${ci_shared_app}
      "-simulator_type reuse_distance -reuse_distance_threshold 256" "")

    # We run common.decode-bad to test markers for faults
    if (X86) # decode-bad is x86-only
      torunonly_simtool(basic_counts common.decode-bad
        "-simulator_type basic_counts" "")
    endif ()

    if (X86 AND X64) # We only bother with a sample trace for x86_64.
      # We test with a fixed trace file so we can test exact numeric results.
      set(small_trace_file
        "${PROJECT_SOURCE_DIR}/clients/drcachesim/tests/drmemtrace.small.x64.trace")
      torunonly_simtool(reuse_offline ${ci_shared_app}
        "-infile ${small_trace_file} -simulator_type reuse_distance -reuse_distance_histogram" "")

      # Our multi-threaded sample trace is larger so we require gzip.
      if (ZLIB_FOUND)
        set(thread_trace_dir
          "${PROJECT_SOURCE_DIR}/clients/drcachesim/tests/drmemtrace.threadsig.x64.tracedir")
        torunonly_simtool(reuse_offline_threads ${ci_shared_app}
          "-indir ${thread_trace_dir} -simulator_type reuse_distance -reuse_distance_histogram" "")
        set(tool.reuse_offline_threads_rawtemp ON) # no preprocessor

        torunonly_simtool(reuse_time_offline ${ci_shared_app}
          "-indir ${thread_trace_dir} -simulator_type reuse_time" "")
        set(tool.reuse_time_offline_rawtemp ON) # no preprocessor

        torunonly_simtool(counts_only_thread ${ci_shared_app}
          "-indir ${thread_trace_dir} -simulator_type basic_counts -only_thread 1257604"
          "")
        set(tool.counts_only_thread_rawtemp ON) # no preprocessor
      endif ()
    endif ()

    # We run an app w/ kernel xfers to test invariant_checker online.
    # Offline invariants are tested in the tool.histogram.offline test below.
    if (UNIX)
      # The signal_invariants asm is x86-only.  The extra checks are cross-arch
      # so we do not try to port it to ARM or Mac but use a different app there.
      if (X86 AND NOT APPLE)
        add_exe(drmemtrace.signal_invariants
          "${PROJECT_SOURCE_DIR}/clients/drcachesim/tests/signal_invariants.c")
        link_with_pthread(drmemtrace.signal_invariants)
        set(kernel_xfer_app drmemtrace.signal_invariants)
      else ()
        set(kernel_xfer_app pthreads.ptsig)
      endif ()
    else ()
      set(kernel_xfer_app client.winxfer) # We want threads and xfers.
    endif()
    if (DEBUG) # for -test_mode
      torunonly_drcachesim(invariants ${kernel_xfer_app}
        "-test_mode -test_mode_name kernel_xfer_app" "")
      # This is slow on Windows.  I tried reducing the tracing window but
      # it seems to vary too much across machines, and if we delay too far
      # we start in the middle of a callback.
      set(tool.drcachesim.invariants_timeout 180)
    endif ()

    if (NOT MACOS AND X86)
      torunonly_drcachesim(scattergather client.drx-scattergather
        "-simulator_type basic_counts" "")
      unset(tool.drcachesim.scattergather_rawtemp) # use preprocessor
      if (proc_supports_avx512)
        set(tool.drcachesim.scattergather_runavx512 1)
      elseif (proc_supports_avx)
        set(tool.drcachesim.scattergather_runavx 1)
      endif ()
    endif ()

    if (NOT RISCV64)
      # Test offline traces.
      # XXX: we could exclude the pipe files and build the offline trace
      # support by itself for Android.
      # TODO i#3544: Port tests to RISC-V 64
      get_target_path_for_execution(drcachesim_path drcachesim "${location_suffix}")
      prefix_cmd_if_necessary(drcachesim_path ON ${drcachesim_path})
      get_target_path_for_execution(drraw2trace_path drraw2trace "${location_suffix}")
      prefix_cmd_if_necessary(drraw2trace_path ON ${drraw2trace_path})

      torunonly_drcachesim(opcode_mix ${ci_shared_app}
        "-instr_encodings -simulator_type opcode_mix" "")
    endif (NOT RISCV64)
    # The sim_atops should start with @ and be in @-as-space format (hence "atops").
    # If the exetgt has drcachesim statically linked in, the _nodr property must be
    # set *before* invoking this macro.
    macro (torunonly_drcacheoff testname exetgt tracer_ops sim_atops app_args)
      set(testname_full "tool.drcacheoff.${testname}")
      if (${testname_full}_nodr)
        # Static apps do not receive our -subdir_prefix param, but they have unique
        # executable names so we're ok.
        set(dir_prefix "drmemtrace.${testname_full}")
      else ()
        set(dir_prefix "${testname_full}")
      endif ()
      set(extra_ops "")
      if (WIN32 AND NOT ${testname_full}_full_run)
        # Shrink test times on Appveyor.
        set(extra_ops "-max_trace_size 1M")
      endif ()
      torunonly_ci(tool.drcacheoff.${testname} ${exetgt} drcachesim
        "offline-${testname}.c" # for templatex basename
        # Set a test-unique prefix to avoid races removing/finding output.
        "-offline -subdir_prefix ${testname_full} ${extra_ops} ${tracer_ops}"
        "" "${app_args}")
      set(${testname_full}_toolname "drcachesim")
      set(${testname_full}_basedir "${PROJECT_SOURCE_DIR}/clients/drcachesim/tests")
      set(${testname_full}_rawtemp ON) # no preprocessor
      set(${testname_full}_runcmp "${CMAKE_CURRENT_SOURCE_DIR}/runmulti.cmake")
      # Support for running under sudo for -use_physical.
      # Each command is assumed to need sudo.
      if (DEFINED ${testname_full}_sudo)
        set(cmd_pfx "sudo@")
      else ()
        set(cmd_pfx "")
      endif ()
      set(${testname_full}_precmd
        "foreach@${cmd_pfx}${CMAKE_COMMAND}@-E@remove_directory@${dir_prefix}.*.dir")
      if (${testname_full}_nopost)
        # No post needed.
      else ()
        set(${testname_full}_postcmd
          "firstglob@${cmd_pfx}${drcachesim_path}@-indir@${dir_prefix}.*.dir${sim_atops}")
      endif ()
      set(${testname_full}_self_serial ON)
    endmacro()

    # We could share drcachesim-simple.templatex if we had the launcher fork
    # and print out the "---- <application exited with code 0> ----".
    torunonly_drcacheoff(simple ${ci_shared_app} ""
      # We pass a small instr count to test multiple chunks in a zipfile.
      "@-chunk_instr_count@10K" "")

    if (UNIX)
      # Test an app with a fork.
      torunonly_drcacheoff(fork linux.fork "" "" "")
    endif ()

    # Test reading a legacy pre-interleaved file.
    if (ZLIB_FOUND)
      torunonly_api(tool.drcacheoff.legacy "${drcachesim_path}" "offline-legacy.c" ""
        "-infile;${PROJECT_SOURCE_DIR}/clients/drcachesim/tests/offline-legacy-trace.gz"
        OFF OFF)
      set(tool.drcacheoff.legacy_basedir
        "${PROJECT_SOURCE_DIR}/clients/drcachesim/tests")
      set(tool.drcacheoff.legacy_rawtemp ON) # no preprocessor
    endif ()

    # Sanity tests for compression of raw output files.
    if (libsnappy)
      torunonly_drcacheoff(raw-snappy ${ci_shared_app} "-raw_compress snappy" "" "")
      set(tool.drcacheoff.raw-snappy_expectbase "offline-simple")
      torunonly_drcacheoff(raw-snappy-nocrc ${ci_shared_app}
        "-raw_compress snappy_nocrc" "" "")
      set(tool.drcacheoff.raw-snappy-nocrc_expectbase "offline-simple")
    endif ()
    if (ZLIB_FOUND)
      torunonly_drcacheoff(raw-zlib ${ci_shared_app} "-raw_compress zlib" "" "")
      set(tool.drcacheoff.raw-zlib_expectbase "offline-simple")
      torunonly_drcacheoff(raw-gzip ${ci_shared_app} "-raw_compress gzip" "" "")
      set(tool.drcacheoff.raw-gzip_expectbase "offline-simple")
    endif ()
    # lz4 is on by default so we test no compression here.
    torunonly_drcacheoff(raw-none ${ci_shared_app} "-raw_compress none" "" "")
    set(tool.drcacheoff.raw-none_expectbase "offline-simple")

    # Test reading a trace in sharded snappy-compressed files.
    if (libsnappy)
      # with a parallel tool (basic_counts)
      torunonly_api(tool.drcacheoff.snappy "${drcachesim_path}" "offline-snappy.c" ""
        "-indir;${PROJECT_SOURCE_DIR}/clients/drcachesim/tests/drmemtrace.chase-snappy.x64.tracedir;-simulator_type;basic_counts"
        OFF OFF)

      # with a legacy serial tool (full simulator)
      torunonly_api(tool.drcacheoff.snappy "${drcachesim_path}" "offline-snappy-serial.c" ""
        "-indir;${PROJECT_SOURCE_DIR}/clients/drcachesim/tests/drmemtrace.chase-snappy.x64.tracedir"
        OFF OFF)
      set(tool.drcacheoff.snappy_basedir
        "${PROJECT_SOURCE_DIR}/clients/drcachesim/tests")
    endif()

    if (UNIX) # Enable on Windows once i#1727 is fixed.
      # FIXME i#2384: we need to combine the subdir from the child with the
      # parent in some way and update the templatex file to include both.
      # Right now we're only ensuring this doesn't crash and that one of
      # the processes produced traces.
      torunonly_drcacheoff(multiproc tool.multiproc "" "" "${tool.multiproc_path}")
    endif ()

    torunonly_drcacheoff(filter ${ci_shared_app} "-L0_filter ${test_mode_flag}" "" "")
    torunonly_drcacheoff(filter-no-i ${ci_shared_app}
      "-L0I_filter -L0I_size 0 ${test_mode_flag}" "@-simulator_type@basic_counts" "")
    torunonly_drcacheoff(filter-i ${ci_shared_app}
      "-L0I_filter -L0I_size 1024 ${test_mode_flag}" "@-simulator_type@basic_counts" "")
    set(tool.drcacheoff.filter-i_expectbase "offline-basic-counts-generic")
    torunonly_drcacheoff(filter-no-d ${ci_shared_app}
      "-L0D_filter -L0D_size 0 ${test_mode_flag}" "@-simulator_type@basic_counts" "")
    torunonly_drcacheoff(filter-d ${ci_shared_app}
      "-L0D_filter -L0D_size 1024 ${test_mode_flag}" "@-simulator_type@basic_counts" "")
    set(tool.drcacheoff.filter-d_expectbase "offline-basic-counts-generic")

    torunonly_drcacheoff(instr-only-trace ${ci_shared_app} "-instr_only_trace" "" "")
    torunonly_drcacheoff(filter-and-instr-only-trace ${ci_shared_app} "-instr_only_trace -L0_filter" "" "")

    if (LINUX) # Test uses Linux-specific timer code.
      if (NOT RISCV64) # TODO i#3544: Port tests to RISC-V 64
        torunonly_drcacheoff(sysnums linux.signal_pre_syscall ""
          "@-simulator_type@basic_counts@${test_mode_flag}" "")
      endif ()
    endif ()

    torunonly_drcacheoff(interval-count-output ${ci_shared_app} ""
      "@-simulator_type@basic_counts@-interval_microseconds@1M" "")

    # As for the online test, we check that only 1 thread is in the final trace.
    torunonly_drcacheoff(max-global client.annotation-concurrency
      # Include function tracing to sanity test combining with delay and max.
      "-trace_after_instrs 20K -max_global_trace_refs 10K -record_heap"
      "@-simulator_type@basic_counts" "${annotation_test_args_shorter}")
    set(tool.drcacheoff.max-global_timeout 240) # Can take >120s.

    torunonly_drcacheoff(delay-func ${ci_shared_app}
      # Delay enough that zero data should be logged to test that function
      # tracing is delayed (i#4893).
      # Update: drmemtrace now does not even create an output file if tracing
      # never starts, so this now serves as a test of that.  It only tests
      # delayed function tracing if enough such tracing accumulated to output
      # on its own which would assert on not having a file.
      # This is also large enough to test the (non-triggering portion of) the
      # per-thread counter feature (i#5026).
      "-trace_after_instrs 200M -record_heap"
      "@-simulator_type@basic_counts" "")
    # There is now no data file at all: which we test by echoing the glob.
    set(tool.drcacheoff.delay-func_nopost ON)
    set(tool.drcacheoff.delay-func_postcmd
      "${CMAKE_COMMAND}@-E@echo@${dir_prefix}.*.dir/raw/*raw*")
    set(tool.drcacheoff.delay-func_timeout 240) # Can take >90s.

    torunonly_drcacheoff(windows-simple ${ci_shared_app}
      "-no_split_windows -trace_after_instrs 20K -trace_for_instrs 5K -retrace_every_instrs 35K"
      "@-simulator_type@basic_counts" "")
    if (ZLIB_FOUND)
      torunonly_drcacheoff(windows-zlib ${ci_shared_app}
        "-raw_compress zlib -no_split_windows -trace_after_instrs 20K -trace_for_instrs 5K -retrace_every_instrs 35K"
        "@-simulator_type@basic_counts" "")
      set(tool.drcacheoff.windows-zlib_expectbase "offline-windows-simple")
      torunonly_drcacheoff(windows-gzip ${ci_shared_app}
        "-raw_compress gzip -no_split_windows -trace_after_instrs 20K -trace_for_instrs 5K -retrace_every_instrs 35K"
        "@-simulator_type@basic_counts" "")
      set(tool.drcacheoff.windows-gzip_expectbase "offline-windows-simple")
    endif ()
    # lz4 is on by default so we test no compression here.
    torunonly_drcacheoff(windows-none ${ci_shared_app}
      "-raw_compress none -no_split_windows -trace_after_instrs 20K -trace_for_instrs 5K -retrace_every_instrs 35K"
      "@-simulator_type@basic_counts" "")
    set(tool.drcacheoff.windows-none_expectbase "offline-windows-simple")
    # Ensure the invariant checker handles window transitions.
    torunonly_drcacheoff(windows-invar ${ci_shared_app}
      "-no_split_windows -trace_after_instrs 20K -trace_for_instrs 5K -retrace_every_instrs 35K"
      "@-simulator_type@invariant_checker" "")
    if (X86 AND X64 AND UNIX)
      torunonly_drcacheoff(windows-asm allasm_repstr
        "-no_split_windows -trace_after_instrs 3 -trace_for_instrs 4 -retrace_every_instrs 4"
        "@-simulator_type@basic_counts" "")
    endif ()
    # Test split-file windows.
    torunonly_drcacheoff(windows-split ${ci_shared_app}
      "-trace_after_instrs 20K -trace_for_instrs 10K -retrace_every_instrs 30K"
      "@-simulator_type@basic_counts" "")
    # The first postcmd did window #0.  There should be at least 2 more windows.
    set(tool.drcacheoff.windows-split_postcmd2
      "firstglob@${drcachesim_path}@-indir@${dir_prefix}.*.dir/raw/window.0001@-simulator_type@basic_counts")
    set(tool.drcacheoff.windows-split_postcmd3
      "firstglob@${drcachesim_path}@-indir@${dir_prefix}.*.dir/raw/window.0002@-simulator_type@basic_counts")

    # Test L0_filter_until_instrs
    torunonly_drcacheoff(L0-filter-until-instrs-windows-split ${ci_shared_app}
        "-trace_after_instrs 10K -L0_filter_until_instrs 10K -trace_for_instrs 10K -retrace_every_instrs 10K"
      "@-simulator_type@basic_counts" "")
    # The first postcmd did window #0.  There should be at least 2 more windows.
    set(tool.drcacheoff.L0-filter-until-instrs-windows-split_postcmd2
      "firstglob@${drcachesim_path}@-indir@${dir_prefix}.*.dir/raw/window.0001@-simulator_type@basic_counts")
    set(tool.drcacheoff.L0-filter-until-instrs-windows-split_postcmd3
      "firstglob@${drcachesim_path}@-indir@${dir_prefix}.*.dir/raw/window.0002@-simulator_type@basic_counts")

    torunonly_drcacheoff(L0-filter-until-instrs-2 ${ci_shared_app}
        "-trace_after_instrs 10 -L0_filter_until_instrs 10K -trace_for_instrs 10K -retrace_every_instrs 10K"
      "@-simulator_type@basic_counts" "")

    torunonly_drcacheoff(L0-filter-until-instrs-exit-after ${ci_shared_app}
        "-trace_after_instrs 10K -L0_filter_until_instrs 10K -exit_after_tracing 10K"
      "@-simulator_type@basic_counts" "")

    torunonly_drcacheoff(L0-filter-until-instrs-max-refs ${ci_shared_app}
        "-trace_after_instrs 10K -L0_filter_until_instrs 10K -max_global_trace_refs 10K"
      "@-simulator_type@basic_counts" "")

    torunonly_drcacheoff(L0-filter-until-instrs-max-trace-size ${ci_shared_app}
        "-trace_after_instrs 10K -L0_filter_until_instrs 10K -max_trace_size 10K"
      "@-simulator_type@basic_counts" "")

    # pthreads not available with MSVC
    if (NOT MSVC)
    set(ci_pthreads_app pthreads.pthreads)
    # pthreads tests
    torunonly_drcacheoff(warmup-pthreads-windows-split ${ci_pthreads_app}
        "-trace_after_instrs 10K -L0_filter_until_instrs 100K -trace_for_instrs 10K -retrace_every_instrs 10K"
      "@-simulator_type@basic_counts" "")
    # The first postcmd did window #0.  There should be at least 2 more windows.
    set(tool.drcacheoff.warmup-pthreads-windows-split_postcmd2
      "firstglob@${drcachesim_path}@-indir@${dir_prefix}.*.dir/raw/window.0001@-simulator_type@basic_counts")
    set(tool.drcacheoff.warmup-pthreads-windows-split_postcmd3
      "firstglob@${drcachesim_path}@-indir@${dir_prefix}.*.dir/raw/window.0002@-simulator_type@basic_counts")

    torunonly_drcacheoff(warmup-pthreads-2 ${ci_pthreads_app}
        "-trace_after_instrs 10 -L0_filter_until_instrs 10K -trace_for_instrs 10K -retrace_every_instrs 10K"
      "@-simulator_type@basic_counts" "")

    torunonly_drcacheoff(warmup-pthreads-max-refs ${ci_pthreads_app}
        "-trace_after_instrs 10K -L0_filter_until_instrs 10K -max_global_trace_refs 300K"
      "@-simulator_type@basic_counts" "20000")

    torunonly_drcacheoff(warmup-pthreads-max-trace-size ${ci_pthreads_app}
        "-trace_after_instrs 10K -L0_filter_until_instrs 10K -max_trace_size 100K"
      "@-simulator_type@basic_counts" "")
    endif ()


    # __builtin_prefetch used in the test is not defined on MSVC.
    if (NOT MSVC)
      torunonly_drcacheoff(builtin-prefetch-basic-counts builtin_prefetch "" "@-simulator_type@basic_counts" "")
      unset(tool.drcacheoff.builtin-prefetch-basic-counts_rawtemp) # use preprocessor
    endif ()

    if (X86 AND X64 AND ZLIB_FOUND)
      # XXX i#5538: Add trace files for other arches.
      set(zip_path
        "${PROJECT_SOURCE_DIR}/clients/drcachesim/tests/drmemtrace.allasm_x86_64.trace.zip")
      torunonly_api(tool.drcacheoff.skip "${drcachesim_path}" "offline-skip" ""
        # Test invariants with global headers after skipping instrs.
        # Here the skip does not find real headers and we expect synthetic.
        "-simulator_type;view;-infile;${zip_path};${test_mode_flag};-skip_instrs;62;-sim_refs;10"
        OFF OFF)
      set(tool.drcacheoff.skip_basedir "${PROJECT_SOURCE_DIR}/clients/drcachesim/tests")
      set(tool.drcacheoff.skip_rawtemp ON) # no preprocessor
      torunonly_api(tool.drcacheoff.skip2 "${drcachesim_path}" "offline-skip2" ""
        # Test invariants with global headers after skipping instrs.
        # Here the skip finds real headers and we expect no synthetic.
        "-simulator_type;view;-infile;${zip_path};${test_mode_flag};-skip_instrs;63;-sim_refs;10"
        OFF OFF)
      set(tool.drcacheoff.skip2_basedir "${PROJECT_SOURCE_DIR}/clients/drcachesim/tests")
      set(tool.drcacheoff.skip2_rawtemp ON) # no preprocessor

      # Ensure -cpu_scheduling follows thread migrations.
      # We have a checked-in trace with a migration.
      set(migrate_path
        "${PROJECT_SOURCE_DIR}/clients/drcachesim/tests/drmemtrace.simple_app.trace.zip")
      torunonly_api(tool.drcacheoff.cpu "${drcachesim_path}" "offline-cpu" ""
        "-cpu_scheduling;-infile;${migrate_path}"
        OFF OFF)
      set(tool.drcacheoff.cpu_basedir "${PROJECT_SOURCE_DIR}/clients/drcachesim/tests")
      set(tool.drcacheoff.cpu_rawtemp ON) # no preprocessor
    endif ()

    # We run common.decode-bad to test markers for faults
    if (X86) # decode-bad is x86-only
      # Do not cap the trace size (done to shorten tests) b/c it will then miss
      # kernel transfer events.
      set(tool.drcacheoff.basic_counts_full_run ON)
      torunonly_drcacheoff(basic_counts common.decode-bad ""
        "@-simulator_type@basic_counts" "")
      unset(tool.drcacheoff.basic_counts_rawtemp)
    endif ()
    if (NOT RISCV64)
      # TODO i#3544: Port tests to RISC-V 64
      torunonly_drcacheoff(opcode_mix ${ci_shared_app} ""
        "@-simulator_type@opcode_mix" "")
      # Ensure the tool works without the raw/ subdir.
      set(tool.drcacheoff.opcode_mix_postcmd
        "firstglob@${drraw2trace_path}@-indir@${dir_prefix}.*.dir")
      set(tool.drcacheoff.opcode_mix_postcmd2
        "foreach@${CMAKE_COMMAND}@-E@remove_directory@${dir_prefix}.*.dir/raw")
      set(tool.drcacheoff.opcode_mix_postcmd3
        "firstglob@${drcachesim_path}@-indir@${dir_prefix}.*.dir@-simulator_type@opcode_mix")

      torunonly_drcacheoff(view ${ci_shared_app} ""
        "@-simulator_type@view@-sim_refs@16384" "")

      set(tool.drcacheoff.func_view_full_run ON) # Fails on Windows if truncated.
      torunonly_drcacheoff(func_view common.fib "-record_function fib|1"
        "@-simulator_type@func_view" "only_5")
    endif (NOT RISCV64)
    if (DR_HOST_X86 AND DR_HOST_X64 AND LINUX)
      # Requires sudo to access pagemap.
      # XXX: Should we not enable this outside of the Github suite where we know
      # we have passwordless sudo?  The pause for a password may cause problems
      # for local interactive test running.
      set(tool.drcacheoff.phys_SUDO_sudo ON)
      set(tool.drcacheoff.phys_SUDO_expectbase "offline-phys")
      torunonly_drcacheoff(phys_SUDO allasm_repstr
        "-use_physical" "@${test_mode_flag}@-simulator_type@view" "")
      set(tool.drcacheoff.phys_nopriv_failok ON)
      set(tool.drcacheoff.phys_nopriv_nopost ON)
      torunonly_drcacheoff(phys_nopriv allasm_x86_64
        "-use_physical" "@${test_mode_flag}@-simulator_type@view" "")
    endif ()

    if (UNIX AND ZLIB_FOUND)
      add_exe(tool.fib_plus
        ${PROJECT_SOURCE_DIR}/clients/drcachesim/tests/fib_plus.c)
      link_with_pthread(tool.fib_plus)
      # We could run this like so, but with threads the output is non-deterministic:
      #    torunonly_drcacheoff(func_view_noret tool.fib_plus
      #      "-record_function fib|1&noret_func|2|noret&noargs|0"
      #      "@-simulator_type@func_view" "")
      # Thus we run this manually and check in the resulting trace.  I ran with
      # a delay to shrink the size, since we are not interesting in the main thread's
      # pre-main() init:
      #     $ bin64/drrun -t drcachesim -offline -trace_after_instrs 210K \
      #       -record_function 'fib|1&noret_func|2|noret&noargs|0' -- \
      #       suite/tests/bin/tool.fib_plus
      #     $ bin64/drrun -t drcachesim -simulator_type func_view -indir \
      #       drmemtrace*.dir/
      torunonly_api(tool.drcacheoff.func_view_noret "${drcachesim_path}"
        "offline-func_view_noret.c" ""
        "-indir;${PROJECT_SOURCE_DIR}/clients/drcachesim/tests/offline-func-trace;-simulator_type;func_view"
        OFF OFF)
      set(tool.drcacheoff.func_view_noret_basedir
        "${PROJECT_SOURCE_DIR}/clients/drcachesim/tests")
      set(tool.drcacheoff.func_view_noret_rawtemp ON) # no preprocessor
    endif ()

    # i#3048: Our mangled operator new/delete tracing is UNIX-only.
    # PR#4178: Despite the aligned-new code in this test being guarded by
    # "if defined(__cpp_aligned_new)" it fails to build on Travis clang-9 and
    # OSX presumably due to runtime vs compiler mismatches.  We just disable
    # for clang for now and can re-enable later w/ more recent runtimes.
    if (UNIX AND NOT CMAKE_COMPILER_IS_CLANG)
      set(heap_test_src ${PROJECT_SOURCE_DIR}/clients/drcachesim/tests/heap_test.cpp)
      add_exe(tool.heap_test ${heap_test_src})
      if (cxx17_available)
        # Enable recent operator new/delete variants.
        set_source_files_properties(${heap_test_src} PROPERTIES
          # XXX: c++17 seems to warn more about the static functions in tools.h.
          # We should move those to tools.c.  For now we disable the warning.
          COMPILE_FLAGS "${ORIG_CMAKE_CXX_FLAGS} -std=c++17 -Wno-unused-function")
      endif ()
      torunonly_drcacheoff(func_view_heap tool.heap_test
        "-record_heap" "@-simulator_type@func_view@-no_show_func_trace" "")
      unset(tool.drcacheoff.func_view_heap_rawtemp) # use preprocessor
    endif ()

    if (LINUX AND X64 AND HAVE_RSEQ)
      torunonly_drcacheoff(rseq linux.rseq
        # Run with -trace_after_instrs to ensure we test the
        # drbbdup + rseq combo (i#5658, i#5659).
        "-trace_after_instrs 5K"
        "@${test_mode_flag}@-test_mode_name@rseq_app" "")
      # Test filtering.
      torunonly_drcacheoff(rseq-filter linux.rseq
        "-trace_after_instrs 5K -L0_filter"
        "@${test_mode_flag}@-test_mode_name@rseq_app" "")
      set(tool.drcacheoff.rseq-filter_expectbase "offline-rseq")
      torunonly_drcacheoff(rseq-dfilter linux.rseq
        "-trace_after_instrs 5K -L0D_filter"
        "@${test_mode_flag}@-test_mode_name@rseq_app" "")
      set(tool.drcacheoff.rseq-dfilter_expectbase "offline-rseq")
    endif ()

    if (AARCH64)
      set(tool.drcacheoff.burst_aarch64_sys_nodr ON)
      torunonly_drcacheoff(burst_aarch64_sys tool.drcacheoff.burst_aarch64_sys ""
        "@-simulator_type@basic_counts" "")
    endif ()

    if (NOT APPLE AND NOT RISCV64) # XXX i#1997: static DR not fully supported on Mac yet
      # We specify this executable here as it uses tools.h code.
      # TODO i#3544: Port tests to RISC-V 64
      set(tool.drcacheoff.gencode_no_reg_compat)
      add_api_exe(tool.drcacheoff.gencode
        ${PROJECT_SOURCE_DIR}/clients/drcachesim/tests/burst_gencode.cpp
        OFF ON)
      use_DynamoRIO_static_client(tool.drcacheoff.gencode drmemtrace_static)
      use_DynamoRIO_drmemtrace_tracer(tool.drcacheoff.gencode)
      target_link_libraries(tool.drcacheoff.gencode drmemtrace_raw2trace
        drmemtrace_analyzer test_helpers)
      target_include_directories(tool.drcacheoff.gencode PUBLIC
        "${PROJECT_SOURCE_DIR}/clients/drcachesim"
        "${PROJECT_SOURCE_DIR}/clients/drcachesim/common"
        "${PROJECT_SOURCE_DIR}/clients/drcachesim/reader"
        "${PROJECT_SOURCE_DIR}/clients/drcachesim/tracer"
        "${PROJECT_SOURCE_DIR}/clients/drcachesim/scheduler")
      set(tool.drcacheoff.gencode_nodr ON)
      torunonly_drcacheoff(gencode tool.drcacheoff.gencode
        "" "@-simulator_type@opcode_mix" "")
      unset(tool.drcacheoff.gencode_rawtemp) # Use preprocessor on template.
    endif ()

    # XXX i#1551: startstop API is NYI on ARM
    # XXX i#1997: dynamorio_static is not supported on Mac yet
    # FIXME i#2949: gcc 7.3 fails to link certain configs
    # TODO i#3544: Port tests to RISC-V 64
    if (NOT ARM AND NOT APPLE AND NOT DISABLE_FOR_BUG_2949 AND NOT RISCV64)
      set(tool.drcacheoff.burst_static_nodr ON)
      torunonly_drcacheoff(burst_static tool.drcacheoff.burst_static "" "" "")

      set(tool.drcacheoff.burst_replace_nodr ON)
      torunonly_drcacheoff(burst_replace tool.drcacheoff.burst_replace "" "" "")

      set(tool.drcacheoff.burst_replaceall_nodr ON)
      torunonly_drcacheoff(burst_replaceall tool.drcacheoff.burst_replaceall ""
        "@-simulator_type@basic_counts" "")
      unset(tool.drcacheoff.burst_replaceall_rawtemp) # use preprocessor

      if (X64 AND UNIX)
        set(tool.drcacheoff.burst_noreach_nodr ON)
        torunonly_drcacheoff(burst_noreach tool.drcacheoff.burst_noreach "" "" "")
      endif ()
      if (LINUX)
        set(tool.drcacheoff.burst_maps_nodr ON)
        torunonly_drcacheoff(burst_maps tool.drcacheoff.burst_maps "" "" "")
      endif ()

      set(tool.drcacheoff.burst_traceopts_nodr ON)
      set(tool.drcacheoff.burst_traceopts_nopost ON)
      torunonly_drcacheoff(burst_traceopts tool.drcacheoff.burst_traceopts "" "" "")

      if (LINUX) # Futex is Linux-only.
        set(tool.drcacheoff.burst_futex_nodr ON)
        set(tool.drcacheoff.burst_futex_nopost ON)
        torunonly_drcacheoff(burst_futex tool.drcacheoff.burst_futex "" "" "")
      endif ()

      if (UNIX)
        # FIXME i#2040: this hits static client issues on Windows
        set(tool.drcacheoff.burst_threads_nodr ON)
        torunonly_drcacheoff(burst_threads tool.drcacheoff.burst_threads
          # We test cpu_scheduling for offline traces here.
          "" "@-cpu_scheduling" "")

        set(tool.drcacheoff.burst_threads_counts_nodr ON)
        torunonly_drcacheoff(burst_threads_counts tool.drcacheoff.burst_threads
          "" "@-simulator_type@basic_counts"
          # This test uses the same app as the one above, so set a new dir name.
          "-subdir_prefix drmemtrace.tool.drcacheoff.burst_threads_counts")

        set(tool.drcacheoff.burst_malloc_nodr ON)
        torunonly_drcacheoff(burst_malloc tool.drcacheoff.burst_malloc
          "" "@-simulator_type@basic_counts" "")

        set(tool.drcacheoff.burst_reattach_nodr ON)
        torunonly_drcacheoff(burst_reattach tool.drcacheoff.burst_reattach
          "" "@-simulator_type@basic_counts" "")

        set(tool.drcacheoff.burst_threadfilter_nodr ON)
        torunonly_drcacheoff(burst_threadfilter tool.drcacheoff.burst_threadfilter
          "" "@-simulator_type@basic_counts" "")
        # This test uses the same app as the one above, so we do not set _nodr
        # so we'll get a custom -subdir_prefix.  We have to pass that in as an
        # app arg here.
        torunonly_drcacheoff(burst_threadL0filter tool.drcacheoff.burst_threadfilter
          "" "@-simulator_type@basic_counts"
          "-L0_filter -L0I_size 0 -subdir_prefix tool.drcacheoff.burst_threadL0filter")
        set(tool.drcacheoff.burst_threadL0filter_nodr ON)

        if (X86 AND NOT APPPLE AND NOT RISCV64) # This test is x86-specific.
          # Test that raw2trace doesn't do more IO than it should.
          get_target_path_for_execution(raw2trace_io_path tool.drcacheoff.raw2trace_io
            "${location_suffix}")
          prefix_cmd_if_necessary(raw2trace_io_path ON ${raw2trace_io_path})
          macro (torunonly_raw2trace testname exetgt extra_ops app_args)
            set(testname_full "tool.raw2trace.${testname}")
            torunonly_ci(tool.raw2trace.${testname} ${exetgt} drcachesim
              "raw2trace-${testname}.c" # for templatex basename
              # Disable compression, since we need to seek and our compression
              # readers do not support seeking.
              "-offline -raw_compress none -subdir_prefix ${testname_full} ${extra_ops}"
              "" "${app_args}")
            set(${testname_full}_toolname "drcachesim")
            set(${testname_full}_basedir
              "${PROJECT_SOURCE_DIR}/clients/drcachesim/tests")
            set(${testname_full}_rawtemp ON) # no preprocessor
            set(${testname_full}_runcmp "${CMAKE_CURRENT_SOURCE_DIR}/runmulti.cmake")
            set(${testname_full}_precmd
              "foreach@${CMAKE_COMMAND}@-E@remove_directory@${testname_full}.*.dir")
            set(${testname_full}_postcmd
              "${raw2trace_io_path}@-indir@${testname_full}.*.dir")
          endmacro()
          # actual trace processing not important -- set a very small limit for speed
          torunonly_raw2trace(simple ${ci_shared_app} "-max_trace_size 8K" "")
        endif()

        # FIXME i#2099: the weak symbol is not supported not work on Windows
        set(tool.drcacheoff.burst_client_nodr ON)
        torunonly_drcacheoff(burst_client tool.drcacheoff.burst_client "" "" "")
      endif ()
    endif ()

    if (UNIX AND X86 AND X64)
      if (proc_supports_avx)
        torunonly_drcacheoff(allasm-scattergather-basic-counts allasm_scattergather
          "" "@-simulator_type@basic_counts" "")
        unset(tool.drcacheoff.allasm-scattergather-basic-counts_rawtemp) # use preprocessor
        if (proc_supports_avx512)
          set(tool.drcacheoff.allasm-scattergather-basic-counts_runavx512 1)
        endif (proc_supports_avx512)
        torunonly_drcachesim(allasm-scattergather-basic-counts allasm_scattergather
          "-simulator_type basic_counts" "")
        unset(tool.drcachesim.allasm-scattergather-basic-counts_rawtemp) # use preprocessor
        if (proc_supports_avx512)
          set(tool.drcachesim.allasm-scattergather-basic-counts_runavx512 1)
        endif (proc_supports_avx512)
      endif (proc_supports_avx)

      torunonly_drcacheoff(allasm-repstr-basic-counts allasm_repstr
        "" "@-simulator_type@basic_counts" "")
      unset(tool.drcacheoff.allasm-repstr-basic-counts_rawtemp) # use preprocessor
      torunonly_drcachesim(allasm-repstr-basic-counts allasm_repstr
        # We test counting encodings for online.
        "-instr_encodings -simulator_type basic_counts" "")
      unset(tool.drcachesim.allasm-repstr-basic-counts_rawtemp) # use preprocessor
    endif (UNIX AND X86 AND X64)

    torunonly_drcacheoff(invariant_checker ${ci_shared_app}
      # We pass a small instr count to test multiple chunks in a zipfile.
      "" "@-chunk_instr_count@1000@-simulator_type@invariant_checker" "")

    # Test the standalone histogram tool.
    # ${ci_shared_app} is already used for an offline test, so we run other apps
    # for variety and to include threads and signals for invariant_checker.
    set(histo_app ${kernel_xfer_app})
    if (WIN32)
      # winxfer produces a 500M+ trace and test taking >3mins so we narrow
      # the window.  2M makes a 50M trace and still hits 4 or 5 of the
      # 13 markers on my machine but is still a little slow so we cut in half
      # for 2+ markers.
      # FIXME i#2752: somehow this is hanging on Appveyor so for now we shrink it
      # from 1M to 50K to make progress.  Once we figure out the hang we should
      # put it back to 1M.
      set(histo_ops "-trace_after_instrs 5K -exit_after_tracing 50K")
      set(tool.histogram.offline_timeout 180)
    else ()
      set(histo_ops "")
    endif ()
    set(testname_full "tool.histogram.offline")
    torunonly_ci(${testname_full} ${histo_app} drcachesim
      "histogram-offline.c"
      "-offline -subdir_prefix ${testname_full} ${histo_ops}" "" "")
    set(${testname_full}_toolname "drcachesim")
    set(${testname_full}_basedir
      "${PROJECT_SOURCE_DIR}/clients/drcachesim/tests")
    set(${testname_full}_rawtemp ON) # no preprocessor
    get_target_path_for_execution(histo_path histogram_launcher "${location_suffix}")
    prefix_cmd_if_necessary(histo_path ON ${histo_path})
    set(${testname_full}_runcmp
      "${CMAKE_CURRENT_SOURCE_DIR}/runmulti.cmake")
    set(${testname_full}_precmd
      "foreach@${CMAKE_COMMAND}@-E@remove_directory@${testname_full}.*.dir")
    set(${testname_full}_postcmd
      "${drcachesim_path}@-indir@${testname_full}.*.dir")
    set(${testname_full}_postcmd2
      "${histo_path}@-test_mode@-test_mode_name@kernel_xfer_app@-trace_dir@${testname_full}.*.dir/trace")

    if (AARCH64)
      set(testname_full "tool.drcacheoff.allasm-aarch64-prefetch-counts")
      torunonly_ci(${testname_full} allasm_aarch64_prefetch drcachesim
        "offline-allasm-aarch64-prefetch-counts"
        "-offline -subdir_prefix ${testname_full}" "" "")
      set(${testname_full}_toolname "drcachesim")
      set(${testname_full}_basedir
        "${PROJECT_SOURCE_DIR}/clients/drcachesim/tests")
      set(${testname_full}_rawtemp ON) # no preprocessor
      get_target_path_for_execution(prefetch_analyzer_path prefetch_analyzer_launcher "${location_suffix}")
      prefix_cmd_if_necessary(prefetch_analyzer_path ON ${prefetch_analyzer_path})
      set(${testname_full}_runcmp
        "${CMAKE_CURRENT_SOURCE_DIR}/runmulti.cmake")
      set(${testname_full}_precmd
        "foreach@${CMAKE_COMMAND}@-E@remove_directory@${testname_full}.*.dir")
      set(${testname_full}_postcmd
        "${drcachesim_path}@-indir@${testname_full}.*.dir@-simulator_type@basic_counts")
      set(${testname_full}_postcmd2
        "${prefetch_analyzer_path}@-trace_dir@${testname_full}.*.dir/trace")
    endif ()

    # Stress-test drcachesim with drstatecmp checks.
    if (AARCH64)
      torunonly_drcachesim(drstatecmp-fuzz drstatecmp-fuzz-app
        "-trace_after_instrs 10M -enable_drstatecmp" "" "")
      set(tool.drcachesim.drstatecmp-fuzz_timeout 180)
    endif ()
    if (NOT WIN32)
      torunonly_drcachesim(drstatecmp-delay-simple ${ci_shared_app}
        "-trace_after_instrs 20000 -enable_drstatecmp" "")
      torunonly_drcacheoff(drstatecmp-delay-simple ${ci_shared_app}
        "-trace_after_instrs 20000 -enable_drstatecmp"
        "@-simulator_type@basic_counts" "")
    endif (NOT WIN32)

    # Test syscall_mix.
    torunonly_drcachesim(syscall-mix ${ci_shared_app} "-simulator_type syscall_mix" "")
    torunonly_drcacheoff(syscall-mix ${ci_shared_app} "" "@-simulator_type@syscall_mix" "")

  endif (NOT ANDROID)

  # Test using our exported drmemtrace analysis framework (i#2006).
  get_property(drmemtrace_src_dir GLOBAL PROPERTY DynamoRIO_drmemtrace_src_dir)
  get_property(drmemtrace_build_dir GLOBAL PROPERTY DynamoRIO_drmemtrace_build_dir)
  if (DEBUG)
    set(debug_cfg -DCMAKE_BUILD_TYPE=Debug -DDEBUG=ON)
  else ()
    set(debug_cfg -DCMAKE_BUILD_TYPE=RelWithDebInfo -DDEBUG=OFF)
  endif ()
  add_test(drmemtrace_proj ${CMAKE_CTEST_COMMAND}
    --build-and-test "${drmemtrace_src_dir}" "${drmemtrace_build_dir}/build_and_test"
    --build-generator ${CMAKE_GENERATOR}
    --build-project DynamoRIO_drmemtrace # needed for VS generators
    --build-makeprogram ${CMAKE_MAKE_PROGRAM}
    --build-options ${debug_cfg} -DDynamoRIO_DIR:PATH=${public_config_dir})
  if (UNIX AND X86 AND NOT X64)
    set_tests_properties(drmemtrace_proj PROPERTIES ENVIRONMENT
      "CFLAGS=-m32;CXXFLAGS=-m32")
  endif ()

  # Test postprocessing and analysis with an alternate binary directory.
  # Limited to UNIX because the checked-in modules.log is in UNIX format
  # (Windows needs extra fields per library).
  # Limited to ZLIB_FOUND because we test gzipped .raw files here as well.
  if (UNIX AND ZLIB_FOUND)
    # We make a writable copy so we can both write the final trace there and run our
    # analyzer in a single command line.
    set(srcdir
      ${PROJECT_SOURCE_DIR}/clients/drcachesim/tests/drmemtrace.threadsig.aarch64.raw)
    set(locdir ${PROJECT_BINARY_DIR}/drmemtrace.threadsig.aarch64)
    file(REMOVE_RECURSE ${locdir})
    file(MAKE_DIRECTORY ${locdir})
    file(COPY ${srcdir}/raw DESTINATION ${locdir}/)
    torunonly_api(tool.drcacheoff.altbindir "${drcachesim_path}"
      "offline-altbindir.c" ""
      "-indir;${locdir};-simulator_type;opcode_mix;-alt_module_dir;${srcdir};-module_file;${locdir}/raw/modules.log"
      OFF OFF)
    set(tool.drcacheoff.altbindir_basedir
      "${PROJECT_SOURCE_DIR}/clients/drcachesim/tests")

    # Test the legacy trace versions OFFLINE_FILE_VERSION_KERNEL_INT_PC-1 and
    # TRACE_ENTRY_VERSION_NO_KERNEL_PC.  This requires a checked-in trace and thus
    # -alt_module_dir; it has gzipped .raw files; hence here next to the altbindir test.
    set(srcdir
      ${PROJECT_SOURCE_DIR}/clients/drcachesim/tests/drmemtrace.legacy-int-offs.raw)
    set(locdir ${PROJECT_BINARY_DIR}/drmemtrace.legacy-int-offs)
    file(REMOVE_RECURSE ${locdir})
    file(MAKE_DIRECTORY ${locdir})
    file(COPY ${srcdir}/raw DESTINATION ${locdir}/)
    torunonly_api(tool.drcacheoff.legacy-int-offs "${drcachesim_path}"
      "offline-legacy-int-offs.c" ""
      "-indir;${locdir};-simulator_type;basic_counts;-alt_module_dir;${srcdir};-module_file;${locdir}/raw/modules.log"
      OFF OFF)
    set(tool.drcacheoff.legacy-int-offs_basedir
      "${PROJECT_SOURCE_DIR}/clients/drcachesim/tests")
  endif ()

  if (proc_supports_pt)
    if (BUILD_PT_TRACER AND BUILD_PT_POST_PROCESSOR)
      get_target_path_for_execution(drpt2trace_path drpt2trace "${location_suffix}")
      macro (torunonly_drcacheoff_kernel testname exetgt extra_ops app_args)
        set(testname_full "tool.drcacheoff.kernel.${testname}_SUDO")
        torunonly_ci(${testname_full} ${exetgt} drcachesim
          "offline-kernel-${testname}.c" # for templatex basename
          "-offline -enable_kernel_tracing -subdir_prefix ${testname_full} ${extra_ops}"
          "" "${app_args}")
        set(${testname_full}_sudo ON)
        set(${testname_full}_self_serial ON)
        set(${testname_full}_toolname "drcachesim")
        set(${testname_full}_basedir "${PROJECT_SOURCE_DIR}/clients/drcachesim/tests")
        set(${testname_full}_rawtemp ON) # no preprocessor
        set(${testname_full}_runcmp "${CMAKE_CURRENT_SOURCE_DIR}/runmulti.cmake")
        # Each command is assumed to need sudo.
        set(cmd_pfx "sudo@")
        set(${testname_full}_precmd
          "foreach@${cmd_pfx}${CMAKE_COMMAND}@-E@remove_directory@${testname_full}.*.dir")
        set(${testname_full}_postcmd
          "firstglob@${cmd_pfx}${drcachesim_path}@-simulator_type@basic_counts@-indir@${testname_full}.*.dir${sim_atops}")
      endmacro ()
      torunonly_drcacheoff_kernel(simple ${ci_shared_app} "" "" "")
    endif (BUILD_PT_TRACER AND BUILD_PT_POST_PROCESSOR)
  endif (proc_supports_pt)

  ###########################################################################
  # drcpusim tests

  if (X86) # i#1732: no ARM/AArch64 support for drcpusim yet
    torunonly_ci(tool.drcpusim.simple ${ci_shared_app} drcpusim
      client-interface/${ci_shared_app} # for .expect
      "" "" "")
    set(tool.drcpusim.simple_toolname "drcpusim")

    if (UNIX AND NOT APPLE)
      # For more targeted tests, we need whole-asm, which is much easier
      # on Linux where we can make a static app that executes just a few insrs.
      macro (add_cpusim_asm_test name model)
        add_exe(tool.${name} ${PROJECT_SOURCE_DIR}/clients/drcpusim/tests/${name}.asm)
        set_target_properties(tool.${name} PROPERTIES LINKER_LANGUAGE C)
        append_link_flags(tool.${name} "-nostartfiles -nodefaultlibs -static")
        torunonly_ci(tool.drcpusim.${name} tool.${name} drcpusim
          ${PROJECT_SOURCE_DIR}/clients/drcpusim/tests/${name}.asm
          "-cpu ${model}" "" "")
        set(tool.drcpusim.${name}_toolname "drcpusim")
        set(tool.drcpusim.${name}_basedir
          "${PROJECT_SOURCE_DIR}/clients/drcpusim/tests")
      endmacro ()

      if (NOT X64)
        add_cpusim_asm_test(mmx Pentium)
        add_cpusim_asm_test(sse PentiumMMX)
        add_cpusim_asm_test(sse2 Pentium3)
        add_cpusim_asm_test(sse3 Banias)
      endif ()
      add_cpusim_asm_test(ssse3 Prescott)
      add_cpusim_asm_test(sse41 Merom)
      add_cpusim_asm_test(sse42 Penryn)

      # Test -blocklist.
      torunonly_ci(tool.drcpusim.block tool.ssse3 drcpusim
        "ignore" "-cpu Prescott -blocklist tool.ssse3" "" "")
      set(tool.drcpusim.block_toolname "drcpusim")
      set(tool.drcpusim.block_expectbase "block")
      set(tool.drcpusim.block_basedir "${PROJECT_SOURCE_DIR}/clients/drcpusim/tests")
      # Legacy option name.
      torunonly_ci(tool.drcpusim.legacy tool.ssse3 drcpusim
        "ignore" "-cpu Prescott -blacklist tool.ssse3" "" "")
      set(tool.drcpusim.legacy_toolname "drcpusim")
      set(tool.drcpusim.legacy_expectbase "block")
      set(tool.drcpusim.legacy_basedir "${PROJECT_SOURCE_DIR}/clients/drcpusim/tests")
    endif ()

    add_exe(tool.cpuid ${PROJECT_SOURCE_DIR}/clients/drcpusim/tests/cpuid.c)
    if (WIN32)
      append_property_string(TARGET tool.cpuid COMPILE_FLAGS "/arch:IA32")
      # Debug libc seems to run newer instructions, messing up the test.
      # Plus on VS2017 *any* libc does that even with /arch:IA32 so we
      # separate libc out.
      use_MD_not_MTd(${PROJECT_SOURCE_DIR}/clients/drcpusim/tests/cpuid.c)
    endif ()
    macro (add_cpusim_cpuid_test model)
      torunonly_ci(tool.drcpusim.cpuid-${model} tool.cpuid drcpusim
        ${PROJECT_SOURCE_DIR}/clients/drcpusim/tests/cpuid-${model}.c
        # Ignore libs to reduce risk of violations (e.g., i#2720)
        "-cpu ${model} -ignore_all_libs" "" "")
      set(tool.drcpusim.cpuid-${model}_toolname "drcpusim")
      set(tool.drcpusim.cpuid-${model}_basedir
        "${PROJECT_SOURCE_DIR}/clients/drcpusim/tests")
    endmacro ()
    # Not running earlier than Klamath b/c the app likely will have bad opcodes.
    if (NOT X64)
      # i#5109: We do not run Klamath or Deschutes since gcc on Ubuntu20 includes
      # CET instructions which cannot be avoided.  DR currently thinks they're
      # SSE-added multi-byte nops; we'll have more trouble when i#4040 is implemented.
      add_cpusim_cpuid_test(Pentium3)
      add_cpusim_cpuid_test(Banias)
    endif ()
    add_cpusim_cpuid_test(Prescott)
    add_cpusim_cpuid_test(Presler)
    add_cpusim_cpuid_test(Merom)
    add_cpusim_cpuid_test(Penryn)
    add_cpusim_cpuid_test(Westmere)
    add_cpusim_cpuid_test(Nehalem)
    # XXX i#1761: we should selectively enable these.  Disabling for now.
    if (OFF)
      add_cpusim_cpuid_test(Sandybridge)
      add_cpusim_cpuid_test(Ivybridge)
    endif ()
    # XXX i#1732: add more tests.  However, we don't want the suite to fail
    # b/c our tests have too-new instrs for older machines.
  endif (X86)

endif (BUILD_CLIENTS)

# ##########################################################################
# drpt2trace tests
# We use the libipt/script/perf-get-opts.bash script to get the options for drpt2trace's tests.
if (BUILD_CLIENTS)
  if (BUILD_PT_TRACER AND BUILD_PT_POST_PROCESSOR)
    set(drpt2trace_commong "-print_instrs"
      "-raw_pt"
      "${PROJECT_SOURCE_DIR}/clients/drcachesim/drpt2trace/test_simple.raw/pt.bin"
      "-pt_cpu_family" "6"
      "-pt_cpu_model" "85"
      "-pt_cpu_stepping" "4"
      "-pt_mtc_freq" "3" "-pt_nom_freq" "37"
      "-pt_cpuid_0x15_eax" "2" "-pt_cpuid_0x15_ebx" "308")
    set(drpt2trace_sideband_args ${drpt2trace_commong}
      "-mode" "SIDEBAND"
      "-raw_pt_format" "PERF"
      "-primary_sb"
      "${PROJECT_SOURCE_DIR}/clients/drcachesim/drpt2trace/test_simple.raw/primary.sideband.pevent"
      "-secondary_sb"
      "${PROJECT_SOURCE_DIR}/clients/drcachesim/drpt2trace/test_simple.raw/secondary.sideband.pevent"
      "-sb_sample_type" "0x10086"
      "-sb_time_shift" "31"
      "-sb_time_mult" "581029125"
      "-sb_time_zero" "18444502336406842448"
      "-sb_sysroot"
      "${PROJECT_SOURCE_DIR}/clients/drcachesim/drpt2trace/test_simple.raw")
    torunonly_api(tool.drpt2trace.sideband drpt2trace
      "../../clients/drcachesim/drpt2trace/test_simple.expect"
      "" "${drpt2trace_sideband_args}" ON OFF)
    set(drpt2trace_elf_args ${drpt2trace_commong}
      "-mode" "ELF"
      "-raw_pt_format" "PERF"
      "-elf"
      "${PROJECT_SOURCE_DIR}/clients/drcachesim/drpt2trace/test_simple.raw/hello")
    torunonly_api(tool.drpt2trace.elf drpt2trace
      "../../clients/drcachesim/drpt2trace/test_simple.expect"
      "" "${drpt2trace_elf_args}" ON OFF)
  endif (BUILD_PT_TRACER AND BUILD_PT_POST_PROCESSOR)
endif (BUILD_CLIENTS)

if (UNIX)
  macro(make_symlink src dst)
    execute_process(COMMAND ${CMAKE_COMMAND} -E create_symlink ${src} ${dst}
      RESULT_VARIABLE ln_result ERROR_VARIABLE ln_err)
    if (ln_result OR ln_err)
      message(FATAL_ERROR "*** symlink failed: ***\n${ln_err}")
    endif ()
  endmacro()

  if (NOT APPLE AND NOT RISCV64) # TODO i#3544: Port tests to RISC-V 64
    tobuild(linux.clone linux/clone.c)

    # A PIE build tests MSR-based TLS instead of GDT-based (i#2089).
    tobuild(linux.clone-pie linux/clone.c)
    append_property_string(TARGET linux.clone-pie COMPILE_FLAGS "-fPIE")
    append_property_string(TARGET linux.clone-pie LINK_FLAGS "-pie")
  endif ()

  # Cross-arch execve test: can only be done via a suite of tests that
  # build both 32-bit and 64-bit.  We have 32-bit just build, and
  # 64-bit then builds and runs both directions.
  #
  # i#2971: We test -c32/-c64 here since modern DR focuses on clients.
  if (X86)
    if (X64)
      add_exe(linux.execve-sub64 linux/execve-sub.c)
      if (TEST_SUITE AND TEST_32BIT_PATH)
        # XXX: Should we remove TEST_32BIT_PATH since we're not using it with
        # our separate 32- and 64-bit jobs (replaced by our test-and-build)?
        tobuild_ops(linux.execve64 linux/execve.c ""
          "${TEST_32BIT_PATH}/linux.execve-sub32")
        torunonly(linux.execve32 ${TEST_32BIT_PATH}/linux.execve32 linux/execve.c ""
          "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/linux.execve-sub64")
      else ()
        # If we didn't just build the entire 32-bit and so don't have an
        # install dir handy, we still perform a cross-arch test by building
        # just the few binaries we need and both faking a multi-arch install
        # via symlinks and using -use_alt_dll to point to the other bitwidth.
        set(32dir ${CMAKE_CURRENT_BINARY_DIR}/32bit)
        set(test_bindir suite/tests/bin)
        file(MAKE_DIRECTORY ${32dir})
        # We use a client that takes some options and prints something at
        # init and at exit to test cross-arch injection with clients.
        get_target_path_for_execution(client64_path client.large_options.dll
          "${location_suffix}")
        set(client32_path ${32dir}/${test_bindir}/libclient.large_options.dll.so)
        # Hardcoding this path as the simplest solution:
        if (DEBUG)
          set(libdr_32bit_path "${32dir}/${INSTALL_LIB_X86}/debug/libdynamorio.so")
        else ()
          set(libdr_32bit_path "${32dir}/${INSTALL_LIB_X86}/release/libdynamorio.so")
        endif ()

        add_exe(linux.execve64 linux/execve.c)
        # Use a relative path for the child's execve to stress i#1660, i#4892.
        file(RELATIVE_PATH relative_sub32 "${CMAKE_CURRENT_BINARY_DIR}"
          "${32dir}/${test_bindir}/linux.execve-sub32")
        get_target_path_for_execution(drrun_path drrun "${location_suffix}")
        get_target_path_for_execution(drlib_path dynamorio "${location_suffix}")

        # We have 2 tests (one each 32<->64 direction) for each of two configs:
        # locating the other-bitwidth path via a config entry, or locating it
        # via assumptions on side-by-side installation.
        # To do both configs with just one build we run buth from a script which
        # we generate here:
        set(xarch_script "${CMAKE_CURRENT_BINARY_DIR}/linux.xarch.cmake")
        file(WRITE "${xarch_script}"
          "macro(make_symlink src dst)\n"
          "  execute_process(COMMAND ${CMAKE_COMMAND} -E create_symlink \${src} \${dst}\n"
          "    RESULT_VARIABLE ln_result ERROR_VARIABLE ln_err)\n"
          "  if (ln_result OR ln_err)\n"
          "    message(FATAL_ERROR \"*** create symlink failed: \${ln_err} ***\")\n"
          "  endif ()\n"
          "endmacro()\n"
          "macro(remove_symlink link)\n"
          "  execute_process(COMMAND ${CMAKE_COMMAND} -E remove \${link}\n"
          "    RESULT_VARIABLE rm_result ERROR_VARIABLE rm_err)\n"
          "  if (rm_result OR rm_err)\n"
          "    message(FATAL_ERROR \"*** remove symlink failed: \${rm_err} ***\")\n"
          "  endif ()\n"
          "endmacro()\n"
          "# 32 to 64, assumed layout:\n"
          "make_symlink(${PROJECT_BINARY_DIR}/${INSTALL_LIB_BASE}\n"
          "  ${32dir}/${INSTALL_LIB_BASE})\n"
          "make_symlink(${32dir}/${INSTALL_LIB_X86}\n"
          "  ${PROJECT_BINARY_DIR}/${INSTALL_LIB_X86})\n"
          "execute_process(WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}\n"
          "  COMMAND ${drrun_path} -32\n"
          "  -dr_home ${32dir} ${dr_test_ops}\n"
          "  # We test the \"-c32 -c64\" syntax here.\n"
          "  -c32 ${client32_path} -paramA foo -paramB bar --\n"
          "  -c64 ${client64_path} -paramA foo -paramB bar --\n"
          "  ${32dir}/${test_bindir}/linux.execve32\n"
          "  \"${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/linux.execve-sub64\")\n"
          "# 64 to 32, assumed layout:\n"
          "execute_process(WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}\n"
          "  COMMAND ${drrun_path} ${dr_test_ops}\n"
          "  -c32 ${client32_path} -paramA foo -paramB bar --\n"
          "  -c64 ${client64_path} -paramA foo -paramB bar --\n"
          "  ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/linux.execve64\n"
          "  \"${relative_sub32}\")\n"
          "remove_symlink(${32dir}/${INSTALL_LIB_BASE})\n"
          "remove_symlink(${PROJECT_BINARY_DIR}/${INSTALL_LIB_X86})\n"
          "# 32 to 64, config entry (symlinks are gone so assumptions fail):\n"
          "execute_process(WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}\n"
          "  COMMAND ${drrun_path} -32\n"
          "  -use_alt_dll ${drlib_path} \n"
          "  -dr_home ${32dir} ${dr_test_ops}\n"
          "  -c32 ${client32_path} -paramA foo -paramB bar --\n"
          "  -c64 ${client64_path} -paramA foo -paramB bar --\n"
          "  ${32dir}/${test_bindir}/linux.execve32\n"
          "  \"${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/linux.execve-sub64\")\n"
          "# 64 to 32, config entry (symlinks are gone so assumptions fail):\n"
          "execute_process(WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}\n"
          "  COMMAND ${drrun_path}\n"
          # Hardcoding this path as the simplest solution:
          "  -use_alt_dll ${libdr_32bit_path}\n"
          "  ${dr_test_ops}\n"
          "  -c32 ${client32_path} -paramA foo -paramB bar --\n"
          "  -c64 ${client64_path} -paramA foo -paramB bar --\n"
          "  ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/linux.execve64\n"
          "  \"${relative_sub32}\")\n"
          )
        if (DEBUG)
          set(config_type -DDEBUG=ON)
        else ()
          set(config_type "")
        endif ()
        add_test(linux.xarch ${CMAKE_CTEST_COMMAND} -V
          --build-and-test ${PROJECT_SOURCE_DIR} ${32dir}
          --build-generator ${CMAKE_GENERATOR}
          --build-target dynamorio
          --build-target linux.execve32
          --build-target linux.execve-sub32
          --build-target client.large_options.dll
          --build-noclean # else it tries to clean between each target!
          --build-makeprogram ${CMAKE_MAKE_PROGRAM}
          --build-options ${config_type} -DBUILD_TESTS=ON -DBUILD_DOCS=OFF
          -DBUILD_SAMPLES=OFF -DBUILD_EXT=OFF -DBUILD_CLIENTS=OFF
          --test-command ${CMAKE_COMMAND} -P ${xarch_script})
        file(READ linux/execve-client.expect expect)
        set_tests_properties(linux.xarch PROPERTIES ENVIRONMENT
          "CFLAGS=-m32;CXXFLAGS=-m32"
          # We do not try to indirect this so we have to read .expect manually.
          # We simplify and assume no regex chars.
          # We start with .* for all the config and build stuff and then we
          # expect 4 straight identical test outputs.
          PASS_REGULAR_EXPRESSION
          ".*${expect}[ \n]*${expect}[ \n]*${expect}[ \n]*${expect}[ \n]*$")
      endif ()
    else (X64)
      add_exe(linux.execve-sub32 linux/execve-sub.c)
      add_exe(linux.execve32 linux/execve.c)
    endif (X64)
  endif ()

  # helper app for execve-null
  add_exe(linux.execve-sub linux/execve-sub.c)
  get_target_path_for_execution(execve-sub-path linux.execve-sub "${location_suffix}")
  tobuild_ops(linux.execve-null linux/execve-null.c "" "${execve-sub-path}")

  if (NOT ANDROID) # XXX i#1874: get working on Android
    # Test blocklist and allowlist child config vs follow-children options (i#1667).
    # Use a different name to avoid parallel test conflicts when we manipulate
    # config files.
    add_exe(linux.execve-config linux/execve-sub.c)
    # We want the environment to keep HOME for config dirs (xref i#1679).
    add_exe(linux.execv linux/execv.c)
    set(linux.child-blocklist_expectbase "child-blocklist")
    get_target_path_for_execution(execve-config-path linux.execve-config "${location_suffix}")
    torunonly(linux.child-blocklist linux.execv linux/execv.c
      "-follow_children" "${execve-config-path}")
    get_target_path_for_execution(drconfig_path drconfig "${location_suffix}")
    prefix_cmd_if_necessary(drconfig_path ON ${drconfig_path})
    set(linux.child-blocklist_runcmp "${CMAKE_CURRENT_SOURCE_DIR}/runmulti.cmake")
    set(linux.child-blocklist_precmd
      "${drconfig_path}@-quiet@-reg@linux.execve-config@-norun")
    set(linux.child-blocklist_postcmd
      "${drconfig_path}@-quiet@-unreg@linux.execve-config")

    set(linux.child-allowlist_expectbase "child-allowlist")
    torunonly(linux.child-allowlist linux.execv linux/execv.c
      "-no_follow_children" "${execve-config-path}")
    set(linux.child-allowlist_runcmp "${CMAKE_CURRENT_SOURCE_DIR}/runmulti.cmake")
    if (DEBUG)
      set(debug_args "@-debug")
    else (DEBUG)
      set(debug_args "")
    endif (DEBUG)
    set(linux.child-allowlist_precmd
      "${drconfig_path}@-quiet@-reg@linux.execve-config${debug_args}@-ops@-stderr_mask@@0")
    set(linux.child-allowlist_postcmd
      "${drconfig_path}@-quiet@-unreg@linux.execve-config")
    # Can't run in parallel w/ blocklist since manipulating same config file:
    set(linux.child-allowlist_depends linux.child-blocklist)
  endif ()

  tobuild(linux.execve-rec linux/execve-rec.c)
  if (ANDROID)
    # Times out in 120s in suite (i#2106).
    set(linux.execve-rec_timeout 180)
  endif ()
  tobuild(linux.exit linux/exit.c)
  if (NOT ANDROID) # XXX i#1874: get working on Android: just output order?!?
    tobuild(linux.fork linux/fork.c)
  endif ()
  tobuild(linux.fork-sleep linux/fork-sleep.c)
  if (X86)
    # i#1537: test MSR for fs/gs in fork
    torunonly(linux.fork-MSR linux.fork linux/fork.c
      "-vm_base 0x100000000 -no_vm_base_near_app" "")
  endif ()

  tobuild(linux.infinite linux/infinite.c)
  tobuild(linux.longjmp linux/longjmp.c)
  if (NOT APPLE)
    tobuild(linux.prctl linux/prctl.c)
  endif ()
  tobuild(linux.mmap linux/mmap.c)
  tobuild(linux.zero-length-mem-ranges linux/zero-length-mem-ranges.c)
  tobuild(linux.signal0000 linux/signal0000.c)
  tobuild(linux.signal0001 linux/signal0001.c)
  tobuild(linux.signal0010 linux/signal0010.c)
  tobuild(linux.signal0011 linux/signal0011.c)
  tobuild(linux.signal0100 linux/signal0100.c)
  tobuild(linux.signal0101 linux/signal0101.c)
  tobuild(linux.signal0110 linux/signal0110.c)
  tobuild(linux.signal0111 linux/signal0111.c)
  tobuild(linux.signal1000 linux/signal1000.c)
  tobuild(linux.signal1001 linux/signal1001.c)
  tobuild(linux.signal1010 linux/signal1010.c)
  tobuild(linux.signal1011 linux/signal1011.c)
  tobuild(linux.signal1100 linux/signal1100.c)
  tobuild(linux.signal1101 linux/signal1101.c)
  tobuild(linux.signal1110 linux/signal1110.c)
  tobuild(linux.signal1111 linux/signal1111.c)
  tobuild(linux.sigplain000 linux/sigplain000.c)
  tobuild(linux.sigplain001 linux/sigplain001.c)
  tobuild(linux.sigplain010 linux/sigplain010.c)
  tobuild(linux.sigplain011 linux/sigplain011.c)
  tobuild(linux.sigplain100 linux/sigplain100.c)
  tobuild(linux.sigplain101 linux/sigplain101.c)
  tobuild(linux.sigplain110 linux/sigplain110.c)
  tobuild(linux.sigplain111 linux/sigplain111.c)
  tobuild(linux.sigaction   linux/sigaction.c)
  # We want a native run to verify our assumptions of kernel behaviour.
  torunonly_native(code_api|linux.sigaction.native linux.sigaction
    "" linux/sigaction.c "")
  if (LINUX)
    tobuild(linux.syscall_pwait linux/syscall_pwait.cpp)
    link_with_pthread(linux.syscall_pwait)
  endif ()
  if (LINUX AND NOT ANDROID) # Only tests RT sigaction which is not supported on Android.
    tobuild(linux.sigaction_nosignals linux/sigaction_nosignals.c)
  endif ()
  # FIXME i#1551, i#1569: port asm to ARM and AArch64
  # FIXME i#58: port test to MacOS
  # TODO i#3544: Port tests to RISC-V 64
  if (NOT ARM AND NOT AARCH64 AND NOT APPLE AND NOT RISCV64)
    tobuild(linux.sigcontext linux/sigcontext.c)
    set_avx_flags(linux.sigcontext)
    if (proc_supports_avx512)
      if (CLANG)
        append_property_string(TARGET linux.sigcontext COMPILE_FLAGS
          "-mllvm -x86-use-vzeroupper=0")
      else ()
        append_property_string(TARGET linux.sigcontext COMPILE_FLAGS
          "-mno-vzeroupper")
      endif ()
    endif ()
  endif ()
  if (NOT APPLE)
    tobuild(linux.thread linux/thread.c)
    tobuild(linux.threadexit linux/threadexit.c)
    if (NOT ANDROID)
      tobuild(linux.threadexit2 linux/threadexit2.c) # XXX i#1874: hangs on Android
      tobuild(linux.signalfd linux/signalfd.c) # XXX i#1874: fails natively on Android
    endif ()
  endif ()
  # i#784: test app behavior on alarm
  tobuild(linux.alarm linux/alarm.c)
  if (NOT APPLE AND NOT ANDROID AND NOT RISCV64) # Test uses Linux-specific timer code.
    # TODO i#3544: Port tests to RISC-V 64
    if (NOT AARCH64) # TODO i#1569: Enable this for AArch64.
      tobuild(linux.signal_race linux/signal_race.c)
      target_link_libraries(linux.signal_race rt)
    endif ()
    tobuild(linux.signal_racesys linux/signal_racesys.c)
    target_link_libraries(linux.signal_racesys rt)

    tobuild(linux.signal_pre_syscall linux/signal_pre_syscall.c)
    target_link_libraries(linux.signal_pre_syscall rt)
  endif ()

  if (NOT RISCV64) # TODO i#3544: Port tests to RISC-V 64
    tobuild(linux.bad-signal-stack linux/bad-signal-stack.c)

    # i#1145: test re-starting syscalls, both inlined and from dispatch
    tobuild(linux.eintr linux/eintr.c)
    link_with_pthread(linux.eintr)
    torunonly(linux.eintr-noinline linux.eintr linux/eintr.c "-no_ignore_syscalls" "")
  endif (NOT RISCV64)

  if (NOT ANDROID AND NOT RISCV64) # XXX i#1874: get working on Android
    # TODO i#3544: Port tests to RISC-V 64
    tobuild(linux.sigsuspend linux/sigsuspend.c)
    link_with_pthread(linux.sigsuspend)

    tobuild(linux.signest linux/signest.c)
    link_with_pthread(linux.signest)

    tobuild(linux.sigmask linux/sigmask.c)
    link_with_pthread(linux.sigmask)
    # Sanity check that this option works.
    torunonly(linux.sigmask-noalarm linux.sigmask linux/sigmask.c
      "-no_reroute_alarm_signals" "")

    if (UNIX)
      if (X64)
        # XXX i#3307: add Windows test.
        tobuild_ops(linux.mangle_asynch linux/mangle_asynch.c "-vm_base 0x100000000 -no_vm_base_near_app" "")
        target_include_directories(linux.mangle_asynch PRIVATE
          ${CMAKE_CURRENT_SOURCE_DIR}/linux)
        link_with_pthread(linux.mangle_asynch)
      endif ()
    endif ()
  endif ()

  if (NOT RISCV64)
    # We pass -native_exec_list which causes us to call strcasecmp in libc,
    # thereby exercising on our ability to copy libc's TLS data.
    # TODO i#3544: Port tests to RISC-V 64
    tobuild_ops(linux.app_tls linux/app_tls.c "-native_exec_list libfoo.so" "")
  endif (NOT RISCV64)

  if (NOT ANDROID AND NOT RISCV64) # XXX i#1874: get working on Android
    # TODO i#3544: Port tests to RISC-V 64
    tobuild_ops(linux.readlink linux/readlink.c "-early_inject" "exec")
  endif ()

  if (NOT ANDROID AND NOT RISCV64) # FIXME i#1874: failing on Android
    # Test collision with DR's preferred base
    # TODO i#3544: Port tests to RISC-V 64
    tobuild(linux.fib-conflict common/fib.c)
    set_preferred_base_start_and_end(linux.fib-conflict ${preferred_base} OFF)
    # i#1227: test 2-stage loader
    torunonly(linux.fib-conflict-early linux.fib-conflict common/fib.c "-early_inject" "")
  endif ()

  # Test fully static executables, which we only support with early injection
  # or static DR.
  # XXX i#1874: get working on Android
  if (NOT APPLE AND NOT ANDROID AND NOT RISCV64) # XXX i#1285: implement private loader on MacOS
    # TODO i#3544: Port tests to RISC-V 64
    set(fib_static_ops "-early_inject")
    tobuild_ops(linux.fib-static common/fib.c "${fib_static_ops}" "")
    append_property_string(TARGET linux.fib-static LINK_FLAGS
      # The default value of CMAKE_SHARED_LIBRARY_LINK_C_FLAGS has -rdynamic,
      # which doesn't play well with clang and -static.  Adding
      # --no-export-dynamic avoids the problem.
      "-static -Wl,--no-export-dynamic")
  endif ()
  # Test position independent executables.
  # FIXME i#1001: fib-pie fails with -early_inject.
  if (NOT RISCV64) # TODO i#3544: Port tests to RISC-V 64
    tobuild(linux.fib-pie common/fib.c)
    append_property_string(TARGET linux.fib-pie COMPILE_FLAGS "-fPIE")
    append_property_string(TARGET linux.fib-pie LINK_FLAGS "-pie")
  endif (NOT RISCV64)

  # vfork is deprecated on macOS.
  if (NOT APPLE AND NOT RISCV64)
    # FIXME i#125: bugs in these, were disabled in the past
    # TODO i#3544: Port tests to RISC-V 64
    tobuild_ops(linux.vfork linux/vfork.c "" "${execve-sub-path}")
    #tobuild(linux.vfork-fib linux/vfork-fib.c)
  endif ()

  if (NOT RISCV64)
    # runs of other builds with custom DR options
    # TODO i#3544: Port tests to RISC-V 64
    torunonly(linux.thread-reset linux.thread linux/thread.c
      "-enable_reset -reset_at_nth_thread 2" "")
    torunonly(linux.clone-reset linux.clone linux/clone.c
      "-enable_reset -reset_at_nth_thread 2" "")
  endif (NOT RISCV64)
  if (AARCHXX)
    # Test our diagnostic option -steal_reg_at_reset, which is also a stress
    # test of reset and register stealing.
    if (ARM)
      set(new_reg 9)
    else ()
      set(new_reg 29)
    endif ()
    torunonly(linux.steal-switch linux.thread linux/thread.c
      "-enable_reset -reset_at_fragment_count 100 -steal_reg_at_reset ${new_reg}" "")
  endif ()
  tobuild(pthreads.pthreads pthreads/pthreads.c)
  tobuild(pthreads.pthreads_exit pthreads/pthreads_exit.c)
  tobuild(pthreads.ptsig pthreads/ptsig.c)
  if (NOT ANDROID) # FIXME i#1874: failing on Android
    # XXX i#951: pthreads_fork reports leaks on occasion so we mark it FLAKY
    tobuild(pthreads.pthreads_fork_FLAKY pthreads/pthreads_fork.c)
  endif ()

  # Clang will likely never support gcc nested functions:
  # http://llvm.org/PR9206
  if (NOT CMAKE_COMPILER_IS_CLANG)
    if (NOT X64 AND NOT ARM) # FIXME i#1551: port asm to ARM
      # FIXME i#16: these tests need to be fixed to compile for x64
      tobuild(security-linux.stacktest security-linux/stacktest.c)
      tochcon(security-linux.stacktest execmem_exec_t)
      mark_execstack(security-linux.stacktest)
    endif (NOT X64 AND NOT ARM)
    if (NOT ARM) # FIXME i#1551: signal TLS on ARM
      tobuild(security-linux.trampoline security-linux/trampoline.c)
    endif (NOT ARM)
  endif (NOT CMAKE_COMPILER_IS_CLANG)

  # this app is used in nudge tests
  # XXX: should all these infloop tests be made cross-platform?
  # or rely on runall tests that run calc covering this on Windows?
  if (NOT ANDROID) # XXX i#1874: add support for running this on Android
    add_exe(linux.infloop linux/infloop.c)
    torunonly(linux.reset linux/infloop linux/reset.runall "-enable_reset" "-v")
  endif ()
  if (NOT X64 AND NOT ARM) # FIXME i#1551: add coarse-grain ARM support
    # XXX: 64-bit support not quite there yet
    torunonly(linux.freeze_FLAKY linux/infloop linux/freeze.runall
      "-coarse_units -coarse_enable_freeze" "-v")
  endif (NOT X64 AND NOT ARM)

  # persistent cache nudge tests with shared cache
  file(MAKE_DIRECTORY "${PCACHE_DIR}")
  file(MAKE_DIRECTORY "${PCACHE_SHARED_DIR}")
  if (NOT X64 AND NOT ARM) # FIXME i#1551: add coarse-grain ARM support
    # XXX: 64-bit support not quite there yet
    torunonly(linux.persist_FLAKY linux/infloop linux/persist.runall
      "-coarse_units -coarse_split_calls -coarse_enable_freeze -coarse_freeze_min_size 0 -no_persist_per_user -no_validate_owner_dir" "-v")
    torunonly(linux.persist-use_FLAKY linux/infloop linux/persist-use.runall
      "-use_persisted -coarse_units -coarse_split_calls -no_persist_per_user -no_validate_owner_dir" "-v")
  endif (NOT X64 AND NOT ARM)
  # when running tests in parallel: have to generate pcaches first
  set(linux.persist-use_FLAKY_depends linux.persist_FLAKY)

  if (LINUX AND X64 AND HAVE_RSEQ AND NOT RISCV64)
    # The rseq kernel feature is Linux-only.
    # TODO i#2350: Port the assembly in the test to 32-bit x86 and to ARM.
    # TODO i#3544: Port tests to RISC-V 64
    set(linux.rseq_no_reg_compat)
    tobuild(linux.rseq linux/rseq.cpp)
    # Test the other sections.  Unfortunately we need a separate binary for each.
    tobuild(linux.rseq_table linux/rseq.cpp)
    append_property_list(TARGET linux.rseq_table
      COMPILE_DEFINITIONS "RSEQ_TEST_USE_OLD_SECTION_NAME")
    tobuild(linux.rseq_noarray linux/rseq.cpp)
    append_property_list(TARGET linux.rseq_noarray
      COMPILE_DEFINITIONS "RSEQ_TEST_USE_NO_ARRAY")
    # Test attaching, which has a separate lazy rseq check.
    # We build with static DR to also test client interactions.
    tobuild_api(api.rseq linux/rseq.cpp "" "" OFF ON OFF)
    link_with_pthread(api.rseq)
    append_property_list(TARGET api.rseq COMPILE_DEFINITIONS "RSEQ_TEST_ATTACH")
    use_DynamoRIO_static_client(api.rseq drmemtrace_static)
    use_DynamoRIO_drmemtrace_tracer(api.rseq)
    set(api.rseq_expectbase rseq_client)
    # Test non-compliant code with our workaround flag.
    tobuild_ops(linux.rseq_disable linux/rseq_disable.c "-disable_rseq" "")
  endif ()
else (UNIX)
  add_exe(win32.infloop win32/infloop.c)
  add_exe(win32.attachee win32/attachee.c)
  if (PROGRAM_SHEPHERDING)
    # too flaky across platforms so we limit to security mode only: not too useful
    # for other than testing our ASLR anyway
    tobuild(win32.aslr-dll win32/aslr-dll.c)
  endif ()
  tobuild(win32.callback win32/callback.c)
  tobuild(win32.crtprcs win32/crtprcs.c)
  if (X64)
    # regression test for in-progress earliest injection feature
    # XXX i#234: once have wow64 support run on 32-bit
    torunonly(win32.crtprcs-earliest win32.crtprcs win32/crtprcs.c
      "-early_inject_map -early_inject_location 5" "")
  endif (X64)

  tobuild_dll(win32.delaybind win32/delaybind.c "")
  append_link_flags(win32.delaybind "/delayload:win32.delaybind.dll.dll /delay:unload")
  tobind(win32.delaybind)
  tobind(win32.delaybind.dll)

  tobuild_dll(win32.dll win32/dll.c "")
  tobuild(win32.fiber-rac win32/fiber-rac.c)
  tobuild(win32.finally win32/finally.c)

  tobuild_dll(win32.multisec win32/multisec.c "")
  # On Win8+ loading via 8.3 name doesn't fool the loader, so we make a copy.
  DynamoRIO_get_full_path(multisec_path win32.multisec.dll "${location_suffix}")
  add_custom_command(TARGET win32.multisec.dll POST_BUILD
    COMMAND ${CMAKE_COMMAND} ARGS -E copy ${multisec_path}
    ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/win32.multisec2.dll.dll VERBATIM)

  tobuild_dll(win32.nativeterminate win32/nativeterminate.c
    "-native_exec_list nativeterminate.dll.dll")
  # FIXME i#192: oomtest ends up killing parent shell: investigate, fix, re-enable!
  #tobuild(win32.oomtest win32/oomtest.c)
  if (TEST_SUITE) # failing on win7 (i#422) so run only in suite
    # Keep exe name same, is checked in DR.
    add_exe(win32.partial_map win32/partial_map.c)
    torunonly(win32.partial_map_FLAKY win32.partial_map win32/partial_map.c "" "")
  endif (TEST_SUITE)
  tobuild(win32.protect-datasec win32/protect-datasec.c)

  tobuild_dll(win32.rebased win32/rebased.c "")
  # On Win8+ loading via 8.3 name doesn't fool the loader, so we make a copy.
  DynamoRIO_get_full_path(rebased_path win32.rebased.dll "${location_suffix}")
  add_custom_command(TARGET win32.rebased.dll POST_BUILD
    COMMAND ${CMAKE_COMMAND} ARGS -E copy ${rebased_path}
    ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/win32.rebased2.dll.dll VERBATIM)

  # Memory usage does go up a bit with reloading due to traces so we disable traces.
  tobuild_dll(win32.reload win32/reload.c "-disable_traces")

  tobuild_dll(win32.reload-newaddr win32/reload-newaddr.c "")
  # FIXME i#267: fix curiosities and re-enable
  #tobuild_dll(win32.reload-race win32/reload-race.c "")

  tobuild_dll(win32.section-max win32/section-max.c "")

  tobuild(win32.suspend win32/suspend.c)
  tobuild(win32.threadchurn win32/threadchurn.c)
  if (TEST_SUITE) # occasionally fails so run only in suite
    tobuild(win32.threadexit_FLAKY win32/threadexit.c)
  endif (TEST_SUITE)
  # PR 215238 covers re-enabling threadinjection
  #tobuild(win32.threadinjection win32/threadinjection.c)
  # FIXME i#125: debugger test was disabled in past: not sure why
  #tobuild(win32.debugger win32/debugger.c)
  tobuild(win32.virtualfree win32/virtualfree.c)
  tobuild(win32.virtualreserve win32/virtualreserve.c)
  tobuild(win32.winapc win32/winapc.c)
  tobuild(win32.winthread win32/winthread.c)
  tobuild(security-win32.apc-shellcode security-win32/apc-shellcode.c)
  tobuild(security-win32.except-int2d security-win32/except-int2d.c)
  tobuild(security-win32.except-guard security-win32/except-guard.c)
  tobuild(security-win32.thread-setcontext security-win32/thread-setcontext.c)
  tobuild(security-win32.singlestep security-win32/singlestep.c)
  if (NOT X64)
    # 32-bit limitation is test only
    # to get a test on x64, we should find a way to set debug registers
    tobuild(security-win32.except-debugreg security-win32/except-debugreg.c)
  endif (NOT X64)

  tobuild(win32.threadterm win32/threadterm.c)

  tobuild_dll(win32.earlythread win32/earlythread.c "")

  if (DEBUG)
    # We have a special allowance for this test to target our nudge handler in DEBUG.
    tobuild(win32.tls win32/tls.c)
  endif ()

  if (NOT X64)
    # FIXME i#16: these tests need to be fixed to compile for x64

    # fpe compiles, but pops up an unhandled exception box
    tobuild(win32.fpe win32/fpe.c)

    tobuild(win32.except win32/except.c)
    # Debug libc seems to mess up the test.
    use_MT_not_MTd(win32/except.c)
    tobuild(win32.getthreadcontext win32/getthreadcontext.c)
    tobuild_dll(win32.hookerfirst win32/hookerfirst.c
      "-handle_ntdll_modify 1 -native_exec_hook_conflict 1")
    tobuild(win32.hooker-secur32 win32/hooker-secur32.c)
    tobuild(win32.rsbtest win32/rsbtest.c)
    tobuild(win32.setcxtsyscall win32/setcxtsyscall.c)
    tobuild(win32.setthreadcontext win32/setthreadcontext.c)

    tobuild_dll(security-win32.aslr-ind security-win32/aslr-ind.c "")
    if (TEST_SUITE) # fails non-det so run only in suite
      tobuild(security-win32.codemod-threads_FLAKY security-win32/codemod-threads.c)
    endif (TEST_SUITE)

    # we tweak cflags in set_cflags() rather than passing extra param all
    # the way through: adding helper func layers gets awkward w/ PARENT_SCOPE
    tobuild(security-win32.except-execution security-win32/except-execution.c)
    # we match exe addresses in output so no ASLR
    append_link_flags(security-win32.except-execution "/dynamicbase:no")

    tobuild(security-win32.except-thread security-win32/except-thread.c)
    tobuild(security-win32.gbop-test security-win32/gbop-test.c)

    if (TEST_SUITE) # fails non-det so run only in suite
      tobuild(security-win32.hooker_FLAKY security-win32/hooker.c)
    endif (TEST_SUITE)
    tobuild_ops(security-win32.hooker-ntdll security-win32/hooker-ntdll.c
      "-handle_ntdll_modify 1" "")
    tobuild(security-win32.indexisting security-win32/indexisting.c)
    tobuild(security-win32.patterns security-win32/patterns.c)
    tobuild(security-win32.ret-SEH security-win32/ret-SEH.c)
  endif (NOT X64)

  tobuild(security-win32.sd_tester security-win32/sd_tester.c)
  tobuild(security-win32.sec-adata security-win32/sec-adata.c)

  # FIXME: getting warnings from pragmas in src code:
  #   secalign-fixed.dll.c.obj : warning LNK4229: invalid directive '/fixed' encountered; ignored
  #  secalign-fixed.dll.c.obj : warning LNK4229: invalid directive '/driver' encountered; ignored
  #  section-max.dll.c.obj : warning LNK4229: invalid directive '/driver' encountered; ignored
  #  sec-xdata.dll.c.obj : warning LNK4078: multiple '.xdata' sections found with different attributes (C0500040)
  # I checked secalign-fixed.dll.dll and it does have 0x2000 alignment and has
  # no relocations, so not sure what the warnings imply.  Did not check the others.
  tobuild_dll(security-win32.secalign-fixed security-win32/secalign-fixed.c "")
  append_link_flags(security-win32.secalign-fixed.dll "/dynamicbase:no /fixed")

  tobuild_dll(security-win32.sec-fixed security-win32/sec-fixed.c "")
  append_link_flags(security-win32.sec-fixed.dll "/dynamicbase:no /fixed")

  tobuild_dll(security-win32.sec-xdata security-win32/sec-xdata.c "")

  # PR 209235 covers re-enabling selfmod-threads
  #tobuild(security-win32.selfmod-threads security-win32/selfmod-threads.c)
  tobuild(security-win32.TestNTFlush security-win32/TestNTFlush.c)

  # FIXME: enable once have .runall support
  #tobuild_runall(runall.calc-detach runall/calc-detach.runall "")
  #tobuild_runall(runall.calc-freeze runall/calc-freeze.runall
  #  # xref PR 226578 which tracks incompatibility between probe_api & pcaches
  #  "-coarse_enable_freeze -no_probe_api")
  #tobuild_runall(runall.calc-hotp runall/calc-hotp.runall "")
  #tobuild_runall(runall.calc-persist runall/calc-persist.runall
  #  "-coarse_enable_freeze -use_persisted -no_probe_api")
  #tobuild_runall(runall.calc-reset runall/calc-reset.runall "")
  #tobuild_runall(runall.calc runall/calc.runall "")
  #tobuild_runall(runall.detach_test runall/detach_test.runall "")
  ## case 9423 covers re-enabling
  ##tobuild_runall(runall.earlythread runall/earlythread.runall "")
  #tobuild_runall(runall.initapc runall/initapc.runall "")
  #tobuild_runall(runall.notepad runall/notepad.runall "")
  #tobuild_runall(runall.preunload runall/preunload.runall "")
  #tobuild_runall(runall.processchain runall/processchain.runall "")

  tobuild_csharp(win32.dotnet win32/dotnet.cs)

  # Cross-arch injection tests.
  if (X86)
    set(xarch_dir ${CMAKE_CURRENT_BINARY_DIR}/xarch)
    set(test_bindir suite/tests/bin)
    file(MAKE_DIRECTORY ${xarch_dir})
    # Unfortunately, while CMake 3.13+ supports create_symlink on Windows, that
    # only works if the user has the right privileges, so we can't set up
    # lib dir symlinks like we do for unix cross-arch build-and-test.
    # We have to make a copy in order to have parallel dirs with the two bitwidths.
    file(MAKE_DIRECTORY "${xarch_dir}/${INSTALL_LIB}")
    set(xarch_copy_stamp "${CMAKE_CURRENT_BINARY_DIR}/xarch_copy.stamp")
    add_custom_command(OUTPUT "${xarch_copy_stamp}"
      DEPENDS dynamorio
      COMMAND ${CMAKE_COMMAND} ARGS
      -E touch "${xarch_copy_stamp}"
      COMMAND ${CMAKE_COMMAND} ARGS
      -E copy "${MAIN_LIBRARY_OUTPUT_DIRECTORY}/dynamorio.dll"
      "${xarch_dir}/${INSTALL_LIB}/dynamorio.dll"
      VERBATIM)
    add_custom_target(xarch_copy DEPENDS "${xarch_copy_stamp}")
    add_dependencies(drcopy xarch_copy)
    # We use a client that takes some options and prints something at
    # init and at exit to test cross-arch injection with clients.
    get_target_path_for_execution(cur_client_path client.large_options.dll
      "${location_suffix}")
    set(xarch_client_path ${xarch_dir}/${test_bindir}/client.large_options.dll.dll)
    get_target_path_for_execution(drrun_path drrun "${location_suffix}")
    if (X64)
      set(client32_path ${xarch_client_path})
      set(client64_path ${cur_client_path})
      set(other_is64 OFF)
    else ()
      set(client32_path ${cur_client_path})
      set(client64_path ${xarch_client_path})
      set(other_is64 ON)
    endif ()
    if (DEBUG)
      set(config_type -DDEBUG=ON)
    else ()
      set(config_type "")
    endif ()
    # I abandoned --build-generator-platform (3.13+ only; VS gen only) or explicitly
    # setting the full generator (fails w/ Ninja) and went with the env var scheme we
    # use in our test suite.
    _DR_set_VS_bitwidth_env_vars(${other_is64} env_names)
    foreach (env ${env_names})
      # Escape internal ; since this goes into a CMake "var=value;..." list.
      string(REPLACE ";" "\\;" value "${${env}_env_value}")
      list(APPEND test_env_pairs "${env}=${value}")
    endforeach ()
    add_test(win32.xarch ${CMAKE_CTEST_COMMAND} -V
      --build-and-test ${PROJECT_SOURCE_DIR} ${xarch_dir}
      --build-generator ${CMAKE_GENERATOR}
      --build-target dynamorio
      --build-target common.eflags
      --build-target client.large_options.dll
      --build-noclean # Else it tries to clean between each target!
      --build-makeprogram ${CMAKE_MAKE_PROGRAM}
      --build-options ${config_type} -DBUILD_TESTS=ON -DBUILD_DOCS=OFF
      -DBUILD_SAMPLES=OFF -DBUILD_EXT=OFF -DBUILD_CLIENTS=OFF
      --test-command ${drrun_path} -dr_home ${xarch_dir} ${dr_test_ops}
      -c32 ${client32_path} -paramA foo -paramB bar --
      -c64 ${client64_path} -paramA foo -paramB bar --
      ${MAIN_RUNTIME_OUTPUT_DIRECTORY}/create_process.exe
      ${xarch_dir}/${test_bindir}/common.eflags.exe)
    file(READ win32/xarch.templatex expect)
    set_tests_properties(win32.xarch PROPERTIES ENVIRONMENT
      "${test_env_pairs}"
      # We start with .* for all the config and build stuff.
      PASS_REGULAR_EXPRESSION ".*${expect}[ \n]*$")
  endif (X86)

  # Cross-arch mixedmode/x86_to_x64 test: can only be done via a suite of tests that
  # build both 32-bit and 64-bit.  We have 32-bit just build, and
  # 64-bit then builds and runs both directions.
  #
  # XXX: alternative of cmake cross-compilation support really
  # requires a separate config anyway; to build 32-bit in 64-bit build
  # dir would require manual build rules.  Not difficult, just a
  # little messy.
  #
  # We support setting TEST_32BIT_PATH manually in a 64-bit build.
  # We go ahead and build in any 32-bit build dir to make that easier.
  #
  # XXX: currently early injection doesn't work for pre-Vista WOW64, where
  # w/ these options the child will end up native and the test will pass (!).
  # For now we assume injection succeeds on Vista+ and that the test thus
  # actually tests mixed-mode, until we've got early/earliest injection working
  # everywhere (xref i#381, i#234, i#803).
  #
  # XXX i#829: mixed-mode stubs and abs far jmps require -heap_in_lower_4GB
  #
  # TODO i#803: Once cross-arch injection works on Windows, add a test of
  # the "-c32 -c64" syntax like we have for Linux.
  if (TEST_SUITE OR TEST_32BIT_PATH OR NOT X64)
    if (X64)
      # XXX i#1636: the earliest injection tests below seem to have the child
      # native.  Until we figure out what's up I'm at least adding this test
      # which does inject into the child:
      torunonly(win32.mixedmode_late
        ${MAIN_RUNTIME_OUTPUT_DIRECTORY}/create_process.exe
        win32/mixedmode.c
        "-early_inject -heap_in_lower_4GB -reachable_heap"
        "${TEST_32BIT_PATH}/win32.mixedmode.exe;mixedmode")
      torunonly(win32.mixedmode
        ${MAIN_RUNTIME_OUTPUT_DIRECTORY}/create_process.exe
        win32/mixedmode.c
        "-early_inject_map -early_inject_location 5 -heap_in_lower_4GB -reachable_heap"
        "${TEST_32BIT_PATH}/win32.mixedmode.exe;mixedmode")
      # i#1494: Trace creation should be disabled for -x86_to_x64.
      # We did not use -disable_traces here to check if trace creation is
      # disabled automatically.
      torunonly(win32.x86_to_x64
        ${MAIN_RUNTIME_OUTPUT_DIRECTORY}/create_process.exe
        win32/mixedmode.c
        "-early_inject_map -early_inject_location 5 -heap_in_lower_4GB -reachable_heap -x86_to_x64"
        "${TEST_32BIT_PATH}/win32.mixedmode.exe;x86_to_x64")
      torunonly(win32.x86_to_x64_ibl_opt
        ${MAIN_RUNTIME_OUTPUT_DIRECTORY}/create_process.exe
        win32/mixedmode.c
        "-early_inject_map -early_inject_location 5 -heap_in_lower_4GB -reachable_heap -x86_to_x64 -x86_to_x64_ibl_opt"
        "${TEST_32BIT_PATH}/win32.mixedmode.exe;x86_to_x64")
    else (X64)
      add_exe(win32.mixedmode win32/mixedmode.c)
    endif (X64)
  endif (TEST_SUITE OR TEST_32BIT_PATH OR NOT X64)

endif (UNIX)

if (NOT RISCV64) # TODO i#3544: Port tests to RISC-V 64
  tobuild(security-common.codemod security-common/codemod.c)
  tochcon(security-common.codemod execmem_exec_t)
  mark_execstack(security-common.codemod)
endif (NOT RISCV64)
if (X86) # FIXME i#1551, i#1569: port asm to ARM and AArch64
  # FIXME i#1308, i#2216: fails non-deterministically on Travis
  set(decode_bad_stack_name security-common.decode-bad-stack_FLAKY)
  tobuild(${decode_bad_stack_name} security-common/decode-bad-stack.c)
  tochcon(${decode_bad_stack_name} execmem_exec_t)
  mark_execstack(${decode_bad_stack_name})
  tobuild(security-common.retexisting security-common/retexisting.c)
endif (X86)
if (PROGRAM_SHEPHERDING) # relies on being aborted on .B violation
  tobuild(security-common.jmp_from_trace security-common/jmp_from_trace.c)
  tochcon(security-common.jmp_from_trace textrel_shlib_t)
endif ()
if (NOT RISCV64) # TODO i#3544: Port tests to RISC-V 64
  tobuild(security-common.ret_noncall_trace security-common/ret_noncall_trace.c)
endif (NOT RISCV64)
if (NOT ARM) # FIXME i#1551: fix bugs on ARM
  # FIXME i#1423: somehow when built with VS2013 x64 you can't catch a fault on
  # jumping to an invalid address!  For now we disable for VS2013 x64.
  # It also seems to fail on win8 x64 with VS2012.
  if (UNIX OR NOT X64 OR CMAKE_C_COMPILER_VERSION VERSION_LESS 17.0)
    tobuild(security-common.retnonexisting security-common/retnonexisting.c)
  endif ()
endif (NOT ARM)
if (X86) # FIXME i#1551, i#1569: port asm to ARM and AArch64
  tobuild(security-common.selfmod2 security-common/selfmod2.c)
  tochcon(security-common.selfmod2 textrel_shlib_t)
  if (NOT APPLE) # XXX i#58: port test to MacOS
    tobuild(security-common.selfmod-big security-common/selfmod-big.c)
    tochcon(security-common.selfmod-big textrel_shlib_t)
  endif ()
  tobuild(security-common.selfmod security-common/selfmod.c)
  tochcon(security-common.selfmod textrel_shlib_t)
  if (NOT X64 AND NOT APPLE) # XXX i#58: port test to MacOS
    # FIXME i#125
    tobuild(security-common.vbjmp-rac-test security-common/vbjmp-rac-test.c)
  endif ()
endif (X86)
if (X64 AND NOT AARCH64 AND NOT RISCV64) # FIXME i#1569: get working on AArch64
  # Reachability tests

  # Floating DR lib.  We'd have to build a 2nd dynamorio.dll on Windows so
  # we only test on Linux for now.
  # TODO i#3544: Port tests to RISC-V 64
  if (UNIX)
    torunonly(float_DR security-common.selfmod security-common/selfmod.c "" "")
    set(float_DR_env "LD_USE_LOAD_BIAS=0")
    torunonly_ci(client.float_DR security-common.selfmod
      client.dr_options.dll security-common/selfmod.c
      # I picked dr_options b/c it has no output, but it requires all these ops:
      "" "-native_exec_list foo.dll,bar.dll -opt_cleancall 3 -thread_private" "")
    set(client.float_DR_env "LD_USE_LOAD_BIAS=0")
  endif (UNIX)

  # Floating vmbase.  Risky to just pick this hardcoded address: any better ideas?
  torunonly(float_vmbase security-common.selfmod security-common/selfmod.c
    "-vm_base 0x30000000000 -no_vm_base_near_app" "")
  torunonly_ci(client.float_vmbase security-common.selfmod
    client.dr_options.dll security-common/selfmod.c
    # I picked dr_options b/c it has no output, but it requires
    # -thread_private:
    "" "-native_exec_list foo.dll,bar.dll -opt_cleancall 3 -thread_private -vm_base 0x30000000000 -no_vm_base_near_app"
    "")
  if (UNIX)
    # Test client with PIE w/ default options
    torunonly_ci(client.float_vmbase_PIE linux.fib-pie
      client.dr_options.dll common/fib.c
      # I picked dr_options b/c it has no output, but it requires
      # -thread_private:
      "" "-native_exec_list foo.dll,bar.dll -opt_cleancall 3 -thread_private" "")
  endif (UNIX)
  torunonly(low4GB security-common.selfmod security-common/selfmod.c
    "-heap_in_lower_4GB -reachable_heap" "")
endif (X64 AND NOT AARCH64 AND NOT RISCV64)

if (NOT RISCV64) # TODO i#3544: Port tests to RISC-V 64
  tobuild("security-common.TestAllocWE" security-common/TestAllocWE.c)
endif (NOT RISCV64)

# XXX i#1874: get working on Android
# FIXME i#1569: get working on AArch64
if (NOT ANDROID)
  set (TestMemProtChg "security-common.TestMemProtChg")
  if (UNIX)  # i#387: Still has issues with proc maps on some kernels.
    set (TestMemProtChg "${TestMemProtChg}_FLAKY")
  endif (UNIX)
  tobuild("${TestMemProtChg}" security-common/TestMemProtChg.c)
  if (UNIX)
    append_property_string(SOURCE security-common/TestMemProtChg.c
      COMPILE_FLAGS "-Wno-vla")
  endif ()
  tochcon("${TestMemProtChg}" wine_exec_t)
  mark_execstack("${TestMemProtChg}")
endif ()

if (vera++_FOUND)
  file(READ "${PROJECT_SOURCE_DIR}/make/style_checks/style_test.templatex" vera_regex)
  add_test(vera ${VERA++_EXECUTABLE} --root "${PROJECT_SOURCE_DIR}/make/style_checks"
    --error "${PROJECT_SOURCE_DIR}/make/style_checks/style_test.c")
  set_tests_properties(vera PROPERTIES PASS_REGULAR_EXPRESSION "${vera_regex}")
endif ()

###########################################################################

# Remember that we do not reach this point for DR_HOST_NOT_TARGET, so do not
# place anything here that should be shared across all configurations.
set_up_test_lists()

if (APPLE)
  # Enable just the quarter of the tests that pass, to get our test suite started
  # on the bot without perturbing all the code above and without disabling tests
  # from the build (all tests build, they just don't all run successfully).
  # FIXME i#1815: fix the failing tests so we can enable the full suite.
  # XXX: can we get this to automatically happen for a local "ctest" w/o
  # the user having to pass "-L OSX"?
  set_tests_properties(
    samples_proj
    code_api|common.broadfun
    code_api|common.fib
    code_api|common.getretaddr
    code_api|libutil.frontend_test
    code_api|linux.exit
    code_api|linux.infinite
    code_api|linux.signal0000
    code_api|linux.signal0010
    code_api|linux.signal0100
    code_api|linux.signal0110
    code_api|linux.app_tls
    code_api|linux.fib-conflict
    code_api|linux.fib-pie
    code_api|security-common.codemod
    code_api|security-common.ret_noncall_trace
    code_api|client.crashmsg
    code_api|client.exception
    code_api|client.syscall-mod
    code_api|client.truncate
    code_api|client.unregister
    code_api|client.null_instrument
    code_api|client.option_parse
    code_api|client.drcontainers-test
    code_api|client.destructor
    code_api|sample.signal
    code_api|api.ir
    code_api|api.ir-static
    code_api|api.drdecode
    code_api|linux.sigaction
    no_code_api,no_intercept_all_signals|linux.sigaction
    code_api|linux.sigaction.native
    PROPERTIES LABELS OSX)
  if (X86)
    set_tests_properties(
      code_api|common.floatpc
      code_api|common.floatpc_xl8all
      code_api|security-common.retexisting
      code_api|client.count-ctis-noopt
      code_api|client.cbr-retarget
      code_api|client.dr_options
      code_api|tool.drcpusim.cpuid-Prescott
      code_api|tool.drcpusim.cpuid-Presler
      code_api|tool.drcpusim.cpuid-Merom
      code_api|tool.drcpusim.cpuid-Penryn
      code_api|tool.drcpusim.cpuid-Westmere
      code_api|tool.drcpusim.cpuid-Nehalem
      code_api|api.dis
      PROPERTIES LABELS OSX)
  elseif (AARCH64)
    set_tests_properties(
      code_api|api.dis-a64
      PROPERTIES LABELS OSX)
  endif ()
  if (DEBUG AND X86) # FIXME i#1806: fails in release
    set_tests_properties(code_api|client.flush PROPERTIES LABELS OSX)
  endif ()
  if (NOT X64)
    set_tests_properties(
      code_api|client.fcache_shift
    code_api|tool.drcpusim.cpuid-Banias
      PROPERTIES LABELS OSX)
  endif ()
endif ()

# XXX i#4719: Support for running aarchxx Linux tests under QEMU.
# Currently about 1/3 of the tests pass.  The rest either are too slow under QEMU
# and hit the (expanded) timeout, or do not yet have full command line insertion
# support (drcachesim and other tools), or hit bugs in QEMU or in DR.
if (NOT ANDROID AND AARCHXX)
  set_tests_properties(
    code_api|common.broadfun
    code_api|common.getretaddr
    code_api|common.nzcv
    code_api|linux.app_tls
    code_api|linux.execve-null
    code_api|linux.exit
    code_api|linux.fib-conflict
    code_api|linux.fib-conflict-early
    code_api|linux.fib-pie
    code_api|linux.fork
    code_api|linux.infinite
    code_api|linux.longjmp
    code_api|linux.readlink
    code_api|linux.sigaction
    code_api|security-common.codemod
    code_api|security-common.ret_noncall_trace
    code_api|security-common.TestMemProtChg_FLAKY
    code_api|client.app_args
    code_api|client.blackbox
    code_api|client.call-retarget
    code_api|client.crashmsg
    code_api|client.destructor
    code_api|client.drbbdup-test
    code_api|client.drbbdup-no-encode-test
    code_api|client.drbbdup-nonzero-test
    code_api|client.drbbdup-analysis-test
    code_api|client.drcontainers-test
    code_api|client.drmodtrack-test
    code_api|client.drsyms-test
    code_api|client.drwrap-test-callconv
    code_api|client.drwrap-drreg-test
    code_api|client.drxmgr-test
    code_api|client.exception
    code_api|client.int64_overrides
    code_api|client.low_on_memory
    code_api|client.null_instrument
    code_api|client.option_parse
    code_api|client.reg_size_test
    code_api|client.partial_module_map
    code_api|client.stack-overflow
    code_api|client.thread_exit_xl8
    code_api|sample.bbbuf
    code_api|sample.bbcount
    code_api|sample.bbsize
    code_api|sample.div
    code_api|sample.empty
    code_api|sample.inline
    code_api|sample.opcode_count
    code_api|sample.opcodes
    code_api|sample.signal
    code_api|sample.stl_test
    code_api|sample.syscall
    code_api|sample.wrap
    PROPERTIES LABELS RUNS_ON_QEMU)
  if (NOT APPLE)
    set_tests_properties(
      code_api|linux.fib-static
      code_api|linux.signalfd
      code_api|linux.signal_racesys
      code_api|client.modules
      PROPERTIES LABELS RUNS_ON_QEMU)
  endif ()
  if (DEBUG)
    set_tests_properties(
      code_api|common.logstderr
      code_api|common.loglevel
      PROPERTIES LABELS RUNS_ON_QEMU)
  endif ()
  if (HAVE_LIBUNWIND_H)
    set_tests_properties(
      code_api|client.drcallstack-test
      PROPERTIES LABELS RUNS_ON_QEMU)
  endif ()
  if (AARCH64)
    set_tests_properties(
      code_api|common.allasm_aarch64_cache
      code_api|linux.mangle_asynch
      code_api|linux.mmap
      code_api|pthreads.pthreads_exit
      code_api|pthreads.pthreads
      code_api,satisfy_w_xor_x|linux.fork
      code_api|security-common.retnonexisting
      code_api|client.app_inscount
      code_api|client.cleancall-opt-1
      code_api|client.cleancallparams
      code_api|client.drmgr-test
      code_api|client.drreg-cross
      code_api|client.drreg-end-restore
      code_api|client.drreg-flow
      code_api|client.drutil-test
      code_api|client.drx_buf-test
      code_api|client.emulation_api_simple
      code_api|client.execfault
      code_api|client.gonative
      code_api|client.inline
      code_api|client.mangle_suspend
      code_api|client.process-id
      code_api|client.syscall-mod
      code_api|client.tls
      code_api|client.truncate
      code_api|client.unregister
      code_api|sample.inscount
      code_api|sample.inscount.cleancall
      code_api|sample.inscount.prof-pcs.cleancall
      code_api|sample.instrace_simple
      code_api|sample.memtrace_simple
      code_api|sample.memval_simple
      PROPERTIES LABELS RUNS_ON_QEMU)
    if (NOT APPLE) # TODO i#5383: Port to Mac M1.
      set_tests_properties(
        code_api|common.allasm_aarch64_isa
        code_api|client.drreg-test
        PROPERTIES LABELS RUNS_ON_QEMU)
    endif ()
    if (NOT CMAKE_COMPILER_IS_CLANG)
      set_tests_properties(
        code_api|security-linux.trampoline
        PROPERTIES LABELS RUNS_ON_QEMU)
    endif ()
    if (LINUX AND X64 AND HAVE_RSEQ)
      # QEMU just returns ENOSYS, but still this is testing that.
      set_tests_properties(
        code_api|linux.rseq
        code_api|linux.rseq_table
        code_api|linux.rseq_noarray
        PROPERTIES LABELS RUNS_ON_QEMU)
    endif ()
  endif ()
  if (ARM)
    set_tests_properties(
      code_api|common.allasm_arm
      code_api|common.allasm_thumb
      code_api|common.broadfun-stress
      code_api|linux.bad-signal-stack
      code_api|linux.alarm
      code_api|linux.fork-sleep
      code_api|linux.signal_race
      code_api|linux.signal0000
      code_api|linux.signal0001
      code_api|linux.signal0010
      code_api|linux.signal0011
      code_api|linux.signal0100
      code_api|linux.signal0101
      code_api|linux.signal0110
      code_api|linux.signal0111
      code_api|linux.signal1000
      code_api|linux.signal1001
      code_api|linux.signal1010
      code_api|linux.signal1011
      code_api|linux.signal1100
      code_api|linux.signal1101
      code_api|linux.signal1110
      code_api|linux.signal1111
      code_api|linux.sigplain000
      code_api|linux.sigplain001
      code_api|linux.sigplain010
      code_api|linux.sigplain011
      code_api|linux.sigplain100
      code_api|linux.sigplain101
      code_api|linux.sigplain110
      code_api|linux.sigplain111
      code_api|security-common.TestAllocWE
      PROPERTIES LABELS RUNS_ON_QEMU)
  endif ()
endif (NOT ANDROID AND AARCHXX)

# XXX i#5437, i#5431: While we work through Ubuntu22 issues we want our new GA CI job to
# just run sanity tests that do pass now as a glibc change detector to see if our
# workarounds so far break over time.
set_tests_properties(
  code_api|common.broadfun
  PROPERTIES LABELS UBUNTU_22)
if (BUILD_SAMPLES AND NOT ANDROID AND NOT RISCV64) # TODO i#3544: Port tests to RISC-V 64
  set_tests_properties(
    code_api|sample.bbsize
    PROPERTIES LABELS UBUNTU_22)
endif ()
if (BUILD_CLIENTS AND NOT ANDROID)
  set_tests_properties(
    code_api|tool.drcachesim.simple
    code_api|tool.drcacheoff.simple
    PROPERTIES LABELS UBUNTU_22)
endif ()
if (LINUX AND X64 AND HAVE_RSEQ AND NOT RISCV64) # TODO i#3544: Port tests to RISC-V 64
  set_tests_properties(
    code_api|api.rseq
    code_api|linux.rseq_disable
    code_api|linux.rseq_noarray
    code_api|linux.rseq
    code_api|linux.rseq_table
    code_api|tool.drcacheoff.rseq
    PROPERTIES LABELS UBUNTU_22)
endif ()

# XXX i#3544: Enable passed tests on riscv64.
if (RISCV64)
  set_tests_properties(
    code_api|api.ir
    code_api|api.ir-static
    PROPERTIES LABELS RISCV64)
endif ()
