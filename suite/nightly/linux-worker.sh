#!/bin/bash

# **********************************************************
# Copyright (c) 2005-2006 VMware, Inc.  All rights reserved.
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
## Nightly regression suite script for linux
##
## This is the worker script that assumes someone has set up the latest
## suite, src, and tools modules.  This script then runs the regression
## tests and creates a file for emailing but does not email it.
## That allows the manager to have the option of running this script remotely
## and doing the cvs update and emailing itself instead of the remote host,
## or simply running on the local machine.
##
## ASSUMPTION: you've already checked out spec2kdata, and that the symlinks
## from the manager will work here (make them local!)

summaryscript="./utreport.pl"

usage="<prefix> <mailto>"
# mandatory 2 arguments
if [ $# -lt 2 ]; then
    echo "Usage: $0 $usage"
    exit 127
fi
prefix=$1
mailto=$2

log=$prefix"/"$prefix".log"
email=$prefix"/"$prefix".email"
details=$prefix"/"$prefix".details"
logdir=$prefix"/"$prefix".001"

if [ -e $prefix ]; then echo "Error: $prefix already exists"; exit -1; fi
mkdir $prefix

# we assume the modules are placed in the same dir as this script.
if ! test -e suite ; then
    echo "Error: cannot find suite"
    exit 127
fi
if ! test -e src ; then
    echo "Error: cannot find src"
    exit 127
fi
if ! test -e tools ; then
    echo "Error: cannot find tools"
    exit 127
fi

export DYNAMORIO_HOME=`pwd`
export DYNAMORIO_TOOLS=`pwd`/tools
export DYNAMORIO_LOGDIR=.

cd suite
./runregression `pwd`/.. -long >> ../$log 2>&1
mv regression-2* ../$prefix
cd ..

# create summary for mailing
echo "From: $mailto" > $email
echo "To: $mailto" >> $email
echo "Errors-To: derek@determina.com" >> $email
echo "Subject: linux regression results for "`date` >> $email
echo "linux regression results for "`date` >> $email
echo "details at dereksha:"`pwd`/$prefix >> $email
echo "mirrored at //fileserver/nightly/linux/"$prefix >> $email
$summaryscript $log $logdir $details >> $email
