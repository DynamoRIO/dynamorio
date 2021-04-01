#!/usr/bin/tclsh
# **********************************************************
# Copyright (c) 2017-2021 Google, Inc.    All rights reserved.
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

# Code style checks for use with vera++:
# Check lines for line length and simple content checks.

set max 90
# Some files are exempt from the 90-char limit:
array set length_exempt [list {statsx.h} {1} \
                             {optionsx.h} {1} \
                             {syscallx.h} {1} \
                             {decode_table.c} {1} \
                             {table_a32_pred.c} {1} \
                             {table_a32_unpred.c} {1} \
                             {table_t32_16.c} {1} \
                             {table_t32_16_it.c} {1} \
                             {table_t32_base.c} {1} \
                             {table_t32_coproc.c} {1} \
                             {table_encode.c} {1} \
                             {instr_create_api.h} {1} \
                             {decode_gen.h} {1} \
                             {encode_gen.h} {1} \
                             {elfdefinitions.h} {1} ]

foreach fname [getSourceFileNames] {
    set nline 1
    foreach line [getAllLines $fname] {
        if {[string length $line] > $max} {
            set basename [file tail $fname]
            if { ![info exists length_exempt($basename)] } {
                report $fname $nline "Line too long: exceeds $max chars."
            }
        }
        if [regexp {\t} $line] {
            report $fname $nline "Line contains tabs."
        }
        if [regexp {\r} $line] {
            report $fname $nline "Line contains carriage returns."
        }
        if [regexp {NOCHECKIN} $line] {
            report $fname $nline "Line contains NOCHECKIN."
        }
        if [regexp {[[:space:]]+$} $line] {
            report $fname $nline "Line contains trailing spaces."
        }
        incr nline
    }
}
