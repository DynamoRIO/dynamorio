# **********************************************************
# Copyright (c) 2010 VMware, Inc.    All rights reserved.
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

cmake_minimum_required(VERSION 2.6)

# library of container utilities in C that use DR API for memory allocation
# and synchronization
add_library(drcontainers STATIC
  hashtable.c
  # add more here
  )
configure_DynamoRIO_client(drcontainers)
if (UNIX)
  # static containers must be PIC to be linked into clients: else requires
  # relocations that run afoul of security policies, etc.
  get_target_property(cur_flags drcontainers COMPILE_FLAGS)
  set_target_properties(drcontainers PROPERTIES COMPILE_FLAGS "${cur_flags} -fPIC")
endif (UNIX)
# ensure we rebuild if includes change
add_dependencies(drcontainers api_headers)

if (WIN32 AND GENERATE_PDBS)
  # I believe it's the lack of CMAKE_BUILD_TYPE that's eliminating this?
  # In any case we make sure to add it (for release and debug, to get pdb):
  get_target_property(cur_flags drcontainers LINK_FLAGS)
  set_target_properties(drcontainers PROPERTIES LINK_FLAGS "${cur_flags} /debug")
endif (WIN32 AND GENERATE_PDBS)

# documentation is put into main DR docs/ dir

export(TARGETS drcontainers
  FILE ${PROJECT_BINARY_DIR}/cmake/${exported_targets_name}.cmake
  APPEND)
install(TARGETS drcontainers EXPORT ${exported_targets_name}
  DESTINATION ${INSTALL_EXT_LIB})
install(FILES
  hashtable.h
  # add more here
  DESTINATION ${INSTALL_EXT_INCLUDE})
