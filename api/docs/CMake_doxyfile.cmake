# **********************************************************
# Copyright (c) 2012-2022 Google, Inc.    All rights reserved.
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

# inputs:
# * srcdir
# * outfile
# * proj_srcdir
# * proj_bindir
# * version_number
# * header_dir
# * gendox_dir
# * dest_dir
# * DOXYGEN_EXECUTABLE
# * doxygen_ver
# * dox_extraN where N=1,2,3,... (to get around cmake problems passing spaces)
# * embeddable

set(outdir "${CMAKE_CURRENT_BINARY_DIR}")
set(srcdir_orig "${srcdir}")
set(outdir_orig "${outdir}")

# Support docs for Extensions (i#277/PR 540817)
file(GLOB dirs "${proj_srcdir}/ext/*/CMakeLists.txt")
foreach (dir ${dirs})
  get_filename_component(dir ${dir} PATH)
  set(ext_dirs ${ext_dirs} ${dir})
endforeach (dir)

# Add drcachesim dirs (i#2006).
set(ext_dirs ${ext_dirs} "${proj_bindir}/clients/include/drmemtrace")
# Add drpt2trace dirs
set(ext_dirs ${ext_dirs} "${proj_srcdir}/clients/drcachesim/drpt2trace")

include("${srcdir}/CMake_doxyutils.cmake")
set(input_paths srcdir proj_srcdir header_dir gendox_dir outdir)
doxygen_path_xform(${DOXYGEN_EXECUTABLE} "${input_paths}")

foreach (dir ${ext_dirs})
  doxygen_path_xform(${DOXYGEN_EXECUTABLE} dir)
  set(ext_input_dirs "${ext_input_dirs} \"${dir}\"")
endforeach (dir)

file(GLOB dirs "${proj_srcdir}/clients/*/CMakeLists.txt")
foreach (dir ${dirs})
  get_filename_component(dir ${dir} PATH)
  set(tool_dirs ${tool_dirs} ${dir})
endforeach (dir)

foreach (dir ${tool_dirs})
  doxygen_path_xform(${DOXYGEN_EXECUTABLE} dir)
  set(tool_input_dirs "${tool_input_dirs} \"${dir}\"")
endforeach (dir)

# Pull out the paths passed as separate dox_extraN vars
set(extra_cnt 1)
while (DEFINED dox_extra${extra_cnt})
  set(dir ${dox_extra${extra_cnt}})
  doxygen_path_xform(${DOXYGEN_EXECUTABLE} dir)
  set(extra_input_dirs "${extra_input_dirs} \"${dir}\"")
  math(EXPR extra_cnt "${extra_cnt} + 1")
endwhile ()

configure_file(${srcdir_orig}/API.doxy ${outfile} COPYONLY)
process_doxyfile(${outfile} ${DOXYGEN_EXECUTABLE} ${doxygen_ver})

file(READ "${outfile}" string)

# Be sure to quote ${string} to avoid interpretation (semicolons removed, etc.)
# i#113: be sure to quote all paths to handle spaces

# FIXME i#59: if epstopdf and latex are available, set "GENERATE_LATEX" to "YES"

# To control the ordering of the top-level pages (where we don't have \subpage
# to control ordering) we specify them explicitly in the order we want before
# the directory glob (doxygen seems ok with the glob duplicating).
# (Using a separate first-alphabetically (CMake globas are alphabetical) file
# that has duplicate \page refs in order works with doxygen 1.9.1 and 1.8.11,
# but not 1.8.17 or 1.8.19 on Windows.)
set(top_order "\"${srcdir}/download.dox\"")
set(top_order "${top_order} \"${gendox_dir}/tool_gendox.dox\"")
set(top_order "${top_order} \"${srcdir}/intro.dox\"")
set(top_order "${top_order} \"${srcdir}/help.dox\"")
set(top_order "${top_order} \"${srcdir}/developers.dox\"")
set(top_order "${top_order} \"${srcdir}/license.dox\"")

