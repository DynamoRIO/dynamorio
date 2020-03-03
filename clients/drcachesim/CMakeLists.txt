# **********************************************************
# Copyright (c) 2015-2020 Google, Inc.    All rights reserved.
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

# 3.7 is DR's min as well, for VS2017, GREATER_EQUAL, etc.
cmake_minimum_required(VERSION 3.7)

include(../../make/policies.cmake NO_POLICY_SCOPE)

if (WIN32)
  set(os_name "win")
  # Our non-client files assume this is set, yet don't include headers that set it.
  add_definitions(-DWINDOWS)
else ()
  set(os_name "unix")
  # Ditto.
  add_definitions(-DUNIX)
endif ()

# GCC 6+ has a warning for an ABI change due to a bug introduced in GCC 5:
# http://gcc.gnu.org/bugzilla/show_bug.cgi?id=77728. As we are building all of
# drcachesim and not linking to other C++ code, we can just ignore it.
if (ARM AND CMAKE_COMPILER_IS_GNUCC)
  include(CheckCXXCompilerFlag)
  CHECK_CXX_COMPILER_FLAG(-Wno-psabi GCC_HAS_NO_PSABI)
  if (GCC_HAS_NO_PSABI)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-psabi")
  endif (GCC_HAS_NO_PSABI)
endif ()

# i#2277: we use zlib if available to read compressed trace files.
# XXX: we could ship with a zlib for Windows: today we simply don't support
# compressed traces on Windows.
if (ZLIB_FOUND)
  add_definitions(-DHAS_ZLIB)
  include_directories(${ZLIB_INCLUDE_DIRS})
  set(zlib_reader reader/compressed_file_reader.cpp)
else ()
  set(zlib_reader "")
endif()

if (libsnappy)
  add_definitions(-DHAS_SNAPPY)
  set(snappy_reader reader/snappy_file_reader.cpp reader/crc32c.cpp)
else ()
  set(snappy_reader "")
endif()

set(client_and_sim_srcs
  common/named_pipe_${os_name}.cpp
  common/options.cpp
  common/trace_entry.cpp)

# i#2006: we split our tools into libraries for combining as desired in separate
# launchers.  Since they are exported in the same dir as other tools like drcov,
# we use a drmemtrace_ prefix.
macro (add_exported_library name type)
  add_library(${name} ${type} ${ARGN})
  DR_export_target(${name})
  install_exported_target(${name} ${INSTALL_CLIENTS_LIB})
  add_dependencies(${name} api_headers)
endmacro ()

add_exported_library(drmemtrace_reuse_distance STATIC tools/reuse_distance.cpp)
add_exported_library(drmemtrace_histogram STATIC tools/histogram.cpp)
add_exported_library(drmemtrace_reuse_time STATIC tools/reuse_time.cpp)
add_exported_library(drmemtrace_basic_counts STATIC tools/basic_counts.cpp)
add_exported_library(drmemtrace_opcode_mix STATIC tools/opcode_mix.cpp)
add_exported_library(drmemtrace_view STATIC tools/view.cpp)
add_exported_library(drmemtrace_func_view STATIC tools/func_view.cpp)
configure_DynamoRIO_standalone(drmemtrace_opcode_mix)
configure_DynamoRIO_standalone(drmemtrace_view)

# We combine the cache and TLB simulators as they share code already.
add_exported_library(drmemtrace_simulator STATIC
  simulator/simulator.cpp
  simulator/cache.cpp
  simulator/cache_lru.cpp
  simulator/cache_fifo.cpp
  simulator/cache_miss_analyzer.cpp
  simulator/caching_device.cpp
  simulator/caching_device_stats.cpp
  simulator/cache_stats.cpp
  simulator/prefetcher.cpp
  simulator/cache_simulator.cpp
  simulator/snoop_filter.cpp
  simulator/tlb.cpp
  simulator/tlb_simulator.cpp
  )

add_exported_library(directory_iterator STATIC common/directory_iterator.cpp)
add_dependencies(directory_iterator api_headers)
target_link_libraries(directory_iterator drfrontendlib)

