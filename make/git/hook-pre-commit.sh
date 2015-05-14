#!/bin/sh

# **********************************************************
# Copyright (c) 2014-2015 Google, Inc.    All rights reserved.
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

# This is the pre-commit git hook, which is run when a developer attempts
# to commit locally.

# Prevent committing to master
# The --short option is too recent to rely on so we use a bash expansion
# to remove refs/heads/.
branch=`git symbolic-ref -q HEAD`
if [ "${branch##*/}" = "master" ]; then
    exec 1>&2
    cat <<\EOF
Error: it looks like you're committing on master.
Use a topic branch instead.
Aborting commit.
EOF
    exit 1
fi

# Avoid mistakes in author configurations:
AUTHORINFO=$(git var GIT_AUTHOR_IDENT) || exit 1
NAME=$(printf '%s\n' "${AUTHORINFO}" | sed -n 's/^\(.*\) <.*$/\1/p')
EMAIL=$(printf '%s\n' "${AUTHORINFO}" | sed -n 's/^.* <\(.*\)> .*$/\1/p')
existing=`git log --author="${NAME} <${EMAIL}>" -n 1`
if [ -z "${existing}" ]; then
    printf "WARNING: \"${NAME} <${EMAIL}>\" is a new author.\n"
    printf "Please double-check that it is configured properly.\n"
fi

# XXX: move code style checks here from runsuite.cmake?
