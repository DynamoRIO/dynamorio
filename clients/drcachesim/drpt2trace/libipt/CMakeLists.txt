# **********************************************************
# Copyright (c) 2023 Google, Inc.    All rights reserved.
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

include(../../../../make/policies.cmake NO_POLICY_SCOPE)

if (NOT LINUX OR NOT X86 OR NOT X64)
  message(FATAL_ERROR "This is only for Linux x86_64.")
endif ()

set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/third_party/${INSTALL_LIB}")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}")

set(PT_VERSION_MAJOR 2)
set(PT_VERSION_MINOR 1)
set(PT_VERSION_PATCH 0)
set(PT_VERSION_BUILD "0" CACHE STRING "")
set(PT_VERSION_EXT "" CACHE STRING "")

set(PT_VERSION "${PT_VERSION_MAJOR}.${PT_VERSION_MINOR}.${PT_VERSION_PATCH}")

add_definitions(
  -DPT_VERSION_MAJOR=${PT_VERSION_MAJOR}
  -DPT_VERSION_MINOR=${PT_VERSION_MINOR}
  -DPT_VERSION_PATCH=${PT_VERSION_PATCH}
  -DPT_VERSION_BUILD=${PT_VERSION_BUILD}
  -DPT_VERSION_EXT=\"${PT_VERSION_EXT}\"
)

add_definitions(
  -DFEATURE_SIDEBAND
  -DFEATURE_ELF
  -DFEATURE_PEVENT
  -DFEATURE_THREADS
)

include_directories(
  ${PROJECT_SOURCE_DIR}/third_party/libipt/include
  ${PROJECT_SOURCE_DIR}/third_party/libipt/include/posix
  ${PROJECT_BINARY_DIR}/third_party/include/libipt
)

set(LIBIPT_ROOT ${PROJECT_SOURCE_DIR}/third_party/libipt/libipt)

include_directories(
    ${LIBIPT_ROOT}/internal/include
    ${LIBIPT_ROOT}/internal/include/posix
)

set(LIBIPT_FILES
  ${LIBIPT_ROOT}/src/pt_error.c
  ${LIBIPT_ROOT}/src/pt_packet_decoder.c
  ${LIBIPT_ROOT}/src/pt_event_decoder.c
  ${LIBIPT_ROOT}/src/pt_query_decoder.c
  ${LIBIPT_ROOT}/src/pt_encoder.c
  ${LIBIPT_ROOT}/src/pt_sync.c
  ${LIBIPT_ROOT}/src/pt_version.c
  ${LIBIPT_ROOT}/src/pt_last_ip.c
  ${LIBIPT_ROOT}/src/pt_tnt_cache.c
  ${LIBIPT_ROOT}/src/pt_ild.c
  ${LIBIPT_ROOT}/src/pt_image.c
  ${LIBIPT_ROOT}/src/pt_image_section_cache.c
  ${LIBIPT_ROOT}/src/pt_retstack.c
  ${LIBIPT_ROOT}/src/pt_insn_decoder.c
  ${LIBIPT_ROOT}/src/pt_time.c
  ${LIBIPT_ROOT}/src/pt_asid.c
  ${LIBIPT_ROOT}/src/pt_event_queue.c
  ${LIBIPT_ROOT}/src/pt_packet.c
  ${LIBIPT_ROOT}/src/pt_config.c
  ${LIBIPT_ROOT}/src/pt_insn.c
  ${LIBIPT_ROOT}/src/pt_block_decoder.c
  ${LIBIPT_ROOT}/src/pt_block_cache.c
  ${LIBIPT_ROOT}/src/pt_msec_cache.c
  ${LIBIPT_ROOT}/src/pt_section.c
  ${LIBIPT_ROOT}/src/pt_section_file.c
  ${LIBIPT_ROOT}/src/posix/pt_section_posix.c
)

add_library(ipt STATIC ${LIBIPT_FILES})
DR_export_target(ipt)
install_exported_target(ipt ${INSTALL_CLIENTS_LIB})

# put the version into the intel-pt header
configure_file(
  ${LIBIPT_ROOT}/include/intel-pt.h.in
  ${PROJECT_BINARY_DIR}/third_party/include/libipt/intel-pt.h
)

include_directories(
  ${PROJECT_SOURCE_DIR}/third_party/libipt/pevent/include
)

add_library(pevent STATIC
  ${PROJECT_SOURCE_DIR}/third_party/libipt/pevent/src/pevent.c
)
DR_export_target(pevent)
install_exported_target(pevent ${INSTALL_CLIENTS_LIB})

set(LIBSB_ROOT ${PROJECT_SOURCE_DIR}/third_party/libipt/sideband)

include_directories(
  ${LIBSB_ROOT}/internal/include
)

set(LIBSB_FILES
  ${LIBSB_ROOT}/src/pt_sb_session.c
  ${LIBSB_ROOT}/src/pt_sb_context.c
  ${LIBSB_ROOT}/src/pt_sb_file.c
  ${LIBSB_ROOT}/src/pt_sb_pevent.c
)

add_library(ipt-sb STATIC ${LIBSB_FILES})

configure_file(
  ${LIBSB_ROOT}/include/libipt-sb.h.in
  ${PROJECT_BINARY_DIR}/third_party/include/libipt/libipt-sb.h
)

target_link_libraries(ipt-sb ipt)
target_link_libraries(ipt-sb pevent)
DR_export_target(ipt-sb)
install_exported_target(ipt-sb ${INSTALL_CLIENTS_LIB})
