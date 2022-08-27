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

cmake_minimum_required(VERSION 3.7)

include(../../make/policies.cmake NO_POLICY_SCOPE)

# We do not need libc, and we avoid a 10x size increase in both our
# dll and pdb (plus we avoid stressing private library isolation) by
# not using it (i#714).
set(DynamoRIO_USE_LIBC OFF)

# add drcov client
add_library(drcov SHARED drcov.c)
configure_DynamoRIO_client(drcov)
use_DynamoRIO_extension(drcov drcovlib)
use_DynamoRIO_extension(drcov drx)
# We keep our shared libs in the lib dir, not the bin dir:
place_shared_lib_in_lib_dir(drcov)

# ensure we rebuild if includes change
add_dependencies(drcov api_headers)

# Provide a hint for how to use the client
if (NOT DynamoRIO_INTERNAL OR NOT "${CMAKE_GENERATOR}" MATCHES "Ninja")
  add_custom_command(TARGET drcov
    POST_BUILD
    COMMAND ${CMAKE_COMMAND}
    ARGS -E echo "Usage: pass to drconfig or drrun: -t drcov"
    VERBATIM)
endif ()

# add drcov2lcov
add_executable(drcov2lcov
  postprocess/drcov2lcov.cpp
  # add more here
  )
# We do want an rpath, which is on by default for standalone
configure_DynamoRIO_standalone(drcov2lcov)
use_DynamoRIO_extension(drcov2lcov drsyms_static)
use_DynamoRIO_extension(drcov2lcov drcontainers)
use_DynamoRIO_extension(drcov2lcov droption)
use_DynamoRIO_extension(drcov2lcov drcovlib_static)
target_link_libraries(drcov2lcov drfrontendlib)

if (ANDROID)
  # XXX i#1749: the Android linker doesn't support rpath, and even when setting
  # LD_LIBRARY_PATH we get a crash.  However, we expect most users to pull
  # the logs and process on a desktop machine.
endif ()

# ensure we rebuild if includes change
add_dependencies(drcov2lcov api_headers)

# Provide a hint for running
if (NOT DynamoRIO_INTERNAL OR NOT "${CMAKE_GENERATOR}" MATCHES "Ninja")
  if (UNIX)
    set(FIND_MSG "(set LD_LIBRARY_PATH)")
  else (UNIX)
    set(FIND_MSG "(set PATH or copy to same directory)")
  endif (UNIX)
  add_custom_command(TARGET drcov2lcov
    POST_BUILD
    COMMAND ${CMAKE_COMMAND}
    ARGS -E echo "Make sure the loader finds the dynamorio library ${FIND_MSG}"
    VERBATIM)
endif ()

install_target(drcov ${INSTALL_CLIENTS_LIB})
install_target(drcov2lcov ${INSTALL_CLIENTS_BIN})

# On Linux we rely on the rpath
if (WIN32)
  DynamoRIO_get_full_path(DR_TARGET_LOCATION dynamorio "${location_suffix}")
  DR_install(FILES "${DR_TARGET_LOCATION}"  DESTINATION "${INSTALL_CLIENTS_BIN}")
  # i#1344: Include a copy of a recent (> 6.0) redistributable version of dbghelp,
  # if we found one, so our drcov2lcov works pre-Vista.
  if (dbghelp_path)
    DR_install(FILES "${dbghelp_path}" DESTINATION ${INSTALL_CLIENTS_BIN})
  endif ()
  # We need a copy of DR for the build dir:
  add_dependencies(drcov2lcov client_dr_copy)
endif (WIN32)

set(INSTALL_DRCOV_CONFIG ${INSTALL_CLIENTS_BASE})

if (X64)
  set(CONFIG_INSTALL ${PROJECT_BINARY_DIR}/drcov.drrun64)
  set(CONFIG_BUILD ${PROJECT_BINARY_DIR}/tools/drcov.drrun64)
