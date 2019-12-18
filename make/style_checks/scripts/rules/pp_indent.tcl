#!/usr/bin/tclsh
# **********************************************************
# Copyright (c) 2019 Google, Inc. All rights reserved.
# Copyright (c) 2017 ARM Limited. All rights reserved.
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
# * Neither the name of ARM Limited nor the names of its contributors may be
#   used to endorse or promote products derived from this software without
#   specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED. IN NO EVENT SHALL ARM LIMITED OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
# SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
# CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
# OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
# DAMAGE.

# Code style check for use with vera++:
# Check that macros inside #if are indented properly.

# FIXME i#2473: fix macro indentation style violations in exempt files
set indent_exempt [list {aarch64/instr_create.h}     {1} \
                        {arch/arch.h}                {1} \
                        {arch/instr_create_shared.h} {1} \
                        {arch/instr.h}               {1} \
                        {arch/instr_inline.h}        {1} \
                        {arch/retcheck.c}            {1} \
                        {arch/sideline.c}            {1} \
                        {arch/steal_reg.c}           {1} \
                        {arch/loadtoconst.h}         {1} \
                        {arm/table_private.h}        {1} \
                        {x86/optimize.c}             {1} \
                        {x86/mangle.c}               {1} \
                        {x86/emit_utils.c}           {1} \
                        {core/config.c}              {1} \
                        {core/dispatch.c}            {1} \
                        {core/dynamo.c}              {1} \
                        {core/fragment.c}            {1} \
                        {core/globals.h}             {1} \
                        {core/hashtable.h}           {1} \
                        {core/hashtablex.h}          {1} \
                        {core/hotpatch.c}            {1} \
                        {core/link.h}                {1} \
                        {core/module_shared.h}       {1} \
                        {core/moduledb.c}            {1} \
                        {core/options.c}             {1} \
                        {core/options.h}             {1} \
                        {core/optionsx.h}            {1} \
                        {core/os_shared.h}           {1} \
                        {core/stats.c}               {1} \
                        {core/stats.h}               {1} \
                        {core/utils.c}               {1} \
                        {core/utils.h}               {1} \
                        {core/vmareas.c}             {1} \
                        {core/vmareas.h}             {1} \
                        {lib/globals_shared.h}       {1} \
                        {lib/hotpatch_interface.h}   {1} \
                        {lib/instrument.c}           {1} \
                        {lib/instrument_api.h}       {1} \
                        {drlibc/drlibc.h}            {1} \
                        {lib/statsx.h}               {1} \
                        {unix/os.c}                  {1} \
                        {unix/signal.c}              {1} \
                        {unix/injector.c}            {1} \
                        {unix/loader.c}              {1} \
                        {unix/module.c}              {1} \
                        {unix/module.h}              {1} \
                        {unix/module_elf.c}          {1} \
                        {unix/module_elf.h}          {1} \
                        {win32/aslr.c}               {1} \
                        {win32/aslr.h}               {1} \
                        {win32/callback.c}           {1} \
                        {win32/syscall.c}            {1} \
                        {win32/pre_inject.c}         {1} \
                        {win32/os_private.h}         {1} \
                        {win32/os.c}                 {1} \
                        {win32/ntdll.h}              {1} \
                        {win32/ntdll.c}              {1} \
                        {win32/module.h}             {1} \
                        {win32/inject_shared.h}      {1} \
                        {win32/inject_shared.c}      {1} \
                        {win32/gbop.h}               {1} \
                        {win32/module.c}             {1} \
                        {win32/module_shared.c}      {1}]

set indent_exempt_dirs [list {*ext/drsyms/libelftc/*}  {1}]

foreach fname [getSourceFileNames] {
    # excluded filenames consist of filename and parent directory.
    set parent [file tail [file dirname $fname]]
    set fileend [file tail $fname]
    if { "$parent/$fileend" in $indent_exempt } {
        continue;
    }

    set ignore_file 0
    foreach dir $indent_exempt_dirs {
        if {[string match $dir $fname]} {
            set ignore_file 1
        }
    }
    if {$ignore_file} {
        continue
    }

    set first_ifndef 1
    set num_if 0
    foreach token [getTokens $fname 1 0 -1 -1 \
                             {pp_if pp_ifdef pp_ifndef pp_define pp_endif \
                              pp_undef pp_elif pp_else pp_qheader}] {
        set value [lindex $token 0]
        set nline [lindex $token 1]
        set ncol [lindex $token 2]
        set token_type [lindex $token 3]
        set toklen [string length $value]
        set line [getLine $fname $nline]

        # Ignore indent for first #ifndef in header files which commonly is the
        # include guard spanning the whole file.
        if {$token_type == "pp_ifndef" && $first_ifndef == 1 && [string match "*.h" $fname]} {
            set first_ifndef 0;
            continue
        }

        if {($token_type == "pp_ifndef" || $token_type == "pp_ifdef" || $token_type == "pp_if") &&
            [string match "*/\* around whole file \*/" $line]} {
            continue
        }

        if {$token_type == "pp_endif"} {
            incr num_if -1
        }

        set token_string [string trim [string range $value 1 $toklen]]
        if {$token_type == "pp_else" || $token_type == "pp_elif"} {
            set spaces [string repeat " " [expr $num_if - 1] ]
        } else {
            set spaces [string repeat " " $num_if]
        }
        set line_start "#$spaces$token_string"
        if {![string match "$line_start*" $line]} {
            report $fname $nline "Wrong indentation for macro, current indentation level is\
                                  $num_if and line should start with '$line_start'."
        }

        if {$token_type == "pp_if" || $token_type == "pp_ifdef" || $token_type == "pp_ifndef"} {
            incr num_if
        }
    }
}
