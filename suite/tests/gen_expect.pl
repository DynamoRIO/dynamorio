#!/usr/local/bin/perl

# **********************************************************
# Copyright (c) 2003-2008 VMware, Inc.  All rights reserved.
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

### gen_expect.pl 
### this file reads in an expect file template for a unit test and 
### generates and expect file based on the Compile defines and options
### passed in 

### template file format
### name = <test_name>.template
### #if : #else #endif #stop #sec_vio_cont #sec_vio_stop

$sec_violation_con = "";
$sec_violation_stop = "";

$usage = "Usage: gen_expect <build_defines> <run_options> <template_file>\n";

$verbose = 0;
    
print stderr "$#ARGV\n" if ($verbose);
print stderr "$ARGV[0]\n" if ($verbose);
print stderr "$ARGV[1]\n" if ($verbose);
print stderr "$ARGV[2]\n" if ($verbose);

if ($#ARGV != 2) {
    print "$usage";
    exit 0;
}

$build_defs = $ARGV[0];
$run_ops = $ARGV[1];
$file = $ARGV[2];

open(IN, "<$file") || print stderr "Error : couldn't open template file $file \n";
$outfile = $file;
$outfile =~ s/\.template$/\.expect/;
open(OUT, ">$outfile") || print stderr "Error : couldn't open expect file $file for generation \n";

$yes = "y";
$no = "n";
$ignore = "i";

$copy_mode = $yes;
$nesting_depth = 0;

$win32 = 0;
if ($build_defs =~ /WINDOWS/) {
    $win32 = 1;

    # Figure out the windows version and add the corresponding
    # RUNREGRESSION_ define to build_defs.  We'll use 'cmd /c' to get
    # the cmd shell version, which should match the OS version.
    # Launching a shell command from perl is slightly problematic if
    # we want this script to work with both the windows cmd shell and
    # cygwin.  Cygwin wants /'s in the path and cmd shell wants \'s.
    # Let's assume cmd is always in the path and omit the full path.
    $verout = `cmd /c ver`;
    if ($verout =~ /Windows NT Version 4\.0/) {
        $winver = "NT";
    } elsif ($verout =~ /Microsoft Windows 2000/) {
        $winver = "2000";
    } elsif ($verout =~ /Microsoft Windows XP/) {
        $winver = "XP";
    } elsif ($verout =~ /Microsoft Windows \[Version 5\.2\..+\]/) {
        $winver = "2003";
    } elsif ($verout =~ /Microsoft Windows \[Version 6\.0\..+\]/) {
        $winver = "Vista";
    } else {
        die "Unknown Windows version\n";
    }

    $build_defs .= "RUNREGRESSION_$winver ";
}

$throw_exception = 0;
if ($run_ops =~ /-throw_exception/) {
    $throw_exception = 1;
}
$kill_thread = 0;
if ($run_ops =~ /-kill_thread/) {
    $kill_thread = 1;
}

