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
option(TEST_SUITE "running series of builds so skip some overlapping tests")
set(TEST_SECONDS "60" CACHE STRING "Test time limit in seconds")
set(TEST_OPTIONS "" CACHE STRING "Extra DynamoRIO options when running tests")

# FIXME: run samples like runregression does
# FIXME: .runall support
# FIXME: xref issue 20 where we build x64 and x86 to have both simultaneously
#   for cross-arch execve.
# FIXME: we need x86 drdeploy to run x64 Windows runall tests
# FIXME: add a VMKERNEL=ON build to the runregression list
#   but need bora headers for that so have to run at vmware => nightly
# FIXME: add command to run natively
# FIXME: update TestSuite.wiki
# FIXME: build latest cmake for "ctest -W 60" and "ctest -j"
# FIXME: can we get .runall tests to run in parallel:
#   rename calc.exe, use drkill instead of closewnd?

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

set(vmap_run_list
  # our main configuration
  "SHORT::-code_api"
  # sanity check: run single app to make sure these options aren't totally broken
  "SHORT::ONLY::client.events::-code_api -opt_memory"
  "SHORT::ONLY::client.events::-code_api -thread_private"
  "SHORT::ONLY::client.events::-code_api -disable_traces"
  "SHORT::ONLY::client.events::-code_api -thread_private -disable_traces"
  # maybe this should be SHORT as -coarse_units will eventually be the default?
  "-code_api -opt_memory"
  "-code_api -thread_private"
  "-code_api -disable_traces"
  "-code_api -thread_private -disable_traces"
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
  # make sure we at least sometimes exercise -checklevel > 0
  "DEBUG::-checklevel 2"

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
  #"WIN::ONLY::^runall::-thin_client",
  "WIN::ONLY::^runall::-probe_api -hotp_only",
  # our supported configurations
  "ONLY::^[^h]::-code_api -probe_api",
  "ONLY::^security ^runall runinterface::-security_api",
  "ONLY::^[^h]::-security_api",
  "WIN::ONLY::^(runall|${osname})::-probe_api -hotp_only",
  "WIN::ONLY::^(common|security)::-probe_api -hotp_only",
  # other configurations we may eventually support
  "ONLY::^[^h]::-security_api -code_api",
  "-security_api -probe_api",
  "-security_api -code_api -probe_api",
  )

# xref the very long set of runs we used to test in
# suite/old-runregression-runs-2-6-08
set(vps_run_list
  "ONLY::^(common|${osname}|security|runall)::",
  "ONLY::^(common|${osname}|security|runall)::-security",
  "ONLY::^security::-security -detect_mode -report_max 0 -no_diagnostics",
  "WIN::ONLY::^(common|${osname}|security|runall)::-security -liveshields",
  "WIN::ONLY::^runall ^security::-security -client",
  "WIN::ONLY::^runall ^security::-security -low", # not a default config
  # Test with -use_moduledb relaxations, (is on by default, but tests Makefile
  # adds -staged which turns it off, we re-enable by turning the option back on).
  # The tests Makefile adds different version resource information to different
  # tests in runall/ and security-{common,win32}/ to fully exercise this.
  "WIN::ONLY::^(runall|security)::-security -use_moduledb",
  )

if (VMAP)
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
        "WIN::ONLY::^(common|${osname}|security|runall)::-security -liveshields",
        "WIN::ONLY::^(runall|security)::-security -client",
        )
    endif (DEBUG AND INTERNAL)
  else (TEST_SUITE)
    set(run_list ${vps_run_list})
  endif (TEST_SUITE)
endif (VMAP)

###########################################################################
# BUILDING

# put libs in same dir for tests that need to load them
set(MAIN_EXECUTABLE_OUTPUT_PATH "${EXECUTABLE_OUTPUT_PATH}")
set(MAIN_LIBRARY_OUTPUT_PATH "${LIBRARY_OUTPUT_PATH}")
set(LIBRARY_OUTPUT_PATH "${CMAKE_CURRENT_BINARY_DIR}/bin")
set(EXECUTABLE_OUTPUT_PATH "${CMAKE_CURRENT_BINARY_DIR}/bin")

include_directories(${CMAKE_CURRENT_SOURCE_DIR}) # tools.h
include_directories(${PROJECT_SOURCE_DIR}/core/x86) # asm_defines.asm

