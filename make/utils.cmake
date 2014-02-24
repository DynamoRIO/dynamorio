## **********************************************************
## Copyright (c) 2012-2014 Google, Inc.    All rights reserved.
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

# sets CMAKE_COMPILER_IS_CLANG and CMAKE_COMPILER_IS_GNUCC in parent scope
function (identify_clang)
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
endfunction (identify_clang)

function (append_property_string type target name value)
  # XXX: if we require cmake 2.8.6 we can simply use APPEND_STRING
  get_property(cur ${type} ${target} PROPERTY ${name})
  if (cur)
    set(value "${cur} ${value}")
  endif (cur)
  set_property(${type} ${target} PROPERTY ${name} "${value}")
endfunction (append_property_string)

function (append_property_list type target name value)
  # XXX: if we require cmake 2.8.6 we can simply use APPEND_LIST
  get_property(cur ${type} ${target} PROPERTY ${name})
  if (cur)
    set(value ${cur} ${value})
  endif (cur)
  set_property(${type} ${target} PROPERTY ${name} ${value})
endfunction (append_property_list)

# Drops the last path element from path and stores it in path_out.
function (dirname path_out path)
  string(REGEX REPLACE "/[^/]*$" "" path "${path}")
  set(${path_out} "${path}" PARENT_SCOPE)
endfunction (dirname)

# Takes in a target and a list of libraries and adds relative rpaths
# pointing to the directories of the libraries.
#
# By default, CMake sets an absolute rpath to the build directory, which it
# strips at install time.  By adding our own relative rpath, so long as the
# target and its libraries stay in the same layout relative to each other,
# the loader will be able to find the libraries.  We assume that the layout
# is the same in the build and install directories.
function (add_rel_rpaths target)
  if (UNIX)
    # Turn off the default CMake rpath setting and add our own LINK_FLAGS.
    set_target_properties(${target} PROPERTIES SKIP_BUILD_RPATH ON)
    foreach (lib ${ARGN})
      # Compute the relative path between the directory of the target and the
      # library it is linked against.
      get_target_property(tgt_path ${target} LOCATION)
      get_target_property(lib_path ${lib} LOCATION)
      dirname(tgt_path "${tgt_path}")
      dirname(lib_path "${lib_path}")
      file(RELATIVE_PATH relpath "${tgt_path}" "${lib_path}")

      # Append the new rpath element if it isn't there already.
      if (APPLE)
        # @loader_path seems to work for executables too
        set(new_lflag "-Wl,-rpath,'@loader_path/${relpath}'")
        get_target_property(lflags ${target} LINK_FLAGS)
        if (NOT lflags MATCHES "@loader_path/${relpath}")
          append_property_string(TARGET ${target} LINK_FLAGS "${new_lflag}")
        endif ()
      else (APPLE)
        set(new_lflag "-Wl,-rpath='$ORIGIN/${relpath}'")
        get_target_property(lflags ${target} LINK_FLAGS)
        if (NOT lflags MATCHES "\\$ORIGIN/${relpath}")
          append_property_string(TARGET ${target} LINK_FLAGS "${new_lflag}")
        endif ()
      endif ()
    endforeach ()
  endif (UNIX)
endfunction (add_rel_rpaths)

# Check if we're using GNU gold.  We use CMAKE_C_COMPILER in
# CMAKE_C_LINK_EXECUTABLE, so call the compiler instead of CMAKE_LINKER.  That
# way we query the linker that the compiler actually uses.
function (check_if_linker_is_gnu_gold var_out)
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
endfunction (check_if_linker_is_gnu_gold)

# disable known warnings
function (disable_compiler_warnings)
  if (WIN32)
    # disable stack protection: "unresolved external symbol ___security_cookie"
    # disable the warning "unreferenced formal parameter" #4100
    # disable the warning "conditional expression is constant" #4127
    # disable the warning "cast from function pointer to data pointer" #4054
    set(CL_CFLAGS "/GS- /wd4100 /wd4127 /wd4054")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${CL_CFLAGS}" PARENT_SCOPE)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${CL_CFLAGS}" PARENT_SCOPE)
    add_definitions(-D_CRT_SECURE_NO_WARNINGS)
  endif (WIN32)
endfunction (disable_compiler_warnings)

# clients/extensions don't include configure.h so they don't get DR defines
function (add_dr_defines)
  foreach (config "" ${CMAKE_BUILD_TYPE} ${CMAKE_CONFIGURATION_TYPES})
    if ("${config}" STREQUAL "")
      set(config_upper "")
    else ("${config}" STREQUAL "")
      string(TOUPPER "_${config}" config_upper)
    endif ("${config}" STREQUAL "")
    foreach (var CMAKE_C_FLAGS${config_upper};CMAKE_CXX_FLAGS${config_upper})
      if (DEBUG)
        set(${var} "${${var}} -DDEBUG" PARENT_SCOPE)
      endif (DEBUG)
      # we're used to X64 instead of X86_64
      if (X64)
        set(${var} "${${var}} -DX64" PARENT_SCOPE)
      endif (X64)
    endforeach (var)
  endforeach (config)
endfunction (add_dr_defines)

function (install_subdirs tgt_lib tgt_bin)
  # These cover all subdirs.
  # Subdirs just need to install their targets.
  DR_install(DIRECTORY ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/
    DESTINATION ${tgt_lib}
    FILE_PERMISSIONS ${owner_access} OWNER_EXECUTE GROUP_READ GROUP_EXECUTE
    WORLD_READ WORLD_EXECUTE
    FILES_MATCHING
    PATTERN "*.debug"
    PATTERN "*.pdb"
    )
  file(GLOB bin_files "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/*")
  if (bin_files)
    DR_install(DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/
      DESTINATION ${tgt_bin}
      FILE_PERMISSIONS ${owner_access} OWNER_EXECUTE GROUP_READ GROUP_EXECUTE
      WORLD_READ WORLD_EXECUTE
      FILES_MATCHING
      PATTERN "*.debug"
      PATTERN "*.pdb"
      )
  endif (bin_files)
endfunction (install_subdirs)