while (<IN>) {
    chop;
    # handle dos end of line
    if ($_ =~ /\r$/) { chop; };
    $line = $_;

    if ($line =~ /^#\s*end/) {
        $nesting_depth -= 1;
        chop $copy_mode;
        print stderr "new nesting depth " . $nesting_depth . "\n" if ($verbose);
        print stderr "copy mode " . $copy_mode . "\n" if ($verbose);
    } elsif ($line =~ /^#\s*else/) {
        $copy_mode =~ /(.*)(.)$/;
        if ($2 eq $no) {
            $copy_mode = $1;
            $copy_mode .= $yes;
        } elsif ($2 eq $yes) {
            $copy_mode = $1;
            $copy_mode .= $no;
        }
        print stderr "hit else copy mode " . $copy_mode . "\n" if ($verbose);
    } elsif ($line =~ /^#\s*if(.*)/) {
        @expr = split(/ /, $1);
        $check_in = $build_defs;
        $nesting_depth += 1;
        print stderr "new nesting depth " . $nesting_depth . "\n" if ($verbose);
        if ($copy_mode =~ /.*$yes$/) {
            $success = $yes;
            $index = 1;
            while ($index < @expr) {
                print "token is $expr[$index]\n" if ($verbose);
                if ($check_in ne $run_ops && $expr[$index] =~ /\:/) {
                    $check_in = $run_ops;
                } elsif ($expr[$index] =~ /\|\|/) {
                    print stderr "found an or, success now is $success\n" if ($verbose);
                    # primitive "or" feature: no composability, only single top-level or
                    if ($success eq $yes) {
                        # short-circuit: ignore other side of the or
                        last;
                    } else {
                        # start over
                        $success = $yes;
                        $check_in = $build_defs;
                    }
                } elsif ($expr[$index] =~ /\+(.*)/) {
                    $tok = $1;
                    # allow for parentheses to avoid spaces around option arguments
                    # replace ( with a space, and remove final )
                    if ($tok =~ /(.*)\((.*)\)/) {
                        $tok = "$1 $2";
                    }
                    if (not($check_in =~ /$tok/)) {
                        print "BAD: required token $tok missing\n" if ($verbose);
                        $success = $no;
                    }
                } elsif ($expr[$index] =~ /\-(.*)/) {
                    $tok = $1;
                    # allow for parentheses to avoid spaces around option arguments
                    # replace ( with a space, and remove final )
                    if ($tok =~ /(.*)\((.*)\)/) {
                        $tok = "$1 $2";
                    }
                    if ($check_in =~ /$tok/) {
                        print "BAD: unwanted token $tok present\n" if ($verbose);
                        $success = $no;
                    }
                } elsif ($expr[$index] !~ /^\s*$/) {
                    # don't leave partial file
                    # FIXME: any cleaner way?  make doesn't auto-remove since
                    # we didn't succeed, and make aborts before any other make
                    # cmds can come into play.
                    close(OUT);
                    unlink $outfile;
                    die "Bad format of template file: \"$expr[$index]\" on line \"$line\"\n";
                }
                $index += 1;
            }
            print "success is now $success\n" if ($verbose);
            if ($success eq $yes) {
                $copy_mode .= $yes;
            } else {
                $copy_mode .= $no;
            }
        } else {
            $copy_mode .= $ignore;
        }
        print stderr "copy mode " . $copy_mode . "\n" if ($verbose);
    } else {
        if ($copy_mode =~ /.*$yes$/) {
            $VIOLATION = "<Execution security violation was intercepted!\nContact your vendor for a security vulnerability fix.\n";
            $CONTINUE_MSG = "Program continuing!";
            $TERMINATE_MSG = "Program terminated.";
            $THREAD_MSG = "Program continuing after terminating thread.";
            $THROW_MSG = "Program continuing after throwing an exception.";
            $MEMORY_MSG = "Out of memory.  Program aborted.";
            if ($line =~ /^#\s*SEC_VIO_CONT/) {
                print OUT $VIOLATION,$CONTINUE_MSG,">\n";
            } elsif ($line =~ /^#\s*SEC_VIO_STOP/) {
                print OUT $VIOLATION,$TERMINATE_MSG,">\n";
            } elsif ($line =~ /^#\s*SEC_VIO_EXCEPTION/) {
                print OUT $VIOLATION,$THROW_MSG,">\n";
            } elsif ($line =~ /^#\s*SEC_VIO_THREAD/) {
                print OUT $VIOLATION,$THREAD_MSG,">\n";
            } elsif ($line =~ /^#\s*OUT_OF_MEMORY/) {
                print OUT "<$MEMORY_MSG>\n";
            } elsif ($line =~ /^#\s*UNHANDLED_EXCEPTION/) {
                if ($win32) {
                   print OUT $VIOLATION,"Unhandled exception caught.\n";
                }
            } elsif ($line =~ /^#\s*SEC_VIO_AUTO_STOP/) {
                if ($throw_exception) {
                    print OUT $VIOLATION,$THROW_MSG,">\n";
                    if ($win32) {
                        print OUT "Unhandled exception caught.\n";
                    }
                } elsif ($kill_thread) {
                    print OUT $VIOLATION,$THREAD_MSG,">\n";
                } else {
                    print OUT $VIOLATION,$TERMINATE_MSG,">\n";
                }
            } elsif ($line =~ /^#\s*STOP/) {
                close(IN);
                close(OUT);
                exit();
            } elsif ($line =~ /^\/\//) {
                # comment!
            } else {
                print OUT "$line\n";
            }
        }
    }
}

close(IN);
close(OUT); 