# Executed inside build dir, so we leave genimages alone
# and have to fix up refs to source dir and subdirs
string(REGEX REPLACE
  "(INPUT[ \t]*=) *\\."
  # We no longer need ${proj_srcdir}/libutil on here, right?
  "\\1 ${top_order} \"${srcdir}\" \"${header_dir}\" \"${gendox_dir}\" ${ext_input_dirs} ${tool_input_dirs} ${extra_input_dirs}"
  string "${string}")
string(REGEX REPLACE
  "([^a-z])images"
  "\\1\"${srcdir}/images\"" string "${string}")
string(REGEX REPLACE
  "(header.html)"
  "\"${dest_dir}/\\1\"" string "${string}")
string(REGEX REPLACE
  "(footer.html)"
  "\"${dest_dir}/\\1\"" string "${string}")

# If we end up with more of these we should generalize and pass them in
# rather than hardcoding here.
string(APPEND string
  "HTML_EXTRA_FILES=${proj_bindir}/clients/drcachesim/docs/new-features-encodings-seek.pdf")

if (doxygen_ver STRGREATER "1.7.2")
  # For 1.7.3+ we use xml file to tweak treeview contents
  string(REGEX REPLACE
    "(TREEVIEW_WIDTH[^\n]*)"
    "\\1\nLAYOUT_FILE = \"${gendox_dir}/DoxygenLayout.xml\"" string "${string}")
endif ()

string(REGEX REPLACE
  # if I don't quote ${string}, then this works: [^$]*$
  # . always seems to match newline, despite book's claims
  # xref cmake emails about regexp ^$ ignoring newlines: ugh!
  "(OUTPUT_DIRECTORY[ \t]*=)[^\n\r](\r?\n)"
  "\\1 \"${outdir}\"\\2" string "${string}")

string(REGEX REPLACE
  "DR_VERSION=X.Y.Z"
  "DR_VERSION=${version_number}" string "${string}")

# We discourage "\if linux" as we prefer to have the same documentation
# cover both platforms, rather than two different version.
# I'm leaving "linux" enabled though in case we do find a need for it.
string(REGEX REPLACE
  "(ENABLED_SECTIONS[ \t]*=)"
  "\\1 linux" string "${string}")

if (embeddable)
  # Turn off the frames.
  string(REGEX REPLACE "(GENERATE_TREEVIEW[ \t]*= )YES" "\\1 NO" string "${string}")
  # Turn off the confusing search box.
  string(REGEX REPLACE "(SEARCHENGINE[ \t]*= )YES" "\\1 NO" string "${string}")
endif ()

file(WRITE ${outfile} "${string}")

# Now update to latest doxygen.  Suppress warnings since they're misleading:
# they say "please run doxygen -u" but we're currently doing just that.
# Newer doxygen complains if header.html and footer.html are missing and we
# don't create those until CMake_rundoxygen.cmake.
file(WRITE header.html "placeholder")
file(WRITE footer.html "placeholder")
execute_process(COMMAND
  ${DOXYGEN_EXECUTABLE} -u
  RESULT_VARIABLE doxygen_u_result
  ERROR_VARIABLE doxygen_u_error
  OUTPUT_QUIET # suppress "Configuration file `Doxyfile' updated."
  )
# I want to see errors other than:
#  Warning: Tag `MAX_DOT_GRAPH_HEIGHT' at line 248 of file Doxyfile has become obsolete.
#  To avoid this warning please update your configuration file using "doxygen -u"
string(REGEX REPLACE
  "(^|(\r?\n))[^\r\n]*has become obsolete.\r?\n"
  "\\1" doxygen_u_error "${doxygen_u_error}")
string(REGEX REPLACE
  "(^|(\r?\n))[^\r\n]*using \"doxygen -u\"\r?\n"
  "\\1" doxygen_u_error "${doxygen_u_error}")
if (doxygen_u_result)
  message(FATAL_ERROR "${DOXYGEN_EXECUTABLE} -u failed: ${doxygen_u_error}")
endif (doxygen_u_result)
if (doxygen_u_error)
  message(FATAL_ERROR "${DOXYGEN_EXECUTABLE} -u failed: ${doxygen_u_error}")
endif (doxygen_u_error)
