#!/bin/bash

# **********************************************************
# Copyright (c) 2003-2008 VMware, Inc.  All rights reserved.
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

#notes:
#current DYNAMORIO_OPTIONS env var will be used to determine options
#BUILD_TOOLS must be set
# for ARCH=x64, INSTALL_BIN must be set

usage="runalltest <runallfile> <drdll> <win32_drpreinject_path> <runall_max_wait> <X64|X86>"

if [ $# -ne 5 ] ; then
    echo "Usage: $0 $usage"
    exit 127
fi

# 3 seconds seems to be the magic number here. I've seen problems
# with <freeze> tests for anything less.
RUNALL_SLEEP=3

RUNALL_MAX_WAIT=$4

ARCH=$5

exepath=`head -1 $1`
caption=`head -2 $1 | tail -1`
mode=`head -3 $1 | tail -1`
drdll=$2
drpreinject=$3
exename=${exepath##*/}
if [ ${exepath:0:6} == "<path>" ]; then
    # support for running executables not on system path, but in runall/ dir
    runalldir=${1%/*} # assumption: .runall file is in same place as exe
    exepath=${exepath/<path>/$runalldir}
fi

if [ "$ARCH" == "x64" ]; then
    drctl="$BUILD_TOOLS/DRcontrol -64"
else
    drctl="$BUILD_TOOLS/DRcontrol -32"
fi

function nudge_exe {
    pid=`$BUILD_TOOLS/DRview -exe $1 | gawk '{print $2}' | sed 's/,//'`
    # case 10382: look for "No such process found", since if we
    # pass a non-number to DRcontrol it will nudge all processes!
    if [ "$pid" = "such" ]; then
        echo "Error: $1 process not found for nudge"
    else
        $drctl $2 $pid
    fi
}

#some tests need to be able to find the tools folder
export DYNAMORIO_WINTOOLS=`cygpath -w $BUILD_TOOLS`

# make sure the registry set is there (only creates if nonexistent).
dr_home=`cygpath -da $PWD`
$drctl -create "$dr_home"

# save the old registry settings so we can leave the registry in the
# same state we found it.
prev_preinject=`$drctl -preinject REPORT`
prev_settings=_prev_settings
$drctl -save $prev_settings

$drctl -preinject OFF
$drctl -app "$exename" -run 1 -options "$DYNAMORIO_OPTIONS"  -drlib $drdll
$drctl -preinject $drpreinject
$drctl -app "$exename" -logdir "$DYNAMORIO_LOGDIR"

# clear pcache dir before starting app and then create the registry
# entry and associated dir
if [ -e cache ]; then
    # remove existing perscache dir(s)
    rm -rf cache;
fi

# we have a discrepancy between DYNAMORIO_CACHE_ROOT, which drinject
# uses, and DRcontrol -sharedcache.  The former specifies the full
# path to the cache dir, but the latter specifies the path to DR_HOME
# (i.e., without a trailing /cache).  We do this so that DRcontrol can
# find the lib and logs directory and copy their permissions.
# FIXME: What a hack.  Can't we just set the permissions directly
# and make DRcontrol -sharedcache more intuitive?
cache_root=`dirname "$DYNAMORIO_CACHE_ROOT"`

# drcontrol -sharedcache gives an error message if it can't copy
# permissions from the lib and logs directories (e.g., if they don't
# exist).  Swallow the error message so it doesn't mess up the output.
junk=`$drctl -app "$exename" -sharedcache "$cache_root" 2>&1`

# for 3rd party apps w/ windows, we use DRview results to ensure we're in control
if [ "$caption" != "<nowindow>" ]; then
    if [ "$ARCH" == "x64" ]; then
        $INSTALL_BIN/drinject.exe -noinject $exepath &
    else
        $exepath &
    fi

    sleep $RUNALL_SLEEP

    $drctl -preinject OFF

    $BUILD_TOOLS/DRview -exe $exename -nopid -nobuildnum

    if [ "$mode" == "<detach>" ]; then
        $drctl -detachexe $exename

        # Currently DRcontrol -detachexe waits for the injected detach thread
        # to exit at which point should be finished detaching, so we don't need
        # to sleep here. DRcontrol does time out if wait is too long so we
        # won't get stuck here.

        $BUILD_TOOLS/DRview -exe $exename -nopid -nobuildnum

    elif [ "$mode" == "<reset>" ]; then
        # test two in a row
        nudge_exe $exename "-nudge reset -pid";
        nudge_exe $exename "-nudge reset -pid";

        # Currently DRcontrol -nudge waits for the injected nudge thread to
        # exit, so we don't need to sleep here. DRcontrol does time out if
        # wait is too long so we won't get stuck here.
        $BUILD_TOOLS/DRview -exe $exename -nopid -nobuildnum

    elif [ "$mode" == "<hotp>" ]; then
        # test both kinds of hotp nudge (even if no patches match)
        nudge_exe $exename -hot_patch_nudge;
        nudge_exe $exename -hot_patch_modes_nudge;

        # Currently DRcontrol -nudge waits for the injected nudge thread to
        # exit, so we don't need to sleep here. DRcontrol does time out if
        # wait is too long so we won't get stuck here.
        $BUILD_TOOLS/DRview -exe $exename -nopid -nobuildnum

    elif [ "$mode" == "<freeze>" ]; then
        # test two in a row
        nudge_exe $exename "-nudge freeze -pid";
        nudge_exe $exename "-nudge freeze -pid";

        # Currently DRcontrol -nudge waits for the injected nudge thread to
        # exit, so we don't need to sleep here. DRcontrol does time out if
        # wait is too long so we won't get stuck here.
        $BUILD_TOOLS/DRview -exe $exename -nopid -nobuildnum

    elif [ "$mode" == "<persist>" ]; then
        # we now have per-user and not per-app dirs
        # we'll use the 'whoami' tool in tools/external/ to get the SID
        cursid=`$BUILD_TOOLS/external/whoami.exe /user | tail -1 | gawk '{print $2}'`
        cachedir="$cache_root/cache/$cursid"

        # test persisting and re-using
        nudge_exe $exename "-nudge persist -pid";

        if [ ! -d $cachedir ]; then
            echo "No persisted caches found";
        fi

        # run a second copy
        $drctl -preinject $drpreinject
        $exepath &
        sleep $RUNALL_SLEEP
        $drctl -preinject OFF
        # we assume that it's using the persisted caches
        # FIXME: add way to find out

        # Currently DRcontrol -nudge waits for the injected nudge thread to
        # exit, so we don't need to sleep here. DRcontrol does time out if
        # wait is too long so we won't get stuck here.
        $BUILD_TOOLS/DRview -exe $exename -nopid -nobuildnum

        $BUILD_TOOLS/closewnd "$caption" $RUNALL_MAX_WAIT
    elif [ "$mode" == "<client_nudge>" ]; then
        # send a client nudge, we pick arbitrary value 10 for argument
        nudge_exe $exename "-client_nudge 10 -pid";

        sleep $RUNALL_SLEEP

        # Currently DRcontrol -nudge waits for the injected nudge thread to
        # exit, so we don't need to sleep here. DRcontrol does time out if
        # wait is too long so we won't get stuck here.
        $BUILD_TOOLS/DRview -exe $exename -nopid -nobuildnum
    fi

    $BUILD_TOOLS/closewnd "$caption" $RUNALL_MAX_WAIT

    sleep $RUNALL_SLEEP

    $BUILD_TOOLS/DRview -exe $exename -nobuildnum
else
    $BUILD_TOOLS/winstats -m 2 -silent $exepath

    $drctl -preinject OFF
fi

#just in case
$BUILD_TOOLS/DRkill -exe $exename -quiet

# restore registry settings
$drctl -remove "$exename"

if [[ ${#prev_preinject} > 0 ]]; then
    $drctl -preinject "$prev_preinject"
else
    $drctl -preinject OFF
fi

if [ -e $prev_settings ]; then
    $drctl -load $prev_settings
    rm -f $prev_settings
fi
