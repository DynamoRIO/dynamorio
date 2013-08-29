# **********************************************************
# Copyright (c) 2013, Branden Clark All rights reserved.
# **********************************************************/

# Dr. GUI
#
# Redistribution and use in source and binary forms, with or without 
# modification, are permitted provided that the conditions outlined in
# the BSD 2-Clause license are met.
#
# This software is provided by the copyright holders and contributors "AS IS"
# and any express or implied warranties, including, but not limited to, the
# implied warranties of merchantability and fitness for a particular purpose
# are disclaimed. See the BSD 2-Clause license for more details.
#

# CMakeLists.txt
# 
# Configures DrGUI build
#

cmake_minimum_required(VERSION 2.8.10)
# Build Qt Visualizer
# DR's boundary of 2 causes alignment issues with Qt, so we stick to default.
# This is okay because this is a standalone app, and does not need to match
# DynamoRIO.
if (NOT WIN32)
  string(REPLACE "-mpreferred-stack-boundary=2 " "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
endif (NOT WIN32)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/ext/${INSTALL_BIN}")

find_package(Qt5Widgets QUIET)
if (NOT Qt5Widgets_FOUND)
  message(STATUS "Could NOT find Qt 5: DrGUI will not be built")
  message(STATUS 
    "Point CMake variable Qt5Widgets_DIR at the Qt5WidgetsConfig.cmake directory")
else (NOT Qt5Widgets_FOUND)
  message(STATUS "Found Qt 5: DrGUI will be built")

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

  # Build
  # Ignore windows specific Qt warnings
  if (WIN32)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -wd4127 -wd4512 -wd4189 -wd4481")
  endif (WIN32)
  # XXX i#1252: Named drgui_qt since Windows gets it confused with tools/drgui
  add_executable(drgui_qt ${drgui_SOURCES} ${drgui_MOC_OUTFILES})
  qt5_use_modules(drgui_qt Widgets)

  # Install
  add_rel_rpaths(drgui_qt)
  DR_install(FILES 
    ${CMAKE_CURRENT_SOURCE_DIR}/drgui_tool_interface.h
    ${CMAKE_CURRENT_SOURCE_DIR}/drgui_options_interface.h 
    DESTINATION 
    ${INSTALL_EXT_INCLUDE})
  DR_export_target(drgui_qt)
  install_exported_target(drgui_qt ${INSTALL_EXT_BIN})
endif (NOT Qt5Widgets_FOUND)
