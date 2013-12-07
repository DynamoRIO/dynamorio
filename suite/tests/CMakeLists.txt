# **********************************************************
# Copyright (c) 2010-2013 Google, Inc.    All rights reserved.
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

option(TEST_LONG "run long set of tests")
# if TEST_SUITE is off, we disable some flaky tests in order to have all
# tests succeed (i#389)
option(TEST_SUITE "running series of builds so skip some overlapping tests; also, include racy/flaky tests (eventually they should be fixed) for better coverage")
set(TEST_SECONDS "60" CACHE STRING "Test time limit in seconds")
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

set(vmap_run_list
  # our main configuration
  "SHORT::-code_api"
  # sanity check: run single app to make sure these options aren't totally broken
  "SHORT::ONLY::client.events$::-code_api -opt_memory"
  "SHORT::ONLY::client.events$::-code_api -opt_speed"
  "SHORT::ONLY::client.events$::-code_api -thread_private"
  "SHORT::ONLY::client.events$::-code_api -disable_traces"
  "SHORT::ONLY::client.events$::-code_api -thread_private -disable_traces"
  "SHORT::LIN::ONLY::client.events$::-code_api -early_inject"
  # maybe this should be SHORT as -coarse_units will eventually be the default?
  "-code_api -opt_memory"
  "-code_api -opt_speed"
  "-code_api -thread_private"
  "-code_api -disable_traces"
  "-code_api -thread_private -disable_traces"
  "LIN::-code_api -early_inject"
  "DEBUG::ONLY::^common::-code_api -loglevel 1"
  "ONLY::^common::-code_api -stack_size 120k"
  "WIN::ONLY::^(runall|client)::-enable_full_api"
  "DEBUG::WIN::ONLY::^(common|client)::-code_api -stack_size 120K -loglevel 1 -no_hide"
  "DEBUG::LIN::ONLY::^(common|client)::-code_api -stack_size 120K -loglevel 1"
  "ONLY::^common::"
  "LIN::ONLY::^${osname}::-code_api -sysenter_is_int80"

  # cover -tracedump_* options, just a couple combinations
  # FIXME: how up time limit?  had TEST_MINS=5 for next 4 runs:
  "ONLY::^common::-code_api -tracedump_text -tracedump_origins"
  "ONLY::^common::-code_api -tracedump_text -tracedump_origins -syntax_intel"
  "ONLY::^common::-code_api -thread_private -tracedump_binary"
  # make sure we at least sometimes exercise non-default -checklevel
  "DEBUG::-checklevel 4"

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

set(vmsafe_run_list
  # keep lightweight modes alive
  # i#24: -thin_client not working: once it works, re-enable
  #"WIN::ONLY::^runall::-thin_client"
  "WIN::ONLY::^runall::-probe_api -hotp_only"
  # our supported configurations
  "ONLY::^[^h]::-code_api -probe_api"
  "ONLY::^security ^runall runinterface::-security_api"
  "ONLY::^[^h]::-security_api"
  "WIN::ONLY::^(runall|${osname})::-probe_api -hotp_only"
  "WIN::ONLY::^(common|security)::-probe_api -hotp_only"
  # other configurations we may eventually support
  "ONLY::^[^h]::-security_api -code_api"
  "-security_api -probe_api"
  "-security_api -code_api -probe_api"
  )

# xref the very long set of runs we used to test in
# suite/old-runregression-runs-2-6-08
set(vps_run_list
  "ONLY::^(common|${osname}|security|runall)::"
  "ONLY::^(common|${osname}|security|runall)::-security"
  "ONLY::^security::-security -detect_mode -report_max 0 -no_diagnostics"
  "WIN::ONLY::^(common|${osname}|security|runall)::-security -liveshields"
  "WIN::ONLY::^runall ^security::-security -client"
  "WIN::ONLY::^runall ^security::-security -low" # not a default config
  # Test with -use_moduledb relaxations, (is on by default, but tests Makefile
  # adds -staged which turns it off, we re-enable by turning the option back on).
  # The tests Makefile adds different version resource information to different
  # tests in runall/ and security-{common,win32}/ to fully exercise this.
  "WIN::ONLY::^(runall|security)::-security -use_moduledb"
  )
# if building dedicated vps, want short suite of all tests
set(vps_short_list
  "SHORT::-security"
  )

if (LINKCOUNT AND TEST_SUITE)
  # FIXME: how up time limit?  had TEST_MINS=5 for this:
  set(run_list "ONLY::^common::-prof_counts -tracedump_binary -no_inline_trace_ibl -indirect_stubs")
elseif (CALLPROF AND TEST_SUITE)
  set(run_list "ONLY::^common::")
elseif (VMAP)
  if (TEST_SUITE)
    # if running multiple builds we don't redo all tests for later builds
    # we expect internal debug to catch most problems
    if (DEBUG AND INTERNAL)
      set(run_list ${vmap_run_list})
    else (DEBUG AND INTERNAL)
      set(run_list "ONLY::^client::-code_api")
    endif (DEBUG AND INTERNAL)
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
else (LINKCOUNT AND TEST_SUITE)
  # no tests
  set(run_list "")
endif (LINKCOUNT AND TEST_SUITE)

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
  endif (NOT GCC)
else (WIN32)
  # i#215: if chcon exists then we'll mark tests w/ text relocations to
  # avoid violating selinux policies.  On Ubuntu, chcon is part of coreutils
  # package too, so it will be there even if selinux isn't enabled, so check for
  # for selinux.
  find_program(SEUNIXENABLED selinuxenabled DOC "path to selinuxenabled")
  if (SEUNIXENABLED)
    find_program(CHCON chcon DOC "path to chcon")
    if (CHCON)
      message(STATUS "Detected selinux: will chcon binaries with text relocations.")
      set(SEUNIXFOUND "yes")
    endif (CHCON)
    mark_as_advanced(CHCON)
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
include_directories(${PROJECT_SOURCE_DIR}/core/x86) # asm_defines.asm

# FIXME i#98: improve code so can set Wall/W4
if (UNIX)
  string(REGEX REPLACE "-Wall" "" CMAKE_C_FLAGS "${CMAKE_C_FLAGS}")
  # some tests rely on specific "nop;nop" patterns that optimization ruins
  # we should probably move the -O from top level into core/CMakeLists.txt
  string(REGEX REPLACE "-O[0-9]? " " " CMAKE_C_FLAGS "${CMAKE_C_FLAGS}")
  # i#720: cmake fails to find 32-bit libraries in Ubuntu 11.10.
  # This is because cmake uses CMAKE_LIBRARY_ARCHITECTURE to handle 
  # multi-arch compilation, whose value is x86_64-linux-gnu or i386-linux-gnu
  # for 64/32-bit. However, in Ubuntu 11.10, some of the 32-bit libraries
  # like libm.so, libdl.so, and libpthread.so are in /usr/lib32 or /lib32
  # instead of /lib/i386-linux-gnu, so cmake cannot to find them.
  # solution: add explicit paths for lookup.
  find_library(libmath m)
  if (NOT libmath AND NOT X64)
    find_library(libmath m PATHS /usr/lib32 /lib32)
  endif ()
  find_library(libdl dl)
  if (NOT libdl AND NOT X64)
    find_library(libdl dl PATHS /usr/lib32 /lib32)
  endif ()
  find_library(libpthread pthread)
  if (NOT libpthread AND NOT X64)
    find_library(libpthread pthread PATHS /usr/lib32 /lib32)
  endif ()
  if (NOT libmath OR NOT libdl OR NOT libpthread)
    message(FATAL_ERROR "cannot find required libs m, dl, and/or pthread")
  endif ()
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
  if (X64)
    set(ARCH_CFLAGS "-m64")
  else (X64)
    set(ARCH_CFLAGS "-m32")
  endif (X64)
  set(ARCH_LDFLAGS "${ARCH_CFLAGS}")
  set(ARCH_DEBUG "${ARCH_CFLAGS}") # somehow not being propagated so we set here
else (UNIX)
  set(ARCH_CFLAGS "")
  # we don't add /debug here (=> SHARED_LINKER flags) since can't remove
  # XXX i#893: VS2012 won't build any tools_asm test w/ safeseh
  set(ARCH_LDFLAGS "/safeseh:no")
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

if (WIN32)
  # for resources.rc to include globals_shared.h
  # must be AFTER build/include now that dr_api.h is configured
  include_directories(AFTER ${PROJECT_SOURCE_DIR}/core/lib)
endif (WIN32)

# Since we're including configure.h we do not want any default defines.
# Note that the new DynamoRIOConfig.cmake doesn't set these so we can
# probably remove this line.  We remove from target-specific flags
# from configure_DynamoRIO_* in tobuild_api().
remove_definitions(-DX64 -DLINUX -DWINDOWS)
# If people want perf stress tests they'll have to manually build to avoid
# the NIGHTLY_REGRESSION define
set(test_defs "-DNIGHTLY_REGRESSION")
set(platform_defs "${test_defs}")
if (X64)
  set(platform_defs "${platform_defs} -DX64")
endif (X64)
if (UNIX)
  set(platform_defs "${platform_defs} -DLINUX")
else (UNIX)
  set(platform_defs "${platform_defs} -DWINDOWS")
endif (UNIX)

##################################################

set(asm_deps
  "${PROJECT_SOURCE_DIR}/core/x86/asm_defines.asm"
  "${PROJECT_BINARY_DIR}/configure.h")
set(asm_defs
  -I "${CMAKE_CURRENT_SOURCE_DIR}" # tools.h
  -I "${PROJECT_SOURCE_DIR}/core/x86") # asm_defines.asm

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
  file(READ ${CMAKE_CURRENT_SOURCE_DIR}/${source} srccode)
  set(srccode "${srccode}" PARENT_SCOPE)
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

# On Linux, the individual object files contained by an archive are
# garbage collected by the linker if they are not referenced.  To avoid
# this, we have to use the --whole-archive option with ld.
function(force_static_link target lib)
  if (UNIX)
    # CMake ignores libraries starting with '-' and preserves the
    # ordering, so we can pass flags through target_link_libraries, which
    # ensures we have the right CMake dependencies.
    target_link_libraries(${target} -Wl,--whole-archive ${lib} -Wl,--no-whole-archive)
  else ()
    target_link_libraries(${target} ${lib})
  endif ()
endfunction(force_static_link)

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

  add_executable(${test} ${test_srcs})

  set_target_properties(${test} PROPERTIES
    LINK_FLAGS "${ARCH_DEBUG}"
    # we don't want the default rpath of <builddir>/lib as it makes
    # it hard to use other DR builds w/ these apps
    SKIP_BUILD_RPATH ON)
  if (UNIX)
    target_link_libraries(${test} ${libmath} ${libdl})
    if ("${test}" MATCHES "^pthread|startstop")
      target_link_libraries(${test} ${libpthread})
    endif ("${test}" MATCHES "^pthread|startstop")
  else (UNIX)
    append_link_flags(${test} "${ARCH_LDFLAGS}")
  endif (UNIX)
  if ("${srccode}" MATCHES "include \"tools\\.h\"")
    target_link_libraries(${test} tools)
  endif ()
  if ("${srccode}" MATCHES "ifndef ASM_CODE_ONLY" AND
      "${CMAKE_GENERATOR}" MATCHES "Visual Studio")
    add_dependencies(${test} ${gen_asm_tgt})
  endif ()

  if (STATIC_LIBRARY AND NOT "${test}" MATCHES "api")
    # Link DR into every test exe except the api tests, which already link
    # against DR or the standalone decoder.
    force_static_link(${test} dynamorio)
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
  add_exe(${test} ${source})
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
  target_link_libraries(${test} ${test}.dll)
  if ("${srccode}" MATCHES "include \"tools\\.h\"")
    target_link_libraries(${test}.dll tools)
  endif ()

  set(testlist_normal ${testlist_normal} ${test} PARENT_SCOPE)
  set(${test}_source ${source} PARENT_SCOPE)
  set(${test}_dr_ops ${dr_ops} PARENT_SCOPE)
endfunction(tobuild_dll)

function(get_client_path client_path_out realclient realexe)
  if (STATIC_LIBRARY)
    set(tgt ${realexe})
  else()
    set(tgt ${realclient})
  endif ()
  get_target_property(client_path ${tgt} LOCATION${location_suffix})
  set(${client_path_out} "${client_path}" PARENT_SCOPE)
endfunction(get_client_path)

# client interface app with client
# note that we can't easily have a routine that takes no ops
# and calls this one w/ "" "" "" b/c the PARENT_SCOPE won't make
# it all the way back then: would need to chain.
function(tobuild_ci test source client_ops dr_ops exe_ops)
  string(REGEX REPLACE "\\.(c|cpp)$" ".dll.\\1" client_source "${source}")
  string(REGEX REPLACE "\\.runall$" ".dll.c" client_source "${client_source}")

  if (NOT "${source}" MATCHES "\\.runall$")
    add_exe(${test} ${source})
  elseif (STATIC_LIBRARY)
    return()  # FIXME i#975: add support for STATIC_LIBRARY runall tests.
  endif ()

  add_library(${test}.dll ${SHARED_OR_STATIC} ${client_source})
  if (NOT "${source}" MATCHES "cpp\\.cpp")
    # to avoid changing all the REG_ constants we ask for compatibility
    set(DynamoRIO_REG_COMPATIBILITY ON)
  endif (NOT "${source}" MATCHES "cpp\\.cpp")
  configure_DynamoRIO_client(${test}.dll)
  append_property_string(TARGET ${test}.dll LINK_FLAGS "${ARCH_DEBUG}")
  add_dependencies(${test}.dll api_headers)
  get_client_path(client_path ${test}.dll ${test})
  if (STATIC_LIBRARY)
    # For STATIC_LIBRARY, we support a single client by linking the client into
    # the exe.
    force_static_link(${test} ${test}.dll)
    # Let clients know if they're being built statically.
    append_property_string(TARGET ${test}.dll COMPILE_FLAGS "-DSTATIC_LIBRARY")
  endif ()

  set(testlist_ci ${testlist_ci} ${test} PARENT_SCOPE)
  set(${test}_source ${source} PARENT_SCOPE)
  set(${test}_client_ops ${client_ops} PARENT_SCOPE)
  set(${test}_dr_ops ${dr_ops} PARENT_SCOPE)
  set(${test}_exe_ops ${exe_ops} PARENT_SCOPE)
  set(${test}_client_path ${client_path} PARENT_SCOPE)
endfunction(tobuild_ci)

function(configure_app_api_build_flags test decoder)
  # save property since configure_DynamoRIO_standalone will clobber it
  get_target_property(pre_lflags ${test} LINK_FLAGS)
  # disable the default rpath for standalone apps
  set(DynamoRIO_RPATH OFF)
  # to avoid changing all the REG_ constants we ask for compatibility
  set(DynamoRIO_REG_COMPATIBILITY ON)
  if (decoder)
    configure_DynamoRIO_decoder(${test})
    set(extra_flags "-DSTANDALONE_DECODER")
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
      configure_app_api_build_flags(${test}.appdll OFF)
      add_dependencies(${test}.appdll api_headers)
    endif()
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

# client interface standalone app
function(tobuild_api test source dr_ops exe_ops decoder)
  add_exe(${test} ${source})
  configure_app_api_build_flags(${test} ${decoder})
  add_dependencies(${test} api_headers)
  if (WIN32 AND NOT decoder)
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
          "${CMAKE_CURRENT_BINARY_DIR}/dynamorio.dll"
        # We copy drsyms.dll too, for api.symtest
        COMMAND ${CMAKE_COMMAND} ARGS
          -E copy "${PROJECT_BINARY_DIR}/ext/${INSTALL_LIB}/drsyms.dll"
          "${CMAKE_CURRENT_BINARY_DIR}/drsyms.dll"
        VERBATIM)
      add_custom_target(drcopy DEPENDS "${drcopy_stamp}")
      set(created_drcopy_target ON PARENT_SCOPE)
    endif ()
    add_dependencies(${test} drcopy)
  endif (WIN32 AND NOT decoder)
  set(testlist_api ${testlist_api} ${test} PARENT_SCOPE)
  set(${test}_source ${source} PARENT_SCOPE)
  set(${test}_dr_ops ${dr_ops} PARENT_SCOPE)
  set(${test}_exe_ops ${exe_ops} PARENT_SCOPE)
  # If using drdecodelib, the app does not load DR, so no reason to use drrun
  # to set options (plus drrun will create a never-deleted .1config file).
  set(${test}_drinject ${decoder} PARENT_SCOPE)
