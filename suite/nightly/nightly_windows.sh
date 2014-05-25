#!/bin/bash

# **********************************************************
# Copyright (c) 2005-2008 VMware, Inc.  All rights reserved.
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

export OS_VER="$1"
export RUNREGRESSION_OS_VER="RUNREGRESSION_$1"
export HOSTNAME=`hostname`

# Let's mark the start time with a file and then check the file's
# existence before starting a new run.  If something's gone wrong and
# the file is still around from the previous run, send mail and don't
# start a new regression.
start_dir=`pwd`

if [ -e ./test_started ]; then
    echo "To:dynamorio-devnull@vmware.com" > mail_results
    echo "Subject: windows $OS_VER ($HOSTNAME) $NIGHTLY_TAG regression ERROR: previous run not finished!" >> mail_results
    echo "" >> mail_results
    read prev_start < ./test_started
    echo "Previous run started on $prev_start did not finish" >> mail_results
    echo "(or was aborted and 'test_started' was not removed)." >> mail_results
    echo "Cancelling current job." >> mail_results

    ssh $NIGHTLY_MAIL "rm -f mail_results.$OS_VER.$HOSTNAME"
    scp mail_results $NIGHTLY_MAIL:mail_results.$OS_VER.$HOSTNAME
    ssh $NIGHTLY_MAIL "/usr/sbin/sendmail -t -fdynamorio-devnull@vmware.com < mail_results.$OS_VER.$HOSTNAME"

    rm -f mail_results.last
    mv -f mail_results mail_results.last
    exit
else
    date > ./test_started
fi

export DYNAMORIO_LOGDIR=$NIGHTLY_LOGDIR_BASE/logs
export DYNAMORIO_BENCHMARKS=$NIGHTLY_BENCHMARKS
export DYNAMORIO_HOME=$NIGHTLY_HOME
export DYNAMORIO_TOOLS=$NIGHTLY_TOOLS
export DYNAMORIO_LIBUTIL=$NIGHTLY_LIBUTIL
export NIGHTLY_START_TIME=`date`
echo $DYNAMORIO_LOGDIR
cd $DYNAMORIO_HOME
#in case of network trouble, make sure we get an updated src module
#rm -rf src
#FIXME how to do this

cd $DYNAMORIO_LIBUTIL
make clean

cd $DYNAMORIO_TOOLS
make clean

cd $NIGHTLY_LOGDIR_BASE
rm -rf logs
mkdir logs

cd $NIGHTLY_SUITE
cd tests
make clean
cd ..
rm -rf regression-*.0*

echo "stopping nodemgr"
net stop scnodemgr

echo "running regression"
./runregression -long 2>&1 | tee regression.log

echo "done running regression, starting nodemgr"
net start scnodemgr

echo "sending summary"
#get output directory name
ls -td regression-*.0* > tmp
read dir_name < tmp
rm -f tmp

#get failure count
grep -c " \*\*\* .* failed:" regression.log > tmp
read fail_count < tmp
rm -f tmp

#start preparing results mail
echo "To:dynamorio-devnull@vmware.com" > mail_results
echo "Subject: windows $OS_VER ($HOSTNAME) $NIGHTLY_TAG regression results : started $NIGHTLY_START_TIME : $fail_count failing runs" >> mail_results
echo "" >> mail_results
echo "this automatically generated email contains the following sections: " >> mail_results
echo " 1) summary of win32 $OS_VER ($HOSTNAME) $NIGHTLY_TAG regression results" >> mail_results
echo " 2) details of win32 $OS_VER ($HOSTNAME) $NIGHTLY_TAG regression (unit test) results" >> mail_results
echo "" >> mail_results
echo "====================================================" >> mail_results

#get runregression summary
dstamp=`date | awk '{print $6 "-" $2$3}'`
details=w32-reg-$dstamp-$OS_VER.$HOSTNAME
../nightly/utreport.pl regression.log $dir_name $details > w32-reg-results 2>&1

echo "regression suite results:" >> mail_results
echo "" >> mail_results
lines=`wc -l w32-reg-results | awk '{print $1}'`;
if [ $lines -lt 50 ]; then
    echo "RESULTS LOOK INCOMPLETE!  CHECK DISK USAGE:" >> mail_results;
    df -h -l >> mail_results;
    echo "" >> mail_results;
    grep 'No space' regression.log | tail >> mail_results;
    echo "" >> mail_results;
fi
cat w32-reg-results >> mail_results

# mail mail_results
ssh $NIGHTLY_MAIL "rm -f mail_results.$OS_VER.$HOSTNAME"
scp mail_results $NIGHTLY_MAIL:mail_results.$OS_VER.$HOSTNAME
#FIXME - where to store this?
#scp $details $NIGHTLY_MAIL:/mnt/determina/bugs/nightly/
ssh $NIGHTLY_MAIL "/usr/sbin/sendmail -t -fdynamorio-devnull@vmware.com < mail_results.$OS_VER.$HOSTNAME"

#cleanup
tar_name="${dir_name%.*}"
tar_ext=".tgz"
# FIXME - disabling till case 5168 and 5000 are fixed, till then will only keep
#  the last night's results
#tar -cf $tar_name$tar_ext regression.log $dir_name w32-reg-results mail_results --gzip
rm -rf regression-dir-last
rm -f regression.log.last
rm -f mail_results.last
rm -f w32-reg-results.last
mv -f regression.log regression.log.last
mv -f $dir_name regression-dir-last
mv -f mail_results  mail_results.last
mv -f w32-reg-results w32-reg-results.last

rm -f $start_dir/test_started
exit
