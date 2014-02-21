#!/usr/bin/perl

# **********************************************************
# Copyright (c) 2003-2010 VMware, Inc.  All rights reserved.
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

### bmtable.pl
### author: Derek Bruening   April 2001
###
### Produces a table summarizing times from one or more benchmark runs
### For times, the default is to use the median,
###   can also calculate min w/ -min parameter
### For memory, the default is to use the max,
###   can also calculate average or use first run w/ -memave or -mem1

$usage = "Usage: $0 [-v] [-k] [-c] [-min] [-mem1] [-memave] <logfile> [logfile2 ...]\n";

# constants
$debug = 0;

# parameters
$verbose = 0;
$ignore_errors = 0;
$ignore_cpu_reqt = 0;
$min = 0;
$files = 0;

# memory: use max of all runs, or:
$mem_first = 0;
$mem_ave = 0;

while ($#ARGV >= 0) {
    if ($ARGV[0] eq "-v") {
        $verbose = 1;
    } elsif ($ARGV[0] eq "-k") {
        $ignore_errors = 1;
    } elsif ($ARGV[0] eq "-c") {
        $ignore_cpu_reqt = 1;
    } elsif ($ARGV[0] eq "-mem1") {
        $mem_first = 1;
    } elsif ($ARGV[0] eq "-memave") {
        $mem_ave = 1;
    } elsif ($ARGV[0] eq "-min") {
        $min = 1;
    } else {
        $logfile[$files] = $ARGV[0];
        $files++;
    }
    shift;
}
if (!defined($logfile[0])) {
    print $usage;
    exit;
}

if ($mem_ave) {
    print "RSS and VSz are average of runs for those with multiple runs\n";
} elsif ($mem_first) {
    print "RSS and VSz are from first run for those with multiple runs\n";
} else {
    print "RSS and VSz are max of runs for those with multiple runs\n";
}

$cwd = `pwd`;
chop $cwd;
foreach $file (@logfile) {
    if (!($file =~ m+^/+ || $file =~ m+^\\+)) {
        # make it absolute
        $file = "$cwd/$file";
    }
    print "Processing $file\n";
    &process_file($file);
}

if ($verbose) {
    print "Key: - = invalid, + = valid, ** = " . ($min ? "min" : "median") . "\n";
}
print "----------------------------------------------------------------------\n";

# only single type of run!
# (this script adapted from wbmtable.pl, which has multiple types)
$run{'all'} = 'all';
foreach $r (sort (keys %run)) {
    if (defined($run{$r})) {
        &print_cur_stats($r);
    }
}

sub process_file($) {
    my ($logfile) = @_;
    open(LOG, "< $logfile") || die "Error opening $logfile\n";
    while (<LOG>) {
        chop;
        # support old logfile formats that don't print name w/ Verify
        if ($_ =~ m|^%%%% /.+/([^/]+)| ||
            $_ =~ /Verify for (\S+):/ ||
            # support for specjvm
            $_ =~ /======= (\S+) Starting/) {
            print "Name is $_\n" if ($debug);
            $bmark = $1;
            $bmark =~ s/\.exe$//;
            if (!defined($name{$bmark})) {
                $name{$bmark} = $bmark;
                $iters{$bmark} = 0;
                print "New bmark $bmark\n" if ($debug);
            }
            # have to allow two names in a row if 1st is runsuite comment,
            # and second is the Verify for the 1st iter
            if ($_ =~ m|^%%%% /.+/([^/]+)|) {
                $runsuite_name = 1;
            }
            $iter = $iters{$bmark};
            print "  New iter $iter for $bmark\n" if ($debug);
            # two differently-triggered names in a row: count as one iter
            if ($runsuite_name && $_ =~ /Verify for (\S+):/) {
                $iter--;
                print "\tDouble-dip: iter back to $iter for $bmark\n" if ($debug);
                $runsuite_name = 0;
            }
            if (!defined($time{$bmark,$iter})) {
                $iters{$bmark}++;
                $time{$bmark,$iter} = 0;
                $rss{$bmark,$iter} = 0;
                $vsz{$bmark,$iter} = 0;
                $cpu{$bmark,$iter} = 0;
                $status{$bmark,$iter} = " ?  ";
            }
        }
        if ($_ =~ /make:/ || $_ =~ /[Ee]rror:/ || $_ =~ /FAILED/) {
            $status{$bmark,$iter} = "FAIL";
        } elsif ($_ =~ /Verify.* correct/ ||
                 # support for specjvm
                 $_ =~ /======= (\S+) Finished/) {
            $status{$bmark,$iter} = " ok ";
        } elsif ($_ =~ /Elapsed: (\d+):(\d+):([\d\.]+)/) {
            # texec format
            $elapsed = $1*60 + $2 + $3/60.; # minutes
            $time{$bmark,$iter} += $elapsed;
        } elsif ($_ =~ /real\t([0-9]+)m([0-9\.]+)s/) {
            # /usr/bin/time simple format
            # FIXME: what do hours look like?
            $elapsed = $1 + $2/60.; # minutes
            $time{$bmark,$iter} += $elapsed;
            printf "\t$bmark, $iter: elapsed %5.2f @$cpu%%cpu\n", $elapsed
                if ($debug);
        } elsif ($_ =~ /(\d+):(\d+):(\d+)elapsed (\d+)%CPU/) {
            # /usr/bin/time and runstats compressed format
            $elapsed = $1*60 + $2 + $3/60.; # minutes
            $time{$bmark,$iter} += $elapsed;
            $cpu{$bmark,$iter} = $4 if ($4 > 0);
            printf "\t$bmark, $iter: elapsed %5.2f @$cpu%%cpu\n", $elapsed
                if ($debug);
        } elsif ($_ =~ /(\d+):(\d+).(\d+)elapsed (\d+)%CPU/) {
            # /usr/bin/time and runstats compressed format, with hours
            $elapsed = $1 + $2/60. + $3/6000.; # minutes
            $time{$bmark,$iter} += $elapsed;
            $try_cpu = $4;
            $cpu{$bmark,$iter} = $4 if ($4 > 0);
            printf "\t$bmark, $iter: elapsed %5.2f @$cpu%%cpu\n", $elapsed
                if ($debug);
        } elsif ($_ =~ /(\d+) tot, (\d+) RSS/) {
            # runstats memory usage
            # use max of all runs by default
            if ($mem_first) {
                $vsz{$bmark,$iter} = $1 if ($vsz{$bmark,$iter} == 0);
                $rss{$bmark,$iter} = $2 if ($rss{$bmark,$iter} == 0);
            } elsif ($mem_ave) {
                $vsz{$bmark,$iter} = ($1 + $vsz{$bmark,$iter}*$memruns{$bmark,$iter}) /
                    ($memruns{$bmark,$iter}+1);
                $rss{$bmark,$iter} = ($1 + $rss{$bmark,$iter}*$memruns{$bmark,$iter}) /
                    ($memruns{$bmark,$iter}+1);
                $memruns{$bmark,$iter}++;
            } else {
                $vsz{$bmark,$iter} = $1 if ($1 > $vsz{$bmark,$iter});
                $rss{$bmark,$iter} = $2 if ($2 > $rss{$bmark,$iter});
            }
        }
    }
    close(LOG);
}

