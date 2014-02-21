#!/usr/bin/perl -w

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

### process-kstats.pl
###
### Summarize kstat info

$verbose = 0;

$usage = "Usage: $0 [-h] <kstats file>\n";

while ($#ARGV >= 0) {
    if ($ARGV[0] eq "-v") {
        $verbose = 1;
    } elsif ($ARGV[0] eq "-h") {
        print $usage;
        exit;
    } else {
        $infile = $ARGV[0];
    }
    shift;
}
if (!defined($infile)) {
    print $usage;
    exit;
}

open(PROFILE, "< $infile") or die "Error opening $infile\n";

$read_kstats = 0;
@hit_array = ();
$total_cycles = 0;

while (<PROFILE>) {

    chop($_);

    if ($verbose) {
        print STDERR "Process $.: <$_>\n";
    }

    if (/^Process KSTATS:/ || /^Thread \d+ KSTATS \{/ ) {

        if ($verbose) {
            print STDERR "Found kstats\n";
        }
        $read_kstats = 1;
    } elsif ($read_kstats) {
        # Kstats output for a single ktats nodes is 2 lines and looks like:
        # interp:   6559513 totc,       1 num,   6559513 minc,   6559513 avg,   6559513 maxc,   6559513 self,         0 sub,
        # 2 ms,         0 ms out,in interp loop
        if ( /\w+:[ ]*(\d+) totc,[ ]*(\d+) num,/ ) {
            $totc = $1;
            $num = $2;
            # /(\w+):(\d)+ totc,[ ]*(\d+) num,[ ]*(\d+) minc,[ ]*(\d+) avg,[ ]*/
            /,[ ]*\d+ maxc,[ ]*(\d+) self,[ ]*(\d+) sub/;
            $self = $1;
            $sub_time = $2;

            # Read the next line to get the name.
            $_ = <PROFILE>;
            /(\d+) ms,[ ]*(\d+) ms out,[ ]*(.+)/;
            $ms = $1;
            $outliers = $2 == 0 ? 1 : $2;
            $name = $3;

            # The total # cyles are attributed to thread_measured, which is also
            # "total measured and propagated in thread"
            if ($name eq "total measured and propagated in thread") {
                $total_cycles = $totc;
            }
            $name =~ s/ /-/g;
            if (length($name) > 30) {
                $name = substr($name, 0, 30);
            }
            $data = "$num $totc $self $sub_time $outliers $ms $name";
            if ($verbose) {
                print STDERR "Data -- $data\n";
            }
            push @hit_array, $data;
        }
    }
}

# Sorts elements of the form "$num $totc $self $sub_time $outliers $ms $name"
sub by_time {
    @a_fields = split / /, $a;
    @b_fields = split / /, $b;
    # Sort based on the 2nd field, the totc
    $b_fields[1] <=> $a_fields[1];
}

@sorted_hits = sort by_time @hit_array;
if ($#sorted_hits == 0) {
   printf "No edges!\n";
   exit;
}

if ($total_cycles == 0) {
    printf "WARNING: total cycles could not be calculated\n";
}
printf "%10s %10s %30s %20s %20s %20s %10s\n", "%-age", "# calls", "Name", "Totc", "Self", "Sub", "Outlier";;
printf "%10s %30s %20s %20s %20s %10s\n", "====", "====", "====", "====", "====", "===", "===";
for ($i = 0; $i < $#sorted_hits; $i++) {
    @fields = split / /, $sorted_hits[$i];
    $outlier = $fields[5] == 0 ? 0 : $fields[4]/$fields[5];
    $percentage = $total_cycles == 0 ? 0 : 100 * $fields[1] / $total_cycles;
    # Elements are of the form "$num $totc $self $sub_time $outliers $ms $name"
    printf "%10.2f %10d %30s %20d %20d %20d %10.2f\n", $percentage, $fields[0],
    $fields[6], $fields[1], $fields[2], $fields[3], $outlier;
    #printf "%10d %30s %20d %20d %20d %10.2f\n", $num, $name, $totc, $self,
    #$sub_time, $outliers / $ms;
}
