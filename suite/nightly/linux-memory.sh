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

# won't src .bashrc so set env var here
export CVS_RSH=ssh
remotehost="amdxp"
remotepath="/home/sshrsa/nightly"
localtree="/home/sshrsa/dr/trees/nightly"
remotetree="/home/sshrsa/dr/trees/nightly"
remotetools="/home/sshrsa/dr/tools"
rsyncout="rsync -e ssh -auvz --delete"
rsynclinkref="rsync -e ssh -Lptgouvz"
rsyncin="rsync -e ssh -auvz"
shellrc="~/.bashrc"
remotescript="./memory_xp.sh"
export CVSROOT=/mnt/data/cvseast

mailto="eng-nightly-results@determina.com"

dstamp=`date | awk '{print $6 "-" $2$3}'`
prefix="memory-"$dstamp
log=$prefix".log"
email=$prefix".email"

if [ -e $log ]; then echo "Error: $log already exists"; exit -1; fi

ssh ${remotehost} "rm -rf ${remotetree}/src" > $log 2>&1
${rsynclinkref} ${remotescript} ${remotehost}:${remotepath} >> $log 2>&1
curdir=`pwd`;
pushd ${localtree} >> $log 2>&1
rm -rf src tools >> $curdir/$log 2>&1
cvs -d $CVSROOT co src tools >> $curdir/$log 2>&1
popd >> $log 2>&1
${rsyncout} ${localtree}/src ${remotehost}:${remotetree} >> $log 2>&1
${rsyncout} ${localtree}/tools ${remotehost}:${remotetools} >> $log 2>&1
ssh ${remotehost} "${remotepath}/${remotescript}" >> $log 2>&1

# create summary for mailing
echo "From: $mailto" > $email
echo "To: $mailto" >> $email
echo "Errors-To: derek@determina.com" >> $email
echo "Subject: desktop memory breakdown "$dstamp >> $email
echo "desktop memory and performance breakdown for "`date` >> $email
echo "details at dereksha:"`pwd`/$log >> $email
echo "" >> $email
# calculate % change since last result
prev=`ls -1t *.email | head -2 | tail -1`
echo "Change in total committed memory from last run $prev:" >> $email
tmpfile=$email-tmp
grep '100\.00' $prev | awk '{print $(NF-1)}' > $tmpfile
grep '100\.00' $log | awk '{print $(NF-1)}' | paste - $tmpfile | awk '{printf "Previous: %10d => %5.1f%% change\n", $2, 100*($1-$2)/$1}' >> $email
rm $tmpfile
echo "" >> $email
# pull out title, /usr/bin/time results, and heapacct breakdown
grep -E '^@|elapsed|RSS,|%.* KB' $log >> $email
echo "" >> $email

# mail summary out
#   sendmail on RH8 puts in the X-Authentication-Warning, despite the manual's
#   claims that turning off authwarnings or editing the trusted user's file will
#   prevent that -- in any case not using -f and instead just using a From:
#   seems to work and avoids the warning
#   (but, I remember in the past it caused problems w/ picky mailservers?)
# update: using -f to avoid alink mail provider's stupid rejection of unknown
# senders
/usr/sbin/sendmail -f$mailto $mailto < $email
