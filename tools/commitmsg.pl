#!/usr/bin/perl

# **********************************************************
# Copyright (c) 2004-2006 VMware, Inc.  All rights reserved.
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

### commitmsg.pl
### author: Derek Bruening   March 2004
###
### Commits files with per-file messages listed in a control file.
### The control file format is arbitrary and of my personal
### preference, of course :)
###
### Each entry has the format:
###
###   [subdir/]<filename>:
###   message line 1
###   message line 2
###   must end with a dot on a line by itself
###   .
###
### Stops parsing the file when it sees a line beginning w/ "=====", so
### put global comments at the bottom -- and w/ the new "make diff*" targets
### you should put a toemail line there as well and use the file as your
### diff notes file.
###
### Here's a real-world example:
###
###   Makefile:
###   + STANDALONE_UNIT_TEST support for building executable rather than library
###   .
###   dynamo.c:
###   + STANDALONE_UNIT_TEST support by sharing standalone_init() w/
###   CLIENT_INTERFACE and augmenting it to support log files
###   .
###   globals.h:
###   + STANDALONE_UNIT_TEST support in export of standalone usage
###   .
###   vmareas.c:
###   fixed vector split/merge bugs by redoing split code
###
###   + VMAREAS_UNIT_TEST vector add/merge/split/delete tests
###   .
###   linux/os.c:
###   no _init() or _fini() for STANDALONE_UNIT_TEST
###   .
###   ===========================================================================
###   toemail:
###   Two items in this diff:
###
###   1) Fixes case 3415
###
###   2) Adds support for easily building a standalone core unit test (case 3136)
###
###   To build the vmareas vector test, add this to Makefile.mydefines:
###
###   STANDALONE_UNIT_TEST=1
###   ADD_DEFINES += $(D)VMAREAS_UNIT_TEST
###
###   Now the make default target will build ../build/x86_linux_dbg/drtest
###   (will add .exe suffix on windows).  Could copy that somewhere more
###   convenient but I didn't bother.
###
###   I added support for logfiles for easier debugging.

$usage = "Usage: $0 [-v] [-n] [-q] <msgfile>\n";
$verbose = 0;
$nop = 0;
$interactive = 1;

# cannot commit until reviewed!
$reviewed = 0;

# just assume these temp file names are ok
$cvs_msgfile = "_cvs_msg";
$cvs_output = "_cvs_out";

$verbose = 0;
while ($#ARGV >= 0) {
    if ($ARGV[0] eq "-v") {
        $verbose = 1;
    } elsif ($ARGV[0] eq "-n") {
        $nop = 1;
    } elsif ($ARGV[0] eq "-q") {
        $interactive = 0;
    } else {
        $msgfile = $ARGV[0];
    }
    shift;
}
if (!defined($msgfile)) {
    print $usage;
    exit;
}

# first ensure that we're up-to-date
# look for anything other than these results from cvs status:
# Up-to-date
# Locally {Modified,Added,Removed}
#
$uptodate = system("cvs status | grep Status | grep -vE 'Up-to-date|Locally|File had conflicts'");
if (!$uptodate) {
    print "Not up to date!  Merge, re-test, and try again\n";
    exit -1;
}

open(MSGFILE, "< $msgfile") || die "Error opening $msgfile\n";
$vers = "";

while (<MSGFILE>) {
    if (!$in_msg) {
        if (/^reviewed by/i) {
            $reviewed = 1;
            $reviewer = $_;
        } elsif (/(\S+):$/) {
            if (!$reviewed) {
                die "Cannot commit until reviewed!\n";
            }
            $file = $1;
            $in_msg = 1;
            $msg = "";
            next;
        } elsif (/^=====/) {
            last;
        } else {
            print "Ignoring line: $_" unless /^$/;
        }
    } else {
        if (/^\.$/) {
            open(MSG, "> $cvs_msgfile") || die "Couldn't open $cvs_msgfile\n";
            print MSG $msg;
            close(MSG);
            print "\n-----------------------------\n";
            print "message for $file is: \n$msg\n";
            if ($interactive) {
                print "hit <enter> to continue, ^C to abort\n";
                <STDIN>;
            }
            if ($nop) {
                $vers .= "$file:<NOT COMMITTED>\n";
            } else {
                print "cvs commit -F $cvs_msgfile $file\n";
                # record output so we can print a version summary at the end

                # somehow pipe ends up sitting there for ~40 seconds so we use a tmp file
                #open(COM, "cvs commit -F $cvs_msgfile $file 2>&1 |") || die "Couldn't run cvs commit\n";
                system("cvs commit -F $cvs_msgfile $file 2>&1 > $cvs_output");
                open(COM, "< $cvs_output") || die "Couldn't open cvs output $cvs_output\n";
                while (<COM>) {
                    # look for:
                    #   /home/dr/cvsroot/src/win32/os_exports.h,v  <--  os_exports.h
                    #   new revision: 1.99; previous revision: 1.98
                    # or for:
                    #   RCS file: /home/dr/cvsroot/src/win32/os_private.h,v
                    #   /home/dr/cvsroot/src/win32/os_private.h,v  <--  os_private.h
                    #   initial revision: 1.1
                    if (m|/cvs[^/]*/[^/]+/([^,]+)| && !/^RCS file/) {
                        $vers .= "$1:";
                    } elsif (/^\w+ revision: ([\d.]+)/) {
                        $vers .= "$1\n";
                    }
                    print $_;
                }
                close(COM);
            }

            $in_msg = 0;
            $msg = "";
        } else {
            $msg .= $_;
        }
    }
}
close(MSGFILE);

print "\n-----------------------------\n";
print "Summary:\n\n${reviewer}\nCommitted:\n${vers}\n";

# finally, ensure that we didn't miss any files
# look for these results from cvs status:
# Locally {Modified,Added,Removed}
#
$alldone = system("cvs status | grep Status | grep Locally");
if (!$alldone) {
    print "Did you forget to commit these files?\n";
    exit -1;
}
