#!/usr/bin/perl

# **********************************************************
# Copyright (c) 2013 Google, Inc.  All rights reserved.
# Copyright (c) 2004-2008 VMware, Inc.  All rights reserved.
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

### I use this script to add the numbers in comments in the OP_
### enum in src/x86/instr.h
### Instructions: send the modified enum from decode_table.c
### as stdin for this script, it sends to standard out the enum
### with the numbers re-calculated.

$num = 0;
while (<>) {
    chop;
    # handle DOS end-of-line:
    if ($_ =~ /\r$/) { chop; };
    $l = $_;
    if ($l =~ /\* OP_/) {
        $l =~ /^\s*\/\*\s*OP_([a-zA-Z0-9_]*)(\s*)\*\/\s*(.*)$/;
        $op = $1;
        $space = $2;
        $table = $3;
        $name = $op;
        $name =~ s/xsave32/xsave/;
        $name =~ s/xrstor32/xrstor/;
        $name =~ s/xsaveopt32/xsaveopt/;
        printf "/* %3d */     OP_$op,$space /* $table */ /**< $name opcode */\n", $num;
        $num++;
    } else {
        print "$l\n";
    }
}
