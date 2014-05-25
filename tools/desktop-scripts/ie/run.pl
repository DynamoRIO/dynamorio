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


# perl script

my $prog = "run.pl";

if  (($#ARGV+1)  < 1 ) {
	print "Usage : <path to dir boot_data>\n";
	exit;
}

$ScriptPath=$ARGV[0];
print "$prog: path=$ARGV[0]\n";

#system("net use Z: \/del");
#system("net use Z: \\\\10.1.5.85\\g_shares \"\" /USER:guest");

doit("");
doit("-mp");

#-------------------------------------------------------------------------
sub doit {
    my $mp = shift;
    my $getmeminfo = "perl $ScriptPath\\getmeminfo_automate.pl $ScriptPath";
    my $launch_ie = "perl $ScriptPath\\ie\\launch-ie.pl";
    my $procs = "10-processes";
    $_ = $mp;
    $procs = "windows-1-process" unless (m/-mp/i);

    for (my $i = 0; $i < 3; $i++) {
	write_to_summary("before-launching-10-IE-$procs\n");
	system("$getmeminfo");
	system("$launch_ie $mp");
	print "$prog: sleeping 120 secs..\n";
	sleep(120);
	write_to_summary("after-launching-10-IE-$procs\n");
	system("$getmeminfo");
	print "$prog: Killing IE windows\n";
	system("drkill -exe iexplore.exe");
	print "$prog: sleeping 120 secs..\n";
	sleep(120);
    }
    write_to_summary("--------- end of test run ----------\n");
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
