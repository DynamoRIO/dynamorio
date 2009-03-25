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

# put libs in same dir for tests that need to load them
set(MAIN_EXECUTABLE_OUTPUT_PATH "${EXECUTABLE_OUTPUT_PATH}")
set(MAIN_LIBRARY_OUTPUT_PATH "${LIBRARY_OUTPUT_PATH}")
set(LIBRARY_OUTPUT_PATH "${CMAKE_CURRENT_BINARY_DIR}/bin")
set(EXECUTABLE_OUTPUT_PATH "${CMAKE_CURRENT_BINARY_DIR}/bin")

include_directories(${CMAKE_CURRENT_SOURCE_DIR}) # tools.h
include_directories(${PROJECT_SOURCE_DIR}/core/x86) # asm_defines.asm

##################################################
# COMPILATION FLAGS

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

string(TOUPPER "${CMAKE_BUILD_TYPE}" CMAKE_BUILD_TYPE_UPPER)
set(ORIG_CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${CMAKE_C_FLAGS_${CMAKE_BUILD_TYPE_UPPER}}")
set(ORIG_CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS}")

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

set(CLIENT_CMAKE_C_FLAGS "${CMAKE_C_FLAGS_${CMAKE_BUILD_TYPE_UPPER}}")
set(CLIENT_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS}")

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
  if (libmath-NOTFOUND OR libdl-NOTFOUND)
    message(FATAL_ERROR "cannot find required libs m and dl")
  endif (libmath-NOTFOUND OR libdl-NOTFOUND)
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

function(template2expect outexpect template runops)
  # We used to use a custom perl script to match defines and runtime
  # options but we've switched to cpp.  We convert -foo to -Dfoo and
  # -foo value to -Dfoo___value.
  string(REGEX REPLACE
    "(^| +)-([^ ]+)"
    " -D\\2" rundefs "${runops}")
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

set(TEST_SECONDS "60" CACHE STRING "Test time limit in seconds")

# FIXME i#65 NOT YET COMPLETED: see comments below: how do multiple
# runs w/ diff options for same build?  don't want to have to
# reconfigure and rebuild tests each time (way too slow on windows).
# will have to move .template->.expect to runtime.
set(TEST_OPTIONS "" CACHE STRING "Extra DynamoRIO options when running tests")

function(rundr_cmd outcmd outops native dr_ops_aux)
  if (UNIX)
    # assumes tools have been built: enforced at top level
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
    # target drinjectex.exe?
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
    # FIXME: for now we convert .template at build time: but we'd like to
    # do it at run time to avoid having to reconfigure/rebuild.
    # FIXME: we need to add dependencies so when change .template don't
    # have to manually reconfigure
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

function(add_exe test source)
  get_filename_component(srcbase ${source} NAME_WE)
  get_filename_component(srcpath ${source} PATH)

  set(test_srcs ${source})

  # we can't set the target properties COMPILE_FLAGS as that will
  # pass -g3 to cpp which ends up not expanding macros!
  # so we set properties on source files only.
  file(READ ${CMAKE_CURRENT_SOURCE_DIR}/${source} srccode)
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
  if (UNIX)
    target_link_libraries(${test} ${libmath} ${libdl})
  endif (UNIX)
endfunction(add_exe)

# normal app
function(tobuild test source)
  add_exe(${test} ${source})
  torun(${test} ${test} ${source} OFF "" "")
endfunction(tobuild)

# client interface app with client
function(tobuild_ci test source client_ops)
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

  torun(${test} ${test} ${source} OFF
    " -code_api -client_lib ${client_path}\;0\;${client_ops}" "")
endfunction(tobuild_ci)

# client interface standalone app
function(tobuild_api test source dr_ops exe_ops)
  add_exe(${test} ${source})
  target_link_libraries(${test} dynamorio)
  add_dependencies(${test} api_headers dynamorio)
  torun(${test} ${test} ${source} ON "${dr_ops}" "${exe_ops}")
endfunction(tobuild_api)

###########################################################################
# tests

tobuild(common.broadfun common/broadfun.c)
tobuild(common.conflict common/conflict.c)
tobuild(common.decode-bad common/decode-bad.c)
tobuild(common.decode common/decode.c)
tobuild(common.eflags common/eflags.c)
tobuild(common.fib common/fib.c)
tobuild(common.getretaddr common/getretaddr.c)
tobuild(common.hello common/hello.c)
tobuild(common.ops common/ops.c)
tobuild(common.protect-dstack common/protect-dstack.c)
tobuild(common.recurse common/recurse.c)
tobuild(common.segfault common/segfault.c)

# FIXME i#65 NOT YET COMPLETED: add rest of tests once resolve further issues

tobuild_ci(client-interface.truncate client-interface/truncate.c "")
tobuild_ci(client-interface.file_io client-interface/file_io.c
  "${CMAKE_CURRENT_SOURCE_DIR}/client-interface/file_io_data.txt")

# FIXME i#65 NOT YET COMPLETED: enable once resolve out-of-memory issues
#tobuild_api(api.dis api/dis.c "-syntax_intel"
#  "${CMAKE_CURRENT_SOURCE_DIR}/api/dis-udis86-randtest.raw")

tobuild(linux.clone linux/clone.c)
# FIXME: execve => execve{32,64} and runs execve-sub
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

# sample of how to add custom DR options
torun(linux.thread-reset linux.thread linux/thread.c OFF
  "-reset_at_fragment_count 100" "")

###########################################################################

# FIXME i#65 NOT YET COMPLETED: add command to run natively

# FIXME: how do multiple runs of same build w/ diff options?
# have list in here?
# and then multiple builds are done externally via ctest --build-and-test?
# but want diff sets of option runs for diff builds: just have ifs in here?
# could be nice: for any one build, can easily run all the tests
# have LONG and SHORT

# FIXME: add a VMKERNEL=ON build to the runregression list

