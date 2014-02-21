#!/usr/bin/perl -w

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

### extract-stats.pl
###
### Summarize data gathered from DR logs under the various loglevels


$usage = "Usage: $0 <kstats file>\n";

if ($#ARGV + 1 == 1) {
    $logfile = $ARGV[0];
}

if (!defined($logfile)) {
    print $usage;
    exit;
}

open(PROFILE, "< $logfile") || die "Error opening $logfile\n";

print "Kstats from logfile $logfile\n(All times are in msec)\n";

@array = ();
$total_time = 0;
while (<PROFILE>) {

    chop($_);

    if (/[ ]*(\w+):[ ]*(\d+) totc/ && /,[ ]*(\d+) self,/) {
        /[ ]*(\w+):[ ]*(\d+) totc/;
        $node = $1;
        $tot_time = $2;
        /,[ ]*(\d+) self,/;
        $self_time = $1;
        $array[$#array + 1] = "$node $tot_time $self_time";
        if ($node eq "thread_measured") {
            $total_time = $tot_time;
        }
    }

}
close(PROFILE);

# tot time is the 2nd element in the entry
sub by_tot {
    ($trash1, $a_time, $trash2) = split / /, $a;
    ($trash1, $b_time, $trash2) = split / /, $b;
    $b_time <=> $a_time;
}

# self time is the 3rd element in the entry
sub by_self {
    ($trash1, $trash2, $a_time) = split / /, $a;
    ($trash1, $trash2, $b_time) = split / /, $b;
    $b_time <=> $a_time;
}

@sorted_kstats = sort by_self @array;
if ($#sorted_kstats > 0) {
    printf "\nSorted by self time\n";
    printf "%50s  %15s  %10s\n", "Node", "Time", "%-age";
    printf "%50s  %15s  %10s\n", "====", "====", "=====";
    for ($i = 0; $i <= $#sorted_kstats; $i++) {
        ($node, $trash, $time) = split / /, $sorted_kstats[$i];
        printf "%50s  %15.0f", $node, $time;
        $percent = 0;
        if ($total_time > 0) {
            $percent = 100 * $time / $total_time;
        }
        printf "  %10.2f\n", $percent;
    }
}

@sorted_kstats = sort by_tot @array;
if ($#sorted_kstats > 0) {
    printf "\nSorted by total time\n";
    printf "%50s  %15s  %10s\n", "Node", "Time", "%-age";
    printf "%50s  %15s  %10s\n", "====", "====", "=====";
    for ($i = 0; $i <= $#sorted_kstats; $i++) {
        ($node, $time, $trash) = split / /, $sorted_kstats[$i];
        printf "%50s  %15.0f", $node, $time;
        $percent = 0;
        if ($total_time > 0) {
            $percent = 100 * $time / $total_time;
        }
        printf "  %10.2f\n", $percent;
    }
}
