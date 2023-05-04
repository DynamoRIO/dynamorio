# **********************************************************
# Copyright (c) 2010-2021 Google, Inc.    All rights reserved.
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

# We used to transform our .fig figures from FIG format to PNG or EPS as
# part of the build process, using fig2dev, ImageMagick's mogrify, and
# ghostscript.  However, we have not added new figures in years, and it is
# painful to install and work with these packages on Windows (i#3235).
# We thus instead require figures to have a checked-in PNG or EPS file.

# Notes for new FIG-based figures:
# Because fig2dev does not perform anti-aliasing, converting directly
# to an image format can result in very poor quality lines and text.
# This problem is compounded if that image is subsequently resized
# without any anti-aliasing, such as by your web browser.  The
# solution is to magnify the vector data to at least 4x and then
# generate a lossless bitmap format such as TIFF; then have a real
# image manipulator resize it to the final size (cannot make
# browser-resizable vector graphics).
# See also http://www.burningcutlery.com/derek/bargraph/

file(GLOB figures_fig *.fig)

set(genimg_dirname ${CMAKE_CURRENT_BINARY_DIR}/genimages)
# This will create the genimages/ subdir for us and populate it:
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/genimages DESTINATION ${CMAKE_CURRENT_BINARY_DIR})

function (gendox_to_dox gendox_in gendox_out path master_list)
  file(GLOB_RECURSE dox_glob "${path}/*.dox" "${path}/*.dox.in")
  set(${master_list} ${${master_list}};${dox_files} PARENT_SCOPE)
  foreach (dox_file ${dox_glob})
    file(READ "${dox_file}" dox_string)
    string(REGEX MATCHALL "\n\\\\page page_[^ ]+" page "${dox_string}")
    set(dox_pages ${dox_pages} ${page})
  endforeach (dox_file)
  foreach (page ${dox_pages})
    string(REGEX REPLACE "\n\\\\page " "" pgname "${page}")
    set(dox_gen_toc "${dox_gen_toc}- \\\\subpage ${pgname}\n")
  endforeach (page)
  file(READ "${CMAKE_CURRENT_SOURCE_DIR}/${gendox_in}.gendox" gendox)
  string(REGEX REPLACE REPLACE_WITH_GENDOX_SUBPAGES "${dox_gen_toc}" gendox "${gendox}")
  file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/${gendox_out}.dox" "${gendox}")
endfunction (gendox_to_dox)

# we want docs for Extensions (i#277/PR 540817) to be subpages but we
# don't want to edit a top-level page to add new ones so we generate it
file(GLOB dox_files *.dox)
gendox_to_dox("ext" "ext_gendox" "${PROJECT_SOURCE_DIR}/ext" dox_files)
gendox_to_dox("tool" "tool_gendox" "${PROJECT_SOURCE_DIR}/clients" dox_files)

# determine doxygen version
include(CMake_doxyutils.cmake)
check_doxygen_version(${DOXYGEN_EXECUTABLE} "${DOXYGEN_VERSION}" doxygen_ver)

get_property(dox_extras GLOBAL PROPERTY DynamoRIO_dox_extras)
# It is a pain to try and pass a cmake list as a single cmdline arg as cmake
# does not seem to let us pass extra quotes or escape spaces.
# Thus we pass separate args with number prefixes.
# XXX: does this same cmake limitation mean a path w/ spaces will fail?
set(doxy_extra_args "")
set(extra_cnt 0)
foreach (extra ${dox_extras})
  math(EXPR extra_cnt "${extra_cnt} + 1")
  set(doxy_extra_args ${doxy_extra_args} -Ddox_extra${extra_cnt}=${extra})
endforeach ()

