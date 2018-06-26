#!/usr/local/bin/perl

# **********************************************************
# Copyright (c) 2012-2018 Google, Inc.  All rights reserved.
# Copyright (c) 2002-2010 VMware, Inc.  All rights reserved.
# Copyright (c) 2018 Arm Limited         All rights reserved.
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

### genapi.pl
###
### generates header files and filter files for our exported data and routines
### N.B.: this script assumes several things about the layout of declarations:
###   1) comments are not sprinkled in weird places
###   2) multiple declarations do not share the same line
###

$usage = "Usage: $0 [-core <core dir>] -header <destinationdir> <defines> | -filter <defines> | -debug\n";

$header = 0;
$debug = 0;
$filter = 0;
$core = ".";

if ($#ARGV < 0) {
    print "$usage";
    exit 1;
}

while ($#ARGV >= 0) {
    if ($ARGV[0] eq '-core') {
        # Take the core source dir path on the command line.
        shift;
        if ($#ARGV < 1) {
            print "$usage";
            print "Expected path after -core\n";
            exit 1;
        }
        $core = shift;
        if (! -d $core) {
            print "$core is not a directory\n";
            print "$usage";
            exit 1;
        }
    }
    if ($ARGV[0] eq '-header') {
        $header = 1;
        shift;
        if ($#ARGV != 1) {
            print "$usage";
            print "Incorrect number of arguments for -header\n";
            exit 1;
        }
        $dir = $ARGV[0];
        if (! -d $dir) {
            print "$dir is not a directory\n";
            print "$usage";
            exit 1;
        }
        shift;
        $define_str = $ARGV[0];
        $define_str =~ s/^[-\/]D//;
        $define_str =~ s/ [-\/]D/ /g;
        @define_els = split(' ', $define_str);
        foreach $i (@define_els) {
            $defines{$i} = 1;
        }
    } elsif ($ARGV[0] eq '-filter') {
        $filter = 1;
        shift;
        $define_str = $ARGV[0];
        $define_str =~ s/^[-\/]D//;
        $define_str =~ s/ [-\/]D/ /g;
        @define_els = split(' ', $define_str);
        foreach $i (@define_els) {
            $defines{$i} = 1;
        }
    } elsif ($ARGV[0] eq '-debug') {
        $debug = 1;
    } else {
        print "$usage";
        print "Unrecognized argument: $ARGV[0]\n";
        exit 1;
    }
    shift;
}

# I was using File::Copy but native-windows perl
# "v5.8.8 built for MSWin32-x86-multi-thread" maintains modtime, and
# even doing utime() after the copy() wasn't updating.
# With the old time we have a rebuild of header files on every make.
# So just doing my own copy rather than waste any more time on
# perl variant issues.  This gives us consistent line endings with
# the generated files, as well.
sub copy_file
{
    my ($src, $dst) = @_;
    open(SRC, "< $src") || die "Error: Couldn't open $src for copy\n";
    open(DST, "> $dst") || die "Error: Couldn't open $dst for copy\n";
    while (<SRC>) {
        print DST $_;
    }
    close(SRC);
    close(DST);
}

if ($header) {
    # clean up existing files first
    # since the set of files we're creating is dynamic it's not clear how
    # we can avoid this auto-clean and have something nicer
    @existing = glob("$dir/dr_*.h");
    if ($#existing >= 0) {
        # dr_api.h is now created at configure time
        foreach $index (0 .. $#existing) {
            if ("$existing[$index]" eq "$dir/dr_api.h") {
                delete $existing[$index];
            }
        }
        if ($#existing >= 0) {
            unlink(@existing);
        }
    }

    if (defined($defines{"CLIENT_INTERFACE"})) {
        # dr_api.h is copied by top-level CMakeLists.txt, for easier
        # substitution of VERSION_NUMBER_INTEGER

        # dr_app.h is copied verbatim
        # We used to have #ifdefs (LOGPC I think) in the func declarations
        # and we had a complex series of commands in core/Makefile to strip
        # them out while leaving the ifdefs at the top of the file.
        copy_file("$core/lib/dr_app.h", "$dir/dr_app.h");

    } else {
        if (!defined($defines{"APP_EXPORTS"})) {
            die "Should not be invoked w/o APP_EXPORTS or CLIENT_INTERFACE\n";
        }
        # dr_app.h is copied verbatim
        copy_file("$core/lib/dr_app.h", "$dir/dr_app.h");
    }
}

