#!/bin/bash

# **********************************************************
# Copyright (c) 2004-2005 VMware, Inc.  All rights reserved.
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
##
## nightly windows regression run and attack suite checkup
##

mailto="eng-nightly-results@determina.com"

# arg1: local email file, arg2: subject string, arg3: body file
function send_email()
{
    # create email
    echo "From: $mailto" > $1
    echo "To: $mailto" >> $1
    echo "Errors-To: derek@determina.com" >> $1
    echo "Subject: $2" >> $1
    echo "" >> $1
    echo "This is an automatically generated email." >> $1
    echo "" >> $1
    cat $3 >> $1
    echo "" >> $1

    # mail summary out
    #   sendmail on RH8 puts in the X-Authentication-Warning, despite the manual's
    #   claims that turning off authwarnings or editing the trusted user's file will
    #   prevent that -- in any case not using -f and instead just using a From:
    #   seems to work and avoids the warning
    #   (but, I remember in the past it caused problems w/ picky mailservers?)
    #/usr/sbin/sendmail -f$mailto $mailto < $1

    /usr/sbin/sendmail -f$mailto $mailto < $1
}

# builds are all merged now, so look for latest build
# FIXME: temporarily look only at 25xxx builds
# in the future, check multiple builds: 22xxx and 25xxx
latest_num=`ls -1t /mnt/build | grep '^25' | head -1`;
latest=/mnt/build/$latest_num
latest_log=$latest/runregression.log
local_log=win32/$latest_num-failures.log
local_email=win32/$latest_num-email

if [ -z $latest_num ]; then
    echo "Error: cannot find latest build: perhaps a /mnt/build mount problem?"
    exit -1
fi

send=0
missing=0
if [ -e $latest_log ]; then
    (grep -A 5000 'Summary of results' $latest_log | grep -B 2 -E '[1-9][0-9]* failed|s in build') > $local_log 2>&1
    if [ ! -s $local_log ]; then
        # make sure regression log not mostly empty
        lines=`wc $latest_log | awk '{print $1}'`;
        if [ $lines -lt 30 ]; then
            missing=1
            echo "Regression log $latest_log looks incomplete" > $local_log 2>&1
            echo "Did the core regression suite run properly?" >> $local_log 2>&1
        else
            echo "No tests failed" > $local_log 2>&1
            # do not send email
            send=0
        fi
    else
        # new policy: win32 includes failure summary, so only send email if cannot
        # find regression results!
        send=0
    fi
else
    missing=1
    echo "Cannot find $latest_log" > $local_log 2>&1
    echo "Did the core regression suite run at all?" >> $local_log 2>&1
fi

if [ $missing == 0 ]; then
    echo "The following tests FAILED:" >> $local_log
    echo "" >> $local_log
fi

if [ $send == 1 ]; then
    send_email $local_email "PROBLEM with build $latest_num windows regression run" $local_log
fi


# check attack suite as well
attack_run_dir="/mnt/qa/QA-Results/Autoattack/$latest_num-1";
attack_log=win32/$latest_num-attack.log
attack_email=win32/$latest_num-attack-email
if [ ! -e $attack_run_dir ]; then
    echo "Cannot find attack suite results $attack_run_dir" > $attack_log
    send_email $attack_email "PROBLEM with build $latest_num attack suite" $attack_log
fi
