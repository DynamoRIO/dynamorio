#!/usr/bin/perl -w

# **********************************************************
# Copyright (c) 2011 Google, Inc.  All rights reserved.
# Copyright (c) 2004-2008 VMware, Inc.  All rights reserved.
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

### extract-profile.pl
###
### Summarize profile info gathered from DR-based PC sampling

$verbose = 0;
$dr_profile = 0;
$dr_dll_hits = 0;
$export_addresses = 0;
$EXPORT = "exported-addresses";
$dump_addresses = 0;
$DUMP = "dumped-addresses";
$three_gigs = 0;
$summary = 0;

# addressquery.pl can handle ~150 address max so bound the
# value of export_limit;
$export_limit = 150;

$usage = "Usage: $0 [-h] [-drprofile] [-num_addrs <num>] <profile file>\n" .
    "   -summary      Produce a summary of code cache hit rate & thread count\n" .
    "   -drprofile    Writes the addresses sampled within the DR DLL to files\n" .
    "                 named $EXPORT and $DUMP.\n" .
    "                 $EXPORT is in a format suitable for the\n" .
    "                 '-f' arg of the address_query script. $DUMP is\n" .
    "                 in a format suitable for the '-address_file' arg to the\n" .
    "                 extract-samples script.\n" .
    "   -num_addrs <num>\n" .
    "                 Specify the max # of addresses to write to the\n" .
    "                 $EXPORT and $DUMP files. The current\n" .
    "                 max is $export_limit as address_query struggles with a\n" .
    "                 larger # of addresses.\n" .
    "   -3GB          Assume user address space is 3GB.\n";