endfunction(tobuild_api)

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
  if (UNIX)
    # Use -Wl escaping because CMake invokes the compiler to link.
    get_target_property(existing_flags ${target} LINK_FLAGS)
    set_target_properties(${target} PROPERTIES LINK_FLAGS
      "-Wl,-z,execstack ${existing_flags}")
  endif (UNIX)
endfunction(mark_execstack)

###########################################################################
# RUNNING

set(PCACHE_DIR "${PROJECT_BINARY_DIR}/pcache")
set(PCACHE_SHARED_DIR "${PROJECT_BINARY_DIR}/pcache/shared")

function(template2expect outexpect template runops)
  # We used to use a custom perl script to match defines and runtime
  # options but we've switched to cpp.  We convert -foo to -Dfoo and
  # -foo value to -Dfoo___value.
  # First we strip out the single-quoted client_lib arg
  string(REGEX REPLACE
    "'.*'"
    "_xx_" rundefs "${runops}")
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

function(rundr_cmd outcmd outops native use_drinject dr_ops_aux separate_script)
  # assumes tools have been built: enforced at top level
  if (UNIX)
    set(os_ops "-dumpcore_mask 0")
  else ()
    # Turn on core dumps for most fatal errors.  Don't dump for curiosities or
    # security violations.
    set(os_ops "-msgbox_mask 0 -dumpcore_mask 0x7d -staged")
  endif ()
  set(dr_ops "-stderr_mask 0xC ${os_ops} ${dr_ops_aux} ${TEST_OPTIONS}")

  if (separate_script)
    # eliminate double-spaces, to help w/ passing strings to runall.cmake
    string(REGEX REPLACE "  +" " " dr_ops "${dr_ops}")
    string(REGEX REPLACE " $" "" dr_ops "${dr_ops}")
    # ; must be quoted: can't seem to match \; or replace w/ \ so using !
    string(REGEX REPLACE ".;" "!" dr_ops "${dr_ops}")
  endif (separate_script)

  if (WIN32 OR NOT native)
    # Both Windows and non-native Unix tests now use the native drrun frontend.
    # Since the creation of CTestTestfile.cmake ends up replacing \ with / we
    # can't escape quotes, so we have to use a list variable
    if (use_drinject)
      get_target_property(drrun_path drinject LOCATION${location_suffix})
    else (use_drinject)
      get_target_property(drrun_path drrun LOCATION${location_suffix})
    endif (use_drinject)
    set(cmd "${drrun_path}" -s ${TEST_SECONDS} -quiet)
    if (DEBUG)
      set(cmd ${cmd} -debug)
    endif ()
    if (UNIX)
      set(cmd ${cmd} -killpg)
    endif ()
    if (UNIX AND TEST_SUITE)
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
    if (NOT use_drinject)
      # add dr_ops last to avoid needing extra \ on ;
      set(${outcmd} ${cmd} -ops "${dr_ops}" PARENT_SCOPE)
    endif ()

  elseif (UNIX AND native)
    set(cmd ${MAIN_RUNTIME_OUTPUT_DIRECTORY}/runstats -s ${TEST_SECONDS} -killpg -silent)
    # We set LD_LIBRARY_PATH for native api/ apps since we're not setting
    # rpath (to make it easier to try different trees)
    set(cmd ${cmd} -env LD_LIBRARY_PATH
      "${MAIN_LIBRARY_OUTPUT_DIRECTORY}:${EXT_LIBRARY_OUTPUT_DIRECTORY}:$ENV{LD_LIBRARY_PATH}")
    # add dr_ops last to avoid needing extra \ on ;
    set(${outcmd} ${cmd} -env DYNAMORIO_OPTIONS "${dr_ops}" PARENT_SCOPE)
  endif ()

  set(${outops} "${dr_ops}" PARENT_SCOPE)
