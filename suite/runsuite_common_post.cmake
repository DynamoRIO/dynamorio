# **********************************************************
# Copyright (c) 2011-2020 Google, Inc.    All rights reserved.
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

# Test suite post-processing
# See instructions in runsuite_common_pre.cmake

cmake_minimum_required (VERSION 2.6)
if (COMMAND cmake_policy)
  # avoid warnings on include()
  cmake_policy(VERSION 2.8)
endif()

# Caller should set build_package boolean and should
# have included runsuite_common_pre.cmake which sets
# last_package_build_dir
if (build_package)
  # PR 534018: pre-commit test suite should test building the full package
  # Plus, we use this for package.cmake now.
  # now package up all the builds
  message("building package in ${last_package_build_dir}")
  set(CTEST_BUILD_NAME "final package")
  set(CTEST_BINARY_DIRECTORY "${last_package_build_dir}")
  file(APPEND "${last_package_build_dir}/CPackConfig.cmake"
    "set(CPACK_INSTALL_CMAKE_PROJECTS\n  ${cpack_projects})")
  if ("${CTEST_CMAKE_GENERATOR}" MATCHES "Visual Studio")
    get_default_config(defconfig "${CTEST_BINARY_DIRECTORY}")
    set(CTEST_BUILD_CONFIGURATION "${defconfig}")
    set(CTEST_BUILD_TARGET "PACKAGE")
    if (arg_use_msbuild)
      string(REPLACE "REPLACE_CONFIG" "${defconfig}" CTEST_BUILD_COMMAND
        "${CTEST_BUILD_COMMAND}")
    endif (arg_use_msbuild)
  else ()
    if (build_source_package)
      set(CTEST_BUILD_FLAGS "package package_source")
    else ()
      set(CTEST_BUILD_FLAGS "package")
    endif ()
  endif ()

  ctest_start(${SUITE_TYPE})
  ctest_build(BUILD "${CTEST_BINARY_DIRECTORY}")
  if (CTEST_DROP_METHOD MATCHES "http")
    ctest_submit()
  endif ()
else (build_package)
  # workaround for http://www.cmake.org/Bug/view.php?id=9647
  # it complains and returns error if CTEST_BINARY_DIRECTORY not set at
  # global scope (we do all our real runs inside a function).
  set(CTEST_BUILD_NAME "bug9647workaround")
  set(CTEST_BINARY_DIRECTORY "${last_build_dir}")
  set(CTEST_SOURCE_DIRECTORY "${CTEST_SCRIPT_DIRECTORY}/..")
  set(CTEST_COMMAND "${CTEST_EXECUTABLE_NAME}")
  # it tries to configure+build, but with a start command it does nothing,
  # which is what we want:
  ctest_start(${SUITE_TYPE})
  # actually it still complains so I'm not sure what version I was using where
  # just the start was enough: so we do a test w/ no tests that would match,
  # which does work for cmake 2.6, but not for 2.8: grrr
  # I tried doing a build w/ "make help" and doing a submit,
  # but still says "Error in read script".
  ctest_test(BUILD "${CTEST_BINARY_DIRECTORY}" INCLUDE notestwouldmatchthis)
endif (build_package)

