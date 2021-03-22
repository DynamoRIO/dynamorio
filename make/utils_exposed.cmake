## **********************************************************
## Copyright (c) 2012-2021 Google, Inc.    All rights reserved.
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

function (_DR_append_property_list type target name value)
  set_property(${type} ${target} PROPERTY ${name} ${value} APPEND)
endfunction (_DR_append_property_list)

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

      get_target_property(target_type ${target} TYPE)
      if (target_type STREQUAL "EXECUTABLE")
        get_target_property(target_loc ${target} RUNTIME_OUTPUT_DIRECTORY)
      else ()
        get_target_property(target_loc ${target} LIBRARY_OUTPUT_DIRECTORY)
      endif()
      # Reading the target paths at configure time is no longer supported in
      # cmake (CMP0026).  For an imported target, we can get the path; otherwise
      # the best we can do is a LOCATION property.
      DynamoRIO_get_full_path(lib_loc_full ${lib} "")
      get_filename_component(lib_loc ${lib_loc_full} PATH)
      file(RELATIVE_PATH relpath "${target_loc}" "${lib_loc}")

      # Append the new rpath element if it isn't there already.
      if (APPLE)
        # 10.5+ supports @rpath but I'm having trouble getting it to work properly,
        # so I'm sticking with @loader_path for now, which works for executables too.
        set(new_lflag "-Wl,-rpath,'@loader_path/${relpath}'")
        get_target_property(lflags ${target} LINK_FLAGS)
        # We match the trailing ' to avoid matching a parent dir only
        if (NOT lflags MATCHES "@loader_path/${relpath}'")
          _DR_append_property_string(TARGET ${target} LINK_FLAGS "${new_lflag}")
        endif ()
      else ()
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

# Takes in a target and returns the expected full target path incl. output name.
#
# XXX i#1557: DynamoRIO cmake files used to query the LOCATION target property at
# configure time. This property has been made obsolete, see CMP0026. The function
# here can be used to retrieve the default target path instead. However, for build
# targets, it only supports the default target directories, as seen at configure
# time. For imported targets, we return the LOCATION property as usual.
function (DynamoRIO_get_full_path out target loc_suffix)
  get_target_property(is_imported ${target} IMPORTED)
  if (is_imported)
    get_target_property(local ${target} LOCATION${loc_suffix})
    set(${out} ${local} PARENT_SCOPE)
  else ()
    get_target_property(output_name ${target} OUTPUT_NAME)
    get_target_property(name ${target} NAME)
    get_target_property(suffix ${target} SUFFIX)
    get_target_property(prefix ${target} PREFIX)
    get_target_property(target_type ${target} TYPE)
    if (NOT prefix)
      set(prefix ${CMAKE_${target_type}_PREFIX})
    endif ()
    if (NOT suffix)
      set(suffix ${CMAKE_${target_type}_SUFFIX})
    endif ()
    set(output_dir "")
    if (WIN32)
      if (target_type STREQUAL "MODULE_LIBRARY")
        get_target_property(library_dir ${target} LIBRARY_OUTPUT_DIRECTORY${loc_suffix})
        set(output_dir ${library_dir})
      elseif (target_type STREQUAL "STATIC_LIBRARY")
        get_target_property(archive_dir ${target} ARCHIVE_OUTPUT_DIRECTORY${loc_suffix})
        set(output_dir ${archive_dir})
      elseif (target_type STREQUAL "EXECUTABLE" OR target_type STREQUAL "SHARED_LIBRARY")
        get_target_property(runtime_dir ${target} RUNTIME_OUTPUT_DIRECTORY${loc_suffix})
        set(output_dir ${runtime_dir})
      endif ()
    else ()
      if (target_type STREQUAL "SHARED_LIBRARY" OR target_type STREQUAL "MODULE_LIBRARY")
        get_target_property(library_dir ${target} LIBRARY_OUTPUT_DIRECTORY${loc_suffix})
        set(output_dir ${library_dir})
      elseif (target_type STREQUAL "STATIC_LIBRARY")
        get_target_property(archive_dir ${target} ARCHIVE_OUTPUT_DIRECTORY${loc_suffix})
        set(output_dir ${archive_dir})
      elseif (target_type STREQUAL "EXECUTABLE")
        get_target_property(runtime_dir ${target} RUNTIME_OUTPUT_DIRECTORY${loc_suffix})
        set(output_dir ${runtime_dir})
      endif ()
    endif ()
    # XXX i#3278: DR's win loader can't handle a path like that until full support has
    # been implemented in convert_to_NT_file_path.
    string(REPLACE "/./" "/" output_dir "${output_dir}")
    if (output_name)
      set(${out} "${output_dir}/${prefix}${output_name}${suffix}" PARENT_SCOPE)
    else ()
      set(${out} "${output_dir}/${prefix}${name}${suffix}" PARENT_SCOPE)
    endif ()
  endif ()
