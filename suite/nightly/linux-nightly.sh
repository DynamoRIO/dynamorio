#!/bin/bash

# **********************************************************
# Copyright (c) 2004-2006 VMware, Inc.  All rights reserved.
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
## nightly regression suite script
##

# assumptions: that you've already done this:
#   cvs co benchmarks spec2kdata tools
#   ln -s `pwd`/spec2kdata benchmarks/spec2000/data
#   ln -s `pwd`/benchmarks/spec2000/Makefile.dynamo benchmarks/spec2000/Makefile.defines
#   ln -s `pwd`/tools/runstats.c benchmarks/spec2000/tools
# actually those symlinks need to be made relative to work on the remote machine!

# To avoid impacting the cvs server we run the tests on a separate machine.
# For security reasons we don't want that machine being able to ssh to dereksha
# for cvs updates, so we push from dereksha instead.
# We run this as a lower-privileged user with blank-rsa access to the other machine.

# FIXME: correlate w/ windows build numbers?
# RESOLUTION: no, windows builds are separate, these linux runs will
# only be labeled by date.

# won't src .bashrc so set env var here
export CVS_RSH=ssh
remotehost="rhel4-as"
remotepath="/home/sshrsa/regression"
# note we do NOT use -u, to support changing to older branches
rsyncout="rsync -e ssh -avz --delete"
rsynclinkref="rsync -e ssh -Lptgovz"
rsyncin="rsync -e ssh -auvz"
shellrc="~/.bashrc"
remotescript="./linux-worker.sh"
remotehelper="./utreport.pl"

mailto="eng-nightly-results@determina.com"

dstamp=`date | awk '{print $6 "-" $2$3}'`
prefix="regression-"$dstamp
managerlog=$prefix"/"$prefix".managerlog"
email=$prefix"/"$prefix".email"

if [ -e $prefix ]; then echo "Error: $prefix already exists"; exit -1; fi
mkdir $prefix

# get fresh copies of these
if [ -e suite ]; then rm -rf suite; fi
if [ -e src ]; then rm -rf src; fi
if [ -e tools ]; then rm -rf tools; fi

cvs -d menlo:/mnt/data/cvseast co suite tools > $managerlog 2>&1
cvs -d menlo:/mnt/data/cvseast co src >> $managerlog 2>&1
cd benchmarks; cvs update >> ../$managerlog 2>&1; cd ..

# now mirror on remote host as this user, making sure to delete as well as update
$rsynclinkref $remotescript $remotehost:$remotepath/ >> $managerlog 2>&1
$rsynclinkref $remotehelper $remotehost:$remotepath/ >> $managerlog 2>&1
$rsyncout suite/ $remotehost:$remotepath/suite >> $managerlog 2>&1
$rsyncout src/ $remotehost:$remotepath/src >> $managerlog 2>&1
$rsyncout tools/ $remotehost:$remotepath/tools >> $managerlog 2>&1
$rsyncout benchmarks/ $remotehost:$remotepath/benchmarks >> $managerlog 2>&1

echo "running regression now..." >> $managerlog

# We must source .bashrc to get the compiler path, etc. set up
# (cygwin bash non-interactive does not source it even if started by sshd).
(ssh $remotehost "source $shellrc; cd $remotepath; $remotescript $prefix $mailto") >> $managerlog 2>&1

echo "finished running regression" >> $managerlog

# get results
$rsyncin $remotehost:$remotepath/$prefix/ $prefix >> $managerlog 2>&1
if [ ! -e $email ]; then echo "Error: $email not found"; exit -1; fi

# mail summary out
#   sendmail on RH8 puts in the X-Authentication-Warning, despite the manual's
#   claims that turning off authwarnings or editing the trusted user's file will
#   prevent that -- in any case not using -f and instead just using a From:
#   seems to work and avoids the warning
#   (but, I remember in the past it caused problems w/ picky mailservers?)
#/usr/sbin/sendmail -f$mailto $mailto < $email
/usr/sbin/sendmail -f $mailto $mailto < $email

echo "checking up on windows now" >> $managerlog

# check on the win32 suite
./linux-win32checkup.sh

# mirror on west coast -- do this last in case it hangs
# send results to log, get errors on all symlinks
# FIXME: tar all non-symlinks and just send those over?
cp -r $prefix /mnt/fileserver/nightly/linux/ >> $managerlog 2>&1

