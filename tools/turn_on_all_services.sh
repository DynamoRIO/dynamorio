#!/bin/bash

# **********************************************************
# Copyright (c) 2005 VMware, Inc.  All rights reserved.
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

# try to start every service and set it to auto-start

old_IFS="${IFS}"
# handle spaces in names
IFS=$'\n'
for i in `sc query state= all | grep SERVICE_NAME | cut -c 15-`; do
    echo ""; echo $i
    start=`sc qc "$i" | grep START_TYPE | awk '{print $NF}'`
    if [ $start = "AUTO_START" ]; then
        echo "$i already set to auto-start"
    else
        # will refuse to start if currently disabled so we set to auto first
        sc config "$i" start= auto > /dev/null
    fi
    state=`sc query "$i" | grep STATE | awk '{print $4}'`
    echo state is $state
    if [ "$state" != "RUNNING" ]; then
        echo "trying to start $i"
        sc start "$i" > /dev/null
        # no exit code here -- have to wait and query status
        sleep 3
        state=`sc query "$i" | grep STATE | awk '{print $4}'`
        if [ "$state" != "RUNNING" ]; then
            if [ "$start" = "DISABLED" ]; then
                echo "unable to start $i -- disabling"
                sc config "$i" start= disabled > /dev/null
            else
                echo "unable to start $i -- setting to manual"
                sc config "$i" start= demand > /dev/null
            fi
        else
            echo "successfully started $i and set to auto"
        fi
    fi
done
IFS="${old_IFS}"
