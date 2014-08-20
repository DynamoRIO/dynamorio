# **********************************************************
# Copyright (c) 2012-2014 Google, Inc.    All rights reserved.
# **********************************************************
#
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
# * Neither the name of Google, Inc. nor the names of its contributors may be
#   used to endorse or promote products derived from this software without
#   specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED. IN NO EVENT SHALL GOOGLE, INC. OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
# SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
# CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
# OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
# DAMAGE.

# CMAKE_CURRENT_LIST_DIR wasn't added until 2.8.3
get_filename_component(utils_cmake_dir "${CMAKE_CURRENT_LIST_FILE}" PATH)
include(${utils_cmake_dir}/utils_exposed.cmake)

# sets CMAKE_COMPILER_IS_CLANG and CMAKE_COMPILER_IS_GNUCC in parent scope
function (identify_clang)
  _DR_identify_clang()
  if (CMAKE_COMPILER_IS_GNUCC)
    set(CMAKE_COMPILER_IS_GNUCC TRUE PARENT_SCOPE)
  endif ()
  set(CMAKE_COMPILER_IS_CLANG ${CMAKE_COMPILER_IS_CLANG} PARENT_SCOPE)
endfunction (identify_clang)

function (append_property_string type target name value)
  _DR_append_property_string(${type} ${target} ${name} "${value}")
endfunction (append_property_string)

function (append_property_list type target name value)
  # XXX: if we require cmake 2.8.6 we can simply use APPEND_LIST
  get_property(cur ${type} ${target} PROPERTY ${name})
  if (cur)
    set(value ${cur} ${value})
  endif (cur)
  set_property(${type} ${target} PROPERTY ${name} ${value})
endfunction (append_property_list)

function (add_rel_rpaths target)
  DynamoRIO_add_rel_rpaths(${target} ${ARGN})
endfunction (add_rel_rpaths)

function (check_if_linker_is_gnu_gold var_out)
  _DR_check_if_linker_is_gnu_gold(is_gold)
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
    REGEX ".*.dSYM/.*DWARF/.*" # too painful to get right # of backslash for literal .
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
      REGEX ".*.dSYM/.*DWARF/.*" # too painful to get right # of backslash for literal .
      )
  endif (bin_files)
endfunction (install_subdirs)

