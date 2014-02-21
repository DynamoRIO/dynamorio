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

### address_query.pl
### Derek Bruening, June 2004
###
### launches a debugger and runs the "ln" command to get the nearest
###   symbol to each address in question
### works best if you have symbols -- for DR dll, have pdb in same dir,
###   for OS set _NT_SYMBOL_PATH
### sample usage:
###   address_query.pl c:/builds/11087/exports/lib32/release/dynamorio.dll 7004e660 77f830e7
### it also accepts stdin or a file with -f <file>
###
### REQUIRES that DYNAMORIO_TOOLS is set and that it points to
###   an already-built DRload.exe
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

# parameters
$usage = "Usage: $0 [-raw] [-i] [-f <addrfile>] [-d <debuggerpath>]
<DRdllpath> [<addr1> ... <addrN>]\n";

$debugger = "";
$verbose = 0;
$raw = 0;
$addrfile = "";
$DRdll = "";
$havedll = 0;
$numaddrs = 0;
$interactive = 0;
while ($#ARGV >= 0) {
    if ($ARGV[0] eq '-f') {
        die $usage if ($#ARGV <= 0);
        shift;
        $addrfile = $ARGV[0];
        unless (-f "$addrfile") {
            die "$err No file $addrfile\n";
        }
    } elsif ($ARGV[0] eq '-d') {
        die $usage if ($#ARGV <= 0);
        shift;
        $debugger = $ARGV[0];
        # checked below for existence
    } elsif ($ARGV[0] eq '-raw') {
        $raw = 1;
    } elsif ($ARGV[0] eq '-i') {
        $interactive = 1;
    } elsif ($ARGV[0] =~ /^-/) {
        die $usage;
    } else {
        if (!$havedll) {
            $DRdll = $ARGV[0];
            $havedll = 1;
        } else {
            # can't use both -f and cmdline params
            die $usage if ($addrfile ne "");
            $addrs[$numaddrs++] = $ARGV[0];
        }
    }
    shift;
}

if ($numaddrs == 0) { # no cmdline addrs
    if ($addrfile ne "") {
        open(ADDRS, "< $addrfile") || die "Couldn't open $addrfile\n";
    } else {
        open(ADDRS, "< -") || die "Couldn't open stdin\n";
    }
    while (<ADDRS>) {
        # remove \n, \r, spaces, etc.
        s|\s*||g;
        $addrs[$numaddrs++] = $_;
    }
    close(ADDRS);
}

die $usage if ($DRdll eq "");
$DRdllwin32 = &unixpath2win32($DRdll);
# try both -- different perls like different paths
unless (-f "$DRdll" || -f "$DRdllwin32") {
    die "Cannot find dll at $DRdll or $DRdllwin32\n";
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
    $use_logfile = 1;
}

$DRtools = $ENV{'DYNAMORIO_TOOLS'};
if ($DRtools eq "") {
    # try same dir this script sits in
    $DRtools = $0;
    if ($DRtools =~ /[\\\/]/) {
        $DRtools =~ s/^(.+)[\\\/][\w\.]+$/\1/;
    } else {
        $DRtools = "./";
    }
}
$DRload = "$DRtools/DRload.exe";
# need win32 path with double backslashes
# e.g., c:\\\\derek\\\\dr\\\\tools\\\\DRload.exe
$DRload = &unixpath2win32($DRload);
unless (-f "$DRload") { # -x seems to require o+x so we don't check
    die "Cannot find \$DYNAMORIO_TOOLS/DRload.exe at $DRload\n";
}

die $usage if ($numaddrs == 0);
$queries = "";
$marker = 'eeeeeeee';
for ($i=0; $i<=$#addrs; $i++) {
    # use a marker to indicate boundaries of output for each command
    # (some have empty output)
    $queries .= "? $marker; ln $addrs[$i]; ";
}
# put marker at the end too
$queries .= "? $marker;";

# FIXME: win2k ntsd does not load symbols by following loaded dll's path!
# setting _NT_SYMBOL_PATH ENV here or exporting in shell doesn't work,
# and -y not available on win2k ntsd, and .sympath takes all chars following
# (even \r, \n, ", ;!), no aS available to break cmd -- giving up

# enable line numbers (off by default in cdb)!
$cmd = ".lines -e; l+l; $queries";

if ($use_logfile) {
    # must go through logfile as debugger won't write to accessible stdout
    my $logdir = $ENV{'TMPDIR'} || $ENV{'TEMP'} || $ENV{'TMP'} || '/tmp';
    die "Cannot find temp directory $logdir" if (! -d $logdir);
    $logfile = "$logdir/address_query-$$.log";
    $logfile = &unixpath2win32($logfile);
    print "Log file is $logfile\n";
    # start fresh -- no stale results
    if (-f $logfile) {
        system("rm $logfile");
    }
    $cmd = ".logopen $logfile; $cmd; .logclose;";
}

$cmd .= " q" unless ($interactive);

# put quotes around debugger to handle spaces
$cmdline = "\"$debugger\" -g -c \"$cmd\" $DRload -debugbreak $DRdllwin32";
print $cmdline if ($verbose);
if ($interactive) {
    system("$cmdline") && die "Error: couldn't run $cmdline\n";
} elsif ($raw) {
    system("$cmdline") && die "Error: couldn't run $cmdline\n";
    if ($use_logfile) {
        system("cat $logfile");
    }
} else {
    # prefix each ln output with the query (debugger doesn't re-print -c cmds)
    $i = -1;
    $output = 0;
    if ($use_logfile) {
        system("$cmdline") && die "Error: couldn't run $cmdline\n";
        open(DBGOUT, "< $logfile") || die "Error: couldn't open $logfile\n";
    } else {
        open(DBGOUT, "$cmdline |") || die "Error: couldn't run $cmdline\n";
    }
    while (<DBGOUT>) {
        # look for our marker
        if (/= $marker/) {
            $i++;
            if ($i <= $#addrs) {
                print "\n$addrs[$i]:\n";
                $output = 1;
            } else {
                $output = 0; # at end
            }
        } elsif ($output) {
            print $_;
        }
        # I used to match the addresses printed but these debuggers sometimes
        # give weird output -- the right-hand is not always a larger address
        # than the left-hand!!!  FIXME: what's going on?
        # example:
        # w/ export symbols:
        #   0:001> ln 77f8d96b
        #   (77f8d02e)   ntdll!RtlNtStatusToDosError+0x93d   |  (77f8db68)   ntdll!aulldiv
        # w/ pdb:
        #   0:001> ln 77f8d96b
        #   (77f8d7c8)   ntdll!RtlpRunTable+0x1a3   |  (77f81ec1)   ntdll!RtlSetUserValueHeap
    }
    close(DBGOUT);
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
