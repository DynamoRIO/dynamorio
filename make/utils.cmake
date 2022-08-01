# **********************************************************
# Copyright (c) 2012-2017 Google, Inc.    All rights reserved.
# **********************************************************
#
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
# ARE DISCLAIMED. IN NO EVENT SHALL GOOGLE, INC. OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
# SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
# CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
# OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
# DAMAGE.

# CMAKE_CURRENT_LIST_DIR wasn't added until 2.8.3
get_filename_component(utils_cmake_dir "${CMAKE_CURRENT_LIST_FILE}" PATH)
include(${utils_cmake_dir}/utils_exposed.cmake)

# sets CMAKE_COMPILER_IS_CLANG and CMAKE_COMPILER_IS_GNUCC in parent scope
function (identify_clang)
  _DR_identify_clang()
  if (CMAKE_COMPILER_IS_GNUCC)
    set(CMAKE_COMPILER_IS_GNUCC TRUE PARENT_SCOPE)
  endif ()
  set(CMAKE_COMPILER_IS_CLANG ${CMAKE_COMPILER_IS_CLANG} PARENT_SCOPE)
endfunction (identify_clang)

function (append_property_string type target name value)
  _DR_append_property_string(${type} ${target} ${name} "${value}")
endfunction (append_property_string)

function (append_property_list type target name value)
  # XXX: if we require cmake 2.8.6 we can simply use APPEND_LIST
  get_property(cur ${type} ${target} PROPERTY ${name})
  if (cur)
    set(value ${cur} ${value})
  endif (cur)
  set_property(${type} ${target} PROPERTY ${name} ${value})
endfunction (append_property_list)

function(get_full_path out target loc_suffix)
  DynamoRIO_get_full_path(local ${target} "${loc_suffix}")
  set(${out} ${local} PARENT_SCOPE)
endfunction (get_full_path)

function (get_target_path_for_execution out target loc_suffix)
  if (ANDROID)
    DynamoRIO_get_target_path_for_execution(local ${target} ${DR_DEVICE_BASEDIR} "${loc_suffix}")
  else ()
    DynamoRIO_get_target_path_for_execution(local ${target} "" "${loc_suffix}")
  endif ()
  set(${out} ${local} PARENT_SCOPE)
endfunction (get_target_path_for_execution)

function (prefix_cmd_if_necessary cmd_out use_ats cmd_in)
  DynamoRIO_prefix_cmd_if_necessary(local ${use_ats} ${cmd_in} ${ARGN})
  set(${cmd_out} ${local} PARENT_SCOPE)
endfunction (prefix_cmd_if_necessary)

# i#1873: we copy individual targets for speed, using up less space, and to
# support "make test" as well as a much nicer rebuild-and-rerun dev model,
# rather than a massive "adb push" at the very end of the build.
function (copy_target_to_device target loc_suffix)
  if (DR_COPY_TO_DEVICE)
    DynamoRIO_copy_target_to_device(${target} ${DR_DEVICE_BASEDIR} "${loc_suffix}")
  endif ()
endfunction (copy_target_to_device)

function (add_rel_rpaths target)
  DynamoRIO_add_rel_rpaths(${target} ${ARGN})
endfunction (add_rel_rpaths)

function (check_if_linker_is_gnu_gold var_out)
  _DR_check_if_linker_is_gnu_gold(is_gold)
  set(${var_out} ${is_gold} PARENT_SCOPE)
endfunction (check_if_linker_is_gnu_gold)

# disable known warnings
function (disable_compiler_warnings)
  if (WIN32)
    # disable stack protection: "unresolved external symbol ___security_cookie"
    # disable the warning "unreferenced formal parameter" #4100
    # disable the warning "conditional expression is constant" #4127
    # disable the warning "cast from function pointer to data pointer" #4054
    set(CL_CFLAGS "/GS- /wd4100 /wd4127 /wd4054")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${CL_CFLAGS}" PARENT_SCOPE)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${CL_CFLAGS}" PARENT_SCOPE)
    add_definitions(-D_CRT_SECURE_NO_WARNINGS)
  endif (WIN32)
endfunction (disable_compiler_warnings)

