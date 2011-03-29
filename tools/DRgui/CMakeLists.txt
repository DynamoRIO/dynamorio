# **********************************************************
# Copyright (c) 2010 Google, Inc.    All rights reserved.
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

if (UNIX)
  message(FATAL_ERROR "DRgui is Windows-only")
endif (UNIX)
if (X64)
  message(FATAL_ERROR "DRgui is 32-bit only (issue 62 covers porting to 64-bit)")
endif (X64)

# FIXME i#77: make DRgui a subproject to support with different compiler from libutil
# For now, DRgui.exe just won't run on NT.
#
# PR 232385: for release packages we want DRgui.exe to run on NT,
# which means we cannot build using VC71 or VC80, as their MFC
# versions add dependencies on libraries like shlwapi.dll and ole*.dll
# that are not present on NT.  So we build using VC60.
# Unfortunately with VC60 we can't make a debug build as libutil uses
# VC71 and we can't incorporate the policy_staticd pdbs.

set(CMAKE_CXX_FLAGS "${CMAKE_C_FLAGS} /EHsc")
# avoid warning about overriding
string(REGEX REPLACE "/W3" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
set(CMAKE_MFC_FLAG 1) # static dlls

add_definitions(-DNOT_DYNAMORIO_CORE -DWIN32 -DUNICODE -D_UNICODE)

include_directories(
  ${PROJECT_SOURCE_DIR}/libutil
  ${PROJECT_SOURCE_DIR}/core
  ${PROJECT_SOURCE_DIR}/core/lib
  ${CMAKE_CURRENT_SOURCE_DIR} # for resource.h for generated DynamoRIO.rc
  )

# can't combine strings w/ cpp defs so we configure the name and version:
configure_file(DynamoRIO.rc ${CMAKE_CURRENT_BINARY_DIR}/DynamoRIO.rc)
set_source_files_properties(${CMAKE_CURRENT_BINARY_DIR}/DynamoRIO.rc
  PROPERTIES GENERATED true)

add_executable(DRgui WIN32
  CmdlineDlg.cpp
  DynamoRIO.cpp
  DynamoRIODoc.cpp
  DynamoRIOView.cpp
  IgnoreDlg.cpp
  LoggingDlg.cpp
  MainFrm.cpp
  OptionsDlg.cpp
  ShellInterface.cpp
  StdAfx.cpp
  SyswideDlg.cpp
  ${CMAKE_CURRENT_BINARY_DIR}/DynamoRIO.rc
  )

target_link_libraries(DRgui policy_static)
# FIXME: not sure why I need these flags
# For VC71 need nodefault of libcmt instead?!?
set_target_properties(DRgui PROPERTIES LINK_FLAGS
  "/NODEFAULTLIB:libc.lib /ENTRY:wWinMainCRTStartup")

DR_install(TARGETS DRgui DESTINATION ${INSTALL_BIN})
