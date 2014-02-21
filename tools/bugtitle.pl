#!/usr/bin/perl

# **********************************************************
# Copyright (c) 2005-2007 VMware, Inc.  All rights reserved.
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

## bugtitle.pl
## by Derek Bruening
## March 2005
##
## produces a bug title from a forensic file
## uses address_query.pl and wld.pl as helpers
##
## by default does an explicit glob if input filename as a * in it
## (since we have users in shells and perls that don't glob!)
## -noglob disables that; -glob causes explicit glob of all input filenames
##
## if updated copy to \\10.1.5.11\QA-Shares\QA\tools
###########################################################################
## Examples of type of output:
##
## > bugtitle.pl bug_4106_dllhost/dllhost.exe.2964.00000000.html
## CRASH 7100d818 (21113 dllhost.exe) place_fragment:fcache.c(1476)
##
## > bugtitle.pl bug_3749_connon/CanoScan_N650U_N656U_CSUv571a.exe.3424.00000000.html
## VIOLATION NZHB.5166.B (21131 CanoScan_N650U_N656U_CSUv571a.exe)  symcjit.dll --->  HEAP section
##
## > bugtitle.pl SmaService.exe.2104.00000000.xml
## VIOLATION NGSU.4493.B (30216 SmaService.exe) 0x7c2e4e8d (ADVAPI32.dll .text rx) => 0x0017b356 (heap rw)
##
## > bugtitle.pl codemod.exe.1416.00000000.xml
## VIOLATION QXMA.5800.A (custom codemod.exe) 0x004013ee (codemod.exe .text rx) => 0x0012ff60 (stack rw)
##
## > bugtitle.pl /work/studies/dgc/detectmode-nonativeexec/dotnet-serialization.exe/serialization.exe.136.00000172.xml
## VIOLATION QKNR.0769.C (custom serialization.exe) 0x791d721e (mscorwks.dll .text rx) => 0x7999cb28 (NULL|mscorlib.dll .text rx)
##
## > bugtitle.pl bug_4243_deadlock/inetinfo.exe.2020.00000000.html
## TIMEOUT (21129 inetinfo.exe)
##
## > bugtitle.pl sqlservr.exe.3048.00000000.xml
## OUT OF MEMORY (30118 sqlservr.exe) MB: 2047 RAM, 8 avail, 2169(^2215)/3940 commit
##
## > bugtitle.pl bug_3749_connon/build_21132/CanoScan_N650U_N656U_CSUv571a.exe.4064.00000000.html
## ASSERT (21132 CanoScan_N650U_N656U_CSUv571a.exe) win32/os.c:3263 is_native_thread_state_valid(trec->dcontext, (app_pc)cxt->Eip, (byte *)cxt->Esp)
##
###########################################################################
## Examples showing that type of slash doesn't matter (though backslashes
## must be doubled within cygwin):
##
## > bugtitle.pl \\\\10.1.5.10\\bugs\\bug_3978_w3wpcrash\\w3wp.exe.1124.00000000.html
## CRASH 7001588e (21102 w3wp.exe) vm_area_add_fragment:vmareas.c(4466)
##
## > bugtitle.pl //10.1.5.10/bugs/bug_3978_w3wpcrash/w3wp.exe.1124.00000000.html
## CRASH 7001588e (21102 w3wp.exe) vm_area_add_fragment:vmareas.c(4466)
##
###########################################################################
## Examples from cmd shell:
##
## C:\derek\dr\tools>perl bugtitle.pl \\10.1.5.10\bugs\bug_3978_w3wpcrash\w3wp.exe.1124.00000000.html
## CRASH 7001588e (21102 w3wp.exe) vm_area_add_fragment:vmareas.c(4466)
##
## c:\>perl c:/derek/dr/tools/bugtitle.pl \\10.1.5.10\bugs\bug_3749_connon\CanoScan_N650U_N656U_CSUv571a.exe.3424.00000000.html
## VIOLATION NZHB.5166.B (21131 CanoScan_N650U_N656U_CSUv571a.exe)  symcjit.dll --->  HEAP section
###########################################################################

$tools = $ENV{'DYNAMORIO_TOOLS'};
if ($tools eq "") {
    # use same dir this script sits in
    $tools = $0;
    if ($tools =~ /[\\\/]/) {
        $tools =~ s/^(.+)[\\\/][\w\.]+$/\1/;
        $tools =~ s/\\/\\\\/g;
    } else {
        $tools = "./";
    }
    # address_query needs DYNAMORIO_TOOLS set to find DRload.exe
    $ENV{'DYNAMORIO_TOOLS'} = $tools;
}
# may not be on cygwin, so explicitly invoke perl
$addrquery = "perl $tools/address_query.pl";
$vioquery = "perl $tools/wld/wld.pl -security_violation";