# clients/extensions don't include configure.h so they don't get DR defines
macro (add_dr_defines)
  foreach (config "" ${CMAKE_BUILD_TYPE} ${CMAKE_CONFIGURATION_TYPES})
    if ("${config}" STREQUAL "")
      set(config_upper "")
    else ("${config}" STREQUAL "")
      string(TOUPPER "_${config}" config_upper)
    endif ("${config}" STREQUAL "")
    foreach (var CMAKE_C_FLAGS${config_upper};CMAKE_CXX_FLAGS${config_upper})
      if (DEBUG)
        set(${var} "${${var}} -DDEBUG")
      endif (DEBUG)
      # we're used to X64 instead of X86_64
      if (X64)
        set(${var} "${${var}} -DX64")
      endif (X64)
    endforeach (var)
  endforeach (config)
endmacro (add_dr_defines)

macro (install_subdirs tgt_lib tgt_bin)
  # These cover all subdirs.
  # Subdirs just need to install their targets.
  DR_install(DIRECTORY ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/
    DESTINATION ${tgt_lib}
    FILE_PERMISSIONS ${owner_access} OWNER_EXECUTE GROUP_READ GROUP_EXECUTE
    WORLD_READ WORLD_EXECUTE
    FILES_MATCHING
    PATTERN "*.debug"
    PATTERN "*.pdb"
    REGEX ".*.dSYM/.*DWARF/.*" # too painful to get right # of backslash for literal .
    ${ARGN}
    )
  # We rely on our shared library targets being redirected to
  # CMAKE_LIBRARY_OUTPUT_DIRECTORY in order to copy the shared lib pdbs
  # and executable pdbs into the right places.  Callers can use
  # place_shared_lib_in_lib_dir() to accomplish this.
  DR_install(DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/
    DESTINATION ${tgt_bin}
    FILE_PERMISSIONS ${owner_access} OWNER_EXECUTE GROUP_READ GROUP_EXECUTE
    WORLD_READ WORLD_EXECUTE
    FILES_MATCHING
    PATTERN "*.debug"
    PATTERN "*.pdb"
    REGEX ".*.dSYM/.*DWARF/.*" # too painful to get right # of backslash for literal .
    ${ARGN}
    )
endmacro (install_subdirs)

# Use this to put shared libraries in the lib dir to separate them from
# executables in the output dir.
function (place_shared_lib_in_lib_dir target)
  set_target_properties(${target} PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY${location_suffix} "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}"
    RUNTIME_OUTPUT_DIRECTORY${location_suffix} "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}"
    ARCHIVE_OUTPUT_DIRECTORY${location_suffix} "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}")
endfunction ()

