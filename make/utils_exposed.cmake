## **********************************************************
## Copyright (c) 2012-2016 Google, Inc.    All rights reserved.
## **********************************************************
##
## Redistribution and use in source and binary forms, with or without
## modification, are permitted provided that the following conditions are met:
##
## * Redistributions of source code must retain the above copyright notice,
##   this list of conditions and the following disclaimer.
##
## * Redistributions in binary form must reproduce the above copyright notice,
##   this list of conditions and the following disclaimer in the documentation
##   and/or other materials provided with the distribution.
##
## * Neither the name of Google, Inc. nor the names of its contributors may be
##   used to endorse or promote products derived from this software without
##   specific prior written permission.
##
## THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
## AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
## IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
## ARE DISCLAIMED. IN NO EVENT SHALL GOOGLE, INC. OR CONTRIBUTORS BE LIABLE
## FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
## DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
## SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
## CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
## LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
## OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
## DAMAGE.
##
## These functions are included into DynamoRIOConfig.cmake and thus into
## the user's project.  Thus we use _DR_ name prefixes to avoid conflict.

# sets CMAKE_COMPILER_IS_CLANG and CMAKE_COMPILER_IS_GNUCC in parent scope
function (_DR_identify_clang)
  # Assume clang behaves like gcc.  CMake 2.6 won't detect clang and will set
  # CMAKE_COMPILER_IS_GNUCC to TRUE, but 2.8 does not.  We prefer the 2.6
  # behavior.
  string(REGEX MATCH "clang" CMAKE_COMPILER_IS_CLANG "${CMAKE_C_COMPILER}")
  if (CMAKE_COMPILER_IS_CLANG)
    set(CMAKE_COMPILER_IS_GNUCC TRUE PARENT_SCOPE)
  else ()
    if (CMAKE_C_COMPILER MATCHES "/cc")
      # CMake 2.8.10 on Mac has CMAKE_C_COMPILER as "/usr/bin/cc"
      execute_process(COMMAND ${CMAKE_C_COMPILER} --version
        OUTPUT_VARIABLE cc_out ERROR_QUIET)
      if (cc_out MATCHES "clang")
        set(CMAKE_COMPILER_IS_CLANG ON)
        set(CMAKE_COMPILER_IS_GNUCC TRUE PARENT_SCOPE)
      endif ()
    endif ()
  endif ()
  set(CMAKE_COMPILER_IS_CLANG ${CMAKE_COMPILER_IS_CLANG} PARENT_SCOPE)
endfunction (_DR_identify_clang)

function (_DR_append_property_string type target name value)
  # XXX: if we require cmake 2.8.6 we can simply use APPEND_STRING
  get_property(cur ${type} ${target} PROPERTY ${name})
  if (cur)
    set(value "${cur} ${value}")
  endif (cur)
  set_property(${type} ${target} PROPERTY ${name} "${value}")
endfunction (_DR_append_property_string)

# Drops the last path element from path and stores it in path_out.
function (_DR_dirname path_out path)
  string(REGEX REPLACE "/[^/]*$" "" path "${path}")
  set(${path_out} "${path}" PARENT_SCOPE)
endfunction (_DR_dirname)

# Takes in a target and a list of libraries and adds relative rpaths
# pointing to the directories of the libraries.
#
# By default, CMake sets an absolute rpath to the build directory, which it
# strips at install time.  By adding our own relative rpath, so long as the
# target and its libraries stay in the same layout relative to each other,
# the loader will be able to find the libraries.  We assume that the layout
# is the same in the build and install directories.
function (DynamoRIO_add_rel_rpaths target)
  if (UNIX AND NOT ANDROID) # No DT_RPATH support on Android
    # Turn off the default CMake rpath setting and add our own LINK_FLAGS.
    set_target_properties(${target} PROPERTIES SKIP_BUILD_RPATH ON)
    foreach (lib ${ARGN})
      # Compute the relative path between the directory of the target and the
      # library it is linked against.
      get_target_property(tgt_path ${target} LOCATION)
      get_target_property(lib_path ${lib} LOCATION)
      _DR_dirname(tgt_path "${tgt_path}")
      _DR_dirname(lib_path "${lib_path}")
      file(RELATIVE_PATH relpath "${tgt_path}" "${lib_path}")

      # Append the new rpath element if it isn't there already.
      if (APPLE)
        # @loader_path seems to work for executables too
        set(new_lflag "-Wl,-rpath,'@loader_path/${relpath}'")
        get_target_property(lflags ${target} LINK_FLAGS)
        # We match the trailing ' to avoid matching a parent dir only
        if (NOT lflags MATCHES "@loader_path/${relpath}'")
          _DR_append_property_string(TARGET ${target} LINK_FLAGS "${new_lflag}")
        endif ()
      else (APPLE)
        set(new_lflag "-Wl,-rpath='$ORIGIN/${relpath}'")
        get_target_property(lflags ${target} LINK_FLAGS)
        if (NOT lflags MATCHES "\\$ORIGIN/${relpath}")
          _DR_append_property_string(TARGET ${target} LINK_FLAGS "${new_lflag}")
        endif ()
      endif ()
    endforeach ()
  endif ()