macro (generate_html name dest_dir embeddable)
  set(doxyfile ${dest_dir}/Doxyfile)
  add_custom_command(
    OUTPUT ${doxyfile}
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/API.doxy
            ${CMAKE_CURRENT_SOURCE_DIR}/CMake_doxyfile.cmake
            ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
    COMMAND ${CMAKE_COMMAND}
    # script does not inherit any vars so we must pass them all in
    # to work around i#84 be sure to put a space after -D for 1st arg at least
    ARGS -D srcdir=${CMAKE_CURRENT_SOURCE_DIR} -Doutfile=${doxyfile}
         -Dproj_srcdir=${PROJECT_SOURCE_DIR} -Dversion_number=${VERSION_NUMBER}
         -Dproj_bindir=${PROJECT_BINARY_DIR} -Dembeddable=${embeddable}
         -Dheader_dir=${BUILD_INCLUDE} -Dgendox_dir=${CMAKE_CURRENT_BINARY_DIR}
         -Ddest_dir=${dest_dir}
         -DDOXYGEN_EXECUTABLE=${DOXYGEN_EXECUTABLE}
         -Ddoxygen_ver=${doxygen_ver}
         ${doxy_extra_args}
         -P ${CMAKE_CURRENT_SOURCE_DIR}/CMake_doxyfile.cmake
    WORKING_DIRECTORY ${dest_dir}
    VERBATIM # recommended: p260
    )

  set(docsgen ${dest_dir}/html/index.html)
  get_property(dox_tgts GLOBAL PROPERTY DynamoRIO_dox_targets)

  # we depend on the samples even though we don't copy them,
  # to have them as dependencies for the install rules
  file(GLOB samples_files ../samples/*.c ../samples/Makefile)
  file(GLOB header_files  ${BUILD_INCLUDE}/*.h)
  add_custom_command(
    OUTPUT ${docsgen} # among other files
    DEPENDS ${dox_files}
            ${dox_tgts}
            ${dox_extras}
            ${header_files}
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
    ARGS -D DOXYGEN_EXECUTABLE=${DOXYGEN_EXECUTABLE}
         -Ddoxygen_ver=${doxygen_ver}
         -Dversion_number=${VERSION_NUMBER}
         -Dmodule_string_long="DynamoRIO Extension Details"
         -Dmodule_string_short="Extension"
         -Dhome_url="http://www.dynamorio.org"
         -Dhome_title="DynamoRIO Home Page"
         -Dlogo_imgfile="drlogo.png"
         -Dembeddable=${embeddable}
         -Dproj_srcdir=${PROJECT_SOURCE_DIR}
         -Dproj_bindir=${PROJECT_BINARY_DIR}
         -P ${CMAKE_CURRENT_SOURCE_DIR}/CMake_rundoxygen.cmake
    WORKING_DIRECTORY ${dest_dir}
    VERBATIM # recommended: p260
    )
  set_directory_properties(PROPERTIES
    ADDITIONAL_MAKE_CLEAN_FILES "html;latex;rtf")

  add_custom_target(${name} ALL
    DEPENDS ${doxyfile}
            ${dox_files}
            ${figures_eps}
            ${figures_png}
            ${docsgen}
    )
  # add_custom_target DEPENDS cannot take targets!
  add_dependencies(${name} api_headers)

  configure_file(${CMAKE_CURRENT_SOURCE_DIR}/images/favicon.png
    ${dest_dir}/html/favicon.png
    COPYONLY)
endmacro ()

generate_html(htmldocs ${CMAKE_CURRENT_BINARY_DIR} OFF)

# We create two output versions of the docs:
# 1) A standalone html version for local viewing.
# 2) A stripped-down version suitable for embedding inside dynamorio.org.
# Since we only need the stripped-down version built on one platform,
# we limit it to UNIX so we can use symlinks.
if (UNIX)
  set(embed_outdir ${CMAKE_CURRENT_BINARY_DIR}/embed)
  file(MAKE_DIRECTORY ${embed_outdir})
  execute_process(COMMAND ${CMAKE_COMMAND} -E create_symlink
    ${genimg_dirname} ${embed_outdir}/genimages
    RESULT_VARIABLE ln_result ERROR_VARIABLE ln_err)
  if (ln_result OR ln_err)
    message(FATAL_ERROR "*** genimages symlink failed: ***\n${ln_err}")
  endif ()
  generate_html(embed_docs ${embed_outdir} ON)
  # We must build the treeview version first, as we use its menu files.
  add_dependencies(embed_docs htmldocs)
endif ()

# TODO i#59: if epstopdf and latex are available, add optional pdf
# target that invokes fix-latex.pl and install rule.
# Need to copy drlogo-16x16.png to latex/drlogo.png after i#80 changes.

###########################################################################
# install rules

DR_install(DIRECTORY
  ${CMAKE_CURRENT_BINARY_DIR}/html
  DESTINATION ${INSTALL_DOCS}
  )

if (UNIX)
  DR_install(DIRECTORY
    ${CMAKE_CURRENT_BINARY_DIR}/embed/html
    DESTINATION ${INSTALL_DOCS_EMBED}
    )
endif ()

# IE for some reason won't initially load an .ico in a web page from a server,
# though will show it via file: or after pointing straight at image from server.
# doxygen fails to copy image referenced only in footer.html
DR_install(FILES
  ${CMAKE_CURRENT_SOURCE_DIR}/images/favicon.png
  DESTINATION ${INSTALL_DOCS}/html
  )

