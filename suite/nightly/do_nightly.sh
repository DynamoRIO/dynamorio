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

log=$PWD/do_nightly.log
rm $log
cd $(cygpath -u $TCROOT)
p4 -d $(cygpath -w $PWD) sync 2>&1 | tee $log
cd $NIGHTLY_HOME
p4 -d $(cygpath -w $PWD) sync 2>&1 | tee -a $log
rm -rf core/Makefile  2>&1 | tee -a $log
p4 -d $(cygpath -w $PWD) sync -f core/Makefile  2>&1 | tee -a $log

# A descriptive name for the host OS should be passed as an
# arg to the nightly_windows script. For example, '2000' for
# Windows 2000, '2003' for Windows 2003, 'XP' for Windows XP
# and 'Vista' for Vista.
# NOTE since some regression tests behave differently on
# different os versions, this string is used when verifying
# output, so expect failures if you don't use the exact string
# (as shown above) for your os as used by runregression
cd $NIGHTLY_NIGHTLY
./other_platforms/nightly_windows.sh $NIGHTLY_PLATFORM 2>&1 | tee -a $log
