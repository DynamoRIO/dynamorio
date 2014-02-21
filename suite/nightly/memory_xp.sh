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

###########################################################################
## VC++ Environment Variables
## From C:\Program Files\Microsoft Visual Studio\VC98\Bin\VCVARS32.BAT
##
VSBaseDirTemp=`cygpath -ms /c/Program\ Files/Microsoft\ Visual\ Studio`
VSBaseDirBASH=`cygpath -u $VSBaseDirTemp`

# Root of Visual Developer Studio Common files.
VSCommonDirBASH="${VSBaseDirBASH}/Common"
VSCommonDir=`cygpath -d ${VSCommonDirBASH}`

# Root of Visual Developer Studio installed files.
MSDevDirBASH="${VSCommonDirBASH}/MSDev98"
MSDevDir=`cygpath -d ${MSDevDirBASH}`

# Root of Visual C++ installed files.
MSVCDirBASH="${VSBaseDirBASH}/VC98"
MSVCDir=`cygpath -d ${MSVCDirBASH}`

# VcOsDir is used to help create either a Windows 95 or Windows NT specific path.
if [ "$OS" = "Windows_NT" ]; then export VcOsDir=WINNT; else export VcOsDir=WIN95; fi

# Set environment for using Microsoft Visual C++ tools.
if [ "$OS" = "Windows_NT" ]; then PATH="$MSDevDirBASH/BIN:$MSVCDirBASH/BIN:$VSCommonDirBASH/TOOLS/$VcOsDir:$VSCommonDirBASH/TOOLS:$PATH"; fi
if [ "$OS" = "" ]; then PATH="$PATH:$MSDevDirBASH/BIN:$MSVCDirBASH/BIN:$VSCommonDirBASH/TOOLS/$VcOsDir:$VSCommonDirBASH/TOOLS:$windir/SYSTEM"; fi
export INCLUDE="$MSVCDir\\ATL\\INCLUDE;$MSVCDir\\INCLUDE;$MSVCDir\\MFC\\INCLUDE;$INCLUDE"
export LIB="$MSVCDir\\LIB;$MSVCDir\\MFC\\LIB;$LIB"

unset VSBaseDirTemp
unset VSBaseDirBASH
unset VSCommonDirBASH
unset VSCommonDir
unset MSDevDirBASH
unset MSDevDir
unset MSVCDirBASH
unset MSVCDir
#
###########################################################################

# aliases not expanded in non-interactive shell by default
shopt -s expand_aliases

myip=`netsh interface ip show address | grep Address | awk '{print $NF}'`
export COLONHOME=`cygpath -m ${HOME}`
export DYNAMORIO_TREEROOT=${COLONHOME}/dr/trees
export DYNAMORIO_HOME=${DYNAMORIO_TREEROOT}/nightly
export DYNAMORIO_LOGDIR=${COLONHOME}/dr/logs-nightly
export DYNAMORIO_TOOLS=${COLONHOME}/dr/tools
export DYNAMORIO_DESKTOP=${COLONHOME}/dr/benchmarks/desktop

alias meminjector='echo "DYNAMORIO_HOME=$DYNAMORIO_HOME"; time $DYNAMORIO_HOME/exports/bin32/drinject.exe -mem -stats ${DYNAMORIO_HOME//\//\\\\}\\exports\\lib32\\debug\\dynamorio.dll'

cd ${DYNAMORIO_HOME}/src
make clear
make
make DEBUG=0

rm -rf ${DYNAMORIO_LOGDIR}
mkdir ${DYNAMORIO_LOGDIR}

function getlogdata {
    pushd ${DYNAMORIO_LOGDIR}
    logdir=`ls -1td *[0-9] | head -1`
    if test -e $logdir.xml ; then
        echo "ERROR: $logdir.xml exists"
    fi
    cd $logdir
    log=`ls *exe* *EXE*`
    echo "log is ${DYNAMORIO_LOGDIR}/$logdir/$log"
    ${DYNAMORIO_TOOLS}/memsummary.sh $log
    ${DYNAMORIO_TOOLS}/heapacct.pl $log
    cd ..
    dstamp=`date | awk '{print $6 "-" $2$3}'`
    tar czf $dstamp-$logdir.tgz $logdir
    rm -rf $logdir
    popd
}

###########################################################################
# do the runs

cd ${DYNAMORIO_DESKTOP}/winword
echo "@@@@@@@@ WINWORD NATIVE @@@@@@@@"
make NATIVE=1 run
echo "@@@@@@@@ WINWORD RELEASE @@@@@@@@"
export DYNAMORIO_OPTIONS=''
make run
echo "@@@@@@@@ WINWORD DEBUG @@@@@@@@"
export DYNAMORIO_OPTIONS='-loglevel 1'
make DEBUG=1 run
getlogdata
echo "@@@@@@@@ WINWORD DEBUG -coarse_units @@@@@@@@"
export DYNAMORIO_OPTIONS='-coarse_units -loglevel 1'
make DEBUG=1 run
getlogdata

cd ${DYNAMORIO_DESKTOP}/excel
echo "@@@@@@@@ EXCEL NATIVE @@@@@@@@"
make NATIVE=1 run
echo "@@@@@@@@ EXCEL RELEASE  (running w/ -C b/c of case 6626) @@@@@@@@"
export DYNAMORIO_OPTIONS='-C'
make run
echo "@@@@@@@@ EXCEL DEBUG (running w/ -C b/c of case 6626) @@@@@@@@"
export DYNAMORIO_OPTIONS='-C -loglevel 1'
make DEBUG=1 run
getlogdata
echo "@@@@@@@@ EXCEL DEBUG -coarse_units (running w/ -C b/c of case 6626) @@@@@@@@"
export DYNAMORIO_OPTIONS='-coarse_units -C -loglevel 1'
make DEBUG=1 run
getlogdata

echo "@@@@@@@@ IEXPLORE DEBUG @@@@@@@@"
# if IIS is running this is easiest way to get javascript to run:
#meminjector C:\\Program\ Files\\Internet\ Explorer\\IEXPLORE.EXE http://localhost/close.html
# on XP though we point to local share (hard to get file: to relax security)
# FIXME: w/ sshrsa user as "limited" type I can run calc over ssh, but not IE,
# so I made sshrsa a sysadmin, which works.  not sure what the problem is -- can
# see share either way.
export DYNAMORIO_OPTIONS='-loglevel 1'
meminjector C:\\Program\ Files\\Internet\ Explorer\\IEXPLORE.EXE \\\\${myip}\\share\\close.html
getlogdata

echo "@@@@@@@@ IEXPLORE -hotp_only @@@@@@@@"
# see notes above on methods of running IIS
export DYNAMORIO_OPTIONS='-hotp_only -loglevel 1'
meminjector C:\\Program\ Files\\Internet\ Explorer\\IEXPLORE.EXE \\\\${myip}\\share\\close.html
getlogdata

echo "@@@@@@@@ IEXPLORE -coarse_units @@@@@@@@"
# see notes above on methods of running IIS
export DYNAMORIO_OPTIONS='-coarse_units -loglevel 1'
meminjector C:\\Program\ Files\\Internet\ Explorer\\IEXPLORE.EXE \\\\${myip}\\share\\close.html
getlogdata

