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

# Unfinished features in i#66 (now under i#121):
# * have a list of known failures and label w/ " (known: i#XX)"
# * ssh support for running on remote machines: copy, disable pdbs, run

cmake_minimum_required (VERSION 2.2)
set(cmake_ver_string
  "${CMAKE_MAJOR_VERSION}.${CMAKE_MINOR_VERSION}.${CMAKE_RELEASE_VERSION}")

# arguments are a ;-separated list (must escape as \; from ctest_run_script())
set(arg_nightly OFF)  # whether to report the results
set(arg_long OFF)     # whether to run the long suite
set(arg_include "")   # cmake file to include up front
set(arg_preload "")   # cmake file to include prior to each 32-bit build
set(arg_preload64 "") # cmake file to include prior to each 64-bit build
set(arg_site "")

foreach (arg ${CTEST_SCRIPT_ARG})
  if (${arg} STREQUAL "nightly")
    set(arg_nightly ON)
  endif (${arg} STREQUAL "nightly")
  if (${arg} STREQUAL "long")
    set(arg_long ON)
  endif (${arg} STREQUAL "long")
  if (${arg} MATCHES "^include=")
    string(REGEX REPLACE "^include=" "" arg_include "${arg}")
  endif (${arg} MATCHES "^include=")
  if (${arg} MATCHES "^preload=")
    string(REGEX REPLACE "^preload=" "" arg_preload "${arg}")
  endif (${arg} MATCHES "^preload=")
  if (${arg} MATCHES "^preload64=")
    string(REGEX REPLACE "^preload64=" "" arg_preload64 "${arg}")
  endif (${arg} MATCHES "^preload64=")
  if (${arg} MATCHES "^site=")
    string(REGEX REPLACE "^site=" "" arg_site "${arg}")
  endif (${arg} MATCHES "^site=")
endforeach (arg)

# allow setting the base cache variables via an include file
set(base_cache "")
if (arg_include)
  message("including ${arg_include}")
  include(${arg_include})
endif (arg_include)

if (arg_long)
  set(TEST_LONG ON)
else (arg_long)
  set(TEST_LONG OFF)
endif (arg_long)

get_filename_component(BINARY_BASE "." ABSOLUTE)

if (arg_nightly)
  # i#11: nightly run
  # Caller should have set CTEST_SITE via site= arg
  if (arg_site)
    set(CTEST_SITE "${arg_site}")
  else (arg_site)
    message(FATAL_ERROR "must set sitename via site= arg")
  endif (arg_site)

  set(CTEST_DASHBOARD_ROOT "${BINARY_BASE}")

  # We assume a manual check out was done, and that CTest can just do "update".
  # If we want a fresh checkout we can set CTEST_BACKUP_AND_RESTORE
  # and CTEST_CHECKOUT_COMMAND but the update should be fine.
  find_program(CTEST_UPDATE_COMMAND svn DOC "source code update command")

  set(SUITE_TYPE Nightly)
  set(DO_UPDATE ON)
  set(DO_SUBMIT ON)
  set(SUBMIT_LOCAL OFF)
else (arg_nightly)
  # a local run, not a nightly
  set(SUITE_TYPE Experimental)
  set(DO_UPDATE OFF)
  set(DO_SUBMIT ON)
  set(SUBMIT_LOCAL ON)
  # CTest does "scp file ${CTEST_DROP_SITE}:${CTEST_DROP_LOCATION}" so for
  # local copy w/o needing sshd on localhost we arrange to have : in the
  # absolute filepath.  Note that I would prefer having the results inside
  # each build dir, but having : in the build dir name complicates
  # LD_LIBRARY_PATH.
  if (WIN32)
    # Colon not allowed in name so use drive
    string(REGEX MATCHALL "^[A-Za-z]" drive "${BINARY_BASE}")
    string(REGEX REPLACE "^[A-Za-z]:" "" nondrive "${BINARY_BASE}")
    set(CTEST_DROP_SITE "${drive}")
    set(CTEST_DROP_LOCATION "${nondrive}/xmlresults")
    set(RESULTS_DIR "${CTEST_DROP_SITE}:${CTEST_DROP_LOCATION}")
  else (WIN32)
    set(CTEST_DROP_SITE "${BINARY_BASE}/xml")
    set(CTEST_DROP_LOCATION "results")
    set(RESULTS_DIR "${CTEST_DROP_SITE}:${CTEST_DROP_LOCATION}")
  endif (WIN32)
  if (EXISTS "${RESULTS_DIR}")
    file(REMOVE_RECURSE "${RESULTS_DIR}")
  endif (EXISTS "${RESULTS_DIR}")
  file(MAKE_DIRECTORY "${CTEST_DROP_SITE}:${CTEST_DROP_LOCATION}")
