# **********************************************************
# Copyright (c) 2014-2015 Google, Inc.    All rights reserved.
# Copyright (c) 2009 Derek Bruening.    All rights reserved.
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
# * Neither the name of Derek Bruening nor the names of contributors may be
#   used to endorse or promote products derived from this software without
#   specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED. IN NO EVENT SHALL DEREK BRUENING OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
# SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
# CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
# OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
# DAMAGE.

cmake_minimum_required(VERSION 3.7)

# Instructions to set up a machine to run nightly regression tests:
#
# 1) Pick a directory $nightly.  Create $nightly/run.
# 2) Check out the sources (read-only):
#    cd $nightly; git clone https://github.com/DynamoRIO/dynamorio.git src
# 3) To keep the sources pristine, copy this file from
#    $nightly/src/suite/nightly.cmake to $nightly/nightly.cmake.  Pick a site
#    name and change the site= string below in $nightly/nightly.cmake.  The
#    convention is Architecture.OSVersion.Compiler.Hostname, where
#    Architecture comes from {X64,X86,ARM,AArch64}.
#    E.g.: X64.Fedora22.gcc511.Ancalagon or X86.Windows10.VS2013.Buildbot44.
#    The site name should not contain spaces or quotes.
# 4) Build a version of CTest that sets the Content-Type header to
#    satisfy the security rules on our CDash server.
#    You will need this patch, which is too new to be in a released CTest version:
#      https://cmake.org/gitweb?p=cmake.git;a=commitdiff;h=1b700612
#    For more information, see https://cmake.org/Bug/view.php?id=15774
#    Recent (after Oct 8 2015) nightly build binaries with the patch can be
#    downloaded from https://cmake.org/download/.
# 5) Set up a cron job to run:
#    cd $nightly/run; /path/to/patched/ctest -V -S ../nightly.cmake > ctest.log 2>&1
#
# Each run will clobber the build directories from the last run to save disk
# space, but the xml file results will be recorded on the CDash server.
# The run directory needs approximately 1.5GB of space.

# We point at src/suite/runsuite.cmake, assuming this file will be executed
# from two dirs below.
# Site name should not have spaces or quotes.
ctest_run_script(${CTEST_SCRIPT_DIRECTORY}/src/suite/runsuite.cmake,nightly\;long\;site=X64.Fedora22.gcc511.Ancalagon)
