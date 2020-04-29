# **********************************************************
# Copyright (c) 2012-2020 Google, Inc.    All rights reserved.
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

###########################################################################
# Usage:
#
# 1) Run check_doxygen_version() at configure time
# 2) Add a custom command to update the Doxyfile that calls
#    process_doxyfile(), using a path obtained from
#    doxygen_path()
# 3) Add a custom command that calls docs_rundoxygen.cmake
#
###########################################################################

# If doxygen_ver is blank, determines the version and returns it in ver_out.
# Prints a warning or a fatal error on an unsupported version.
function (check_doxygen_version DOXYGEN_EXECUTABLE doxygen_ver ver_out)
  # older versions of FindDoxygen.cmake don't set DOXYGEN_VERSION
  if (NOT doxygen_ver)
    execute_process(COMMAND
      ${DOXYGEN_EXECUTABLE} --version
      RESULT_VARIABLE doxygen_v_result
      ERROR_VARIABLE doxygen_v_error
      OUTPUT_VARIABLE doxygen_v_out
      )
    string(REGEX MATCH "^[0-9]*\\.[0-9]*\\.[0-9]*" doxygen_ver "${doxygen_v_out}")
  endif ()
  if (doxygen_ver STREQUAL "1.8.0" OR
      doxygen_ver STREQUAL "1.7.3")
    # 1.8.0 fails to build (i#814) while 1.7.3 doesn't have the navbar for some reason.
    message(FATAL_ERROR "doxygen version ${doxygen_ver} is not supported.  "
      "Use an earlier or later version.")
  endif ()
  if (doxygen_ver MATCHES "^1.7.[3-6]")
    # These versions have a "Related Pages" index which we don't want.
    # Plus, 1.7.3-1.7.5 won't have a home page link on the navbar.
    message(STATUS "WARNING: doxygen versions 1.7.3-1.7.6 will not produce "
      "release-quality documentation.")
  endif ()
  set(${ver_out} ${doxygen_ver} PARENT_SCOPE)
endfunction (check_doxygen_version)

# Transforms each variable in list_of_path_vars in-place to become
# a path suitable for passing to DOXYGEN_EXECUTABLE.
function (doxygen_path_xform DOXYGEN_EXECUTABLE list_of_path_vars)
  if (WIN32)
    # We can't rely on the presence of "cygwin" in the path to indicate this
    # is a cygwin executable, as some users place native-Windows apps in
    # c:/cygwin/usr/local/bin.  We thus try to write to a cygwin path.
    # Doxygen will happily clobber if our temp file already exists.
    execute_process(COMMAND
      ${DOXYGEN_EXECUTABLE} -g /tmp/_test_doxygen_.dox
      RESULT_VARIABLE doxygen_query_result
      ERROR_VARIABLE doxygen_query_error
      OUTPUT_VARIABLE doxygen_query_out
      )
    if (NOT doxygen_query_result)
      # Cygwin doxygen cannot handle mixed paths!
      #    *** E:/cygwin/bin/doxygen.exe failed: ***
      #    Failed to open temporary file
      #    /d/derek/opensource/dynamorio/build/api/docs/D:/derek/opensource/dynamorio/build/api/docs/doxygen_objdb_3156.tmp
      # We're using native windows cmake, so
      #   file(TO_CMAKE_PATH) => mixed, file(TO_NATIVE_PATH) => windows
      # and thus we invoke cygpath.  But, caller should keep the
      # original path to use for passing to process_doxyfile().
      find_program(CYGPATH cygpath)
      if (NOT CYGPATH)
        message(FATAL_ERROR "cannot find cygpath: thus cannot use cygwin doxygen")
      endif ()
      foreach (var ${list_of_path_vars})
        execute_process(COMMAND
          ${CYGPATH} -u "${${var}}"
          RESULT_VARIABLE cygpath_result
          ERROR_VARIABLE cygpath_err
          OUTPUT_VARIABLE ${var}
          )
        if (cygpath_result OR cygpath_err)
          message(FATAL_ERROR "*** ${CYGPATH} failed: ***\n${cygpath_err}")
        endif ()
        string(REGEX REPLACE "[\r\n]" "" ${var} "${${var}}")
        set(${var} ${${var}} PARENT_SCOPE)
      endforeach ()
    endif ()
  endif ()
endfunction (doxygen_path_xform)

# Modifies doxyfile to work with docs_rundoxygen.cmake
function (process_doxyfile doxyfile DOXYGEN_EXECUTABLE doxygen_ver)
  get_filename_component(outdir ${doxyfile} PATH)

  file(READ ${doxyfile} string)

  string(REGEX REPLACE
    "(header.html)"
    "\"${outdir}/\\1\"" string "${string}")

  if (doxygen_ver STRGREATER "1.7.2")
    # For 1.7.3+ we use xml file to tweak treeview contents
    string(REGEX REPLACE
      "(TREEVIEW_WIDTH[^\n]*)"
      "\\1\nLAYOUT_FILE = \"${outdir}/DoxygenLayout.xml\"" string "${string}")
  endif ()
endfunction (process_doxyfile)
