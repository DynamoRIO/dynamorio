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

# borrowed from Lina
#
use Date::EzDate;
use Time::Local;

my $build;
my $output;
my $hostname;
my %Totals;
my $titles;
my $output_tag;
my $TimeNow;
my $LastBoot;
my $TimeToBoot;
my $ScriptPath;
my $OutputPath;

if (($#ARGV+1)  < 1 ) {
    print " Usage : <path to dir boot_data>\n";
    exit;
}


$ScriptPath=$ARGV[0];
print "path=$ARGV[0]\n";

# first get boot time
$TimeToBoot=&GetLastBootTime();

$OutputPath = "Z:\\desktop-benchmarks\\results\\boot_data";
&GetNetworkDrive();

&GetBuild();
&CollectData();

#--------------------------------------------------------------------------

sub GetNetworkDrive {
    if (-e $OutputPath) {
        # use it
        return;
    }
    else {
        system("net use Z: \/del");
        system("net use Z: \\\\10.1.5.85\\g_shares \"\" /USER:guest");
    }
}

sub CollectData {} {
        &GetDate();
        &GetHost();
        &go();
        &PrintTotals();
        system("del $ScriptPath\\$output");
        system("del $ScriptPath\\build.out");
        system("del $ScriptPath\\OS.out");
        system("del $ScriptPath\\host.out");
        system("del $ScriptPath\\lastboot.txt");
}

sub GetBuild() {
        system("drview -listdr > $ScriptPath\\build.out");
        open(IN,"$ScriptPath\\build.out") or die "Cannot find the build #\n";
        while( $line=<IN> ) {
                if ($line =~ /found/i ) {
                   $build="Native";
                   print "NATIVE!!!!!!\n";
                   return;
                }

                $line=~ s/\)//g;
                my @build=split(' ',$line);
                $build="DR_". $build[$#build];
                last;
        }
}


sub GetDate() {
        my $mydate = Date::EzDate->new();
        $mydate->set_format('myformat', '{monthnumbase1}{day of month}{yearshort}{hour}{min}{sec}');
        $mytoday=$mydate->{'myformat'};
        $output=$build . "_" . $mytoday . "\.txt";  ###$output="jnk";
        $output_tag=$build . "_" . $mytoday;
        print "your output file ==>> $output\n";
}

sub GetHost() {
        system("hostname > $ScriptPath\\host.out");
        system("ver > $ScriptPath\\OS.out"); #get the OS

        open(IN,"$ScriptPath\\host.out") or die "Cannot find the build #\n";
        $hostname=<IN>;
        chomp;
        s/\n//g;
        #print "host22 = $hostname";
        close IN;

        open(IN,"$ScriptPath\\OS.out") or die "Cannot find the build #\n";
        $line=<IN>;
        while ($line=<IN>) {
                print "line= $line";
                if ($line =~ /microsoft/gi) {
                        if ($line =~ /XP/gi) { $myos= "xp_"; last;}
                        elsif ($line =~ /2000/ ) {$myos = "2k_"; last}
                        else { $myos = "2k3_";last}
                }
        }


        my $name= $myos . $hostname;
        $hostname=$name;
        print "host = $hostname";

        system("mkdir $OutputPath\\$hostname");
}

sub go() {
        system("drview -listall -showmem > $ScriptPath\\$output");
        system("copy $ScriptPath\\$output $OutputPath\\$hostname");
        close IN;
}

sub GetTotal(){
        open(IN,"<$ScriptPath\\$output");
        chomp(my $committed = <IN>);
        $committed =~ s/.*KB.: //;
        $line=<IN>; #dont need second line
        $titles_tmp=<IN>;
        $titles_tmp=~ s/\s+/,/g;
        @titles=split(',',$titles_tmp);

        while ($line=<IN>) {

                $line=~ s/\s+/,/g;
                #print "line1= $line \n";
                $line=~ s/%//g;

                #print "line= $line, count=$#titles\n";
                @line2=split(',',$line);                #print "line2= @line2\n";
                for ($i=4; $i < ($#titles+1);$i++) {
                        $Totals{$titles[$i]}+=$line2[$i];
                }
        }
        $Totals{"Committed"} = $committed;
        close IN;
}

sub PrintTotals() {
        $hostname=~s/\n//g;
        my $sumfile="Boot_Summary_" . $hostname. "\.txt";
        my $fullpath="$OutputPath\\$hostname\\$sumfile";
        print "Summary= $sumfile\n";
        my @arr;
        format OUT=
@>>>>>>> @>>>>>>> @>>>>>>> @>>>>>>> @>>>>>>> @>>>>>>> @>>>>>>> @>>>>>>> @>>>>>>> @>>>>>>> @>>>>>>> @>>>>>>> @>>>>>>> @>>>>>>> @>>>>>>> @>>>>>>> @>>>>>>> @>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
        $Bld,$Totals{"CPU"},$Totals{"User"} ,$Totals{"Hndl"},$Totals{"Thr"},$Totals{"PVSz"},$Totals{"VSz"},$Totals{"PPriv"},$Totals{"Priv"}$Totals{"PWSS"},$Totals{"WSS"},$Totals{"Fault"},$Totals{"PPage"},$Totals{"Page"},$Totals{"PNonP"},$Totals{"NonP"},$Totals{"BootTime"},$Totals{"Committed"}
.


        if ( -e $fullpath) {
                open(OUT,">>$fullpath") or die "cannot open existing summary: $!\n";
        }
        else {
                print "Cannot find a summary file starting a new one..\n";
                open(OUT,">$fullpath") or die "cannot open a new summary: $!\n";
                my $j=1;
                $Bld="Build";
                $Totals{"CPU"}="CPU";
                $Totals{"User"}="User";
                $Totals{"Hndl"}="Hndl";
                $Totals{"Thr"}="Thr";
                $Totals{"PVSz"}="PVSz";
                $Totals{"VSz"}="VSz";
                $Totals{"PPriv"}="PPriv";
                $Totals{"Priv"}="Priv";
                $Totals{"PWSS"}="PWSS";
                $Totals{"WSS"}="WSS";
                $Totals{"Fault"}="Fault";
                $Totals{"PPage"}="PPage";
                $Totals{"Page"}="Page";
                $Totals{"PNonP"}="PNonP";
                $Totals{"NonP"}="NonP";
                $Totals{"BootTime"}="BootTime";
                $Totals{"Committed"}="Commit Charge";
                write(OUT);

        }

        &GetTotal();
        $Bld=$build;
        $Totals{"BootTime"}=$TimeToBoot;
        write(OUT);
}

sub GetLastBootTime(){
        system("cscript $ScriptPath\\uptime.vbs > $ScriptPath\\lastboot.txt");
        open(IN,"<$ScriptPath\\lastboot.txt") or die "Cannot open sys time file:$!\n";
        while ($line=<IN>) {
                if ($line =~ /uptime in seconds/i) {
                        print "line=$line \n";
                        @arr=split(':',$line); #ex:Uptime in seconds: 15764
                        close IN;
                        return($arr[1]);

                }
        }
}
