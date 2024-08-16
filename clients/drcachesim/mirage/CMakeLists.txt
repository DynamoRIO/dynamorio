cmake_minimum_required(VERSION 3.7)

include (../../../make/policies.cmake NO_POLICY_SCOPE)

if (NOT LINUX OR NOT X86 OR NOT X64)
  message(FATAL_ERROR "This is only for Linux x86_64.")
endif ()

add_dr_defines()

set(mirage_srcs
    dr_mir_api.cpp
    # frontend - translating DRIR to MIR
    ./fe/gen_ops.cpp
    # backend - interpreting MIR
    # ./be/*.cpp
    # interpreter - MIR specification
    # ./ir/*.cpp
)

add_library(mirage STATIC ${mirage_srcs})
configure_DynamoRIO_decoder(mirage)
add_dependencies(mirage api_headers)
target_link_libraries(mirage)
install_client_nonDR_header(drmemtrace dr_mir_api.h)
DR_export_target(mirage)
install_exported_target(mirage ${INSTALL_CLIENTS_LIB})