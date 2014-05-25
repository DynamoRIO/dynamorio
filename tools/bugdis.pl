#!/usr/bin/perl

# **********************************************************
# Copyright (c) 2006-2007 VMware, Inc.  All rights reserved.
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

### bugdis.pl
### Derek Bruening, December 2006
###
### disassembles code from forensic files
###
### note about -i interactive use feature:
###   cdb within rxvt has some problems: interactive typing loses the last
###   char or more, so put spaces after everything.
###   for interactive should launch ntsd, or run cdb from within a cygwin-in-cmd shell

# default debugger paths to try
# we first try cdb from Debugging Tools For Windows
#   (so we can get stdout and don't need a temp logfile)
# else we use the ntsd on everyone's machines and a logfile,
#   which we clobber every time
$sysroot = &win32path2unix($ENV{'SYSTEMROOT'});
$progroot = &win32path2unix($ENV{'PROGRAMFILES'});
# try to find installation of debugging tools for windows
@debugtools = glob("$progroot/Debug*/cdb.exe");
@try_dbgs = ("$debugtools[0]", "$sysroot/system32/ntsd.exe");
$app = "cmd"; # we assume on path

# parameters
$usage = "Usage: $0 [-raw] [-v] [-i] [-u <lines>] [-b <backward bytes>]" .
"[-d <debuggerpath>] <forensic files>\n";

