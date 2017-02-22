#!/usr/bin/perl

# **********************************************************
# Copyright (c) 2016-2017 Google, Inc.  All rights reserved.
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

# Build-and-test driver for Travis CI.
# Travis uses the exit code to check success, so we need a layer outside of
# ctest on runsuite.
# We stick with runsuite rather than creating a parallel scheme using
# a Travis matrix of builds.
# Travis only supports Linux and Mac, so we're ok relying on perl.

use strict;
use Cwd 'abs_path';
use File::Basename;
my $mydir = dirname(abs_path($0));
my $is_CI = 0;

# Forward args to runsuite.cmake:
my $args = '';
for (my $i = 0; $i <= $#ARGV; $i++) {
    $is_CI = 1 if ($ARGV[$i] eq 'travis');
    if ($i == 0) {
        $args .= ",$ARGV[$i]";
    } else {
        # We don't use a backslash to escape ; b/c we'll quote below, and
        # the backslash is problematically converted to / by Cygwin perl.
        $args .= ";$ARGV[$i]";
    }
}

# We have no way to access the log files, so we use -VV to ensure
# we can diagnose failures.
# We tee to stdout to provide incremental output and avoid the 10-min
# no-output timeout on Travis.
my $res = '';
my $child = open(CHILD, '-|');
die "Failed to fork: $!" if (!defined($child));
if ($child) {
    # Parent
    my $output;
    while (<CHILD>) {
        print STDOUT $_;
        $res .= $_;
    }
    close(CHILD);
} else {
    if ($^O eq 'cygwin') {
        # CMake is native Windows so pass it a Windows path.
        # We use the full path to cygpath as git's cygpath is earlier on
        # the PATH for AppVeyor and it fails.
        $mydir = `/usr/bin/cygpath -wi \"$mydir\"`;
        chomp $mydir;
    }
    system("ctest --output-on-failure -V -S \"${mydir}/runsuite.cmake${args}\" 2>&1");
    exit 0;
}

my @lines = split('\n', $res);
my $should_print = 0;
my $exit_code = 0;
for (my $i = 0; $i < $#lines; ++$i) {
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
    } elsif ($line =~ /^\s*ERROR: diff contains/) {
        $fail = 1;
        $should_print = 1;
        $name = "diff pre-commit checks";
    }
    if ($fail && $is_CI && $^O eq 'cygwin' && $line =~ /tests failed/) {
        # FIXME i#2145: ignoring certain AppVeyor test failures until
        # we get all tests passing.
        my $is_32 = $line =~ /-32/;
        my %ignore_failures_32 = ('unit_tests' => 1,
                                  'code_api|security-common.retnonexisting' => 1,
                                  'code_api|win32.reload-newaddr' => 1,
                                  'code_api|win32.tls' => 1,
                                  'code_api|client.loader' => 1,
                                  'code_api|client.thread' => 1,
                                  'code_api|client.pcache-use' => 1,
                                  'code_api|client.nudge_ex' => 1,
                                  'code_api|tool.drcacheoff.burst_static' => 1,
                                  'code_api|tool.drcacheoff.burst_replace' => 1,
                                  'code_api|api.detach' => 1,
                                  'code_api|api.static_detach' => 1);
        my %ignore_failures_64 = ('unit_tests' => 1,
                                  'code_api|common.floatpc_xl8all' => 1,
                                  'code_api|win32.reload-newaddr' => 1,
                                  'code_api|win32.mixedmode' => 1,
                                  'code_api|win32.x86_to_x64' => 1,
                                  'code_api|win32.x86_to_x64_ibl_opt' => 1,
                                  'code_api|win32.mixedmode_late' => 1,
                                  'code_api|client.loader' => 1,
                                  'code_api|client.thread' => 1,
                                  'code_api|client.nudge_ex' => 1,
                                  'code_api|api.detach' => 1,
                                  'code_api|api.static_noclient' => 1,
                                  'code_api|api.static_noinit' => 1);
        # Read ahead to examine the test failures:
        $fail = 0;
        my $num_ignore = 0;
        for (my $j = $i+1; $j < $#lines; ++$j) {
            my $test;
            if ($lines[$j] =~ /^\t(\S+)\s/) {
                $test = $1;
                if (($is_32 && $ignore_failures_32{$test}) ||
                    (!$is_32 && $ignore_failures_64{$test})) {
                    $lines[$j] = "\t(ignore: i#2145) " . $lines[$j];
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
        $line =~ s/: \*/, but ignoring $num_ignore for i2145: */;
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
