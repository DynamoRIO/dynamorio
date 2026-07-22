# Builds the DynamoRIO Linux kernel module via the kernel's own Kbuild
# system. Kernel modules can't be compiled with CMake's normal compiler
# invocation, so this shells out to `make` against KERNEL_DIR and
# MAKE_PROGRAM, both validated and cached by the parent CMakeLists.txt.

set(dynamorio_module_name dynamorio_module)

# Single source of truth for module sources. Add new files here only.
# This list drives both CMake's own rebuild-staleness tracking (DEPENDS
# below) and the generated Kbuild Makefile's object list, so the two can't
# drift out of sync as the module grows.
#
# Note: No source file may be named "${dynamorio_module_name}.c" because
# Kbuild composite targets cannot share a name with a component object file.
set(dynamorio_module_sources dynamorio_module_main.c)

set(dynamorio_module_ko "${CMAKE_CURRENT_SOURCE_DIR}/${dynamorio_module_name}.ko")
set(dynamorio_module_makefile "${CMAKE_CURRENT_SOURCE_DIR}/Makefile")

list(TRANSFORM dynamorio_module_sources REPLACE "\\.c$" ".o"
     OUTPUT_VARIABLE dynamorio_module_objs)
string(REPLACE ";" " " dynamorio_module_objs "${dynamorio_module_objs}")
set(dynamorio_module_kbuild_extra "${dynamorio_module_name}-y := ${dynamorio_module_objs}")

configure_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/Makefile.in
  ${dynamorio_module_makefile}
  @ONLY
)

list(TRANSFORM dynamorio_module_sources PREPEND "${CMAKE_CURRENT_SOURCE_DIR}/")

add_custom_command(
  OUTPUT ${dynamorio_module_ko}
  COMMAND ${MAKE_PROGRAM} -C ${KERNEL_DIR} M=${CMAKE_CURRENT_SOURCE_DIR} modules
  DEPENDS ${dynamorio_module_sources} ${dynamorio_module_makefile}
  COMMENT "Building DynamoRIO Linux kernel module (${dynamorio_module_name}.ko) against KERNEL_DIR=${KERNEL_DIR}"
  VERBATIM
)

add_custom_target(dynamorio_module ALL DEPENDS ${dynamorio_module_ko})

# Standard `make clean` cannot remove Kbuild bookkeeping files.
# Run `make clean_dynamorio_module` to invoke Kbuild's clean target.
add_custom_target(clean_dynamorio_module
  COMMAND ${MAKE_PROGRAM} -C ${KERNEL_DIR} M=${CMAKE_CURRENT_SOURCE_DIR} clean
)
