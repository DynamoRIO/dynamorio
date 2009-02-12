#!/bin/bash

# **********************************************************
# Copyright (c) 2008-2009 VMware, Inc.  All rights reserved.
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

# Copyright (c) 2003-2007 Determina Corp.
# Copyright (c) 2001-2003 Massachusetts Institute of Technology
# Copyright (c) 2001-2002 Hewlett Packard Company

# PR 202095: packages up a DynamoRIO release

# The first build# is the exported version build# and must be <64K
# The second uniquebuild# can be larger
usage="<viper|vmap> <absolute build path> <build#> <uniquebuild#> <ver#> [publish path]";

projname=$1

# mandatory 1st 5 arguments
if [ $# -ne 6 ] && [ $# -ne 5 ]; then
    echo "Usage: $0 $usage"
    exit 127
fi
if [ $projname != "viper" -a $projname != "vmap" ]; then
    echo "USAGE: $0 $usage"
    exit 127
fi
if [ $projname == "vmap" ]; then
    # we want the directory and zip file to have the known name "DynamoRIO"
    projname="dynamorio";
    pkgname="DynamoRIO";
else
    pkgname=$projname
fi
base=$2
if test -e $base ; then 
    echo "Error: directory $base already exists"
    exit 127
fi
buildnum=$3
# internal build number guaranteed to be unique but may be >64K
unique_buildnum=$4
vernum=$5
publishdir=$6
vercomma=${vernum//./,}
defines="BUILD_NUMBER=$buildnum VERSION_NUMBER=$vernum VERSION_COMMA_DELIMITED=$vercomma UNIQUE_BUILD_NUMBER=$unique_buildnum"

if [ $projname == "viper" ]; then
target_config="VMSAFE=1"
else
target_config="VMAP=1"
fi

# this is how compiler.mk checks for Linux vs. Windows - only windows should have OS set
if [ -z "$OS" ]; then
    linux=1
    windows=0
    echo Linux
    CP="cp -a"
    os="Linux"
    # PR 236973: there is no "g++" binary we can put on path so we have
    # to explicitly reference the binary.  This path needs ARCH
    # replaced with "i686" or "x86_64".
    gxx=/build/toolchain/lin32/gcc-4.1.2-2/bin/ARCH-linux-g++
else
    linux=0
    windows=1
    echo Windows
    CP="cp -a"
    os="Windows"
fi

mkdir $base
mkdir $base/$projname
mkdir $base/$projname/include
mkdir $base/$projname/bin
if [ $windows -eq 1 ]; then
    mkdir $base/$projname/bin/bin32
    mkdir $base/$projname/bin/bin64
fi
mkdir $base/$projname/lib32
mkdir $base/$projname/lib32/debug
mkdir $base/$projname/lib32/release
mkdir $base/$projname/lib64
mkdir $base/$projname/lib64/debug
mkdir $base/$projname/lib64/release
mkdir $base/$projname/docs
mkdir $base/$projname/logs

# not part of zip file, pdbs on windows, non-stripped binaries on Linux
mkdir $base/symbols
mkdir $base/symbols/lib32
mkdir $base/symbols/lib32/release
mkdir $base/symbols/lib32/debug
mkdir $base/symbols/lib64
mkdir $base/symbols/lib64/release
mkdir $base/symbols/lib64/debug
mkdir $base/symbols/bin
if [ $windows -eq 1 ]; then
    mkdir $base/symbols/bin/bin32
    mkdir $base/symbols/bin/bin64
fi

mkdir $base/include_test

##################################################
# defaults for DYNAMORIO_ env vars

if [ -z "$DYNAMORIO_HOME" ] ; then
    makezip_dir=`dirname $0`
    DYNAMORIO_HOME=`cd $makezip_dir/../ ; pwd`
fi
if [ -z "$DYNAMORIO_MAKE" ] ; then
    DYNAMORIO_MAKE=$DYNAMORIO_HOME/make
fi
if [ -z "$DYNAMORIO_LIBUTIL" ] ; then
    DYNAMORIO_LIBUTIL=$DYNAMORIO_HOME/libutil
fi
if [ -z "$DYNAMORIO_TOOLS" ] ; then
    DYNAMORIO_TOOLS=$DYNAMORIO_HOME/tools
fi
if [ -z "$DYNAMORIO_API" ] ; then
    DYNAMORIO_API=$DYNAMORIO_HOME/api
fi

##################################################
# core

cd $DYNAMORIO_HOME/core
if test -e Makefile.mydefines ; then 
    mv Makefile.mydefines .ignoreme
fi

check_includes () {
    if [ $# -ne 0 ]; then
        echo "Internal error calling check_includes()"
        exit 127
    fi
    if diff -r $base/include_test $base/$projname/include; then
        rm -f $base/include_test/*.h
    else
        echo "Error: generated include files aren't identical to those previously generated"
        exit 127
    fi
}

#args are <extra_defines> <86|64> <rel|dbg>
run_build () {
    if [ $# -ne 3 ]; then
        echo "Internal error calling run_build()"
        exit 127
    fi
    if [ $3 == "rel" ]; then
        lib_sub="release"
    else
        lib_sub="debug"
    fi
    if [ $2 == "86" ]; then
        lib="lib32"
        bin="bin32"
    else
        lib="lib64"
        bin="bin64"
    fi

    make $target_config $1 $defines clean install
    if [ $linux -eq 1 ]; then
        $CP ../exports/$lib/$lib_sub/libdynamorio.so $base/$projname/$lib/$lib_sub
        $CP ../exports/$lib/$lib_sub/libdynamorio.so $base/symbols/$lib/$lib_sub
        if [ $3 == "rel" ]; then
            $CP ../exports/$lib/$lib_sub/libdrpreload.so $base/$projname/$lib
            $CP ../exports/$lib/$lib_sub/libdrpreload.so $base/symbols/$lib
        fi
        $CP ../exports/include/*.h $base/include_test
    else
        $CP ../exports/$lib/$lib_sub/dynamorio.{dll,lib} $base/$projname/$lib/$lib_sub
        $CP ../exports/$lib/$lib_sub/dynamorio.{pdb,map} $base/symbols/$lib/$lib_sub
        if [ $3 == "rel" ]; then
            $CP ../exports/$lib/{drpreinject,drearlyhelp1,drearlyhelp2}.dll $base/$projname/$lib
            $CP ../exports/$lib/{drpreinject.{pdb,map},drearlyhelp1.{pdb,map},drearlyhelp2.{pdb,map}} $base/symbols/$lib
            $CP ../exports/bin/$bin/drinject.exe $base/$projname/bin/$bin
            $CP ../exports/bin/$bin/drinject.{pdb,map} $base/symbols/bin/$bin
        fi
        $CP ../exports/include/*.h $base/include_test
    fi
}

make clear

run_build "DEBUG=0 INTERNAL=0 EXTERNAL_INJECTOR=1" 86 rel
$CP $base/include_test/*.h $base/$projname/include
rm -f $base/include_test/*.h

run_build "DEBUG=1 INTERNAL=0" 86 dbg
check_includes

run_build "DEBUG=0 INTERNAL=0 ARCH=x64 EXTERNAL_INJECTOR=1" 64 rel
check_includes

run_build "DEBUG=1 INTERNAL=0 ARCH=x64" 64 dbg
check_includes

rm -rf $base/include_test

if [ $linux -eq 1 ]; then
    # strip debug and local symbols (leave only global exports and imports)
    stripcmd='strip -p --strip-debug --discard-all'
    find $base/$projname/lib* -name \*.so -exec $stripcmd {} \;
fi

if test -e .ignoreme ; then 
    mv .ignoreme Makefile.mydefines
fi

##################################################
# tools

if [ $linux -eq 1 ]; then
    $CP $DYNAMORIO_TOOLS/dynamorio $base/$projname/bin/drdeploy

    #Removed since no longer need license/installer
    #$CP $DYNAMORIO_TOOLS/linux_install.sh $base/install.sh
    #gzexe $base/install.sh
    #rm -f $base/install.sh~

    # if we ever have binaries here we should strip them as well.
    # we could do a find at the end over all dirs except the samples.
else
    cd $DYNAMORIO_LIBUTIL
    make $target_config $defines clean all
    $CP drconfig.{dll,lib} $base/$projname/lib32
    $CP drconfig.{pdb,map} $base/symbols/lib32
    cd $DYNAMORIO_TOOLS
    make $target_config $defines EXTERNAL_DRVIEW=1 clean all
    $CP drdeploy.exe $base/$projname/bin
    $CP drdeploy.{pdb,map} $base/symbols/bin
    # drdeploy needs this in same dir: make dup copy
    $CP drconfig.dll $base/$projname/bin
    $CP DRview.exe $base/$projname/bin/bin32/drview.exe
    $CP DRview.{pdb,map} $base/symbols/bin/bin32

    if [ $projname != "viper" ]; then
        cd DRgui
        make $defines clean all
        $CP build_dbg_demo/DRgui.exe $base/$projname/bin/bin32/drgui.exe
    fi
fi

##################################################
# docs

# release-build headers were built above; libutil/dr_config.h should be there
if [ -z $DYNAMORIO_API ] ; then
    cd $DYNAMORIO_HOME/api
else
    cd $DYNAMORIO_API
fi
cd docs
# do separate clean to avoid cygwin bug where dir is not fully deleted
# and so it can't create it when do "make clean pdf/all"
make clean
if [ $projname == "viper" ]; then
    make $target_config pdf
else
    # see comments below: we're not building pdf
    make $target_config all
fi
if [ $projname == "viper" ]; then
    # replace DR, DynamoRIO, and client in header files w/ VIPER and VIPA
    make VMSAFE=1 HEADER_DIR=$base/$projname/include update_headers
fi
$CP -r html $base/$projname/docs
if [ $projname == "viper" ]; then
    # The pdf output is messed up (more so on Linux) xref PR 314339 about fixing it.
    $CP latex/refman.pdf $base/$projname/docs/$projname.pdf
fi

##################################################
# provide pre-compiled dlls for samples
# test our setup so far by using it to build

if [ -z $DYNAMORIO_API ] ; then
    cd $DYNAMORIO_HOME/api
else
    cd $DYNAMORIO_API
fi
cd samples

# args are <defines> <subdir> <arch>
build_samples() {
    if [ $# -ne 3 ]; then
        echo "Internal error calling build_samples()"
        exit 127
    fi
    # no stats sample for viper and no security sample for vmap
    if [ $projname == "viper" ]; then
        DYNAMORIO_HOME=$base/$projname make EXCLUDE="stats.c tracedump.c" $1 clean all
    else
        # PR 236973: there is no "g++" toolchain binary so we have to include whole path
        DYNAMORIO_HOME=$base/$projname make GXX=${gxx/ARCH/$3} EXCLUDE=MF_moduledb.c $1 clean all
    fi
    mkdir -p $base/$projname/docs/html/samples/build$2
    if [ $linux -eq 1 ]; then
        $CP *.so $base/$projname/docs/html/samples/build$2
        # FIXME - better way to handle executables
        $CP tracedump $base/$projname/docs/html/samples/build$2
    else
        $CP *.{pdb,dll,exe} $base/$projname/docs/html/samples/build$2
        if [ $projname != "viper" ]; then
        # we don't want security api sample app
            rm -f $base/$projname/docs/html/samples/build$2/VIPA_test.exe
        fi
        # PR 232490: ensure services can read these
        chmod ugo+rx $base/$projname/docs/html/samples/build$2/*.{dll,exe}
    fi
}

mkdir -p $base/$projname/docs/html/samples
# must setup environment for samples Makefile
# we assume for Linux that something reasonable is already set up
if [ $windows -eq 1 ]; then
    export INCLUDE=`cygpath -wa "${TCROOT}/win32/winsdk-6.1.6000/VC/INCLUDE"`\;`cygpath -wa "${TCROOT}/win32/winsdk-6.1.6000/Include"`;
    export LIB=`cygpath -wa "${TCROOT}/win32/winsdk-6.1.6000/VC/LIB"`\;`cygpath -wa "${TCROOT}/win32/winsdk-6.1.6000/Lib"`;
    export PATH=`cygpath -ua "${TCROOT}/win32/winsdk-6.1.6000/VC/Bin"`:$PATH;
fi
build_samples "ARCH=x86 GCC=/build/toolchain/lin32/gcc-4.1.2-2/bin/i686-linux-gcc" 32 i686
if [ $windows -eq 1 ]; then
    export LIB=`cygpath -wa "${TCROOT}/win32/winsdk-6.1.6000/VC/LIB/x64"`\;`cygpath -wa "${TCROOT}/win32/winsdk-6.1.6000/Lib/x64"`;
    export PATH=`cygpath -ua "${TCROOT}/win32/winsdk-6.1.6000/VC/Bin/x86_x64"`:$PATH;
fi
build_samples "ARCH=x64 GCC=/build/toolchain/lin32/gcc-4.1.2-2/bin/x86_64-linux-gcc" 64 x86_64


##################################################
# README file

cd $DYNAMORIO_HOME
$CP License.txt $base/$projname/

# FIXME PR 232127: once gets larger we won't want to auto-generate but for now:
echo -e 'QUICKSTART\n' > $base/$projname/README
if [ $linux -eq 1 ]; then
    echo -e "% bin/drdeploy -client docs/html/samples/build32/bbsize.so 0x1 \"\" ls" >> $base/$projname/README
    echo -e "Or if on 64-bit Linux" >> $base/$projname/README
    echo -e "% bin/drdeploy -64 -client docs/html/samples/build64/bbsize.so 0x1 \"\" ls" >> $base/$projname/README
    echo -e "" >> $base/$projname/README
    echo -e "% bin/drdeploy" >> $base/$projname/README
    echo -e "For additional options." >> $base/$projname/README
else
    if [ $projname == "viper" ]; then
        echo -e '% bin/drdeploy.exe -32 -reg calc.exe -root `cygpath -wa .` -mode code -client `cygpath -wa ./docs/html/samples/build32/bbsize.dll` 0x1 ""' >> $base/$projname/README
        echo -e 'Or for 64-bit' >> $base/$projname/README
        echo -e '% bin/drdeploy.exe -64 -reg calc.exe -root `cygpath -wa .` -mode code -client `cygpath -wa ./docs/html/samples/build64/bbsize.dll` 0x1 ""' >> $base/$projname/README
    else
        # no -mode arg for vmap
        echo -e '% bin/drdeploy.exe -32 -reg calc.exe -root `cygpath -wa .` -client `cygpath -wa ./docs/html/samples/build32/bbsize.dll` 0x1 ""' >> $base/$projname/README
        echo -e 'Or for 64-bit' >> $base/$projname/README
        echo -e '% bin/drdeploy.exe -64 -reg calc.exe -root `cygpath -wa .` -client `cygpath -wa ./docs/html/samples/build64/bbsize.dll` 0x1 ""' >> $base/$projname/README
    fi
    echo -e 'Then:' >> $base/$projname/README
    echo -e '% cmd /c start calc' >> $base/$projname/README
    echo -e '\nNow close calc and a messagebox should pop up.' >> $base/$projname/README
fi


cd $base
if [ $linux -eq 1 ]; then
    tar -c -z -v -f $pkgname-$os-$vernum-$buildnum.tgz $projname
    tar -c -z -v -f $pkgname-$os-$vernum-$buildnum-symbols.tgz symbols

    #Removed since no longer need license/installer
    #the zip file must not have the version in it since install.sh hardcodes its name
    #zip -r -P UId3E4c2WPSiFlOzJDQx $projname.zip $projname.tgz 
    #tar -cf $projname-$vernum-$buildnum.tar $projname.zip install.sh
else
    zip -r $pkgname-$os-$vernum-$buildnum.zip $projname/
    zip -r $pkgname-$os-$vernum-$buildnum-symbols.zip symbols/

    #Removed since no longer need license/installer
    #if [ $projname != "viper" ]; then
    #    # PR 259103: create a setup.exe that includes the zip file inside it
    #    cd $DYNAMORIO_TOOLS/DRinstall
    #    make ZIPFILE=$base/$projname-$vernum-$buildnum.zip clean all
    #    $CP build_rel/DRinstall.exe $base/setup-$vernum-$buildnum.exe
    #fi
fi
    
if [ -n "$publishdir" ]; then
    cd $DYNAMORIO_HOME
    if ! test -e $publishdir; then
        mkdir -p $publishdir
    fi
    $CP $base/$pkgname-$os-$vernum-$buildnum* $publishdir
fi