endfunction(rundr_cmd)

# if test != exe we assume exe is already added as a build target
# key is the test name without the options, and is used to look up ${key}_expectbase
function(torun test key source native use_drinject dr_ops exe_ops added_out)
  # Avoid running some tests with invalid option combinations.  For example,
  # coarse fragments are incompatible with thread private code caches.
  if ("${dr_ops}" MATCHES "-thread_private" AND
      "${dr_ops}" MATCHES "-opt_memory")
    set(added_out OFF PARENT_SCOPE)
    return()
  endif ()
  if ("${dr_ops}" MATCHES "-early_inject" AND
      "${dr_ops}" MATCHES "-no_private_loader")
    set(added_out OFF PARENT_SCOPE)
    return()
  endif ()
  if (STATIC_LIBRARY AND "${dr_ops}" MATCHES "-early_inject")
    set(added_out OFF PARENT_SCOPE)
    return()
  endif ()

  if (DEFINED ${key}_realtest)
    set(exe ${${key}_realtest})
  else (DEFINED ${key}_realtest)
    set(exe ${key})
  endif (DEFINED ${key}_realtest)

  get_filename_component(srcbase ${source} NAME_WE)
  get_filename_component(srcpath ${source} PATH)
  if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${srcpath}/${srcbase}.runall")
    set(is_runall ON)
  else ()
    set(is_runall OFF)
  endif ()
  if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${srcpath}/${srcbase}.runcmp" OR
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

  rundr_cmd(rundr runops ${native} ${use_drinject} "${dr_ops}" ${separate_script})
  get_target_property(exepath ${exe} LOCATION${location_suffix})
  # support running binaries that are not targets of this build
  if (NOT exepath)
    set(exepath ${exe})
  endif (NOT exepath)

  if (${test} MATCHES "linux.execve32")
    # We're an x64 build, but we need to launch a 32-bit app under DR.  The
    # suite sets -dr_home to the full install dir with both archs, but we're
    # invoking bin64/drrun.  Ideally drrun would detect which ELF class we're
    # about to exec, but until then we have to add -32 for 32-bit apps.
    string(REGEX REPLACE "/drrun;" "/drrun;-32;" rundr "${rundr}")
  endif ()

  if (is_runall)
    # to run the test we use a series of commands in runall.cmake.
    file(READ "${CMAKE_CURRENT_SOURCE_DIR}/${srcpath}/${srcbase}.runall" runall)
    if (UNIX)
      # swap from drrun to run_in_bg to run the test in the background
      set(tmpfile "${CMAKE_CURRENT_BINARY_DIR}/${test}-out")
      string(REGEX REPLACE "^.*(${MAIN_RUNTIME_OUTPUT_DIRECTORY}/drrun)" 
        "${MAIN_RUNTIME_OUTPUT_DIRECTORY}/run_in_bg;-out;${tmpfile};\\1"
        rundr "${rundr}")
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
    endif (UNIX)
    # FIXME i#120: for Windows, get app name to run from .runall file.
    # For linux we always run linux.infloop.
    set(cmd_with_at ${rundr} ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/linux.infloop ${exe_ops})
    # we pass intra-arg spaces via @@ and inter-arg via @ and ; via !
    # to get around the pain of trying to quote everything just right:
    # much simpler this way.
    string(REGEX REPLACE " " "@@" cmd_with_at "${cmd_with_at}")
    string(REGEX REPLACE ";" "@" cmd_with_at "${cmd_with_at}")
    add_test(${test} ${CMAKE_COMMAND} -D toolbindir=${MAIN_RUNTIME_OUTPUT_DIRECTORY}
      -D cmd=${cmd_with_at} -D out=${tmpfile} -D nudge=${nudge_arg}
      -D clear=${clear_arg}
      -P ${CMAKE_CURRENT_SOURCE_DIR}/runall.cmake)
  elseif (is_runcmp)
    # Some apps have enough output that ctest runs out of memory when
    # doing its built-in regex cmp so we use a separate script.
    # We also use this approach for tests that need custom success tests.
    set(cmd_with_at ${rundr} ${exepath} ${exe_ops})
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
    add_test(${test} ${CMAKE_COMMAND} -D cmd=${cmd_with_at}
      -D cmp=${CMAKE_CURRENT_BINARY_DIR}/${srcbase}.expect
      -P ${runcmp_script})
    # No support for regex here (ctest can't handle large regex)
    set(ALREADY_REGEX ON)
  else (is_runcmp)
    add_test(${test} ${rundr} ${exepath} ${exe_ops})
  endif (is_runall)

  if (DEFINED ${key}_expectbase)
    set(expectbase ${${key}_expectbase})
  else ()
    set(expectbase ${srcbase})
  endif ()

  if (EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${srcpath}/${expectbase}.expect)
    file(READ ${CMAKE_CURRENT_SOURCE_DIR}/${srcpath}/${expectbase}.expect expect)
    # add dependence so cmake will reconfigure if file changes
    configure_file(${CMAKE_CURRENT_SOURCE_DIR}/${srcpath}/${expectbase}.expect
      ${CMAKE_CURRENT_BINARY_DIR}/ignoreme_for_dep)
  elseif (EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${srcpath}/${expectbase}.template)
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
      ${CMAKE_CURRENT_SOURCE_DIR}/${srcpath}/${expectbase}.template
      ${runops})
    # add dependence so cmake will reconfigure if file changes
    configure_file(${CMAKE_CURRENT_SOURCE_DIR}/${srcpath}/${expectbase}.template
      ${CMAKE_CURRENT_BINARY_DIR}/ignoreme_for_dep)
  elseif (EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${srcpath}/${expectbase}.templatex)
    # A .templatex is a .template that is treated directly as a regex:
    # so any regex chars inside are not converted to literals.
    # We separate the two so that we don't have to escape everything in
    # regular templates where we want a literal match.
    template2expect(expect
      ${CMAKE_CURRENT_SOURCE_DIR}/${srcpath}/${expectbase}.templatex
      ${runops})
    # add dependence so cmake will reconfigure if file changes
    configure_file(${CMAKE_CURRENT_SOURCE_DIR}/${srcpath}/${expectbase}.templatex
      ${CMAKE_CURRENT_BINARY_DIR}/ignoreme_for_dep)
    set(ALREADY_REGEX ON)
  else (EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${srcpath}/${expectbase}.expect)
    message(FATAL_ERROR
      "no .expect or .template or .templatex for ${srcpath}/${expectbase} for ${key}")
  endif (EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${srcpath}/${expectbase}.expect)

  if (NOT ALREADY_REGEX)
    # turn regex chars into literals
    # \\] somehow messes up the match though $expect looks identical
    # some tests do have ']' so we replace it in a separate step below
    string(REGEX REPLACE "([\\^\\$\\.\\*\\+\\?\\|\\(\\)\\[])" 
      "\\\\\\1" expect "${expect}")
    string(REGEX REPLACE "\\]"
      "\\\\]" expect "${expect}")
  endif (NOT ALREADY_REGEX)

  if (WIN32)
    # libc print => CRLF, dr_printf => plain newline: match both
    string(REGEX REPLACE "\n" "\r?\n" expect "${expect}")
  endif (WIN32)    

  if (is_runcmp)
    file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/${expectbase}.expect" "${expect}")
  else ()
    # match whole output
    set(expect "^${expect}$")
    set_tests_properties(${test} PROPERTIES PASS_REGULAR_EXPRESSION "${expect}")
  endif ()

  if (WIN32)
    # With i#265/PR 486139 we use local one-time config files instead of
    # global reg keys and can thus run multiple instances of the same
    # app name simultaneously so we do not need to mark RUN_SERIAL
  endif (WIN32)
  set(added_out ON PARENT_SCOPE)
