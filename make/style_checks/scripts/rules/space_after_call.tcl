#!/usr/bin/tclsh
# **********************************************************
# Copyright (c) 2017 Google, Inc.    All rights reserved.
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
# * Neither the name of Google, Inc. nor the names of its contributors may be
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

# Code style check for use with vera++:
# Check for extra spaces after function names.
foreach fname [getSourceFileNames] {
    foreach token [getTokens $fname 1 0 -1 -1 {identifier}] {
        set value [lindex $token 0]
        set nline [lindex $token 1]
        set ncol [lindex $token 2]
        set toklen [string length $value]
        set prior [getTokens $fname $nline 0 $nline $ncol {}]
        if {([lindex [lindex $prior 0] 3]) == "pp_define"} {
            break
        }
        set rest [getTokens $fname $nline [expr $ncol + $toklen] [expr $nline + 1] -1 {}]
        set spaces 0
        set in_params 0
        set parens 0
        set past_close 0
        foreach next $rest {
            # Look for an open paren to indicate a function call.
            if {$in_params == 0} {
                if {([lindex $next 3]) == "space"} {
                    incr spaces
                    continue
                }
                if {([lindex $next 3]) == "leftparen"} {
                    set in_params 1
                    incr parens
                    continue
                } else {
                    break
                }
            } elseif {$parens > 0} {
                if {([lindex $next 3]) == "leftparen"} {
                    incr parens
                    set start 1
                }
                if {([lindex $next 3]) == "rightparen"} {
                    set parens [expr $parens - 1]
                }
                continue
            }
            # To distinguish a function pointer type we look for another (.
            if {([lindex $next 3]) == "space" ||
                ([lindex $next 3]) == "newline"} {
                continue
            }
            if {([lindex $next 3]) == "leftparen"} {
                set spaces 0
            }
            break
        }
        if {$in_params > 0 && $spaces > 0} {
            report $fname $nline "Extra space after function name: \"$value \"."
        }
    }
}
