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

# top-level CMakeLists.txt finds the packages we need

file(GLOB figures_fig *.fig)

set(genimg_dirname ${CMAKE_CURRENT_BINARY_DIR}/genimages)
file(MAKE_DIRECTORY ${genimg_dirname})

if (WIN32 AND "${ImageMagick_mogrify_EXECUTABLE}" MATCHES "cygwin")
  # i#100: cygwin mogrify mis-parses mixed paths when the drive is a,
  # b, c, or g (haven't tracked down why those particular letters):
  #   % mogrify -resize 33% -format png c:/client.tiff[0]
  #   mogrify: Must specify image size `/client.tiff'.
  string(REGEX REPLACE "^([a-zA-Z]):/" "/cygdrive/\\1/"
    genimg_dirname_IM "${genimg_dirname}")
else (WIN32 AND "${ImageMagick_mogrify_EXECUTABLE}" MATCHES "cygwin")
  set(genimg_dirname_IM "${genimg_dirname}")
endif (WIN32 AND "${ImageMagick_mogrify_EXECUTABLE}" MATCHES "cygwin")

# Because fig2dev does not perform anti-aliasing, converting directly
# to an image format can result in very poor quality lines and text.
# This problem is compounded if that image is subsequently resized
# without any anti-aliasing, such as by your web browser.  The
# solution is to magnify the vector data to at least 4x and then
# generate a lossless bitmap format such as TIFF; then have a real
# image manipulator resize it to the final size (cannot make
# browser-resizable vector graphics).
# See also http://www.burningcutlery.com/derek/bargraph/
foreach (fig ${figures_fig})
  get_filename_component(base ${fig} NAME_WE)
  add_custom_command(
    OUTPUT ${genimg_dirname}/${base}.eps
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/${base}.fig
    # fig2dev doesn't quote gs output path; set working dir to avoid spaces
    COMMAND ${FIG2DEV_EXECUTABLE}
    ARGS -L eps ${CMAKE_CURRENT_SOURCE_DIR}/${base}.fig ${base}.eps
    WORKING_DIRECTORY ${genimg_dirname}
    VERBATIM # recommended: p260
    )
  add_custom_command(
    OUTPUT ${genimg_dirname}/${base}.tiff
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/${base}.fig
    # fig2dev doesn't quote gs output path; set working dir to avoid spaces
    COMMAND ${FIG2DEV_EXECUTABLE}
    ARGS -L tiff -m 4 ${CMAKE_CURRENT_SOURCE_DIR}/${base}.fig ${base}.tiff
    WORKING_DIRECTORY ${genimg_dirname}
    VERBATIM # recommended: p260
    )
  add_custom_command(
    OUTPUT ${genimg_dirname}/${base}.png
    DEPENDS ${genimg_dirname}/${base}.tiff
    COMMAND ${ImageMagick_mogrify_EXECUTABLE}
    # fig2dev produces multi-part image so ask for just 1st
    ARGS -resize 33% -format png ${genimg_dirname_IM}/${base}.tiff[0]
    VERBATIM # recommended: p260
    )
  # FYI: for VMSAFE we did: $(SED) 's|DynamoRIO|${VMSAFE_NAME}|g'
  set(figures_png ${figures_png} ${genimg_dirname}/${base}.png)
  set(figures_eps ${figures_eps} ${genimg_dirname}/${base}.eps)
endforeach (fig)

set(doxyfile ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile)
add_custom_command(
  OUTPUT ${doxyfile}
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/API.doxy
          ${CMAKE_CURRENT_SOURCE_DIR}/CMake_doxyfile.cmake
  COMMAND ${CMAKE_COMMAND}
  # script does not inherit any vars so we must pass them all in
  # to work around i#84 be sure to put a space after -D for 1st arg at least
  ARGS -D srcdir=${CMAKE_CURRENT_SOURCE_DIR} -Doutfile=${doxyfile}
       -Dproj_srcdir=${PROJECT_SOURCE_DIR} -Dversion_number=${VERSION_NUMBER}
       -Dheader_dir=${BUILD_INCLUDE} -DDOXYGEN_EXECUTABLE=${DOXYGEN_EXECUTABLE}
       -P ${CMAKE_CURRENT_SOURCE_DIR}/CMake_doxyfile.cmake
  VERBATIM # recommended: p260
  )