add_exported_library(drmemtrace_raw2trace STATIC
  tracer/raw2trace.cpp
  tracer/raw2trace_directory.cpp
  tracer/instru.cpp
  tracer/instru_online.cpp
  tracer/instru_offline.cpp
  )
configure_DynamoRIO_standalone(drmemtrace_raw2trace)
target_link_libraries(drmemtrace_raw2trace directory_iterator drfrontendlib)
use_DynamoRIO_extension(drmemtrace_raw2trace drutil_static)
link_with_pthread(drmemtrace_raw2trace)

set(drcachesim_srcs
  launcher.cpp
  analyzer.cpp
  analyzer_multi.cpp
  ${client_and_sim_srcs}
  reader/reader.cpp
  reader/config_reader.cpp
  reader/file_reader.cpp
  ${zlib_reader}
  ${snappy_reader}
  reader/ipc_reader.cpp
  simulator/analyzer_interface.cpp
  tracer/instru.cpp
  tracer/instru_online.cpp
  )
if (DEBUG)
  # We include the invariants analyzer for testing.
  set(drcachesim_srcs ${drcachesim_srcs} tests/trace_invariants.cpp)
endif ()
add_executable(drcachesim ${drcachesim_srcs})
# In order to embed raw2trace we need to be standalone:
configure_DynamoRIO_standalone(drcachesim)
# Link in our tools:
target_link_libraries(drcachesim drmemtrace_simulator drmemtrace_reuse_distance
  drmemtrace_histogram drmemtrace_reuse_time drmemtrace_basic_counts
  drmemtrace_opcode_mix drmemtrace_view drmemtrace_func_view
  drmemtrace_raw2trace directory_iterator)
if (libsnappy)
  target_link_libraries(drcachesim snappy)
endif ()
# To avoid dup symbol errors between drinjectlib and drdecode on Windows we have
# to explicitly list drdecode up front:
target_link_libraries(drcachesim drdecode drinjectlib drconfiglib drfrontendlib)
use_DynamoRIO_extension(drcachesim droption)
# These are also for raw2trace:
use_DynamoRIO_extension(drcachesim drcovlib_static)
use_DynamoRIO_extension(drcachesim drutil_static)

# This is to avoid ../ and common/ in the #includes of headers that we
# export in a single dir for 3rd-party tool integration.
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/common)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/reader)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/tracer)
include_directories(${CMAKE_CURRENT_SOURCE_DIR})

add_exported_library(drmemtrace_analyzer STATIC
  analyzer.cpp
  common/trace_entry.cpp
  reader/reader.cpp
  reader/config_reader.cpp
  reader/file_reader.cpp
  ${zlib_reader}
  ${snappy_reader}
  )
target_link_libraries(drmemtrace_analyzer directory_iterator)
if (libsnappy)
  target_link_libraries(drmemtrace_analyzer snappy)
endif ()
link_with_pthread(drmemtrace_analyzer)
# We get away w/ exporting the generically-named "utils.h" by putting into a
# drmemtrace/ subdir.
install_client_nonDR_header(drmemtrace common/utils.h)
install_client_nonDR_header(drmemtrace common/trace_entry.h)
install_client_nonDR_header(drmemtrace common/memref.h)
install_client_nonDR_header(drmemtrace reader/reader.h)
install_client_nonDR_header(drmemtrace analysis_tool.h)
install_client_nonDR_header(drmemtrace analyzer.h)
install_client_nonDR_header(drmemtrace tools/reuse_distance_create.h)
install_client_nonDR_header(drmemtrace tools/histogram_create.h)
install_client_nonDR_header(drmemtrace tools/reuse_time_create.h)
install_client_nonDR_header(drmemtrace tools/basic_counts_create.h)
install_client_nonDR_header(drmemtrace tools/opcode_mix_create.h)
install_client_nonDR_header(drmemtrace simulator/cache_simulator.h)
install_client_nonDR_header(drmemtrace simulator/cache_simulator_create.h)
install_client_nonDR_header(drmemtrace simulator/tlb_simulator_create.h)
install_client_nonDR_header(drmemtrace tools/view_create.h)
install_client_nonDR_header(drmemtrace tools/func_view_create.h)
install_client_nonDR_header(drmemtrace tracer/raw2trace.h)

