#!/usr/local/bin/perl

# **********************************************************
# Copyright (c) 2002-2008 VMware, Inc.  All rights reserved.
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

# Copyright (c) 2003-2007 Determina Corp.
# Copyright (c) 2002-2003 Massachusetts Institute of Technology
# Copyright (c) 2002 Hewlett Packard Company

### mkrelfile.pl
###
### strips vararg ifdef DEBUG macros from files
### major assumption: a line with LOG on it has nothing else after
### the LOG!
###

$usage = "Usage: $0 <infile>\n";
if ($#ARGV != 0) {
    print "$usage";
    exit 0;
}
$infile = $ARGV[0];
shift;

$inmacro = 0;
open(IN, "< $infile") || die "Error: Couldn't open $infile for input\n";
while (<IN>) {
    $l = $_;
    if ($inmacro) {
        # try to find ; not inside a string
        if ($l =~ /;[^\"]*$/) {
            $inmacro = 0;
            $l =~ s/[^;]*;//;
        } else {
            # print line so that compiler will have right line numbers
            # use backslash so will work inside of multiline macros
            print "\\\n";
            next;
        }
    }
    elsif ($l =~ /\WLOG\W/) {
        if (! ($l =~ /;[^\"]*$/) ) {
            $inmacro = 1;
        }
        if ($l =~ /\\\s*$/) {
            $l =~ s/(\W)LOG.*(\s*\\\s*)$/\1;\2/;
        } else {
            $l =~ s/(\W)LOG.*$/\1;/;
        }
    }
    print "$l";
}
close(IN);