# To avoid having separate subdirs with separate CMakeLists.txt files,
# we use DynamoRIOUse.cmake here.  It makes global changes, though.
# We live with those changes on all our tests even though we don't
# need them everywhere for:
# - add_definitions: we remove, since using configure.h
# - include_directories
# - link_directories
# We isolate these:
# - CMAKE_C_FLAGS_${CMAKE_BUILD_TYPE_UPPER}
# - CMAKE_SHARED_LINKER_FLAGS

if (UNIX)
  if (X64)
    set(ARCH_CFLAGS "-m64")
  else (X64)
    set(ARCH_CFLAGS "-m32")
  endif (X64)
endif (UNIX)

string(TOUPPER "${CMAKE_BUILD_TYPE}" CMAKE_BUILD_TYPE_UPPER)
set(ORIG_CMAKE_C_FLAGS
  "${CMAKE_C_FLAGS} ${CMAKE_C_FLAGS_${CMAKE_BUILD_TYPE_UPPER}} ${ARCH_CFLAGS}")
set(ORIG_CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} ${ARCH_CFLAGS}")

if (NOT DEFINED DynamoRIO_DIR)
  set(DynamoRIO_DIR "${PROJECT_SOURCE_DIR}/../cmake" CACHE PATH
    "DynamoRIO installation's cmake directory")
endif (NOT DEFINED DynamoRIO_DIR)
find_package(DynamoRIO)
if (DynamoRIO_FOUND)
  set(DynamoRIO_X64 ${X64})
  if ("${CMAKE_BUILD_TYPE}" STREQUAL "Debug")
    set(DynamoRIO_DEBUG ON)
  else ("${CMAKE_BUILD_TYPE}" STREQUAL "Debug")
    set(DynamoRIO_DEBUG OFF)
  endif ("${CMAKE_BUILD_TYPE}" STREQUAL "Debug")
  set(DynamoRIO_CXX OFF)
  include(${DynamoRIO_USE_FILE})
endif(DynamoRIO_FOUND)

set(CLIENT_CMAKE_C_FLAGS "${CMAKE_C_FLAGS_${CMAKE_BUILD_TYPE_UPPER}} ${ARCH_CFLAGS}")
set(CLIENT_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} ${ARCH_CFLAGS}")

# We clear the absolute flags so we can use PROPERTIES to set for each test
set(CMAKE_C_FLAGS_${CMAKE_BUILD_TYPE_UPPER} "")
set(CMAKE_C_FLAGS "")
set(CMAKE_SHARED_LINKER_FLAGS "")

# we're not assuming gcc >= 3.4 in our header files
string(REGEX REPLACE "-fvisibility=internal" "" CLIENT_CMAKE_C_FLAGS
  "${CLIENT_CMAKE_C_FLAGS}")

# FIXME i#98: improve code so can set Wall/W4
if (UNIX)
  string(REGEX REPLACE "-Wall" "" ORIG_CMAKE_C_FLAGS "${ORIG_CMAKE_C_FLAGS}")
  string(REGEX REPLACE "-std=gnu99" "" ORIG_CMAKE_C_FLAGS "${ORIG_CMAKE_C_FLAGS}")
  find_library(libmath m)
  find_library(libdl dl)
  find_library(libpthread pthread)
  if (libmath-NOTFOUND OR libdl-NOTFOUND OR libpthread-NOTFOUND)
    message(FATAL_ERROR "cannot find required libs m, dl, and/or pthread")
  endif (libmath-NOTFOUND OR libdl-NOTFOUND OR libpthread-NOTFOUND)
else (UNIX)
  # W2 is default (we're using W3).  We should also replace
  # all references to unsafe functions (e.g., fopen) and
  # remove /wd4996
  string(REGEX REPLACE "/W4" "/W3" ORIG_CMAKE_C_FLAGS "${ORIG_CMAKE_C_FLAGS}")
endif (UNIX)

# since we're including configure.h we do not want any default defines
remove_definitions(-DX64 -DLINUX -DWINDOWS)
set(platform_defs "")
if (X64)
  set(platform_defs "${platform_defs} -DX64")
endif (X64)
if (UNIX)
  set(platform_defs "${platform_defs} -DLINUX")
else (UNIX)
  set(platform_defs "${platform_defs} -DWINDOWS")
endif (UNIX)

##################################################

