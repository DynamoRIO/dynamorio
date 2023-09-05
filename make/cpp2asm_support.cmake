# **********************************************************
# Copyright (c) 2010-2022 Google, Inc.    All rights reserved.
# Copyright (c) 2009-2010 VMware, Inc.    All rights reserved.
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
# * Neither the name of VMware, Inc. nor the names of its contributors may be
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

###########################################################################
# Enhanced assembly support for CMake projects
#
# Features:
# + Cmake support for asm with Makefiles and Visual Studio.
# + Cross-platform asm using Intel syntax and C pre-processor
#   to feed to assembler.
# + Asm code in C/C++ files (separated via #ifdef ASM_CODE_ONLY) to avoid
#   needing standalone assembly files with 64-bit cl.
#
###################################################
# Usage:
#
# Setup:
# + Place both this file and cpp2asm_add_newlines.cmake in the same
#   directory (or, point at cpp2asm_add_newlines.cmake with the
#   variable ${cpp2asm_newline_script_path}).
# + Include this file in your CMakeLists.txt file.
# + For UNIX, the available assembler may not support Intel syntex.
#   In that case, CMAKE_ASM_SUPPORTS_INTEL_SYNTAX will be set to false.
#   The includer of this file can decide whether that's a fatal error.
#
# Example setup:
#   include(cpp2asm_support.cmake)
#   if (UNIX)
#     if (NOT CMAKE_ASM_SUPPORTS_INTEL_SYNTAX)
#       <either FATAL_ERROR or point CMAKE_ASM_COMPILER at a gas binary to use>
#     endif ()
#   endif (UNIX)
#
# Assembly file:
# + Write assembly using Intel syntax and the macros in the
#   cpp2asm_defines.h file included in this directory.
# + Before #include of cpp2asm_defines.h in your asm file, define
#   one of ASSEMBLE_WITH_{GAS,MASM,NASM}
# + cpp2asm_defines.h is not automatically added as a dependence.
#   If you are using it, pass in its path as part of the extra_deps option.
# + cpp2asm_defines.h's dir is not automatically added as an include dir.
#   If you are using it, pass in "-I;<path>" as part of the extra_defs option.
# + The global preprocessor defines are used for Makefile generators.
#   For Visual Studio generators, pass in the defines needed (as a list)
#   as part of the extra_defs option.  These extra_defs are NOT honored
#   for Makefile generators and should be added by the caller to the
#   target or global flags.
# + Have assembly files #include a configure.h or other header to obtain
#   their defines (this file does not support passing them in via cmdline
#   to the preprocessor or the assembler).
# + For pure assembly files, use add_asm_target(); for split C/C++ + asm files,
#   separate the two pieces via #ifdef ASM_CODE_ONLY and use
#   add_split_asm_target().
# + For VS generators, add the returned target as a dependence of the final
#   executable/library target to avoid races when building.
#
# Example:
#
#   set(asm_deps "/path/to/cpp2asm_defines.h" "/path/to/configure.h")
#   set(asm_defs -I "/path/to/cpp2asm_defines.h" -I "/path/to/configure.h")
#   add_asm_target(foo.asm foo_asm_src foo_asm_tgt "" "${asm_defs}" ${asm_deps})
#   add_library(bar bar.c ${foo_asm_src})
#   if ("${CMAKE_GENERATOR}" MATCHES "Visual Studio")
#     add_dependencies(bar ${foo_asm_tgt})
#   endif ("${CMAKE_GENERATOR}" MATCHES "Visual Studio")
#
###########################################################################

if ("${CMAKE_GENERATOR}" MATCHES "Visual Studio")
  string(REPLACE "Visual Studio " "" vsgen_ver "${CMAKE_GENERATOR}")
else ()
  set(vsgen_ver "0.0")
endif ()

if (${vsgen_ver} VERSION_GREATER 9)
  # For i#801 workaround
  cmake_minimum_required(VERSION 3.7)
else ()
  if (APPLE)
    # We want ASM NASM support
    cmake_minimum_required(VERSION 3.7)
  else (APPLE)
    # Require 2.6.4 to avoid cmake bug #8639, unless this var is
    # properly set (which it is for DR b/c it has a workaround):
    if (NOT "${CMAKE_ASM_SOURCE_FILE_EXTENSIONS}" MATCHES "asm")
      cmake_minimum_required(VERSION 3.7)
    endif ()
  endif (APPLE)
endif ()

##################################################
# Helper files

