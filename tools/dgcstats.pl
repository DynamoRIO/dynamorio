#!/usr/bin/perl

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

### dgcstats.pl
### author: Derek Bruening   August 2003
###
### Analyzes DGC_DIAGNOSTICS results -- point it at a log directory

$usage = "Usage: $0 [root]\n";

$verbose = 0;

$dir = ".";
if ($#ARGV >= 0) {
    $dir = $ARGV[0];
}

chdir $dir;
$main = `ls *[Ee][Xx][Ee].[0-9]*`;
chop $main;
print "Reading $main\n";
open(MAIN, "< $main") || die "Error: Couldn't open $main for input\n";
while (<MAIN>) {
    next unless /^DYNGEN/;
    if (/=> (0x[0-9a-fA-F]+)-(0x[0-9a-fA-F]+) (.*)$/) {
        $start = hex($1);
        $end = hex($2);
        $comment = $3;
        if (!defined($dg_start{$start})) {
            $dg_start{$start} = $start;
            $dg_end{$start} = $end;
            $dg_comment{$start} = $comment;
        }
    }
}
close(MAIN);

$find = "find . -name log.\\*";
open(FIND, "$find |") || die "Error running $find\n";
while (<FIND>) {
    chop;
    print "Reading $_\n";
    open(LOG, "< $_") || die "Error: Couldn't open $_ for input\n";
    while (<LOG>) {
        if ($in_alloc) {
            # old log files: /^Currently in (0x[0-9a-fA-F]+) (.*)/
            if (/current pc = (0x[0-9a-fA-F]+)/) {
                # we could always append and for multiple allocations to
                # same address get an accumulated callstack...but we don't
                $prot_stack{$addr} = "\t$1";
            # old log files: /frame ptr.*ret = (0x[0-9a-fA-F]+) (.*)/
            } elsif (/frame ptr.*ret = (0x[0-9a-fA-F]+)/) {
                $prot_stack{$addr} .= "\t$1";
            } elsif (/\s+(\[.*\])$/) {
                $prot_stack{$addr} .= " $1\n";
            } elsif (!/^Call stack/) {
                $in_alloc = 0;
            }
        }
        # FIXME: log.* order could matter...but don't have synch info so who cares
        # old log files: /NtAllocateVirtualMemory base=(0x[0-9a-fA-F]+) size=(0x[0-9a-fA-F]+)/
        if (/NtAllocateVirtualMemory.*@(0x[0-9a-fA-F]+) sz=(0x[0-9a-fA-F]+)/) {
            $addr = hex($1);
            $end = $addr + hex($2);
            $prot_start{$addr} = $addr;
            $prot_end{$addr} = $end;
            $prot_line{$addr} = $_;
            $in_alloc = $addr;
        }
        # FIXME: try to watch NtFree as well?  don't have synch info though
        if (/^Entry into dyngen F[0-9]+\((0x[0-9a-fA-F]+)(.*)\) via: +(0x[0-9a-fA-F]+).*  ([<A-Za-z]:?([->!,+a-zA-Z~_\\0-9\.]| {1})+)/) {
            $dst = hex($1) & 0xfffff000;
            $src = hex($3) & 0xfffff000;
            $desc = $2; # not used since we search the .exe. file
            $dll = $4;
            $found = 0;
            for ($i=0; $i<$num_entries{$dst}; $i++) {
                if ($src == $entry_list{$dst}[$i]) {
                    $entry_count{$dst}[$i]++;
                    $found = 1;
                    break;
                }
            }
            if (!$found) {
                $entry_list{$dst}[$num_entries{$dst}] = $src;
                $entry_dll{$dst}[$num_entries{$dst}] = $dll;
                $entry_count{$dst}[$num_entries{$dst}] = 1;
                if ($entry_desc{$dst} eq "") {
                    foreach $s (sort (keys %dg_start)) {
                        if ($dst >= $s && $dst < $dg_end{$s}) {
                            $entry_desc{$dst} = $dg_comment{$s};
                            break;
                        }
                    }
                }
                $num_entries{$dst}++;
            }
            if ($verbose && $dst != $src) {
                printf "0x%08x => 0x%08x $3\n", $src, $dst;
            }
        }
        elsif (/^Exit from dyngen F[0-9]+\((0x[0-9a-fA-F]+).*targeting +(0x[0-9a-fA-F]+) ([<A-Za-z]:?([->!,+a-zA-Z_~\\0-9\.]| {1}[A-Za-z0-9])+)/) {
            $dst = hex($1) & 0xfffff000;
            $src = hex($2) & 0xfffff000;
            $dll = $3;
            $found = 0;
            for ($i=0; $i<$num_exits{$dst}; $i++) {
                if ($src == $exit_list{$dst}[$i]) {
                    $exit_count{$dst}[$i]++;
                    $found = 1;
                    break;
                }
            }
            if (!$found) {
                $exit_list{$dst}[$num_exits{$dst}] = $src;
                $exit_dll{$dst}[$num_exits{$dst}] = $dll;
                $exit_count{$dst}[$num_exits{$dst}] = 1;
                $num_exits{$dst}++;
            }
            if ($verbose && $dst != $src) {
                printf "0x%08x => 0x%08x $dll\n", $src, $dst;
            }
        }
        # old log files: /region (0x[0-9a-fA-F]+)-(0x[0-9a-fA-F]+) written @(0x[0-9a-fA-F]+)/
        elsif (/Exec ([0-9a-fA-F]+)-([0-9a-fA-F]+) written @(0x[0-9a-fA-F]+)/) {
            $at = hex($3);
            foreach $s (sort (keys %dg_start)) {
                if ($at >= $s && $at < $dg_end{$s}) {
                    $entry_write{$s}++;
                    break;
                }
            }
        }
        elsif (/^Write target is actually inside app bb @(0x[0-9a-fA-F]+)/) {
            $at = hex($3);
            foreach $s (sort (keys %dg_start)) {
                if ($at >= $s && $at < $dg_end{$s}) {
                    $entry_write_frag{$s}++;
                    break;
                }
            }
        }
    }
    close(LOG);
}
close(FIND);

foreach $dst (sort (keys %entry_list)) {
    printf "0x%08x %s\n", $dst, $entry_desc{$dst};
    if ($entry_write{$dst} > 0) {
        if (!defined($entry_write_frag{$dst})) {
            $entry_write_frag{$dst} = 0;
        }
        print "  written $entry_write{$dst} times, fragments $entry_write_frag{$dst} times\n";
    }
    foreach $addr (sort (keys %prot_start)) {
        if ($dst >= $addr && $dst < $prot_end{$addr}) {
            print "  " . $prot_line{$addr};
            print $prot_stack{$addr};
        }
    }

    # hack for sorting: sort list of ints as nice indirection
    undef @sortme;
    for ($i=0; $i<$num_entries{$dst}; $i++) {
        $sortme[$i] = $i;
    }
    @sortme = sort { $entry_list{$dst}[$a] <=> $entry_list{$dst}[$b] } @sortme;
    printf "  entries:\n";
    for ($i=0; $i<$num_entries{$dst}; $i++) {
        printf("\t%2d: #%4d  0x%08x $entry_dll{$dst}[$sortme[$i]]\n", $i,
               $entry_count{$dst}[$sortme[$i]], $entry_list{$dst}[$sortme[$i]]);
    }

    if (defined($num_exits{$dst})) {
        printf "  exits:\n";
        # hack for sorting: sort list of ints as nice indirection
        undef @sortme;
        for ($i=0; $i<$num_exits{$dst}; $i++) {
            $sortme[$i] = $i;
        }
        @sortme = sort { $exit_list{$dst}[$a] <=> $exit_list{$dst}[$b] } @sortme;
        for ($i=0; $i<$num_exits{$dst}; $i++) {
            printf("\t%2d: #%4d  0x%08x $exit_dll{$dst}[$sortme[$i]]\n", $i,
                   $exit_count{$dst}[$sortme[$i]], $exit_list{$dst}[$sortme[$i]]);
        }
    }
}
foreach $dst (sort (keys %exit_list)) {
    if (!defined($num_entries{$dst})) {
        # hack for sorting: sort list of ints as nice indirection
        undef @sortme;
        for ($i=0; $i<$num_exits{$dst}; $i++) {
            $sortme[$i] = $i;
        }
        @sortme = sort { $exit_list{$dst}[$a] <=> $exit_list{$dst}[$b] } @sortme;
        printf "0x%08x exits:\n", $dst;
        for ($i=0; $i<$num_exits{$dst}; $i++) {
            printf("\t%2d: #%4d  0x%08x $exit_dll{$dst}[$sortme[$i]]\n", $i,
                   $exit_count{$dst}[$sortme[$i]], $exit_list{$dst}[$sortme[$i]]);
        }
    }
}