set(outf "${BINARY_BASE}/results.txt")
file(WRITE ${outf} "==================================================\nRESULTS\n\n")
file(GLOB all_missing_cross_compilations ${BINARY_BASE}/*/Testing/missing-cross-compile)
foreach (missing_cross_compile ${all_missing_cross_compilations})
  file(READ ${missing_cross_compile} missing_cross_compile_name)
  file(APPEND ${outf} "${missing_cross_compile_name}: skipped ")
  file(APPEND ${outf} "(cross-compile configuration unavailable)\n")
endforeach (missing_cross_compile)
if (arg_already_built)
  file(GLOB all_xml ${RESULTS_DIR}/*Test.xml ${RESULTS_DIR}/*final*Build.xml)
else (arg_already_built)
  file(GLOB all_xml ${RESULTS_DIR}/*Configure.xml ${RESULTS_DIR}/*final*Build.xml)
endif (arg_already_built)
list(SORT all_xml)
foreach (xml ${all_xml})
  get_filename_component(fname "${xml}" NAME_WE)
  string(REGEX REPLACE "^___([^_]+)___.*$" "\\1" build "${fname}")
  file(READ ${xml} string)
  if ("${string}" MATCHES "Configuring incomplete" OR
      "${string}" MATCHES "Generate step failed")
    file(APPEND ${outf} "${build}: **** pre-build configure errors ****\n")
    string(REGEX MATCHALL "CMake Error at .*errors occurred!"
      config_error "${string}")
    file(APPEND ${outf} "\t${config_error}\n")
  else ()
    string(REGEX REPLACE "Configure.xml$" "Build.xml" xml "${xml}")
    file(READ ${xml} string)
    # i#835: make sure to treat warnings-as-errors as errors
    string(REGEX MATCHALL "<Error>|warning treated as error" build_errors "${string}")
    if (build_errors)
      list(LENGTH build_errors num_errors)
      file(APPEND ${outf} "${build}: **** ${num_errors} build errors ****\n")
      # avoid ; messing up interp as list
      string(REGEX REPLACE ";" ":" string "${string}")
      string(REGEX MATCHALL
        "<Error>[^<]*<BuildLogLine>[^<]*</BuildLogLine>[^<]*<Text>[^<]+<"
        failures "${string}")
      # XXX i#802: ideally we'd show all the warnings that led to the error, but
      # that's not easy, so for now we just show the final msg which just has
      # one of the line #s and not the actual warning text.  Once we have no
      # non-fatal warnings we can just display all warnings here.
      string(REGEX MATCHALL
        "<Text>[^<]*warning treated as error[^<]*</Text>"
        warn_failures "${string}")
      foreach (failure ${failures} ${warn_failures})
        string(REGEX REPLACE "^.*<Text>([^<]+)<" "\\1" text "${failure}")
        string(REGEX REPLACE "/Text>$" "" text "${text}") # warn_failures has this
        # replace escaped chars for weird quote with simple quote
        string(REGEX REPLACE "&lt:-30&gt:&lt:-128&gt:&lt:-10[34]&gt:" "'" text "${text}")
        string(STRIP "${text}" text)
        file(APPEND ${outf} "\t${text}\n")
      endforeach (failure)
    else (build_errors)
      # Check whether the max warning limit is hiding build failures (i#1137):
      string(REGEX MATCHALL "The maximum number of reported" build_errors "${string}")
      if (build_errors)
        file(APPEND ${outf} "WARNING: maximum warning/error limit hit for ${build}!"
          "\n  Manually verify whether it succeeded.\n")
        set(build_status "status **UNKNOWN**")
      else (build_errors)
        set(build_status "successful")
      endif (build_errors)

      string(REGEX REPLACE "Build.xml$" "Test.xml" xml "${xml}")
      if (EXISTS ${xml})
        file(READ ${xml} string)
        string(REGEX MATCHALL "Status=\"passed\"" passed "${string}")
        list(LENGTH passed num_passed)
        string(REGEX MATCHALL "Status=\"failed\"" test_errors "${string}")
      else (EXISTS ${xml})
        set(passed OFF)
        set(test_errors OFF)
      endif (EXISTS ${xml})
      if (test_errors)
        list(LENGTH test_errors num_errors)

        # sanity check
        file(GLOB lastfailed build_${build}/Testing/Temporary/LastTestsFailed*.log)
        if (EXISTS "${lastfailed}") # won't exist for package build
          file(READ "${lastfailed}" faillist)
          string(REGEX MATCHALL "\n" faillines "${faillist}")
          list(LENGTH faillines failcount)
          if (NOT failcount EQUAL num_errors)
            message("WARNING: ${num_errors} errors != ${lastfailed} => ${failcount}")
          endif (NOT failcount EQUAL num_errors)
        endif (EXISTS "${lastfailed}")

        set(summary "")
        # avoid ; messing up interp as list
        string(REGEX REPLACE "&[^;]+;" "" string "${string}")
        string(REGEX REPLACE ";" ":" string "${string}")
        # work around cmake regexps doing maximal matching: we want minimal
        # so we pick a char unlikely to be present to avoid using ".*"
        # avoid any existing % messing up error processing
        string(REGEX REPLACE "%" "x" string "${string}")
        string(REGEX REPLACE "</Measurement>" "%</Measurement>" string "${string}")
        string(REGEX MATCHALL "Status=\"failed\">[^%]*%</Measurement>"
          failures "${string}")
        # FIXME: have a list of known failures and label w/ " (known: i#XX)"
        set(num_flaky 0)
        foreach (failure ${failures})
          # show key failures like crashes and asserts
          string(REGEX REPLACE "^.*<Name>([^<]+)<.*$" "\\1" name "${failure}")
          error_string("${failure}" reason)
          set(summary "${summary}\t${name} ${reason}\n")
          if ("${name}" MATCHES "_FLAKY$")
            math(EXPR num_flaky "${num_flaky} + 1")
          endif ()
        endforeach (failure)

        # Append summary.
        file(APPEND ${outf} "${build}: ${num_passed} tests passed, ")
        file(APPEND ${outf} "**** ${num_errors} tests failed")
        if (${num_flaky} GREATER 0)
          file(APPEND ${outf} ", of which ${num_flaky} were flaky")
        endif ()
        file(APPEND ${outf} ": ****\n")
        file(APPEND ${outf} "${summary}")
      else (test_errors)
        if (passed)
          file(APPEND ${outf} "${build}: all ${num_passed} tests passed\n")
        else (passed)
          file(APPEND ${outf}
            "${build}: build ${build_status}; no tests for this build\n")
        endif (passed)
      endif (test_errors)
    endif (build_errors)
  endif ()
endforeach (xml)

file(READ ${outf} string)
message("${string}")
