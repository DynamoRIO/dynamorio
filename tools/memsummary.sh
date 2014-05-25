#!/bin/bash

# **********************************************************
# Copyright (c) 2005-2007 VMware, Inc.  All rights reserved.
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

if [ $# -ne 1 ]; then
    echo "Usage: $0 <logfile>"
    exit 127
fi

log=$1

if ! test -e $log ; then
    echo "Error: $log not found"
    exit 127
fi

tail -n $((`wc -l $log | awk '{print $1}'` - `grep -n -o breakdown $log | tail -1 | cut -f 1 -d :`)) $log | grep -v locks
start=`grep -n '^(Begin) All statistics' $log | tail -1 | cut -d : -f 1`
total=`wc -l $log | awk '{print $1}'`
tail -$((total-start+1)) $log | grep -E 'Our peak com|Peak total m|Peak heap cap|Peak heap align|Peak stack|Peak mmap|Peak file|peak cap|Peak spec|Peak file'
grep -A 5 '^Process Memory' $log