$file = "";
$debugger = "";
$verbose = 0;
$raw = 0;
$interactive = 0;
# lines to eb
$u_lines = 3;
$backward = 0;
while ($#ARGV >= 0) {
    if ($ARGV[0] eq '-d') {
        die $usage if ($#ARGV <= 0);
        shift;
        $debugger = $ARGV[0];
        # checked below for existence
    } elsif ($ARGV[0] eq '-raw') {
        $raw = 1;
    } elsif ($ARGV[0] eq '-i') {
        $interactive = 1;
    } elsif ($ARGV[0] eq '-v') {
        $verbose = 1;
    } elsif ($ARGV[0] eq '-u') {
        die $usage if ($#ARGV <= 0);
        shift;
        $u_lines = $ARGV[0];
    } elsif ($ARGV[0] eq '-b') {
        die $usage if ($#ARGV <= 0);
        shift;
        $backward = $ARGV[0];
    } elsif ($ARGV[0] =~ /^-/) {
        die $usage;
    } else {
        # rest are forensic files
        last;
    }
    shift;
}

# see if user-specified debugger exists
if ($debugger ne "") {
    $debugger = &win32path2unix($debugger);
    print "Trying debugger at $debugger\n" if ($verbose);
    if (! -f "$debugger") { # -x seems to require o+x so we don't check
        die "Cannot find debugger at $debugger\n";
    }
} else {
    for ($i = 0; $i <= $#try_dbgs; $i++) {
        $debugger = &win32path2unix($try_dbgs[$i]);
        print "Trying debugger at $debugger\n" if ($verbose);
        last if (-f "$debugger");
    }
    die "Cannot find a debugger: @try_dbgs\n" if ($i > $#try_dbgs);
}
if ($debugger !~ /cdb/) {
    # FIXME: does ntsd not support $>< ?
    # will have to pass it entire block of cmds on cmdline
    die "ntsd not yet supported: you need cdb\n";
    $use_logfile = 1;
}

$tmpdir = $ENV{'TMPDIR'} || $ENV{'TEMP'} || $ENV{'TMP'} || '/tmp';
die "Cannot find temp directory $tmpdir" if (! -d $tmpdir);
$script = "$tmpdir/bugdis-$$.script";
$winscript = &unixpath2win32($script);

die $usage if ($#ARGV < 0);
$multiple = ($#ARGV > 0);
while ($#ARGV >= 0) {
    my $filearg = $ARGV[0];
    # make it easy for users w/ non-globbing shell and non-globbing perl to
    # pass *, w/o double-globbing and messing up escaped chars for other users
    if (!$noglob && ($globall || $filearg =~ /\*/)) {
        # FIXME: must convert from \ to / BEFORE the glob -- yet don't
        # want to ruin escaped chars!  It's a tradeoff: is \* a directory delimiter
        # followed by metachar *, or is it an escaped * trying to be literal?
        # In everyday usage we assume no escaped *'s!
        # If we didn't care for \* directory delim we might do:
        #  $filearg =~ s|\\\\|//|g; # next rule won't get both slashes so need this rule
        #  $filearg =~ s|\\([\w\d])|/\1|g; # \ to / only if before a normal char
        $filearg =~ s|\\|/|g; # assume \ is dir delim, no support for \ as escape char
        @files = glob($filearg);
        print "globbing: \"$filearg\" => \"@files\"\n" if ($verbose);
        if ($#files < 0) {
            # if glob comes up empty, don't use its results (to get error msg)
            @files = ($filearg);
        }
    } else {
        @files = ($filearg);
    }
    $multiple = $multiple || $#files > 0;
    foreach $file (@files) {
        print "processing \"$file\"\n" if ($verbose);
        $file =~ s|\\|/|g; # normalize
        unless (-f "$file") {
            die "Error: No forensic file $file\n";
        }
        if ($multiple) {
            print "\n$file:\n";
        }
        process_file($file);
    }
    shift;
}

sub process_file($) {
    my ($file) = @_;
    $processing = 0;

    print "script file is $script\n" if ($verbose);
    open(OUT, "> $script") || die "Error: couldn't open $script";

    if ($use_logfile) {
        # must go through logfile as debugger won't write to accessible stdout
        $logfile = "$tmpdir/bugdis-$$.log";
        $logfile = &unixpath2win32($logfile);
        print "Log file is $logfile\n";
        # start fresh -- no stale results
        if (-f $logfile) {
            system("rm $logfile");
        }
        print OUT ".logopen $logfile\n";
    }

    open(IN, "< $file") || die "Error: couldn't open $file\n";
    while (<IN>) {
        if (/^((source)|(target)): 0x([0-9a-fA-F]+)/) {
            my $label = "$1";
            $addr{$label} = $4;
            my $val = hex($addr{$label});
            my $start = sprintf("%08x", ($val - $backward) & 0xfffffff0);
            my $dis = sprintf("%08x", ($val - $backward));
            print "Going to disassemble $label $addr{$label} $dis $start\n"
                if ($verbose);

            print OUT ".dvalloc /b $addr{$label} 1000\n";
            my $ebout = 0;
            while (<IN>) {
                if (/^$start /) {
                    $ebout = 1;
                }
                if ($ebout > 0) {
                    $ebout++;
                    last if ($ebout > ($u_lines+2)); # +2 since we back-aligned
                    # clear out ascii append
                    s/  .{16}$//;
                    print OUT "eb $_";
                }
            }
            # let's assume 5-byte instrs
            $num_instrs = $u_lines*3;
            print OUT "u $dis L$num_instrs\n";
        }
    }
    if ($use_logfile) {
        print OUT ".logclose\n";
    }
    # get syntax error if put newline after q!
    print OUT "q" unless ($interactive);
    &run_debugger($script, $addr{'source'}, $addr{'target'});
}

system("rm $script");

sub run_debugger($) {
    my ($runfile, $tgt1, $tgt2) = @_;

    $cmd = "\$><$winscript";

    # put quotes around debugger to handle spaces
    $cmdline = "\"$debugger\" -c \"$cmd\" $app";
    print "$cmdline\n" if ($verbose);
    if ($interactive) {
        system("$cmdline") && die "Error: couldn't run $cmdline\n";
    } elsif ($raw) {
        system("$cmdline") && die "Error: couldn't run $cmdline\n";
        if ($use_logfile) {
            system("cat $logfile");
        }
    } else {
        # prefix each ln output with the query (debugger doesn't re-print -c cmds)
        $output = 0;
        if ($use_logfile) {
            system("$cmdline") && die "Error: couldn't run $cmdline\n";
            open(DBGOUT, "< $logfile") || die "Error: couldn't open $logfile\n";
        } else {
            open(DBGOUT, "$cmdline |") || die "Error: couldn't run $cmdline\n";
        }
        while (<DBGOUT>) {
            if (/^Allocat/) {
                $output = 1;
                print "\n";
            } elsif ($output) {
                if (/^([0-9A-Fa-f]+)/) {
                    if ($1 eq $tgt1 || $1 eq $tgt2) {
                        print "=> $_";
                    } else {
                        print "   $_";
                    }
                } elsif (/Attempt to access/) {
                    # We deliberately continue past this message:
                    #  Allocation failed, Win32 error 487
                    #      "Attempt to access invalid address."
                    # as it often means we tried to allocate within our 1st alloc
                    print "WARNING: alloc failure (may simply mean 2nd within 1st)\n";
                } else {
                    $output = 0;
                }
            }
        }
        close(DBGOUT);
    }
}

sub unixpath2win32($) {
    my ($p) = @_;
    # use cygpath if available, it will convert /home to c:\cygwin\home, etc.
    $cp = `cygpath -wi \"$p\"`;
    chomp $cp;
    if ($cp eq "") {
        # do it by hand
        # add support for /x => x:
        $p =~ s|^/([a-z])/|\1:/|;
        # forward slash becomes backslash since going through another shell
        $p =~ s|/|\\|g;
    } else {
        $p = $cp;
    }
    # make single backslashes doubles
    $p =~ s|\\{1}|\\\\|g;
    return $p;
}

sub win32path2unix($) {
    my ($p) = @_;
    # avoid issues with spaces by using 8.3 names
    $cp = `cygpath -dmi \"$p\"`;
    chomp $cp;
    if ($cp eq "") {
        # do it by hand
        $p =~ s|\\|/|g;
    } else {
        $p = $cp;
    }
    return $p;
}