# some tests need to link with tools.c and its embedded asm code
set(tools_c ${CMAKE_CURRENT_SOURCE_DIR}/tools.c)
set(tools_asm ${CMAKE_CURRENT_BINARY_DIR}/tools_asm.asm)
set_source_files_properties(${tools_c} PROPERTIES
  COMPILE_FLAGS "${ORIG_CMAKE_C_FLAGS}")
set_source_files_properties(${tools_asm} PROPERTIES
  GENERATED ON COMPILE_FLAGS "-DASM_CODE_ONLY")
add_custom_command(
  OUTPUT ${tools_asm}
  DEPENDS ${tools_c}
  COMMAND ${CMAKE_COMMAND}
  ARGS -E copy ${tools_c} ${tools_asm}
  VERBATIM # recommended: p260
  )

function(add_exe test source)
  get_filename_component(srcbase ${source} NAME_WE)
  get_filename_component(srcpath ${source} PATH)

  set(test_srcs ${source} ${tools_c} ${tools_asm})

  # FIXME: add mresources.res and oresources.res to windows apps

  # We can't set the target properties COMPILE_FLAGS as that will
  # pass -g3 to cpp which ends up not expanding macros!
  # So we set properties on source files only.
  file(READ ${CMAKE_CURRENT_SOURCE_DIR}/${source} srccode)
  # We support a test either getting defines from configure.h or
  # needing them passed in on cmd line.
  if ("${srccode}" MATCHES "tools\\.h" OR
      "${srccode}" MATCHES "configure\\.h")
    # getting defines from configure.h
    set_source_files_properties(${source} PROPERTIES
      COMPILE_FLAGS "${ORIG_CMAKE_C_FLAGS}")
  else ()
    set_source_files_properties(${source} PROPERTIES
      COMPILE_FLAGS "${platform_defs} ${ORIG_CMAKE_C_FLAGS}")
  endif ()

  # Some files use asm code, which must be separate for x64, but it's much
  # more convenient to have it in the same source file and auto-split.
  if ("${srccode}" MATCHES "ifndef ASM_CODE_ONLY")
    # we rely on the asm rule doing preprocessing for us, so we just make
    # a copy and set the ASM_CODE_ONLY define
    set(asm_source ${CMAKE_CURRENT_BINARY_DIR}/${srcpath}/${srcbase}_asm.asm)
    set(test_srcs ${test_srcs} ${asm_source})
    set_source_files_properties(${asm_source} PROPERTIES
      GENERATED ON COMPILE_FLAGS "-DASM_CODE_ONLY")
    add_custom_command(
      OUTPUT ${asm_source}
      DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/${source}
      COMMAND ${CMAKE_COMMAND}
      ARGS -E copy ${CMAKE_CURRENT_SOURCE_DIR}/${source} ${asm_source}
      VERBATIM # recommended: p260
      )
  endif ("${srccode}" MATCHES "ifndef ASM_CODE_ONLY")

  add_executable(${test} ${test_srcs})
  set_target_properties(${test} PROPERTIES
    LINK_FLAGS "${ARCH_CFLAGS}")
  if (UNIX)
    target_link_libraries(${test} ${libmath} ${libdl})
    if ("${test}" MATCHES "^pthread")
      target_link_libraries(${test} ${libpthread})
    endif ("${test}" MATCHES "^pthread")
  endif (UNIX)
endfunction(add_exe)

# normal app
function(tobuild test source)
  add_exe(${test} ${source})
  set(testlist_normal ${testlist_normal} ${test} PARENT_SCOPE)
  set(${test}_source ${source} PARENT_SCOPE)
endfunction(tobuild)

# normal app that has a dll and an executable
function(tobuild_dll test source dr_ops)
  string(REGEX REPLACE "\\.c$" ".dll.c" dll_source "${source}")

  add_exe(${test} ${source})

  add_library(${test}.dll SHARED ${dll_source})
  set_target_properties(${test}.dll PROPERTIES
    COMPILE_FLAGS "${ORIG_CMAKE_C_FLAGS} ${platform_defs}"
    LINK_FLAGS "${ORIG_SHARED_LINKER_FLAGS}")
  target_link_libraries(${test} ${test}.dll)

  set(testlist_normal ${testlist_normal} ${test} PARENT_SCOPE)
  set(${test}_source ${source} PARENT_SCOPE)
  set(${test}_dr_ops ${dr_ops} PARENT_SCOPE)
endfunction(tobuild_dll)