$arch = (defined($defines{"AARCH64"}) ? "aarch64" :
         defined($defines{"ARM"}) ? "arm" : "x86");

# I used to just do
#   open(FIND, "find . -name \\*.h |")
# but there are dependencies between header files, and certain orders
# look nicer than others, so we have an explicit list here:
# Also, we now send output to multiple files, and their names are indicated
# by comments in the header files.
@headers =
    (
     "$core/arch/instrlist.h",
     "$core/lib/globals_shared.h", # defs
     "$core/lib/c_defines.h", # defs
     "$core/globals.h",
     "$core/arch/arch_exports.h", # encode routines
     "$core/arch/proc.h",
     "$core/os_shared.h",        # before instrument_api.h
     "$core/module_shared.h",    # before instrument_api.h
     "$core/lib/instrument_api.h",
     "$core/arch/x86/opcode.h",
     "$core/arch/arm/opcode.h",
     "$core/arch/opnd.h",
     "$core/arch/instr.h",
     "$core/arch/instr_inline.h",
     "$core/arch/instr_create_shared.h",
     "$core/arch/x86/instr_create.h",
     "$core/arch/aarch64/instr_create.h",
     "$core/arch/arm/instr_create.h",
     "$core/arch/decode.h",       # OPSZ_ consts, decode routines
     "$core/arch/decode_fast.h",  # decode routines
     "$core/arch/disassemble.h",  # disassemble routines
     "$core/fragment.h",         # binary tracedump format
     "$core/win32/os_private.h", # rsrc section walking
     "$core/hotpatch.c",         # probe api
     "$core/lib/dr_config.h",
     "$core/lib/dr_inject.h",
     "$core/../libutil/dr_frontend.h",
     );

if (defined($defines{"ANNOTATIONS"})) {
    push(@headers, "$core/annotations.h");
}

# AArch64's opcode.h is auto-generated. We expect $dir point to a directory
# one level deep in the build directory.
if (defined($defines{"AARCH64"})) {
    push(@headers, "$dir/../opcode.h");
}

# PR 214947: VMware retroactively holds the copyright.
$copyright = q+/* **********************************************************
 * Copyright (c) 2010-2011 Google, Inc.  All rights reserved.
 * Copyright (c) 2002-2010 VMware, Inc.  All rights reserved.
 * **********************************************************/

/*
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * * Redistributions of source code must retain the above copyright notice,
 *   this list of conditions and the following disclaimer.
 *
 * * Redistributions in binary form must reproduce the above copyright notice,
 *   this list of conditions and the following disclaimer in the documentation
 *   and/or other materials provided with the distribution.
 *
 * * Neither the name of VMware, Inc. nor the names of its contributors may be
 *   used to endorse or promote products derived from this software without
 *   specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL VMWARE, INC. OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

+;

if ($header) {
    $outfile_init = 0;
} else {
    open(OUT, ">-"); # stdout
    if ($filter) {
        # linker version script w/ no versions, just global and local symbol lists
        # except old ld requires version -- ok, we'll put in one
        print OUT "DYNAMORIO_0.9.5 {\nglobal:\n";
    }
}

sub keep_define($)
{
    my ($def) = @_;
    return ($def eq "WINDOWS" || $def eq "LINUX" || $def eq "UNIX" ||
            $def eq "MACOS" || $def eq "X64" || $def eq "CLANG" ||
            $def eq "X86" || $def eq "AARCH64" || $def eq "AARCHXX" || $def eq "ARM" ||
            $def eq "X86_32" || $def eq "X86_64" ||
            $def eq "ANDROID" || $def eq "USE_VISIBILITY_ATTRIBUTES" ||
            $def eq "DR_FAST_IR" || $def eq "__cplusplus" || $def eq "PAGE_SIZE" ||
            $def eq "DR_PAGE_SIZE_COMPATIBILITY" ||
            $def eq "DR_LOG_DEFINE_COMPATIBILITY");
}

foreach $file (@headers) {
    open(IN, "< $file") || die "Error: Couldn't open $file for input\n";
    if ($debug) {
        print stderr "Working on $file\n";
        print OUT "\n/* from $file ----------------------------- */\n";
    }
    $output_routine = 0;
    $output_directly = 0;
    $output_verbatim = 0;
    $did_output_something = 0;
    $prev_define = 0;
    $in_define = 0;
    while (<IN>) {
        # support mcxtx.h include.  can generalize if necessary.
        if ($_ =~ /# *include "mcxtx.h"/) {
            open(INC, "< $core/lib/mcxtx.h") || die "Error: couldn't open mcxtx.h\n";
            my $start_inc = 0;
            while (<INC>) {
                # skip copyright and first comment
                if (/START INCLUDE/) {
                    $start_inc = 1;
                    next;
                }
                next unless ($start_inc);
                &process_header_line($_);
            }
            close(INC);
        } else {
            &process_header_line($_);
        }
    }
    if ($did_output_something && !$filter) {
        print OUT "\n";
    }
    close(IN);
}