if (NOT cpp2asm_newline_script_path)
  # By default we expect cpp2asm_add_newlines.cmake in same dir as this file.
  get_filename_component(my_dir "${CMAKE_CURRENT_LIST_FILE}" PATH)
  set(cpp2asm_newline_script_path "${my_dir}/cpp2asm_add_newlines.cmake")
endif ()
if (NOT EXISTS "${cpp2asm_newline_script_path}")
  message(FATAL_ERROR "Cannot find \"${cpp2asm_newline_script_path}\". "
    "Please set the cpp2asm_newline_script_path variable.")
endif ()

##################################################
# Preprocessor location and flags

if (MSVC)
  set(CMAKE_CPP ${CMAKE_C_COMPILER})

  set(CPP_KEEP_COMMENTS /C)
  set(CPP_NO_LINENUM /EP)
  set(CPP_KEEP_WHITESPACE "")
  set(CMAKE_CPP_FLAGS "/nologo")
else ()
  set(CPP_KEEP_COMMENTS -C)
  set(CPP_NO_LINENUM -P)
  set(CPP_KEEP_WHITESPACE -traditional-cpp)

  # Allow user to set C pre-processor from environment variable
  if (DEFINED ENV{CPP})
    set(CMAKE_CPP $ENV{CPP})
    set(CMAKE_CPP_FLAGS "")
  else ()
    # Cmake-discovered compiler on macOS does not include system
    # directories by default.
    if (APPLE)
      find_program(CMAKE_CPP cc)
    else ()
      set(CMAKE_CPP ${CMAKE_C_COMPILER})
    endif ()
    set(CMAKE_CPP_FLAGS "-xc")
  endif ()
  mark_as_advanced(CMAKE_CPP)
endif (MSVC)

##################################################
# Assembler location and flags

if (NOT "${CMAKE_GENERATOR}" MATCHES "Visual Studio")
  if (UNIX)
    # i#646: cmake 2.8.5 requires ASM-ATT to find as: but we don't want to change
    # all our vars to have -ATT suffix so we set the search list instead.
    # This must be done prior to enable_language(ASM).
    set(CMAKE_ASM_COMPILER_INIT gas as)
  endif (UNIX)
  # CMake does not support assembly with VS generators
  # (http://public.kitware.com/Bug/view.php?id=11536)
  # so we have to add our own custom commands and targets
  if (APPLE AND NOT AARCH64)
    # NASM support was added in 2.8.3.  It clears ASM_DIALECT for us.
    enable_language(ASM_NASM)
  else (APPLE AND NOT AARCH64)
    enable_language(ASM)
  endif ()
endif ()

if (APPLE)
  if (AARCH64)
    set(CMAKE_ASM_COMPILER "clang")
    set(ASM_FLAGS "${ASM_FLAGS} -c")
    if (DEFINED OLDEST_OSX_SUPPPORTED)
      set(ASM_FLAGS "${ASM_FLAGS} -mmacosx-version-min=${OLDEST_OSX_SUPPPORTED}")
    endif ()
  else (AARCH64)
    # XXX: we may be able to avoid some of this given CMake 2.8.3's NASM support.
    find_program(NASM nasm DOC "path to nasm assembler")
    if (NOT NASM)
      message(FATAL_ERROR "nasm assembler not found: required to build")
    endif (NOT NASM)
    message(STATUS "Found nasm: ${NASM}")
    execute_process(COMMAND ${NASM} -hf OUTPUT_VARIABLE nasm_result ERROR_QUIET)
    if (X64)
      # x64 support added between 0.98.40 and 2.10.07
      if (NOT nasm_result MATCHES macho64)
        message(FATAL_ERROR "nasm is too old: no 64-bit support")
      endif ()
      set(ASM_FLAGS "${ASM_FLAGS} -fmacho64")
    else (X64)
      if (nasm_result MATCHES macho32)
        set(ASM_FLAGS "${ASM_FLAGS} -fmacho32")
      else ()
        set(ASM_FLAGS "${ASM_FLAGS} -fmacho")
      endif ()
    endif (X64)
  endif (AARCH64)
  if (DEBUG)
    set(ASM_FLAGS "${ASM_FLAGS} -g")
  endif (DEBUG)
