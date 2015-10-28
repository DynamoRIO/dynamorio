#!/bin/bash

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

# Usage: git review (-u|-c) [-q] [-r <reviewer>] [-s <subject>]
# One of -u (upload new patchset) or -c (finalize committed review) must be
# supplied.
# Optional arguments:
#  -q = quiet, don't auto-send email
#  -r <reviewer> = specify reviewer email (else it will be queried for)
#  -s <subject> = specify patchset title (else queried, or log's first line
#                 for 1st patchset)
#  -t = prepends "TBR" to the review title
#  -b = base git ref to diff against
#
# If $HOME/.codereview_dr_token exists, its contents are passed as the
# oauth2 token to the code review site.  The file should contain the text
# string obtained from https://codereview.appspot.com/get-access-token.
# If the file does not exist, a browser window will be auto-launched
# for oauth2 authentication.

# Send email by default
email="--send_mail --cc=dynamorio-devs@googlegroups.com"
hashurl="https://github.com/DynamoRIO/dynamorio/commit/"
# We use HEAD^ instead of origin/master to avoid messing up code review diff
# after we sync on the master branch.
base="HEAD^"
tokfile=$HOME/.codereview_dr_token

while getopts ":ucqtr:s:b:" opt; do
  case $opt in
    u)
      mode="upload"
      ;;
    b)
      base="$OPTARG"
      ;;
    c)
      mode="commit"
      ;;
    r)
      reviewer="-r $OPTARG"
      ;;
    s)
      subject="$OPTARG"
      ;;
    q)
      email=""
      ;;
    t)
      prefix="TBR: "
      ;;
    \?)
      echo "Invalid option: -$OPTARG" >&2
      exit 1
      ;;
    :)
      echo "Option -$OPTARG requires an argument." >&2
      exit 1
      ;;
  esac
done

if ! git diff-index --quiet HEAD --; then
    echo "ERROR: branch contains uncommitted changes.  Please commit them first."
    exit 1
fi

if test -e "$tokfile"; then
    echo "Using oauth2 token from $tokfile"
    token="--oauth2_token `cat $tokfile`"
fi

branch=$(git symbolic-ref -q HEAD)
branch=${branch##*/}
issue=$(git config branch.${branch}.rietveldissue)
root=$(git rev-parse --show-toplevel)
user=$(git config user.email)
log=$(git log -n 1 --format=%B)
if [ "$mode" = "upload" ]; then
    commits=$(git rev-list --count origin/master..)
    if [ "$commits" -ne "1" ]; then
        echo "ERROR: only a single commit on top of origin/master is supported."
        echo "Either squash commits in this branch or use multiple branches."
        exit 1
    fi
    if test -z "$issue"; then
        # New CL
        echo "Creating a new code review request."
        label="first patchset";
        if test -z "$subject"; then
            subject=$(git log -n 1 --format=%s)
        fi
        subject="${prefix}${subject}"
        while test -z "$reviewer"; do
            echo -n "Enter the email of the reviewer: "
            read reviewer
            reviewer="-r $reviewer"
        done
    else
        echo "Updating existing code review request #${issue}."
        # New patchset on existing CL
        while test -z "$subject"; do
            echo -n "Enter the label for the patchset: "
            read subject
        done
        label="latest patchset";
        issue="-i $issue"
    fi
    msg=$(echo -e "Commit log for ${label}:\n---------------\n${log}\n---------------")
    echo "Uploading the review..."
    # For re-authentication, upload.py prompts on stdout, so we need to tee it.
    # We assume all devs have tee and cat, or that git's built-in shell has them.
    exec 9>&1
    # Pass -u to avoid python's buffering from preventing any output while
    # python script sits there waiting for input.
    output=$(python -u ${root}/make/upload.py -y -e "${user}" ${reviewer} ${issue} \
        --oauth2 ${token} -t "${subject}" -m "${msg}" ${email} "${base}".. \
        | tee >(cat - >&9))
    exec 9>&-
    if test -z "$issue"; then
        number=$(echo "$output" | grep http://)
        number=${number##*/}
        if test -z "$number"; then
            echo "ERROR: failed to record Rietveld issue number."
            exit 1
        else
            git config branch.${branch}.rietveldissue ${number}
            # Add the Review-URL line
            git commit --amend -m "$log" \
                -m "Review-URL: https://codereview.appspot.com/${number}"
        fi
    fi
elif [ "$mode" = "commit" ]; then
    if test -n "$issue"; then
        # Upload the committed diff, for easy viewing of final changes.
        echo "Finalizing existing code review request #${issue}."
        subject="Committed"
        hash=$(git log -n 1 --format=%H)
        msg=$(echo -e "Committed as ${hashurl}${hash}\n\nFinal commit log:" \
            "\n---------------\n${log}\n---------------")
        python -u ${root}/make/upload.py -y -e "${user}" -i ${issue} \
            --oauth2 ${token} -t "${subject}" -m "${msg}" ${email} HEAD^
        # Remove the issue marker
        git config --unset branch.${branch}.rietveldissue
    else
        echo "WARNING: this branch is not associated with any review."
        # Keep exit status 0
    fi
else
    echo "Invalid mode"
    exit 1
fi
