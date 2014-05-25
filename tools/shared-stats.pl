#!/usr/bin/perl

# **********************************************************
# Copyright (c) 2004-2005 VMware, Inc.  All rights reserved.
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

# shared-stats.pl
# quick script to process thread sharing study results from a
# SHARING_STUDY DynamoRIO build
# by Derek Bruening, Dec 2003

# usage: give it the process logfile as stdin

# will see many copies of each stat, just want last one

$who = 0;
$have_traces = 0;
while (<>) {
    if (/^Shared basic block statistics/) {
        $who = 0;
        $print_bb = $_; # start over each time so get last
    } elsif (/^Shared trace statistics/) {
        $who = 1;
        $have_traces = 1;
        $print_bb .= $_;
    } elsif (/total blocks:\s*(\d+)/) {
        $tot[$who]=$1;
        $print_bb .= $_;
    } elsif (/shared count:\s*(\d+)/) {
        $cnt[$who]=$1;
        $print_bb .= $_;
    } elsif(/shared blocks:\s*(\d+)/) {
        $shar[$who]=$1;
        $print_bb .= $_;
    } elsif (/shared heap:\s*(\d+)/) {
        $heap[$who]=$1;
        $print_bb .= $_;
    } elsif(/shared cache:\s*(\d+)/) {
        $cache[$who]=$1;
        $print_bb .= $_;
    } elsif(/Peak stack/) {
        $print_st .= $_;
    } elsif(/Peak fcache.*:\s*(\d+)/) {
        $peak_cache=$1;
        $print_st .= $_;
    } elsif(/Peak heap.*:\s*(\d+)/) {
        $peak_heap=$1;
        $print_st .= $_;
    } elsif(/Peak total mem.*:\s*(\d+)/) {
        $peak_mem=$1;
        $print_st .= $_;
    } elsif(/Threads ever/) {
        $print_st = $_; # start over each time so get last
    } elsif(/Threads under.*:\s*(\d+)/) {
        $threads_under += $1;
        $threads_under_num ++;
    }
}

print $print_st;

print $print_bb;

printf  "Ave # threads under DR control: %9.2f\n", $threads_under/$threads_under_num;
printf  "  <ave over stats dumps, not time!>\n";

for ($i=0; $i<2; $i++) {
    if ($i == 0) {
        printf "\n*** Basic blocks ***\n";
    } else {
        if (!$have_traces) {
            last;
        }
        printf "\n*** Traces ***\n";
    }
    if ($shar[$i] == 0) {
        printf "No sharing!\n";
        printf "%% peak mem saved:           %9.2f\n", 0;
        next;
    }
    $per = ($cnt[$i] - ($tot[$i] - $shar[$i])) / $shar[$i];
    printf  "Unshared blocks:                %9d\n", $tot[$i] - $shar[$i];
    printf "%% blocks that are shared:       %9.2f\n", 100*$shar[$i]/$tot[$i];
    printf  "Ave # threads per shared block: %9.2f\n", $per;
        printf "%% peak heap that's shared:      %9.2f\n", 100*$heap[$i] / $peak_heap;
    printf "%% peak cache that's shared:     %9.2f\n", 100*$cache[$i] / $peak_cache;
    printf "%% peak mem that's shared:       %9.2f\n", 100*($cache[$i]+$heap[$i]) / $peak_mem;
# if we assume even split among threads, w/ shared bb cache should
# save $heap * (1 - 1/$per)
    $save_heap = $heap[$i] * (1 - 1/$per);
    $save_cache = $cache[$i] * (1 - 1/$per);
    printf "%% peak heap saved:              %9.2f\n", 100*$save_heap / $peak_heap;
    printf "%% peak cache saved:             %9.2f\n", 100*$save_cache / $peak_cache;
    printf "%% peak mem saved:               %9.2f\n", 100*($save_cache+$save_heap) / $peak_mem;
}