elseif (UNIX)
  if (DR_HOST_X86)
    set(ASM_FLAGS "-mmnemonic=intel -msyntax=intel -mnaked-reg")
    if (X64)
      set(ASM_FLAGS "${ASM_FLAGS} --64")
    else (X64)
      # putting --32 last so we fail on -mmnemonic=intel on older as, not --32
      set(ASM_FLAGS "${ASM_FLAGS} --32")
    endif (X64)
  elseif (DR_HOST_ARM)
    # No 64-bit support yet.
    # Some tests and libgcc/arm use deprecated instructions, disable warnings.
    set(ASM_FLAGS "${ASM_FLAGS} -mfpu=neon -mno-warn-deprecated")
  endif ()
  set(ASM_FLAGS "${ASM_FLAGS} --noexecstack")
  if (DEBUG)
    set(ASM_FLAGS "${ASM_FLAGS} -g")
  endif (DEBUG)
else ()
  if (X64)
    # In case of NMake Makefiles CMAKE_ASM_COMPILER will find cl.exe, thus we need
    # to do it manually for both ml.exe and ml64.exe
    find_program(CMAKE_ASM_COMPILER ml64.exe HINTS "${cl_path}" DOC "path to assembler")
    if ("${CMAKE_GENERATOR}" MATCHES "NMake Makefiles")
      set(CMAKE_ASM_COMPILER "ml64.exe")
    endif()
  else (X64)
    find_program(CMAKE_ASM_COMPILER ml.exe HINTS "${cl_path}" DOC "path to assembler")
    if ("${CMAKE_GENERATOR}" MATCHES "NMake Makefiles")
      set(CMAKE_ASM_COMPILER "ml.exe")
    endif()
  endif (X64)
  if (NOT CMAKE_ASM_COMPILER)
    message(FATAL_ERROR "assembler not found: required to build")
  endif (NOT CMAKE_ASM_COMPILER)
  message(STATUS "Found assembler: ${CMAKE_ASM_COMPILER}")
  if (NOT DEFINED GENERATE_PDBS OR GENERATE_PDBS)
    set(ASM_DBG "/Zi /Zd")
  else ()
    set(ASM_DBG "")
  endif ()
  set(ASM_FLAGS "/nologo ${ASM_DBG}")
  if (${vsgen_ver} VERSION_GREATER 10 AND NOT X64)
    # i#893: VS2012 adds default /safeseh for C files and we must match for asm
    # (needed for 32-bit only, and in fact /safeseh flag is not supported by ml64).
    set(ASM_FLAGS "${ASM_FLAGS} /safeseh")
  endif ()
endif ()
string(REPLACE " " ";" ASM_FLAGS_LIST "${ASM_FLAGS}")

if (APPLE)
  # XXX: xcode assembler uses Intel mnemonics but opposite src,dst order!
  # We rely on having nasm installed as a workaround.
  set(CMAKE_ASM_SUPPORTS_INTEL_SYNTAX ON)
endif (APPLE)
if (UNIX AND NOT APPLE)
  # We require gas >= 2.18.50 for --32, --64, and the new -msyntax=intel, etc.
  execute_process(COMMAND
    ${CMAKE_ASM_COMPILER} --help
    RESULT_VARIABLE asm_result
    ERROR_VARIABLE asm_error
    OUTPUT_VARIABLE asm_out)
  if (asm_result OR asm_error)
    message(FATAL_ERROR "*** ${CMAKE_ASM_COMPILER} failed: ***\n${asm_error}")
  endif (asm_result OR asm_error)
  # turn the flags into a vector
  string(REGEX REPLACE " " ";" flags_needed "${ASM_FLAGS}")
  # -mfpu= does not list the possibilities
  string(REGEX REPLACE "-mfpu=[a-z]*" "-mfpu" flags_needed "${flags_needed}")
  # we want "-mmnemonic=intel" to match "-mmnemonic=[att|intel]"
  string(REGEX REPLACE "=" ".*" flags_needed "${flags_needed}")
  set(flag_present 1)
  foreach (flag ${flags_needed})
    if (flag_present)
      string(REGEX MATCH "${flag}" flag_present "${asm_out}")
      if (NOT flag_present)
        message("${CMAKE_ASM_COMPILER} missing flag \"${flag}\"")
      endif (NOT flag_present)
    endif (flag_present)
  endforeach (flag)
  # let caller decide whether a fatal error or not
  if (NOT flag_present)
    set(CMAKE_ASM_SUPPORTS_INTEL_SYNTAX OFF)
  else (NOT flag_present)
    set(CMAKE_ASM_SUPPORTS_INTEL_SYNTAX ON)
  endif (NOT flag_present)
endif (UNIX AND NOT APPLE)

##################################################
# Assembler build rule for Makefile generators

