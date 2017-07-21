# **********************************************************
# Copyright (c) 2010-2017 Google, Inc.    All rights reserved.
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

cmake_minimum_required (VERSION 2.6)

set(CTEST_PROJECT_NAME "DynamoRIO")
set(cpack_project_name "DynamoRIO")
set(run_tests ON)
set(CTEST_SOURCE_DIRECTORY "${CTEST_SCRIPT_DIRECTORY}/..")
if (APPLE)
  # For now we run just a quarter of the tests, using a test label.
  # FIXME i#1815: get all the tests working.
  set(extra_ctest_args INCLUDE_LABEL OSX)
endif ()
include("${CTEST_SCRIPT_DIRECTORY}/runsuite_common_pre.cmake")

# extra args (note that runsuite_common_pre.cmake has already walked
# the list and did not remove its args so be sure to avoid conflicts).
set(arg_travis OFF)
set(cross_only OFF)
foreach (arg ${CTEST_SCRIPT_ARG})
  if (${arg} STREQUAL "travis")
    set(arg_travis ON)
    if ($ENV{DYNAMORIO_CROSS_ONLY} MATCHES "yes")
      set(cross_only ON)
    endif()
  endif ()
endforeach (arg)

if (arg_travis)
  # XXX i#1801, i#1962: under clang we have several failing tests.  Until those are
  # fixed, our Travis clang suite only builds and does not run tests.
  if (UNIX AND NOT APPLE AND "$ENV{CC}" MATCHES "clang")
    set(run_tests OFF)
    message("Detected a Travis clang suite: disabling running of tests")
  endif ()
endif()

