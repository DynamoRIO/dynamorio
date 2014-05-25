#!/usr/local/bin/perl

# **********************************************************
# Copyright (c) 2004-2009 VMware, Inc.  All rights reserved.
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

# Copyright (c) 2004-2007 Determina Corp.

### gen_event_strings.pl
### reads in a .mc file and creates a header file with fmt string constants
### matching those in the .mc file (in printf style format)

$usage = "Usage : gen_event_strings <mc file> <target file>";

#print stderr "$#ARGV\n";
#print stderr "$ARGV[0]\n";
#print stderr "$ARGV[1]\n";

if ($#ARGV != 1) {
    print "$usage";
    exit 0;
}

open(IN, "<$ARGV[0]") || die("Error : gen_event_strings : couldn't open .mc file ","$ARGV[0]","\n");

open(OUT, ">$ARGV[1]") || die("Error : gen_event_strings : couldn't open output file ","$ARGV[0]","\n");

while ($line = <IN>) {
    $line =~ s/\r?\n$//; # chomp not sufficient for cross-plaform perl
    if ($line =~ /^SymbolicName *= *(\S+)\s*$/) {
        $name = $1 . "_STRING";
        $val = "";
        if ($line = <IN>) {
            $line =~ s/\r?\n$//; # chomp not sufficient for cross-plaform perl
            if ($line !~ /^Language *= *English\s*$/) {
                # may need to remove once have multilingual support? FIXME
                die("Error : gen_event_strings : pattern match for Language=English failed for ",$name,"\n");
            }
            $success = 0;
            while ($line = <IN>) {
                $line =~ s/\r?\n$//; # chomp not sufficient for cross-plaform perl
                if ($line =~ /^\.\s*$/) {
                    $sucess = 1;
                    last;
                }
                $val .= $line;
                $val .= "\\n";
            }
            # remove final newline
            chop $val;
            chop $val;
            if ($sucess == 0) {
                die("Error : file ended unexpectedly");
            }
            # FIXME we only need %s for now, but following should be general
            # as long as the letter codes match (seems to be the case), what
            # about 1, 2, 3 can the .mc fmt string reorder the arguments?
            $val =~ s/%.!([^!]+)!/%\1/g;
            print OUT "#define ",$name," \"",$val,"\"\n";
        } else {
            die("Error : file ended unexpectedly");
        }
    }
}
