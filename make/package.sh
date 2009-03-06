#!/bin/bash

# **********************************************************
# Copyright (c) 2009 VMware, Inc.  All rights reserved.
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

# Issue 63: package up a Linux DynamoRIO release
# Xref the old tools/makezip.sh from PR 202095

# Run this from where you want the build dirs to be and point it
# at the source dir.
# The first build# is the exported version build# and must be <64K
# The second uniquebuild# can be larger
usage="<source path> <publish path> <ver#> <build#> <uniquebuild#>";
# mandatory 5 arguments
if [ $# -ne 5 ]; then
    echo "Usage: $0 $usage"
    exit 127
fi
srcdir=$1
if ! test -e $srcdir/CMakeLists.txt ; then 
    echo "Error: directory $srcdir is not a source directory"
    exit 127
fi
publishdir=$2
vernum=$3
buildnum=$4
# internal build number guaranteed to be unique but may be >64K
unique_buildnum=$5

DEFS="-DVERSION_NUMBER:STRING=${vernum} -DBUILD_NUMBER:STRING=${buildnum} -DUNIQUE_BUILD_NUMBER:STRING=${unique_buildnum}"

rm -rf build*{rel,dbg}

mkdir build32rel; cd build32rel
cmake $DEFS -DX64:BOOL=OFF ${srcdir}
make -j 4

mkdir ../build32dbg; cd ../build32dbg
cmake $DEFS -DX64:BOOL=OFF -DDEBUG:BOOL=ON -DBUILD_TOOLS:BOOL=OFF -DBUILD_DOCS:BOOL=OFF -DBUILD_DRGUI:BOOL=OFF -DBUILD_SAMPLES:BOOL=OFF ${srcdir}
make -j 4

mkdir ../build64rel; cd ../build64rel
cmake $DEFS -DBUILD_DOCS:BOOL=OFF ${srcdir}
make -j 4

mkdir ../build64dbg; cd ../build64dbg
cmake $DEFS -DDEBUG:BOOL=ON -DBUILD_TOOLS:BOOL=OFF -DBUILD_DOCS:BOOL=OFF -DBUILD_DRGUI:BOOL=OFF -DBUILD_SAMPLES:BOOL=OFF ${srcdir}
make -j 4

# debug first so we take release files in case they overlap
echo "set(CPACK_INSTALL_CMAKE_PROJECTS
  \"$PWD/../build32dbg;DynamoRIO;ALL;/\"
  \"$PWD/../build32rel;DynamoRIO;ALL;/\"
  \"$PWD/../build64dbg;DynamoRIO;ALL;/\"
  \"$PWD/../build64rel;DynamoRIO;ALL;/\"
  )
" >> CPackConfig.cmake
make package

rm -rf ${publishdir}
mkdir -p ${publishdir}
cp -a DynamoRIO-* ${publishdir}