# We show one example of how to create a standalone analyzer of trace
# files that does not need to link with DR.
# We also use this to link in our trace_invariants sanity checker.
add_executable(histogram_launcher
  tools/histogram_launcher.cpp
  tests/trace_invariants.cpp
  )
target_link_libraries(histogram_launcher drmemtrace_analyzer drmemtrace_histogram
  drfrontendlib)
use_DynamoRIO_extension(histogram_launcher droption)
add_dependencies(histogram_launcher api_headers)
# We have a companion test built using a separate --build-and-test CMake project in
# tests/analyzer_separate.cpp to better test 3rd-party usage.
set_property(GLOBAL PROPERTY DynamoRIO_drmemtrace_src_dir
  "${CMAKE_CURRENT_SOURCE_DIR}/tests")
set_property(GLOBAL PROPERTY DynamoRIO_drmemtrace_build_dir
  "${CMAKE_CURRENT_BINARY_DIR}/tests")

# We have one more example of a standalone launcher that uses raw2trace to catch
# link errors (xref i#1409).  We just build it to test the linking; no test.
# XXX i#2007: a binutils error causes static dynamorio to fail to link on AArch64.
# We'll enable this once Travis and others have a fixed binutils.
# XXX i#1997: static DR is not fully supported on Mac yet.
if (NOT AARCH64 AND NOT APPLE)
  add_executable(opcode_mix_launcher
    tools/opcode_mix_launcher.cpp
    )
  # We hit dup symbol errors on Windows so we need libc earlier before DR:
  _DR_get_static_libc_list(static_libc)
  target_link_libraries(opcode_mix_launcher drmemtrace_analyzer drmemtrace_opcode_mix
    drmemtrace_raw2trace drcovlib_static drfrontendlib ${static_libc})
  use_DynamoRIO_extension(opcode_mix_launcher droption)
  add_dependencies(opcode_mix_launcher api_headers)
  configure_DynamoRIO_static(opcode_mix_launcher)
endif ()

if (ZLIB_FOUND)
  target_link_libraries(drcachesim ${ZLIB_LIBRARIES})
  target_link_libraries(histogram_launcher ${ZLIB_LIBRARIES})
  target_link_libraries(drmemtrace_raw2trace ${ZLIB_LIBRARIES})
  if (NOT AARCH64 AND NOT APPLE)
    target_link_libraries(opcode_mix_launcher ${ZLIB_LIBRARIES})
  endif ()
endif ()

macro(add_drmemtrace name type)
  if (${type} STREQUAL "STATIC")
    set(ext_sfx "_static")
  else ()
    set(ext_sfx "")
  endif ()
  add_library(${name} ${type}
    tracer/tracer.cpp
    tracer/instru.cpp
    tracer/instru_offline.cpp
    tracer/instru_online.cpp
    tracer/physaddr.cpp
    tracer/func_trace.cpp
    ${client_and_sim_srcs}
    )
  configure_DynamoRIO_client(${name})
  use_DynamoRIO_extension(${name} drmgr${ext_sfx})
  use_DynamoRIO_extension(${name} drsyms${ext_sfx})
  use_DynamoRIO_extension(${name} drwrap${ext_sfx})
  use_DynamoRIO_extension(${name} drreg${ext_sfx})
  use_DynamoRIO_extension(${name} drutil${ext_sfx})
  use_DynamoRIO_extension(${name} drx${ext_sfx})
  use_DynamoRIO_extension(${name} droption)
  use_DynamoRIO_extension(${name} drcovlib${ext_sfx})
  add_dependencies(${name} api_headers)
  install_target(${name} ${INSTALL_CLIENTS_LIB})
endmacro()

add_drmemtrace(drmemtrace SHARED)
add_drmemtrace(drmemtrace_static STATIC)
append_property_string(TARGET drmemtrace_static COMPILE_FLAGS "-DDRMEMTRACE_STATIC")
# We export drmemtrace.h to the same place as the analysis tool headers
# for simplicity, rather than sticking it into ext/include or sthg.
install_client_nonDR_header(drmemtrace tracer/drmemtrace.h)

