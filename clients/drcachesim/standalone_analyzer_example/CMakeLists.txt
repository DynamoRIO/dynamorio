cmake_minimum_required(VERSION 3.10)
project(analyzer_example)

find_package(DynamoRIO)
if (NOT DynamoRIO_FOUND)
    message(FATAL_ERROR "DynamoRIO package required to build")
endif(NOT DynamoRIO_FOUND)

set (target analyzer_example)
add_executable (${target} ${target}.cpp)

# The target OS:
if (APPLE)
    set(MACOS 1)
elseif (UNIX)
    set(LINUX 1)
endif (APPLE)
if (WIN32)
    set(WINDOWS 1)
endif (WIN32)

if (LINUX)
    target_compile_definitions(${target} PRIVATE -DLINUX -DUNIX)
elseif (WINDOWS)
    target_compile_definitions(${target} PRIVATE WINDOWS)
endif (LINUX)

target_compile_definitions(${target} PRIVATE X86_64 X64)

configure_DynamoRIO_main_headers()
use_DynamoRIO_drmemtrace(${target})
use_DynamoRIO_extension(${target} droption)
use_DynamoRIO_extension(${target} drfrontendlib)

target_link_libraries(${target} drmemtrace_analyzer)
