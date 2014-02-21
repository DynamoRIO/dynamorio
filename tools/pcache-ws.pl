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

## pcache-ws.pl
## by Derek Bruening
## February 2007
##
## Lists persistent cache files and modules in a process's working set,
## using vadump.exe.

$usage = "Usage: $0 <pid>";
$tools = $ENV{'DYNAMORIO_TOOLS'};
if ($tools eq "") {
    # use same dir this script sits in
    $tools = $0;
    if ($tools =~ /[\\\/]/) {
        $tools =~ s/^(.+)[\\\/][^\\\/]+$/\1/;
        $tools =~ s/\\/\\\\/g;
    } else {
        $tools = "./";
    }
    # address_query needs DYNAMORIO_TOOLS set to find DRload.exe
    $ENV{'DYNAMORIO_TOOLS'} = $tools;
}
$vadump = `cygpath -u $tools/external/vadump`;
chomp $vadump;

die $usage if ($#ARGV < 0);
$pid = $ARGV[0];

$cmd = "$vadump -o -p $pid";
print "Running $cmd\n" if ($verbose);

# FIXME: win32 perl (non-cygwin) doesn't need the pipe-hang workaround;
# we try to distinguish via the interpreter path: no guarantees though!
if ($^X =~ /\/usr/) {
    # FIXME: a direct pipe hangs, but passing through something else works
    open(VADUMP, "$cmd 2>&1 | cat |") || die "Error running $cmd\n";
} else {
    # FIXME: can't get stderr redirection working for win32 perl
    open(VADUMP, "$cmd |") || die "Error running $cmd\n";
}
while (<VADUMP>) {
    chomp;
    s/\r//; # in case DOS files get in here!
    if (/^OpenProcess/) {
        die "No such process with pid $pid\n";
    }
    next unless (/^0x[0-9A-F]{8} \((\d+)\) (.*)$/);
    $count = $1;
    $descr = $2;
    print "Got $count $descr\n" if ($verbose);
    next if ($descr =~ /^PRIVATE/i || $descr =~ /^UNKNOWN/ ||
             $descr =~ /^Process/ || $descr =~ /^TEB/ ||
             $descr =~ /^Stack/ || $descr =~ /^DATAFILE.*\.nls$/);
    $descr =~ s/DATAFILE_MAPPED Base 0x[0-9A-F]{8} //;
    $descr = lc($descr);
    $raw_descr = $descr;
    if ($raw_descr ne $last_raw_descr) {
        $finished{$last_descr} = 1;
        # handle duplicate names
        if ($finished{$descr}) {
            $dupnum = 2;
            do {
                $dupdescr = "$descr ($dupnum)";
                $dupnum++;
            } while ($finished{$dupdescr});
            $descr = $dupdescr;
        }
    } else {
        $descr = $last_descr;
    }
    $private{$descr}++ if ($count == 0);
    $shareable{$descr}++ if ($count == 1);
    $shared{$descr}++ if ($count > 1);
    $mem{$descr} = $descr;
    $last_descr = $descr;
    $last_raw_descr = $raw_descr;
}
close(VADUMP);

die "Error running $cmd\n" if (scalar(keys %mem) == 0);
printf "%6s %6s %6s %6s %s \n", "Priv", "S-able", "Shared", "Total";
foreach $i (sort (keys %mem)) {
    printf "%6d %6d %6d %6d  %s \n", $private{$i}, $shareable{$i}, $shared{$i},
    $private{$i} + $shareable{$i} + $shared{$i}, $i;
}