sub print_cur_stats($) {
# run name is holdover from wbmtable.pl, so disabling this
#    my ($runname) = @_;
#    print "\nRun $runname\n";
    printf("%14s      #    %-6s  %-4s  %-9s   %-7s   %-7s\n",
           "Benchmark", "Status", "%CPU", "Time(min)", "RSS(KB)", "VSz(KB)");
    $format       = "  %2d/%-2d   %4s    %4s   %6.2f   %7d   %7d\n";
    $empty_format = "  %2d/%-2d   %4s    %4s   %6s   %7s   %7s\n";
    foreach $bm (sort (keys %name)) {
        # ignore FORTRAN90
        next if ($bm =~ /facerec/ ||
                 $bm =~ /fma3d/ ||
                 $bm =~ /galgel/ ||
                 $bm =~ /lucas/);
        # find median time, assume memory doesn't change much, just
        # use rss and vsz from median time run
        $num_ok = 0;
        $#ints = -1; # clear from last time
        for ($i=0; $i<$iters{$bm}; $i++) {
            if ($ignore_errors || &valid($status{$bm,$i}, $cpu{$bm,$i})) {
                $ints[$num_ok] = $i;
                $num_ok++;
            }
        }
        if ($num_ok == 0) {
            $median = -1;
        } else {
            @sorted = sort({$time{$bm,$a} <=> $time{$bm,$b}} @ints);
            if ($min) {
                $median = $sorted[0];
            } else {
                $median = $sorted[$num_ok/2]; # if even number, take smaller!
            }
        }
        if ($verbose) {
            for ($i=0; $i<$iters{$bm}; $i++) {
                if ($i == $median) {
                    $str = "%14s**";
                } elsif (&valid($status{$bm,$i}, $cpu{$bm,$i})) {
                    $str = "%14s+ ";
                } else {
                    $str = "%14s- ";
                }
                printf($str.$format,
                       $bm, $i+1, $iters{$bm},
                       $status{$bm,$i}, &cpustr($cpu{$bm,$i}),
                       $time{$bm,$i}, $rss{$bm,$i}, $vsz{$bm,$i});
            }
        } else {
            if ($median == -1) {
                printf("%14s  ".$empty_format,
                       $bm, 0, $iters{$bm}, $status{$bm,0}, " -- ",
                       "----", "----", "----");
            } else {
                if ($num_ok == $iters{$bm}) {
                    $stat = $status{$bm,$median};
                } else {
                    $stat = "<ok>";
                }
                printf("%14s  ".$format,
                       $bm, $num_ok, $iters{$bm},
                       $stat, &cpustr($cpu{$bm,$median}),
                       $time{$bm,$median}, $rss{$bm,$median},
                       $vsz{$bm,$median});
            }
        }
    }
}


sub cpustr($) {
    my ($cpu) = @_;
    if ($cpu == 0) {
        return " -- ";
    } elsif ($cpu < 99) {
        return sprintf "*%02d*", $cpu;
    } else {
        return sprintf "%3d ", $cpu;
    }
}

sub valid($, $) {
    my ($status, $cpu) = @_;
    return ($status eq " ok " &&
            # passed as param -> defined to 0, so check for 0
            ($ignore_cpu_reqt || !defined($cpu) || $cpu == 0 || $cpu >= 99));
}
