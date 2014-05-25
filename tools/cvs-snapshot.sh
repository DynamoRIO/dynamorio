# **********************************************************
# Copyright (c) 2007 VMware, Inc.  All rights reserved.
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

# FIXME: use a real tag in CVS when this is done for real

# -kv doesn't handle properly binary files

mkdir dynamorio
cd dynamorio

#export CO=checkout -D 2007-09-28
# note a date in the future should work fine

# thanksgiving_cvs_snapshot is last CVS checkin
# look for the code at its new home at perforce-tiger://depot/dynamorio/...

# all modules in both repositories should be tagged with cvs rtag thanksgiving_cvs_snapshot

export TAG=thanksgiving_cvs_snapshot

# -D 2007-11-21
export CO="export -r $TAG -kv"

#rename src -> core
cvs -d /repo/cvseast $CO -d core src
cvs -d /repo/cvseast $CO tools
#rename share -> libutil
cvs -d /repo/cvs $CO -d libutil share
#rename liveshield-update -> liveshield
cvs -d /repo/cvs $CO -d liveshield liveshield-update

cvs -d /repo/cvseast $CO suite
cvs -d /repo/cvseast $CO api
cvs -d /repo/cvseast $CO clients
cvs -d /repo/cvseast $CO win32lore
cvs -d /repo/cvseast $CO docs
cvs -d /repo/cvseast $CO papers

#remove some old stuff
rm -rf docs/status

cvs -d /repo/cvs $CO nightly
cvs -d /repo/cvs $CO nodemgr
cvs -d /repo/cvs $CO threatinfo

# these take a little more time, comment out if testing
# EAST coast repository
cvs -d /repo/cvseast $CO exploits
cvs -d /repo/cvs $CO -d exploits exploits # merge from west coast repository exploits/antihips
cvs -d /repo/cvseast $CO benchmarks
cvs -d /repo/cvseast $CO spec2kdata

# adding config temporarily
cvs -d /repo/cvs $CO config

### below the line
# detertray

# do we want the exploits from the west coast repository??

cd ..
tar czvf dynamorio-main.tgz dynamorio
