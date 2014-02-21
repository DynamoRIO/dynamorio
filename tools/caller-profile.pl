#!/usr/bin/perl

# **********************************************************
# Copyright (c) 2006-2007 VMware, Inc.  All rights reserved.
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

### caller-profile.pl
### author: Derek Bruening   April 2006
###
### Analyzes the output of crude caller profiling that
### looks like this:
###   0x710282fc 0x7102826c 0x710283a2 0x7103569a 242
###   0x710282fc 0x7102b25e 0x71035934 0x710299a5 31537
###   0x710282fc 0x7102826c 0x710283e3 0x710358f6 36271

$usage = "Usage: $0 [-build [exports/ subdir for lib]] [-base <DRdllbase>] [-dll <DRdllpath>] <callproffile>\n";

$bld = "lib32/debug";
$DR = $ENV{'DYNAMORIO_HOME'};
$DRdll = "";
$infile = "";
$base = 0;
$reloc = 0;

# get optional params
while ($#ARGV >= 0) {
    if ($ARGV[0] eq '-build') {
        if ($#ARGV <= 0) { print $usage; exit; }
        shift;
        $bld = $ARGV[0];
    } elsif ($ARGV[0] eq '-base') {
        if ($#ARGV <= 0) { print $usage; exit; }
        shift;
        $base = $ARGV[0];
    } elsif ($ARGV[0] eq '-dll') {
        if ($#ARGV <= 0) { print $usage; exit; }
        shift;
        $DRdll = $ARGV[0];
    } else {
        $infile = $ARGV[0];
        last;
    }
    shift;
}

die $usage if ($infile eq "");

if ($DRdll eq "") {
    $DRdll = "$DR/exports/$bld/dynamorio.dll";
}
die "Cannot find $DRdll" unless (-f $DRdll);

if ($base != 0) {
    if ($DRdll =~ /dbg/ || $DRdll =~ /debug/) {
        $normbase = 0x15000000;
    } else {
        $normbase = 0x71000000;
    }
    $reloc = $normbase - hex($base);
}

open(IN, "< $infile") || die "Error: Couldn't open $infile for input\n";
while (<IN>) {
    if (/^([0-9xa-fA-F ]+) (\d+)$/) {
        $addrs = $1;
        if ($reloc != 0) {
            $newaddrs = "";
            foreach $a (split(' ', $addrs)) {
                $newaddrs .= sprintf("0x%08x ", hex($a) + $reloc);
            }
            $addrs = $newaddrs;
        }
        $cnt=$2;
        $hexes[$num]=$addrs;
        # FIXME: ensure no symbol path set that will have network delays --
        # perhaps even have address_query.pl clear the symbol path every time?
        # FIXME: would be faster to batch up all addresses and then feed
        # through a single invocation of address_query.pl
        open(ADDR, "address_query.pl $DRdll $addrs |") ||
            die "Error running address_query.pl\n";
        $out[$num]=sprintf("count=$cnt\n");
        while (<ADDR>) {
            $out[$num] .= $_;
            if (/^[\w\.\/]+\((\d+)\)/) {
                $line = $1;
            } elsif (/^\(\w+\)\s+dynamorio!([^\+]+)/) {
                $syms[$num] .= "$1:$line ";
            }
        }
        $sortme[$num]=$num;
        $count[$num++]=$cnt;
        $sum+=$cnt;
    }
}
close(IN);

@sortme=sort({$count[$a]<=>$count[$b]} @sortme);

# summary
print "Total calls: $sum\n\n";
for ($i=$num-1; $i>=0; $i--) {
    $n=$sortme[$i];
    printf "%5.2f%% %s\n", 100*$count[$n]/$sum, $syms[$n];
}

# details
for ($i=$num-1; $i>=0; $i--) {
    $n=$sortme[$i];
    printf "\n--------\n%5.2f%%\n%s", 100*$count[$n]/$sum, $out[$n];
}
