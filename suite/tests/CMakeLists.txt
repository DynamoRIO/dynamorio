# **********************************************************
# Copyright (c) 2010-2021 Google, Inc.    All rights reserved.
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
cmake_minimum_required(VERSION 3.7)

include(../../make/policies.cmake NO_POLICY_SCOPE)
include(../runsuite_common_pre.cmake) # For _DR_set_VS_bitwidth_env_vars.

option(RUN_LONG_SUITE "run long set of tests")
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
  "SHORT::X64::LIN::ONLY::drcache.*\\.simple$|selfmod2|racesys|reachability|fork$::-code_api -satisfy_w_xor_x"
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
      set(${test}_realtest ${ci_shared_app} PARENT_SCOPE)
    endif ()
  endif ()

  add_library(${test}.dll SHARED ${client_source})
  if (NOT "${source}" MATCHES "cpp\\.cpp")
    # to avoid changing all the REG_ constants we ask for compatibility
    set(DynamoRIO_REG_COMPATIBILITY ON)
  endif (NOT "${source}" MATCHES "cpp\\.cpp")
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
macro(tobuild_api test source dr_ops exe_ops use_decoder use_static_DR)
  add_api_exe(${test} ${source} ${use_decoder} ${use_static_DR})
  torunonly_api(${test} OFF ${source} "${dr_ops}" "${exe_ops}" ${use_decoder})
endmacro(tobuild_api)

# tobuild_static_nohide_api builds a static API test which uses the
# dynamorio_static_nohide as a base library. It is a macro so the PARENT_SCOPE
# in torunonly_api will reach caller of this.
# XXX: Do we want to support a nicer API for 'nohide' than assuming that
# add_api_exe adds dynamorio_static and replacing it manually with
# dynamorio_static_nohide? Punting until there are more uses: it's a lot of
# plumbing for no clear gain.
macro(tobuild_static_nohide_api test source dr_ops exe_ops use_decoder)
  add_api_exe(${test} ${source} ${use_decoder} ON)
  torunonly_api(${test} OFF ${source} "${dr_ops}" "${exe_ops}" ${use_decoder})
  get_target_property(lib_list ${test} LINK_LIBRARIES)
  list(REMOVE_ITEM lib_list dynamorio_static)
  set_property(TARGET ${test} PROPERTY LINK_LIBRARIES ${lib_list} )
  target_link_libraries(${test} dynamorio_static_nohide)
endmacro(tobuild_static_nohide_api)

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
  append_property_string(TARGET ${target} LINK_FLAGS
    # The default value of CMAKE_SHARED_LIBRARY_LINK_C_FLAGS has -rdynamic, which
    # with some tool chains (such as SUSE GCC 6.2) results in an executable that
    # is not properly static. Adding --no-export-dynamic avoids the problem.
    "-Wl,--no-export-dynamic")
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
  if (WIN32 AND CMAKE_C_COMPILER_VERSION VERSION_LESS 19.0)
    set(rundefs "${rundefs} -DPRE_VS2015")
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

  if (is_runall)
    # To run the test we use a series of commands in runall.cmake.
    file(READ "${testpath}/${srcbase}.runall" runall)
    if (UNIX)
      # We always run infloop.
      get_target_path_for_execution(app_path linux.infloop "${location_suffix}")
    else ()
      # It's easier to run our own infloop, which prints so we know when it's up,
      # than to run notepad or sthg.
      get_target_path_for_execution(app_path win32.infloop "${location_suffix}")
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
      string(REGEX REPLACE ";-stderr_mask;0xC;" ";-verbose;-stderr_mask;0xF;" nudge_arg "${nudge_arg}")
      string(REGEX REPLACE ";" "@" nudge_arg "${nudge_arg}")
      # clear client from dr command and run native
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
        get_target_path_for_execution(toolbindir nudgeunix "${location_suffix}")
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
  endif ()

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
    "${${test}_exe_ops}" added)
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
      "${${test}_exe_ops}" added)
  else ()
    # simple client
    torun("${name}" ${test} ${${test}_source} OFF OFF
      "${${test}_dr_ops};${ops};-c;${${test}_client_path};${cli_ops}"
      "${${test}_exe_ops}" added)
  endif ()
  if (added)
    set_properties(${name} ${test} ${myprefix})
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
else ()
  set(sfx "x86")
