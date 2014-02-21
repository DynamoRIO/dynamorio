#!/usr/bin/perl

# **********************************************************
# Copyright (c) 2005-2007 VMware, Inc.  All rights reserved.
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

# Prints out summary of memory statistics from a global log file
# This script is a merger of old heapacct.pl and new memsummary.sh

# FIXME: add cmd line arg -v
$verbose = 0;
$debug = 0;

if ($#ARGV < 0) {
    print "Usage: $0 <file>\n";
    exit 0;
}
$file = $ARGV[0];

open(IN, "< $file") || die "Couldn't open $file\n";
while (<IN>) {
    # heap acct: use final printed stats, since using max
    if (/Updated-at-end Process.*heap breakdown/) {
        while (<IN>) {
            if (/^([^:]+): cur=.*max=\s*(\d+)K, \#=\s*(\d+)/){
                $hnm[$hnms++]=$1;
                $hmax{$1}=$2;
                $hnum{$1}=$3;
            } else {
                last;
            }
        }
    }

    if (/^\(Begin\) All statistics/) {
        # reset
        $hnms = 0;
        $pnms = 0;
    } elsif (/Peak heap cap|Peak heap claim|Peak heap align|Peak stack|Peak mmap|Peak file|peak cap|Peak spec/) {
        if (/^\s*(.*) \(bytes\) :\s*(\d+)/){
            print "Matched \"$1\" \"$2\" \"$3\"\n" if ($debug);
            $nm=$1;
            $val=$2;
            $pnm[$pnms++]=$nm;
            $p{$nm}=$val;
        } else {
            print "Error: $_" unless (/Peak special heap units/);
        }
    } elsif (/Peak total memory from OS :\s*(\d+)/){
        $reserved_tot=$1;
    } elsif (/Our peak commited capacity \(bytes\) :\s*(\d+)/){
        $tot=$1;
    }

    # stats we don't or haven't always had peaks for
    if (/Heap align space \(bytes\) :\s*(\d+)/) {
        # old logs don't have this as a peak, so take max printed
        if ($1 > $align) {
            $align = $1;
        }
    } elsif (/Heap headers \(bytes\) :\s*(\d+)/) {
        # not a peak, so take max printed
        if ($1 > $header) {
            $header = $1;
        }
    } elsif (/Heap claimed \(bytes\) :\s*(\d+)/) {
        # old logs don't have this as a peak, so take max printed
        if ($1 > $claimed) {
            $claimed = $1;
        }
    }

    if ($verbose) {
        # completely separate numbers: variable-sized frequency
        # last printed is fine since these never decrease
        if (/Heap allocs in buckets :\s*(\d+)/) {
            $bucket = $1;
        } elsif (/Heap allocs variable-sized :\s*(\d+)/) {
            $variable = $1;
        }
    }
}
close(IN);

# things we don't have peak for
if (defined($p{'Peak heap claimed'})) {
    $claimed = $p{'Peak heap claimed'};
}
$tot_cap = $p{'Peak heap capacity'};
$pnm[$pnms++]='Unused heap (capacity - claimed)';
$p{$pnm[$pnms-1]} = $p{'Peak heap capacity'} - $claimed;

if (!defined($p{'Peak heap align space'})) {
    $pnm[$pnms++]='(Estimated) Peak heap align space';
    $p{$pnm[$pnms-1]} = $align;
}
# FIXME: don't have peak
$pnm[$pnms++]='(Estimated) Peak heap headers';
$p{$pnm[$pnms-1]} = $header;


printf "%35s 100.00%% %10d KB\n", "TOTAL COMMITTED MEMORY", $tot/1024;
printf "%35s %6.2f%% %10d KB\n", "TOTAL RESERVED (non-vmm) MEMORY",
    100*$reserved_tot/$tot, $reserved_tot/1024;
for ($i=0; $i<$pnms; $i++) {
    printf "%35s %6.2f%% %10d KB\n", $pnm[$i],
    100*$p{$pnm[$i]}/$tot, $p{$pnm[$i]}/1024;
}
for ($i=0; $i<$hnms; $i++) {
    printf "%35s %6.2f%% %10d KB\n", $hnm[$i],
    100*1024*$hmax{$hnm[$i]}/$tot, $hmax{$hnm[$i]};
    $hsum += $hmax{$hnm[$i]};
}

if ($verbose) {
    printf "Sum of heap acct is %10d KB\n", $hsum;
    printf "Adding special %10d\n", $p{'Peak special heap capacity'}/1024;
    printf "Adding align %10d\n", $p{'Peak heap align space'}/1024;
    $hsum += $p{'Peak heap align space'}/1024;
    $hsum += $p{'Peak special heap capacity'}/1024;
    # FIXME: this often comes out negative, presumably b/c the heapacct
    # maxes are not all simultaneous
    printf "Rest of heap: %10d KB\n", ($p{'Peak heap capacity'}/1024) - $hsum;
}

if ($verbose) {
    # FIXME: could print out detailed bucket info w/ new stats in logfiles
    $tot_alloc = $bucket + $variable;
    printf "Tot alloc: %10d\n", $tot_alloc;
    printf "Buckets:   %10d => %7.3f%%\n", $bucket, 100*$bucket/$tot_alloc;
    printf "Variable:  %10d => %7.3f%%\n", $variable, 100*$variable/$tot_alloc;
}