add_executable(drraw2trace
  tracer/raw2trace_launcher.cpp
  tracer/instru.cpp
  tracer/instru_online.cpp
  )
target_link_libraries(drraw2trace drmemtrace_raw2trace)
# To avoid dup symbol errors on some VS builds we list drdecode before DR:
target_link_libraries(drraw2trace drdecode)
configure_DynamoRIO_standalone(drraw2trace)
target_link_libraries(drraw2trace drfrontendlib)
use_DynamoRIO_extension(drraw2trace droption)
use_DynamoRIO_extension(drraw2trace drcovlib_static)
# Because we're leveraging instru_online code we have to link with drutil:
use_DynamoRIO_extension(drraw2trace drutil_static)

# We add a useful warning that's not in Wall.
CHECK_C_COMPILER_FLAG("-Wimplicit-fallthrough" implicit_fallthrough_avail)

macro(restore_nonclient_flags target)
  # Restore debug and other flags to our non-client executables
  set_target_properties(${target} PROPERTIES
    COMPILE_FLAGS "${ORIG_CMAKE_CXX_FLAGS}")
  if (NOT DEBUG)
    append_property_list(TARGET ${target} COMPILE_DEFINITIONS "NDEBUG")
  endif ()
  # However, we need the target os and arch defines (XXX: better way?) for
  # the config, inject, and frontend headers:
  DynamoRIO_extra_cflags(extra_cflags "" ON)
  append_property_string(TARGET ${target} COMPILE_FLAGS "${extra_cflags}")
  if (implicit_fallthrough_avail)
    append_property_string(TARGET ${target} COMPILE_FLAGS "-Wimplicit-fallthrough")
  endif ()
endmacro()

restore_nonclient_flags(drcachesim)
restore_nonclient_flags(drraw2trace)
restore_nonclient_flags(histogram_launcher)
if (NOT AARCH64 AND NOT APPLE)
  restore_nonclient_flags(opcode_mix_launcher)
endif ()
restore_nonclient_flags(drmemtrace_simulator)
restore_nonclient_flags(drmemtrace_reuse_distance)
restore_nonclient_flags(drmemtrace_histogram)
restore_nonclient_flags(drmemtrace_reuse_time)
restore_nonclient_flags(drmemtrace_basic_counts)
restore_nonclient_flags(drmemtrace_opcode_mix)
restore_nonclient_flags(drmemtrace_view)
restore_nonclient_flags(drmemtrace_func_view)
restore_nonclient_flags(drmemtrace_analyzer)

# We need to pass /EHsc and we pull in libcmtd into drcachesim from a dep lib.
# Thus we need to override the /MT with /MTd.
macro(add_win32_flags target)
  if (WIN32)
    if (DEBUG)
      get_property(cur TARGET ${target} PROPERTY COMPILE_FLAGS)
      string(REPLACE "/MT " "" cur "${cur}") # Avoid override warning.
      set_target_properties(${target} PROPERTIES COMPILE_FLAGS "${cur} /EHsc /MTd")
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
    if (APPLE)
      # Match the core/ flags.
      append_property_string(TARGET ${target} COMPILE_FLAGS "-mmacosx-version-min=10.9")
    endif ()
  endif ()
endmacro ()

add_win32_flags(drcachesim)
add_win32_flags(drraw2trace)
add_win32_flags(histogram_launcher)
add_win32_flags(drmemtrace_raw2trace)
if (NOT AARCH64 AND NOT APPLE)
  add_win32_flags(opcode_mix_launcher)