endfunction (DynamoRIO_add_rel_rpaths)

# Check if we're using GNU gold.  We use CMAKE_C_COMPILER in
# CMAKE_C_LINK_EXECUTABLE, so call the compiler instead of CMAKE_LINKER.  That
# way we query the linker that the compiler actually uses.
function (_DR_check_if_linker_is_gnu_gold var_out)
  if (WIN32)
    # We don't support gold on Windows.  We only support the MSVC toolchain.
    set(is_gold OFF)
  else ()
    if (APPLE)
      # Running through gcc results in failing exit code so run ld directly:
      set(linkver ${CMAKE_LINKER};-v)
    else (APPLE)
      set(linkver ${CMAKE_C_COMPILER};-Wl,--version)
    endif (APPLE)
    execute_process(COMMAND ${linkver}
      RESULT_VARIABLE ld_result
      ERROR_QUIET  # gcc's collect2 always writes to stderr, so ignore it.
      OUTPUT_VARIABLE ld_out)
    set(is_gold OFF)
    if (ld_result)
      message("failed to get linker version, assuming ld.bfd (${ld_result})")
    elseif ("${ld_out}" MATCHES "GNU gold")
      set(is_gold ON)
    endif ()
  endif ()
  set(${var_out} ${is_gold} PARENT_SCOPE)
endfunction (_DR_check_if_linker_is_gnu_gold)

function (DynamoRIO_get_target_path_for_execution out target device_base_dir)
  get_target_property(abspath ${target} LOCATION${location_suffix})
  if (NOT ${device_base_dir} STREQUAL "")
    get_filename_component(builddir ${PROJECT_BINARY_DIR} NAME)
    file(RELATIVE_PATH relpath "${PROJECT_BINARY_DIR}" "${abspath}")
    set(${out} ${device_base_dir}/${builddir}/${relpath} PARENT_SCOPE)
  else ()
    set(${out} ${abspath} PARENT_SCOPE)
  endif ()
endfunction (DynamoRIO_get_target_path_for_execution)

function (DynamoRIO_prefix_cmd_if_necessary cmd_out use_ats cmd_in)
  if (ANDROID)
    if (use_ats)
      set(${cmd_out} "adb@shell@${cmd_in}${ARGN}" PARENT_SCOPE)
    else ()
      set(${cmd_out} adb shell ${cmd_in} ${ARGN} PARENT_SCOPE)
    endif ()
  else ()
    set(${cmd_out} ${cmd_in} ${ARGN} PARENT_SCOPE)
  endif ()
endfunction (DynamoRIO_prefix_cmd_if_necessary)

function (DynamoRIO_copy_target_to_device target device_base_dir)
  get_target_property(abspath ${target} LOCATION${location_suffix})
  get_filename_component(builddir ${PROJECT_BINARY_DIR} NAME)
  file(RELATIVE_PATH relpath "${PROJECT_BINARY_DIR}" "${abspath}")
  add_custom_command(TARGET ${target} POST_BUILD
    COMMAND ${ADB} push ${abspath} ${device_base_dir}/${builddir}/${relpath}
    VERBATIM)
endfunction (DynamoRIO_copy_target_to_device)

# On Linux, the individual object files contained by an archive are
# garbage collected by the linker if they are not referenced.  To avoid
# this, we have to use the --whole-archive option with ld.
function(DynamoRIO_force_static_link target lib)
  if (UNIX)
    # CMake ignores libraries starting with '-' and preserves the
    # ordering, so we can pass flags through target_link_libraries, which
    # ensures we have the right CMake dependencies.
    target_link_libraries(${target} -Wl,--whole-archive ${lib} -Wl,--no-whole-archive)
  else ()
    target_link_libraries(${target} ${lib})
  endif ()
endfunction(DynamoRIO_force_static_link)
