#!/usr/bin/perl

# **********************************************************
# Copyright (c) 2004-2006 VMware, Inc.  All rights reserved.
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

### tracestats.pl
### formerly called Frags, with fewer options
### author: Derek Bruening   February 2001
###
### Analyzes DynamoRIO trace output: sorts traces based
### on counts or time spent in them (from DynamoRIO profiling info)
###
### -brief: lists summary table instead of full info for each trace
### -stats: lists completion stats instead of full info
### -prefix_stats: lists counts of prefixes that need to restore eflags
### -links: will print % of time each exit from a trace was used
###   (only with Dynamo's -prof_counts)
### -groups: attempts to group traces based on their links
### -exe exename: will look up line numbers in original
###   code that traces came from
###   (only with Dynamo's -dump_trace_origins flag)
###
### Several scalars below could overflow a 32-bit int, I assume
### perl always uses floats internally and won't overflow

# FIXME: some of these are mutually exclusive, indicate which
$usage = "Usage: $0 [-brief] [-stats] [-prefix_stats] [-links] [-groups]
\t[-sideline] [-exe exename] [-bbpcs] [-pcs pcfile] <tracefile>

-stats and -bbpcs: work best with -tracedump_origins output in the tracefile
-links and -groups: only work with -prof_counts output in the tracefile
";

# defaults
$have_exe = 0;
$src = 0;
$brief = 0;
$links = 0;
$groups = 0;
$pcs = 0;
$linux_pcs = 0;

# win32 pcs only
$win32_pcs_step = 0;
$overcount_instrs = 0;
$overcount_samples = 0;

$stats = 0;
$prefix_stats = 0;
$infile = "";