# client interface app with client
function(tobuild_ci test source client_ops dr_ops)
  string(REGEX REPLACE "\\.c$" ".dll.c" client_source "${source}")

  add_exe(${test} ${source})
  add_dependencies(${test} api_headers dynamorio)

  add_library(${test}.dll SHARED ${client_source})
  set_target_properties(${test}.dll PROPERTIES
    COMPILE_FLAGS "${CLIENT_CMAKE_C_FLAGS} ${platform_defs}"
    LINK_FLAGS "${CLIENT_SHARED_LINKER_FLAGS}")
  target_link_libraries(${test}.dll dynamorio)
  add_dependencies(${test}.dll api_headers dynamorio)
  get_target_property(client_path ${test}.dll LOCATION)

  set(testlist_ci ${testlist_ci} ${test} PARENT_SCOPE)
  set(${test}_source ${source} PARENT_SCOPE)
  set(${test}_client_ops ${client_ops} PARENT_SCOPE)
  set(${test}_dr_ops ${dr_ops} PARENT_SCOPE)
  set(${test}_client_path ${client_path} PARENT_SCOPE)
endfunction(tobuild_ci)

# client interface standalone app
function(tobuild_api test source dr_ops exe_ops)
  add_exe(${test} ${source})
  set_target_properties(${test} PROPERTIES
    COMPILE_FLAGS "${CLIENT_CMAKE_C_FLAGS} -DUSE_DYNAMO")
  target_link_libraries(${test} dynamorio)
  add_dependencies(${test} api_headers dynamorio)
  set(testlist_api ${testlist_api} ${test} PARENT_SCOPE)
  set(${test}_source ${source} PARENT_SCOPE)
  set(${test}_dr_ops ${dr_ops} PARENT_SCOPE)
  set(${test}_exe_ops ${exe_ops} PARENT_SCOPE)
endfunction(tobuild_api)

# run that uses a different build
function(torunonly test realtest source dr_ops exe_ops)
  set(testlist_normal ${testlist_normal} ${test} PARENT_SCOPE)
  set(${test}_realtest ${realtest} PARENT_SCOPE)
  set(${test}_source ${source} PARENT_SCOPE)
  set(${test}_dr_ops ${dr_ops} PARENT_SCOPE)
  set(${test}_exe_ops ${exe_ops} PARENT_SCOPE)
endfunction(torunonly)

###########################################################################
# RUNNING

