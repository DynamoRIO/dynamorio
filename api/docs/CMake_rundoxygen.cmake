# **********************************************************
# Copyright (c) 2012-2021 Google, Inc.    All rights reserved.
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
# + embeddable          : whether to edit content for jekyll embedding
# + proj_srcdir         : project base source dir, for editing titles
# + proj_bindir         : project base binary dir, for editing titles
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
if (embeddable)
  # Add jekyll header.
  set(string "---
title: \"$title\"
layout: default
---
${string}")
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

if (embeddable)
  # We need to make some edits for insertion into our jekyll site.
  file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/html/keywords.js "---\nlayout: null\n---\n")
  file(GLOB all_html ${CMAKE_CURRENT_BINARY_DIR}/html/*.html)
  foreach (html ${all_html})
    file(READ ${html} string)
    # Remove headers and body tag.
    string(REGEX REPLACE "<!-- HTML header.*<body>\n" "" string "${string}")
    # Remove body and html close tags.
    string(REGEX REPLACE "</body>.*</html>\n" "" string "${string}")
    # Remove full paths.
    string(REGEX REPLACE "\ntitle: \"${proj_bindir}" "\ntitle: \"" string "${string}")
    string(REGEX REPLACE "\ntitle: \"${proj_srcdir}" "\ntitle: \"" string "${string}")
    # Add permalink to front matter so the web site hides the docs/ subdir.
    get_filename_component(linkname ${html} NAME)
    string(REGEX REPLACE "(\nlayout: default\n)" "\\1permalink: /${linkname}\n"
      string "${string}")
    # Our images are in an images/ subdir.
    string(REGEX REPLACE "<img src=\"" "<img src=\"/images/" string "${string}")
    string(REGEX REPLACE
        "<object type=\"image/svg\\+xml\" data=\""
        "<object type=\"image/svg+xml\" data=\"/images/"
        string "${string}")
    # Ensure that strings that look like Jekyll tags are not interpreted as such.
    # We add the Jekyll raw-endraw tags around "{%" and "%}" separately. We use
    # a random suffix for jekyll_raw_tag and jekyll_endraw_tag to reduce the
    # possibility of a conflict with an actual use of these strings.
    string(REGEX REPLACE "{%" "jekyll_raw_tag_f00d{%jekyll_endraw_tag_f00d" string "${string}")
    string(REGEX REPLACE "%}" "jekyll_raw_tag_f00d%}jekyll_endraw_tag_f00d" string "${string}")
    string(REGEX REPLACE "jekyll_raw_tag_f00d" "{% raw %}" string "${string}")
    string(REGEX REPLACE "jekyll_endraw_tag_f00d" "{% endraw %}" string "${string}")

    # Collect type info for keyword searches with direct anchor links (else the
    # search finds the page but the user then has to manually search within the long
    # page -- and there are no doygen options to split each onto its own page).
    # The format here is the javascript object used by search.js to set up lunr.
    # XXX: This is fragile and highly dependent on the precise doxygen output.
    get_filename_component(fname ${html} NAME_WE)
    # The ;'s mess up the MATCHALL results so we remove them.
    string(REPLACE ";" "_" nosemis "${string}")
    string(REGEX MATCHALL "<tr class=\"memitem[^\n]*</tr>" types "${nosemis}")
    foreach (type ${types})
      string(REGEX MATCH "href=\"[^\"]+\"" url "${type}")
      string(REGEX REPLACE "href=\"([^\"]+)\"" "\\1" url "${url}")
      string(REGEX MATCH "\">[_a-zA-Z0-9]+</a>" name "${type}")
      string(REGEX REPLACE "\">([_a-zA-Z0-9]+)</a>" "\\1" name "${name}")
      if (name STREQUAL "")
        continue ()
      endif ()
      # Support searching by partial names.
      set(extra "")
      string(REPLACE "_" " " separate "${name}")
      if (NOT separate STREQUAL name)
        set(extra "${extra} ${separate}")
      endif ()
      set(prefix "${name}")
      while (prefix MATCHES "[^_]_[^_]")
        string(REGEX REPLACE "_[^_]*$" "" prefix "${prefix}")
        set(extra "${extra} ${prefix}")
      endwhile ()
      set(keywords "window.data[\"${fname}-${name}\"]={\
\"name\":\"${fname}-${name}\",\
\"title\":\"${name} in ${fname} header\",\
\"url\":\"${url}\",\
\"content\":\"${name} ${extra}\"};\n")
      file(APPEND ${CMAKE_CURRENT_BINARY_DIR}/html/keywords.js "${keywords}")
    endforeach ()
    # Similarly, put in explicit search entries for sections so searches go
    # straight to the anchor link.
    string(REGEX MATCHALL "\n<h1><a class=\"anchor\" id=\"[^\"]*\"></a>\n[^\n]+</h1>"
      sections "${nosemis}")
    foreach (section ${sections})
      string(REGEX REPLACE ".* id=\"([^\"]+)\".*" "\\1" id "${section}")
      string(REGEX REPLACE ".*</a>\n([^<]+)</h1>" "\\1" name "${section}")
      if (name STREQUAL "" OR id STREQUAL "" OR name MATCHES "<")
        message(FATAL_ERROR "Failed to find section name or anchor: ${section}")
      endif ()
      set(url "/${fname}.html#${id}")
      # Quotes cause JavaScript syntax errors.
      string(REPLACE "\"" "" name "${name}")
      # Spaces would be ok but it feels nicer w/o them in the key.
      string(REPLACE " " "_" key "${name}")
      set(keywords "window.data[\"${fname}-${key}\"]={\
\"name\":\"${fname}-${key}\",\
\"title\":\"${name}\",\
\"url\":\"${url}\",\
\"content\":\"${name}\"};\n")
      file(APPEND ${CMAKE_CURRENT_BINARY_DIR}/html/keywords.js "${keywords}")
    endforeach ()

    file(WRITE ${html} "${string}")
  endforeach ()

  # We leverage the javascript menu from the non-embedded version.
  file(GLOB all_js ${CMAKE_CURRENT_BINARY_DIR}/../html/*.js)
  set(found_user_docs OFF)
  foreach (js ${all_js})
    if (js MATCHES "navtree.js" OR
        js MATCHES "resize.js" OR
        js MATCHES "navtreeindex")
      continue ()
    endif ()
    file(READ ${js} string)
    # Ask jekyll to inline, instead of just naming for JS.
    string(REGEX REPLACE ", \"([^\"]+)\" \\]" ", {% include_relative \\1.js %} ]"
      string "${string}")
    if (js MATCHES "navtreedata.js")
      # Add a jekyll header.
      set(string "---\npermalink: /navtreedata.js\n---\n${string}")
      # Remove the index array from navtreedata.js.
      string(REGEX REPLACE "var NAVTREEINDEX =[^;]*;" "" string "${string}")
      # Remove one layer from navtreedata.js.
      string(REGEX REPLACE "\n *\\[ \"DynamoRIO\", \"index.html\", \\[" ""
        string "${string}")
      # Remove the home page at the end.
      string(REGEX REPLACE "\n[^\n]*DynamoRIO Home Page[^\n]*\n" "" string "${string}")
      # Insert a layer in navtreedata.js, using the end of the top layer we removed.
      string(REGEX REPLACE "\n( *\\[ \"Deprecated List)"
        "\n[ \"API Reference\", \"files.html\", [\n\\1"
        string "${string}")
      if (NOT string MATCHES "Extension API")
        message(FATAL_ERROR "Cannot find menu entry for page \"Extension API\"")
      endif ()
      string(REGEX MATCH "\n[^\n]+\"Extension API\"[^\n]+\n" ext_entry "${string}")
      string(REPLACE "${ext_entry}" "\n" string "${string}")
    else ()
      # Remove name so we can inline.
      string(REGEX REPLACE "var [^\n]* =\n" "" string "${string}")
      # End in a comma for inlining.
      string(REGEX REPLACE "\\];" "]," string "${string}")
      # Update directory index names which are hashes that differ between the
      # treeview and embed generations.
      string(REGEX MATCHALL "\"[a-zA-Z_0-9]+\", \"dir_[a-f0-9]+\\.html\""
        dir_hashes "${string}")
      foreach (dir_hash ${dir_hashes})
        string(REGEX REPLACE "\"([a-zA-Z_0-9]+)\".*" "\\1" key "${dir_hash}")
        # We go search the dir_*.html embed files looking for key.
        file(GLOB dir_html ${CMAKE_CURRENT_BINARY_DIR}/html/dir_*.html)
        set(found_hash OFF)
        foreach (html ${dir_html})
          file(READ ${html} html_string)
          if (html_string MATCHES "${key} Directory Reference")
            get_filename_component(fname ${html} NAME)
            string(REGEX REPLACE "\"${key}\", \"dir_[a-f0-9]+\\.html\""
              "\"${key}\", \"${fname}\"" string "${string}")
            set(found_hash ON)
            break ()
          endif ()
        endforeach ()
        if (NOT found_hash)
          message(FATAL_ERROR "Cannot find corresonding page for ${key}")
        endif ()
      endforeach ()
    endif ()
    if (js MATCHES "page_user_docs.js")
      # CMake 3.6+ guarantees the glob is sorted lexicographically, so we've already
      # seen navtreedata.js.
      set(found_user_docs ON)
      if (ext_entry)
        if (NOT string MATCHES "Disassembly Library")
          message(FATAL_ERROR "Cannot find menu entry for page \"Disassembly Library\"")
        endif ()
        string(REGEX REPLACE "\n([^\n]+\"Disassembly Library)" "${ext_entry}\\1"
          string "${string}")
      else ()
        message(FATAL_ERROR "Failed to find the menu entries to move")
      endif ()
    endif ()
    # Put the modified contents into our dir.
    get_filename_component(fname ${js} NAME)
    file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/html/${fname} "${string}")
  endforeach ()
  if (NOT found_user_docs)
    message(FATAL_ERROR "Cannot find \"page_user_docs\" menu file")
  endif ()

else ()
  # Edit navbar.
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
    # Doxygen 1.9.1 has this license-end line.
    string(REGEX REPLACE
      "\\}\n/\\* @license-end \\*/\n$"
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