# i#1792: cmake 3.4 splits off <INCLUDES>.
# We do not want <FLAGS> since it passes things like --MD which fail cpp.
# This means we can't have -DFOO in COMPILE_FLAGS on targets: we have to
# properly use COMPILE_DEFINITIONS for anything affecting asm code.
set(rule_flags "<INCLUDES>")
# Unfortunately we do have some flags we need to pass to cpp.
# We special-case them here:
if (proc_supports_avx512)
  set(rule_flags "${rule_flags} ${CFLAGS_AVX512}")
elseif (proc_supports_avx2)
  set(rule_flags "${rule_flags} ${CFLAGS_AVX2}")
elseif (proc_supports_avx)
  set(rule_flags "${rule_flags} ${CFLAGS_AVX}")
endif ()

# Include a define that can be used to identify asm code.
set(rule_defs "<DEFINES> -DCPP2ASM")
# Don't use "--defsym" since we pass this to cpp.
set(CMAKE_ASM_DEFINE_FLAG "-D")

if (APPLE AND NOT AARCH64)
  # Despite the docs, -o does not work: cpp prints to stdout.
  set(CMAKE_ASM_NASM_COMPILE_OBJECT
     "${CMAKE_CPP} ${CMAKE_CPP_FLAGS} ${rule_flags} ${rule_defs} -E <SOURCE> > <OBJECT>.s"
    "<CMAKE_COMMAND> -Dfile=<OBJECT>.s -P \"${cpp2asm_newline_script_path}\""
    "<NASM> ${ASM_FLAGS} -o <OBJECT> <OBJECT>.s"
    )
elseif (UNIX OR (APPLE AND AARCH64))
  set(CMAKE_ASM_COMPILE_OBJECT
    "${CMAKE_CPP} ${CMAKE_CPP_FLAGS} ${rule_flags} ${rule_defs} -E <SOURCE> -o <OBJECT>.s"
    "<CMAKE_COMMAND> -Dfile=<OBJECT>.s -P \"${cpp2asm_newline_script_path}\""
    "<CMAKE_ASM_COMPILER> ${ASM_FLAGS} -o <OBJECT> <OBJECT>.s"
    )
else ()
  # Even if we didn't preprocess we'd need our own rule since cmake doesn't
  # support ml.
  set(CMAKE_ASM_COMPILE_OBJECT
    # There's no way to specify a non-default name with /P: writes to
    # cwd/sourcebase.i.  Could copy with "cmake -E copy" but no way to
    # run get_filename_component on a tag var.  So going with
    # redirection operator which should work in all supported shells.
    #
    # ml can't handle line number markers so using ${CPP_NO_LINENUM}.
    "<CMAKE_C_COMPILER> ${CMAKE_CPP_FLAGS} ${rule_flags} ${rule_defs} -E ${CPP_NO_LINENUM} <SOURCE> > <OBJECT>.s"
    # cmake does add quotes in custom commands, etc. but not in this rule so we add
    # them to handle paths with spaces:
    "<CMAKE_COMMAND> -Dfile=<OBJECT>.s -P \"${cpp2asm_newline_script_path}\""
    "<CMAKE_ASM_COMPILER> ${ASM_FLAGS} /c /Fo<OBJECT> <OBJECT>.s"
    )
endif ()

##################################################
# Assembler build commands for Visual Studio generators and
# for assembly code contained in C source files for all generators

