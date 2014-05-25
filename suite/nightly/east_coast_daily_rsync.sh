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

# update some folders, taked on to here since it runs every night
cd /c/dynamo/base
cvs update -P -A -d -C
cd /c/dynamo/bench
cvs update -P -A -d -C
cd /c/dynamo/full
cvs update -P -A -d -C
cd

# Building the file lists takes a long time over the slow connection so we do
# a subset of the full rsync every weekday night with a longer run on the
# weekends that rsyncs different folders.  This the short run.

echo ""
date
date >> do_rsync.log

# sync builds
echo "rsync builds"
rsync -rltDuv --include=/*/ --include=ver_*/ --include=lib/ --include=lib**/* --include=config/ --include=config**/* --include=msi/ --include=msi/Data1.cab --include=msi/SecureCore.msi --include=tools/ --exclude=tools/external**/* --include=tools**/* --exclude=* /mnt/deter-build_nightly/ /e/all_shares/builds/ >> do_rsync.log
echo "done"

# sync the weekly builds
date
echo "rsync weekly builds"
rsync -rltDuv /mnt/deter-build_weekly/ /e/all_shares/builds/weekly/ >> do_rsync.log

# get latest pdb's, just dynamorio.dll's to speed things up
date
echo "rsync product symbols (just dynamorio.dll pdbs) <- west coast"
rsync -rltDuv /mnt/deter-build_symbols/product/dynamorio.pdb/ /e/all_shares/symbols/symbols/product/dynamorio.pdb/ >> do_rsync.log
echo "done"

# sync bugs folder
date
echo "rsync bugs <- west coast"
rsync -rltDuv /mnt/bugs/ /e/all_shares/bugs/ >> do_rsync.log
echo "done"

date
# fixup permission, fixme why do we keep having to do this? also modify issues
chmod -R aug+rxw /e/all_shares/builds
chmod -R aug+rxw /e/all_shares/symbols/symbols/Product
chmod -R aug+rxw /e/all_shares/bugs
date
echo "All Finished"
date >> do_rsync.log
echo "All Finished" >> do_rsync.log