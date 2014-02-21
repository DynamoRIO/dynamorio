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

# measure_mem.pl
#
# NOTE: ASSUMES drview is in path
# NOTE: ASSUMES c:\desktop-nightly\boot_data_scripts and
#               c:\desktop-nightly\boot_data exist  #TODO do a mkdir
# NOTE: ASSUMES this script is in c:\desktop-nightly\boot_data_scripts #FIXME
# NOTE: USES: lionfish_core_srv_exes.txt
#
# usage: perl measure_mem.pl --int 3 --boot 57 --summary zzz --nightly $(cygpath /tmp) --name test8r

#TODO: exe list from file should be same as procs under DR, if not we have
#      more running on the machine, warn?
#TODO: print usage

use strict;
use Getopt::Long;

my $PROG = "measure_mem.pl";

my %opt;
&GetOptions(\%opt,
    "int:i",
    "times=i",
    "tillidle",
    "boot=i",
    "name=s",
    "nightly=s",
    "summary=s",
    "help",
    "usage",
    "debug",
);

$opt{'int'} = 30 if $opt{'int'} == 0;

# just paranoid about spaces
my $name            = qq($opt{'name'});
my $nightly         = qq($opt{'nightly'});
my $summary         = qq($opt{'summary'});
my $nightly_summary = qq($nightly\\$summary);

my $ROOT      = qq(C:\\desktop-nightly);
my $scriptdir = qq($ROOT\\boot_data_scripts);
my $outdir    = qq($ROOT\\boot_data);

if ($opt{'debug'}) {
    print "scrdir: $scriptdir\n";
    print "outdir: $outdir\n";
    print "boot: $opt{'boot'}\n";
    print "name: $name\n";
    print "nightly: $nightly\n";
    print "summary: $summary\n";
    print "nightly_summary: $nightly_summary\n";
}

# log file
my $stamp         = &timestamp();
my $drtop_file    = qq($outdir\\drtop_$stamp.txt);
my $drtop_summary = qq($outdir\\drtop_$stamp).qq(_summary.txt);

# exes we care about for summary
my $exes_file = qq($scriptdir\\lionfish_core_srv_exes.txt);
my @exes = &read_file($exes_file);

my $title_string = &title_string();
my $summary_string;
my $count = 0;
my $CPU_IDX = 1; #TODO: ok?

print "$PROG: measuring memorty till idle\n";

# do drview -showmem every 30s and compute summation for exes
# in lionfish configuration.  Stop when the %cpu is <= 2%
for (my $iter = 1; 1 ; $iter++) {
    my @drtop_out = `drview -listall -showmem`;

    &append_to_file($drtop_file, @drtop_out);

    &append_to_file($drtop_summary, ($title_string)) if $iter == 1;

    $summary_string =
        &compute_summary($name, $opt{'boot'}, $iter, @drtop_out);

    &append_to_file($drtop_summary, ($summary_string));

    if (defined $opt{'tillidle'}) {
        # double work
        my @sums = split /\s+/, $summary_string;

        $sums[$CPU_IDX] =~ s/%//;
        my $cpu = $sums[$CPU_IDX];

        # increment if cpu is <= 2%, otherwise reset to 0
        $count = ($cpu < 3) ? $count + 1 : $count - $count;

        # exit if cpu utilization is <= 2%, 3 times in a row
        last if ($count > 2);
    }
    else {
        last if (defined $opt{'times'} && $iter > $opt{'times'});
    }

    sleep($opt{'int'});
}

print "$PROG: copying data to $nightly\n";
# copy results to network
system("copy /y $drtop_file    $nightly");
system("copy /y $drtop_summary $nightly");

print "$PROG: Writing summary to $nightly_summary\n";
# write memory summary when machine is idle to nightly dir summary file
&append_to_file($nightly_summary, ($title_string)) unless (-e $nightly_summary);
&append_to_file($nightly_summary, ($summary_string));

