#!/bin/bash

# **********************************************************
# Copyright (c) 2014-2021 Google, Inc.    All rights reserved.
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

branch=$(git symbolic-ref -q HEAD)
branch=${branch##*/}

prefix=${branch%%-*}
if [ "${prefix}" == "${branch}" ] ||
   [ "${prefix:0:1}" != "i" ] ||
    !([ "${prefix}" == "iX" ] ||
      # Check whether a number.
      [ "${prefix:1}" -eq "${prefix:1}" ] 2>/dev/null); then
    echo "ERROR: please use the branch naming conventions documented at "
    echo "https://dynamorio.org/page_workflow.html#sec_branch_naming"
    exit 1
fi

count=$(git rev-list --count origin/master..)
if [ "${count}" -eq "1" ]; then
    echo "Pushing ${branch} to remote.  Please visit https://github.com/DynamoRIO/dynamorio"
    echo "and create a pull request to compare ${branch} to master."
else
    echo "Pushing new commits on ${branch} to remote."
fi

git push origin ${branch}