endif ()
add_win32_flags(drmemtrace_simulator)
add_win32_flags(drmemtrace_reuse_distance)
add_win32_flags(drmemtrace_histogram)
add_win32_flags(drmemtrace_reuse_time)
add_win32_flags(drmemtrace_basic_counts)
add_win32_flags(drmemtrace_opcode_mix)
add_win32_flags(drmemtrace_view)
add_win32_flags(drmemtrace_func_view)
add_win32_flags(drmemtrace_analyzer)
add_win32_flags(directory_iterator)
if (WIN32 AND DEBUG)
  get_target_property(sim_srcs drcachesim SOURCES)
  get_target_property(raw2trace_srcs drraw2trace SOURCES)
  # The client, and our standalone DR users, had /MT added so we need to override.
  # XXX: solve this by avoiding the /MT in the first place!
  foreach (src ${client_and_sim_srcs} ${sim_srcs} ${raw2trace_srcs} tools/opcode_mix.cpp tools/view.cpp)
    get_property(cur SOURCE ${src} PROPERTY COMPILE_FLAGS)
    string(REPLACE "/MT " "" cur ${cur}) # Avoid override warning.
    set_source_files_properties(${src} COMPILE_FLAGS "${cur} /MTd")
  endforeach ()
endif ()

place_shared_lib_in_lib_dir(drmemtrace)

add_dependencies(drcachesim api_headers)

# Provide a hint for how to use the client
if (NOT DynamoRIO_INTERNAL OR NOT "${CMAKE_GENERATOR}" MATCHES "Ninja")
  add_custom_command(TARGET drmemtrace
    POST_BUILD
    COMMAND ${CMAKE_COMMAND}
    ARGS -E echo "Usage: pass to drconfig or drrun: -t drcachesim"
    VERBATIM)
endif ()

install_target(drcachesim ${INSTALL_CLIENTS_BIN})
install_target(drraw2trace ${INSTALL_CLIENTS_BIN})

set(INSTALL_DRCACHESIM_CONFIG ${INSTALL_CLIENTS_BASE})

function (write_config_file dst bindir libdir)
  if (DEBUG)
    set(debugopt "TOOL_OP=-dr_debug")
  else ()
    set(debugopt "")
  endif ()
  file(GENERATE OUTPUT ${dst} CONTENT
"# drcachesim tool config file\n\
FRONTEND_REL=${bindir}/$<TARGET_FILE_NAME:drcachesim>\n\
TOOL_OP=-dr\n\
TOOL_OP_DR_PATH\n\
TOOL_OP_DR_BUNDLE=-dr_ops\n\
TOOL_OP=-tracer\n\
CLIENT_REL=${libdir}/${LIB_PFX}drmemtrace${LIB_EXT}\n\
${debugopt}")
endfunction ()

if (X64)
  set(CONFIG_INSTALL ${PROJECT_BINARY_DIR}/drcachesim.drrun64)
  set(CONFIG_BUILD ${PROJECT_BINARY_DIR}/tools/drcachesim.drrun64)
else (X64)
  set(CONFIG_INSTALL ${PROJECT_BINARY_DIR}/drcachesim.drrun32)
  set(CONFIG_BUILD ${PROJECT_BINARY_DIR}/tools/drcachesim.drrun32)
endif (X64)

set(BUILD_CLIENTS_BIN clients/${INSTALL_BIN})
set(BUILD_CLIENTS_LIB clients/${INSTALL_LIB})

write_config_file(${CONFIG_INSTALL} ${INSTALL_CLIENTS_BIN} ${INSTALL_CLIENTS_LIB})
write_config_file(${CONFIG_BUILD} ${BUILD_CLIENTS_BIN} ${BUILD_CLIENTS_LIB})

DR_install(FILES "${CONFIG_INSTALL}" DESTINATION ${INSTALL_DRCACHESIM_CONFIG})
register_tool_file("drcachesim")

