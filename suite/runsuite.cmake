# **********************************************************
# Copyright (c) 2010-2018 Google, Inc.    All rights reserved.
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
set(arg_package OFF)
set(arg_require_format OFF)
set(cross_aarchxx_linux_only OFF)
set(cross_android_only OFF)
foreach (arg ${CTEST_SCRIPT_ARG})
  if (${arg} STREQUAL "travis")
    set(arg_travis ON)
    if ($ENV{DYNAMORIO_CROSS_AARCHXX_LINUX_ONLY} MATCHES "yes")
      set(cross_aarchxx_linux_only ON)
    endif()
    if ($ENV{DYNAMORIO_CROSS_ANDROID_ONLY} MATCHES "yes")
      set(cross_android_only ON)
    endif()
  elseif (${arg} STREQUAL "package")
    # This builds a package out of *all* build dirs.  That will result in
    # conflicts if different architectures are being built: e.g., ARM
    # and AArch64.  It's up to the caller to arrange things properly.
    set(arg_package ON)
  elseif (${arg} STREQUAL "require_format")
    set(arg_require_format ON)
  endif ()
endforeach (arg)

if (arg_travis)
  # XXX i#1801, i#1962: under clang we have several failing tests.  Until those are
  # fixed, our Travis clang suite only builds and does not run tests.
  if (UNIX AND NOT APPLE AND "$ENV{CC}" MATCHES "clang")
    set(run_tests OFF)
    message("Detected a Travis clang suite: disabling running of tests")
  endif ()
  if ("$ENV{TRAVIS_EVENT_TYPE}" STREQUAL "cron" OR
      "$ENV{APPVEYOR_REPO_TAG}" STREQUAL "true")
    # We don't want flaky tests to derail package deployment.  We've already run
    # the tests for this same commit via regular master-push triggers: these
    # package builds are coming from a cron trigger (Travis) or a tag addition
    # (Appveyor), not a code change.
    # XXX: I'd rather set this in the .yml files but I don't see a way to set
    # one env var based on another's value there.
    set(run_tests OFF)
    message("Detected a cron package build: disabling running of tests")
  endif()
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
  endif (SVN)
