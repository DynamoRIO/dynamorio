#!/usr/bin/perl

# **********************************************************
# Copyright (c) 2014 Google, Inc.  All rights reserved.
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
# * Neither the name of Google, Inc. nor the names of its contributors may be
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

# Pass this 1) an OP_ constant and 2) a decoding table file.
# It will construct the encoding chain and in-place edit the table file.
# It has assumptions on the precise format of the decoding table
# and of the op_instr* starting point array.
#
# To run on everything:
#
#   for i in `egrep '^ *OP_[a-z]' core/arch/arm/opcode.h | sed 's/,//'`; do echo $i; tools/arm_table_chain.pl $i core/arch/arm/table_a32.c; done
#
# Currently requires manual fixup for optimized encoding chains (OP_bl and OP_cdp).

my $verbose = 1;

die "Usage: $0 OP_<opcode> <table-file>\n" if ($#ARGV != 1);
my $op = shift;
my $infile = shift;
my $table = "";
my $shape = "";
my $major = 0;
my $minor = 0;
my $instance = 0;

open(INFILE, "< $infile") || die "Couldn't open $file\n";
while (<INFILE>) {
    print "xxx $_\n" if ($verbose > 2);
    if (/^#define (\w+)\s+\(ptr_int_t\)&(\w+)/) {
        $shorthand{$2} = $1;
        print "shorthand for $2 = $1\n" if ($verbose > 1);
    }
    if (/^const instr_info_t (\w+)([^=]+)=/) {
        $table = $1;
        $shape = $2;
        $major = -1;
        $minor = 0;
    }
    if (/{\s*\/\*\s*(\d+)\s*\*\/\s*$/) {
        $major++;
        $minor = 0;
    }
    if (/^\s*{$op[ ,]/) {
        $entry[$instance]{'line'} = $_;
        if ($shape =~ /\d/) {
            $entry[$instance]{'addr_short'} =
                sprintf "$shorthand{$table}\[$major][0x%02x]", $minor;
            $entry[$instance]{'addr_long'} =
                sprintf "$table\[$major][0x%02x]", $minor;
        } else {
            $entry[$instance]{'addr_short'} =
                sprintf "$shorthand{$table}\[0x%02x]", $minor;
            $entry[$instance]{'addr_long'} =
                sprintf "$table\[0x%02x]", $minor;
        }
        # Order for sorting:
        # + Prefer no-shift over shift
        # + Prefer shift via immed over shift via reg
        # + Prefer P=1 and U=1
        my $priority = 0;
        $priority--   if (/sh2, i/);
        $priority--   if (/sh2, R/);
        $priority--   if (/xop_shift/);
        $priority++   if (/, [in]/);
        $priority++   if (/, M.\d/);
        $priority--   if (/, M.S/);
        $priority+=5  if (/PUW=1../);
        $priority+=10 if (/PUW=1.0/);
        $priority++   if (/PUW=.1./);
        $entry[$instance]{'priority'} = $priority;
        if ($verbose > 0) {
            my $tmp = $entry[$instance]{'addr_long'};
            print "$priority $tmp $_";
        }
        $instance++;
    }
    $minor++ if (/^\s*{[A-Z]/);
}
close(INFILE);

@entry = sort({$b->{'priority'} <=> $a->{'priority'}} @entry);

if ($verbose > 1) {
    print "Sorted:\n";
    for (my $i = 0; $i < @entry; $i++) {
        my $tmp = $entry[$i]{'addr_long'};
        my $pri = $entry[$i]{'priority'};
        print "$pri $tmp\n";
    }
}

# Now edit the file in place
$^I = '.bak';
@ARGV = ($infile);
while (<>) {
    if (/^\s+\/\* $op[ ,]/) {
        my $start = $entry[0]{'addr_long'};
        s/&.*,/&$start,/;
    }
    for (my $i = 0; $i < @entry - 1; $i++) {
        if ($_ eq $entry[$i]{'line'}) {
            my $chain = $entry[$i+1]{'addr_short'};
            s/, [\w\[\]]+},/, $chain},/;
        }
    }
    print;
}