if (TEST_LONG)
  set(DO_ALL_BUILDS ON)
  set(base_cache "${base_cache}
    BUILD_TESTS:BOOL=ON
    TEST_LONG:BOOL=ON")
else (TEST_LONG)
  set(DO_ALL_BUILDS OFF)
endif (TEST_LONG)

if (UNIX)
  # For cross-arch execve tests we need to run from an install dir
  set(installpath "${BINARY_BASE}/install")
  set(install_build_args "install")
  set(install_path_cache "CMAKE_INSTALL_PREFIX:PATH=${installpath}
    ")
else (UNIX)
  set(install_build_args "")
endif (UNIX)

if (APPLE)
  # DRi#58: core DR does not yet support 64-bit Mac
  set(arg_32_only ON)
endif ()

##################################################
# Pre-commit source file checks.
# We do have some pre-commit hooks but we don't rely on them.
# We also have vera++ style checking rules for C/C++ code, but we
# keep a few checks here to cover cmake and other code.
# We could do a GLOB_RECURSE and read every file, but that's slow, so
# we try to construct the diff.
if (EXISTS "${CTEST_SOURCE_DIRECTORY}/.svn" OR
    # in case the top-level dir and not trunk was checked out
    EXISTS "${CTEST_SOURCE_DIRECTORY}/../.svn")
  find_program(SVN svn DOC "subversion client")
  if (SVN)
    execute_process(COMMAND ${SVN} diff
      WORKING_DIRECTORY "${CTEST_SOURCE_DIRECTORY}"
      RESULT_VARIABLE svn_result
      ERROR_VARIABLE svn_err
      OUTPUT_VARIABLE diff_contents)
    if (svn_result OR svn_err)
      message(FATAL_ERROR "*** ${SVN} diff failed: ***\n${svn_result} ${svn_err}")
    endif (svn_result OR svn_err)
    # Remove tabs from the revision lines
    string(REGEX REPLACE "\n(---|\\+\\+\\+)[^\n]*\t" "" diff_contents "${diff_contents}")
  endif (SVN)
else ()
  if (EXISTS "${CTEST_SOURCE_DIRECTORY}/.git")
    find_program(GIT git DOC "git client")
    if (GIT)
      # Included committed, staged, and unstaged changes.
      # We assume "origin/master" contains the top-of-trunk.
      execute_process(COMMAND ${GIT} diff origin/master
        WORKING_DIRECTORY "${CTEST_SOURCE_DIRECTORY}"
        RESULT_VARIABLE git_result
        ERROR_VARIABLE git_err
        OUTPUT_VARIABLE diff_contents)
      if (git_result OR git_err)
        if (git_err MATCHES "unknown revision")
          # It may be a cloned branch
          execute_process(COMMAND ${GIT} remote -v
            WORKING_DIRECTORY "${CTEST_SOURCE_DIRECTORY}"
            RESULT_VARIABLE git_result
            ERROR_VARIABLE git_err
            OUTPUT_VARIABLE git_out)
        endif ()
        if (git_result OR git_err)
          # Not a fatal error as this can happen when mixing cygwin and windows git.
          message(STATUS "${GIT} remote -v failed: ${git_err}")
          set(git_out OFF)
        endif (git_result OR git_err)
        if (NOT git_out)
          # No remotes set up: we assume this is a custom git setup that
          # is only likely to get used on our buildbots, so we skip
          # the diff checks.
          message(STATUS "No remotes set up so cannot diff and must skip content checks.  Assuming this is a buildbot.")
          set(diff_contents "")
        else ()
          message(FATAL_ERROR "*** Unable to retrieve diff for content checks: do you have a custom remote setup?")
        endif ()
      endif (git_result OR git_err)
    endif (GIT)
  endif (EXISTS "${CTEST_SOURCE_DIRECTORY}/.git")
endif ()
if (NOT DEFINED diff_contents)
  message(FATAL_ERROR "Unable to construct diff for pre-commit checks")
endif ()

# Check for tabs.  We already removed them from svn's diff format.
string(REGEX MATCH "\t" match "${diff_contents}")
if (NOT "${match}" STREQUAL "")
  string(REGEX MATCH "\n[^\n]*\t[^\n]*" match "${diff_contents}")
  message(FATAL_ERROR "ERROR: diff contains tabs: ${match}")
endif ()

# Check for NOCHECKIN
string(REGEX MATCH "NOCHECKIN" match "${diff_contents}")
if (NOT "${match}" STREQUAL "")
  string(REGEX MATCH "\n[^\n]*NOCHECKIN[^\n]*" match "${diff_contents}")
  message(FATAL_ERROR "ERROR: diff contains NOCHECKIN: ${match}")
endif ()

# CMake seems to remove carriage returns for us so we can't easily
# check for them unless we switch to perl or python or something
# to get the diff and check it.  The vera++ rules do check C/C++ code.

# Check for trailing space.  This is a diff with an initial column for +-,
# so a blank line will have one space: thus we rule that out.
string(REGEX MATCH "[^\n] \n" match "${diff_contents}")
if (NOT "${match}" STREQUAL "")
  # Get more context
  string(REGEX MATCH "\n[^\n]+ \n" match "${diff_contents}")
  message(FATAL_ERROR "ERROR: diff contains trailing spaces: ${match}")
endif ()

##################################################


# for short suite, don't build tests for builds that don't run tests
# (since building takes forever on windows): so we only turn
# on BUILD_TESTS for TEST_LONG or debug-internal-{32,64}

if (NOT cross_only)
  # For cross-arch execve test we need to "make install"
  testbuild_ex("debug-internal-32" OFF "
    DEBUG:BOOL=ON
    INTERNAL:BOOL=ON
    BUILD_TESTS:BOOL=ON
    ${install_path_cache}
    " OFF ON "${install_build_args}")
  if (last_build_dir MATCHES "-32")
    set(32bit_path "TEST_32BIT_PATH:PATH=${last_build_dir}/suite/tests/bin")
  else ()
    set(32bit_path "")
  endif ()
  testbuild_ex("debug-internal-64" ON "
    DEBUG:BOOL=ON
    INTERNAL:BOOL=ON
    BUILD_TESTS:BOOL=ON
    ${install_path_cache}
    ${32bit_path}
    " OFF ON "${install_build_args}")
  # we don't really support debug-external anymore
  if (DO_ALL_BUILDS_NOT_SUPPORTED)
    testbuild("debug-external-64" ON "
      DEBUG:BOOL=ON
      INTERNAL:BOOL=OFF
      ")
    testbuild("debug-external-32" OFF "
      DEBUG:BOOL=ON
      INTERNAL:BOOL=OFF
      ")
  endif ()
  testbuild_ex("release-external-32" OFF "
    DEBUG:BOOL=OFF
    INTERNAL:BOOL=OFF
    ${install_path_cache}
    " OFF OFF "${install_build_args}")
  if (last_build_dir MATCHES "-32")
    set(32bit_path "TEST_32BIT_PATH:PATH=${last_build_dir}/suite/tests/bin")
  else ()
    set(32bit_path "")
  endif ()
  testbuild_ex("release-external-64" ON "
    DEBUG:BOOL=OFF
    INTERNAL:BOOL=OFF
    ${install_path_cache}
    ${32bit_path}
    " OFF OFF "${install_build_args}")
  if (DO_ALL_BUILDS)
    # we rarely use internal release builds but keep them working in long
    # suite (not much burden) in case we need to tweak internal options
    testbuild("release-internal-32" OFF "
      DEBUG:BOOL=OFF
      INTERNAL:BOOL=ON
      ${install_path_cache}
      ")
    testbuild("release-internal-64" ON "
      DEBUG:BOOL=OFF
      INTERNAL:BOOL=ON
      ${install_path_cache}
      ")
  endif (DO_ALL_BUILDS)
  # Non-official-API builds but not all are in pre-commit suite, esp on Windows
  # where building is slow: we'll rely on bots to catch breakage in most of these
  # builds.
  if (ARCH_IS_X86 AND NOT APPLE)
    # we do not bother to support these on ARM
    if (DO_ALL_BUILDS)
      testbuild("vmsafe-debug-internal-32" OFF "
        VMAP:BOOL=OFF
        VMSAFE:BOOL=ON
        DEBUG:BOOL=ON
        INTERNAL:BOOL=ON
        ${install_path_cache}
        ")
    endif ()
    if (DO_ALL_BUILDS)
      testbuild("vmsafe-release-external-32" OFF "
        VMAP:BOOL=OFF
        VMSAFE:BOOL=ON
        DEBUG:BOOL=OFF
        INTERNAL:BOOL=OFF
        ${install_path_cache}
        ")
    endif (DO_ALL_BUILDS)
    # i#2406: we skip the vps build to speed up PR's, using just the merge to
    # master to catch breakage in vps.
    if (NOT DEFINED ENV{APPVEYOR_PULL_REQUEST_NUMBER})
      testbuild("vps-debug-internal-32" OFF "
        VMAP:BOOL=OFF
        VPS:BOOL=ON
        DEBUG:BOOL=ON
        INTERNAL:BOOL=ON
        ${install_path_cache}
        ")
    endif ()
    if (DO_ALL_BUILDS)
      testbuild("vps-release-external-32" OFF "
        VMAP:BOOL=OFF
        VPS:BOOL=ON
        DEBUG:BOOL=OFF
        INTERNAL:BOOL=OFF
        ${install_path_cache}
        ")
      # Builds we'll keep from breaking but not worth running many tests
      testbuild("callprof-32" OFF "
        CALLPROF:BOOL=ON
        DEBUG:BOOL=OFF
        INTERNAL:BOOL=OFF
        ${install_path_cache}
        ")
    endif (DO_ALL_BUILDS)
  endif (ARCH_IS_X86 AND NOT APPLE)
endif (NOT cross_only)

if (UNIX AND ARCH_IS_X86)
  # Optional cross-compilation for ARM/Linux and ARM/Android if the cross
  # compilers are on the PATH.
  if (NOT cross_only)
    # For Travis cross_only builds, we want to fail on config failures.
    # For user suite runs, we want to just skip if there's no cross setup.
    set(optional_cross_compile ON)
  endif ()
  set(ARCH_IS_X86 OFF)
  set(ENV{CFLAGS} "") # environment vars do not obey the normal scope rules--must reset
  set(ENV{CXXFLAGS} "")
  set(prev_run_tests ${run_tests})
  set(run_tests OFF) # build tests but don't run them
  testbuild_ex("arm-debug-internal-32" OFF "
    DEBUG:BOOL=ON
    INTERNAL:BOOL=ON
    BUILD_TESTS:BOOL=ON
    CMAKE_TOOLCHAIN_FILE:PATH=${CTEST_SOURCE_DIRECTORY}/make/toolchain-arm32.cmake
    " OFF OFF "")
  testbuild_ex("arm-release-external-32" OFF "
    DEBUG:BOOL=OFF
    INTERNAL:BOOL=OFF
    CMAKE_TOOLCHAIN_FILE:PATH=${CTEST_SOURCE_DIRECTORY}/make/toolchain-arm32.cmake
    " OFF OFF "")
  testbuild_ex("arm-debug-internal-64" ON "
    DEBUG:BOOL=ON
    INTERNAL:BOOL=ON
    BUILD_TESTS:BOOL=ON
    CMAKE_TOOLCHAIN_FILE:PATH=${CTEST_SOURCE_DIRECTORY}/make/toolchain-arm64.cmake
    " OFF OFF "")
  testbuild_ex("arm-release-external-64" ON "
    DEBUG:BOOL=OFF
    INTERNAL:BOOL=OFF
    CMAKE_TOOLCHAIN_FILE:PATH=${CTEST_SOURCE_DIRECTORY}/make/toolchain-arm64.cmake
    " OFF OFF "")
  set(run_tests ${prev_run_tests})

  # Android cross-compilation and running of tests using "adb shell"
  # FIXME i#2207: once we have Android cross-compilation working on Travis, remove this:
  set(optional_cross_compile ON)
  find_program(ADB adb DOC "adb Android utility")
  if (ADB)
    execute_process(COMMAND ${ADB} get-state
      RESULT_VARIABLE adb_result
      ERROR_VARIABLE adb_err
      OUTPUT_VARIABLE adb_out OUTPUT_STRIP_TRAILING_WHITESPACE)
    if (adb_result OR NOT adb_out STREQUAL "device")
      message("Android device not connected: NOT running Android tests")
      set(ADB OFF)
    endif ()
  else ()
    message("adb not found: NOT running Android tests")
  endif ()
  set(prev_run_tests ${run_tests})
  if (ADB)
    set(android_extra_dbg "DR_COPY_TO_DEVICE:BOOL=ON")
    if (TEST_LONG)
      set(android_extra_rel "DR_COPY_TO_DEVICE:BOOL=ON")
    endif ()
  else ()
    set(android_extra_dbg "")
    set(android_extra_rel "")
    set(run_tests OFF) # build tests but don't run them
  endif ()
  testbuild_ex("android-debug-internal-32" OFF "
    DEBUG:BOOL=ON
    INTERNAL:BOOL=ON
    CMAKE_TOOLCHAIN_FILE:PATH=${CTEST_SOURCE_DIRECTORY}/make/toolchain-android.cmake
    BUILD_TESTS:BOOL=ON
    ${android_extra_dbg}
    " OFF OFF "")
  testbuild_ex("android-release-external-32" OFF "
    DEBUG:BOOL=OFF
    INTERNAL:BOOL=OFF
    CMAKE_TOOLCHAIN_FILE:PATH=${CTEST_SOURCE_DIRECTORY}/make/toolchain-android.cmake
    ${android_extra_rel}
    " OFF OFF "")
  set(run_tests ${prev_run_tests})

  set(optional_cross_compile OFF)
  set(ARCH_IS_X86 ON)
endif (UNIX AND ARCH_IS_X86)

# XXX: do we still care about these builds?
## defines we don't want to break -- no runs though since we don't currently use these
#    "BUILD::NOSHORT::ADD_DEFINES=\"${D}DGC_DIAGNOSTICS\"",
#    "BUILD::NOSHORT::LIN::ADD_DEFINES=\"${D}CHECK_RETURNS_SSE2\"",

######################################################################
# SUMMARY

# sets ${outvar} in PARENT_SCOPE
function (error_string str outvar)
  string(REGEX MATCHALL "[^\n]*Platform exception[^\n]*" crash "${str}")
  string(REGEX MATCHALL "[^\n]*debug check failure[^\n]*" assert "${str}")
  string(REGEX MATCHALL "[^\n]*CURIOSITY[^\n]*" curiosity "${str}")
  if (crash OR assert OR curiosity)
    string(REGEX REPLACE "[ \t]*<Value>" "" assert "${assert}")
    set(${outvar} "=> ${crash} ${assert} ${curiosity}" PARENT_SCOPE)
  else (crash OR assert OR curiosity)
    set(${outvar} "" PARENT_SCOPE)
  endif (crash OR assert OR curiosity)
endfunction (error_string)

set(build_package OFF)
include("${CTEST_SCRIPT_DIRECTORY}/runsuite_common_post.cmake")
