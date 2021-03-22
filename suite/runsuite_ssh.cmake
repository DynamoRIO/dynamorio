# **********************************************************
# Copyright (c) 2010 VMware, Inc.    All rights reserved.
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

# Run test suite on a remote machine over ssh (Issue 310).
#
# Setup requirements:
# * Install rsync and ssh on local host, rsync and sshd on remote host
# * Set up RSA-key passwordless login to avoid repeatedly typing your
#   password
# * For a remote Windows host, cygwin sshd is assumed, and cygpath must be
#   on the path.  Also, make sure the USERPROFILE env var is set in your
#   ~/.bash_profile file as it doesn't seem to be set by default like it is
#   with normal Windows login.  E.g.:
#     if [ ! -n "$USERPROFILE" ]; then
#         export USERPROFILE="E:\Documents and Settings\derek"
#     fi
#
# Required parameters:
#   HOST:STRING          = Name of remote host.
#   REMOTE_WINDOWS:BOOL  = Whether remote host is Windows.
#
# Optional parameters (defaults will typically work):
#   USER:STRING           = Username on remote host.  Default is $USER from env.
#   LOCAL_SRCDIR:PATH     = Path to source dir on local machine.
#                           Default is parent dir of this script's path.
#   REMOTE_SRCDIR:PATH    = Path to source dir on remote host.
#                           Default is "~/dr/regression/src"
#   REMOTE_TESTDIR:PATH   = Path to build dir on remote host.
#                           Default is "~/dr/regression/build"
#   REMOTE_CTEST:FILEPATH = Path to ctest on remote host.
#                           Default assumes "ctest" is on the path.
#
# Examples of usage:
#
#   cd /my/checkout && cmake -DHOST=dr-ubuntu -P suite/runsuite_ssh.cmake
#
#   cmake -DHOST=mylaptop -DUSER=derek -DREMOTE_WINDOWS=ON -P /my/checkout/suite/runsuite_ssh.cmake

# we need CMAKE_CURRENT_LIST_FILE, not sure when added,
# but everyone should be using at least 2.6
cmake_minimum_required(VERSION 3.7)

if (NOT DEFINED USER)
  if (NOT DEFINED ENV{USER})
    message(FATAL_ERROR "must set USER variable")
  else (NOT DEFINED ENV{USER})
    set(USER "$ENV{USER}")
  endif (NOT DEFINED ENV{USER})
endif (NOT DEFINED USER)

if (NOT DEFINED HOST)
  message(FATAL_ERROR "must set HOST variable")
endif (NOT DEFINED HOST)

if (NOT DEFINED LOCAL_SRCDIR)
  get_filename_component(LOCAL_SRCDIR "${CMAKE_CURRENT_LIST_FILE}" PATH)
  get_filename_component(LOCAL_SRCDIR "${LOCAL_SRCDIR}/.." ABSOLUTE)
  if (NOT EXISTS "${LOCAL_SRCDIR}/core")
    message(FATAL_ERROR "must set LOCAL_SRCDIR since using non-standard path to script")
  endif ()
endif (NOT DEFINED LOCAL_SRCDIR)

if (NOT DEFINED REMOTE_SRCDIR)
  set(REMOTE_SRCDIR "~/dr/regression/src")
endif (NOT DEFINED REMOTE_SRCDIR)

if (NOT DEFINED REMOTE_TESTDIR)
  set(REMOTE_TESTDIR "~/dr/regression/build")
endif (NOT DEFINED REMOTE_TESTDIR)

if (NOT DEFINED REMOTE_CTEST)
  set(REMOTE_CTEST "ctest")
endif (NOT DEFINED REMOTE_CTEST)

find_program(RSYNC rsync)
if (NOT RSYNC)
  message(FATAL_ERROR "cannot find rsync")
endif (NOT RSYNC)

find_program(SSH ssh)
if (NOT SSH)
  message(FATAL_ERROR "cannot find ssh")
endif (NOT SSH)

function (run_cmd cmd)
  string(REGEX REPLACE ";" " " cmd_print "${cmd}")
  message("${cmd_print}")
  # let stdout and stderr go to screen
  execute_process(COMMAND ${cmd} RESULT_VARIABLE cmd_result)
  if (cmd_result)
    message(FATAL_ERROR "*** ${cmd_print} returned failure ${cmd_result} ***")
  endif (cmd_result)
endfunction (run_cmd)

# ensure remote dirs exist
set(cmd "${SSH}" "${USER}@${HOST}"
  "mkdir" "-p" "${REMOTE_SRCDIR}" "&&"
  "mkdir" "-p" "${REMOTE_TESTDIR}")
run_cmd("${cmd}")

# copy the local source tree to the remote host using rsync
# * using -C to avoid sending ~ files, but need explicit include of core/,
#   and also all files under ext/drsyms/libelftc/ in order to copy pre-built
#   static libraries
# * not using -u since last tree tested might have newer timestamps
# * not paying cost of checksums (-c) since assuming that either
#   time or size will be different
# * if coming from a perforce checkout don't want to preserve read-only
#   perms so should use "-rltgoD" instead of "-a" though should just
#   get replaced next time so shouldn't matter
set(cmd "${RSYNC}" "-avzC" "--include=core/" "--include=ext/drsyms/libelftc/**" "--delete"
  # exclude, in case people keep build/install dirs inside src dir
  "--exclude=/build*" "--exclude=/exports*"
  "${LOCAL_SRCDIR}/"
  "${USER}@${HOST}:${REMOTE_SRCDIR}/")
run_cmd("${cmd}")

# run test suite with "ssh" param to set GENERATE_PDBS:BOOL=OFF and
# disable cmake try-compile (i#310)
if (REMOTE_WINDOWS)
  # using Windows cmake so we can't use a cygwin path
  set(SCRIPT_ARG "`cygpath -m ${REMOTE_SRCDIR}`/suite/runsuite.cmake,ssh")
else (REMOTE_WINDOWS)
  set(SCRIPT_ARG "${REMOTE_SRCDIR}/suite/runsuite.cmake,ssh")
endif (REMOTE_WINDOWS)

set(cmd "${SSH}" "${USER}@${HOST}"
  "cd" "${REMOTE_TESTDIR}" "&&"
  "${REMOTE_CTEST}" "-V" "-S" "${SCRIPT_ARG}")
run_cmd("${cmd}")
