# ***************************************************************************
# Copyright (c) 2014-2020 Google, Inc.  All rights reserved.
# Copyright (c) 2013 Branden Clark  All rights reserved.
# ***************************************************************************

#
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
# ARE DISCLAIMED. IN NO EVENT SHALL GOOGLE, INC. OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
# SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
# CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
# OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
# DAMAGE.
#

# CMakeLists.txt
#
# Configures DrGUI build
#

find_package(Qt5Widgets QUIET)
if (NOT Qt5Widgets_FOUND)
  message(STATUS "WARNING: Could not find Qt 5: DrGUI will NOT be built")
  message(STATUS
    "  Point CMake variable Qt5Widgets_DIR at the Qt5WidgetsConfig.cmake directory")
elseif ("${CMAKE_VERSION}" VERSION_LESS "3.2")
  message(STATUS "WARNING: CMake version is < 3.2: DrGUI will NOT be built")
else () # Qt5 and CMake 3.2+
  # Build Qt Visualizer
  cmake_minimum_required(VERSION 3.7)

  message(STATUS "Found Qt 5: DrGUI will be built")

  include(../../make/policies.cmake NO_POLICY_SCOPE)

  cmake_policy(SET CMP0043 OLD)

  # Set ouput_dir since drgui_qt is the only executable extension
  set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/ext/${INSTALL_BIN}")

  # Sources and MOC
  set(drgui_MOC_HEADERS
    ${CMAKE_CURRENT_SOURCE_DIR}/drgui_tool_interface.h
    ${CMAKE_CURRENT_SOURCE_DIR}/drgui_options_interface.h
    ${CMAKE_CURRENT_SOURCE_DIR}/drgui_options_window.h
    ${CMAKE_CURRENT_SOURCE_DIR}/drgui_main_window.h)

  qt5_wrap_cpp(drgui_MOC_OUTFILES ${drgui_MOC_HEADERS})

  set(drgui_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/drgui_options_window.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/drgui_main_window.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp)

  if (WIN32)
    set(drgui_SOURCES ${drgui_SOURCES} ${PROJECT_SOURCE_DIR}/core/win32/resources.rc)
  endif ()

  # Build
  # Ignore windows specific Qt warnings
  if (WIN32)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -wd4127 -wd4512 -wd4189 -wd4481")
  else ()
    # Work around signed-unsigned cmp in Qt5's qvector.h (i#1797)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-strict-overflow")
  endif (WIN32)
  add_executable(drgui ${drgui_SOURCES} ${drgui_MOC_OUTFILES})

  # qt5_use_modules has been dropped from newer QT versions.
  if (Qt5Widgets_VERSION VERSION_LESS 5.11)
    qt5_use_modules(drgui Widgets)
  else()
    target_link_libraries(drgui Qt5::Widgets)
  endif()

  if (WIN32)
    # For version info we need global_shared.h included in resources.rc.
    include_directories(${PROJECT_SOURCE_DIR}/core/lib)
    set_property(TARGET drgui PROPERTY COMPILE_DEFINITIONS "RC_IS_drgui")
  endif ()

  # Install
  add_rel_rpaths(drgui)
  DR_export_target(drgui)
  install_exported_target(drgui ${INSTALL_EXT_BIN})
  DR_install(FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/drgui_tool_interface.h
    ${CMAKE_CURRENT_SOURCE_DIR}/drgui_options_interface.h
    DESTINATION
    ${INSTALL_EXT_INCLUDE})
endif () # Qt5 and CMake 3.2+
