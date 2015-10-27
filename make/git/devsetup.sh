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

# Developers should run this script once in each repository
# immediately after cloning.

# Convert CRLF to LF on commit but not checkout:
git config core.autocrlf input

# Highlight tabs at start of line and check in pre-commit:
git config core.whitespace blank-at-eol,tab-in-indent

# Pull should always rebase:
git config branch.autosetuprebase always

# Aliases for our workflow:
git config alias.newbranch "!sh -c \"git checkout --track -b \$1 origin/master\""
# For symmetry with Dr. Memory even though we have no submodules:
git config alias.pullall "pull --rebase"
# Shell aliases always run from the root dir.  Use "$@" to preserve quoting.
git config alias.review "!myf() { make/git/git_review.sh -u \"\$@\"; }; myf"
git config alias.dcommit "!myf() { make/git/git_review.sh -c \"\$@\" && git push origin HEAD:master; }; myf"
git config alias.split "!sh -c \"git checkout -b \$1 \$2 && git branch --set-upstream-to=origin/master \$1\""

# Commit template
git config commit.template make/git/commit-template.txt

# Set up hooks
cp make/git/hook-pre-commit-launch.sh .git/hooks/pre-commit
cp make/git/hook-commit-msg-launch.sh .git/hooks/commit-msg

# Author name and email
# XXX: we could try to read in the info here
echo "Initial setup is complete."
echo Please ensure your author name is correct: \"$(git config user.name)\"
echo "  Run \"git config user.name New Name\" to update"
echo Please ensure your author email is correct: \"$(git config user.email)\"
echo "  Run \"git config user.email New Email\" to update"
