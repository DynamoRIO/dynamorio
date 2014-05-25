#!/usr/bin/perl -w

# **********************************************************
# Copyright (c) 2005 VMware, Inc.  All rights reserved.
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

# drview-analyze.pl
#    analysis of continous drview snapshosts
#
# assuming DRview output of
# Name          PID DR  Bld CPU User  Hndl Thr    PVSz     VSz    PPriv    Priv    PWSS     WSS   Fault PPage  Page PNonP  NonP
# System          8 ?    -1  0%   0%   353  55    5836    5772      640      36     856     232   48566     0     0     0     0
# smss.exe      216 N    -1  0%   0%    33   6    8456    4296     1720     156    1972     400     999    13     5    15     1
#

# takes variable and current value,
# returns result in global last and delta
sub calculate_delta
{
    my ($var, $value) = @_;

    if (!$last{$exe,$pid,$var}) {
        $last{$exe,$pid,$var} = 0;
        $delta{$var} = 0;
    } else {
        $delta{$var} = $value - $last{$exe,$pid,$var};
    }
    $last{$exe,$pid,$var} = $value;

    if (!$churn{$exe,$pid,$var}) {
        $churn{$exe,$pid,$var} = 0;
    }

    # we should count only positive values
    # this way monotonically growing functions will also
    if ($delta{$var} > 0 ) {
        $churn{$exe,$pid,$var} += $delta{$var};
    }

#TODO: count delta edges - so we know how many flex points we've had

}

$showall = 0;

sub header
{

    if (!$showall) {
        print "Process       PID     Threads             WorkSet/KB                 Page Faults            PrivateVSz/KB\n";
        print "                  cur dlt  chrn     cur     dlt     churn       cum     dlt      chrn     cur     dlt      chrn\n";
# For current value metrics like threads and workset delta is last difference, churn is total positive difference
# For cumulative value metrics like page faults, churn is simply the difference from the starting point
    }
}

header();

$errors = 0;
$ignored = 0;
$verbose = 0;

while ($line = <>) {
    chomp ($line);

    if ($line =~ "    Could not open.*") {
        $errors++;
    }
    elsif ($line =~ "--------------------------------------------------") {
        $ignored++;
    }
    elsif ($line eq "\r") { # blank
        $ignored++;
    }
    elsif ($line eq "") { # blank
        $ignored++;
    }
    elsif ($line =~ /\w+ \w+ \d+ (\d+):(\d+):(\d+) (\d+)/) {
        # Wed Mar 23 10:27:26 2005
        $hour = $1;
        $min = $2;
        $sec = $3;
        # TODO: need smaller units as well
        if ($verbose) {
            print "$hour:$min:$sec\n";
        }
    }
    elsif ($line =~ /System load is -?(\d+)%/) {
        # TODO: fix the -
        $sysload = $1;
        if ($verbose) {
            print "Sysload $sysload\n";
        }
    }
    elsif ($line =~
/Name          PID DR  Bld CPU User  Hndl Thr    PVSz     VSz    PPriv    Priv    PWSS     WSS   Fault PPage  Page PNonP  NonP/
               ) {
        $newset = 1;
    }
    else {
        if ($newset) {
            print "\n";
        }

        ($exe, $pid, $underdr, $build,
         $cpuPCT, $userPCT,
         $handles, $threads,
         $peak_virtsz, $virtsz,
         $peak_privsz, $privsz,
         $peak_workset, $workset,
         $faults,
         $peak_paged_pool, $paged_pool,
         $peak_nonpaged_pool, $nonpaged_pool)
            = split(/\s+/, $line);

        # TODO: make sure nonpaged_pool is indeed a number to make sure this is a complete line

#    print "$exe, $pid, $underdr, $build, $cpuPCT, $userPCT, $handles, $threads, $peak_virtsz, $virtsz, $peak_privsz, $privsz, ";
#    print "$peak_workset, $workset, $faults, $peak_paged_pool, $paged_pool, $peak_nonpaged_pool, $nonpaged_pool\n";

#    /* single line best so can line up columns and process output easily */
        if ($showall) {
            printf "%-11.11s %5d %s %5d %s %s %5d %3d %7d %7d %8d %7d %7d %7d %7d %5d %5d %5d %5d\n",
            $exe, $pid, $underdr, $build, $cpuPCT, $userPCT, $handles, $threads, $peak_virtsz, $virtsz, $peak_privsz, $privsz,
            $peak_workset, $workset, $faults, $peak_paged_pool, $paged_pool, $peak_nonpaged_pool, $nonpaged_pool;
        }

# extract Thread handle rate and calculate difference

        calculate_delta('threads', $threads);
        calculate_delta('workset', $workset);
        calculate_delta('faults', $faults);
        calculate_delta('privsz', $privsz);

        printf "%-11.11s %5d", $exe, $pid;
        printf " %3d", $threads;
        printf " %3d", $delta{'threads'};
        printf " %5d", $churn{$exe,$pid,'threads'};

        printf " %7d", $workset;
        printf " %7d", $delta{'workset'};
        printf " %9d", $churn{$exe,$pid,'workset'};

        printf " %9d", $faults;
        printf " %7d", $delta{'faults'};
        printf " %9d", $churn{$exe,$pid,'faults'};

        printf " %7d", $privsz;
        printf " %7d", $delta{'privsz'};
        printf " %9d", $churn{$exe,$pid,'privsz'};
#TODO: print all other data - don't lose information!

        print "\n";
        $newset = 0;
    }
}

header();

# with absolute of negatives
#winmgmt.exe  1880  10   0  121   3132   -808   409996 5559853   3943  662978
