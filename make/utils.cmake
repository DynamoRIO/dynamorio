## **********************************************************
## Copyright (c) 2012 Google, Inc.    All rights reserved.
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
      set(new_lflag "-Wl,-rpath='$ORIGIN/${relpath}'")
      get_target_property(lflags ${target} LINK_FLAGS)
      if (NOT lflags MATCHES "\\$ORIGIN/${relpath}")
        append_property_string(TARGET ${target} LINK_FLAGS "${new_lflag}")
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
    execute_process(COMMAND ${CMAKE_C_COMPILER} -Wl,--version
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