endif (arg_nightly)

if (TEST_LONG)
  set(DO_ALL_BUILDS ON)
  set(test_long_cache "TEST_LONG:BOOL=ON")
else (TEST_LONG)
  set(DO_ALL_BUILDS OFF)
  set(test_long_cache "")
endif (TEST_LONG)

set(CTEST_SOURCE_DIRECTORY "${CTEST_SCRIPT_DIRECTORY}/..")
# CTest goes and does our builds and then wants to configure
# and build again and complains there's no top-level setting of
# CTEST_BINARY_DIRECTORY: 
#   "CMake Error: Some required settings in the configuration file were missing:"
# but we don't want to do another build so we just ignore the error.
set(CTEST_CMAKE_COMMAND "${CMAKE_EXECUTABLE_NAME}")
# FIXME: add support for NMake on Windows if devs want to use that
# Doesn't support -j though
set(CTEST_CMAKE_GENERATOR "Unix Makefiles")
set(CTEST_PROJECT_NAME "DynamoRIO")
find_program(MAKE_COMMAND make DOC "make command")
set(CTEST_BUILD_COMMAND_BASE "${MAKE_COMMAND} -j5")
set(CTEST_COMMAND "${CTEST_EXECUTABLE_NAME}")

if (UNIX)
  # For cross-arch execve tests we need to run from an install dir
  set(installpath "${BINARY_BASE}/install")
  set(install_build_args "install")
  set(install_path_cache "CMAKE_INSTALL_PREFIX:PATH=${installpath}
    ")
  # Each individual 64-bit build should also set TEST_32BIT_PATH and one of
  # these.  Note that we put BOTH dirs on the path, so we can run the other test
  # (loader will bypass name match if wrong elf class).
  # FIXME i#145: should we use this dual path as a solution for cross-arch execve?
  # Only if we verify it works on all versions of ld.
  set(use_lib64_debug_cache
    "TEST_LIB_PATH:PATH=${installpath}/lib64/debug:${installpath}/lib32/debug
    ")
  set(use_lib64_release_cache
    "TEST_LIB_PATH:PATH=${installpath}/lib64/release:${installpath}/lib64/release
    ")
else (UNIX)
  set(install_build_args "")
endif (UNIX)

# returns the build dir in "last_build_dir"
function(testbuild_ex name is64 initial_cache build_args)
  set(CTEST_BUILD_NAME "${name}")
  # Support other VC installations than VS2005 via pre-build include file.
  # Preserve path so include file can simply prepend each time.
  set(pre_path "$ENV{PATH}")
  if (is64)
    set(preload_file "${arg_preload64}")
  else (is64)
    set(preload_file "${arg_preload}")
  endif (is64)
  if (preload_file)
    # Command-style CTest (i.e., using ctest_configure(), etc.) does
    # not support giving args to CTEST_CMAKE_COMMAND so we are forced
    # to do an include() instead of -C
    include("${preload_file}")
  endif (preload_file)
  # support "make install"
  set(CTEST_BUILD_COMMAND "${CTEST_BUILD_COMMAND_BASE} ${build_args}")
  set(CTEST_BINARY_DIRECTORY "${BINARY_BASE}/build_${CTEST_BUILD_NAME}")
  set(last_build_dir "${CTEST_BINARY_DIRECTORY}" PARENT_SCOPE)
  # for short suite, don't build tests for builds that don't run tests
  # (since building takes forever on windows)
  if (NOT "${test_long_cache}" STREQUAL "" OR
      "${name}" STREQUAL "debug-internal-32" OR
      "${name}" STREQUAL "debug-internal-64")
    set(tests_cache "BUILD_TESTS:BOOL=ON")
  else ()
    set(tests_cache "")
  endif()
  if (WIN32)
    # i#111: Disabling progress and status messages can speed the Windows build
    # up by up to 50%.  I filed CMake bug 8726 on this and this variable will be
    # in the 2.8 release; going ahead and setting now.
    # For Linux these messages make little perf difference, and can help
    # diagnose errors or watch progress.
    set(os_specific_defines "CMAKE_RULE_MESSAGES:BOOL=OFF")
  else (WIN32)
    set(os_specific_defines "")
  endif (WIN32)
  set(CTEST_INITIAL_CACHE "${initial_cache}
    ${tests_cache}
    TEST_SUITE:BOOL=ON
    ${os_specific_defines}
    ${test_long_cache}
    ${base_cache}
    ")
  ctest_empty_binary_directory(${CTEST_BINARY_DIRECTORY})
  file(WRITE "${CTEST_BINARY_DIRECTORY}/CMakeCache.txt" "${CTEST_INITIAL_CACHE}")

  if (WIN32)
    # If other compilers also on path ensure we pick cl
    set(ENV{CC} "cl")
    set(ENV{CXX} "cl")
    # Convert env vars to run proper compiler.
    # Note that this is fragile and won't work with non-standard
    # directory layouts: we assume standard VS2005 or SDK.
    # FIXME: would be nice to have case-insensitive regex flag!
    # For now hardcoding VC, Bin, amd64
    if (is64)
      if (NOT "$ENV{LIB}" MATCHES "[Aa][Mm][Dd]64")
        # Note that we can't set ENV{PATH} as the output var of the replace:
        # it has to be its own set().
        string(REGEX REPLACE "VC([/\\\\])Bin" "VC\\1Bin\\1amd64"
          newpath "$ENV{PATH}")
        set(ENV{PATH} "${newpath}")
        string(REGEX REPLACE "([/\\\\])([Ll][Ii][Bb])" "\\1\\2\\1amd64"
          newlib "$ENV{LIB}")
        set(ENV{LIB} "${newlib}")
        string(REGEX REPLACE "([/\\\\])([Ll][Ii][Bb])" "\\1\\2\\1amd64"
          newlibpath "$ENV{LIBPATH}")
        set(ENV{LIBPATH} "${newlibpath}")
      endif (NOT "$ENV{LIB}" MATCHES "[Aa][Mm][Dd]64")
    else (is64)
      if ("$ENV{LIB}" MATCHES "[Aa][Mm][Dd]64")
        string(REGEX REPLACE "(VC[/\\\\]Bin[/\\\\])amd64" "\\1"
          newpath "$ENV{PATH}")
        set(ENV{PATH} "${newpath}")
        string(REGEX REPLACE "([Ll][Ii][Bb])[/\\\\]amd64" "\\1"
          newlib "$ENV{LIB}")
        set(ENV{LIB} "${newlib}")
        string(REGEX REPLACE "([Ll][Ii][Bb])[/\\\\]amd64" "\\1"
          newlibpath "$ENV{LIBPATH}")
        set(ENV{LIBPATH} "${newlibpath}")
      endif ("$ENV{LIB}" MATCHES "[Aa][Mm][Dd]64")
    endif (is64)
  else (WIN32)
    if (is64)
      set(ENV{CFLAGS} "-m64")
      set(ENV{CXXFLAGS} "-m64")
    else (is64)
      set(ENV{CFLAGS} "-m32")
      set(ENV{CXXFLAGS} "-m32")
    endif (is64)
  endif (WIN32)

  ctest_start(${SUITE_TYPE})
  if (DO_UPDATE)
    ctest_update(SOURCE "${CTEST_SOURCE_DIRECTORY}")
  endif (DO_UPDATE)
  ctest_configure(BUILD "${CTEST_BINARY_DIRECTORY}")
  ctest_build(BUILD "${CTEST_BINARY_DIRECTORY}")
  # to run a subset of tests add an INCLUDE regexp to ctest_test.  e.g.:
  #   INCLUDE broadfun
  if ("${cmake_ver_string}" STRLESS "2.8.")
    # Parallel tests not supported
    set(RUN_PARALLEL OFF)
  elseif (WIN32 AND TEST_LONG)
    # FIXME i#265: on Windows we can't run multiple instances of
    # the same app b/c of global reg key conflicts: should support
    # env vars and not require registry
    set(RUN_PARALLEL OFF)
  else ()
    set(RUN_PARALLEL ON)
  endif ()
  if (RUN_PARALLEL)
    # i#111: run tests in parallel, supported on CTest 2.8.0+
    # Note that adding -j to CMAKE_COMMAND does not work, though invoking
    # this script with -j does work, but we want parallel by default.
    ctest_test(BUILD "${CTEST_BINARY_DIRECTORY}" PARALLEL_LEVEL 5)
  else (RUN_PARALLEL)
    ctest_test(BUILD "${CTEST_BINARY_DIRECTORY}")
  endif (RUN_PARALLEL)
  if (DO_SUBMIT)
    # include any notes via set(CTEST_NOTES_FILES )?
    ctest_submit()
  endif (DO_SUBMIT)
  set(ENV{PATH} "${pre_path}")
endfunction(testbuild_ex)

function(testbuild name is64 initial_cache)
  testbuild_ex(${name} ${is64} ${initial_cache} "")
endfunction(testbuild)

# For cross-arch execve test we need to "make install"
testbuild_ex("debug-internal-32" OFF "
  DEBUG:BOOL=ON
  INTERNAL:BOOL=ON
  ${install_path_cache}
  " "${install_build_args}")
testbuild_ex("debug-internal-64" ON "
  DEBUG:BOOL=ON
  INTERNAL:BOOL=ON
  ${install_path_cache}
  ${use_lib64_debug_cache}
  TEST_32BIT_PATH:PATH=${last_build_dir}/suite/tests/bin
  " "${install_build_args}")
testbuild("debug-external-32" OFF "
  DEBUG:BOOL=ON
  INTERNAL:BOOL=OFF
  ")
testbuild("debug-external-64" ON "
  DEBUG:BOOL=ON
  INTERNAL:BOOL=OFF
  ")
testbuild_ex("release-external-32" OFF "
  DEBUG:BOOL=OFF
  INTERNAL:BOOL=OFF
  ${install_path_cache}
  " "${install_build_args}")
testbuild_ex("release-external-64" ON "
  DEBUG:BOOL=OFF
  INTERNAL:BOOL=OFF
  ${install_path_cache}
  ${use_lib64_release_cache}
  TEST_32BIT_PATH:PATH=${last_build_dir}/suite/tests/bin
  " "${install_build_args}")
# we don't really use internal release builds for anything, but keep it working
if (DO_ALL_BUILDS)
  testbuild("release-internal-32" OFF "
    DEBUG:BOOL=OFF
    INTERNAL:BOOL=ON
    ")
  testbuild("release-internal-64" ON "
    DEBUG:BOOL=OFF
    INTERNAL:BOOL=ON
    ")
endif (DO_ALL_BUILDS)
# non-official-API builds
testbuild("vmsafe-debug-internal-32" OFF "
  VMAP:BOOL=OFF
  VMSAFE:BOOL=ON
  DEBUG:BOOL=ON
  INTERNAL:BOOL=ON
  ")
if (DO_ALL_BUILDS)
  testbuild("vmsafe-release-external-32" OFF "
    VMAP:BOOL=OFF
    VMSAFE:BOOL=ON
    DEBUG:BOOL=OFF
    INTERNAL:BOOL=OFF
    ")
endif (DO_ALL_BUILDS)
testbuild("vps-debug-internal-32" OFF "
  VMAP:BOOL=OFF
  VPS:BOOL=ON
  DEBUG:BOOL=ON
  INTERNAL:BOOL=ON
  ")
if (DO_ALL_BUILDS)
  testbuild("vps-release-external-32" OFF "
    VMAP:BOOL=OFF
    VPS:BOOL=ON
    DEBUG:BOOL=OFF
    INTERNAL:BOOL=OFF
    ")
  # Builds we'll keep from breaking but not worth running many tests
  testbuild("callprof-32" OFF "
    CALLPROF:BOOL=ON
    DEBUG:BOOL=OFF
    INTERNAL:BOOL=OFF
    ")
  testbuild("linkcount-32" OFF "
    LINKCOUNT:BOOL=ON
    DEBUG:BOOL=ON
    INTERNAL:BOOL=ON
    ")
  testbuild("nodefs-debug-internal-32" OFF "
    VMAP:BOOL=OFF
    DEBUG:BOOL=ON
    INTERNAL:BOOL=ON
    ")
endif (DO_ALL_BUILDS)

# FIXME: what about these builds?
## defines we don't want to break -- no runs though since we don't currently use these
#    "BUILD::NOSHORT::ADD_DEFINES=\"${D}DGC_DIAGNOSTICS\"",
#    "BUILD::NOSHORT::LIN::ADD_DEFINES=\"${D}CHECK_RETURNS_SSE2\"",

######################################################################
# SUMMARY

set(outf "${BINARY_BASE}/results.txt")
file(WRITE ${outf} "==================================================\nRESULTS\n\n")
file(GLOB all_xml ${RESULTS_DIR}/*Configure.xml)
list(SORT all_xml)
foreach (xml ${all_xml})
  get_filename_component(fname "${xml}" NAME_WE)
  string(REGEX REPLACE "^___([^_]+)___.*$" "\\1" build "${fname}")
  file(READ ${xml} string)
  if ("${string}" MATCHES "Configuring incomplete")
    file(APPEND ${outf} "${build}: **** pre-build configure errors ****\n")
  else ("${string}" MATCHES "Configuring incomplete")
    string(REGEX REPLACE "Configure.xml$" "Build.xml" xml "${xml}")
    file(READ ${xml} string)
    string(REGEX MATCHALL "<Error>" build_errors "${string}")
    if (build_errors)
      list(LENGTH build_errors num_errors)
      file(APPEND ${outf} "${build}: **** ${num_errors} build errors ****\n")
      # avoid ; messing up interp as list
      string(REGEX REPLACE ";" ":" string "${string}")
      string(REGEX MATCHALL
        "<Error>[^<]*<BuildLogLine>[^<]*</BuildLogLine>[^<]*<Text>[^<]+<"
        failures "${string}")
      foreach (failure ${failures})
        string(REGEX REPLACE "^.*<Text>([^<]+)<" "\\1" text "${failure}")
        # replace escaped chars for weird quote with simple quote
        string(REGEX REPLACE "&lt:-30&gt:&lt:-128&gt:&lt:-10[34]&gt:" "'" text "${text}")
        string(STRIP "${text}" text)
        file(APPEND ${outf} "\t${text}\n")
      endforeach (failure)
    else (build_errors)
      string(REGEX REPLACE "Build.xml$" "Test.xml" xml "${xml}")
      file(READ ${xml} string)
      string(REGEX MATCHALL "Status=\"passed\"" passed "${string}")
      list(LENGTH passed num_passed)
      string(REGEX MATCHALL "Status=\"failed\"" test_errors "${string}")
      if (test_errors)
        list(LENGTH test_errors num_errors)

        # sanity check
        file(GLOB lastfailed build_${build}/Testing/Temporary/LastTestsFailed*.log)
        file(READ ${lastfailed} faillist)
        string(REGEX MATCHALL "\n" faillines "${faillist}")
        list(LENGTH faillines failcount)
        if (NOT failcount EQUAL num_errors)
          message("WARNING: ${num_errors} errors != ${lastfailed} => ${failcount}")
        endif (NOT failcount EQUAL num_errors)

        file(APPEND ${outf}
          "${build}: ${num_passed} tests passed, **** ${num_errors} tests failed: ****\n")
        # avoid ; messing up interp as list
        string(REGEX REPLACE "&[^;]+;" "" string "${string}")
        string(REGEX REPLACE ";" ":" string "${string}")
        # work around cmake regexps doing maximal matching: we want minimal
        # so we pick a char unlikely to be present to avoid using ".*"
        string(REGEX REPLACE "</Measurement>" "}</Measurement>" string "${string}")
        string(REGEX MATCHALL "Status=\"failed\">[^}]*}</Measurement>"
          failures "${string}")
        # FIXME: have a list of known failures and label w/ " (known: i#XX)"
        foreach (failure ${failures})
          # show key failures like crashes and asserts
          string(REGEX MATCHALL "[^\n]*Unrecoverable[^\n]*" crash "${failure}")
          string(REGEX MATCHALL "[^\n]*Internal DynamoRIO Error[^\n]*" 
            assert "${failure}")
          string(REGEX MATCHALL "[^\n]*CURIOSITY[^\n]*" curiosity "${failure}")
          string(REGEX REPLACE "^.*<Name>([^<]+)<.*$" "\\1" name "${failure}")
          if (crash OR assert OR curiosity)
            string(REGEX REPLACE "[ \t]*<Value>" "" assert "${assert}")
            set(reason "=> ${crash} ${assert} ${curiosity}")
          else (crash OR assert OR curiosity)
            set(reason "")
          endif (crash OR assert OR curiosity)
          file(APPEND ${outf} "\t${name} ${reason}\n")
        endforeach (failure)
      else (test_errors)
        if (passed)
          file(APPEND ${outf} "${build}: all ${num_passed} tests passed\n")
        else (passed)
          file(APPEND ${outf} "${build}: build successful; no tests for this build\n")
        endif (passed)
      endif (test_errors)
    endif (build_errors)
  endif ("${string}" MATCHES "Configuring incomplete")
endforeach (xml)

file(READ ${outf} string)
message("${string}")

# workaround for http://www.cmake.org/Bug/view.php?id=9647
# it complains and returns error if CTEST_BINARY_DIRECTORY not set
set(CTEST_BINARY_DIRECTORY "${BINARY_BASE}/")
# it tries to configure+build, but with a start command it does nothing,
# which is what we want:
ctest_start(${SUITE_TYPE})