function(template2expect outexpect template runops)
  # We used to use a custom perl script to match defines and runtime
  # options but we've switched to cpp.  We convert -foo to -Dfoo and
  # -foo value to -Dfoo___value.
  # We replace bad define-name chars in client_lib with _.
  string(REGEX REPLACE
    "[;/\\.]"
    "_" rundefs "${runops}")
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
  
  # FIXME i#65 NOT YET COMPLETED: RUNREGRESSION_winversion like
  # gen_expect.pl does

  string(STRIP "${rundefs}" rundefs)

  # relies on ${defines} from top level
  # we need a list: else passed as a single arg in quotes
  string(REGEX REPLACE " " ";" deflist "${defines} ${rundefs}")
  execute_process(COMMAND
    ${CMAKE_CPP} ${CMAKE_CPP_FLAGS} ${deflist} -E ${CPP_NO_LINENUM} ${template}
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

  # remove blank lines left by cpp directives
  string(REGEX REPLACE
    "(\r?\n)+"
    "\\1" cpp_out "${cpp_out}")
  string(REGEX REPLACE
    "^(\r?\n)+"
    "" cpp_out "${cpp_out}")
  # allow matching blank lines via @@
  # FIXME i#65 NOT YET COMPLETED: haven't modified any .template files for this
  #  string(REGEX REPLACE
  #    "@@"
  #    "\n" cpp_out "${cpp_out}")

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
  string(REGEX REPLACE "\n *SEC_VIO_CONT\n"        "\n${msg_vio}${msg_cont}>\n"
    cpp_out "${cpp_out}")
  string(REGEX REPLACE "\n *SEC_VIO_STOP\n"        "\n${msg_vio}${msg_term}>\n"
    cpp_out "${cpp_out}")
  string(REGEX REPLACE "\n *SEC_VIO_EXCEPTION\n"   "\n${msg_vio}${msg_throw}>\n"
    cpp_out "${cpp_out}")
  string(REGEX REPLACE "\n *SEC_VIO_THREAD\n"      "\n${msg_vio}${msg_thread}>\n"
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

function(rundr_cmd outcmd outops native dr_ops_aux)
  # assumes tools have been built: enforced at top level
  if (UNIX)
    set(dr_ops "-stderr_mask 0xC -dumpcore_mask 0 ${dr_ops_aux} ${TEST_OPTIONS}")
    # Since the creation of CTestTestfile.cmake ends up replacing \ with / we
    # can't escape quotes, so we have to use a list variable
    set(cmd ${MAIN_EXECUTABLE_OUTPUT_PATH}/runstats -s ${TEST_SECONDS} -silent)
    if (NOT native)
      set(cmd ${cmd}
        -env LD_LIBRARY_PATH "${MAIN_LIBRARY_OUTPUT_PATH}:$ENV{LD_LIBRARY_PATH}"
        -env LD_PRELOAD "libdrpreload.so libdynamorio.so")
    endif (NOT native)
    # add dr_ops last to avoid needing extra \ on ;
    set(${outcmd} ${cmd} -env DYNAMORIO_OPTIONS "${dr_ops}" PARENT_SCOPE)
  else (UNIX)
    # FIXME i#65 NOT YET COMPLETED: need internal drinject: have separate
    # target drinjectex.exe?  would also help avoid confusion w/ drmemory.sh, etc.
    set(dr_ops "-stderr_mask 0xC -msgbox_mask 0 -dumpcore_mask 0xfd -staged ${dr_ops_aux} ${TEST_OPTIONS}")
  endif (UNIX)
  set(${outops} "${dr_ops}" PARENT_SCOPE)
endfunction(rundr_cmd)

# if test != exe we assume exe is already added as a build target
function(torun test exe source native dr_ops exe_ops)
  rundr_cmd(rundr runops ${native} "${dr_ops}")
  add_test(${test} ${rundr} ${EXECUTABLE_OUTPUT_PATH}/${exe} ${exe_ops})

  get_filename_component(srcbase ${source} NAME_WE)
  get_filename_component(srcpath ${source} PATH)
  if (EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${srcpath}/${srcbase}.expect)
    file(READ ${CMAKE_CURRENT_SOURCE_DIR}/${srcpath}/${srcbase}.expect expect)
  elseif (EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${srcpath}/${srcbase}.template)
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
    # * Changes in .template don't require reconfiguring + rebuilding
    #   (FIXME: is there a way to say "CTestTestfile.cmake depends on .template"?)
    # * We could support extra runtime options via env var DYNAMORIO_OPTIONS,
    #   but while that might be useful it could also be confusing.
    template2expect(expect
      ${CMAKE_CURRENT_SOURCE_DIR}/${srcpath}/${srcbase}.template
      ${runops})
  else (EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${srcpath}/${srcbase}.expect)
    message(FATAL_ERROR "no .expect or .template for ${srcpath}/${srcbase}")
  endif (EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${srcpath}/${srcbase}.expect)

  # FIXME i#65 NOT YET COMPLETED: api/dis output is too long => ctest
  # runs out of memory
  # => need to have a script to run the app and then diff the results,
  # since ctest supports only one command per test.
  # We can use the same script to get runtime generation of .expect.

  # turn regex chars into literals
  # \\] somehow messes up the match though $expect looks identical
  # some tests do have ']' so we replace it in a separate step below
  string(REGEX REPLACE "([\\^\\$\\.\\*\\+\\?\\|\\(\\)\\[])" 
    "\\\\\\1" expect "${expect}")
  string(REGEX REPLACE "\\]"
    "\\\\]" expect "${expect}")
  # match whole output
  set(expect "^${expect}$")

  set_tests_properties(${test} PROPERTIES PASS_REGULAR_EXPRESSION "${expect}")
endfunction(torun)

function(torun_normal name test ops)
  if (DEFINED ${test}_realtest)
    set(exe ${${test}_realtest})
  else (DEFINED ${test}_realtest)
    set(exe ${test})
  endif (DEFINED ${test}_realtest)
  torun("${name}" ${exe} ${${test}_source} OFF "${${test}_dr_ops} ${ops}" "")
endfunction(torun_normal)

function(torun_ci name test ops)
  set(cli_ops "${${test}_client_ops}")
  if ("${test}" MATCHES "client\\.flush")
    # See PR 244090 - currently there is no way for a client to see what
    # options DR was run with.  The flush tests needs to know whether it
    # can use the unlink flush routines or not which depends on having
    # either -thread_private or -enable_full_api We check here and pass
    # a client option if the unlink flush is safe to use.
    if ("${ops}" MATCHES "(-thread_private|-enable_full_api)")
      set(cli_ops "use_unlink")
    endif ("${ops}" MATCHES "(-thread_private|-enable_full_api)")
  endif ("${test}" MATCHES "client\\.flush")
  torun("${name}" ${test} ${${test}_source} OFF
    " -code_api -client_lib ${${test}_client_path}\;0\;${cli_ops} ${${test}_dr_ops} ${ops}" "")
endfunction(torun_ci)

function(torun_api name test ops)
  torun("${name}" ${test} ${${test}_source} ON
    "${${test}_dr_ops} ${ops}" "${${test}_exe_ops}")
endfunction(torun_api)

function(torun_runall name test ops)
  # FIXME NYI i#65
endfunction(torun_runall)

###########################################################################
# tests

tobuild(common.broadfun common/broadfun.c)
tobuild(common.decode-bad common/decode-bad.c)
tobuild(common.decode common/decode.c)
# We build all tests before running so decode will be built before
# ctest -j ever ran this.  If we switch to --build-and-test then we'll
# make these runonly tests make sure their exe is built.
torunonly(common.decode-stress common.decode common/decode.c
  "-stress_recreate_state" "")
tobuild(common.eflags common/eflags.c)
tobuild(common.fib common/fib.c)
tobuild(common.getretaddr common/getretaddr.c)
tobuild(common.protect-dstack common/protect-dstack.c)
tobuild(common.segfault common/segfault.c)
# PR 217255: these 4 removed to shorten the regression suite
#tobuild(common.hello common/hello.c)
#tobuild(common.conflict common/conflict.c)
#tobuild(common.ops common/ops.c)
#tobuild(common.recurse common/recurse.c)

tobuild_ci(client.abort client-interface/abort.c "" "")
tobuild_ci(client.alloc client-interface/alloc.c "" "")
tobuild_ci(client.call-retarget client-interface/call-retarget.c "" "")
tobuild_ci(client.cleancall client-interface/cleancall.c "" "")
tobuild_ci(client.count-ctis client-interface/count-ctis.c "" "")
if (UNIX)
  # just happens to not make as many traces
  tobuild_ci(client.events client-interface/events.c "" "-trace_threshold 5")
  tobuild_ci(client.signal client-interface/signal.c "" "")
  tobuild_ci(client.syscall-mod client-interface/syscall-mod.c "" "")
else (UNIX)
  tobuild_ci(client.events client-interface/events.c "" "")
  tobuild_ci(client.cbr3 client-interface/cbr3.c "" "")
  tobuild_ci(client.cbr4 client-interface/cbr4.c "" "-thread_private")
  tobuild_ci(client.modules client-interface/modules.c "" "")
  tobuild_ci(client.syscall client-interface/syscall.c "" "")
  # FIXME: enable these .runall tests once have .runall support
  #tobuild_ci_runall(client.cbr2 client-interface/cbr2.runall "" "")
  #tobuild_ci_runall(client.cbr client-interface/cbr.runall "" "")
  #tobuild_ci_runall(client.custom_traces client-interface/custom_traces.runall "" "")
  #tobuild_ci_runall(client.decode-bb client-interface/decode-bb.runall "" "")
  #tobuild_ci_runall(client.nudge_test client-interface/nudge_test.runall "" "")
  #tobuild_ci_runall(client.pc-check client-interface/pc-check.runall "" "")
endif (UNIX)
tobuild_ci(client.file_io client-interface/file_io.c
  "${CMAKE_CURRENT_SOURCE_DIR}/client-interface/file_io_data.txt" "")
# we add custom option to flush test based on dr ops in torun_ci()
tobuild_ci(client.flush client-interface/flush.c "" "")
# FIXME: PR 199115 to re-enable fragdel, get some more of the LINUX tests working
#tobuild_ci(client.fragdel client-interface/fragdel.c "" "")
if (PROGRAM_SHEPHERDING)
  tobuild_ci(client.security client-interface/security.c "" "-security_api")
endif (PROGRAM_SHEPHERDING)
tobuild_ci(client.strace client-interface/strace.c "" "")
# FIXME: quotes are being removed so test gets paramx and DR gets paramy,
# and paramy is defined to be 1 by the cpp .template step!
#tobuild_ci(client.thread client-interface/thread.c "\\\"-paramx -paramy\\\"" "")
tobuild_ci(client.truncate client-interface/truncate.c "" "")
tobuild_ci(client.unregister client-interface/unregister.c "" "")

# FIXME i#65 NOT YET COMPLETED: enable once resolve out-of-memory issues
#tobuild_api(api.dis api/dis.c "-syntax_intel"
#  "${CMAKE_CURRENT_SOURCE_DIR}/api/dis-udis86-randtest.raw")
tobuild_api(api.ir api/ir.c "" "")
tobuild_api(api.startstop api/startstop.c "" "")

if (UNIX)
  tobuild(linux.clone linux/clone.c)
  # FIXME: execve => execve{32,64} and runs execve-sub
  # has custom args as well
  #tobuild(linux.execve linux/execve.c)
  tobuild(linux.execve-null linux/execve-null.c)
  tobuild(linux.execve-rec linux/execve-rec.c)
  tobuild(linux.exit linux/exit.c)
  tobuild(linux.fork linux/fork.c)
  tobuild(linux.infinite linux/infinite.c)
  tobuild(linux.longjmp linux/longjmp.c)
  # FIXME TOFILE: suddently getting (non-det) curiosities and asserts in module list
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
  tobuild(linux.thread linux/thread.c)
  tobuild(linux.threadexit linux/threadexit.c)
  tobuild(linux.threadexit2 linux/threadexit2.c)
  # FIXME: bugs in these, were disabled in the past
  #tobuild(linux.vfork linux/vfork.c)
  #tobuild(linux.vfork-fib linux/vfork-fib.c)

  # runs of other builds with custom DR options
  torunonly(linux.thread-reset linux.thread linux/thread.c
    "-reset_at_fragment_count 100" "")
  torunonly(linux.clone-reset linux.clone linux/clone.c
    "-reset_at_fragment_count 100" "")

  tobuild(pthreads.pthreads pthreads/pthreads.c)
  tobuild(pthreads.ptsig pthreads/ptsig.c)

  if (NOT X64)
    # FIXME i#16: these tests need to be fixed to compile for x64
    tobuild(security-linux.stacktest security-linux/stacktest.c)
  endif (NOT X64)
  tobuild(security-linux.trampoline security-linux/trampoline.c)
else (UNIX)
  # FIXME: need these custom build settings:
  #win32/delaybind.runnative win32/delaybind.runinjector: LINK += /delayload:delaybind.dll.dll /delay:unload
  #win32/delaybind.runnative win32/delaybind.runinjector: win32/delaybind.exe.bind win32/delaybind.dll.dll.bind
  #security-win32/sec-fixed.dll.dll: LINK += /FIXED
  #security-win32/secalign-fixed.dll.dll: LINK += /FIXED
  ## PR 229292: we want over-ssh and local builds to match the template so
  ## we always build w/o pdbs
  #security-win32/except-execution.exe: DBG=
  #win32/membar.exe: CC_BASE += $(OPT) # need optimizations

  tobuild(win32.aslr-dll win32/aslr-dll.c)
  tobuild(win32.callback win32/callback.c)
  tobuild(win32.crtprcs win32/crtprcs.c)
  tobuild(win32.delaybind win32/delaybind.c)
  tobuild_dll(win32.dll win32/dll.c "")
  tobuild(win32.except win32/except.c)
  tobuild(win32.fiber-rac win32/fiber-rac.c)
  tobuild(win32.finally win32/finally.c)
  tobuild(win32.fpe win32/fpe.c)
  tobuild(win32.getthreadcontext win32/getthreadcontext.c)
  tobuild_dll(win32.hookerfirst win32/hookerfirst.c)
  tobuild(win32.hooker-secur32 win32/hooker-secur32.c)
  tobuild_dll(win32.multisec win32/multisec.c)
  tobuild_dll(win32.nativeexec win32/nativeexec.c
    "-native_exec_list nativeexec.dll.dll")
  tobuild_dll(win32.nativeterminate win32/nativeterminate.c
    "-native_exec_list nativeterminate.dll.dll")
  tobuild(win32.oomtest win32/oomtest.c)
  tobuild(win32.partial_map win32/partial_map.c)
  tobuild(win32.protect-datasec win32/protect-datasec.c)
  tobuild_dll(win32.rebased win32/rebased.c "")
  tobuild_dll(win32.reload win32/reload.c "")
  tobuild_dll(win32.reload-newaddr win32/reload-newaddr.c "")
  tobuild_dll(win32.reload-race win32/reload-race.c "")
  tobuild(win32.rsbtest win32/rsbtest.c)
  tobuild_dll(win32.section-max win32/section-max.c "")
  tobuild(win32.setcxtsyscall win32/setcxtsyscall.c)
  tobuild(win32.setthreadcontext win32/setthreadcontext.c)
  tobuild(win32.suspend win32/suspend.c)
  tobuild(win32.threadchurn win32/threadchurn.c)
  tobuild(win32.threadexit win32/threadexit.c)
  # PR 215238 covers re-enabling threadinjection
  #tobuild(win32.threadinjection win32/threadinjection.c)
  #tobuild(win32.debugger win32/debugger.c)
  tobuild(win32.tls win32/tls.c)
  tobuild(win32.virtualfree win32/virtualfree.c)
  tobuild(win32.virtualreserve win32/virtualreserve.c)
  tobuild(win32.winapc win32/winapc.c)
  tobuild(win32.winthread win32/winthread.c)

  tobuild(security-win32.apc-shellcode security-win32/apc-shellcode.c)
  tobuild_dll(security-win32.aslr-ind security-win32/aslr-ind.c "")
  tobuild(security-win32.codemod-threads security-win32/codemod-threads.c)
  tobuild(security-win32.except-execution security-win32/except-execution.c)
  tobuild(security-win32.except-thread security-win32/except-thread.c)
  tobuild(security-win32.gbop-test security-win32/gbop-test.c)
  tobuild(security-win32.hooker security-win32/hooker.c)
  tobuild(security-win32.hooker-ntdll security-win32/hooker-ntdll.c)
  tobuild(security-win32.indexisting security-win32/indexisting.c)
  tobuild(security-win32.patterns security-win32/patterns.c)
  tobuild(security-win32.ret-SEH security-win32/ret-SEH.c)
  tobuild(security-win32.sd_tester security-win32/sd_tester.c)
  tobuild(security-win32.sec-adata security-win32/sec-adata.c)
  tobuild_dll(security-win32.secalign-fixed security-win32/secalign-fixed.c "")
  tobuild_dll(security-win32.sec-fixed security-win32/sec-fixed.c "")
  tobuild_dll(security-win32.sec-xdata security-win32/sec-xdata.c "")
  # case 4757 covers re-enabling selfmod-threads
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
endif (UNIX)

tobuild(security-common.codemod security-common/codemod.c)
tobuild(security-common.decode-bad-stack security-common/decode-bad-stack.c)
tobuild(security-common.jmp_from_trace security-common/jmp_from_trace.c)
tobuild(security-common.retexisting security-common/retexisting.c)
tobuild(security-common.ret_noncall_trace security-common/ret_noncall_trace.c)
tobuild(security-common.retnonexisting security-common/retnonexisting.c)
if (NOT X64)
  # FIXME i#16: these tests need to be fixed to compile for x64
  tobuild(security-common.selfmod2 security-common/selfmod2.c)
  # FIXME: not building on 32-bit b/c protect_mem() from tools.h being stripped
  #tobuild(security-common.selfmod-big security-common/selfmod-big.c)
  tobuild(security-common.vbjmp-rac-test security-common/vbjmp-rac-test.c)
endif (NOT X64)
tobuild(security-common.selfmod security-common/selfmod.c)
tobuild(security-common.TestAllocWE security-common/TestAllocWE.c)
tobuild(security-common.TestMemProtChg security-common/TestMemProtChg.c)

###########################################################################

# We'll want the latest CTest (2.6.4) so we can use the -W parameter
# for wider test name fields as ours include the runtime options and
# app name.

function(run2testname testname ops test)
  string(REGEX REPLACE " [^-][^ ]*" "" name "${ops}")
  string(REGEX REPLACE " +-" "," name "${name}")
  string(REGEX REPLACE "^-" "" name "${name}")
  string(STRIP "${name}" name)
  # want it grouped by run, not by test, and it auto-alpha-sorts
  set(${testname} "${name}|${test}" PARENT_SCOPE)
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
