#!/usr/bin/tclsh
# **********************************************************
# Copyright (c) 2023 Google, Inc.    All rights reserved.
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
# Check for required namespaces in C++ code.

foreach fname [getSourceFileNames] {
    puts "Checking $fname"
    set is_header false
    if {[regexp {\.h$} $fname] } {
        set is_header true
    } elseif {! [regexp {\.cpp$} $fname] } {
        continue
    }
    set found_dr_namespace false
    set found_anon_namespace false
    set found_main false
    set found_non_main false
    set found_class false
    foreach line [getAllLines $fname] {
        if [regexp {^namespace dynamorio} $line] {
            set found_dr_namespace true
        }
        if [regexp {^namespace \{} $line] {
            set found_anon_namespace true
        }
        if {[regexp {^main\(} $line] || [regexp {^_tmain\(} $line]} {
            set found_main true
        } elseif {[regexp {^[a-zA-Z_].*\(} $line]} {
            set found_non_main true
        }
        if [regexp {^class} $line] {
            set found_class true
        }
    }
    if { $is_header && !$found_class } {
        continue
    }
    if {[regexp {droption.h$} $fname] } {
        # TODO i#4343: Remove this exclusion after adding a namespace
        # to droption.
        continue
    }
    # Allow small executable front-ends to have only an anoymous
    # namespace, or even no namespace if there are no functions
    # besides main.
    if {! ($found_dr_namespace ||
           ($found_main &&
            ($found_anon_namespace || !$found_non_main))) } {
        report $fname 1 "C++ file is missing namespace declarations."
    }
}