else (X64)
  set(CONFIG_INSTALL ${PROJECT_BINARY_DIR}/drcov.drrun32)
  set(CONFIG_BUILD ${PROJECT_BINARY_DIR}/tools/drcov.drrun32)
endif (X64)

function (write_config_file dst libdir)
  # We include the alternate-bitwidth path, though it won't be there for
  # a single build dir and such a child will have a fatal error.
  if (X64)
    string(REPLACE "lib64" "lib32" alt_libdir ${libdir})
    set(CUR_BIT "64")
    set(ALT_BIT "32")
  else ()
    set(CUR_BIT "32")
    set(ALT_BIT "64")
    string(REPLACE "lib64" "lib32" alt_libdir ${libdir})
  endif ()
  file(WRITE  ${dst} "# drcov tool config file\n")
  file(APPEND ${dst} "# DynamoRIO options\n")
  file(APPEND ${dst} "DR_OP=-nop_initial_bblock\n")
  file(APPEND ${dst} "# client tool path\n")
  file(APPEND ${dst} "CLIENT${CUR_BIT}_REL=${libdir}/${LIB_PFX}drcov${LIB_EXT}\n")
  file(APPEND ${dst} "CLIENT${ALT_BIT}_REL=${alt_libdir}/${LIB_PFX}drcov${LIB_EXT}\n")
  file(APPEND ${dst} "# client tool options\n")
  file(APPEND ${dst} "TOOL_OP=\n")
endfunction ()

set(BUILD_CLIENTS_LIB clients/${INSTALL_LIB})

write_config_file(${CONFIG_INSTALL} ${INSTALL_CLIENTS_LIB})
write_config_file(${CONFIG_BUILD} ${BUILD_CLIENTS_LIB})

DR_install(FILES "${CONFIG_INSTALL}" DESTINATION ${INSTALL_DRCOV_CONFIG})
register_tool_file("drcov")

# We provide a version of genhtml that has Windows support
DR_install(FILES "${PROJECT_SOURCE_DIR}/third_party/lcov/genhtml"
  DESTINATION ${INSTALL_CLIENTS_BIN}
  PERMISSIONS ${owner_access} OWNER_EXECUTE GROUP_READ GROUP_EXECUTE
  WORLD_READ WORLD_EXECUTE)

##################################################
# Documentation

# We auto-generate the list of options in the html docs by running a helper app.
# To avoid having a separate options.h, our helper here is drcov2locv itself when
# passed a special flag.

# We then have to insert it into the doxygen files at build time:
set(srcdoc ${CMAKE_CURRENT_SOURCE_DIR}/drcov.dox.in)
set(gendoc ${CMAKE_CURRENT_BINARY_DIR}/drcov.dox)
set(doctgt drcov_docs)

get_property(dox_extras GLOBAL PROPERTY DynamoRIO_dox_extras)
set_property(GLOBAL PROPERTY DynamoRIO_dox_extras ${dox_extras} ${gendoc})

get_property(dox_targets GLOBAL PROPERTY DynamoRIO_dox_targets)
set_property(GLOBAL PROPERTY DynamoRIO_dox_targets ${dox_targets} ${doctgt})

add_custom_target(${doctgt} DEPENDS ${gendoc})
# FIXME i#3544: Re-enable once RISC-V binaries work so that options can be generated.
if (RISCV64 AND NOT CMAKE_CROSSCOMPILING)
  file(COPY_FILE ${srcdoc} ${gendoc})
else ()
  add_custom_command(
    OUTPUT ${gendoc}
    DEPENDS ${srcdoc}
    drcov2lcov
    COMMAND ${CMAKE_COMMAND}
    ARGS -D src=${srcdoc}
        -D dst=${gendoc}
        -D CMAKE_CROSSCOMPILING=${CMAKE_CROSSCOMPILING}
        -D prog=$<TARGET_FILE:drcov2lcov>
        -D prog_arg=-help_html
        -P ${CMAKE_CURRENT_SOURCE_DIR}/../common/gendocs.cmake
    VERBATIM)
endif ()
