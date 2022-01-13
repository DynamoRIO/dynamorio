# **********************************************************
# Copyright (c) 2010-2021 Google, Inc.    All rights reserved.
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
  message(FATAL_ERROR "DRstats is Windows-only")
endif (UNIX)

# FIXME i#77: make DRstats a subproject to support with different compiler from libutil
# For now, DRstats.exe just won't run on NT.
#
# PR 232385: for release packages we want DRstats.exe to run on NT,
# which means we cannot build using VC71 or VC80, as their MFC
# versions add dependencies on libraries like shlwapi.dll and ole*.dll
# that are not present on NT.  So we build using VC60.
# Unfortunately with VC60 we can't make a debug build as libutil uses
# VC71 and we can't incorporate the policy_staticd pdbs.

foreach (config "" ${CMAKE_BUILD_TYPE} ${CMAKE_CONFIGURATION_TYPES})
  if ("${config}" STREQUAL "")
    set(config_upper "")
  else ("${config}" STREQUAL "")
    string(TOUPPER "_${config}" config_upper)
  endif ("${config}" STREQUAL "")
  set(CMAKE_CXX_FLAGS${config_upper} "${CMAKE_C_FLAGS${config_upper}} /EHsc")
  # avoid warning about overriding
  string(REGEX REPLACE "/W3" "" CMAKE_CXX_FLAGS${config_upper}
    "${CMAKE_CXX_FLAGS${config_upper}}")
  # static for portability
  if (CMAKE_CXX_FLAGS${config_upper} MATCHES "/MD")
    string(REGEX REPLACE "/MD" "/MT" CMAKE_CXX_FLAGS${config_upper}
      "${CMAKE_CXX_FLAGS${config_upper}}")
  else ()
    if (DEBUG)
      set(CMAKE_CXX_FLAGS${config_upper} "${CMAKE_CXX_FLAGS${config_upper}} /MTd")
    else ()
      set(CMAKE_CXX_FLAGS${config_upper} "${CMAKE_CXX_FLAGS${config_upper}} /MT")
    endif ()
  endif ()
endforeach (config)

set(CMAKE_MFC_FLAG 1) # static dlls

add_definitions(-DNOT_DYNAMORIO_CORE -DDYNAMORIO_INTERNAL -DWIN32 -DUNICODE -D_UNICODE)

include_directories(
  ${PROJECT_SOURCE_DIR}/libutil
  ${PROJECT_SOURCE_DIR}/core
  ${PROJECT_SOURCE_DIR}/core/lib
  ${CMAKE_CURRENT_SOURCE_DIR} # for resource.h for generated DynamoRIO.rc
  )

# can't combine strings w/ cpp defs so we configure the name and version:
configure_file(DynamoRIO.rc ${CMAKE_CURRENT_BINARY_DIR}/DynamoRIO.rc)
if (NOT "${CMAKE_GENERATOR}" MATCHES "Ninja")
  # FIXME: This file gets deleted by 'ninja -t clean', but it's only copied at
  # config time, so running ninja doesn't re-generate it.  Marking it as
  # non-generated means 'ninja -t clean' doesn't delete it.
  set_source_files_properties(${CMAKE_CURRENT_BINARY_DIR}/DynamoRIO.rc
    PROPERTIES GENERATED true)
endif ()

add_executable(DRstats WIN32
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

# somehow VS2010 doesn't find its own SDK uuid.lib
find_library(uuid_lib uuid)
if (NOT uuid_lib)
  if (X64)
    set(uuid_lib "${kits_dir}/Lib/${CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION}/um/x64/Uuid.Lib")
  else (X64)
    set(uuid_lib "${kits_dir}/Lib/${CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION}/um/x86/Uuid.Lib")
  endif (X64)
  if (NOT EXISTS ${uuid_lib})
    message(FATAL_ERROR "uuid.lib not found")
  endif ()
endif (NOT uuid_lib)
target_link_libraries(DRstats policy_static ${uuid_lib})
# FIXME: not sure why I need these flags
# For VC71 need nodefault of libcmt instead?!?
set_target_properties(DRstats PROPERTIES LINK_FLAGS
  "/NODEFAULTLIB:libc.lib /NODEFAULTLIB:msvcrt.lib /ENTRY:wWinMainCRTStartup")

DR_install(TARGETS DRstats DESTINATION ${INSTALL_BIN})