# get optional params
while ($#ARGV >= 0) {
    if ($ARGV[0] eq '-exe') {
        if ($#ARGV <= 0) { print $usage; exit; }
        shift;
        $exe = $ARGV[0];
        unless (-f "$exe" && -x "$exe" ) {
            die "$err No executable $exe\n";
        }
        $have_exe = 1;
        $src = 1; # src is not a separate option anymore
    } elsif ($ARGV[0] eq '-in') {
        if ($#ARGV <= 0) { print $usage; exit; }
        shift;
        $infile = $ARGV[0];
    } elsif ($ARGV[0] eq '-pcs') {
        if ($#ARGV <= 0) { print $usage; exit; }
        shift;
        $pcs = 1;
        $pcfile = $ARGV[0];
    } elsif ($ARGV[0] eq '-stats') {
        $stats = 1;
    } elsif ($ARGV[0] eq '-prefix_stats') {
        # study of static and dynamic count of prefixes that need to restore eflags
        $prefix_stats = 1;
        $prefix_noeflags = 0;
        $prefix_eflags = 0;
        $prefix_noeflags_count = 0;
        $prefix_eflags_count = 0;
    } elsif ($ARGV[0] eq '-brief') {
        $brief = 1;
    } elsif ($ARGV[0] eq '-sideline') {
        $brief = 1;
        $sideline = 1;
    } elsif ($ARGV[0] eq '-links') {
        $brief = 1;
        $links = 1;
    } elsif ($ARGV[0] eq '-groups') {
        $groups = 1;
        $brief = 1;
        $links = 1;
    } elsif ($ARGV[0] eq '-bbpcs') {
        $pcs_by_orig_bb = 1;
    } elsif ($ARGV[0] =~ /^-/) {
        print $usage;
        exit;
    } else {
        $infile = $ARGV[0];
        last;
    }
    shift;
}

die $usage if ($infile eq "");

if ($src && !$have_exe) {
    print "Error: must specify executable name with -exe when using -src\n";
    exit 0;
}

if ($pcs) {
    # read in all pcs into a table
    open(PCS, "< $pcfile") || die "Error: Couldn't open $pcfile for input\n";
    # Linux format:
    #    ITIMER distribution (74):
    #       74.3% of time in INTERPRETER (55)
    #    ...
    #
    #    PC PROFILING RESULTS
    #    pc=0xf6e8c129   #=1     in DynamoRIO dispatch
    #    pc=0x179d102c   #=1     in trace #   251 @0x08048470 w/ offs 0x00000028
    #    pc=0x002c1192   #=1     in DynamoRIO interpreter
    #    ...
    # Windows format:
    #    ...
    #    Dumping dynamorio.dll profile
    #    127 hits out of 596, 21.30%
    #    ...
    #    Dumping global profile
    #    596 hits
    #    ...
    #    Dumping fcache trace unit profile (Shared)
    #    134 hits
    #    Profile Dump
    #    Range 0x182f1000-0x182fefff
    #    Step 0x00000008 (0-7167)
    #    0x182f1c78          1
    #    0x182f1d60          1
    #    0x182f1d78          1
    #    ...
    #    Finished Profile Dump
    my $in_fcache_pcs = 0;
    while (<PCS>) {
        chop;
        # handle DOS end-of-line:
        if ($_ =~ /
$/) { chop; };
        if ($_ =~ /ITIMER distribution/) {
            $linux_pcs = 1;
        }
        if ($_ =~ /ITIMER distribution \(([0-9]+)\)/ ||
            $_ =~ /hits out of ([0-9]+)/) {
            $total_samples = $1;
        }
        $in_fcache_pcs = 1 if (/trace unit/);
        next unless ((!$linux_pcs && $in_fcache_pcs) || ($linux_pcs && $_ =~ /^pc/));
        $l = $_;
        if ($linux_pcs) {
            # only care about traces and fragments
            # can be multiple entries for a single pc: must key by pc AND tag
            if ($l =~ /trace/ || $l =~ /fragment/) {
                if ($l =~ /trace \#/ || $l =~ /fragment \#/) {
                    $l =~ /^pc=([x0-9a-f]+)[^\#]+\#=([0-9]+)[^\#]+\# *([0-9]+) @([x0-9a-f]+)/;
                    # $1 = pc, $2 = count, $3 = id, $4 = tag
                    $pc_samples{$4, $1} = $2;
                } else {
                    # release run: no trace id!
                    $l =~ /^pc=([x0-9a-f]+)[^\#]+\#=([0-9]+)[^@]+@([x0-9a-f]+)/;
                    # $1 = pc, $2 = count, $3 = tag
                    $pc_samples{$3, $1} = $2;
                }
            }
        } else {
            if (/Finished Profile Dump/) {
                $in_fcache_pcs = 0;
            } elsif (/Step 0x0*([1-9]+)/) {
                # bucket size
                $win32_pcs_step = hex($1);
            } else {
                next unless ($l =~ /^0x/);
                $l =~ /([x0-9a-f]+)\s+([0-9]+)/;
                # FIXME: no tag info available so will double-count for traces
                # that re-use same fcache slots
                $pc_samples{$1} = $2;
                $pc_sample_taken{$1} = 0;
            }
        }
    }
    close(PCS);
}

if ($src) {
    # get info on shared libraries
    $shared_num = 0;
    open(LDD, "ldd $exe |") || die "Error: Couldn't run ldd $exe\n";
    while (<LDD>) {
        chop;
        $l = $_;
        if ($l =~ /^\s*([\/a-zA-Z_\.0-9-]+) => .* \((0x[0-9A-Fa-f]+)\)/) {
            $shared_name[$shared_num] = $1;
            $shared_start[$shared_num] = $2;
            $shared_sort[$shared_num] = $shared_num;
            $shared_num++;
        }
    }
    close(LDD);

    # now sort by starting address
    @shared_sort = sort({hex($shared_start[$b]) <=> hex($shared_start[$a])}
                        @shared_sort);
    print "Shared library assumptions:\n";
    for ($i=$shared_num-1; $i>=0; $i--) {
        $addr1 = $shared_start[$shared_sort[$i]];
        if ($i == 0) {
            $addr2 = "above";
        } else {
            $addr2 = sprintf("0x%08x",
                             hex($shared_start[$shared_sort[$i-1]]) - 1);
        }
        print "\t$addr1 .. $addr2 == $shared_name[$shared_sort[$i]]\n";
    }
    print "\n";
}

# read in and store all lines from file
$in_trace = 0;
$in_stub_summary = 0;
$line = 0;
$tnum = 0;
$linked_to = 0;
$have_times = 0;
$max_stub = 0;
$stub_64bit = 0;
$body_start = 0;
$direct_total = 0;
$indirect_total = 0;
$exitstub_boundary = -1;
$flushed = 0;
$stub_num = 0;
$in_original = 0;
$unseen_count = 0;

# for pc samples
$stubs_start = 0;
$stub_samples = 0;
$ind_samples = 0;
$in_cmp = 0;
$samples = 0;
$dcontext_ecx = 0;
$ret_samples = 0;
$indjc_samples = 0; # indirect jumps & calls
$branch_samples = 0;

open(IN, "< $infile") || die "Error: Couldn't open $infile for input\n";
while (<IN>) {
    chop;
    # handle DOS end-of-line:
    if ($_ =~ /
$/) { chop; };
    $l = $_;
    if ($l =~ /^TRACE \# ([0-9]+)/) {
        # start of trace
        $in_trace = 1;
        # in case no Fragment# data
        $id[$tnum] = $1;
        if ($pcs && $pcs_by_orig_bb) {
            # only keep src bb tag info for the current trace
            $cur_bbnum = 0;
            $cur_stubnum = 0;
        }
        if ($prefix_stats) {
            $trace_total_exec = 0;
        }
    }
    if ($in_trace) {
        if ($l =~ /^Fragment \# ([0-9]+)/) {
            # get fragment id
            $id[$tnum] = $1;
            # now initialize everything using fragment id:
            # for reverse-lookup
            $tnum_from_id{$1} = $tnum;
            $tag[$tnum] = -1;
            $size[$tnum] = -1;
            if ($pcs) {
                $time[$tnum] = 0;
            } else {
                $time[$tnum] = -1;
            }
            $count[$tnum] = -1;
            $sideline_pre[$tnum] = -1;
            $sideline_during[$tnum] = -1;
            $sideline_post[$tnum] = -1;
            # trace completion stats
            $bbs[$tnum] = 0;
            $bbs_noelide[$tnum] = 0;
            $ind_branches[$tnum] = 0;
            $bb_instrs[$tnum] = 0;
            $bb_bytes[$tnum] = 0;
            $end_in_cbr[$tnum] = 0;
            $completion_all[$tnum] = 0;
            $completion_half[$tnum] = 0;
            $last_instr = "<not init>";
        }
        if (!$in_original && $l =~ /^ORIGINAL CODE/) {
            $in_original = 1;
        }
        if ($in_original) {
            if ($l =~ /^END ORIGINAL CODE/) {
                $in_original = 0;
                if ($pcs && $pcs_by_orig_bb) {
                    # now count over again via exit jmps from trace
                    $cur_bbnum = 0;
                }
                if ($stats || ($pcs && $pcs_by_orig_bb)) {
                    # does trace end with conditional branch?
                    if ($last_instr =~ /  0x[0-9a-fA-F]+   7/ ||        # jcc short
                        $last_instr =~ /  0x[0-9a-fA-F]+   0[fF] 8/ ||  # jcc
                        $last_instr =~ /  0x[0-9a-fA-F]+   e[0123]/) {  # loop*, jecxz
                        $end_in_cbr[$tnum] = 1;
                    }
                    #printf("Frag \# %2d: ends in %s => %d cbr\n",
                    #      $id[$tnum], $last_instr, $end_in_cbr[$tnum]);
                }
            }
            if ($pcs && $pcs_by_orig_bb) {
                if ($l =~ /^basic block.*start_pc = (0x[0-9a-fA-F]+)/ ||
                    # tracedump.exe output: arbitrarily different, should fix
                    $l =~ /^Basic block.*tag (0x[0-9a-fA-F]+)/) {
                    $bbtag[$cur_bbnum++] = $1;
                }
                # handle elision
                # easy for tracedump_text:
                #    if ($l =~ /continuing in .* at (0x[0-9a-fA-F]+)/) {
                # but rather than doing that and having to distinguish text
                # and binary we calculate ourselves from call and jmp instrs:
                if ($l =~ /  0x([0-9a-fA-F]+)\s+(e8|e9) (([0-9a-fA-F]+ ){4})\s*(call|jmp)/) {
                    my $instraddr = $1;
                    my $disp = $3;
                    $disp =~ /(\w\w)\s+(\w\w)\s+(\w\w)\s+(\w\w)/;
                    my $first = $4;
                    $disp = hex("$4$3$2$1");
                    # perl doesn't consider leading 1 to be negative
                    if ($first =~ /^[89a-fA-F]/) {
                        $disp -= 0xffffffff;
                    }
                    my $target = hex($instraddr) + 5 # len of instr
                        + $disp;
                    $target = sprintf("0x%08x", $target);
                    # no exit for it so instead of making a new bb
                    # just list it with the original tag
                    $bbtag[$cur_bbnum-1] .= ",$target";
                }
                if ($l =~ /^  0x[0-9a-fA-F]+/) {
                    $last_instr = $l;
                }
            }
            if ($stats) {
                if ($l =~ /^basic block \# ([0-9]+)/ ||
                    # tracedump.exe output: arbitrarily different, should fix
                    $l =~ /^Basic block ([0-9]+):/) {
                    $bbs[$tnum] = $1 + 1;
                    $bbs_noelide[$tnum]++;
                } elsif ($l =~ /continuing/) {
                    $bbs_noelide[$tnum]++;
                } elsif ($l =~ /^  0x[0-9a-fA-F]+   (([0-9a-fA-F][0-9a-fA-F] )+)/) {
                    $bb_instrs[$tnum]++;
                    $bb_bytes[$tnum] += (length($1)/3);
                    $last_instr = $l;
                } elsif ($l =~ /^              (( [0-9a-fA-F][0-9a-fA-F])+)/) {
                    $bb_bytes[$tnum] += (length($1)/3);
                }
            }
        }
        # even if brief, if pcs, we need to traverse
        if (!$brief || $pcs) {
            if ($prefix_stats && !$in_original && $l =~ /prefix entry/) {
                # was prefix simply restore of eax?
                if ($trace{$tnum,$line-2} =~ /indirect branch target entry/) {
                    $prefix_noeflags++;
                    $prefix_noeflags_count += $trace_total_exec;
                } else {
                    $prefix_eflags++;
                    $prefix_eflags_count += $trace_total_exec;
                }
            }
            if (!$in_original && ($pcs || $stats) && $dcontext_ecx == 0 &&
                $l =~ /normal entry/) {
                # get dcontext addr by examining prefix
                # final instruction is always a restore to ecx
                $trace{$tnum,$line-1} =~ /mov\s+(0x[0-9a-fA-F]*) -> \%ecx/;
                $dcontext_ecx = hex($1); # 4 = ecx_offs - ebx_offs
            }
            if (!$in_original && ($pcs || $stats) && $l =~ /^  (0x[0-9a-fA-F]+)/) {
                $addr = $1; # keep in string form
                if ($linux_pcs) {
                    $samples = $pc_samples{$tag[$tnum], $addr};
                } else {
                    $addrnum = $addr;
                    $addrnum =~ s/0x//;
                    $addrnum = hex($addrnum);
                    # Round it down to bucket size
                    $addrnum = $addrnum & (~($win32_pcs_step - 1));
                    # Now back to hex
                    $addrnum = sprintf("0x%08x", $addrnum);
                    # FIMXE: no tag so will dup in every trace that occupied that spot!
                    $samples = $pc_samples{$addrnum};
                    $pc_sample_taken{$addrnum}++;
                    if ($pc_sample_taken{$addrnum} > 1) {
                        $overcount_instrs++;
                        $overcount_samples += $samples;
                        # FIXME: rather than attributing samples to EVERY instr
                        # that rounds into this bucket (and thus have summary stats
                        # massively over-count), we instead assign samples to the
                        # first instr in the bucket only, potentially missing
                        # samples that actually happened on DR-overhead instrs.
                        $samples = 0;
                    }
                    $time[$tnum] += $samples;
                    if ($pcs && $pcs_by_orig_bb) {
                        if ($stubs_start) {
                            $stubsamples[$cur_stubnum] += $samples;
                        } else {
                            $bbsamples[$cur_bbnum] += $samples;
                        }
                    }
                }
                if ($l =~ / j[a-z]+\s*\$0x/ && !$stubs_start && !$in_cmp) {
                    # direct branch
                    $branch_samples += $samples;
                }
                # look for ind br overhead by watching for "save ecx"
                if (is_save_ecx($l)) {
                    $in_cmp = 1;
                }
                if ($in_cmp) {
                    # assume ind br is a return if "save ecx" followed by a pop
                    if ($in_cmp == 2) {
                        if ($stats) {
                            $ind_branches[$tnum]++;
                        }
                        if ($pcs && $pcs_by_orig_bb) {
                            $bbib[$cur_bbnum] = 1;
                        }
                        if ($l =~ /pop /) {
                            $ret_samples += $samples;
                        } else {
                            $indjc_samples += $samples;
                        }
                    }
                    $in_cmp++;
                    # we count from save ecx through either restore ecx
                    # (inlined) or jmp to exit stub, we do include jmp
                    $ind_samples += $samples;
                    if ($l =~ /jmp.*exit stub/) {
                        $in_cmp = 0;
                    }
                    if (is_restore_ecx($l)) {
                        $in_cmp = 0;
                    }
                }
                if ($stubs_start) {
                    $stub_samples += $samples;
                }
                if (!$brief) {
                    $trace{$tnum,$line++} = sprintf("%3d%s", $samples, $l);
                }
                if ($pcs && $pcs_by_orig_bb) {
                    if (!$stubs_start && is_exit_cti($l)) {
                        $cur_bbnum++;
                    }
                    # $stubs_start case is handled below where we look for "exit stub
                    # n", as looking at jmps is not enough for -prof_counts
                }
            } else {
                if (!$brief) {
                    $trace{$tnum,$line++} = $l;
                }
            }
            if (!$in_original && $pcs && $pcs_by_orig_bb && $stubs_start &&
                $l =~ /-- exit stub /) {
                # $stubs_start is set below, which is what we want: don't inc until #1
                $cur_stubnum++;
            }
        }
        if ($in_original) {
            # no more processing
            next;
        }
        if ($in_stub_summary) {
            if ($l =~ /^\s*\#/) {
                if ($l =~ /\#([0-9]+): target = 0x([0-9a-fA-F]+) \(F\#([0-9-]+)\), count = ([0-9]+)(.*)/) {
                    $stub_target{$tnum,$num_stubs[$tnum]} = $3;
                    $cnt = $4;
                } elsif ($l =~ /\#([0-9]+): target = 0x([0-9a-fA-F]+).*count = ([0-9]+)(.*)/) {
                    $stub_target{$tnum,$num_stubs[$tnum]} = 0;
                    $cnt = $3;
                }
                # count total direct & indirect
                if ($prefix_stats) {
                    $trace_total_exec += $cnt;
                }
                if ($2 eq "0") {
                    $indirect_total += $cnt;
                } else {
                    $direct_total += $cnt;
                }
                $linked = $5;
                if ($linked ne "") {
                    if (!($linked =~ /linked/)) {
                        die "Weird exit stub string: $linked\n";
                    }
                    $stub_linked{$tnum,$num_stubs[$tnum]} = 1;
                } else {
                    $stub_linked{$tnum,$num_stubs[$tnum]} = 0;
                }
                # old-style (before %U): 0 prior to real number
                if ($cnt =~ /^0([0-9]+)/) {
                    $cnt = $1;
                }
                if ($cnt > hex("0xffffffff")) {
                    $stub_64bit = 1;
                }

                if ($cnt > $max_stub) {
                    $max_stub = $cnt;
                }
                $stub_count{$tnum,$num_stubs[$tnum]} = $cnt;
                # set to -1 so can tell if gets set later
                $stub_eflags{$tnum,$num_stubs[$tnum]} = -1;
                $num_stubs[$tnum]++;
            } else {
                $in_stub_summary = 0;
            }
        }
        if ($have_stubs) {
            # find out if direct linked exit stub saves eflags or not
            if ($exitstub_boundary > -1) {
                if ($stub_linked{$tnum,$exitstub_boundary} &&
                    $stub_target{$tnum,$exitstub_boundary} > -1) {
                    # this is first instr of stub
                    if ($l =~ /mov    \%eax -> /) {
                        # saves eflags
                        $stub_eflags{$tnum,$exitstub_boundary} = 1;
                    } else {
                        # does not save eflags
                        $stub_eflags{$tnum,$exitstub_boundary} = 0;
                    }
                }
                $exitstub_boundary = -1;
            } elsif ($l =~ /-- exit stub ([0-9]+)/) {
                $exitstub_boundary = $1;
            }
        }
        if ($linked_to) {
            if ($l =~ /^\s/) {
                $l =~ /^\sFragment \# ([0-9]+)/;
                if ($1 eq $id[$tnum]) {
                    $trace{$tnum,$line++} = "\t  ==> Self-loop";
                }
            } else {
                $linked_to = 0;
            }
        }
        if ($body_start && $l =~ /^  (0x[0-9a-fA-F]+)/) {
            $start_addr = hex($1);
            $body_start = 0;
        }
        if ($l =~ /^Tag = (0x[0-9a-fA-F]+)/) {
            $tag[$tnum] = $1;
        } elsif ($l =~ /normal entry/) {
            $body_start = 1;
        } elsif ($l =~ /Flushed/) {
            $flushed++;
        } elsif ($l =~ /^Size = ([0-9]+)/) {
            $size[$tnum] = $1;
        } elsif ($l =~ /pre-opt count = ([0-9]+)/) {
            $sideline_pre[$tnum] = $1;
        } elsif ($l =~ /post-opt count = ([0-9]+)/) {
            $sideline_during[$tnum] = $1;
        } elsif ($l =~ /new trace count = ([0-9]+)/) {
            $sideline_post[$tnum] = $1;
        } elsif ($l =~ /^\stime/) {
            $have_times = 1;
            $l =~ /[^0-9]([0-9.]+)/;
            $time[$tnum] = $1;
            if ($count[$tnum] == 0) {
                $per = 0.;
            } else {
                $per = 1000.0 * ($time[$tnum] / $count[$tnum]);
            }
            $trace{$tnum,$line++} = "\t  => us per execution: $per";
        } elsif ($l =~ /^\scount/) {
            $l =~ /[^0-9]([0-9]+)/;
            $count[$tnum] = $1;
        } elsif ($l =~/^Exit stubs/) {
            $have_stubs = 1;
            $in_stub_summary = 1;
            $num_stubs[$tnum] = 0;
        } elsif ($l =~/^Linked to/) {
            $linked_to = 1;
        } elsif ($l =~ /^  (0x[0-9a-fA-F]+)\s.+</ ||
                 # workaround for bug where <exit stub N> is not printed:
                 (!$stubs_start && $l =~ /^  (0x[0-9a-fA-F]+)\s.+j.+/)) {
            # measure # of instr bytes from top until this exit from trace
            # we assume all entires were to normal entry, not ibt entry
            # we also do not count the # bytes of the exit instr itself
            # neither of these assumptions is that egregious -- we're only
            # using this estimate as a relative weight among different exits!
            $addr = hex($1);
            $stub_size{$tnum,$stub_num} = ($addr - $start_addr);
            $stub_num++;
        } elsif ($l =~ /-- exit stub 0/) {
            $stubs_start = 1;
        } elsif ($l =~ /^END TRACE/) {
            # end of trace
            $trace{$tnum,$line++} = "Total samples: $time[$tnum]";
            if ($pcs && $pcs_by_orig_bb) {
                $cur_stubnum++;
                $trace{$tnum,$line++} = "BB samples breakdown:";
                die "Error: trace $tnum tag $tag[$tnum]: $cur_stubnum stubs != $cur_bbnum bbs in trace $tnum"
                    if ($cur_stubnum != $cur_bbnum);
                if ($end_in_cbr[$tnum]) {
                    # last jmp is fall-through, count as part of penultimate
                    $cur_bbnum--;
                    $bbsamples[$cur_bbnum-1] += $bbsamples[$cur_bbnum];
                    $stubsamples[$cur_bbnum-1] += $stubsamples[$cur_bbnum];
                }
                for ($i=0; $i<$cur_bbnum; $i++) {
                    $trace{$tnum,$line++} =
                        sprintf("\tbb# %2d %-20s = %4d samples, %4d in %8s exit => %4d total",
                                $i, $bbtag[$i], $bbsamples[$i], $stubsamples[$i],
                                $bbib[$i] ? "indirect" : "direct",
                                $bbsamples[$i] + $stubsamples[$i]);
                    $bbtag[$i] = "";
                    $bbib[$i] = 0;
                    $bbsamples[$i] = 0;
                    $stubsamples[$i] = 0;
                }
            }
            $sortme[$tnum] = $tnum;
            $length[$tnum++] = $line;
            $in_trace = 0;
            $line = 0;
            $stub_num = 0;
            $stubs_start = 0;
        }
    } else { # !in_trace
        if ($l =~ /^Traces below dump threshold of (\d+): (\d+)/) {
            $unseen_threshold = $1;
            $unseen_num = $2;
        } elsif ($l =~ /^Total count below dump threshold: (\d+)/) {
            $unseen_count = $1;
        }
    }
}
close(IN);

if ($prefix_stats) {
    printf("Prefixes that restore eflags vs. those that do not:\n");
    printf("Static restore:  %12d, do not: %12d, => %5.2f%% restore\n",
           $prefix_eflags, $prefix_noeflags,
           $prefix_eflags*100/($prefix_eflags+$prefix_noeflags));
    printf("Dynamic restore: %12.0f, do not: %12.0f, => %5.2f%% restore\n",
           $prefix_eflags_count, $prefix_noeflags_count,
           $prefix_eflags_count*100/($prefix_eflags_count+$prefix_noeflags_count));
    exit 0;
}

print "Total traces dumped: $tnum\n";
print "\tTraces prematurely kicked out of cache: $flushed\n";
print "Total traces not dumped: $unseen_num\n";
$total_traces = $tnum + $unseen_num;
print "=> Total traces: $total_traces\n\n";

if ($pcs) {
    print "Total PC samples: $total_samples\n";
    if (!$linux_pcs) {
        print "\tOverlapping instrs: $overcount_instrs\n";
        print "\tOverlapping samples: $overcount_samples\n";
    }
    print "PC samples inside exit stubs: $stub_samples\n";
    print "PC samples in indirect branch overhead: $ind_samples\n";
    $tot = $stub_samples + $ind_samples;
    $tot_frac = $tot / $total_samples * 100.;
    printf("Total of previous 2 lines = %d = %3.1f%% of total samples\n",
           $tot, $tot_frac);
    print "PC samples on return instructions: $ret_samples\n";
    print "PC samples on other indirect branches: $indjc_samples\n";
    $tot = $ret_samples + $indjc_samples;
    if ($tot == 0) {
        $tot_frac = 0;
    } else {
        $tot_frac = $ret_samples / $tot * 100.;
    }
    printf("=> %% indirect branches that are returns: %3.1f%%\n", $tot_frac);
    print "PC samples on direct branches: $branch_samples\n";
    if ($branch_samples + $tot == 0) {
        $tot_frac = 0;
    } else {
        $tot_frac = $tot / ($branch_samples + $tot) * 100.;
    }
    printf("=> %% branches that are indirect: %3.1f%%\n", $tot_frac);
    print "\n";
}

# if have both stubs and times, sort by stubs
if ($have_stubs) {
    if (!$groups) {
        print "Max stub count is $max_stub\n";
        if ($stub_64bit) {
            print "Needed 64-bit stub counters!\n";
        }
        $grand_total = $direct_total + $indirect_total;
        print "Indirect=$indirect_total, direct=$direct_total, total=$grand_total\n";
        $direct_frac = 100. * ($direct_total / $grand_total);
        $indirect_frac = 100. * ($indirect_total / $grand_total);
        printf("\t=> %4.1f%% direct, %4.1f%% indirect\n",
               $direct_frac, $indirect_frac);

        if ($unseen_count > 0) {
            $grand_grand_total = $grand_total + $unseen_count;
            print "\nUnseen traces below $unseen_threshold threshold: $unseen_num traces\n";
            $unseen_cnt_frac = 100. * $unseen_count / $grand_grand_total;
            printf("\ttotal count=$unseen_count (%4.1f%%) => grand total=$grand_grand_total\n\n",
            $unseen_cnt_frac);
        }

        # eflags saving stats
        $eflags_save_count = 0;
        $eflags_save_stubs = 0;
        $eflags_nosave_count = 0;
        $eflags_nosave_stubs = 0;
        for ($t=0; $t<$tnum; $t++) {
            for ($e=0; $e<$num_stubs[$t]; $e++) {
                # only look at linked direct branches
                if ($stub_linked{$t,$e} && $stub_target{$t,$e} > -1) {
                    if ($stub_eflags{$t,$e} == -1) {
                        print "Error: eflags not set for trace $id[$t] exit $e\n";
                    } elsif ($stub_eflags{$t,$e}) {
                        $eflags_save_stubs++;
                        $eflags_save_count += $stub_count{$t,$e};
                    } else {
                        $eflags_nosave_stubs++;
                        $eflags_nosave_count += $stub_count{$t,$e};
                    }
                }
            }
        }
        if ($eflags_save_stubs + $eflags_nosave_stubs > 0) {
            $eflags_stub_frac = 100. * $eflags_save_stubs /
                ($eflags_save_stubs + $eflags_nosave_stubs);
            print "Eflags stubs: save=$eflags_save_stubs";
            printf(" (%4.1f%%), nosave=$eflags_nosave_stubs\n",
                   $eflags_stub_frac);
            $eflags_count_frac = 100. * $eflags_save_count /
                ($eflags_save_count + $eflags_nosave_count);
            print "Eflags counts: save=$eflags_save_count";
            printf(" (%4.1f%%), nosave=$eflags_nosave_count\n",
                   $eflags_count_frac);
        } else {
            print "Error obtaining eflags stubs stats\n";
        }

        print "\n";
    }
    print "----------------------------------------------\n\n";
    # estimate times
    $total_estimate = 0;
    if ($unseen_count > 0) {
        # FIXME: can we do better than this average number of instrs
        # in a trace, *3/4 since half exit at halfway point?
        # ave # instrs across spec & win32 is 29
        $unseen_size = 29 * 0.75;
        $unseen_estimate = $unseen_count / 1000000. * $unseen_size;
        $total_estimate += $unseen_estimate;
    }
    for ($t=0; $t<$tnum; $t++) {
        # estimate time spent inside
        $estimate[$t] = 0.;
        $entry_count[$t] = 0.;
        for ($e=0; $e<$num_stubs[$t]; $e++) {
            # scale down to avoid overflow and for easier visual comparisons
            $cnt = $stub_count{$t,$e} / 1000000.;
            if (! defined($stub_size{$t,$e})) {
                die "Error: size of trace $id[$t]'s stub $e not defined\n";
            }
            $entry_count[$t] += $cnt;
            $estimate[$t] += $cnt * $stub_size{$t,$e};
        }
        $total_estimate += $estimate[$t];
    }
    for ($t=0; $t<$tnum; $t++) {
        $estimate_frac[$t] = 100. * $estimate[$t] / $total_estimate;
    }
    if ($unseen_count > 0) {
        $unseen_frac = 100. * $unseen_estimate / $total_estimate;
        print "Unseen traces, time based only on general averages:\n";
        printf("%6d below %8d threshold traces: count %7.2f M, time %9.2f (%4.1f%%)\n\n",
               $unseen_num, $unseen_threshold,
               $unseen_count/1000000., $unseen_estimate, $unseen_frac);
        print "Dumped traces:\n";
    }
    @sortme = sort({$estimate[$a] <=> $estimate[$b]} @sortme);
}
elsif ($have_times) {
    @sortme = sort({$time[$a] <=> $time[$b]} @sortme);

    # get total time
    $total_time = 0;
    for ($t=0; $t<$tnum; $t++) {
        $total_time += $time[$t];
    }
}
elsif ($pcs) {
    @sortme = sort({$time[$a] <=> $time[$b]} @sortme);
}
else {
    # leave in original order == reverse order
    @sortme = sort({$b <=> $a} @sortme);
}

# now output in sorted order
if ($stats) {
    $max_bbs = 0;
    $ave_bbs = 0;
    $max_bbs_noelide = 0;
    $ave_bbs_noelide = 0;
    $max_ibs = 0;
    $ave_ibs = 0;
    $max_ins = 0;
    $ave_ins = 0;
    $max_bytes = 0;
    $ave_bytes = 0;
    $ave_comp_all = 0;
    $ave_comp_half = 0;
    $max10_bbs = 0;
    $ave10_bbs = 0;
    $max10_bbs_noelide = 0;
    $ave10_bbs_noelide = 0;
    $max10_ibs = 0;
    $ave10_ibs = 0;
    $max10_ins = 0;
    $ave10_ins = 0;
    $max10_bytes = 0;
    $ave10_bytes = 0;
    $ave10_comp_all = 0;
    $ave10_comp_half = 0;
    $coverage5 = 0;
    $coverage10 = 0;
    $coverage50 = 0;
}
for ($t=$tnum-1; $t>=0; $t--) {
    $n = $sortme[$t];

    if (($brief && !$pcs_by_orig_bb) || $stats) {
        if (!$groups && !$stats) {
            # if have both stubs and times, just use stubs
            if ($have_stubs) {
                $s = sprintf(", count %7.2f M, time %9.2f (%4.1f%%)",
                             $entry_count[$n], $estimate[$n], $estimate_frac[$n]);
                $frac = $estimate_frac[$n];
            } elsif ($have_times) {
                $frac = 100 * $time[$n] / $total_time;
                my $cnt = $count[$n] / 1000000;
                $s = sprintf(", count %7.2f M, time %9.2f ms (%4.1f%%)",
                             $cnt, $time[$n], $frac);
            } elsif ($pcs) {
                $frac = 100 * $time[$n] / $total_samples;
                $s = sprintf(", samples %8d (%4.1f%%)",
                             $time[$n], $frac);
            } else {
                $s = "";
            }
            if ($sideline) {
                if ($sideline_pre[$n] == -1) {
                    printf("Frag \# %5d ($tag[$n]) (%4.1f%%) = not sideline-optimized\n",
                           $id[$n], $frac);
                } else {
                    $post_frac = 100. * $sideline_post[$n] /
                        ($sideline_pre[$n] + $sideline_during[$n] +
                         $sideline_post[$n]);
                    printf("Frag \# %5d ($tag[$n]) (%4.1f%%) = pre %6d, dur %6d, post %8d (%5.1f%%)\n",
                           $id[$n], $frac, $sideline_pre[$n], $sideline_during[$n],
                           $sideline_post[$n], $post_frac);
                }
            } else {
                printf("Frag \# %5d ($tag[$n]) = size %5d$s\n",
                       $id[$n], $size[$n]);
            }
        }

        if (($links || $stats) && $have_stubs) {
            $link_total = 0;
            for ($e=0; $e<$num_stubs[$n]; $e++) {
                $link_total += $stub_count{$n,$e};
            }
            if ($link_total == 0) {
                for ($e=0; $e<$num_stubs[$n]; $e++) {
                    $stub_percent{$n,$e} = 0;
                }
                next;
            }
            if ($stats) {
                # trying to get % of time completed trace (went all the way to
                # end bb) and % of time got halfway through trace
                if ($end_in_cbr[$n]) {
                    # for purposes of completion, final 2 exits are identical
                    # (just different directions of final cbr)
                    $end_stub = $num_stubs[$n] - 2; # index of penultimate
                } else {
                    $end_stub = $num_stubs[$n] - 1; # index of final
                }
            }
            for ($e=0; $e<$num_stubs[$n]; $e++) {
                $stub_frac = 100. * ($stub_count{$n,$e} / $link_total);
                if (!$groups && !$stats) {
                    if ($stub_frac > 1.0) {
                        printf("\tStub \#%2d: %4.1f%% => %s\n",
                               $e, $stub_frac, $stub_target{$n,$e});
                    }
                }
                # store for use in finding groups
                $stub_percent{$n,$e} = $stub_frac;
                if ($stats && $e < $end_stub/2) {
                    # add up through halfway point
                    $completion_half[$n] += $stub_frac;
                }
            }
            # store for use in finding groups
            $stub_total[$n] = $link_total;
            if ($stats) {
                $completion_all[$n] = $stub_percent{$n,$num_stubs[$n]-1};
                $completion_half[$n] = 100. - $completion_half[$n];
            }
        }
        next if (!$stats);
    }

    if ($stats) {
        printf("Frag \# %5d = %2d/%2d bbs, %3d ibs, %5d ins, %5d bytes; %5.1f%% all, %5.1f%% half\n",

               $id[$n], $bbs[$n], $bbs_noelide[$n], $ind_branches[$n], $bb_instrs[$n], $bb_bytes[$n],
               $completion_all[$n], $completion_half[$n]);
        if ($stats) {
            $max_bbs = $bbs[$n] if ($bbs[$n] > $max_bbs);
            $ave_bbs += $bbs[$n];
            $max_bbs_noelide = $bbs_noelide[$n] if ($bbs_noelide[$n] > $max_bbs_noelide);
            $ave_bbs_noelide += $bbs_noelide[$n];
            $max_ibs = $ind_branches[$n] if ($ind_branches[$n] > $max_ibs);
            $ave_ibs += $ind_branches[$n];
            $max_ins = $bb_instrs[$n] if ($bb_instrs[$n] > $max_ins);
            $ave_ins += $bb_instrs[$n];
            $max_bytes = $bb_bytes[$n] if ($bb_bytes[$n] > $max_bytes);
            $ave_bytes += $bb_bytes[$n];
            $ave_comp_all += $completion_all[$n];
            $ave_comp_half += $completion_half[$n];
            if ($t >= $tnum-10) {
                # stats for top 10 traces
                $max10_bbs = $bbs[$n] if ($bbs[$n] > $max10_bbs);
                $ave10_bbs += $bbs[$n];
                $max10_bbs_noelide = $bbs_noelide[$n] if ($bbs_noelide[$n] > $max10_bbs_noelide);
                $ave10_bbs_noelide += $bbs_noelide[$n];
                $max10_ibs = $ind_branches[$n] if ($ind_branches[$n] > $max10_ibs);
                $ave10_ibs += $ind_branches[$n];
                $max10_ins = $bb_instrs[$n] if ($bb_instrs[$n] > $max10_ins);
                $ave10_ins += $bb_instrs[$n];
                $max10_bytes = $bb_bytes[$n] if ($bb_bytes[$n] > $max10_bytes);
                $ave10_bytes += $bb_bytes[$n];
                $ave10_comp_all += $completion_all[$n];
                $ave10_comp_half += $completion_half[$n];
                $coverage10 += $estimate_frac[$n];
            }
            if ($t >= $tnum-5) {
                # stats for top 5 traces
                $coverage5 += $estimate_frac[$n];
            }
            if ($t >= $tnum-50) {
                # stats for top 50 traces
                $coverage50 += $estimate_frac[$n];
            }
        }
        next;
    }

    print "===========================================================================\n\n";
    $in_origins = 0;
    $last_srcline = "";
    for ($o=0; $o<$length[$n]; $o++) {
        $l = $trace{$n,$o};

        if ($in_origins) {
            if ($src && $l =~ /^\s*(0x[0-9A-Fa-f]+)/) {
                $addr = $1;
                $instr = substr($l, 36);
                $srcline = lookup_line($exe, $addr);
                if ($srcline =~ /\?\?\(\) @ \?\?:0/) {
                    $i = 0;
                    while ($i < $shared_num) {
                        if (hex($addr) >= $shared_start[$i] &&
                            ($i == $shared_num-1 ||
                             hex($addr) < $shared_start[$i+1])) {
                            $srcline = "[somewhere in $shared_name[$i]]";
                        }
                        $i++;
                    }
                    if ($srcline eq $last_srcline) {
                        $srcline = "";
                    } else {
                        $last_srcline = $srcline;
                    }
                }
                ## get actual code
                if ($srcline =~ /@ ([\w.]+):(\d+)/) {
                    $file = $1;
                    $lineno = $2;
                    if ($srcline eq $last_srcline) {
                        $srcline = "";
                    } else {
                        $last_srcline = $srcline;
                        $actual = `sed -n '$lineno p' $file`;
                        chomp $actual;
                        ## strip leading WS: ($actual) = ($actual =~ /\s*(.*)$/);
                        $srcline = sprintf("[%s]%s", $srcline, $actual);
                    }
                }
                ## print sprintf("%-35s %s\n", $instr, $srcline);
                if ($srcline ne "") {
                    print "$srcline\n";
                }
            }
            if ($l =~ /^END ORIGINAL CODE/) {
                $in_origins = 0;
            }
        }
        print "$l\n";
        if ($have_times && $o==1) {
            $frac = 100 * $time[$n] / $total_time;
            printf("Time Recorded: %9.2f ms (%4.1f%%)\n", $time[$n], $frac);
        }
        if ($have_stubs && $o==1) {
            printf("Time Estimate: %9.2f (%4.1f%%)\n",
                   $estimate[$n], $estimate_frac[$n]);
            for ($e=0; $e<$num_stubs[$n]; $e++) {
                print "\tStub \#$e: size $stub_size{$n,$e}\n";
            }
        }
        if ($l =~ /^ORIGINAL CODE/ && $have_exe) {
            $in_origins = 1;
        }
    }
    print "\n";
}

if ($stats) {
    printf("Max all      = %3d bbs, %3d ibs, %5d ins, %5d bytes\n",
           $max_bbs, $max_ibs, $max_ins, $max_bytes);
    printf("Max top 10   = %3d bbs, %3d ibs, %5d ins, %5d bytes\n",
           $max10_bbs, $max10_ibs, $max10_ins, $max10_bytes);
    printf("Ave all    = %4.1f bbs, %4.1f ibs, %5d ins, %5d bytes; %5.1f%% all, %5.1f%% half\n",
           $ave_bbs/$tnum, $ave_ibs/$tnum, $ave_ins/$tnum, $ave_bytes/$tnum,
           $ave_comp_all/$tnum, $ave_comp_half/$tnum);
    printf("Ave top 10 = %4.1f bbs, %4.1f ibs, %5d ins, %5d bytes; %5.1f%% all, %5.1f%% half\n",
           $ave10_bbs/10, $ave10_ibs/10, $ave10_ins/10, $ave10_bytes/10,
           $ave10_comp_all/10, $ave10_comp_half/10);
    printf("No elide bbs: all: %3d max, %4.1f ave; top10: %3d max, %4.1f ave\n",
           $max_bbs_noelide, $ave_bbs_noelide/$tnum, $max10_bbs_noelide, $ave10_bbs_noelide/10);
    printf("Coverage: top 5 = %5.1f%%, top 10 = %5.1f%%, top 50 = %5.1f%%\n",
           $coverage5, $coverage10, $coverage50);
    exit;
}

# Find groups
# My method: go through traces in sorted order
# For each trace: recursively walk exits w/ exit freq >= 10%
#   If hit an already-examined trace, stop
#   Add est time % to "group %" unless trace is already part of another group
#   (this way total group %'s will add to 100, and by going through in
#    sorted order this shouldn't cause us to miss larger groups in
#    favor of smaller)
if ($groups && $have_stubs) {
    @todo = {};
    @tabs = {};
    @frac = {};
    for ($t=$0; $t<$tnum; $t++) {
        $n = $sortme[$t];
        push(@todo, $n);
        push(@tabs, 0);
        push(@frac, -1);
        $group_done[$t] = 0;
    }
    $gnum = 0;
    $group_percent[0] = 0;
    $group_out[0] = "";
    $in_group = 0;
    while ($#todo > 0) {
        $t = pop(@todo);
        $indent = pop(@tabs);
        $wt = pop(@frac);
        if ($indent == 0) {
            if ($in_group) {
                $group_out[$gnum] .= sprintf("\tTotal for group = %4.1f%%\n\n",
                                             $group_percent[$gnum]);
                $sort_group[$gnum] = $gnum;
                $gnum++;
                $group_percent[$gnum] = 0;
                for ($n=$0; $n<$tnum; $n++) {
                    $group_done[$n] = 0;
                }
            }
            if ($done[$t]) {
                $in_group = 0;
                next;
            } else {
                $in_group = 1;
                $group_top[$gnum] = $t;
            }
        }
        # yes group_done is not needed, I'm leaving it in so I can go
        # back to "double counting" that may find bigger/better groups
        # (but I doubt it)
        if ($in_group && $t != -1 && !$done[$t] && !$group_done[$t]) {
            $group_percent[$gnum] += $estimate_frac[$t];
            $group_done[$t] = 1;
        }
        for ($i=0; $i<$indent; $i++) {
            $group_out[$gnum] .= "\t";
        }
        if ($t == -1) {
            $name = sprintf("%-6s", "*");
        } else {
            $name = sprintf("%-6d", $id[$t]);
        }
        $group_out[$gnum] .= "$name";
        if ($indent > 0) {
            $group_out[$gnum] .= sprintf("(%4.1f%%)", $wt);
        }
        if ($t == -1) {
            $group_out[$gnum] .= "\n";
            next;
        }
        if ($done[$t]) {
            $out = sprintf("  [%9.2f (%4.1f%%)] +",
                           $estimate[$t], $estimate_frac[$t]);
            if ($group_num[$t] != $gnum) {
                if ($t == $group_top[$group_num[$t]]) {
                    $out .= " ==> top of group $group_num[$t]";
                } else {
                    $out .= " ==> middle of group $group_num[$t]";
                }
            } else {
                $out .= " ==> loop";
            }
        } else {
            $out = sprintf("  [%9.2f (%4.1f%%)]",
                           $estimate[$t], $estimate_frac[$t]);
            if ($t > -1) {
                # gather exits above the cutoff to be sorted
                $num_sort_stub = 0;
                undef @sort_stub;
                for ($e=0; $e<$num_stubs[$t]; $e++) {
                    # cutoff is 10%!
                    if ($stub_percent{$t,$e} > 9.999) {
                        $sort_stub[$num_sort_stub++] = $e;
                    }
                }
                @sort_stub = sort( { $stub_percent{$t,$a} <=> $stub_percent{$t,$b} }
                                   @sort_stub );
                # now push exits, in reverse sorted order so do highest first
                for ($i=0; $i<$num_sort_stub; $i++) {
                    $e = $sort_stub[$i];
                    if ($stub_target{$t,$e} == -1) {
                        $n = -1;
                    } else {
                        $n = $tnum_from_id{$stub_target{$t,$e}};
                    }
                    push(@todo, $n);
                    push(@tabs, ($indent+1));
                    push(@frac, $stub_percent{$t,$e});
                }
            }
            $done[$t] = 1;
            $group_num[$t] = $gnum;
        }
        $group_out[$gnum] .= "$out\n";
    }
    @sort_group = sort({$group_percent[$a] <=> $group_percent[$b]} @sort_group);
    for ($g=$gnum-1; $g>=0; $g--) {
        $gp = $sort_group[$g];
        print "Group $gp:\n$group_out[$gp]";
    }
}


sub compare_numbers() {
    $a <=> $b;
# $a <=> $b is equivalent to:
# if ($a<$b){return -1;} elsif ($a>$b){return 1;} else{return 0;}
}

sub lookup_line($, $) {
    my ($exe, $addr) = @_;
    my ($i, $func, $loc);
    open(FOO, "addr2line -e $exe -f $addr |") ||
        die "Error running addr2line\n";
    $i = 0;
    while (<FOO>) {
        chop;
        if ($i == 0) {
            $func = $_;
        } else {
            $loc = $_;
        }
        $i++;
    }
    close(FOO);
    $loc =~ s/\/.+\///;
    $res = "$func() @ $loc";
    return $res;
}

sub is_save_ecx($) {
    my ($l) = @_;
    # save ecx can either be a move to dcontext as absolute
    # address, or a move to a tls slot in fs:
    return (($l =~ /mov\s+\%ecx -> (0x[0-9a-fA-F]*)/ &&
             hex($1) == $dcontext_ecx) ||
            ($l =~ /mov\s+\%ecx -> \%fs:0x[0-9a-fA-F]*/));
}

sub is_restore_ecx($) {
    my ($l) = @_;
    # save ecx can either be a move to dcontext as absolute
    # address, or a move to a tls slot in fs:
    return (($l =~ /mov\s+(0x[0-9a-fA-F]*) -> \%ecx/ &&
             hex($1) == $dcontext_ecx) ||
            ($l =~ /mov\s+\%fs:0x[0-9a-fA-F]* -> \%ecx/));
}

sub is_exit_cti($) {
    my ($l) = @_;
    # 32-bit jmps only to exclude jmp part of jecxz mangle
    return ($l =~ /(e9|(0f 8[0-9a-fA-F])) ([0-9a-fA-F]+ ){4}\s*j/);
}

### OBSOLETE CODE
### This code ran objdump, read in resulting assembly code, and then
### used it to print out disassembly of trace:
###
###   if ($have_exe && $dis) {
###       # disassemble exe, read in assembly code
###       system("objdump -d $exe > $exe.dis");
###       system("echo \"\" >> $exe.dis"); # blank line to get end of last func!
###       $i = 0;
###       $in_func = 0;
###       open(OBJ, "< $exe.dis") || die "Error: Couldn't open $exe.dis for input\n";
###       while (<OBJ>) {
###     chop;
###     $l = $_;
###     if ($in_func) {
###         if ($l =~ /^\s*([0-9A-Fa-f]+)/) {
###             $addr{$faddr,$i} = hex($1);
###             $instr{$faddr,$i} = $l;
###             $num_instrs{$faddr}++;
###             $i++;
###         } else {
###             $last{$faddr} = $addr{$faddr,($i-1)};
###             $in_func = 0;
###             $i = 0;
###             # print "Function $func{$faddr} = $first{$faddr}...$last{$faddr}\n";
###         }
###     }
###     if ($l =~ /^([0-9A-Fa-f]+) <([^>]+)>/) {
###         # can have multiple func names (if static) so key off of
###         #   function address, not func name!
###         $faddr = hex($1);
###         $func{$faddr} = $2;
###         $first{$faddr} = $faddr;
###         $in_func = 1;
###     }
###       }
###       close(OBJ);
###       system("rm -f $exe.dis");
###   }
###
###             $l =~ /^\s(0x[0-9A-Fa-f]+)\.\.\.(0x[0-9A-Fa-f]+)/;
###             $start = $1;
###             $end = $2;
###             if ($src && 0) {
###                 $start_line = lookup_line($exe, $start);
###                 $end_line = lookup_line($exe, $end);
###                 print "\t  == $start_line ... $end_line\n";
###             }
###             if ($dis) {
###                 $start =~ s/0x//;
###                 $end =~ s/0x//;
###                 $found = lookup_dis($start, $end);
###                 if (!$found) {
###                     print "Couldn't find $start...$end in $exe\n";
###                 }
###                 print "\n";
###             }
###
###   sub lookup_dis($, $) {
###       my ($start, $end) = @_;
###       my ($f, $i);
###       $num_start = hex($start);
###       $num_end = hex($end);
###       # start & end guaranteed to be in same function
###       # just do linear search, performance not an issue
###       foreach $f (keys %first) {
###     if ($num_start >= $first{$f} &&
###         $num_end <= $last{$f}) {
###         print "$start...$end is in $func{$f}:\n";
###         $between = 0;
###         for ($i=0; $i<$num_instrs{$f}; $i++) {
###             if ($num_start == $addr{$f,$i}) {
###                 $between = 1;
###             }
###             # end addr is past the last addr
###             if ($num_end == $addr{$f,$i}) {
###                 $between = 0;
###             }
###             if ($between) {
###                 print "$instr{$f,$i}\n";
###             }
###         }
###         return 1;
###     }
###       }
###       return 0;
###   }
###
### END OBSOLETE CODE

