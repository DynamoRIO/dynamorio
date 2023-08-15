#!/usr/bin/perl

# **********************************************************
# Copyright () 2016-2023 Google, Inc.  All rights reserved.
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
# * Neither the name of Google, Inc. nor the names of its contributors may be
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

# Build-and-test driver for automated CI.
# The CI host uses the exit code to check success, so we need a layer outside of
# ctest on runsuite.
# We stick with runsuite rather than creating a parallel scheme using
# a CI config matrix of builds.

# XXX: We currently have a patchwork of scripts and methods of passing arguments
# (some are env vars while others are command-line parameters) and thus have
# too many control points for the details of test builds and package builds.
# Maybe we can clean it up and eliminate this layer of script by moving logic
# in both directions (ci-*.yml and {runsuite,package}.cmake)?

use strict;
use Config;
use Cwd 'abs_path';
use File::Basename;
my $mydir = dirname(abs_path($0));
my $is_CI = 0;
my $is_aarchxx = $Config{archname} =~ /(aarch64)|(arm)/;
my $is_x86_64 = $Config{archname} =~ /x86_64/;
my $is_long = $ENV{'CI_TRIGGER'} eq 'push' && $ENV{'CI_BRANCH'} eq 'refs/heads/master';

# Forward args to runsuite.cmake:
my $args = '';
for (my $i = 0; $i <= $#ARGV; $i++) {
    my $arg = $ARGV[$i];
    # Backward compatibility for CI setups passing the old "travis" arg.
    $arg = 'automated_ci' if ($arg eq 'travis');
    $is_CI = 1 if ($arg eq 'automated_ci');
    if ($i == 0) {
        $args .= ",$arg";
    } else {
        # We don't use a backslash to escape ; b/c we'll quote below, and
        # the backslash is problematically converted to / by Cygwin perl.
        $args .= ";$arg";
    }
}

my $osdir = $mydir;
if ($^O eq 'cygwin') {
    # CMake is native Windows so pass it a Windows path.
    # We use the full path to cygpath as git's cygpath is earlier on
    # the PATH for AppVeyor and it fails.
    $osdir = `/usr/bin/cygpath -wi \"$mydir\"`;
    chomp $osdir;
}

# We tee to stdout to provide incremental output and avoid the 10-min
# no-output timeout on some CI (such as Travis).
# If we're on UNIX or we have a Cygwin perl, we do this via a fork.
my $res = '';
my $child = 0;
my $outfile = '';
if ($^O ne 'MSWin32') {
    print "Forking child for stdout tee\n";
    $child = open(CHILD, '-|');
    die "Failed to fork: $!" if (!defined($child));
} else {
    $outfile = "runsuite_output.txt";
}
if ($child) {
    # Parent
    # i#4126: We include extra printing to help diagnose hangs on the CI.
    if ($^O ne 'cygwin') {
        print "Parent tee-ing child stdout...\n";
        local $SIG{ALRM} = sub {
            print "\nxxxxxxxxxx 30s elapsed xxxxxxxxxxx\n";
            alarm(30);
        };
        alarm(30);
        while (<CHILD>) {
            print STDOUT $_;
            $res .= $_;
        }
    } else {
        while (<CHILD>) {
            print STDOUT $_;
            $res .= $_;
        }
    }
    close(CHILD);
} elsif ($ENV{'CI_TARGET'} eq 'package') {
    # A package build.
    my $build = "0";
    if ($ENV{'VERSION_NUMBER'} =~ /-(\d+)$/) {
        $build = $1;
    }
    if ($args eq '') {
        $args = ",";
    } else {
        $args .= ";";
    }
    $args .= "build=${build}";
    if ($ENV{'VERSION_NUMBER'} =~ /^(\d+\.\d+\.\d+)/) {
        my $version = $1;
        $args .= ";version=${version}";
    }
    if ($ENV{'DEPLOY_DOCS'} eq 'yes') {
        $args .= ";copy_docs";
    }
    # Include Dr. Memory.
    if (($is_aarchxx || $ENV{'DYNAMORIO_CROSS_AARCHXX_LINUX_ONLY'} eq 'yes') &&
        $args =~ /64_only/) {
        # Dr. Memory is not ported to AArch64 yet.
    } else {
        $args .= ";invoke=${osdir}/../drmemory/package.cmake;drmem_only";
    }
    my $cmd = "ctest -VV -S \"${osdir}/../make/package.cmake${args}\"";
    print "Running ${cmd}\n";
    if ($^O eq 'MSWin32') {
        system("${cmd} 2>&1 | tee ${outfile}");
    } else {
        system("${cmd} 2>&1");
        exit 0;
    }
} else {
    # We have no way to access the log files, so we use -VV to ensure
    # we can diagnose failures.
    my $verbose = "-VV";
    my $cmd = "ctest --output-on-failure ${verbose} -S \"${osdir}/runsuite.cmake${args}\"";
    print "Running ${cmd}\n";
    if ($^O eq 'MSWin32') {
        system("${cmd} 2>&1 | tee ${outfile}");
        print "Finished running ${cmd}\n";
    } else {
        system("${cmd} 2>&1");
        print "Finished running ${cmd}\n";
        exit 0;
    }
}

