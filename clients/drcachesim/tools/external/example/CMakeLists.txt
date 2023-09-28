cmake_minimum_required(VERSION 3.7)

macro (add_exported_library name type)
  add_library(${name} ${type} ${ARGN})
  DR_export_target(${name})
  install_exported_target(${name} ${INSTALL_CLIENTS_LIB})
  add_dependencies(${name} api_headers)
endmacro ()

get_filename_component(DRCACHESIM_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../.. ABSOLUTE)

add_exported_library(drmemtrace_empty STATIC empty.cpp ${DRCACHESIM_SOURCE_DIR}/common/options.cpp)
set_property(TARGET drmemtrace_empty PROPERTY POSITION_INDEPENDENT_CODE ON)
configure_DynamoRIO_standalone(drmemtrace_empty)

add_exported_library(empty_creator SHARED empty_creator.cpp)
target_link_libraries(empty_creator drmemtrace_empty)
configure_DynamoRIO_standalone(empty_creator)

place_shared_lib_in_lib_dir(empty_creator)

if (REDIRECT_OUTPUT)
    append_property_list(TARGET drmemtrace_empty COMPILE_DEFINITIONS "REDIRECT_OUTPUT_")
endif()

if (NOT APPLE)
  add_executable(empty_launcher
    empty_launcher.cpp
    )
  configure_DynamoRIO_static(empty_launcher)
  target_link_libraries(empty_launcher drmemtrace_empty)
  add_dependencies(empty_launcher api_headers)
  add_dependencies(empty_launcher drmemtrace_analyzer)
  target_link_libraries(empty_launcher drmemtrace_analyzer)
  configure_DynamoRIO_static(empty_launcher)
  install_target(empty_launcher ${INSTALL_CLIENTS_BIN})
endif()

macro(add_win32_flags target)
  if (DEBUG AND NOT DEFINED ${target}_uses_configure)
    append_property_list(TARGET ${target} COMPILE_DEFINITIONS "DEBUG")
  endif ()
  if (WIN32)
    if (DEBUG)
      get_property(cur TARGET ${target} PROPERTY COMPILE_FLAGS)
      string(REPLACE "/MT " "" cur "${cur}") # Avoid override warning.
      set_target_properties(${target} PROPERTIES COMPILE_FLAGS "${cur} /EHsc /MTd /Zi")
      append_property_string(TARGET ${target} LINK_FLAGS "/nodefaultlib:libcmt")
    else ()
      append_property_string(TARGET ${target} COMPILE_FLAGS "/EHsc /MT")
    endif ()
  else ()
    # Work around configure_DynamoRIO_static() clobbering flags by re-adding
    # the important ones for our tests, so they can include our headers
    # with C++11-isms.
    get_property(cur TARGET ${target} PROPERTY COMPILE_FLAGS)
    if (NOT cur MATCHES "-std=")
      append_property_string(TARGET ${target} COMPILE_FLAGS "-std=c++11")
    endif ()
    if (DEBUG AND NOT cur MATCHES "-g")
      # We can't use restore_nonclient_flags() for clients, so we manually re-add debug
      # here.
      append_property_string(TARGET ${target} COMPILE_FLAGS "-g")
    endif ()
    if (APPLE)
      # Match the core/ flags.
      append_property_string(TARGET ${target}
        COMPILE_FLAGS "-mmacosx-version-min=${OLDEST_OSX_SUPPPORTED}")
    endif ()
  endif ()
endmacro ()

if (NOT APPLE)
  add_win32_flags(empty_launcher)
endif()

function (write_config_file id dst libdir)
  file(GENERATE OUTPUT ${dst} CONTENT
"# analysis tool creator config file\n\
TOOL_ID=${id}
CREATOR_BIN=${libdir}/${LIB_PFX}${id}_creator${LIB_EXT}\n\
")
endfunction ()

set(EMPTY_CONFIG_INSTALL ${PROJECT_BINARY_DIR}/empty.drcachesim)
set(EMPTY_CONFIG_BUILD ${PROJECT_BINARY_DIR}/tools/empty.drcachesim)
write_config_file(empty ${EMPTY_CONFIG_BUILD} clients/${INSTALL_LIB})
write_config_file(empty ${EMPTY_CONFIG_INSTALL} tools/${INSTALL_LIB})
DR_install(FILES "${EMPTY_CONFIG_INSTALL}" DESTINATION ${INSTALL_CLIENTS_BASE})