else ()
  if (EXISTS "${CTEST_SOURCE_DIRECTORY}/.git")
    find_program(GIT git DOC "git client")
    if (GIT)
      # Included committed, staged, and unstaged changes.
      # We assume "origin/master" contains the top-of-trunk.
      # We pass -U0 so clang-format-diff only complains about touched lines.
      execute_process(COMMAND ${GIT} diff -U0 origin/master
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
          message(STATUS "${GIT} remote -v failed (${git_result}): ${git_err}")
          set(git_out OFF)
        endif (git_result OR git_err)
        if (NOT git_out)
          # No remotes set up: we assume this is a custom git setup that
          # is only likely to get used on our buildbots, so we skip
          # the diff checks.
          message(STATUS "No remotes set up so cannot diff and must skip content checks.  Assuming this is a buildbot.")
          set(diff_contents "")
        elseif (WIN32 AND arg_travis)
          # This happens with tagged builds, such as cronbuilds, where there
          # is no master in the shallow clone.
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

# Ensure changes are formatted according to clang-format.
# XXX i#2876: we'd like to ignore changes to files like this which we don't
# want to mark up with "// clang-format off":
# + ext/drsyms/libefltc/include/*.h
# + third_party/libgcc/*.c
# + third_party/valgrind/*.h
# However, there's no simple way to do that.  For now we punt until someone
# changes one of those.
#
# Prefer named version 6.0 from apt.llvm.org.
find_program(CLANG_FORMAT_DIFF clang-format-diff-6.0 DOC "clang-format-diff")
if (NOT CLANG_FORMAT_DIFF)
  find_program(CLANG_FORMAT_DIFF clang-format-diff DOC "clang-format-diff")
endif ()
if (NOT CLANG_FORMAT_DIFF)
  find_program(CLANG_FORMAT_DIFF clang-format-diff.py DOC "clang-format-diff")
endif ()
if (CLANG_FORMAT_DIFF)
  get_filename_component(CUR_DIR "." ABSOLUTE)
  set(diff_file "${CUR_DIR}/runsuite_diff.patch")
  file(WRITE ${diff_file} "${diff_contents}")
  execute_process(COMMAND ${CLANG_FORMAT_DIFF} -p1
    WORKING_DIRECTORY "${CTEST_SOURCE_DIRECTORY}"
    INPUT_FILE ${diff_file}
    RESULT_VARIABLE format_result
    ERROR_VARIABLE format_err
    OUTPUT_VARIABLE format_out)
  if (format_result OR format_err)
    message(FATAL_ERROR
      "Error (${format_result}) running clang-format-diff: ${format_err}")
  endif ()
  if (format_out)
    # The WARNING and FATAL_ERROR message types try to format the diff and it
    # looks bad w/ extra newlines, so we use STATUS for a more verbatim printout.
    message(STATUS
      "Changes are not formatted properly:\n${format_out}")
    message(FATAL_ERROR
      "FATAL ERROR: Changes are not formatted properly (see diff above)!")
  else ()
    message("clang-format check passed")
  endif ()
else ()
  if (arg_require_format)
    message(FATAL_ERROR "FATAL ERROR: clang-format is required but not found!")
  else ()
    message("clang-format-diff not found: skipping format checks")
  endif ()
endif ()

# Check for tabs other than on the revision lines.
# The clang-format check will now find these in C files, but not non-C files.
string(REGEX REPLACE "\n(---|\\+\\+\\+)[^\n]*\t" "" diff_notabs "${diff_contents}")
string(REGEX MATCH "\t" match "${diff_notabs}")
if (NOT "${match}" STREQUAL "")
  string(REGEX MATCH "\n[^\n]*\t[^\n]*" match "${diff_notabs}")
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
# The clang-format check will now find these in C files, but not non-C files.
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

if (NOT cross_aarchxx_linux_only AND NOT cross_android_only)
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
    " OFF ${arg_package} "${install_build_args}")
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
    " OFF ${arg_package} "${install_build_args}")
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
endif (NOT cross_aarchxx_linux_only AND NOT cross_android_only)

if (UNIX AND ARCH_IS_X86)
  # Optional cross-compilation for ARM/Linux and ARM/Android if the cross
  # compilers are on the PATH.
  set(prev_optional_cross_compile ${optional_cross_compile})
  if (NOT cross_aarchxx_linux_only)
    # For Travis cross_aarchxx_linux_only builds, we want to fail on config failures.
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
    " OFF ${arg_package} "")
  testbuild_ex("arm-release-external-32" OFF "
    DEBUG:BOOL=OFF
    INTERNAL:BOOL=OFF
    CMAKE_TOOLCHAIN_FILE:PATH=${CTEST_SOURCE_DIRECTORY}/make/toolchain-arm32.cmake
    " OFF ${arg_package} "")
  testbuild_ex("arm-debug-internal-64" ON "
    DEBUG:BOOL=ON
    INTERNAL:BOOL=ON
    BUILD_TESTS:BOOL=ON
    CMAKE_TOOLCHAIN_FILE:PATH=${CTEST_SOURCE_DIRECTORY}/make/toolchain-arm64.cmake
    " OFF ${arg_package} "")
  testbuild_ex("arm-release-external-64" ON "
    DEBUG:BOOL=OFF
    INTERNAL:BOOL=OFF
    CMAKE_TOOLCHAIN_FILE:PATH=${CTEST_SOURCE_DIRECTORY}/make/toolchain-arm64.cmake
    " OFF ${arg_package} "")
  set(run_tests ${prev_run_tests})
  set(optional_cross_compile ${prev_optional_cross_compile})

  # Android cross-compilation and running of tests using "adb shell"
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

  # Pass through toolchain file.
  if (DEFINED ENV{DYNAMORIO_ANDROID_TOOLCHAIN})
    set(android_extra_dbg "${android_extra_dbg}
                           ANDROID_TOOLCHAIN:PATH=$ENV{DYNAMORIO_ANDROID_TOOLCHAIN}")
    set(android_extra_rel "${android_extra_dbg}
                           ANDROID_TOOLCHAIN:PATH=$ENV{DYNAMORIO_ANDROID_TOOLCHAIN}")
  endif()

  # For Travis cross_android_only builds, we want to fail on config failures.
  # For user suite runs, we want to just skip if there's no cross setup.
  if (NOT cross_android_only)
    set(optional_cross_compile ON)
  endif ()

  testbuild_ex("android-debug-internal-32" OFF "
    DEBUG:BOOL=ON
    INTERNAL:BOOL=ON
    CMAKE_TOOLCHAIN_FILE:PATH=${CTEST_SOURCE_DIRECTORY}/make/toolchain-android.cmake
    BUILD_TESTS:BOOL=ON
    ${android_extra_dbg}
    " OFF ${arg_package} "")
  testbuild_ex("android-release-external-32" OFF "
    DEBUG:BOOL=OFF
    INTERNAL:BOOL=OFF
    CMAKE_TOOLCHAIN_FILE:PATH=${CTEST_SOURCE_DIRECTORY}/make/toolchain-android.cmake
    ${android_extra_rel}
    " OFF ${arg_package} "")
  set(run_tests ${prev_run_tests})

  set(optional_cross_compile ${prev_optional_cross_compile})
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

set(build_package ${arg_package})
include("${CTEST_SCRIPT_DIRECTORY}/runsuite_common_post.cmake")
