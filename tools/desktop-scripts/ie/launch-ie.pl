# **********************************************************
# Copyright (c) 2006 VMware, Inc.  All rights reserved.
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


# perl script

# Usage: perl launch-ie.pl [-mp] [htmlpage]
#
# -mp creates multiple IE processes
# otherwise creates one IE process with 10 windows
# assumes c:\desktop-benchmarks\ie as directory for html files

#
# ASSUMES IE is in the path
#

# TODO: use get options, and print usage/help
# FIXME: hardcodes cnn.htm
#
# usage: launch-ie [-mp] [10]
#


my $ie = "";
my $prog = "launch-ie.pl";

$_ = $ARGV[0];
if (m/-mp/) {
    $ie = "IEXPLORE.EXE";
    print "$prog: Launching multiple IE processes\n";
    shift;
}

my $NUM_IE = 10;

$NUM_IE = $ARGV[0] if (defined $ARGV[0]);

# TODO: get from command line/current dir
my $htmldir = "c:\\desktop-benchmarks\\boot_data_scripts\\ie";
#my $file = $ARGV[0];
my $file;

$file = "cnn.htm" unless defined $file;

my $i = 0;
for ($i = 0; $i < $NUM_IE; $i++) {
    system("start $ie $htmldir\\$file");
}
