# **********************************************************
# Copyright (c) 2025 Google, Inc.    All rights reserved.
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

include(../../make/policies.cmake NO_POLICY_SCOPE)

set(DynamoRIO_INTERNAL ON) # do not import dynamorio lib target
set(DynamoRIO_DIR ${PROJECT_BINARY_DIR}/cmake)
find_package(DynamoRIO ${VERSION_NUMBER_MAJOR}.${VERSION_NUMBER_MINOR})
if (NOT DynamoRIO_FOUND)
  message(FATAL_ERROR "DynamoRIO package required to build")
endif(NOT DynamoRIO_FOUND)

# Versioning: we use separate versioning for DRMF from the tool versioning.
# This is major*100 + minor.
set(DRMF_VERSION_DEFAULT "1.0.${VERSION_NUMBER_PATCHLEVEL}")
set(DRMF_VERSION "" CACHE STRING "DRMF version number: leave empty for default")
if ("${DRMF_VERSION}" STREQUAL "")
  set(DRMF_VERSION ${DRMF_VERSION_DEFAULT})
endif()
message(STATUS "DRMF version number: ${DRMF_VERSION}")
string(REGEX REPLACE "^([0-9]+)\\..*" "\\1" DRMF_VERSION_MAJOR "${DRMF_VERSION}")
string(REGEX REPLACE "^[0-9]+\\.([0-9]+)\\..*" "\\1" DRMF_VERSION_MINOR "${DRMF_VERSION}")
# We use just major.minor for .so versioning (and no, SOVERSION is not sufficient
# here: we have to set VERSION to control the file name and import dependence).
string(REGEX REPLACE "^([0-9]+\\.[0-9]+)\\..*" "\\1"
  DRMF_VERSION_MAJOR_MINOR "${DRMF_VERSION}")
math(EXPR DRMF_VERSION_INTEGER "${DRMF_VERSION_MAJOR}*100 + ${DRMF_VERSION_MINOR}")
set(DRMF_VERSION_COMPAT 9)                # Oldest compatible version
set(DRMF_VERSION_CUR ${DRMF_VERSION_INTEGER}) # Current version

set(framework_incdir "${PROJECT_BINARY_DIR}/ext/include")
configure_file(public.h ${framework_incdir}/drmemory_framework.h)

DR_install(FILES ${framework_incdir}/drmemory_framework.h DESTINATION ${INSTALL_EXT_LIB})
