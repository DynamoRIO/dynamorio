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

# inputs:
# * srcdir
# * outfile
# * proj_srcdir
# * version_number
# * header_dir
# * DOXYGEN_EXECUTABLE

set(outdir "${CMAKE_CURRENT_BINARY_DIR}")

file(READ ${srcdir}/API.doxy string)

string(REGEX MATCH "cygwin" is_cygwin "${DOXYGEN_EXECUTABLE}")
if (is_cygwin)
  # cygwin doxygen cannot handle mixed paths!
  #    *** E:/cygwin/bin/doxygen.exe failed: ***
  #    Failed to open temporary file
  #    /d/derek/opensource/dynamorio/build/api/docs/D:/derek/opensource/dynamorio/build/api/docs/doxygen_objdb_3156.tmp
  # we're using native windows cmake, so
  #   file(TO_CMAKE_PATH) => mixed, file(TO_NATIVE_PATH) => windows
  # thus we invoke cygpath, but after we've read in API.doxy so we can
  # now clobber srcdir w/ a cygwin path.
  find_program(CYGPATH cygpath)
  if (NOT CYGPATH)
    message(FATAL_ERROR "cannot find cygpath: thus cannot use cygwin doxygen")
  endif (NOT CYGPATH)
  set(input_paths srcdir proj_srcdir header_dir outdir)
  foreach (var ${input_paths})
    execute_process(COMMAND
      ${CYGPATH} -u "${${var}}"
      RESULT_VARIABLE cygpath_result
      ERROR_VARIABLE cygpath_err
      OUTPUT_VARIABLE ${var}
      )
    if (cygpath_result OR cygpath_err)
      message(FATAL_ERROR "*** ${CYGPATH} failed: ***\n${cygpath_err}")
    endif (cygpath_result OR cygpath_err)
    string(REGEX REPLACE "[\r\n]" "" ${var} "${${var}}")
  endforeach (var)
endif (is_cygwin)

# Be sure to quote ${string} to avoid interpretation (semicolons removed, etc.)

# FIXME i#59: if epstopdf and latex are available, set "GENERATE_LATEX" to "YES"

# Executed inside build dir, so we leave genimages and footer.html alone
# and have to fix up refs to source dir and subdirs
string(REGEX REPLACE
  "(INPUT[ \t]*=) *\\."
  # We no longer need ${proj_srcdir}/libutil on here, right?
  "\\1 ${srcdir} ${header_dir}" string "${string}")
string(REGEX REPLACE
  "([^a-z])images"
  "\\1${srcdir}/images" string "${string}")
string(REGEX REPLACE
  "(header.html)"
  "${srcdir}/\\1" string "${string}")
string(REGEX REPLACE
  "(htmlstyle.css)"
  "${srcdir}/\\1" string "${string}")

string(REGEX REPLACE
  # if I don't quote ${string}, then this works: [^$]*$
  # . always seems to match newline, despite book's claims
  # xref cmake emails about regexp ^$ ignoring newlines: ugh!
  "(OUTPUT_DIRECTORY[ \t]*=)[^\n\r](\r?\n)"
  "\\1 ${outdir}\\2" string "${string}")

string(REGEX REPLACE
  "DR_VERSION=X.Y.Z"
  "DR_VERSION=${version_number}" string "${string}")

string(REGEX REPLACE
  "(ENABLED_SECTIONS[ \t]*=)"
  "\\1 linux" string "${string}")

# We no longer support VMSAFE.
# Here's what we used to do:
#   ifdef VMSAFE
#   	$(SED) -i 's/\(PROJECT_NAME[ \t]*=\).*/\1 "VMsafe In-Process API"/' $@
#   	$(SED) -i 's/\(ENABLED_SECTIONS[ \t]*=\)/\1 vmsafe/' $@
#   	$(SED) -i 's/DynamoRIO=DynamoRIO/DynamoRIO=${VMSAFE_NAME}/' $@
#   	$(SED) -i 's/client=client/client=${VMSAFE_CLIENT}/' $@
#   	$(SED) -i 's/clients=clients/clients=${VMSAFE_CLIENT}s/' $@
#   	$(SED) -i 's/Client=Client/Client=${VMSAFE_CLIENT}/' $@
#   	$(SED) -i 's/Clients=Clients/Clients=${VMSAFE_CLIENT}s/' $@
#   else
#   	$(SED) -i 's/\(ENABLED_SECTIONS[ \t]*=\)/\1 linux/' $@
#   endif

file(WRITE ${outfile} "${string}")

# Now update to latest doxygen.  Suppress warnings since they're misleading:
# they say "please run doxygen -u" but we're currently doing just that.
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
