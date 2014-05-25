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

# NOTE: ASSUMES word and drcontrol are in the path
# NOTE: USES: getmeminfo_automate.pl
# NOTE: ASSUME hardcoded locations for $masterdoc and $bmarkdoc:
#       $masterdoc = "c:\\desktop-benchmarks\\derek-win32-bmarks\\master.doc";
#       $bmarkdoc = "c:\\desktop-benchmarks\\derek-win32-bmarks\\master-bmark-noclose.doc"; # FIXME YUCK!
# FIXME: has hard coded OutputPath


my $prog = "measure-word.pl";
my $app  = "winword.exe";

if (($#ARGV+1)  < 1 ) {
    print "Usage : $prog <path to dir boot_data> [3 num word] [1 do-bmark]\n";
    exit;
}

my $NUM_WORD;
my $do_bmark;

($ScriptPath, $NUM_WORD, $do_bmark)=($ARGV[0], $ARGV[1], $ARGV[2]);

$NUM_WORD = 3 unless (defined $NUM_WORD);
$do_bmark = 0 unless (defined $do_bmark);

my $masterdoc = "c:\\desktop-benchmarks\\derek-win32-bmarks\\master.doc";
my $bmarkdoc  = "c:\\desktop-benchmarks\\derek-win32-bmarks\\master-bmark-noclose.doc";

if ($NUM_WORD > 0) {
    write_to_summary("before-$NUM_WORD-word-docs\n");
    system("perl.exe $ScriptPath\\getmeminfo_automate.pl $ScriptPath");

    for (my $i = 1; $i < $NUM_WORD+1; $i++) {
        system("start $app $masterdoc");
        print "$prog: sleeping 60 secs\n";
        sleep(60);
        write_to_summary("after-$i-word\n");
        system("perl.exe $ScriptPath\\getmeminfo_automate.pl $ScriptPath");
    }

    print "$prog: sleeping 300 secs\n";
    sleep(300);
}

if ($do_bmark) {
    write_to_summary("before-1-bmark\n");
    system("perl.exe $ScriptPath\\getmeminfo_automate.pl $ScriptPath");
    system("start $app $bmarkdoc");
    print "$prog: sleeping 300 secs\n";
    sleep(300);
    write_to_summary("after-1-bmark\n");
    system("perl.exe $ScriptPath\\getmeminfo_automate.pl $ScriptPath");
}

#--------------------------------------------------------------------

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