if (WIN32)
  # drcachesim needs these dlls (i#1737 would eliminate this)
  DynamoRIO_get_full_path(injectlib_loc drinjectlib "${location_suffix}")
  DR_install(FILES "${injectlib_loc}"  DESTINATION "${INSTALL_CLIENTS_BIN}")
  DynamoRIO_get_full_path(configlib_loc drconfiglib "${location_suffix}")
  DR_install(FILES "${configlib_loc}"  DESTINATION "${INSTALL_CLIENTS_BIN}")
  DynamoRIO_get_full_path(drlib_loc dynamorio "${location_suffix}")
  DR_install(FILES "${drlib_loc}"  DESTINATION "${INSTALL_CLIENTS_BIN}")
  add_custom_command(TARGET drcachesim POST_BUILD
    COMMAND ${CMAKE_COMMAND} ARGS -E copy ${DR_LIBRARY_BASE_DIRECTORY}/drinjectlib.dll
    ${PROJECT_BINARY_DIR}/${BUILD_CLIENTS_BIN}/drinjectlib.dll VERBATIM)
  add_custom_command(TARGET drcachesim POST_BUILD
    COMMAND ${CMAKE_COMMAND} ARGS -E copy ${DR_LIBRARY_BASE_DIRECTORY}/drconfiglib.dll
    ${PROJECT_BINARY_DIR}/${BUILD_CLIENTS_BIN}/drconfiglib.dll VERBATIM)
  add_custom_command(TARGET drcachesim POST_BUILD
    COMMAND ${CMAKE_COMMAND} ARGS -E copy ${DR_LIBRARY_OUTPUT_DIRECTORY}/dynamorio.dll
    ${PROJECT_BINARY_DIR}/${BUILD_CLIENTS_BIN}/dynamorio.dll VERBATIM)
endif ()

##################################################
# Test executables
#
# We build larger executables here.  All tests are added in suite/tests/ except unit tests.
# Be sure to give the targets qualified test names ("tool.drcache*...").