endif ()
if (NOT ANDROID)
  # XXX i#1874: get working on Android
  tobuild_api(api.ir api/ir_${sfx}.c "" "" OFF OFF)
  if (AARCH64)
    # The ir_aarch64.expect file is too large for CMake's regexes.
    set(api.ir_runcmp "${CMAKE_CURRENT_SOURCE_DIR}/runcmp.cmake")
    set(api.ir_runcmp_capture "stderr")
  endif ()
  # XXX i#1686: add ARM's headers too once they are all created
  if (NOT ARM AND NOT AARCH64 AND "${CMAKE_GENERATOR}" MATCHES "Unix Makefiles")
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
    tobuild_api(api.ir_negative api/ir_aarch64_negative.c "" "" OFF OFF)
  endif (AARCH64)
endif ()

# test static decoder library
tobuild_api(api.ir-static api/ir_${sfx}.c "" "" ON OFF)
if (AARCH64)
  # The ir_aarch64.expect file is too large for CMake's regexes.
  set(api.ir-static_runcmp "${CMAKE_CURRENT_SOURCE_DIR}/runcmp.cmake")
  set(api.ir-static_runcmp_capture "stderr")
endif ()

if (ARM)
  if (NOT ANDROID) # XXX i#1874: get working on Android
    tobuild_api(api.drdecode api/drdecode_arm.c "" "" ON OFF)
  endif ()
elseif (AARCH64)
  tobuild_api(api.drdecode api/drdecode_aarch64.c "" "" ON OFF)
else ()
  tobuild_api(api.drdecode api/drdecode_x86.c "" "" ON OFF)
endif ()

if (BUILD_CLIENTS)
  ###########################################################################
  # drdisas tests
  # XXX: We should try to export or share the test list, filtering, matching,
  # and other code here to support tests being declared locally rather than
  # in one central place.
  if (AARCH64)
    set(drdisas_args "f94017a0" "a9be7bfd" "88")
  elseif (ARM)
    set(drdisas_args "6813" "f243" "42c8" "b2db" "88")
  else ()
    set(drdisas_args "-syntax" "dr" "66" "90" "c4" "e2" "65" "90" "14" "80" "88")
  endif ()
  torunonly_api(tool.drdisas drdisas
    "../../clients/drdisas/test_simple.template" "" "${drdisas_args}" ON)
  if (ARM)
    torunonly_api(tool.drdisas_thumb drdisas
      # Pass -thumb as a "DR op" to define it in the .template file.
      "../../clients/drdisas/test_simple.template"
      "-thumb" "-mode;thumb;${drdisas_args}" ON)
  endif ()
  # Test syntax
  # XXX i#:4021: ARM/AARCH64 not yet supported.
  if (X86)
      set(drdisas_args "66" "90" "c4" "e2" "65" "90" "14" "80")
      torunonly_api(tool.drdisas_syntax drdisas
          "../../clients/drdisas/test_syntax.template" "" "${drdisas_args}" ON)
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
      OFF)
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
tobuild(common.${control_flags} common/${control_flags}.c)

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
  tobuild_ops(common.allasm_aarch64_isa common/allasm_aarch64_isa.asm
    "-early_inject" "") # FIXME i#1569: -enable_traces here
  set_target_properties(common.allasm_aarch64_isa PROPERTIES LINKER_LANGUAGE C)
  append_link_flags(common.allasm_aarch64_isa
    "-nostartfiles -nodefaultlibs -static")

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
      tobuild_ops(common.loglevel common/allasm_x86_64.asm
        "-loglevel 19" "")
      set_target_properties(common.loglevel PROPERTIES LINKER_LANGUAGE C)
      append_link_flags(common.loglevel "-nostartfiles -nodefaultlibs -static")
    endif ()
  elseif (DR_HOST_AARCH64)
    torunonly(common.loglevel common.allasm_aarch64_cache
      common/allasm_aarch64_cache.asm "-loglevel 19" "")
  elseif (DR_HOST_ARM AND NOT ANDROID)
    torunonly(common.loglevel common.allasm_thumb
      common/allasm_thumb.asm "-loglevel 19" "")
  endif ()
endif ()

if (NOT ANDROID) # XXX i#1874: get working on Android
  # We don't want to link with DR.  We hack that for now by requesting drdecode so
  # we'll get the arch defs.  XXX: Add a better way!
  tobuild_api(libutil.frontend_test libutil/frontend_test.c "" "" ON OFF)
  target_link_libraries(libutil.frontend_test drfrontendlib)
endif ()

set(dr_root ${PROJECT_BINARY_DIR})
# Just like frontend_test we request drdecode.
tobuild_api(libutil.drconfig_test libutil/drconfig_test.c "" "${dr_root}" ON OFF)
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

tobuild_ci(client.attach_test client-interface/attach_test.runall "" "" "")