function (check_avx_processor_and_compiler_support out)
  if (WIN32)
    # XXX i#1312: add Windows support.
    message(FATAL_ERROR "Windows not supported yet.")
  endif ()
  include(CheckCSourceRuns)
  set(avx_prog "#include <immintrin.h>
                int main() {
                  register __m256 ymm0 asm(\"ymm0\");
                  (void)ymm0;
                  asm volatile(\"vmovdqu %ymm0, %ymm1\");
                  return 0;
                }")
  set(CMAKE_REQUIRED_FLAGS ${CFLAGS_AVX})
  check_c_source_runs("${avx_prog}" proc_found_avx)
  if (proc_found_avx)
    message(STATUS "Compiler and processor support AVX.")
  else ()
    message(STATUS "WARNING: Compiler or processor do not support AVX. "
                   "Skipping tests")
  endif ()
  set(${out} ${proc_found_avx} PARENT_SCOPE)
endfunction (check_avx_processor_and_compiler_support)

function (check_avx2_processor_and_compiler_support out)
  if (WIN32)
    # XXX i#1312: add Windows support.
    message(FATAL_ERROR "Windows not supported yet.")
  endif ()
  include(CheckCSourceRuns)
  set(avx2_prog "#include <immintrin.h>
                int main() {
                  register __m256 ymm0 asm(\"ymm0\");
                  (void)ymm0;
                  asm volatile(\"vpsravd %ymm0, %ymm1, %ymm0\");
                  return 0;
                }")
  set(CMAKE_REQUIRED_FLAGS ${CFLAGS_AVX2})
  check_c_source_runs("${avx2_prog}" proc_found_avx2)
  if (proc_found_avx2)
    message(STATUS "Compiler and processor support AVX2.")
  else ()
    message(STATUS "WARNING: Compiler or processor do not support AVX2. "
                   "Skipping tests")
  endif ()
  set(${out} ${proc_found_avx2} PARENT_SCOPE)
endfunction (check_avx2_processor_and_compiler_support)

function (check_avx512_processor_and_compiler_support out)
  if (WIN32)
    # XXX i#1312: add Windows support.
    message(FATAL_ERROR "Windows not supported yet.")
  endif ()
  include(CheckCSourceRuns)
  set(avx512_prog "#include <immintrin.h>
                   int main() {
                     register __m512 zmm0 asm(\"zmm0\");
                     (void)zmm0;
                     asm volatile(\"vmovdqu64 %zmm0, %zmm1\");
                     return 0;
                   }")
  set(CMAKE_REQUIRED_FLAGS ${CFLAGS_AVX512})
  check_c_source_runs("${avx512_prog}" proc_found_avx512)
  if (proc_found_avx512)
    message(STATUS "Compiler and processor support AVX-512.")
  else ()
    message(STATUS "WARNING: Compiler or processor do not support AVX-512. "
                   "Skipping tests")
  endif ()
  set(${out} ${proc_found_avx512} PARENT_SCOPE)
endfunction (check_avx512_processor_and_compiler_support)

# Check if the building machine support Intel PT.
# This function only checks if PT-related tests need to be built. PT-capable
# binaries can be built on any system. When building an export module, please
# do not use it to check if PT-related libraries need to be built.
function(check_intel_pt_support out)
  if (NOT LINUX OR NOT X86 OR NOT X64)
    message(STATUS "Intel PT not supported on this platform.")
    set(${out} 0 PARENT_SCOPE)
  else ()
    if (EXISTS "/sys/devices/intel_pt/type")
      message(STATUS "Intel PT is available.")
      set(${out} 1 PARENT_SCOPE)
    else ()
      message(STATUS "Intel PT not found.")
      set(${out} 0 PARENT_SCOPE)
    endif()
  endif ()
endfunction(check_intel_pt_support)

if (UNIX)
  # We always use a script for our own library bounds (PR 361594).
  # We could build this at configure time instead of build time as
  # it does not depend on the source files.
  # XXX: this is duplicated in DynamoRIOConfig.cmake

  function (set_preferred_base_start_and_end target base set_bounds)
    if (ANDROID)
      # i#1863: Android's loader doesn't support a non-zero base.
      # Turning off SET_PREFERRED_BASE is not sufficient as we get
      # a 0x10000 base.
      set(base 0)
    endif ()
    if (APPLE)
      set(ldflags "-image_base ${base}")
    elseif (NOT LINKER_IS_GNU_GOLD)
      set(ld_script ${CMAKE_CURRENT_BINARY_DIR}/${target}.ldscript)
      set_directory_properties(PROPERTIES
        ADDITIONAL_MAKE_CLEAN_FILES "${ld_script}")
      set(ldflags "-Wl,${ld_script_option},\"${ld_script}\"")
      add_custom_command(TARGET ${target}
        PRE_LINK
        COMMAND ${CMAKE_COMMAND}
        # script does not inherit any vars so we must pass them all in
        # to work around i#84 be sure to put a space after -D for 1st arg at least
        ARGS -D outfile=${ld_script}
             -DCMAKE_LINKER=${CMAKE_LINKER}
             -DCMAKE_COMPILER_IS_GNUCC=${CMAKE_COMPILER_IS_GNUCC}
             -DLD_FLAGS=${LD_FLAGS}
             -Dset_preferred=${SET_PREFERRED_BASE}
             -Dreplace_maxpagesize=${REPLACE_MAXPAGESIZE}
             -Dpreferred_base=${base}
             -Dadd_bounds_vars=${set_bounds}
             -P ${PROJECT_SOURCE_DIR}/make/ldscript.cmake
        VERBATIM # recommended: p260
        )
    else ()
      set(ldflags "")
      if (SET_PREFERRED_BASE)
        # FIXME: This should be -Ttext-segment for bfd, but most golds want -Ttext.
        # See http://sourceware.org/ml/binutils/2013-02/msg00194.html
        set(ldflags "-Wl,-Ttext=${base}")
      endif ()
      if (set_bounds)
        # Add our start and end symbols for library bounds.
        # XXX: hardcoded to dynamorio for now: generalize when necessary.
        set(ldflags "${ldflags} -Wl,--defsym,dynamorio_so_start=__executable_start")
        set(ldflags "${ldflags} -Wl,--defsym,dynamorio_so_end=end")
      endif ()
    endif ()
    if (ARM AND NOT set_bounds) # XXX: don't want for libDR: using "set_bounds" for now
      # Somehow the entry point gets changed to A32 by using ldscript.
      # Adding --entry causes the linker to set it to either A32 or T32 as appropriate.
      set(ldflags "${ldflags} -Wl,--entry -Wl,_start")
    endif ()
    append_property_string(TARGET ${target} LINK_FLAGS "${ldflags}")
  endfunction (set_preferred_base_start_and_end)

endif (UNIX)