if (BUILD_TESTS)
  add_executable(tool.drcachesim.unit_tests tests/drcachesim_unit_tests.cpp)
  if (ZLIB_FOUND)
    target_link_libraries(tool.drcachesim.unit_tests drmemtrace_simulator
      drmemtrace_static drmemtrace_analyzer ${ZLIB_LIBRARIES})
  else ()
    target_link_libraries(tool.drcachesim.unit_tests drmemtrace_simulator
      drmemtrace_static drmemtrace_analyzer)
  endif ()
  add_win32_flags(tool.drcachesim.unit_tests)
  add_test(NAME tool.drcachesim.unit_tests
           COMMAND tool.drcachesim.unit_tests)

  # FIXME i#2007: fails to link on A64
  # XXX i#1997: dynamorio_static is not supported on Mac yet
  # FIXME i#2949: gcc 7.3 fails to link certain configs
  if (NOT AARCH64 AND NOT APPLE AND NOT DISABLE_FOR_BUG_2949)
    # Tests for the cache miss analyzer.
    add_executable(tool.drcachesim.miss_analyzer_unit_test tests/cache_miss_analyzer_test.cpp)
    if (ZLIB_FOUND)
      target_link_libraries(tool.drcachesim.miss_analyzer_unit_test drmemtrace_simulator
        drmemtrace_static drmemtrace_analyzer ${ZLIB_LIBRARIES})
    else ()
      target_link_libraries(tool.drcachesim.miss_analyzer_unit_test drmemtrace_simulator
        drmemtrace_static drmemtrace_analyzer)
    endif ()
    add_win32_flags(tool.drcachesim.miss_analyzer_unit_test)
    add_test(NAME tool.drcachesim.miss_analyzer_unit_test
             COMMAND tool.drcachesim.miss_analyzer_unit_test)

    add_executable(tool.drcacheoff.burst_static tests/burst_static.cpp)
    configure_DynamoRIO_static(tool.drcacheoff.burst_static)
    use_DynamoRIO_static_client(tool.drcacheoff.burst_static drmemtrace_static)
    add_win32_flags(tool.drcacheoff.burst_static)

    add_executable(tool.drcacheoff.burst_replace tests/burst_replace.cpp)
    configure_DynamoRIO_static(tool.drcacheoff.burst_replace)
    use_DynamoRIO_static_client(tool.drcacheoff.burst_replace drmemtrace_static)
    target_link_libraries(tool.drcacheoff.burst_replace drmemtrace_raw2trace)
    if (WIN32)
      # burst_replace is unusual in cramming the tracer and post-processor into the
      # same binary and we need some massaging to avoid duplicate symbol link errors.
      target_link_libraries(tool.drcacheoff.burst_replace ${static_libc})
    endif ()
    add_win32_flags(tool.drcacheoff.burst_replace)
    use_DynamoRIO_drmemtrace_tracer(tool.drcacheoff.burst_replace)
    use_DynamoRIO_extension(tool.drcacheoff.burst_replace drcovlib_static)

    add_executable(tool.drcacheoff.burst_replaceall tests/burst_replaceall.cpp)
    configure_DynamoRIO_static(tool.drcacheoff.burst_replaceall)
    use_DynamoRIO_static_client(tool.drcacheoff.burst_replaceall drmemtrace_static)
    add_win32_flags(tool.drcacheoff.burst_replaceall)
    use_DynamoRIO_extension(tool.drcacheoff.burst_replaceall drcontainers)
    use_DynamoRIO_drmemtrace_tracer(tool.drcacheoff.burst_replaceall)

    add_executable(tool.drcacheoff.burst_malloc tests/burst_malloc.cpp)
    configure_DynamoRIO_static(tool.drcacheoff.burst_malloc)
    use_DynamoRIO_static_client(tool.drcacheoff.burst_malloc drmemtrace_static)
    add_win32_flags(tool.drcacheoff.burst_malloc)

    add_executable(tool.drcacheoff.burst_threads tests/burst_threads.cpp)
    configure_DynamoRIO_static(tool.drcacheoff.burst_threads)
    use_DynamoRIO_static_client(tool.drcacheoff.burst_threads drmemtrace_static)
    add_win32_flags(tool.drcacheoff.burst_threads)
    link_with_pthread(tool.drcacheoff.burst_threads)

    add_executable(tool.drcacheoff.burst_threadfilter tests/burst_threadfilter.cpp)
    configure_DynamoRIO_static(tool.drcacheoff.burst_threadfilter)
    use_DynamoRIO_static_client(tool.drcacheoff.burst_threadfilter drmemtrace_static)
    add_win32_flags(tool.drcacheoff.burst_threadfilter)
    link_with_pthread(tool.drcacheoff.burst_threadfilter)

    if (X64 AND UNIX)
      add_executable(tool.drcacheoff.burst_noreach tests/burst_noreach.cpp)
      configure_DynamoRIO_static(tool.drcacheoff.burst_noreach)
      use_DynamoRIO_static_client(tool.drcacheoff.burst_noreach drmemtrace_static)
      add_win32_flags(tool.drcacheoff.burst_noreach)
    endif ()
    if (LINUX) # Uses mremap.
      add_executable(tool.drcacheoff.burst_maps tests/burst_maps.cpp)
      configure_DynamoRIO_static(tool.drcacheoff.burst_maps)
      use_DynamoRIO_static_client(tool.drcacheoff.burst_maps drmemtrace_static)
      add_win32_flags(tool.drcacheoff.burst_maps)
    endif ()

    if (UNIX)
      if (X86 AND NOT APPLE) # This test is x86-specific.
        # uses ptrace and looks for linux-specific syscalls
        add_executable(tool.drcacheoff.raw2trace_io tests/raw2trace_io.cpp
          tracer/instru.cpp
          tracer/instru_online.cpp)
        configure_DynamoRIO_standalone(tool.drcacheoff.raw2trace_io)
        add_win32_flags(tool.drcacheoff.raw2trace_io)
        target_link_libraries(tool.drcacheoff.raw2trace_io drmemtrace_raw2trace)
        use_DynamoRIO_extension(tool.drcacheoff.raw2trace_io droption)
        target_link_libraries(tool.drcacheoff.raw2trace_io drdecode)
        use_DynamoRIO_extension(tool.drcacheoff.raw2trace_io drcovlib_static)
        # Because we're leveraging instru_online code we have to link with drutil:
        use_DynamoRIO_extension(tool.drcacheoff.raw2trace_io drutil_static)
      endif ()

      # FIXME i#2099: the weak symbol is not supported on Windows.
      add_executable(tool.drcacheoff.burst_client tests/burst_static.cpp)
      append_property_list(TARGET tool.drcacheoff.burst_client
        COMPILE_DEFINITIONS "TEST_APP_DR_CLIENT_MAIN")
      configure_DynamoRIO_static(tool.drcacheoff.burst_client)
      use_DynamoRIO_static_client(tool.drcacheoff.burst_client drmemtrace_static)
      # A nop, keep it for the future Windows support.
      add_win32_flags(tool.drcacheoff.burst_client)
    endif ()

    set(source_abs "${CMAKE_CURRENT_SOURCE_DIR}/tests/burst_traceopts.cpp")
    set(asm_deps
      "${PROJECT_SOURCE_DIR}/core/arch/asm_defines.asm"
      "${PROJECT_BINARY_DIR}/configure.h")
    include_directories(${PROJECT_SOURCE_DIR}/core/arch) # asm_defines.asm
    add_split_asm_target("${source_abs}" asm_source gen_asm_tgt
      "_asm" "" "${asm_deps}")
    add_executable(tool.drcacheoff.burst_traceopts ${source_abs}
      ${asm_source})
    configure_DynamoRIO_static(tool.drcacheoff.burst_traceopts)
    if ("${CMAKE_GENERATOR}" MATCHES "Visual Studio")
      add_dependencies(tool.drcacheoff.burst_traceopts ${gen_asm_tgt})
    endif ()
    use_DynamoRIO_static_client(tool.drcacheoff.burst_traceopts drmemtrace_static)
    target_link_libraries(tool.drcacheoff.burst_traceopts drmemtrace_raw2trace
      drmemtrace_analyzer)
    if (WIN32)
      # Just like for burst_replace, linking everything together takes effort.
      target_link_libraries(tool.drcacheoff.burst_traceopts ${static_libc})
    endif ()
    add_win32_flags(tool.drcacheoff.burst_traceopts)
    use_DynamoRIO_drmemtrace_tracer(tool.drcacheoff.burst_traceopts)
    use_DynamoRIO_extension(tool.drcacheoff.burst_traceopts drcovlib_static)

  endif ()