if ($header) {
    foreach $fname (keys(%files)) {
        # FIXME: should store original filehandle, and close it here
        open(OUT, ">> $dir/$files{$fname}") ||
            die "Error: Couldn't open $dir/$files{$fname} for append\n";
        print OUT "\n#endif /* $wrapdef{$fname} */\n";
        close(OUT);
    }
    if (!defined($defines{"HOT_PATCHING_INTERFACE"})) {
        # Need to add appropriate ifdefs for the core to get this auto-removed.
        # For now this is the easiest method.
        unlink("$dir/dr_probe.h");
    }
} else {
    if ($filter) {
        if (defined($defines{DR_APP_EXPORTS})) {
            # now add in the APP_API routines -- ok to list symbol not found,
            # so we need no hacks to remove the LOGPC ones based on the defines:
            $app = `grep 'DR_APP_API.*;' ./lib/dr_app.h`;
            $app =~ s/DR_APP_API [a-z]+ ([A-Za-z_]+)\(void\);/\1;/g;
            print OUT $app;
        }
        # dynamorio_app_init and dynamorio_app_take_over are used
        # by the linux injector
        print OUT "dynamorio_app_init;\n";
        print OUT "dynamorio_app_take_over;\n";
        # add in our_std*, which are hard to match w/ DR_API and I'm in a hurry
        # (should switch to visibility attributes now that we upgraded gcc)
        print OUT "our_stdout;\n";
        print OUT "our_stderr;\n";
        # linker version script w/ no versions, just global and local symbol lists
        print OUT "\nlocal: *;\n};\n";
    }
    close(OUT);
}

