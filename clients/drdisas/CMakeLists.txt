# **********************************************************
# Copyright (c) 2020-2021 Google, Inc.    All rights reserved.
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

add_executable(drdisas drdisas.cpp)
configure_DynamoRIO_decoder(drdisas)
use_DynamoRIO_extension(drdisas droption)
add_dependencies(drdisas api_headers)

if (WIN32)
  append_property_string(TARGET drdisas COMPILE_FLAGS "/EHsc")
endif ()

if (DEFINED DR_INSTALL_DEPLOY_BIN_DEST)
  set(dst ${DR_INSTALL_DEPLOY_BIN_DEST})
else ()
  set(dst "${INSTALL_BIN}")
endif ()
DR_target_install(TARGETS drdisas DESTINATION ${dst})

get_property(dox_extras GLOBAL PROPERTY DynamoRIO_dox_extras)
set_property(GLOBAL PROPERTY DynamoRIO_dox_extras
  ${dox_extras} ${CMAKE_CURRENT_SOURCE_DIR}/drdisas.dox)
