# **********************************************************
# Copyright (c) 2006 VMware, Inc.  All rights reserved.
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

# measure_reset.pl
#
# NOTE: ASSUMES drcontrol and drkill are in path
# NOTE: USES: getmeminfo_automate.pl
#
# TODO: has hard coded OutputPath

my $prog = "measure_reset.pl";

if (($#ARGV+1)  < 1 ) {
    print "Usage : $prog <path to dir boot_data>\n";
    exit;
}

$ScriptPath=$ARGV[0];

my $BOOTWAIT = 600;
my $WAITFOR  = 300;

waitfor($BOOTWAIT);

nudge_reset("core-srv\n");

waitfor($WAITFOR);

launch_ie("10-IE-windows-1-proc\n","", 10);
nudge_reset("srv-10-IE-windows-1-proc\n");

# do some work after reset
waitfor($WAITFOR);
launch_ie("2-more-IE-windows\n", "", 2);

print "$prog: killing IE\n";
system("drkill -exe iexplore.exe");
waitfor($WAITFOR);


launch_ie("10-IE-procs\n","-mp", 10);
nudge_reset("srv-10-IE-procs\n");

# do some work after reset, some process get it
waitfor($WAITFOR);
launch_ie("2-more-IE-windows\n", "", 2);

print "$prog: killing IE\n";
system("drkill -exe iexplore.exe");
waitfor($WAITFOR);


system("perl $ScriptPath\\measure_word.pl $ScriptPath 3 0");
nudge_reset("srv-3-word-docs\n");

# do some work after reset
waitfor($WAITFOR);
system("perl $ScriptPath\\measure_word.pl $ScriptPath 0 1");

print "$prog: killing word\n";
system("drkill -exe winword.exe");
waitfor($WAITFOR);
getmem("after-kill-word\n");




#--------------------------------------------------------------------

sub launch_ie {
    my $text = shift;
    my $arg1 = shift;
    my $arg2 = shift;

    getmem("before-$text");
    print "$prog: launching $text..\n";
    system("perl $ScriptPath\\ie\\launch-ie.pl $arg1 $arg2");
    waitfor($WAITFOR);
    getmem("after-$text");
}

sub nudge_reset {
    my $text = shift;

    getmem("before-reset-$text");
    print "$prog: nudging to reset $text..\n";
    system("drcontrol -hot_patch_nudge_all");
    waitfor($WAITFOR);
    getmem("after-reset-$text");
}

sub waitfor {
    my $waitfor = shift;
    print "$prog: Waiting $waitfor secs..\n";
    sleep($waitfor);
}

sub getmem {
    my $text = shift;
    write_to_summary($text);
    system("perl.exe $ScriptPath\\getmeminfo_automate.pl $ScriptPath");
}

sub write_to_summary {
    my $text = shift;

    my $OutputPath = "Z:\\desktop-benchmarks\\results\\boot_data";
    my $hostname = "xp_qatest";  #TODO: fix this
    my $sumfile="Boot_Summary_$hostname.txt";
    my $fullpath="$OutputPath\\$hostname\\$sumfile";

    open(OUT,">>$fullpath") or die "$prog: Cannot find $fullpath: $!";
    print OUT "$text";
    close OUT;
}
