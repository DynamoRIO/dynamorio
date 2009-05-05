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

# We don't really support VMSAFE docs but we may as well be macro-clean
foreach (dox ${dox_files})
  file(READ ${dox} string)
  # Exceptions inside \code
  string(REGEX REPLACE "find_package\(DynamoRIO\)" "" string "${string}")
  string(REGEX REPLACE "ERROR \"DynamoRIO" "" string "${string}")
  # These are case-sensitive, to allow function names and macros
  string(REGEX MATCH
    "(^|[^\\/\(])DynamoRIO[^-A-Za-z_]"
    bad_dr "${string}")
  string(REGEX MATCH
    "(^|[^-_\\/a-z])client($|[^_])"
    bad_client "${string}")
  if (bad_dr OR bad_client)
    message(FATAL_ERROR 
      "In ${dox}: use macro \\DynamoRIO and \\client to allow name substitution")
  endif (bad_dr OR bad_client)
endforeach (dox)

# Run doxygen and fail on warnings.
# Now update to latest doxygen.  Suppress warnings since they're misleading:
# they say "please run doxygen -u" but we're currently doing just that.
execute_process(COMMAND
  ${DOXYGEN_EXECUTABLE}
  RESULT_VARIABLE doxygen_u_result
  ERROR_VARIABLE doxygen_u_error
  OUTPUT_QUIET
  )
if (doxygen_u_result OR doxygen_u_error)
  message(FATAL_ERROR "*** ${DOXYGEN_EXECUTABLE} failed: ***\n${doxygen_u_error}")
endif (doxygen_u_result OR doxygen_u_error)

# Doxygen does not put header.html on the top-level frame so we add favicon manually
file(READ ${CMAKE_CURRENT_BINARY_DIR}/html/index.html string)
string(REGEX REPLACE
  "<title>"
  "<link rel=\"shortcut icon\" type=\"image/x-icon\" href=\"favicon.ico\"/>\n<title>"
  string "${string}")
file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/html/index.html "${string}")

# Add link to home page to treeview pane
file(APPEND 
  ${CMAKE_CURRENT_BINARY_DIR}/html/tree.html 
  "<p>&nbsp;<a target=\"main\" href=\"http://code.google.com/p/dynamorio/\">DynamoRIO Home Page</a></p>")

# Add our logo to treeview pane (yes there is no closing </body></html>)
file(APPEND 
  ${CMAKE_CURRENT_BINARY_DIR}/html/tree.html 
  "<p>&nbsp;<br><img src=\"drlogo.png\"></p>")

# Workaround for doxygen bug
configure_file(
  ${CMAKE_CURRENT_BINARY_DIR}/html/htmlstyle.css
  ${CMAKE_CURRENT_BINARY_DIR}/html/doxygen.css
  COPYONLY)

# We do not copy samples or favicon for build dir: those are install rules
