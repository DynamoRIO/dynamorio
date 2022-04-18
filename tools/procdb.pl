#!/usr/bin/perl

# **********************************************************
# Copyright (c) 2021-2022 Google, Inc.  All rights reserved.
# Copyright (c) 2006 VMware, Inc.  All rights reserved.
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
## by Vladimir Kiriansky
## August 2006
##
## find odd processes and show description of all found
###########################################################################

## Requires wget

## example use:
# just collect all links to select a few suspicious ones
# $ ./procdb.pl -s bug_8704/iexplore.exe.1608.00000000.xml
#   ...
#   wget -O -  http://www.processlibrary.com/directory/files/wscntfy/ | grep -A 5 'Description:'

# normal use:
# $ ./procdb.pl bug_8704/iexplore.exe.1608.00000000.xml > processes.out
# $ cat process.out
#   ..
#   <td width="390">regscan.exe is added by
#           Trojan.W32.Rbot. It is a worm which attemps to spread via
#           network shares. It also contains backdoor Trojan
#           capabilities allowing unauthorised remote access to the
#           infected computer. If found on your system make sure that
#           you have downloaded the latest update for your antivirus
#           application. This process is a security risk and should be
#           removed from your system.<br>


# copy&paste from bugtitle.pl

# FIXME: should merge with the logic from wld/wld.pl
# about having an allowlist database on DLLs


# adding only exe's that should be seen only once!
# so that we can detect a c:\temp\services.exe as rogue

# FIXME: not yet used
%seen_known = ();

# should add more entries from config/applications.html or even have a
# generator from config/application_data.h
%known_exe_description = {
    'lsass' => 'lsass',
    'services' => 'services'};

$tools = $ENV{'DYNAMORIO_TOOLS'};
if ($tools eq "") {
    # use same dir this script sits in
    $tools = $0;
    if ($tools =~ /[\\\/]/) {
        $tools =~ s/^(.+)[\\\/][\w\.]+$/$1/;
        $tools =~ s/\\/\\\\/g;
    } else {
        $tools = "./";
    }
    # address_query needs DYNAMORIO_TOOLS set to find DRload.exe
    $ENV{'DYNAMORIO_TOOLS'} = $tools;
}

# parameters
$usage = "Usage: $0 [-v] [-q] [-glob] [-noglob] <forensic_file>\n";

$just_show = 0;
$verbose = 0;
$quiet = 0;

$globall = 0;
$noglob = 0;
while ($#ARGV >= 0) {
    if ($ARGV[0] eq '-v') {
        $verbose = 1;
    } elsif ($ARGV[0] eq '-q') {
        $quiet = 1;
    } elsif ($ARGV[0] eq '-s') {
        $just_show = 1;
    } elsif ($ARGV[0] eq '-glob') {
        $globall = 1;
    } elsif ($ARGV[0] eq '-noglob') {
        $noglob = 1;
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

sub is_known_exe
{
    my ($exe) = @_;

    return 0;

    return 0 unless exists $known_exe_description { $exe };

    # known should be only once
    if (defined $seen_known{ $exe } &&
        $seen_known{ $exe } == 1) {
        # test with svchost.exe which may also be the Welchia worm
        print "WARNING: second appearance of $exe\n";
    }
    $seen_known{ $exe } = 1;

    return 1;
}

sub get_external_data
{
 # wget -q http://www.processlibrary.com/directory/files/defwatch -O - | grep -A 5 "Description:"
    my ($exe) = @_;

    my $base = "http://www.processlibrary.com/directory/files/";
    my $url = "$base$exe/";
    my $wget = "wget -q -O - ";
    my $postprocess = "grep -A 5 'Description:'";
    my $desc = "";

    my $command = "$wget $url | $postprocess";

    $desc = `$command` if (!$just_show);
    print "$command \n" if ($just_show);

# FIXME: note processlibrary tries to find a substring if they can't find an exact match
# so we have to verify that the process name they are returning is the one we really asked for
# for the time being leaving users to worry about it by being more verbose
    return $desc;
}


#    http://www.processlibrary.com/directory/files/rtvscan/index.html

sub process_file($) {
    my ($file) = @_;
    $processing = 0;

    open(IN, "< $file") || die "Error: couldn't open $file\n";
    while (<IN>) {
        # Handle CR
        s/\r//;

        # Start of process list
        if (/^<process-list> <!\[CDATA\[/) {
            $processing = 1;
        }
        elsif (/]]> <\/process-list>/) {
            $processing = 0;
        } elsif ($processing) {
            $skip = 0;
            if (/^([^\s\<]+).exe$/i) {
                $process=$1;
            } else {
                $process=$_;
                chomp $process;
                if ($process ne "System") {
                    print "not an EXE '$process!'\n";
                } else {
                    $skip = 1;
                }
            }

            if (!$skip) {
                print "Querying '$process'\n" if ($verbose);
                $skip = is_known_exe($exe);
            }

            if (!$skip) {
                print "Querying Externally '$process'\n";
                print &get_external_data($process), "\n";
            }
        }
    }
    close(IN);
}

sub unixpath2win32($) {
    my ($p) = @_;
    # use cygpath if available, it will convert /home to c:\cygwin\home, etc.
    $cp = `cygpath -wi \"$p\"`;
    chomp $cp;
    if ($cp eq "") {
        # do it by hand
        # add support for /x => x:
        $p =~ s|^/([a-z])/|$1:/|;
        # forward slash becomes backslash since going through another shell
        $p =~ s|/|\\|g;
    } else {
        $p = $cp;
    }
    # make single backslashes doubles
    $p =~ s|\\{1}|\\\\|g;
    return $p;
}