if ($^O eq 'MSWin32') {
    open my $handle, '<', "$outfile" or die "Failed to open teed ${outfile}: $!";
    $res = do {
        local $/; <$handle>
    };
}

my @lines = split('\n', $res);
my $should_print = 0;
my $exit_code = 0;
for (my $i = 0; $i <= $#lines; ++$i) {
    my $line = $lines[$i];
    my $fail = 0;
    my $name = '';
    $should_print = 1 if ($line =~ /^RESULTS/);
    if ($line =~ /^([-\w]+):.*\*\*/) {
        $name = $1;
        if ($line =~ /build errors/ ||
            $line =~ /configure errors/ ||
            $line =~ /tests failed:/) {
            $fail = 1;
        } elsif ($line =~ /(\d+) tests failed, of which (\d+)/) {
            $fail = 1 if ($2 < $1);
        }
    } elsif ($line =~ /CMake Error.*runsuite/) {
        # Try to catch things like failing to read a file (i#4312) in
        # our runsuite code, but ruling out non-suite-fatal errors like
        # missing optional cross-compilation setups.
        $fail = 1;
        $should_print = 1;
        $name = "runsuite script itself";
    } elsif ($line =~ /^\s*ERROR: diff contains/) {
        $fail = 1;
        $should_print = 1;
        $name = "diff pre-commit checks";
    }
    if ($fail && $is_CI && $line =~ /tests failed/) {
        my $is_32 = $line =~ /-32/;
        my $issue_no = "";
        my %ignore_failures_32 = ();
        my %ignore_failures_64 = ();
        if ($^O eq 'cygwin' ||
            $^O eq 'MSWin32') {
            # FIXME i#2145: ignoring certain Windows CI test failures until
            # we get all tests passing.
            %ignore_failures_32 = (
                # i#5195: These are failing on GA Server19.
                'code_api|client.drsyms-test' => 1, # i#5195
                # i#4131: These are failing on GA Server16 and need investigation.
                # Some also failed on Appveyor (i#4058).
                'code_api|win32.earlythread' => 1, # i#4131
                'code_api|client.drx-test' => 1, # i#4619
                'code_api|client.drwrap-test' => 1, # i#4131
                'code_api|client.drutil-test' => 1, # i#4131
                'code_api|tool.histogram.offline' => 1, # i#4621
                'code_api|tool.drcacheoff.burst_replace' => 1, # i#4622,i#6131
                'code_api|tool.drcacheoff.burst_traceopts' => 1, # i#4622
                'code_api|tool.drcacheoff.burst_replaceall' => 1, # i#4622
                'code_api|tool.drcacheoff.burst_static' => 1, # i#4486
                'code_api|tool.drcacheoff.gencode' => 1, # i#6131
                'code_api|api.symtest' => 1, # i#4131
                'code_api|client.drwrap-test-detach' => 1, # i#4616
                'code_api|client.cbr4' => 1, # i#4792
                'code_api|win32.hookerfirst' => 1, # i#4870
                'code_api|client.winxfer' => 1, # i#4732
                # These are from earlier runs on Appveyor:
                'code_api|security-common.retnonexisting' => 1,
                'code_api|security-win32.gbop-test' => 1, # i#2972
                'code_api|win32.reload-newaddr' => 1,
                'code_api|client.pcache-use' => 1,
                'code_api|api.detach' => 1, # i#2246
                'code_api|api.detach_spawn' => 1, # i#2611
                'code_api|api.startstop' => 1, # i#2093
                'code_api|client.drmgr-test' => 1, # i#653
                'code_api|client.nudge_test' => 1, # i#2978
                'code_api|client.nudge_ex' => 1,
                'code_api|client.alloc-noreset' => 1, # i#4436
                # These are from the long suite.
                'code_api,thread_private,tracedump_binary|common.fib' => 1, # i#1807
                'code_api,opt_speed|common.decode-stress' => 1, # i#1807
                'code_api,thread_private|common.decode-stress' => 1, # i#1807
                'code_api,thread_private,disable_traces|common.decode-stress' => 1, # i#1807
                'code_api,thread_private,tracedump_binary|common.decode-stress' => 1, # i#1807
                'code_api|client.file_io' => 1, # i#5802
                );

            %ignore_failures_64 = (
                # i#5195: These are failing on GA Server19.
                'code_api|client.drsyms-test' => 1, # i#5195
                'code_api|client.drsyms-testgcc' => 1, # i#5195
                # i#4131: These are failing on GA Server16 and need investigation.
                # Some also failed on Appveyor (i#4058).
                'code_api|client.cleancall' => 1, # i#4618
                'code_api|win32.callback' => 1, # i#4058
                'code_api|common.nativeexec' => 1, # i#4058
                'code_api|client.drx-test' => 1, # i#4619
                'code_api|client.drutil-test' => 1, # i#4131
                'code_api|client.pcache-use' => 1, # i#4058
                'code_api|api.startstop' => 1, # i#2246
                'code_api|api.detach' => 1, # i#2246
                'code_api|client.drwrap-test-detach' => 1, # i#4616
                'code_api|tool.histogram.offline' => 1, # i#4621
                'code_api|tool.drcacheoff.burst_static' => 1, # i#4486
                'code_api|tool.drcacheoff.burst_replace' => 1, # i#4486
                # i#4617: These need build-and-test to build
                # the 32-bit test app in our separate 64-bit job.
                'code_api|win32.mixedmode_late' => 1, # i#4617
                'code_api|win32.mixedmode' => 1, # i#4617
                'code_api|win32.x86_to_x64' => 1, # i#4617
                'code_api|win32.x86_to_x64_ibl_opt' => 1, # i#4617
                # These are from earlier runs on Appveyor:
                'code_api|common.floatpc_xl8all' => 1, # i#2267
                'code_api|win32.reload-newaddr' => 1,
                'code_api|client.loader' => 1,
                'code_api|client.drmgr-test' => 1, # i#1369
                'code_api|client.nudge_test' => 1, # i#2978
                'code_api|client.nudge_ex' => 1,
                'code_api|client.alloc-noreset' => 1, # i#4436
                'code_api|api.detach_spawn' => 1, # i#2611
                'code_api|api.static_noinit' => 1,
                # These are from the long suite.
                'code_api,opt_memory|common.nativeexec' => 1, # i#1807
                'code_api,opt_speed|common.decode-bad' => 1, # i#1807
                'code_api,opt_speed|common.decode-stress' => 1, # i#1807
                'code_api,opt_speed|common.nativeexec' => 1, # i#1807
                'code_api,thread_private|common.nativeexec' => 1, # i#1807
                'code_api,disable_traces|common.nativeexec' => 1, # i#1807
                'code_api,thread_private,disable_traces|common.nativeexec' => 1, # i#1807
                'code_api,loglevel|common.nativeexec' => 1, # i#1807
                'code_api,stack_size|common.nativeexec' => 1, # i#1807
                'enable_full_api|common.nativeexec' => 1, # i#1807
                'code_api,stack_size,loglevel,no_hide|common.nativeexec' => 1, # i#1807
                '|common.nativeexec' => 1, # i#1807
                'code_api,tracedump_text,tracedump_origins|common.nativeexec' => 1, # i#1807
                'code_api,tracedump_text,tracedump_origins,syntax_intel|common.nativeexec' => 1, # i#1807
                'code_api,thread_private,tracedump_binary|common.nativeexec' => 1, # i#1807
                'code_api,bbdump_tags|common.nativeexec' => 1, # i#1807
                'checklevel|common.nativeexec' => 1, # i#1807
                'finite_shared_bb_cache,cache_shared_bb_regen|common.nativeexec' => 1, # i#1807
                'finite_shared_trace_cache,cache_shared_trace_regen|common.nativeexec' => 1, # i#1807
                # We list this without any "options|" which will match all variations.
                'common.floatpc_xl8all' => 1, # i#2267
                'code_api|client.file_io' => 1, # i#5802
                );
            if ($is_long) {
                # These are important tests so we only ignore in the long suite,
                # in an attempt to still detect fails-every-time breakage until we
                # can reproduce and fix these failures while keeping merges green.
                $ignore_failures_32{'code_api|tool.drcachesim.invariants'} = 1; # i#3320
                $ignore_failures_32{'code_api|tool.drcachesim.threads-with-config-file'} = 1; # i#3320
                $ignore_failures_32{'code_api|tool.drcachesim.simple-config-file'} = 1; # i#3320
                $ignore_failures_64{'code_api|tool.drcachesim.invariants'} = 1; # i#3320
                $ignore_failures_64{'code_api|tool.drcachesim.threads-with-config-file'} = 1; # i#3320
                $ignore_failures_64{'code_api|tool.drcachesim.simple-config-file'} = 1; # i#3320
            }
            $issue_no = "#2145";
        } elsif ($is_aarchxx) {
            # FIXME i#2416: fix flaky AArch32 tests
            %ignore_failures_32 = ('code_api|tool.histogram.offline' => 1,
                                   'code_api|linux.eintr-noinline' => 1, # i#2894
                                   'code_api|pthreads.ptsig' => 1,
                                   'code_api|linux.sigaction_nosignals' => 1,
                                   'code_api|linux.signal_race' => 1,
                                   'code_api|linux.sigsuspend' => 1, # i#2898
                                   'code_api|client.drmgr-test' => 1, # i#2893
                                   'code_api|tool.drcachesim.delay-simple' => 1, # i#2892
                                   'code_api|tool.drcachesim.invariants' => 1, # i#2892
                                   'code_api|tool.drcacheoff.simple' => 1,
                                   'code_api|tool.histogram.gzip' => 1,
                                   );
            # FIXME i#2417: fix flaky/regressed AArch64 tests
            %ignore_failures_64 = ('code_api|linux.sigsuspend' => 1,
                                   'code_api|pthreads.pthreads_exit' => 1,
                                   'code_api|tool.histogram.offline' => 1, # i#3980
                                   'code_api|linux.fib-conflict' => 1,
                                   'code_api|linux.fib-conflict-early' => 1,
                                   'code_api|linux.mangle_asynch' => 1,
                                   'code_api,tracedump_text,tracedump_origins,syntax_intel|common.loglevel' => 1, # i#1807
                                   'code_api|client.attach_test' => 1, # i#5740
                                   'code_api|client.attach_blocking' => 1, # i#5740
                                   'code_api|tool.drcacheoff.rseq' => 1, # i#5734
                                   'code_api|tool.drcacheoff.windows-zlib' => 1, # i#5507
                                   );
            if ($is_32) {
                $issue_no = "#2416";
            } else {
                $issue_no = "#2417";
            }
        } elsif ($is_x86_64 && ($ENV{'DYNAMORIO_CROSS_AARCHXX_LINUX_ONLY'} eq 'yes') && $args =~ /64_only/) {
            # These AArch64 cross-compiled tests fail on x86-64 QEMU but pass
            # on native AArch64 hardware.
            $ignore_failures_64{'code_api|client.drx_buf-test'} = 1;
            $ignore_failures_64{'code_api|sample.memval_simple'} = 1;
            $ignore_failures_64{'code_api|client.drreg-test'} = 1;
            $issue_no = "#6260";
        } elsif ($^O eq 'darwin') {
            %ignore_failures_32 = ('code_api|common.decode-bad' => 1, # i#3127
                                   'code_api|linux.signal0000' => 1, # i#3127
                                   'code_api|linux.signal0010' => 1, # i#3127
                                   'code_api|linux.signal0100' => 1, # i#3127
                                   'code_api|linux.signal0110' => 1, # i#3127
                                   'code_api|linux.sigaction' => 1, # i#3127
                                   'code_api|security-common.codemod' => 1, # i#3127
                                   'code_api|client.crashmsg' => 1, # i#3127
                                   'code_api|client.exception' => 1, # i#3127
                                   'code_api|client.timer' => 1, # i#3127
                                   'code_api|sample.signal' => 1); # i#3127
        } else {
            %ignore_failures_32 = (
                'code_api|pthreads.ptsig' => 1, # i#2921
                'code_api|client.drwrap-test-detach' => 1, # i#4593
                'code_api|linux.thread-reset' => 1, # i#4604
                'code_api|linux.clone-reset' => 1, # i#4604
                # These are from the long suite.
                'common.decode-stress' => 1, # i#1807 Ignored for all options.
                'code_api,opt_speed|common.fib' => 1, # i#1807: Undiagnosed timeout.
                'prof_pcs|common.nativeexec_exe_opt' => 1, # i#2052
                'prof_pcs|common.nativeexec_retakeover_opt' => 1, # i#2052
                'prof_pcs|common.nativeexec_bindnow_opt' => 1, # i#2052
                'prof_pcs,thread_private|common.nativeexec_exe_opt' => 1, # i#2052
                'prof_pcs,thread_private|common.nativeexec_retakeover_opt' => 1, # i#2052
                'prof_pcs,thread_private|common.nativeexec_bindnow_opt' => 1, # i#2052
                );
            %ignore_failures_64 = (
                'code_api|tool.drcacheoff.burst_threadfilter' => 1, # i#2941
                # These are from the long suite.
                'code_api,opt_memory|common.loglevel' => 1, # i#1807
                'code_api,opt_speed|common.decode-stress' => 1, # i#1807
                'code_api,opt_memory|common.nativeexec_exe_opt' => 1, # i#1807
                'common.nativeexec_bindnow_opt' => 1, # i#5010 Ignored for all options.
                'common.nativeexec_retakeover_opt' => 1, # i#5010 Ignored for all options.
                'common.nativeexec_exe' => 1, # i#5010 Ignored for all options.
                'common.nativeexec_bindnow' => 1, # i#5010 Ignored for all options.
                'code_api,opt_speed|common.floatpc_xl8all' => 1, # i#1807
                'prof_pcs|common.nativeexec_exe_opt' => 1, # i#2052
                'prof_pcs|common.nativeexec_retakeover_opt' => 1, # i#2052
                'prof_pcs|common.nativeexec_bindnow_opt' => 1, # i#2052
                'prof_pcs,thread_private|common.nativeexec_exe_opt' => 1, # i#2052
                'prof_pcs,thread_private|common.nativeexec_retakeover_opt' => 1, # i#2052
                'prof_pcs,thread_private|common.nativeexec_bindnow_opt' => 1, # i#2052
                );
            if ($is_long) {
                # These are important tests so we only ignore in the long suite,
                # in an attempt to still detect fails-every-time breakage until we
                # can reproduce and fix these failures while keeping merges green.
                $ignore_failures_32{'code_api|api.startstop'} = 1; # i#4604
                $ignore_failures_64{'code_api|api.detach_state'} = 1; # i#5123
                $ignore_failures_64{'code_api|client.cleancallsig'} = 1; # i#1807
                $ignore_failures_64{'code_api|client.drx_buf-test'} = 1; # i#2657
            }
            $issue_no = "#2941";
        }

        # Read ahead to examine the test failures:
        $fail = 0;
        my $num_ignore = 0;
        for (my $j = $i+1; $j <= $#lines; ++$j) {
            my $test;
            my $test_base_name;
            if ($lines[$j] =~ /^\t(\S+)\s/) {
                $test = $1;
                # Tests listed in ignore list without any options (that is,
                # without any '|' in their name) are ignored for all possible
                # option combinations.
                $test_base_name = (split '\|', $test)[-1];
                if (($is_32 && ($ignore_failures_32{$test} ||
                                $ignore_failures_32{$test_base_name})) ||
                    (!$is_32 && ($ignore_failures_64{$test} ||
                                 $ignore_failures_64{$test_base_name}))) {
                    $lines[$j] = "\t(ignore: i" . $issue_no . ") " . $lines[$j];
                    $num_ignore++;
                } elsif ($test =~ /_FLAKY$/) {
                    # Don't count toward failure.
                } else {
                    $fail = 1;
                }
            } else {
                last if ($lines[$j] =~ /^\S/);
            }
        }
        $line =~ s/: \*/, but ignoring $num_ignore for i$issue_no: */;
    }
    if ($fail) {
        $exit_code++;
        print "\n====> FAILURE in $name <====\n";
    }
    print "$line\n" if ($should_print);
}
if (!$should_print) {
    print "Error: RESULTS line not found\n";
    $exit_code++;
}

exit $exit_code;