#--------------------------------------------------------------------------
sub compute_summary() {
    my $name      = shift;
    my $boot      = shift;
    my $iter      = shift;
    my @drtop_out = @_;

    my @totals   = 0 x 15;
    my @cols     = ( "\n" );
    my $name_idx = 0;
    my $num_exes = 0;
    my $charge   = "0";

    my $line;
    foreach $line (@drtop_out) {
        chomp($line);

        # skip uninteresting lines
        next if $line =~ /^$/i;
        next if $line =~ /System load is/i;
        if ($line =~ s/System committed memory.*: //i) {
            $charge = $line;
            next;
        }

        # skip the title line
        next if ($line =~ m/Name.*PID.*Bld.*CPU.*WSS.*/i);

        # split each column (Priv, WSS etc.)
        @cols = split (/\s+/, $line);

        # skip the blank process
        next if $cols[$name_idx] =~ m/^\s*$/;

        # just get the name of the exe so we can grep for it
        # for e.g svchost.exe-netsvcs -> svchost
        $cols[$name_idx] =~ s/\.exe.*//i;

        # we are only interested in exes listed in the configuration file
        next unless (scalar grep(m/$cols[$name_idx]/i, @exes));

        # list the number of procs in summation
        $num_exes++;

        # add the various columns of data up
        for(my $col = 4; $col < 19; $col++) {
            $cols[$col] =~ s/%//;
            $totals[$col - 4] += $cols[$col];
        }
    }

    # return a line of summary
    return &summary_string($name, $boot, $iter, $num_exes, $charge, @totals);
}

sub title_string {
    my $title = sprintf "Build           CPU User  Thr    PVSz     VSz    PPriv    Priv    PWSS     WSS   Fault PPage  Page PNonP  NonP Commit charge                 Boot #Exes *30s\n";
    return $title;
}

sub summary_string {
    my $name     = shift;
    my $boot     = shift;
    my $iter     = shift;
    my $num_exes = shift;
    my $charge   = shift;
    my @totals   = @_;

    my $summary = sprintf(
        "%-14.14s %3d%% %3d%% %4d %7d %7d %8d %7d %7d %7d %7d %5d %5d %5d %5d %s %5d %5d %4d\n",
        $name,
        $totals[ 0],      # CPU %
        $totals[ 1],      # User %
                          # totals[2] handles, not printed
        $totals[ 3],      # Threads
        $totals[ 4],      # Peak Vsz
        $totals[ 5],      # VSz
        $totals[ 6],      # Peak Priv
        $totals[ 7],      # Priv
        $totals[ 8],      # Peak WSS
        $totals[ 9],      # WSS
        $totals[10],      # Fault
        $totals[11],      # Peak Page pool
        $totals[12],      # Page pool
        $totals[13],      # Peak Non-Paged pool
        $totals[14],      # Non-paged
        $charge,          # commit charge
        $boot,            # boot time
        $num_exes,        # number of exes in summation
        $iter,            # iter
    );

    return $summary;
}

sub append_to_file {
    my $file = shift;
    my @data = @_;

    open (FD, ">>$file") or
        die "$PROG: cannot open $file: $!";
    print FD @data;
    close FD;
}

sub read_file {
    my $file = shift;
    my @data = ();

    open (FD, "<$file") or
        die "$PROG: cannot open $file: $!";
    @data = <FD>;
    close FD;

    return @data;
}

sub timestamp() {
    # ignore wday - day of the week, $yday - day of the year, and
    # $is_dst - is daylight savings time
    my ($sec,  $min,  $hour,
        $mday, $mon,  $year,
        $wday, $yday, $is_dst) = localtime(time);

    # last 2 digits of year
    $year = ($year + 1900) % 100;
    my $stamp = sprintf("%02d%02d%02d_%02d%02d%02d",
                            $year,$mon+1,$mday, $hour,$min,$sec);
    return $stamp;
}
