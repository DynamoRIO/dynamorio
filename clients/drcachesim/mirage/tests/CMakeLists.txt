cmake_minimum_required(VERSION 3.7)

include (../../../../make/policies.cmake NO_POLICY_SCOPE)

add_dr_defines()

add_executable(test_replayer_all test_replayer_all.cpp)

target_link_libraries(test_replayer_all mirage)