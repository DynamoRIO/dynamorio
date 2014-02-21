#!/usr/bin/perl

# **********************************************************
# Copyright (c) 2004-2010 VMware, Inc.  All rights reserved.
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

### bmmemabs.pl
### author: Derek Bruening   April 2003
###
### compares benchmark results produced with bmtable.pl
### computes absolute memory usage difference between two runs

$usage = "Usage: $0 [-k] <table1> <table2>\n";

if ($#ARGV < 0) {
    print $usage;
    exit;
}
if ($ARGV[0] eq "-k") {
    $ignore_errors = 1;
    shift;
}
if ($#ARGV != 1) {
    print $usage;
    exit;
}
$file1 = $ARGV[0];
$file2 = $ARGV[1];
$PWD = $ENV{'PWD'};
open(FILE1, "< $file1") || die "Error: Couldn't open $file1 for input\n";
open(FILE2, "< $file2") || die "Error: Couldn't open $file2 for input\n";
print "# in dir $PWD:\n#\t$file1 - $file2\n";
print "-------------------------------------------------\n";
$num_bmarks = 0;
while (<FILE1>) {
    if ($_ =~ /^\s*(\w+)\s+\d+\/\d+\s+(\S+)\s+(\S+)\s+([\d\.]+)\s+(\d+)\s+(\d+)/) {
        $bmark = $1;
        $status1{$bmark} = $2;
        $cpu1{$bmark} = $3;
        $time1{$bmark} = $4;
        $rss1{$bmark} = $5;
        $vsz1{$bmark} = $6;
        $name_bmarks[$num_bmarks] = $bmark;
        $num_bmarks++;
    }
}
close(FILE1);
while (<FILE2>) {
    if ($_ =~ /^\s*(\w+)\s+\d+\/\d+\s+(\S+)\s+(\S+)\s+([\d\.]+)\s+(\d+)\s+(\d+)/) {
        $bmark = $1;
        $status2{$bmark} = $2;
        $cpu2{$bmark} = $3;
        $time2{$bmark} = $4;
        $rss2{$bmark} = $5;
        $vsz2{$bmark} = $6;
        $name_bmarks[$num_bmarks] = $bmark;
        $num_bmarks++;
    }
}
close(FILE2);

printf("%10s   %-6s  %-9s  %-7s  %-7s\n",
       "Benchmark", "Status", "Time(min)", "RSS(KB)", "VSz(KB)");

@sorted = sort(@name_bmarks);
$last = "";
$harsum{"time"} = 0;
$harnum{"time"} = 0;
$harsum{"rss"} = 0;
$harnum{"rss"} = 0;
$harsum{"vsz"} = 0;
$harnum{"vsz"} = 0;
foreach $s (@sorted) {
    if ($s eq $last) {
        next;
    }
    $last = $s;

    printf "%10s  %4s/%-4s   ", $s, &stat($status1{$s}), &stat($status2{$s});

    # both must be ok
    # we only look at status -- we assume table filtered out bad %CPU
    if ($ignore_errors || ($status1{$s} =~ /ok/ && $status2{$s} =~ /ok/)) {
        $bad = 0;
    } else {
        $bad = 1;
    }

    if (!$bad && defined($time1{$s}) && defined($time2{$s}) &&
        $time1{$s} > 0 && $time2{$s} > 0) {
        $ratio = $time1{$s} / $time2{$s};
        $harsum{"time"} += 1/$ratio;
        $harnum{"time"}++;
        printf "%6.3f    ", $ratio;
    } else {
        printf "%6s    ", "------";
    }
    if (!$bad && defined($rss1{$s}) && defined($rss2{$s}) &&
        $rss1{$s} > 0 && $rss2{$s} > 0) {
        $diff = $rss1{$s} - $rss2{$s};
        $harsum{"rss"} += $diff;
        $harnum{"rss"}++;
        printf "%6d    ", $diff;
    } else {
        printf "%6s    ", "------";
    }
    if (!$bad && defined($vsz1{$s}) && defined($vsz2{$s}) &&
        $vsz1{$s} > 0 && $vsz2{$s} > 0) {
        $diff = $vsz1{$s} - $vsz2{$s};
        $harsum{"vsz"} += $diff;
        $harnum{"vsz"}++;
        printf "%6d    ", $diff;
    } else {
        printf "%6s    ", "------";
    }
    print "\n";
}

print "-------------------------------------------------\n";
printf "%12s     ", "harmonic/arith mean";
if ($harsum{"time"} > 0) {
    $hmean = $harnum{"time"} / $harsum{"time"};
    printf "%4.3f    ", $hmean;
} else {
    printf "%5s    ", "-----";
}
if ($harsum{"rss"} > 0) {
    $hmean = $harsum{"rss"} / $harnum{"rss"};
    printf "%6d    ", $hmean;
} else {
    printf "%5s    ", "-----";
}
if ($harsum{"vsz"} > 0) {
    $hmean = $harsum{"vsz"} / $harnum{"vsz"};
    printf "%6d    ", $hmean;
} else {
    printf "%5s    ", "-----";
}
print "\n";

sub stat($) {
    my ($status) = @_;
    if ($status eq "") {
        return "--";
    } else {
        return $status;
    }
}
