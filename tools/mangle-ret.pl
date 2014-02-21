#!/usr/bin/perl

# **********************************************************
# Copyright (c) 2004 VMware, Inc.  All rights reserved.
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

### mangle-ret.pl
### for converting ret into jmp* in a .s file,
### to see impact of rsb on native apps (independent of DynamoRIO)
###
### author: Derek Bruening   August 2002
###

$usage = "Usage: $0 <file>\n";

if ($#ARGV < 0) {
    print $usage;
    exit 1;
}
$file = $ARGV[0];
$file =~ /(.*)\.s$/;
if ($1 eq "") {
    print "Error: input file must be .s file\n";
    exit 1;
}
$out = "$1.mangle.s";
open(FILE, "< $file") || die "Error: Couldn't open $file for input\n";
open(OUT, "> $out") || die "Error: Couldn't open $out for output\n";
$rets = 0;
while (<FILE>) {
    if ($_ =~ /^\s*ret\s*$/) {
        $rets++;
        print OUT "# ---- translation of ret -----\n";
        print OUT "        addl \$4,%esp\n";
        print OUT "        jmp *0xfffffffc(%esp)\n";
    } elsif ($_ =~ /^\s*ret\s+\$([0-9a-fA-Fx]+)\s*$/) {
        print "Found ret with immed operand: $_";
        # extra stack space is removed after return address is popped
        $rets++;
        $extra = $1;
        if ($extra =~ /0x([0-9a-fA-F]+)/) {
            $remove = 4 + hex($1);
        } else {
            $remove = 4 + $extra;
        }
        $offs = -$remove;
        print  OUT "# ---- translation of ret -----\n";
        print  OUT "        addl \$$remove,%esp\n";
        printf OUT "        jmp *0x%08x(%%esp)\n", $offs;
    } elsif ($_ =~ /^\s*ret\s+/) {
        print "Error: Found unidentified ret: $_";
    } else {
        print OUT $_;
    }
}
close(FILE);
print "Translated $rets returns in $file to $out\n";