endif ()

##################################################
# Documentation

# We auto-generate the list of options in the html docs via this helper app.
# i#1730: We cannot execute this when cross-compiling so for now we generate
# docs without this list (better than no docs at all).
add_executable(drcachesim_ops
  optionlist.cpp
  common/options.cpp)
set_target_properties(drcachesim_ops PROPERTIES COMPILE_FLAGS "${ORIG_CMAKE_CXX_FLAGS}")
add_win32_flags(drcachesim_ops)
use_DynamoRIO_extension(drcachesim_ops droption)

# We then have to insert it into the doxygen files at build time:
set(srcdoc ${CMAKE_CURRENT_SOURCE_DIR}/drcachesim.dox.in)
set(gendoc ${CMAKE_CURRENT_BINARY_DIR}/drcachesim.dox)
set(doctgt drcachesim_docs)

get_property(dox_extras GLOBAL PROPERTY DynamoRIO_dox_extras)
set_property(GLOBAL PROPERTY DynamoRIO_dox_extras ${dox_extras} ${gendoc})

get_property(dox_targets GLOBAL PROPERTY DynamoRIO_dox_targets)
set_property(GLOBAL PROPERTY DynamoRIO_dox_targets ${dox_targets} ${doctgt})

add_custom_target(${doctgt} DEPENDS ${gendoc})
add_custom_command(
  OUTPUT ${gendoc}
  DEPENDS ${srcdoc}
  drcachesim_ops
  common/options.h
  COMMAND ${CMAKE_COMMAND}
  ARGS -D src=${srcdoc}
       -D dst=${gendoc}
       -D CMAKE_CROSSCOMPILING=${CMAKE_CROSSCOMPILING}
       -D prog=$<TARGET_FILE:drcachesim_ops>
       -P ${CMAKE_CURRENT_SOURCE_DIR}/../common/gendocs.cmake
  VERBATIM)

# propagate to parent dir
set(exported_targets_append "${exported_targets_append}" PARENT_SCOPE)
