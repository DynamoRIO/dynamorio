# **********************************************************
# Copyright (c) 2014-2016 Google, Inc.    All rights reserved.
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

if ("${CMAKE_VERSION}" VERSION_EQUAL "3.3" OR
    "${CMAKE_VERSION}" VERSION_GREATER "3.3")
  # XXX i#1955: update our code to satisfy the changes in 3.3
  cmake_policy(SET CMP0058 OLD)
endif ()

if ("${CMAKE_VERSION}" VERSION_EQUAL "3.1" OR
    "${CMAKE_VERSION}" VERSION_GREATER "3.1")
  # XXX i#1557: update our code to satisfy the changes in 3.x
  cmake_policy(SET CMP0054 OLD)
endif ()

if ("${CMAKE_VERSION}" VERSION_EQUAL "3.0" OR
    "${CMAKE_VERSION}" VERSION_GREATER "3.0")
  # XXX i#1557: update our code to satisfy the changes in 3.x
  cmake_policy(SET CMP0026 OLD)
  # XXX i#1375: if we make 2.8.12 the minimum we can remove the @rpath
  # Mac stuff and this policy, right?
  cmake_policy(SET CMP0042 OLD)
endif ()

if ("${CMAKE_VERSION}" VERSION_EQUAL "2.8.12" OR
    "${CMAKE_VERSION}" VERSION_GREATER "2.8.12")
  # XXX DrMem-i#1481: update to cmake 2.8.12's better handling of interface imports
  cmake_policy(SET CMP0022 OLD)
endif ()

if ("${CMAKE_VERSION}" VERSION_EQUAL "2.8.11" OR
    "${CMAKE_VERSION}" VERSION_GREATER "2.8.11")
  # XXX i#1418: update to cmake 2.8.12's better handling of interface imports, for Qt
  cmake_policy(SET CMP0020 OLD)
endif ()

if ("${CMAKE_VERSION}" VERSION_EQUAL "2.6.4" OR
    "${CMAKE_VERSION}" VERSION_GREATER "2.6.4")
  # Recognize literals in if statements
  cmake_policy(SET CMP0012 NEW)
endif ()