set(footer ${CMAKE_CURRENT_BINARY_DIR}/footer.html)
add_custom_command(
  OUTPUT ${footer}
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/footer.html
          ${CMAKE_CURRENT_SOURCE_DIR}/CMake_footer.cmake
  COMMAND ${CMAKE_COMMAND}
  # to work around i#84 be sure to put a space after -D for 1st arg at least
  ARGS -D srcdir=${CMAKE_CURRENT_SOURCE_DIR} -Doutfile=${footer}
       -Dversion_number=${VERSION_NUMBER}
       -P ${CMAKE_CURRENT_SOURCE_DIR}/CMake_footer.cmake
  VERBATIM # recommended: p260
  )

set(docsgen ${CMAKE_CURRENT_BINARY_DIR}/html/index.html)
file(GLOB dox_files *.dox)
# we depend on the samples even though we don't copy them,
# to have them as dependencies for the install rules
file(GLOB samples_files ../samples/*.c ../samples/Makefile)
file(GLOB header_files  ${BUILD_INCLUDE}/*.h)
add_custom_command(
  OUTPUT ${docsgen} # among other files
  DEPENDS ${dox_files}
          ${header_files}
          ${CMAKE_CURRENT_SOURCE_DIR}/header.html
          ${CMAKE_CURRENT_SOURCE_DIR}/htmlstyle.css
          ${footer}
          ${doxyfile}
          ${figures_eps}
          ${figures_png}
          ${CMAKE_CURRENT_SOURCE_DIR}/CMake_rundoxygen.cmake
          ${samples_files}
  # We want doxygen to fail if it has warnings like "can't find header
  # files" so we run it through a script
  COMMAND ${CMAKE_COMMAND}
  # Have to quote dox_files to keep ;-separated
  # to work around i#84 be sure to put a space after -D for 1st arg at least
  ARGS -D srcdir=${CMAKE_CURRENT_SOURCE_DIR} "-Ddox_files=${dox_files}"
       -DDOXYGEN_EXECUTABLE=${DOXYGEN_EXECUTABLE}
       -P ${CMAKE_CURRENT_SOURCE_DIR}/CMake_rundoxygen.cmake
  VERBATIM # recommended: p260
  )
set_directory_properties(PROPERTIES
  ADDITIONAL_MAKE_CLEAN_FILES "html;latex;rtf")

add_custom_target(htmldocs ALL
  DEPENDS ${doxyfile}
          ${footer}
          ${figures_eps}
          ${figures_png}
          ${docsgen}
  )
# add_custom_target DEPENDS cannot take targets!
add_dependencies(htmldocs api_headers)

# FIXME i#59: if epstopdf and latex are available, add optional pdf
# target that invokes fix-latex.pl and install rule.
# Need to copy drlogo-16x16.png to latex/drlogo.png after i#80 changes.

###########################################################################
# install rules

install(DIRECTORY
  ${CMAKE_CURRENT_BINARY_DIR}/html
  DESTINATION ${INSTALL_DOCS}
  )

# IE for some reason won't initially load an .ico in a web page from a server,
# though will show it via file: or after pointing straight at image from server.
# doxygen fails to copy image referenced only in footer.html
install(FILES
  ${CMAKE_CURRENT_SOURCE_DIR}/images/favicon.png
  DESTINATION ${INSTALL_DOCS}/html
  )

# We no longer support VMSAFE, where we deleted stats.c and tracedump.c
# and copied MF_moduledb-sample.config and VIPA_test.exe
string(REGEX REPLACE
  # list of files is ;-separated
  "[^;]*MF_moduledb.c"
  "" samples_tocopy "${samples_files}")
if (UNIX)
  # no stats sample on linux
  string(REGEX REPLACE
    "[^;]*stats.c"
    "" samples_tocopy "${samples_tocopy}")
endif (UNIX)
install(FILES
  ${samples_tocopy}
  DESTINATION ${INSTALL_DOCS}/html/samples
)

