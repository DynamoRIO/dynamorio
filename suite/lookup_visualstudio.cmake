# **********************************************************
# Copyright (c) 2014 Google, Inc.    All rights reserved.
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
#
#
###########################################################################
# Lookup visual studio registry hives and figure out the newest installed
# visual studio version. It returns the installation path, the version number
# and the appropriate generator string.
#
function(get_visualstudio_info vs_dir_out vs_version_out vs_generator_out)
  get_filename_component(${vs_dir_out} [HKEY_CURRENT_USER\\SOFTWARE\\Microsoft\\VisualStudio\\12.0_Config\\Setup\\VS;ProductDir] REALPATH)
  # on failure we get "c:/registry"
  # XXX: this won't work if user happens to install visual studio in a path
  # ending in "/registry".
  if ("${vs_dir_out}" MATCHES "/registry$")
    set(${vs_version_out} "NOTFOUND" PARENT_SCOPE)
  else ()
    set(${vs_version_out} "12.0" PARENT_SCOPE)
    set(${vs_generator_out} "Visual Studio 12" PARENT_SCOPE)
    return()
  endif()
  get_filename_component(${vs_dir_out} [HKEY_CURRENT_USER\\SOFTWARE\\Microsoft\\VisualStudio\\11.0_Config\\Setup\\VS;ProductDir] REALPATH)
  if ("${vs_dir_out}" MATCHES "/registry$")
    # on failure we get "/registry"
    set(${vs_version_out} "NOTFOUND" PARENT_SCOPE)
  else ()
    set(${vs_version_out} "11.0" PARENT_SCOPE)
    set(${vs_generator_out} "Visual Studio 11" PARENT_SCOPE)
    return()
  endif()
  get_filename_component(${vs_dir_out} [HKEY_CURRENT_USER\\SOFTWARE\\Microsoft\\VisualStudio\\10.0_Config\\Setup\\VS;ProductDir] REALPATH)
  if ("${vs_dir_out}" MATCHES "/registry$")
      # Visual Studio 2010 may store the regkey "ProductDir" in a different
      # registry path.
      get_filename_component(${vs_dir_out} [HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\VisualStudio\\10.0\\Setup\\VS;ProductDir] REALPATH)
  endif ()
  if ("${vs_dir_out}" MATCHES "/registry$")
    set(${vs_version_out} "NOTFOUND" PARENT_SCOPE)
  else ()
    set(${vs_version_out} "10.0" PARENT_SCOPE)
    set(${vs_generator_out} "Visual Studio 10" PARENT_SCOPE)
    return()
  endif()
  # Visual Studio prior to 2010, stores the regkey "ProductDir" in a different
  # registry path.
  get_filename_component(${vs_dir_out} [HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\VisualStudio\\9.0\\Setup\\VS;ProductDir] REALPATH)
  if ("${vs_dir_out}" MATCHES "/registry$")
    # on failure we get "/registry"
    set(${vs_version_out} "NOTFOUND" PARENT_SCOPE)
  else ()
    set(${vs_version_out} "9.0" PARENT_SCOPE)
    set(${vs_generator_out} "Visual Studio 9 2008" PARENT_SCOPE)
    return()
  endif()
  get_filename_component(${vs_dir_out} [HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\VisualStudio\\8.0\\Setup\\VS;ProductDir] REALPATH)
  if ("${vs_dir_out}" MATCHES "/registry$")
    # on failure we get "/registry"
    set(${vs_version_out} "NOTFOUND" PARENT_SCOPE)
  else ()
    set(${vs_version_out} "8.0" PARENT_SCOPE)
    set(${vs_generator_out} "Visual Studio 8 2005" PARENT_SCOPE)
    return()
  endif()
endfunction(get_visualstudio_info)
