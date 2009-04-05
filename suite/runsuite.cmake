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

# Unfinished features in i#66:
# * Windows support: must set env vars for cl32 vs cl64
# * ssh support
# * have a list of known failures and label w/ " (known: i#XX)"
# * use the features in the latest CTest: -W, -j

cmake_minimum_required (VERSION 2.2)

if ("${CTEST_SCRIPT_ARG}" MATCHES "nightly")
  # FIXME NOT FINISHED i#11: nightly long run
  # FIXME: need to pass in CTEST_DASHBOARD_ROOT and CTEST_SITE
  set(BINARY_BASE "${CTEST_DASHBOARD_ROOT}/")

  # We assume a manual check out was done, and that CTest can just do "update".
  # If we want a fresh checkout we can set CTEST_BACKUP_AND_RESTORE
  # and CTEST_CHECKOUT_COMMAND but the update should be fine.
  set(CTEST_UPDATE_COMMAND "/usr/bin/svn")

  set(SUITE_TYPE Nightly)
  set(DO_UPDATE ON)
  set(DO_SUBMIT ON)
  set(SUBMIT_LOCAL OFF)
  set(TEST_LONG ON)
else ("${CTEST_SCRIPT_ARG}" MATCHES "nightly")
  # a local run, not a nightly
  get_filename_component(BINARY_BASE "." ABSOLUTE)
  set(SUITE_TYPE Experimental)
  set(DO_UPDATE OFF)
  set(DO_SUBMIT ON)
  set(SUBMIT_LOCAL ON)
  if ("${CTEST_SCRIPT_ARG}" MATCHES "long")
    set(TEST_LONG ON)
  else ("${CTEST_SCRIPT_ARG}" MATCHES "long")
    set(TEST_LONG OFF)
  endif ("${CTEST_SCRIPT_ARG}" MATCHES "long")
endif ("${CTEST_SCRIPT_ARG}" MATCHES "nightly")

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
set(CTEST_CMAKE_GENERATOR "Unix Makefiles")
set(CTEST_PROJECT_NAME "DynamoRIO")
set(CTEST_BUILD_COMMAND "/usr/bin/make -j5")
set(CTEST_COMMAND "${CTEST_EXECUTABLE_NAME}")

function(testbuild name initial_cache)
  set(CTEST_BUILD_NAME "${name}")
  set(CTEST_BINARY_DIRECTORY "${BINARY_BASE}/build_${CTEST_BUILD_NAME}")
  set(CTEST_DROP_LOCATION "${CTEST_BINARY_DIRECTORY}")
  set(CTEST_INITIAL_CACHE "${initial_cache}
    BUILD_TESTS:BOOL=ON
    TEST_SUITE:BOOL=ON
    ${test_long_cache}
    ")
  ctest_empty_binary_directory(${CTEST_BINARY_DIRECTORY})
  file(WRITE "${CTEST_BINARY_DIRECTORY}/CMakeCache.txt" "${CTEST_INITIAL_CACHE}")
  ctest_start(${SUITE_TYPE})
  if (DO_UPDATE)
    ctest_update(SOURCE "${CTEST_SOURCE_DIRECTORY}")
  endif (DO_UPDATE)
  ctest_configure(BUILD "${CTEST_BINARY_DIRECTORY}")
  ctest_build(BUILD "${CTEST_BINARY_DIRECTORY}")
  # to run a subset of tests add an INCLUDE regexp to ctest_test.  e.g.:
  #   INCLUDE broadfun
  ctest_test(BUILD "${CTEST_BINARY_DIRECTORY}")
  if (DO_SUBMIT)
    ctest_submit()
  endif (DO_SUBMIT)
endfunction(testbuild)

testbuild("debug-internal-32" "
  X64:BOOL=OFF
  DEBUG:BOOL=ON
  INTERNAL:BOOL=ON
  ")
testbuild("debug-internal-64" "
  X64:BOOL=ON
  DEBUG:BOOL=ON
  INTERNAL:BOOL=ON
  ")
testbuild("debug-external-32" "
  X64:BOOL=OFF
  DEBUG:BOOL=ON
  INTERNAL:BOOL=OFF
  ")
testbuild("debug-external-64" "
  X64:BOOL=ON
  DEBUG:BOOL=ON
  INTERNAL:BOOL=OFF
  ")
testbuild("release-external-32" "
  X64:BOOL=OFF
  DEBUG:BOOL=OFF
  INTERNAL:BOOL=OFF
  ")
testbuild("release-external-64" "
  X64:BOOL=ON
  DEBUG:BOOL=OFF
  INTERNAL:BOOL=OFF
  ")
# we don't really use internal release builds for anything, but keep it working
if (DO_ALL_BUILDS)
  testbuild("release-internal-32" "
    X64:BOOL=OFF
    DEBUG:BOOL=OFF
    INTERNAL:BOOL=ON
    ")
  testbuild("release-internal-64" "
    X64:BOOL=ON
    DEBUG:BOOL=OFF
    INTERNAL:BOOL=ON
    ")
endif (DO_ALL_BUILDS)
# non-official-API builds
testbuild("vmsafe-debug-internal-32" "
  VMAP:BOOL=OFF
  VMSAFE:BOOL=ON
  X64:BOOL=OFF
  DEBUG:BOOL=ON
  INTERNAL:BOOL=ON
  ")
if (DO_ALL_BUILDS)
  testbuild("vmsafe-release-external-32" "
    VMAP:BOOL=OFF
    VMSAFE:BOOL=ON
    X64:BOOL=OFF
    DEBUG:BOOL=OFF
    INTERNAL:BOOL=OFF
    ")
endif (DO_ALL_BUILDS)
testbuild("vps-debug-internal-32" "
  VMAP:BOOL=OFF
  VPS:BOOL=ON
  X64:BOOL=OFF
  DEBUG:BOOL=ON
  INTERNAL:BOOL=ON
  ")
if (DO_ALL_BUILDS)
  testbuild("vps-release-external-32" "
    VMAP:BOOL=OFF
    VPS:BOOL=ON
    X64:BOOL=OFF
    DEBUG:BOOL=OFF
    INTERNAL:BOOL=OFF
    ")
  # Builds we'll keep from breaking but not worth running many tests
  testbuild("callprof-32" "
    CALLPROF:BOOL=ON
    X64:BOOL=OFF
    DEBUG:BOOL=OFF
    INTERNAL:BOOL=OFF
    ")
  testbuild("linkcount-32" "
    LINKCOUNT:BOOL=ON
    X64:BOOL=OFF
    DEBUG:BOOL=ON
    INTERNAL:BOOL=ON
    ")
  testbuild("nodefs-debug-internal-32" "
    VMAP:BOOL=OFF
    X64:BOOL=OFF
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
file(GLOB all_xml build*/*Configure.xml)
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