###########################################################################
sub process_header_line($)
{
    my ($l) = @_;
    chop $l;
    # handle DOS end-of-line:
    if ($l =~ /\r$/) { chop; };

    if ($output_routine || $output_directly || $output_verbatim) {
        # Enforce the rename to DR_REG_ and DR_SEG_
        if (($l =~ /[^_]REG_/ || $l =~ /[^_]SEG_/) &&
            # We have certain exceptions
            ($l !~ /^# *define [RS]EG_/ &&
             $l !~ /DR_REG_ENUM_COMPATIBILITY/ &&
             $l !~ /REG_SPECIFIER_BITS/ &&
             $l !~ /REG_kind/ &&
             $l !~ /conflict/ &&
             $l !~ /compatibility/ &&
             $l !~ /weird errors/)) {
            die "Error: update to DR_{REG,SEG} constants:\n$l\n";
        }
    }

    if ($l =~ /^DR_API/) {
        $output_routine = 1;
        $did_output_something = 1;
        $type = "";
    } elsif ($header && $l =~ /DR_API EXPORT TOFILE ([A-Za-z_0-9\.]+)/) {
        $outfile_init = 1;
        $outfile = $1;
        if (!defined($files{$outfile})) {
            $files{$outfile} = $outfile;
            $wrapdef{$outfile} = $outfile;
            $wrapdef{$outfile} =~ s/\./_/g;
            $wrapdef{$outfile} = "_" . uc($wrapdef{$outfile}) . "_";
            if (! -e "$dir/$outfile") {
                open(OUT, "> $dir/$outfile") ||
                    die "Error: Couldn't open $dir/$outfile for output\n";
                print OUT "$copyright";
                print OUT "#ifndef $wrapdef{$outfile}\n#define $wrapdef{$outfile} 1\n\n";
            } else {
                open(OUT, ">> $dir/$outfile") ||
                    die "Error: Couldn't open $dir/$outfile for append\n";
            }
        } else {
            # FIXME: should store original filehandle and re-use it
            open(OUT, ">> $dir/$outfile") ||
                die "Error: Couldn't open $dir/$outfile for append\n";
        }
    } elsif ($header && $l =~ /DR_API EXPORT BEGIN/) {
        $output_directly = 1;
        $did_output_something = 1;
        $first_pound = 0;
    } elsif ($header && $l =~ /DR_API EXPORT VERBATIM/) {
        $output_verbatim = 1;
        $did_output_something = 1;
    }
    # if outputting verbatim, just output and grab the next line
    # we can skip the following logic that interprets the #ifdefs,
    # #ifndefs, etc.
    elsif ($output_verbatim) {
        if ($l =~ /DR_API EXPORT END/) {
            $output_verbatim = 0;
        }
        else {
            print OUT "$l\n";
        }
        next;
    }

    # to handle defines (yes a hack...need a better solution):
    # only a define on immediately prior line to DR_API or to
    # EXPORT BEGIN will kill following output, until the very next
    # endif
    # assumption: only those defines passed to us count,
    # and they're only used in simple "#ifdef" statements
    # (can't use cpp b/c it also removes #if's around entire .h file,
    # plus we want to keep #ifdef UNIX and #ifdef WINDOWS stuff)
    if ($prev_define && ($output_routine || $output_directly)) {
        $in_undefined = 1;
        if ($debug) {
            print stderr "\tSkipping!\n";
        }
    }
    if ($in_undefined) {
        if ($l =~ /^\#\s*endif/ || $l =~ /^\#\s*else/) {
            $in_undefined = 0;
            if ($l =~ /^\#\s*else/) {
                $kill_endif = 1;
            }
        }
        # assumption: nothing else on endif line
        $skip = 1;
        # have to check for closings here
        if ($output_routine && $l =~ /;/) {
            $output_routine = 0;
        }
        if ($output_directly && $l =~ /DR_API EXPORT END/) {
            $output_directly = 0;
            $shrink_indent = 0;
        }
    } else {
        $skip = 0;
    }
    if ($kill_endif && $l =~ /^\#\s*endif/) {
        $kill_endif = 0;
        $skip = 1;
    }
    if ($in_define || $in_define_keep) {
        if ($l =~ /^\#\s*endif/) {
            # we only support keep-defines being innermost
            if ($in_define_keep) {
                $in_define_keep = 0;
            } else {
                $in_define = 0;
                $skip = 1;
            }
        }
        if ($in_define && $l =~ /^\# +/) {
            # Reduce the indent added inside API_EXPORT_ONLY by clang-format.
            $l =~ s/^\#    /#/;
        }
    }
    if ($output_directly) {
        if (!$first_pound && $l =~ /^\#/) {
            $first_pound = 1;
            if ($l =~ /^\# /) {
                # Reduce the indent inside surrounding CLIENT_INTERFACE by clang-format.
                $shrink_indent = 1;
            }
        }
        if ($shrink_indent) {
            $l =~ s/^\#    /#/;
        }
    }

    if ($l =~ /^\#\s*ifdef (\S+)/ || $l =~ /^\#\s*if defined\((\S+)\)/) {
        # we want to keep all WINDOWS, UNIX, and X64 defines, so ignore those.
        if (&keep_define($1)) {
            $in_define_keep = 1;
        } else {
            if ($debug) {
                print stderr "Found ifdef $1 => $defines{$1}\n";
            }
            if (!defined($defines{$1})) {
                $prev_define = 1;
            } elsif ($output_routine || $output_directly) {
                $in_define = 1;
            }
            $skip = 1;
        }
    } elsif ($l =~ /^\#\s*ifndef (\S+)/) {
        # we want to keep all WINDOWS, UNIX, and X64 defines, so ignore those.
        if (&keep_define($1)) {
            $in_define_keep = 1;
        } else {
            if ($debug) {
                print stderr "Found ifndef $1 => $defines{$1}\n";
            }
            if (defined($defines{$1})) {
                $prev_define = 1;
            } elsif ($output_routine || $output_directly) {
                $in_define = 1;
            }
            $skip = 1;
        }
    } else {
        $prev_define = 0;
    }
    if ($skip) {
        next;
    }

    if ($output_routine) {
        if ($filter) {
            # only export these guys for DYNAMORIO_IR_EXPORTS
            if (defined($defines{DYNAMORIO_IR_EXPORTS})) {
                # symbol export list (-filter option)
                if ($l =~ /^([A-Za-z0-9_]+)\s*\(/ ||
                    # order is important: 2nd line might pick up _IF_X64
                    # inside param list
                    $l =~ /^[a-zA-Z_].*\s+([A-Za-z0-9_]+)\s*\(/) {
                    print OUT "$1;\n";
                }
            }
            if ($l =~ /;\s*$/ &&
                $l !~ /^\s*\*/ && $l !~ /^\s*\/\*/) { # ignore ; inside comment
                $output_routine = 0;
            }
        } else {
            if ($header && !$outfile_init) {
                die "Error: no initial output file specified\n";
            }
            unless ($debug) {
                $l =~ s/^DR_API\s*//;
            }
            if ($type eq "") {
                if ($l =~ /^\s*[A-Za-z_]+/) {
                    $l =~ /^\s*([A-Za-z0-9_]+\s*\*?)/;
                    $type = $1;
                    $type =~ s/\s*$//;
                }
            }
            $l =~ s/dcontext_t *\*dcontext/void *drcontext/;
            if ($l =~ /\);\s*$/) {
                $output_routine = 0;
                if ($debug || $header) {
                    print OUT "$l\n";
                } else {
                    $l =~ s/;//;
                    print OUT "$l\n";
                    if ($type =~ /\*/) {
                        $ret = "\treturn NULL;\n";
                    } elsif ($type eq "void") {
                        $ret = "";
                    } elsif ($type eq "bool") {
                        $ret = "\treturn false;\n";
                    } elsif ($type eq "float") {
                        $ret = "\treturn 0.f;\n";
                    } elsif ($type eq "int" || $type eq "uint") {
                        $ret = "\treturn 0;\n";
                    } else {
                        # static to avoid "uninitialized var" compiler warnings
                        $ret = "\tstatic $type bogus;\n\treturn bogus;\n";
                    }
                    print OUT "{\n$ret}\n";
                }
            } else {
                print OUT "$l\n";
            }
        }
    } elsif ($output_directly) {
        # instr_create.h needs this in its raw output for x64, but keep var name
        $l =~ s/dcontext_t *\*dcontext/void *dcontext/;
        # let's keep blank lines if we didn't create them by stripping
        # everything out
        if ($l =~ /^\s*$/) {
            print OUT "\n";
        }
        elsif ($l !~ /DR_API/) {
            if ($l =~ /OP_/) {
                # remove pointers into decode tables
                $l =~ s/(OP_[a-zA-Z0-9_]*,) *\/\*[^\*]*\*\/(.*)/\1\2/;
            }
            # PR 227381: auto-insert doxygen comments for DR_REG_ enum lines w/o any
            if ($file =~ "/opnd.h" &&
                $l =~ /^ *DR_[RS]EG_/ && $l !~ /\/\*\*</) {
                $l =~ s|(DR_[RS]EG_)(\w+), *|\1\2, /**< The "\L\2" register. */\n    |g;
            }
            # Strip out FIXME comments
            $l =~ s/\/\* FIXME.*\*\///;
            if ($l !~ /^\s*$/) {
                print OUT "$l\n";
            }
        }
        if ($l =~ /DR_API EXPORT END/) {
            $output_directly = 0;
            print OUT "\n";
        }
    }
}