# Adds rules to pre-process and assemble the assembly file in ${source}.
# ${extra_defs} should be a list.  It is passed to the pre-processor.
# ${extra_deps} should be a list.  It is listed as dependencies of the build rule.
# The name of the generated output will be stored in the output_out variable.
# This will be an .obj file for VS or an .asm file for Makefiles, but in either case
# it should be listed as a source of final target executable/library
# If ${extra_suffix} is non-empty, it will be inserted into the output file
# and target name before the extension.
# For VS generators, the caller should add ${tgt_out} to the final target's
# dependencies.
function (add_asm_target source output_out tgt_out extra_suffix extra_defs extra_deps)
  if ("${CMAKE_GENERATOR}" MATCHES "Visual Studio")
    get_filename_component(srcbase "${source}" NAME)

    set(s_file "${CMAKE_CURRENT_BINARY_DIR}/${srcbase}${extra_suffix}.s")
    set_source_files_properties("${s_file}" PROPERTIES GENERATED true)
    set(output "${CMAKE_CURRENT_BINARY_DIR}/${srcbase}${extra_suffix}.obj")

    # XXX i#801: a VS2010 bug results in custom commands having all their sources
    # rebuilt which results in racy collision for asm deps (for DR tests, tools_asm).
    # Workaround is to indirect through a dummy .stamp file to avoid
    # chaining sources to custom commands, although this will only
    # include the .obj file in the target link with cmake 2.8.8+.
    # (Alternative is to not list as a source and add to link flags.)
    set(output_stamp "${output}.stamp")
    set_property(SOURCE "${output}" PROPERTY GENERATED true)
    set(output_tgt generate_${srcbase}${extra_suffix})

    add_custom_command(OUTPUT "${output_stamp}"
      DEPENDS
        "${source}"
        "${cpp2asm_newline_script_path}"
        ${extra_deps}
      COMMAND ${CMAKE_COMMAND}
        ARGS -E touch "${output_stamp}"
      COMMAND "${CMAKE_C_COMPILER}" ARGS ${CMAKE_CPP_FLAGS}
        /I "${PROJECT_BINARY_DIR}" ${extra_defs}
        -E ${CPP_NO_LINENUM} "${source}"
        ">" "${s_file}"
      COMMAND "${CMAKE_COMMAND}" ARGS
        -D file=${s_file}
        -P "${cpp2asm_newline_script_path}"
      # We assume we don't need ${defines} => list on cmdline b/c assembly
      # files include configure.h and cpp is where most defines are wanted
      COMMAND "${CMAKE_ASM_COMPILER}" ARGS ${ASM_FLAGS_LIST} /c
        /Fo "${output}"
        "${s_file}"
      WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
      VERBATIM # recommended: p260
      )

    # for correct parallel builds we need a target to avoid
    # duplicate and racy VS per-project rules
    add_custom_target(${output_tgt} DEPENDS "${output_stamp}")
    set(${output_out} "${output}" PARENT_SCOPE)
    set(${tgt_out} ${output_tgt} PARENT_SCOPE)
  else ("${CMAKE_GENERATOR}" MATCHES "Visual Studio")
    if (extra_deps)
      # XXX: how get cmake to analyze #includes of my asm files?
      set_source_files_properties(${source} PROPERTIES OBJECT_DEPENDS
        "${cpp2asm_newline_script_path};${extra_deps}")
    endif (extra_deps)
    # We can't set the extra_defs as source file property b/c caller might
    # use same source for two different targets
    set(${output_out} "${source}" PARENT_SCOPE)
    set(${tgt_out} "" PARENT_SCOPE)
  endif ("${CMAKE_GENERATOR}" MATCHES "Visual Studio")
endfunction (add_asm_target)

# Adds rules to pre-process and assemble the assembly portion of the C/C++
# file ${source}, which must be separated via #ifdef ASM_CODE_ONLY.
# ${extra_defs} are passed to the pre-processor.
# ${extra_deps} are listed as dependencies of the build rule.
# The name of the generated output will be stored in the output_out variable.
# This will be an .obj file for VS or an .asm file for Makefiles, but in either case
# it should be listed as a source of final target executable/library
# If ${extra_suffix} is non-empty, it will be inserted into the output file
# and target name before the extension.
# For VS generators, the caller should add ${tgt_out} to the final target's
# dependencies.
function (add_split_asm_target source output_out tgt_out
    extra_suffix extra_defs extra_deps)
  get_filename_component(srcbase ${source} NAME)
  set(asm_file "${CMAKE_CURRENT_BINARY_DIR}/${srcbase}${extra_suffix}.asm")

  # Be sure to make extra_defs a list!
  set(extra_defs -DASM_CODE_ONLY ${extra_defs})
  add_asm_target("${asm_file}" asm_output asm_target ""
    "${extra_defs}" "${extra_deps}")

  if ("${CMAKE_GENERATOR}" MATCHES "Visual Studio")
    add_custom_command(TARGET ${asm_target} PRE_BUILD
      COMMAND ${CMAKE_COMMAND}
        ARGS -E copy "${source}" "${asm_file}")
  else ("${CMAKE_GENERATOR}" MATCHES "Visual Studio")
    set_source_files_properties(${asm_file} PROPERTIES
      GENERATED ON COMPILE_DEFINITIONS "ASM_CODE_ONLY")
    add_custom_command(
      OUTPUT ${asm_file}
      DEPENDS ${source}
      COMMAND ${CMAKE_COMMAND}
      ARGS -E copy "${source}" "${asm_file}"
      VERBATIM # recommended: p260
      )
  endif ("${CMAKE_GENERATOR}" MATCHES "Visual Studio")

  set(${output_out} "${asm_output}" PARENT_SCOPE)
  set(${tgt_out} ${asm_target} PARENT_SCOPE)
endfunction(add_split_asm_target)
