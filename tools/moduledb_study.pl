#!/usr/bin/perl

# **********************************************************
# Copyright (c) 2008 VMware, Inc.  All rights reserved.
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

# processes a file created by moduledb_study.sh

$file = $ARGV[0];
open(FILE, "< $file") || die "Error: Couldn't open $file\n";

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

$path = ".";
if ($ARGV[0] =~ /^(.*)\/[^\/]*$/) {
    $path = $1;
}

while (<FILE>) {
    $root = "-";
    $case_num = 0;
    $build = "-";
    $app = "-";
    $source = "-";
    $source_addr = "-";
    $source_prop = "-";
    $target = "-";
    $target_addr = "-";
    $target_prop = "-";
    $id = "-";

    if ($_ =~ /^(bug|case)-?_?\s?(\d+)/i) {
        $case_num = $2;
    } elsif ($_ =~ /^([^\/]*)\//) {
        $root = $1;
    }

    $bugtitle_info = `perl $tools/bugtitle.pl \Q$path/$_\E`;
    if ($bugtitle_info =~ /^VIOLATION\s([^\s]*)\s\((<unknown build>|custom|\d*)\s([^\)]*)\)(.*)$/) {
        $id = $1;
        $build = $2;
        $app = $3;
        $rest = $4;
        if ($rest =~ /^\s*(.*)\s--->\s(.*)$/) {
            $source = $1;
            $target = $2;
        } elsif ($rest =~ /^\s*(0x[\da-fA-F]*)\s\(([^\s]*)\s(.*)\)\s=>\s(0x[\da-fA-F]*)\s\(([^\s]*)\s(.*)\).*$/) {
            $source_addr = $1;
            $source = $2;
            $source_prop = $3;
            $target_addr = $4;
            $target = $5;
            $target_prop = $6;
        }
    }

    if ($case_num == 0) {
        print "$root";
    } else {
        print "$case_num";
    }
    print " $id \"$app\" $source $target\n";
}

# use this | sort | uniq > file
# grep -iE " ....\.....\.A " results_uniq.txt > results_a.txt
# ...
# grep -iEv " ....\.....\.[abcef] " results_uniq.txt > results_other.txt

# 1129 unique violations (however many are core bugs!)
# gathered from determina bugs folder, does not include zipped up forensics files
# 37 .O or .P
# 31 .A (stack) (mostly bugs)
# 471 .C/E/F
# 590 .B

# of .B (number may not add up due to some truncated forensics not being included)
# grep -iE " heap heap$" results_b.txt > results_b_heap_to_heap.txt
# grep -iEv " (heap|unknown) heap$" results_b.txt | grep -iE " heap$" > results_b_mod_to_
# 56 heap -> heap
# 50 unknown -> heap
# 408 module -> heap [potential dll2heap relaxation]
#   grep -iEv "(ntdll.dll|kernel32.dll|advapi32.dll|user32.dll|rpcrt4.dll|ole32.dll)" results_b_mod_to_heap.txt > res
#   118 not common dll (most of the remainder are bugs, from before we got the hook policy nailed down, or bad hookers)
#   even of these most are microsoft (need to get company name)
# 76 module -> module
#   grep -iEv " kernel32.dll$" results_b_mod_to_mod.txt > results_b_mod_to_mod_less_commo
#   54 less targeting kernel32 (bugs and bad hookers)
#     27 unique target dlls (~2/3 MS? need to run through my company name script)

# of rct - note there tend to be more unique violations per dll
# grep -Ev "dynamorio.dll" results_rct.txt > results_rct2.txt
# grep -iEv "unknown heap$" results_rct2.txt | grep  -iE " heap$" > results_rct2_mod_to_h
# (need to lean to select columns in emacs, for now used excel)
# 386 ignoring to dynamorio.dll (a bug)
#  15 unknown to heap
#  73 module to heap (.C) [currently not easily addressable, should add rct from list? limit to just heap? maybe stopping some of the auto-attacks?]
#  298 module to module
#   93 unique targets (~2/3 MS? need to run through company name script)
#   61 unique src modules ""
