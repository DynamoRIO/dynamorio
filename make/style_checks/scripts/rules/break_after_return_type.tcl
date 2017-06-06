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
# Check for missing newline after function definition return type.
foreach fname [getSourceFileNames] {
    set nline 1
    set continuation 0
    foreach line [getAllLines $fname] {
        if {$continuation == 1} {
            incr nline
            continue
        }
        set tokens [getTokens $fname $nline 0 [expr $nline + 5] -1 {}]
        if [regexp {\\[[:space:]]*$} $line] {
            set continuation 1
        } else {
            set continuation 0
        }
        # We limit our check to function definitions at column 0.
        set first [lindex [lindex $tokens 0] 3]
        if {$first == "space" ||
            $first == "typedef" ||
            $first == "cppcomment" ||
            [string range $first 0 2] == "pp_"} {
            incr nline
            continue
        }
        set found_open 0
        set pre_paren_count 0
        set bind_to_prev 0
        set found_newline 0
        set in_template 0
        foreach token $tokens {
            # Skip over a complete template type.
            if {$in_template == 1} {
                if {([lindex $token 3]) == "greater"} {
                    set in_template 0
                }
                continue
            } elseif {([lindex $token 3]) == "less"} {
                set in_template 1
                continue
            }
            if {([lindex $token 3]) == "newline"} {
                if {$found_open == 0} {
                    set found_newline 1
                }
            }
            if {([lindex $token 3]) == "leftparen"} {
                set found_open 1
            } elseif {$found_open == 0 && $bind_to_prev == 0} {
                incr pre_paren_count
            }
            # Bind together foo::bar, foo::~foo, foo::operator!.
            if {([lindex $token 3]) == "colon_colon" ||
                ([lindex $token 3]) == "compl" ||
                ([lindex $token 3]) == "operator"} {
                set bind_to_prev 1
                set pre_paren_count [expr $pre_paren_count - 1]
            } else {
                set bind_to_prev 0
            }
            if {([lindex $token 3]) == "semicolon"} {
                # Declaration, where a break is not required.
                break
            }
            if {([lindex $token 3]) == "leftbracket" ||
                ([lindex $token 3]) == "assign"} {
                # Array or struct (with name created via macro or sthg).
                break
            }
            if {([lindex $token 3]) == "leftbrace"} {
                if {$pre_paren_count > 1 && $found_open == 1 && $found_newline == 0} {
                    set cur_line [getLine $fname [lindex $token 1]]
                    if [regexp {\{.*\}} $cur_line] {
                        # We do allow a single-line definition as there are a bunch in
                        # existing code.
                    } else {
                        report $fname $nline "Missing newline after return type."
                    }
                }
                break
            }
        }
        incr nline
    }
}
