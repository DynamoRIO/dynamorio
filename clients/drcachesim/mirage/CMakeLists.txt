cmake_minimum_required(VERSION 3.7)

include (../../../make/policies.cmake NO_POLICY_SCOPE)

if (NOT LINUX OR NOT X86 OR NOT X64)
  message(FATAL_ERROR "This is only for Linux x86_64.")
endif ()

add_dr_defines()

set(mirage_srcs
    dr_mir_api.cpp
    # common - shared code
    ./common/list.cpp
    ./common/bitmap.cpp
    # frontend - translating DRIR to MIR
    ./fe/gen_ops.cpp
    ./fe/gen_opnd_api.cpp
    ./fe/translate_context.cpp
    # backend - interpreting MIR
    # ./be/*.cpp
    # interpreter - MIR specification
    ./ir/mir_insn.cpp
    ./ir/mir_opnd.cpp
)

add_library(mirage STATIC ${mirage_srcs})

target_include_directories(mirage PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/common>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/fe>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/ir>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/be>
)

configure_DynamoRIO_decoder(mirage)
add_dependencies(mirage api_headers)
target_link_libraries(mirage)
install_client_nonDR_header(drmemtrace dr_mir_api.h)
DR_export_target(mirage)
install_exported_target(mirage ${INSTALL_CLIENTS_LIB})