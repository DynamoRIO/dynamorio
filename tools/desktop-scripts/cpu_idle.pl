# **********************************************************
# Copyright (c) 2006 VMware, Inc.  All rights reserved.
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

# cpu_idle.pl
#
# NOTE: ASSUMES drview is in path

use strict;

my $PROG = "cpu_idle.pl";

my @drview_out = `drview -listall -showmem`;

my $cpu_idx = -1;
my $wss_idx = -1;

($cpu_idx, $wss_idx) = get_idx(@drview_out);

my @data;
my $total_cpu = 0;
my $total_wss = 0;
my $parse = 0;

foreach $line (@drview_out) {
    $_ = $line;
    # skip lines till we see memory data
    if (m/PID.*Bld.*CPU/) {
        $parse = 1;
        next;
    }

    # skip cpu time spent running drview, shells etc.
    # (add others as needed)
    next if m/(DRview|cmd|bash|perl)/i;

    if ($parse) {
        @data = split(/\s+/, $line);
        $data[$cpu_idx] =~ s/\%//;
        $total_cpu += $data[$cpu_idx] ;
    }
}

print "$PROG: cpu almost idle ($total_cpu% total CPU utilization)\n" if ($total_cpu < 3);

exit ($total_cpu < 3);

#------------------------------------------------------------------------------
# To keep it a little more robust, find index for CPU, WSS etc.
#
sub get_idx() {
    my $line;
    my @titles;
    # get the title line
    foreach $line (@drview_out) {
        $_ = $line;
        if (m/PID.*CPU.*WSS/i) {
            @titles = split(/\s+/, $line);
            last;
        }
    }

    die "CPU column needed.." unless defined @titles;

    # little robust, get the index CPU, so this will work
    # even if showmem columns change (overboard?)
    my $idx = 0;
    my $cpu_idx = -1;
    my $wss_idx = -1;
    foreach $_ (@titles) {
        $cpu_idx = $idx if (m/CPU/i);
        $wss_idx = $idx if (m/WSS/i);
        $idx++;
    }

    return ($cpu_idx, $wss_idx);
}