endfunction(torun)

function(torun_normal name test ops)
  if (STATIC_LIBRARY AND "${ops}" MATCHES "-early_inject")
    return()
  endif ()
  torun("${name}" ${test} ${${test}_source} OFF OFF "${${test}_dr_ops} ${ops}"
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
  if (STATIC_LIBRARY AND "${${test}_dr_ops} ${ops}" MATCHES "-early_inject")
    return()
  endif ()
  # note that we cannot use double quotes as they'll be stripped out
  torun("${name}" ${test} ${${test}_source} OFF OFF
    " -code_api -client_lib '${${test}_client_path}\;0\;${cli_ops}' ${${test}_dr_ops} ${ops}" "${${test}_exe_ops}" added)
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
  torun("${name}" ${test} ${${test}_source} ON ${${test}_drinject}
    "${${test}_dr_ops} ${ops}" "${${test}_exe_ops}" added)
endfunction(torun_api)

function(torun_runall name test ops)
  # FIXME NYI i#120
endfunction(torun_runall)

function(disable_optimizations_for_file source_file)
  get_source_file_property(cflags "${source_file}" COMPILE_FLAGS)
  string(REGEX REPLACE "[/-]O[1-9]" "" cflags "${cflags}")
  set_source_files_properties("${source_file}" PROPERTIES
    COMPILE_FLAGS "${cflags}")
endfunction(disable_optimizations_for_file)

###########################################################################
# tests

tobuild(common.broadfun common/broadfun.c)
tobuild(common.decode-bad common/decode-bad.c)
# FIXME i#105: get this working for 32-bit linux
if (X64 OR WIN32)
  tobuild(common.decode common/decode.c)
endif (X64 OR WIN32)
# FIXME i#1025: get working on Linux
if (WIN32)
  # We build all tests before running so decode will be built before
  # ctest -j ever ran this.  If we switch to --build-and-test then we'll
  # make these runonly tests make sure their exe is built.
  torunonly(common.decode-stress common.decode common/decode.c
    "-stress_recreate_state" "")
endif (WIN32)
tobuild(common.eflags common/eflags.c)
tobuild(common.fib common/fib.c)
tobuild(common.getretaddr common/getretaddr.c)
tobuild(common.floatpc common/floatpc.c)
torunonly(common.floatpc_xl8all common.floatpc common/floatpc.c "-translate_fpu_pc" "")

tobuild_appdll(common.nativeexec common/nativeexec.c)
get_target_property(native_dll_name common.nativeexec.appdll LOCATION${location_suffix})
get_filename_component(native_dll_name "${native_dll_name}" NAME)
tobuild_ops(common.nativeexec common/nativeexec.c "-native_exec_list ${native_dll_name}" "")
target_link_libraries(common.nativeexec common.nativeexec.appdll)
# We want rpath on Linux so we can load the appdll.
set_target_properties(common.nativeexec PROPERTIES SKIP_BUILD_RPATH OFF)
if (UNIX)
  # FIXME i#978: Windows support NYI
  torunonly(common.nativeexec_retakeover common.nativeexec common/nativeexec.c
    "-native_exec_list ${native_dll_name} -native_exec_retakeover" "")
  torunonly(common.nativeexec_exe common.nativeexec common/nativeexec.c
    "-native_exec_list common.nativeexec -native_exec_retakeover" "")
  torunonly(common.nativeexec_bindnow common.nativeexec common/nativeexec.c
    "-native_exec_list common.nativeexec -native_exec_retakeover" "-bind_now")
  torunonly(common.nativeexec_retakeover_opt common.nativeexec common/nativeexec.c
    "-native_exec_list ${native_dll_name} -native_exec_retakeover  -native_exec_opt -no_kstats -no_private_loader -no_mangle_app_seg" "")
  # No swap TLS and no kstats for optimized native_exec
  torunonly(common.nativeexec_exe_opt common.nativeexec common/nativeexec.c
    "-native_exec_list common.nativeexec -native_exec_retakeover -native_exec_opt -no_kstats -no_private_loader -no_mangle_app_seg" "")
  torunonly(common.nativeexec_bindnow_opt common.nativeexec common/nativeexec.c
    "-native_exec_list common.nativeexec -native_exec_retakeover -native_exec_opt -no_kstats -no_private_loader -no_mangle_app_seg" "-bind_now")
endif ()

# app seg mangling makes dstack test fail so we disable such mangling
if (UNIX)
  # private loader requires the mangling
  tobuild_ops(common.protect-dstack common/protect-dstack.c
    "-no_mangle_app_seg -no_private_loader" "")
else (UNIX)
  tobuild_ops(common.protect-dstack common/protect-dstack.c
    "-no_mangle_app_seg" "")
endif (UNIX)

tobuild(common.segfault common/segfault.c)
# PR 217255: these 4 removed to shorten the regression suite since not
# adding much value in terms of testing corner cases
#tobuild(common.hello common/hello.c)
#tobuild(common.conflict common/conflict.c)
#tobuild(common.ops common/ops.c)
#tobuild(common.recurse common/recurse.c)

if (CLIENT_INTERFACE)
  tobuild_ci(client.abort client-interface/abort.c "" "" "")
  tobuild_ci(client.alloc client-interface/alloc.c "" "" "")
  tobuild_ci(client.call-retarget client-interface/call-retarget.c "" "" "")
  tobuild_ci(client.cleancall client-interface/cleancall.c "" "" "")
  tobuild_ci(client.count-ctis client-interface/count-ctis.c "" "" "")
  # check dr_insert_cbr_instrumentation with out-of-line clean call
  torunonly_ci(client.count-ctis-noopt client.count-ctis client.count-ctis.dll
    client-interface/count-ctis.c "" "-opt_cleancall 0" "")
  tobuild_ci(client.count-bbs client-interface/count-bbs.c "" "" "")
  tobuild_ci(client.syscall client-interface/syscall.c "" "-no_follow_children" "")
  tobuild_ci(client.modules client-interface/modules.c "" "" "")
  tobuild_appdll(client.modules client-interface/modules.c)
  if (UNIX)
    # XXX i#1246: Make partial_module_map work for Windows as well.
    tobuild_ci(client.partial_module_map client-interface/partial_module_map.c "" "" "")
    # FIXME i#975: we can't load libc with STATIC_LIBRARY yet.
    append_property_string(TARGET client.modules.appdll LINK_FLAGS "-nodefaultlibs")
  endif ()
  tobuild_ci(client.segfault client-interface/segfault.c "" "" "")
  tobuild_appdll(client.events client-interface/events.c)
  get_target_property(events_appdll_path client.events.appdll LOCATION${location_suffix})
  if (UNIX)
    # Just happens to not make as many traces.
    tobuild_ci(client.events client-interface/events.c
      "" "-trace_threshold 5" "${events_appdll_path}")
    tobuild_ci(client.events_cpp client-interface/events_cpp.cpp
      "" "-trace_threshold 5" "${events_appdll_path}")
    tobuild_ci(client.signal client-interface/signal.c "" "" "")
    tobuild_ci(client.syscall-mod client-interface/syscall-mod.c "" "" "")
    tobuild_ci(client.nudge_test client-interface/nudge_test.runall "" "" "")
    tobuild_ci(client.timer client-interface/timer.c "" "" "")
    tobuild_ci(client.cbr-retarget client-interface/cbr-retarget.c "" "" "")
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
    #tobuild_ci_runall(client.nudge_test client-interface/nudge_test.runall "" "" "")
    #tobuild_ci_runall(client.pc-check client-interface/pc-check.runall "" "" "")
    if (NOT X64)
      # FIXME i#16, does not work in Win-X64 because of inline assembly code.
      tobuild_ci(client.cbr-retarget client-interface/cbr-retarget.c "" "" "")
    endif (NOT X64)
  endif (UNIX)
  tobuild_ci(client.file_io client-interface/file_io.c
    "${CMAKE_CURRENT_SOURCE_DIR}/client-interface/file_io_data.txt" "" "")
  # we add custom option to flush test based on dr ops in torun_ci()
  tobuild_ci(client.flush client-interface/flush.c "" "" "")
  # FIXME: PR 199115 to re-enable fragdel, get some more of the UNIX tests working
  #tobuild_ci(client.fragdel client-interface/fragdel.c "" "" "")
  if (PROGRAM_SHEPHERDING)
    tobuild_ci(client.security client-interface/security.c "" "-security_api" "")
  endif (PROGRAM_SHEPHERDING)
  tobuild_ci(client.strace client-interface/strace.c "" "" "")
  use_DynamoRIO_extension(client.strace.dll drmgr)
  tobuild_ci(client.thread client-interface/thread.c "-paramx -paramy" "" "")
  tobuild_appdll(client.thread client-interface/thread.c)
  tobuild_ci(client.truncate client-interface/truncate.c "" "" "")
  tobuild_ci(client.unregister client-interface/unregister.c "" "" "")
  tobuild_ci(client.inline client-interface/inline.c "" "-opt_cleancall 3" "")
  tobuild_ci(client.dr_options client-interface/dr_options.c
    "" "-native_exec_list foo.dll,bar.dll -opt_cleancall 3 -thread_private" "")

  tobuild_ci(client.null_instrument client-interface/null_instrument.c "" "" "")
  tobuild_appdll(client.null_instrument client-interface/null_instrument.c)
  target_link_libraries(client.null_instrument client.null_instrument.appdll)
  # We want rpath on Linux so we can load the appdll.
  set_target_properties(client.null_instrument PROPERTIES SKIP_BUILD_RPATH OFF)

  # Test passing a really long (600 chars) client option string.
  tobuild_ci(client.large_options client-interface/large_options.c
    "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
    "" "")

  tobuild_ci(client.drcontainers-test client-interface/drcontainers-test.c "" "" "")
  use_DynamoRIO_extension(client.drcontainers-test.dll drcontainers)

  tobuild_ci(client.drmgr-test client-interface/drmgr-test.c "" "" "")
  use_DynamoRIO_extension(client.drmgr-test.dll drmgr)
  if (UNIX)
    target_link_libraries(client.drmgr-test ${libpthread})
  endif (UNIX)

  tobuild_ci(client.drx-test client-interface/drx-test.c "" "" "")
  use_DynamoRIO_extension(client.drx-test.dll drx)

  tobuild_appdll(client.drwrap-test client-interface/drwrap-test.c)
  get_target_property(drwrap_libpath client.drwrap-test.appdll LOCATION${location_suffix})
  tobuild_ci(client.drwrap-test client-interface/drwrap-test.c "" "" "${drwrap_libpath}")
  use_DynamoRIO_extension(client.drwrap-test.dll drwrap)
  tochcon(client.drwrap-test.appdll textrel_shlib_t)
  if (WIN32)
    # export from asm code
    append_link_flags(client.drwrap-test.appdll "/export:makes_tailcall")
  endif (WIN32)

  # We rely on dbghelp >= 6.0 for our drsyms and sample.instrcalls tests,
  # but the system dbghelp pre-Vista is too old, so we copy one from VS.
  if ("${CMAKE_SYSTEM_VERSION}" STRLESS "6.0")
    if (dbghelp_path) # set at top level
      configure_file(${dbghelp_path} ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/dbghelp.dll
        COPYONLY)
      if (BUILD_SAMPLES)
        get_property(sample_list GLOBAL PROPERTY DynamoRIO_sample_list)
        list(GET sample_list 1 sample_one) # 0 is empty
        get_target_property(sample_loc ${sample_one} LOCATION${location_suffix})
        get_filename_component(sample_dir ${sample_loc} PATH)
        configure_file(${dbghelp_path} ${sample_dir}/dbghelp.dll COPYONLY)
      endif (BUILD_SAMPLES)
      message(STATUS "Using ${dbghelp_path} for drsyms tests")
    endif (dbghelp_path)
  endif ()

  if (DR_EXT_DRSYMS_STATIC)
    # required for using drsyms built as a static library
    set(DynamoRIO_USE_LIBC ON)
  endif ()
  tobuild_appdll(client.drsyms-test client-interface/drsyms-test.cpp)
  get_target_property(drsyms_libpath client.drsyms-test.appdll LOCATION${location_suffix})
  tobuild_ci(client.drsyms-test client-interface/drsyms-test.cpp ""
    "-stack_size 36K" "${drsyms_libpath}")
  # Disable optimizations for the exe and appdll to allow stack tracing.
  disable_optimizations_for_file(client-interface/drsyms-test.cpp)
  disable_optimizations_for_file(client-interface/drsyms-test.appdll.cpp)
  use_DynamoRIO_extension(client.drsyms-test.dll drsyms)
  use_DynamoRIO_extension(client.drsyms-test.dll drwrap)  # Makes testing easy
  if (DR_EXT_DRSYMS_STATIC)
    set(DynamoRIO_USE_LIBC OFF) # reset
  endif ()

  if (WIN32 AND GCC AND NOT X64)
    # test drsyms dwarf-on-windows capabilities.
    # cmake doesn't support a new compiler so we do a simple build here.
    # we disable this for X64 b/c there is no 64-bit Cygwin and 64-bit MinGW
    # is in beta.
    tobuild_gcc(client.drsyms-testgcc.exe
      client-interface/drsyms-test.cpp client.drsyms-test "")
    tobuild_gcc(client.drsyms-testgcc.appdll.dll
      client-interface/drsyms-test.appdll.cpp client.drsyms-test "-shared")
    torunonly_ci(client.drsyms-testgcc
      ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/client.drsyms-testgcc.exe
      client.drsyms-test.dll client-interface/drsyms-test.cpp "" "-stack_size 36K"
      "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/client.drsyms-testgcc.appdll.dll")
    set(client.drsyms-testgcc_expectbase "drsyms-testgcc")
  endif (WIN32 AND GCC AND NOT X64)

  tobuild_ci(client.drutil-test client-interface/drutil-test.c "" "" "")
  use_DynamoRIO_extension(client.drutil-test.dll drutil)
  use_DynamoRIO_extension(client.drutil-test.dll drmgr)
  if (UNIX)
    target_link_libraries(client.drutil-test ${libpthread})
  endif (UNIX)

  # We need to load w/ the same base so the test passes
  set(DynamoRIO_SET_PREFERRED_BASE ON)
  set(PREFERRED_BASE 0x6f000000)
  # Pass extra options to first run so this will pass when run repeatedly in local
  # build dir w/o loading any pcache
  tobuild_ci(client.pcache client-interface/pcache.c ""
    "-persist -no_use_persisted -no_coarse_disk_merge -no_coarse_lone_merge" "")
  use_DynamoRIO_extension(client.pcache.dll drcontainers)
  if (WIN32)
    append_link_flags(client.pcache "/dynamicbase:no")
  endif ()
  set(DynamoRIO_SET_PREFERRED_BASE OFF)
  torunonly_ci(client.pcache-use client.pcache client.pcache.dll
    client-interface/pcache.c "" "-persist" "")
  # XXX: we should have the key be the default for the .expect but
  # currently that's not the case (thread-reset, etc.).
  set(client.pcache-use_expectbase "pcache-use")
  # when running tests in parallel: have to generate pcaches first
  set(client.pcache-use_depends client.pcache)

  tobuild_api(api.dis api/dis.c "-syntax_intel"
    "${CMAKE_CURRENT_SOURCE_DIR}/api/dis-udis86-randtest.raw" OFF)
  tobuild_api(api.ir api/ir.c "" "" OFF)
  if ("${CMAKE_GENERATOR}" MATCHES "Unix Makefiles")
    # CMake's Unix Makefiles dependence analysis doesn't run the preprocessor
    # and so doesn't find headers named via macros
    # (http://www.cmake.org/Bug/view.php?id=13718)
    set(api_ir_headers # must be full paths:
      ${CMAKE_CURRENT_SOURCE_DIR}/api/ir_0args.h
      ${CMAKE_CURRENT_SOURCE_DIR}/api/ir_1args.h
      ${CMAKE_CURRENT_SOURCE_DIR}/api/ir_2args.h
      ${CMAKE_CURRENT_SOURCE_DIR}/api/ir_2args_mm.h
      ${CMAKE_CURRENT_SOURCE_DIR}/api/ir_3args_avx.h
      ${CMAKE_CURRENT_SOURCE_DIR}/api/ir_3args.h
      ${CMAKE_CURRENT_SOURCE_DIR}/api/ir_4args.h)
    append_property_string(SOURCE api/ir.c OBJECT_DEPENDS "${api_ir_headers}")
  endif ()
  tobuild_api(api.startstop api/startstop.c "" "" OFF)
  # test static decoder library
  tobuild_api(api.ir-static api/ir.c "" "" ON)
  tobuild_api(api.static api/static.c "" "" ON)

  # XXX: we should expand this test of drsyms standalone to be cross-platform and
  # not just check libstdc++-6.dll.
  if (WIN32)
    find_program(mingwlib libstdc++-6.dll
      HINTS "c:/cygwin/usr/i686-pc-mingw32/sys-root/mingw/bin"
      DOC "path to MinGW libstdc++-6.dll")
    if (mingwlib)
      message(STATUS "Found ${mingwlib}, enabling api.symtest")
      tobuild_api(api.symtest api/symtest.c "" "${mingwlib}" OFF)
      use_DynamoRIO_extension(api.symtest drsyms)
    endif ()
  endif ()

  if (NOT X64)
    # i#696: Use -thread_private and small fcache units to trigger shifts.  x64
    # does not support fcache unit resizing and won't allow 4k unit sizes, so
    # skip it.
    tobuild_ci(client.fcache_shift client-interface/fcache_shift.c
      "" "-thread_private -cache_bb_unit_init 4K" "")
  endif ()

  tobuild_ci(client.nudge_ex client-interface/nudge_ex.c "" "" "")
  use_DynamoRIO_extension(client.nudge_ex.dll drmgr)

  tobuild_ci(client.retaddr client-interface/retaddr.c "" "" "")

  if (BUILD_SAMPLES)
    # Sanity tests: we run the samples without SHOW_RESULTS (turned off
    # if BUILD_TESTS is on) so we're only ensuring the client doesn't crash.
    # This requires that each sample does not affect stdout and works
    # when given no arguments.
    # We run common.eflags mainly for memtrace and other big clients which
    # are quite slow on common.fib or common.broadfun: just a sanity check
    # after all.
    get_property(sample_list GLOBAL PROPERTY DynamoRIO_sample_list)
    foreach (sample ${sample_list})
      torunonly_ci(sample.${sample} common.eflags ${sample} common/eflags.c "" "" "")
      if (sample STREQUAL "inscount")
        # test out-of-line clean call
        torunonly_ci(sample.${sample}.cleancall common.eflags ${sample}
          common/eflags.c "" "-opt_cleancall 0" "")
      endif ()
    endforeach ()
  endif (BUILD_SAMPLES)

  if (BUILD_CLIENTS)
    torunonly_ci(tool.drltrace common.eflags drltrace common/eflags.c
      "-only_from_app" "" "")
    set(tool.drltrace_runcmp "${PROJECT_SOURCE_DIR}/clients/drltrace/runtest.cmake")
  endif (BUILD_CLIENTS)

endif (CLIENT_INTERFACE)

if (UNIX)
  tobuild(linux.clone linux/clone.c)

  # Cross-arch execve test: can only be done via a suite of tests that
  # build both 32-bit and 64-bit.  We have 32-bit just build, and
  # 64-bit then builds and runs both directions.
  if (TEST_SUITE OR TEST_32BIT_PATH)
    if (X64)
      add_exe(linux.execve-sub64 linux/execve-sub.c)
      tobuild_ops(linux.execve64 linux/execve.c ""
        "${TEST_32BIT_PATH}/linux.execve-sub32")
      torunonly(linux.execve32 ${TEST_32BIT_PATH}/linux.execve32 linux/execve.c ""
        "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/linux.execve-sub64")
    else (X64)
      add_exe(linux.execve-sub32 linux/execve-sub.c)
      add_exe(linux.execve32 linux/execve.c)
    endif (X64)
  endif (TEST_SUITE OR TEST_32BIT_PATH)
    
  # helper app for execve-null
  add_exe(linux.execve-sub linux/execve-sub.c)
  tobuild_ops(linux.execve-null linux/execve-null.c ""
    "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/linux.execve-sub")

  tobuild(linux.execve-rec linux/execve-rec.c)
  tobuild(linux.exit linux/exit.c)
  tobuild(linux.fork linux/fork.c)
  tobuild(linux.infinite linux/infinite.c)
  tobuild(linux.longjmp linux/longjmp.c)
  tobuild(linux.prctl linux/prctl.c)
  # FIXME TOFILE: suddently seeing non-det curiosities/asserts in module list on mmap
  tobuild(linux.mmap linux/mmap.c)
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
  tobuild(linux.sigcontext linux/sigcontext.c)
  tobuild(linux.thread linux/thread.c)
  tobuild(linux.threadexit linux/threadexit.c)
  tobuild(linux.threadexit2 linux/threadexit2.c)
  tobuild(linux.signalfd linux/signalfd.c)
  # i#784: test app behavior on alarm
  tobuild(linux.alarm linux/alarm.c)

  # i#1145: test re-starting syscalls, both inlined and from dispatch
  tobuild(linux.eintr linux/eintr.c)
  target_link_libraries(linux.eintr ${libpthread})
  torunonly(linux.eintr-noinline linux.eintr linux/eintr.c "-no_ignore_syscalls" "")

  tobuild(linux.sigsuspend linux/sigsuspend.c)
  target_link_libraries(linux.sigsuspend ${libpthread})

  # We pass -native_exec_list which causes us to call strcasecmp in libc,
  # thereby exercising on our ability to copy libc's TLS data.
  tobuild_ops(linux.app_tls linux/app_tls.c "-native_exec_list libfoo.so" "")

  # Test fully static executables, which we only support with early injection
  # or STATIC_LIBRARY.
  set(fib_static_ops "-early_inject")
  if (STATIC_LIBRARY)
    set(fib_static_ops "")
  endif ()
  tobuild_ops(linux.fib-static common/fib.c "${fib_static_ops}" "")
  append_property_string(TARGET linux.fib-static LINK_FLAGS
    # The default value of CMAKE_SHARED_LIBRARY_LINK_C_FLAGS has -rdynamic,
    # which doesn't play well with clang and -static.  Adding
    # --no-export-dynamic avoids the problem.
    "-static -Wl,--no-export-dynamic")
  # Test position independent executables.
  # FIXME i#1001: fib-pie fails with -early_inject.
  tobuild(linux.fib-pie common/fib.c)
  append_property_string(TARGET linux.fib-pie COMPILE_FLAGS "-fPIE")
  append_property_string(TARGET linux.fib-pie LINK_FLAGS "-pie")

  # FIXME i#125: bugs in these, were disabled in the past
  tobuild_ops(linux.vfork linux/vfork.c ""
    "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/linux.execve-sub")
  #tobuild(linux.vfork-fib linux/vfork-fib.c)

  # runs of other builds with custom DR options
  torunonly(linux.thread-reset linux.thread linux/thread.c
    "-reset_at_fragment_count 100" "")
  torunonly(linux.clone-reset linux.clone linux/clone.c
    "-reset_at_fragment_count 100" "")

  tobuild(pthreads.pthreads pthreads/pthreads.c)
  tobuild(pthreads.pthreads_exit pthreads/pthreads_exit.c)
  tobuild(pthreads.ptsig_FLAKY pthreads/ptsig.c)
  tobuild(pthreads.pthreads_fork pthreads/pthreads_fork.c)

  # Clang will likely never support gcc nested functions:
  # http://llvm.org/PR9206
  if (NOT CMAKE_COMPILER_IS_CLANG)
    if (NOT X64)
      # FIXME i#16: these tests need to be fixed to compile for x64
      tobuild(security-linux.stacktest security-linux/stacktest.c)
      tochcon(security-linux.stacktest execmem_exec_t)
      mark_execstack(security-linux.stacktest)
    endif (NOT X64)
    tobuild(security-linux.trampoline security-linux/trampoline.c)
  endif (NOT CMAKE_COMPILER_IS_CLANG)

  # this app is used in nudge tests
  # XXX: should all these infloop tests be made cross-platform?
  # or rely on runall tests that run calc covering this on Windows?
  add_exe(linux.infloop linux/infloop.c)
  torunonly(linux.reset linux/infloop linux/reset.runall "" "-v")
  if (NOT X64)
    # XXX: 64-bit support not quite there yet
    torunonly(linux.freeze_FLAKY linux/infloop linux/freeze.runall
      "-coarse_units -coarse_enable_freeze" "-v")
  endif (NOT X64)

  # persistent cache nudge tests with shared cache
  file(MAKE_DIRECTORY "${PCACHE_DIR}")
  file(MAKE_DIRECTORY "${PCACHE_SHARED_DIR}")
  if (NOT X64)
    # XXX: 64-bit support not quite there yet
    torunonly(linux.persist_FLAKY linux/infloop linux/persist.runall
      "-coarse_units -coarse_split_calls -coarse_enable_freeze -coarse_freeze_min_size 0 -no_persist_per_user -no_validate_owner_dir" "-v")
    torunonly(linux.persist-use_FLAKY linux/infloop linux/persist-use.runall
      "-use_persisted -coarse_units -coarse_split_calls -no_persist_per_user -no_validate_owner_dir" "-v")
  endif (NOT X64)
  # when running tests in parallel: have to generate pcaches first
  set(linux.persist-use_FLAKY_depends linux.persist_FLAKY)
else (UNIX)
  if (VPS)
    # too flaky across platforms so we limit to VPS only: not too useful
    # for other than testing our ASLR anyway
    tobuild(win32.aslr-dll win32/aslr-dll.c)
  endif (VPS)
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
  if (TEST_SUITE) # fails non-det so run only in suite
    tobuild_dll(win32.reload_FLAKY win32/reload.c "")
  endif (TEST_SUITE)
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

  if (NOT X64)
    # FIXME i#16: these tests need to be fixed to compile for x64

    # fpe compiles, but pops up an unhandled exception box 
    tobuild(win32.fpe win32/fpe.c)

    tobuild(win32.except win32/except.c)
    tobuild(win32.getthreadcontext win32/getthreadcontext.c)
    tobuild_dll(win32.hookerfirst win32/hookerfirst.c
      "-handle_ntdll_modify 1 -native_exec_hook_conflict 1")
    tobuild(win32.hooker-secur32 win32/hooker-secur32.c)
    tobuild(win32.rsbtest win32/rsbtest.c)
    tobuild(win32.setcxtsyscall win32/setcxtsyscall.c)
    tobuild(win32.setthreadcontext win32/setthreadcontext.c)
    tobuild(win32.tls win32/tls.c)

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
  #if (CLIENT_INTERFACE)
  #  tobuild_runall(runall.calc-freeze runall/calc-freeze.runall
  #    # xref PR 226578 which tracks incompatibility between probe_api & pcaches
  #    "-coarse_enable_freeze -no_probe_api")
  #else (CLIENT_INTERFACE)
  #  tobuild_runall(runall.calc-freeze runall/calc-freeze.runall
  #    "-coarse_enable_freeze")
  #endif (CLIENT_INTERFACE)
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
  if (TEST_SUITE OR TEST_32BIT_PATH OR NOT X64)
    if (X64)
      torunonly(win32.mixedmode
        ${MAIN_RUNTIME_OUTPUT_DIRECTORY}/create_process.exe
        win32/mixedmode.c
        "-early_inject_map -early_inject_location 5 -heap_in_lower_4GB"
        "${TEST_32BIT_PATH}/win32.mixedmode.exe;mixedmode")
      torunonly(win32.x86_to_x64
        ${MAIN_RUNTIME_OUTPUT_DIRECTORY}/create_process.exe
        win32/mixedmode.c
        "-early_inject_map -early_inject_location 5 -heap_in_lower_4GB -x86_to_x64"
        "${TEST_32BIT_PATH}/win32.mixedmode.exe;x86_to_x64")
    else (X64)
      add_exe(win32.mixedmode win32/mixedmode.c)
    endif (X64)
  endif (TEST_SUITE OR TEST_32BIT_PATH OR NOT X64)
   
endif (UNIX)

tobuild(security-common.codemod security-common/codemod.c)
tochcon(security-common.codemod execmem_exec_t)
mark_execstack(security-common.codemod)
tobuild(security-common.decode-bad-stack security-common/decode-bad-stack.c)
tochcon(security-common.decode-bad-stack execmem_exec_t)
mark_execstack(security-common.decode-bad-stack)
if (VPS) # relies on being aborted on .B violation
  tobuild(security-common.jmp_from_trace security-common/jmp_from_trace.c)
  tochcon(security-common.jmp_from_trace textrel_shlib_t)
endif (VPS)
tobuild(security-common.retexisting security-common/retexisting.c)
tobuild(security-common.ret_noncall_trace security-common/ret_noncall_trace.c)
tobuild(security-common.retnonexisting security-common/retnonexisting.c)
tobuild(security-common.selfmod2 security-common/selfmod2.c)
tochcon(security-common.selfmod2 textrel_shlib_t)
tobuild(security-common.selfmod-big security-common/selfmod-big.c)
tochcon(security-common.selfmod-big textrel_shlib_t)
if (NOT X64)
  # FIXME i#125
  tobuild(security-common.vbjmp-rac-test security-common/vbjmp-rac-test.c)
endif (NOT X64)
tobuild(security-common.selfmod security-common/selfmod.c)
tochcon(security-common.selfmod textrel_shlib_t)
if (X64)
  # Reachability tests

  # Floating DR lib.  We'd have to build a 2nd dynamorio.dll on Windows so
  # we only test on Linux for now.
  if (UNIX)
    torunonly(float_DR security-common.selfmod security-common/selfmod.c "" "")
    set(float_DR_env "LD_USE_LOAD_BIAS=0")
    if (CLIENT_INTERFACE)
      torunonly_ci(client.float_DR security-common.selfmod
        client.dr_options.dll security-common/selfmod.c
        # I picked dr_options b/c it has no output, but it requires all these ops:
        "" "-native_exec_list foo.dll,bar.dll -opt_cleancall 3 -thread_private" "")
      set(client.float_DR_env "LD_USE_LOAD_BIAS=0")
    endif ()
  endif (UNIX)

  # Floating vmbase.  Risky to just pick this hardcoded address: any better ideas?
  torunonly(float_vmbase security-common.selfmod security-common/selfmod.c
    "-vm_base 0x30000000000 -no_vm_base_near_app" "")
  if (CLIENT_INTERFACE)
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
  endif (CLIENT_INTERFACE)

endif (X64)

tobuild("security-common.TestAllocWE" security-common/TestAllocWE.c)

set (TestMemProtChg "security-common.TestMemProtChg")
if (UNIX)  # i#387: Still has issues with proc maps on some kernels.
  set (TestMemProtChg "${TestMemProtChg}_FLAKY")
endif (UNIX)
tobuild("${TestMemProtChg}" security-common/TestMemProtChg.c)
tochcon("${TestMemProtChg}" wine_exec_t)
mark_execstack("${TestMemProtChg}")

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