# parameters
$usage = "Usage: $0 [-b <build_dir_base>] [-d <path-to-dynamorio.dll>] [-v] [-a] [-q] [-glob] [-noglob] <forensic_file>\n";

$verbose = 0;
$quiet = 0;
$DRdll = "";
$builddir = "";
$globall = 0;
$noglob = 0;
$analyze = 0;

while ($#ARGV >= 0) {
    if ($ARGV[0] eq '-v') {
        $verbose = 1;
    } elsif ($ARGV[0] eq '-q') {
        $quiet = 1;
    } elsif ($ARGV[0] eq '-a') {
        $analyze = 1;
    } elsif ($ARGV[0] eq '-glob') {
        $globall = 1;
    } elsif ($ARGV[0] eq '-noglob') {
        $noglob = 1;
    } elsif ($ARGV[0] eq '-b') {
        die $usage if ($#ARGV <= 0);
        shift;
        $builddir = $ARGV[0];
        $builddir =~ s|\\|/|g; # normalize
        unless (-d "$builddir") {
            die "$err No directory $builddir\n";
        }
    } elsif ($ARGV[0] eq '-d') {
        die $usage if ($#ARGV <= 0);
        shift;
        $DRdll = $ARGV[0];
        $DRdll =~ s|\\|/|g; # normalize
        unless (-f "$DRdll") {
            die "$err No file $DRdll\n";
        }
    } elsif ($ARGV[0] =~ /^-/) {
        die $usage;
    } else {
        # rest should be forensic file names
        last;
    }
    shift;
}

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
    my $prefix = "<unknown type>";
    my $suffix = ""; # ok to be empty
    my $build_num = "<unknown build>";
    my $app = "<unknown app>";

    open(IN, "< $file") || die "Error: couldn't open $file\n";
    while (<IN>) {
        # Handle CR
        s/\r//;
        # Type of bug
        if (/^Execution security violation/) {
            $prefix = "VIOLATION";
            if ($file =~ /\.html/) {
                # use wld for html files
                $vioinfo = `$vioquery $file`;
                # example: Security violation detected from  symcjit.dll --->  HEAP section
                $vioinfo =~ /Security violation detected from (.+)/m;
                $suffix = $1;
                # wld.pl likes to put extra spaces in!
                $suffix =~ s/(\s)\s+/\1/g;
                $suffix =~ s/^\s+//;
            } # else we hit the source and target-properties below
        }
        elsif (/Threat ID:\s*([^\s\<]+)/) {
            $threat = $1;
            $prefix .= " $threat";
            if ($threat =~ /c0000017/) {
                $prefix .= " V";
            }
            elsif ($threat =~ /c0000012d/) {
                $prefix .= " P";
            }
        }
        elsif (/<dll\s+range=\"0x(\w+)-0x(\w+)\"\s*name=\"([^\"]+)\"/) {
            $dll_name[$num_dlls] = $3;
            $dll_start[$num_dlls] = hex($1);
            $dll_end[$num_dlls] = hex($2);
            $num_dlls++;
        }
        elsif (/<source-properties/) {
            # for hot patch violations, source isn't relevant; see case 7686
            if (!($threat =~ /[PH]$/)) {
                $suffix = &address_properties();
            }
        }
        elsif (/<target-properties/) {
            # for hot patch violations, target isn't relevant; see case 7686
            if (!($threat =~ /[PH]$/)) {
                $suffix .= " => " . &address_properties();
            }
        }
        elsif (/^Unrecoverable error at PC 0x([0-9A-Za-z]+)/) {
            # w/ multiple files, need local here so we look at each
            # crash separately
            my $symdll;
            $addr = $1;
            if ($addr =~ /^15/) {
                $lib = "debug";
            } else {
                $lib = "release";
            }
            if ($DRdll eq "") {
                $symdll = &findDRdll($builddir, $build_num, $lib);
            } else {
                # if dr dll passed to us, use it for every file
                $symdll = $DRdll;
            }
            $prefix = "CRASH $addr";
            $suffix = "[error acquiring line number]";
            if ($symdll ne "") {
                my $query = "$addrquery";
                $query .= " -raw" if ($verbose);
                print "invoking \"$query $symdll $addr\"\n" if ($verbose);
                $sym = `$query $symdll $addr`;
                print "addrquery returned: $sym\n" if ($verbose);
                if ($sym =~ m|([\w\./]+\(\d+\))|m) {
                    $line = $1;
                    $sym =~ /dynamorio!(.+)\+/m;
                    $func = $1;
                    $suffix = "$func:$line";
                } else {
                    print "Unable to find address in $symdll\n";
                    # We found the dll, but couldn't find the address.
                    # Is likely an exception in generated code.
                    $suffix = "<exception in generated code>";
                }
            } else {
                print "Error locating dynamorio.dll $build_num\n";
            }
        }
        elsif (/^Internal SecureCore Error: (.*)/) {
            $prefix = "ASSERT";
            $suffix = $1;
        }
        elsif (/^CURIOSITY : (.*)/) {
            $prefix = "CURIOSITY";
            $suffix = $1;
        }
        elsif (/^Timeout expired/) {
            $prefix = "TIMEOUT";
        }
        elsif (/^Out of memory/) {
            $prefix = "OUT OF MEMORY";
        }
        elsif (($analyze || $prefix =~ /MEMORY/) && /<basic-information>/) {
            $_ = <IN>;
            /\d+ \d+ \d+ (\d+) \d+ \d+ \d+/;
            $ram = $1 * 4/1024;
            $suffix .= sprintf("%u RAM, ", $ram);
        }
        elsif (($analyze || $prefix =~ /MEMORY/) && /num_threads:/) {
            /num_threads: (\d+)/;
            $ThreadCount = $1;
            $suffix .= sprintf(", %d Thr", $ThreadCount);
            if ($ThreadCount > 1000) {
                $suffix .= "ead PILE!";
            }
        }
        elsif (($analyze || $prefix =~ /MEMORY/) && /<vm-counters>/) {
            $_ = <IN>;
            ($PeakVirtualSize,
             $VirtualSize,
             $PageFaultCount,
             $PeakWorkingSetSize,
             $WorkingSetSize,
             $QuotaPeakPagedPoolUsage,
             $QuotaPagedPoolUsage)
                = split / /, $_;

            $_ = <IN>;
            ($QuotaPeakNonPagedPoolUsage,
             $QuotaNonPagedPoolUsage,
             $PagefileUsage,
             $PeakPagefileUsage)
                = split / /, $_;

            # matching drview names (e.g. PagefileUsage is Priv) */
            print "KB: " .
                $PeakVirtualSize/1024 . " PVSz," .
                $VirtualSize/1024 . " VSz," .
                $PeakPagefileUsage/1024 . " PPriv," .
                $PagefileUsage/1024 . " Priv," .
                $PeakWorkingSetSize/1024 . " PWSS," .
                $WorkingSetSize/1024 . " WSS," .
                $PageFaultCount/1 . " Fault," .
                $QuotaPeakPagedPoolUsage/1024 . " PPage,"  .
                $QuotaPagedPoolUsage/1024 . " Page," .
                $QuotaPeakNonPagedPoolUsage/1024 . " PNonP," .
                $QuotaNonPagedPoolUsage/1024 . " NonP" .
                "\n" if ($verbose);

            # note that pre 4.3 core writes these counters as %d  (case 10481)
            # whlie we want to convert them to uint's here
            if ($VirtualSize < 0) {
                $VirtualSize += 0x100000000; # 2^32
            }
            if ($PagefileUsage < 0) {
                $PagefileUsage += 0x100000000; # 2^32
            }
            if ($PeakWorkingSetSize < 0) {
                $PeakWorkingSetSize += 0x100000000; # 2^32
            }

            # all in MB
            $suffix .= sprintf("MB: %u VSz, %u Priv, %u PWSS; ",
                               $VirtualSize/1024/1024,
                               $PagefileUsage/1024/1024,
                               $PeakWorkingSetSize/1024/1024);
        }
        elsif (($analyze || $prefix =~ /MEMORY/) && /<performance-information>/) {
            # skip 1st line
            <IN>;
            # this line has what we want
            $_ = <IN>;
            /-?\d+ -?\d+ -?\d+ (\d+) (\d+) (\d+) (\d+)/;
            # don't expect these to be negative
            $avail = $1 * 4/1024;
            $commit = $2 * 4/1024;
            $limit = $3 * 4/1024;
            $peak = $4 * 4/1024;
            $suffix .= sprintf("%u avail, %u(^%u)/%u commit",
                               $avail, $commit, $peak, $limit);
        }

        # Build version
        elsif (/^Generated by SecureCore [^,]+,\s*(.+)/) {
            $build_string = $1;
            if ($build_string =~ /build ([0-9]+)/) {
                $build_num = $1;
            } else {
                # FIXME: grab path from reg keys and try that path on local host?
                $build_num = "custom";
            }

        }

        # Process Name
        #
        # violation
        elsif (/^Short[\s\-]Qualified[\s\-]Name[:=]\s*\"?([^\"\n]+)\"?/i) {
            $app = $1; # short qualified name
        }
        # crash
        elsif (/^\s*Name\s?=\s*\"?([^\"\n]+)\"?/i) {
            $app = $1; # full qualified name
            $app =~ s/.*[\\\/]//; # remove path
        }
        # 2.1 compatibility, let better name finding (above) override since we make
        # assumptions (that may not always hold) about name mangling here
        elsif ($app =~ /^<unknown app>$/) {
            if (/^Command Line:\s*(.+)/) {
                $app = $1; # full cmdline
                if ($app !~ /svchost.exe/) {
                    # remove args
                    $app =~ s/(\.exe)\W+.*$/\1/;
                    # remove quotes
                    $app =~ s/\"//g;
                }
                $app =~ s/.*[\\\/]//; # remove path
            }
        }
    }
    close(IN);

    print "$prefix ($build_num $app) $suffix\n";
}

sub findDRdll($,$,$) {
    my ($builddir, $build_num, $lib) = @_;
    # would be nice to use symbol server, but cannot locate
    # based solely on build number w/o querying every dll, or having
    # a symlink set up externally or something.
    my @try = ( "//10.1.5.15/nightly",
                "//10.1.5.15/nightly_archive" );
    if ($builddir ne "") {
        unshift @try, $builddir;
    }
    print "searching these bases: @try\n" if ($verbose);
    # match ver_2_1, ver_2_5, etc., as well as the new dist
    # we avoid {ver*,dist} since that gives two entries regardless of existence
    my $ver = "[vd][ei][rs]*";
    my $missed_at_all = 0;
    foreach $base (@try) {
        my $glob_path = "$base/$build_num/$ver/lib/$lib/dynamorio.dll";
        my @wildcard = glob($glob_path);
        if (@wildcard == 0) {
            print STDERR "couldn't find $glob_path\n" if (!$quiet);
            $missed_at_all = 1;
        }
        foreach $w (@wildcard) {
            print "looking for $w\n" if ($verbose);
            if (-f $w) {
                # avoid found message if did it 1st try
                print STDERR "found $w\n" if (!$quiet && $missed_at_all);
                return $w;
            }
            print STDERR "couldn't find $w\n" if (!$quiet);
            $missed_at_all = 1;
        }
    }
    return "";
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

# examples:
# 0x04932af4 (stack rx)
# 0x003b2f76 (heap rw)
# 0x79b99773 (NULL|mscorlib.dll .data rwx)"
sub address_properties() {
    my $address = "";
    my $module = "";
    my $section = "";
    my $protect = "";
    my $type = "";
    while (<IN>) {
        # Handle CR
        s/\r//;
        if (/address=.*\"([^\"]+)\"/) {
            $address = $1;
        } elsif (/module=.*\"([^\"]+)\"/) {
            $module = $1;
        } elsif (/sec_name=.*\"([^\"]+)\"/) {
            $section = $1;
        } elsif (/State=.* FREE/) {
            $protect = 'free';
        } elsif ($protect ne 'free' && /Protect=.*\"\w+ ([^\"]+)\"/) {
            $protect = $1;
            $protect =~ s/-//g;
        } elsif (/Type=.*\"\w+ ([^\"]+)\"/) {
            $type = $1;
            if ($type =~ /IMAGE/i) {
                # if no export name get file name from module list
                if ($module =~ /\(none\)/) {
                    # brute force
                    # FIXME: use nice interval data structure
                    $address =~ /0x(\w+)/;
                    my $addrval = hex($1);
                    for ($i=0; $i<$num_dlls; $i++) {
                        if ($addrval >= $dll_start[$i] &&
                            $addrval < $dll_end[$i]) {
                            $module = "NULL|" . $dll_name[$i];
                        }
                    }
                }
                $module .= " $section";
            } else {
                if ($threat =~ /\.A$/) {
                    $module = "stack";
                } else {
                    $module = "heap";
                }
            }
            # we assume we'll see a Type line else we'll loop to the end!
            last;
        }
    }
    return "$address ($module $protect)";
}