while ($#ARGV >= 0) {
    if ($ARGV[0] eq "-v") {
        $verbose = 1;
    } elsif ($ARGV[0] eq "-drprofile") {
        $dr_profile = 1;
        $export_addresses = 1;
        $dump_addresses = 1;
        @dr_profile_array = ();
    } elsif ($ARGV[0] eq "-num_addrs") {
        shift;
        # addressquery.pl can handle ~150 address max so bound the
        # value of export_limit;
        $export_limit = $ARGV[0] > 150 ? 150 : $ARGV[0];
    } elsif ($ARGV[0] eq "-summary") {
        $summary = 1;
    } elsif ($ARGV[0] eq "-3GB") {
        $three_gigs = 1;
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

$marker_count = 0;
$DR_dll = "dynamorio.dll";

# This is where new entries should be added. Simply add the string that
# is printed in the log file suffixed with the '=> $marker_count++' glob.
%marker_hash =
    (
     # The IBL profile region strings need to be kept in sync with the labels
     # that are dumped in x86/arch.c/arch_process_profile().
     "trace IBL code ret" => $marker_count++,
     "trace IBL code indcall" => $marker_count++,
     "trace IBL code indjmp" => $marker_count++,
     "BB IBL code ret" => $marker_count++,
     "BB IBL code indcall" => $marker_count++,
     "BB IBL code indjmp" => $marker_count++,
     "coarse IBL code ret" => $marker_count++,
     "coarse IBL code indcall" => $marker_count++,
     "coarse IBL code indjmp" => $marker_count++,

     "cache enter/exit code" => $marker_count++,
     "fcache bb unit" => $marker_count++,
     "fcache trace unit" => $marker_count++,
     "special heap unit" => $marker_count++,
     "generated code" => $marker_count++,
     $DR_dll => $marker_count++,
     "kernel32.dll" => $marker_count++,
     "ntdll.dll" => $marker_count++,
     "global" => $marker_count++,
     );

# Init the counters
for ($i = 0; $i < $marker_count; $i++) {
    $counter_array[ $i ] = 0;
}

$reported_hits = 1;
$kernel_hits_one = 0;
$kernel_hits_two = 0;
$false_kernel_hits = 0;
$global_profile = 0;
$thread_count = 0;
$thread_with_hits = 0;
$dll_base = 0;
$delta = 0;
$dr_pref_base = hex("0x71000000");
while (<PROFILE>) {

    chop($_);

    if ($verbose) {
        print STDERR "Process $.: <$_>\n";
    }

    if (/^.*built with: -D.*-DDEBUG/) {
        $dr_pref_base = hex("0x15000000");
    }

    if (/^Profile for thread (\d+)/) {
        $thread_count++;
    }
    $found = 0;
    while (($key, $value) = each %marker_hash) {
        if ($found) {
            if ($verbose) {
                print STDERR "Tag match already found skip...\n";
            }
            next;
        }
        if (/^Dumping ${key} profile/) {

            if ($verbose) {
                print STDERR "Match for $key\n";
            }

            # Track # threads
            if (/Thread (\d+)/ || /thread (\d+)/) {
                # Have we seen this thread before?
                if (!defined($thread_hash{$1})) {
                    $thread_hash{$1} = 1;
                    $thread_with_hits++;
                }
            }

            # Read the next line
            $_ = <PROFILE>;
            if (/^(\d+)[ ]hits/) {
                $count = $1;
                if ($verbose) {
                    print STDERR "Found hit info -- $count\n";
                }
            } else {
                print "Expecting hit info, found $_\n";
                exit;
            }
            if ($key eq "global") {
                if ($verbose) {
                    print STDERR "Found global profile $_...\n";
                }
                $global_profile = 1;
                $reported_hits = $count;
                if ($verbose) {
                    print STDERR "Reported hits $reported_hits\n";
                }
                # Read the next 3 lines, which are like:
                # Profile Dump
                # Range 0x00000000-0xffffffff
                # Step 0x40000000 (0-3)
                $_ = <PROFILE>;
                $_ = <PROFILE>;
                $_ = <PROFILE>;
                # Validate the step.
                if (!(/^Step 0x40000000 \(0-3\)/)) {
                    die "Error: unexpected step in global profile\n";
                }
                # Read & process the next 4 lines, which are like:
                # 0x00000000       1821
                # 0x40000000       7570
                # 0x80000000        671
                # 0xc0000000         99
                # But may not be present if no hits!
                while ($_ !~ /^0x8/ && $_ !~ /0xc/) {
                    last if (!($_ = <PROFILE>));
                    last if (/^Finished/);
                }
                # Read the first set of possible kernel space hits
                if (!$three_gigs) {
                    if (/^0x80000000[ ]+(\d+)/) {
                        $kernel_hits_one = $1;
                    } else {
                        $kernel_hits_one = 0;
                    }
                }
                $_ = <PROFILE>;
                # Read the second set of possible kernel space hits
                if (/^0xc0000000[ ]+(\d+)/) {
                    $kernel_hits_two = $1;
                } else {
                    $kernel_hits_two = 0;
                }
            }
            else {
                $counter_array[$value] += $count;
                # Read the next 2 lines, which are like:
                # Profile Dump
                # Range 0x85331000-0x8533efff
                $_ = <PROFILE>;
                $_ = <PROFILE>;
                if (!($key =~ /dll/)) {
                    if (/^Range 0x[89abcdef]/) {
                        $false_kernel_hits += $count;
                    }
                }
                else {
                    if (/^Range 0x([\d|a-f]+)-0x([\d|a-f]+)/) {
                        $dll_base = $1;
                    }
                }
                if ($dr_profile && $key eq "dynamorio.dll") {
                    $delta = hex($dll_base) - $dr_pref_base;
                    do {
                        my $hit_addr = "";
                        $_ = <PROFILE>;
                        if (/^0x([\d|a-f]+)[ ]+(\d+)/) {
                            $hit_addr = sprintf("%x", hex($1) - $delta);
                            $dr_profile_array[$#dr_profile_array + 1] = "$hit_addr $2";
                            $dr_dll_hits += $2;
                        }
                    } while ( !(/^Finished Profile Dump/) );
                }
            }

            # We don't 'last' out of the loop because we'd have
            # to reset the hash iter using 'keys' on the next
            # key match. And keys could be a sort underneath, which
            # is costlier than continuing the loop, which is
            # linear.
            $found = 1;
        }
    }
}

if (!$global_profile) {
    print "Global profile dump not found -- no summary available\n";
    exit;
}

sub by_hits {
    ($trash, $a_hits) = split / /, $a;
    ($trash, $b_hits) = split / /, $b;
    $b_hits <=> $a_hits;
}

if ($dr_profile && $#dr_profile_array > 0) {

    @sorted_by_hits = sort by_hits @dr_profile_array;
    if ($export_addresses) {
        open(EXPORT, "> $EXPORT") or
            die "Error opening address export file\n";
    }
    if ($dump_addresses) {
        open(DUMP, "> $DUMP") or
            die "Error opening address dump file\n";
    }
    print "Writing up to $export_limit addresses to $EXPORT and $DUMP\n";
    for ($i = 0; $i <= $#sorted_by_hits; $i++) {
        if ($i == $export_limit) {
            last;
        }
        ($address, $hits) = split / /, $sorted_by_hits[$i];
        if ($export_addresses) {
            print EXPORT "$address\n";
        }
        if ($dump_addresses) {
            printf DUMP "$address %8d %4.2f\n", $hits, 100 * $hits/$dr_dll_hits;
        }
    }
    if ($export_addresses) {
        close(EXPORT);
    }
    if ($dump_addresses) {
        close(DUMP);
    }
}

if ($thread_count == 0) {
    # Must be an old core that produced the profile.
    $thread_count = $thread_with_hits;
}
if (!$summary) {
    print "Profile from file $infile ($thread_count threads total, $thread_with_hits w/profiling hits)\n\n";
}

if (!$summary) {
    printf "%35s  %10s %10s\n",
    "Segment", "Hits", "%-age:";
    printf "%35s  %10s %10s\n",
    "==========", "=======", "=====";
}
$classified_hits = 0;
$cache_hits = 0;
$DR_hits = 0;
$aggregate_reported = 0;
foreach $key (sort keys %marker_hash) {

    $value = $marker_hash{$key};

    # Bypass the global profile
    if ($key eq "global") {
        next;
    }
    # Bypass region w/no hits
    if ($counter_array[$value] == 0) {
        next;
    }
    $classified_hits += $counter_array[$value];
    $aggregate_reported += ($counter_array[$value] / $reported_hits) * 100;
    if ($summary) {
        if ($key =~ /fcache/ || $key =~ /IBL/) {
            $cache_hits += $counter_array[$value];
        } elsif ($key =~ /$DR_dll/) {
            $DR_hits = $counter_array[$value];
        }
    }
    if (!$summary) {
        printf "%35s  %10d %10.2f\n",
        $key, $counter_array[$value],
        ($counter_array[$value] / $reported_hits) * 100;
    }
}
$kernel_hits = $kernel_hits_one + $kernel_hits_two;
$unexplained_hits = $reported_hits - $classified_hits - $kernel_hits;
if (!$summary) {
    printf "%35s  %10d %10.2f\n",
    "os kernel", $kernel_hits, ($kernel_hits / $reported_hits) * 100;
    printf "%35s  %10d %10.2f\n",
    "unexplained", $unexplained_hits, ($unexplained_hits / $reported_hits) * 100;
}
$aggregate_reported += ($kernel_hits / $reported_hits) * 100;

if (!$summary) {
    printf "%35s  %10s %10s\n",
    "==========", "=======", "=====";
    printf "%35s  %10d %10.2f\n",
    "Totals", "$reported_hits", "$aggregate_reported";
} else {
    printf "SUMMARY: %s -- cache hit rate %.2f%% (hits %d), thread count %d, thread w/profiling hits %d\n",
    $infile, 100 * $cache_hits / ($cache_hits + $DR_hits), $cache_hits,
    $thread_count, $thread_with_hits;
}
