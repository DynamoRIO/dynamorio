# **********************************************************
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

# Where to submit CTest results
set(CTEST_PROJECT_NAME "DynamoRIO")

if (SUBMIT_LOCAL)
  set(CTEST_DROP_METHOD "scp")
  if (WIN32)
    # Cygwin scp won't take : later in path (and interprets drive as host like
    # unix scp would) so we use a batch file.
    # Note that CTEST_SCP_COMMAND must be a single executable so we can't
    # use "cmake -E copy".  We could use cygwin "cp" but that requires cygwin.
    set(CTEST_SCP_COMMAND "${CTEST_SCRIPT_DIRECTORY}/../make/copy.bat")
  else (WIN32)
    find_program(CTEST_SCP_COMMAND scp DOC "scp command for local copy of results")
  endif (WIN32)
  set(CTEST_TRIGGER_SITE "")
  set(CTEST_DROP_SITE_USER "")
  # CTest does "scp file ${CTEST_DROP_SITE}:${CTEST_DROP_LOCATION}" so for
  # local copy w/o needing sshd on localhost we arrange to have : in the
  # absolute filepath (when absolute, scp interprets as local even if : later)
  if (NOT EXISTS "${CTEST_DROP_SITE}:${CTEST_DROP_LOCATION}")
    message(FATAL_ERROR
      "must set ${CTEST_DROP_SITE}:${CTEST_DROP_LOCATION} to an existing directory")
  endif (NOT EXISTS "${CTEST_DROP_SITE}:${CTEST_DROP_LOCATION}")
else (SUBMIT_LOCAL)
  # Nightly runs will use sources as of this time
  set(CTEST_NIGHTLY_START_TIME "04:00:00 EST")
  set(CTEST_DROP_METHOD "http")
  set(CTEST_DROP_SITE "dynamorio.org")
  set(CTEST_DROP_LOCATION "/CDash/submit.php?project=DynamoRIO")
  set(CTEST_DROP_SITE_CDASH TRUE)
endif (SUBMIT_LOCAL)