endfunction (DynamoRIO_get_full_path)

function (DynamoRIO_get_target_path_for_execution out target device_base_dir loc_suffix)
  DynamoRIO_get_full_path(abspath ${target} "${loc_suffix}")
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
  elseif (CMAKE_CROSSCOMPILING AND DEFINED CMAKE_FIND_ROOT_PATH AND QEMU_BINARY)
    if (use_ats)
      set(${cmd_out}
        "${QEMU_BINARY}@-L@${CMAKE_FIND_ROOT_PATH}@${cmd_in}${ARGN}"
        PARENT_SCOPE)
    else ()
      set(${cmd_out} ${QEMU_BINARY} -L ${CMAKE_FIND_ROOT_PATH}
        ${cmd_in} ${ARGN} PARENT_SCOPE)
    endif ()
  else ()
    set(${cmd_out} ${cmd_in} ${ARGN} PARENT_SCOPE)
  endif ()
endfunction (DynamoRIO_prefix_cmd_if_necessary)

function (DynamoRIO_copy_target_to_device target device_base_dir loc_suffix)
  DynamoRIO_get_full_path(abspath ${target} "${loc_suffix}")
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
    # There is no equivalent for MSVC.  The best we can do is keep a client in place,
    # for our caller in use_DynamoRIO_static_client().
    target_link_libraries(${target} ${lib})
    if (X64)
      set(incname "dr_client_main")
    else ()
      set(incname "_dr_client_main")
    endif ()
    append_property_string(TARGET ${target} LINK_FLAGS "/include:${incname}")
  endif ()
endfunction(DynamoRIO_force_static_link)

function (_DR_get_static_libc_list liblist_out)
  if (WIN32)
    if (DEBUG OR "${CMAKE_BUILD_TYPE}" MATCHES "Debug")
      set(static_libc libcmtd)
      if (tgt_cxx)
        set(static_libc libcpmtd ${static_libc})
      endif ()
      # https://blogs.msdn.microsoft.com/vcblog/2015/03/03/introducing-the-universal-crt
      if (NOT (MSVC_VERSION LESS 1900)) # GREATER_EQUAL is cmake 3.7+ only
        set(static_libc ${static_libc} libvcruntimed.lib libucrtd.lib)
      endif ()
      # libcmt has symbols libcmtd does not so we need all files compiled w/ _DEBUG
      set(extra_flags "${extra_flags} -D_DEBUG")
    else ()
      set(static_libc libcmt)
      if (tgt_cxx)
        set(static_libc libcpmt ${static_libc})
      endif ()
      # https://blogs.msdn.microsoft.com/vcblog/2015/03/03/introducing-the-universal-crt
      if (NOT (MSVC_VERSION LESS 1900)) # GREATER_EQUAL is cmake 3.7+ only
        set(static_libc ${static_libc} libvcruntime.lib libucrt.lib)
      endif ()
    endif ()
    set(${liblist_out} ${static_libc} PARENT_SCOPE)
  else ()
    set(${liblist_out} "" PARENT_SCOPE)
  endif ()
endfunction ()
