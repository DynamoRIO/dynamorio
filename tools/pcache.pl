#!/usr/bin/perl

# **********************************************************
# Copyright (c) 2007 VMware, Inc.  All rights reserved.
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

## pcache.pl
## by Derek Bruening
## February 2007

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
$usage = "Usage: $0 [-raw] [-i] [-d <debuggerpath>]" .
    "[-dr <DRdllpath>] <pcache>\n";

$debugger = "";
$verbose = 0;
$raw = 0;
$DRdll = "";
$interactive = 0;
$pfile = "";
die $usage if ($#ARGV < 0);
while ($#ARGV >= 0) {
    if ($ARGV[0] eq '-dr') {
        die $usage if ($#ARGV <= 0);
        shift;
        $DRdll = $ARGV[0];
        unless (-f "$DRdll") {
            die "$err No file $DRdll\n";
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
        $pfile = $ARGV[0];
        die $usage if ($#ARGV > 0);
    }
    shift;
}

$pfile_win32 = &unixpath2win32($pfile);
# try both -- different perls like different paths
unless (-f "$pfile" || -f "$pfile_win32") {
    die "Cannot find pcache file at $pfile or $pfile_win32\n";
}

if ($DRdll eq "") {
    $DRhome = $ENV{'DYNAMORIO_HOME'};
    if ($pfile =~ /-dbg/) {
        $suffix = "debug";
    } else {
        $suffix = "release";
    }
    $DRdll = "$DRhome/exports/lib32/$suffix/dynamorio.dll";
}
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

# FIXME: win2k ntsd does not load symbols by following loaded dll's path!
# setting _NT_SYMBOL_PATH ENV here or exporting in shell doesn't work,
# and -y not available on win2k ntsd, and .sympath takes all chars following
# (even \r, \n, ", ;!), no aS available to break cmd -- giving up

$cmd = "";
$marker = 'eeeeeeee';
$cmd .= "? $marker; ";

$map_pc = "13000000"; # arbitrary
$cmd .= ".echo \"Flags:\"; ";
$cmd .= ".if ((poi($map_pc+10) & 0x000001)==0x000001) {.echo \"X86_32\";}; ";
$cmd .= ".if ((poi($map_pc+10) & 0x000002)==0x000002) {.echo \"X86_64\";}; ";
$cmd .= ".if ((poi($map_pc+10) & 0x000004)==0x000004) {.echo \"SEEN_BORLAND_SEH\";}; ";
$cmd .= ".if ((poi($map_pc+10) & 0x000008)==0x000008) {.echo \"ELIDED_UBR\";}; ";
$cmd .= ".if ((poi($map_pc+10) & 0x000010)==0x000010) {.echo \"SUPPORT_RAC\";}; ";
$cmd .= ".if ((poi($map_pc+10) & 0x000020)==0x000020) {.echo \"SUPPORT_RCT\";}; ";
$cmd .= ".if ((poi($map_pc+10) & 0x000040)==0x000040) {.echo \"ENTIRE_MODULE_RCT\";}; ";
$cmd .= ".if ((poi($map_pc+10) & 0x000080)==0x000080) {.echo \"SUPPORT_TRACES\";}; ";
$cmd .= ".if ((poi($map_pc+10) & 0x000100)==0x000100) {.echo \"MAP_RW_SEPARATE\";}; ";
$cmd .= ".if ((poi($map_pc+10) & 0x000200)==0x000200) {.echo \"EXEMPTION_OPTIONS\";}; ";
$cmd .= "dt -r1 _coarse_persisted_info_t $map_pc; ";
$cmd .= ".echo Build number in decimal:; ";
$cmd .= "n10; ?? (uint)((_coarse_persisted_info_t *)0x$map_pc)->build_number; n16; ";
$cmd .= ".echo Options:; ";
$cmd .= "da @@(0x$map_pc + ((_coarse_persisted_info_t *)0x$map_pc)->header_len); ";
$cmd .= ".echo Hotpatch list:; ";
$cmd .= "dd @@(0x$map_pc + ((_coarse_persisted_info_t *)0x$map_pc)->header_len + ((_coarse_persisted_info_t *)0x$map_pc)->option_string_len) L@@(((_coarse_persisted_info_t *)0x$map_pc)->hotp_patch_list_len / 4); ";
$cmd .= ".echo Main hashtable:; ";
$cmd .= "?? *((_coarse_table_t *) (0x$map_pc + ((_coarse_persisted_info_t *)0x$map_pc)->header_len + ((_coarse_persisted_info_t *)0x$map_pc)->option_string_len + ((_coarse_persisted_info_t *)0x$map_pc)->hotp_patch_list_len + ((_coarse_persisted_info_t *)0x$map_pc)->reloc_len + ((_coarse_persisted_info_t *)0x$map_pc)->rac_htable_len + ((_coarse_persisted_info_t *)0x$map_pc)->rct_htable_len)); ";
$cmd .= ".echo Stub hashtable:; ";
$cmd .= "?? *((_coarse_table_t *) (0x$map_pc + ((_coarse_persisted_info_t *)0x$map_pc)->header_len + ((_coarse_persisted_info_t *)0x$map_pc)->option_string_len + ((_coarse_persisted_info_t *)0x$map_pc)->hotp_patch_list_len + ((_coarse_persisted_info_t *)0x$map_pc)->reloc_len + ((_coarse_persisted_info_t *)0x$map_pc)->rac_htable_len + ((_coarse_persisted_info_t *)0x$map_pc)->rct_htable_len + ((_coarse_persisted_info_t *)0x$map_pc)->cache_htable_len)); ";
$cmd .= ".echo RCT hashtable:; ";
$cmd .= "?? *((_app_pc_table_t *) (0x$map_pc + ((_coarse_persisted_info_t *)0x$map_pc)->header_len + ((_coarse_persisted_info_t *)0x$map_pc)->option_string_len + ((_coarse_persisted_info_t *)0x$map_pc)->hotp_patch_list_len + ((_coarse_persisted_info_t *)0x$map_pc)->reloc_len + ((_coarse_persisted_info_t *)0x$map_pc)->rac_htable_len)); ";
$cmd .= ".echo RAC hashtable:; ";
$cmd .= "?? *((_app_pc_table_t *) (0x$map_pc + ((_coarse_persisted_info_t *)0x$map_pc)->header_len + ((_coarse_persisted_info_t *)0x$map_pc)->option_string_len + ((_coarse_persisted_info_t *)0x$map_pc)->hotp_patch_list_len + ((_coarse_persisted_info_t *)0x$map_pc)->reloc_len)); ";

if ($use_logfile) {
    my $logdir = $ENV{'TMPDIR'} || $ENV{'TEMP'} || $ENV{'TMP'} || '/tmp';
    die "Cannot find temp directory $logdir" if (! -d $logdir);
    # must go through logfile as debugger won't write to accessible stdout
    $logfile = "$logdir/pcache.pl-$$.log";
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
$cmdline = "\"$debugger\" -g -c \"$cmd\" $DRload -debugbreak " .
    "-map $pfile_win32 $map_pc $DRdllwin32";
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
        $saw_marker = 1 if (/= $marker/);
        next unless ($saw_marker);
        print $_ unless (/= $marker/ || /^quit/ || /^\*\*\* ERROR: Symbol/);
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
