# **********************************************************
# Copyright (c) 2012-2013 Google, Inc.    All rights reserved.
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

###########################################################################
# Usage:
# + Add a custom command to invoke this script in the docs build dir
# + Assumes a favicon.ico is present in the target build dir
#
# Args:
# + DOXYGEN_EXECUTABLE  : path to doxygen
# + doxygen_ver         : version of doxygen
# + version_number      : version number to put in the docs
# + module_string_long  : replacement text for Modules
# + module_string_short : replacement text for Module
# + files_string        : replacement text for Files; optional
# + structs_string      : replacement text for Data Structures; optional
# + home_url            : home page URL
# + home_title          : home page title
# + logo_imgfile        : image file with logo
#
###########################################################################

# Any quotes carry over and it's simplest to assume not there.
string(REPLACE "\"" "" module_string_long ${module_string_long})
string(REPLACE "\"" "" module_string_short ${module_string_short})
if (DEFINED files_string)
  string(REPLACE "\"" "" files_string ${files_string})
endif ()
if (DEFINED structs_string)
  string(REPLACE "\"" "" structs_string ${structs_string})
endif ()
string(REPLACE "\"" "" home_url ${home_url})
string(REPLACE "\"" "" home_title ${home_title})
string(REPLACE "\"" "" logo_imgfile ${logo_imgfile})

# First, generate header.html and footer.html suitable for current
# doxygen version and then tweak them.
# Note that we must generate a css file and we must name it doxygen.css
# or else we have to later copy it to html dir.
execute_process(COMMAND
  ${DOXYGEN_EXECUTABLE} -w html header.html footer.html doxygen.css
  RESULT_VARIABLE doxygen_w_result
  ERROR_VARIABLE doxygen_w_error
  OUTPUT_QUIET
  )
if (doxygen_w_result OR doxygen_w_error)
  message(FATAL_ERROR "*** ${DOXYGEN_EXECUTABLE} failed: ***\n${doxygen_w_error}")
endif (doxygen_w_result OR doxygen_w_error)

# Add favicon to header
file(READ ${CMAKE_CURRENT_BINARY_DIR}/header.html string)
string(REGEX REPLACE
  "<title>"
  "<link rel=\"shortcut icon\" type=\"image/x-icon\" href=\"favicon.ico\"/>\n<title>"
  string "${string}")
if (doxygen_ver VERSION_LESS "1.7.0")
  # For some reason older doxygen doesn't replace its own vars w/ custom header
  string(REGEX REPLACE "\\$relpath\\$" "" string "${string}")
endif ()
file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/header.html "${string}")

# Tweak footer to have version
file(READ ${CMAKE_CURRENT_BINARY_DIR}/footer.html string)
set(foot "<img border=0 src=\"favicon.png\"> &nbsp; ")
set(foot "${foot} $projectname version ${version_number} --- $datetime")
set(foot "${foot} &nbsp; <img border=0 src=\"favicon.png\">")
# Recent doxygen
string(REGEX REPLACE "\\$generated.*doxygenversion" "${foot}" string "${string}")
# Older doxygen
string(REGEX REPLACE "Generated.*doxygenversion" "${foot}" string "${string}")
# Prefer center to default float:right
string(REGEX REPLACE "class=\"footer\""
  "class=\"footer\" style=\"float:none;text-align:center\""
  string "${string}")
file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/footer.html "${string}")

if (doxygen_ver STRGREATER "1.7.2")
  # Create a layout file so we can control the treeview index
  execute_process(COMMAND
    ${DOXYGEN_EXECUTABLE} -l
    RESULT_VARIABLE doxygen_l_result
    ERROR_VARIABLE doxygen_l_error
    OUTPUT_QUIET
    )
  if (doxygen_l_result OR doxygen_l_error)
    message(FATAL_ERROR "*** ${DOXYGEN_EXECUTABLE} failed: ***\n${doxygen_l_error}")
  endif (doxygen_l_result OR doxygen_l_error)

  file(READ ${CMAKE_CURRENT_BINARY_DIR}/DoxygenLayout.xml string)
  # Rename "Modules" (done below for older doxygen)
  string(REGEX REPLACE
    "tab type=\"modules\" visible=\"yes\" title=\"\""
    "tab type=\"modules\" visible=\"yes\" title=\"${module_string_long}\""
    string "${string}")
  if (doxygen_ver STRGREATER "1.7.5")
    # Add link to home page (done below for older doxygen; skipped for 1.7.3-1.7.5)
    string(REGEX REPLACE
      "</navindex>"
      "<tab type=\"user\" url=\"${home_url}\" title=\"${home_title}\"/>\n</navindex>"
      string "${string}")
  endif ()
  file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/DoxygenLayout.xml "${string}")
endif ()

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

if (doxygen_ver VERSION_LESS "1.7.3")
  # Add link to home page to treeview pane
  file(APPEND
    ${CMAKE_CURRENT_BINARY_DIR}/html/tree.html
    "<p>&nbsp;<a target=\"main\" href=\"${home_url}/\">${home_title}</a></p>")

  # Add our logo to treeview pane (yes there is no closing </body></html>)
  file(APPEND
    ${CMAKE_CURRENT_BINARY_DIR}/html/tree.html
    "<p>&nbsp;<br><img src=\"${logo_imgfile}\"></p>")
else ()
  # This is a fragile insertion into the javascript used for the navbar.
  # We insert at the end of the final function (initNavTree).
  # This works in doxygen 1.7.4-1.8.1.
  set(add_logo "
        var imgdiv = document.createElement(\"div\");
        document.getElementById(\"nav-tree-contents\").appendChild(imgdiv);
        var img = document.createElement(\"img\");
        imgdiv.appendChild(img);
        img.src = \"${logo_imgfile}\";
        img.style.marginLeft = \"80px\";
        img.style.marginTop = \"30px\";
    ")
  file(READ ${CMAKE_CURRENT_BINARY_DIR}/html/navtree.js string)
  string(REGEX REPLACE
    "\\}\n*$"
    "${add_logo}\n}"
    string "${string}")
  # While we're here we replace Files and Data Structures, if requested
  if (DEFINED files_string)
    string(REGEX REPLACE "\"Files\"" "\"${files_string}\"" string "${string}")
  endif ()
  if (DEFINED structs_string)
    string(REGEX REPLACE "\"Data Structures\"" "\"${structs_string}\""
      string "${string}")
  endif ()
  file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/html/navtree.js "${string}")
endif ()

# For modules/extensions (i#277/PR 540817) we use doxygen groups which show up under
# top-level "Modules" which we rename here
if (doxygen_ver VERSION_LESS "1.7.3")
  file(READ ${CMAKE_CURRENT_BINARY_DIR}/html/tree.html string)
  string(REGEX REPLACE "Modules" "${module_string_long}" string "${string}")
  file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/html/tree.html "${string}")
endif ()
if (EXISTS ${CMAKE_CURRENT_BINARY_DIR}/html/modules.html)
  file(READ ${CMAKE_CURRENT_BINARY_DIR}/html/modules.html string)
  string(REGEX REPLACE "Module" "${module_string_short}" string "${string}")
  string(REGEX REPLACE "module" "${module_string_short}" string "${string}")
  file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/html/modules.html "${string}")
endif ()

# We do not copy samples or favicon for build dir: those are install rules
