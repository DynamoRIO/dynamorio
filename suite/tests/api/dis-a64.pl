#!/usr/bin/perl -w

# **********************************************************
# Copyright (c) 2016 ARM Limited. All rights reserved.
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
# * Neither the name of ARM Limited nor the names of its contributors may be
#   used to endorse or promote products derived from this software without
#   specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED. IN NO EVENT SHALL ARM LIMITED OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
# SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
# CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
# OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
# DAMAGE.

# Generate a new draft of "dis-a64.txt" from an existing "dis-a64.txt" and
# "codec.txt". (Use "/dev/null" if you do not already have those files.)
# Review the output carefully before relying on it.

use strict;

if ($#ARGV != 1) {
    print "Usage: dis-a64.pl .../dis-a64.txt .../codec.txt\n";
    exit 0;
}

my $dis_file = $ARGV[0];
my $codec_file = $ARGV[1];
my $asmfile = "tmp-dis-a64.s";
my $objfile = "tmp-dis-a64.o";

my $out = "";
my %enc;

# Get encodings from old "dis-a64.txt".
{
    open(my $fd, "<", $dis_file) || die;
    foreach (<$fd>) {
        if (/^[ \t]*(#.*)?$/) {
            $out .= $_;
        } else {
            /^[ \t]*([0-9a-f]{8})[: \t]/ || die;
            $enc{hex($1)} = 1;
        }
    }
}

# Get encoding from "codec.txt".
{
    open(my $fd, "<", $codec_file) || die;
    foreach (<$fd>) {
        next if /^[ \t]*(#.*)?$/;
        my ($t1, $t2);
        if (/^\s*([01x]{32})\s/) {
            ($t1, $t2) = ($1, $1);
            $t1 =~ tr/x/0/;
            $t2 =~ tr/1x/01/;
            ($t1, $t2) = (oct("0b$t1"), oct("0b$t2"));
        }
        elsif (/^\s*[x?-]{32}\s/) {
            next;
        }
        else {
            die ;
        }

        # Set every variable bit.
        $enc{$t1 | $t2} = 1;
        # Put a different value in each of the four register fields.
        # 0x81041 == 0b01000_0_00100_00010_00001:
        $enc{$t1 | ($t2 & 0x81041)} = 1;
    }
}

foreach my $e (sort {$a <=> $b} keys %enc) {
    $out .= sprintf("%08x : %-31s: %s\n", $e, &dis_objdump($e), &dis_dynamorio($e));
}

print $out;
exit;

sub dis_objdump {
    my ($e) = @_;
    my $fd;
    open($fd, ">", $asmfile) || die;
    print $fd sprintf("        .inst   0x%08x\n", $e);
    system("gcc", $asmfile, "-c", "-o", $objfile) && die;
    open($fd, "-|", "objdump", "--adjust-vma=0x10000000", "-d", $objfile) || die;
    my $hex = sprintf("%08x", $e);
    if (join("", <$fd>) =~ /\n\s*10000000:\s+$hex\s+(.*)/) {
        my $dis = $1;
        $dis =~ s/\s+/ /g;
        $dis =~ s/;.*//;
        $dis =~ s/\/\/.*//;
        $dis =~ s/<\S+> ?$//;
        $dis =~ s/ $//;
        if ($dis =~ / /) {
            $dis = sprintf("%-6s %s", "$`", "$'");
        }
        return $dis;
    }
    return "";
}

sub dis_dynamorio {
    my ($e) = @_;
    my $hex = sprintf("%08x", $e);
    my $dis =
        `LD_LIBRARY_PATH=lib64/debug/:lib64/release suite/tests/bin/api.dis-a64 -d $hex`;
    $dis =~ s/\n//;
    return $dis;
}
