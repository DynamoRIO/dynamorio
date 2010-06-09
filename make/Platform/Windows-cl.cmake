# i#310: support building over cygwin ssh where we cannot build pdbs.
# To prevent cmake's try-compile for its working compiler test and
# its ABI determination test we request a Release build config
# via a custom Plaform/Windows-cl.cmake in our make/ dir.
include(${CMAKE_ROOT}/Modules/Platform/Windows-cl.cmake)
set(CMAKE_BUILD_TYPE_INIT Release)
