# **********************************************************
# Copyright (c) 2012-2013 Google, Inc.    All rights reserved.
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

# Custom commands and source file properties are per-subdir so we must
# include this in each instead of just placing in top-level CMakeLists.txt.

if (WIN32)
  # i#894: Win8 WDK ntdll.lib does not list Ki routines so we make our own .lib.
  # i#938: We expand our .lib to include everything so we don't need DDK/WDK.
  # We could build a full dll (not through cmake target though b/c then have
  # ntdll.lib name conflicts) but the lib.exe tool seems cleaner.
  # Note that we need not only stub routines but to also list the stdcall
  # (un-mangled) names in a .def file for it to work.
  # XXX: we should make sure we don't rely on ntdll routines not in older systems!
  find_program(LIB_EXECUTABLE lib.exe DOC "path to lib.exe")
  if (NOT LIB_EXECUTABLE)
    message(FATAL_ERROR "Cannot find lib.exe")
  endif (NOT LIB_EXECUTABLE)
  set(ntimp_base "ntdll_imports")
  set(ntimp_src "${PROJECT_SOURCE_DIR}/core/win32/${ntimp_base}.c")
  set(ntimp_def "${PROJECT_SOURCE_DIR}/core/win32/${ntimp_base}.def")
  # We can't call it ntdll.lib b/c then link.exe will ignore the one from
  # the WDK/DDK.  Update i#938: we have now replaced the regular ntdll.lib
  # but we stick with our unique name.
  set(ntimp_lib "${PROJECT_BINARY_DIR}/core/${ntimp_base}.lib")
  set_property(SOURCE "${ntimp_lib}" PROPERTY GENERATED true)
  # Because the Ki are stdcall we can't just use a .def file: we need
  # an .obj file built from stubs w/ the same signatures.
  # We don't need a stamp file b/c the .lib is not a source; plus a stamp
  # file causes Ninja to fail to build.
  add_custom_command(OUTPUT "${ntimp_lib}"
    DEPENDS "${ntimp_src}" "${ntimp_def}"
    COMMAND "${CMAKE_C_COMPILER}" ARGS /nologo /c /Ob0 ${ntimp_src}
    COMMAND "${LIB_EXECUTABLE}" ARGS
      /nologo /name:ntdll.dll /def:${ntimp_def}
      ${PROJECT_BINARY_DIR}/core/${ntimp_base}.obj
    WORKING_DIRECTORY ${PROJECT_BINARY_DIR}/core
    VERBATIM # recommended: p260 of cmake book
    )
  if ("${CMAKE_GENERATOR}" MATCHES "Visual Studio")
    # We can't add a custom target here b/c that's shared and we'd get dup targets
    # so we do that at one include point, in core/
    set(ntimp_dep "")
  else ()
    set(ntimp_dep "${ntimp_lib}")
  endif ()
  set(ntimp_flags "\"${ntimp_lib}\"")
endif (WIN32)
