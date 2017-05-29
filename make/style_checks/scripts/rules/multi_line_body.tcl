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
# Check for multi-line body without {}.
foreach fname [getSourceFileNames] {
    # Not checking "do" or "while" for now.
    foreach token [getTokens $fname 1 0 -1 -1 {for if}] {
        set value [lindex $token 0]
        set nline [lindex $token 1]
        set ncol [lindex $token 2]
        set type [lindex $token 3]
        set toklen [string length $value]
        set rest [getTokens $fname $nline [expr $ncol + $toklen] [expr $nline+10] -1 {}]
        set newlines 0
        set parens 0
        set start 0
        set eol 0
        set prev_cppcomment 0
        # Skip "for" after #error.
        set line [getLine $fname $nline]
        if [regexp {^#} $line] {
            continue
        }
        foreach next $rest {
            set token_type [lindex $next 3]
            # Work around a bug (https://bitbucket.org/verateam/vera/issues/90) where
            # vera++ skips a newline token after a cppcomment.  We rely on there
            # being space on the next line here.
            if {$token_type == "space" && $prev_cppcomment == 1} {
                set token_type "newline"
            }
            if {$token_type == "cppcomment"} {
                set prev_cppcomment 1
            } else {
                set prev_cppcomment 0
            }
            if {$start == 0 || $parens > 0} {
                if {$token_type == "leftparen"} {
                    incr parens
                    set start 1
                }
                if {$token_type == "rightparen"} {
                    set parens [expr $parens - 1]
                }
                continue
            }
            if {$eol == 0} {
                # The vera parser has to combine continuations, but we want to separate.
                set line [getLine $fname [lindex $next 1]]
                if [regexp {\\[[:space:]]*$} $line] {
                    set eol 1
                }
                if {$token_type == "space"} {
                    continue
                }
                if {$token_type == "leftbrace"} {
                    # All good.
                    break
                }
                if {$token_type == "semicolon" && ([lindex $token 3]) == "if" &&
                    $eol == 0} {
                    report $fname $nline "Body should be on its own line."
                    break
                }
                if {$token_type == "newline"} {
                    set eol 1
                }
                continue
            }
            if {$token_type == "newline"} {
                incr newlines
            }
            # Handle a #endif line
            set line [getLine $fname [lindex $next 1]]
            if [regexp {^# *endif$} $line] {
                set newlines [expr $newlines - 1]
            }
            if {$token_type == "semicolon"} {
                if {$newlines > 0} {
                    report $fname $nline "Multi-line body needs {}."
                }
                break
            }
        }
    }
}
