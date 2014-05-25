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

use Date::EzDate;
use Time::Local;

my $build;
my $output;
my $hostname;
my %Totals;
my $titles;
my $output_tag;
my $ScriptPath;
my $sumfile;
my $fullpath;
my %Slist='';
my %DrView;
my %STAT; #holds the current exe status in DR or native?
my $OutputPath;
$ScriptPath=$ARGV[0];

system("drcontrol -fulldump > $ScriptPath\\drcontrol_full.txt");


&GetBuild();

system("del \*\.parsed");
if ($build  =~ /native/i) {
        print "Running native, nothing to parse..\n";
        exit;
}

#system("net use Z: \/del");
#system("net use Z: \\\\10.1.5.85\\g_shares \"\" /USER:guest");
$OutputPath = "Z:\\desktop-benchmarks\\results\\boot_data";
(-e $OutputPath) or die "Can't find $OutputPath\n";


&GetDate();
&GetHost();
&go();


#this section added to identify which services are in DR which are not
&CreateSlist();
&ParseDrview();
&Compare();

#-------------------------------------------------------------------------

my $AUTOINJECT=0;
sub GetBuild() {
        my $DR=0;
        open IN,"< $ScriptPath\\drcontrol_full.txt";
        while(<IN>) {
                $line=$_;
                print "reading: $line";
                if ($line =~ /AUTOINJECT/i ) {  #DYNAMORIO_AUTOINJECT=c:\builds\25093\debug\dynamorio.dll
                        print "FOUND Autoinject.........\n";
                        $line=~ s/\\+/}/g;
                        @tmp=split('}',$line);
                        for ($i=0; $i < $#tmp ; $i++) {
                                if ($tmp[$i] =~ /^\d\d\d\d./) {
                                        $build2=$tmp[$i];
                                        $build="DR_". $build2;
                                        $AUTOINJECT=1;
                                        $i=$#tmp;
                                }
                        }

                }
                if ($line =~ /Config Group:/gi) {
                        print "Found a registry entry: $line\n";
                        if ($line =~ /\.exe/) {
                                $DR=1;
                                last;
                        }

                }
         }
         if  ((!$DR) | (!$AUTOINJECT))  {
                print "Running native........\n";
                 system("$ScriptPath\\psservice.exe > $ScriptPath\\native.txt");
                 $build="Native";
        }
        print "build=$build....\n";
}


sub GetDate() {
        my $mydate = Date::EzDate->new();
        $mydate->set_format('myformat', '{monthnumbase1}{day of month}{yearshort}{hour}{min}{sec}');
        $mytoday=$mydate->{'myformat'};
        $output=$build . "_" . $mytoday . "\.txt";
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

sub Parse(){ # parses the output of psservice.exe
        my $PsOut=shift;
        open (IN,"<$PsOut") or die "cannot open $PsOut: $!\n";
        $temp=$PsOut . "\.parsed";
        open (OUT,">$temp");

        while ($line=<IN>) {
                #print "line= $line\n";
                if ($line =~ /SERVICE_NAME/) {
                        @lines=split(':',$line);
                        chomp $lines[1];
                        print OUT $lines[1],"\t";
                }
                elsif($line =~ /STATE/) {
                        @lines=split(':',$line);
                        chomp $lines[1];
                        print OUT "=";
                        print OUT $lines[1],"\n";
                }

        }
}



sub go() {

        system("$ScriptPath\\psservice > $ScriptPath\\$build.txt");
        &Parse("$ScriptPath\\native.txt");
        &Parse("$ScriptPath\\$build.txt");
        &ComparePs("$ScriptPath\\native.txt.parsed","$ScriptPath\\$build.txt.parsed");
        #move *NOTFOUND* results
        #system("del DR_psservice_%2");
        #system("del DR_psservice_%2\.parsed");
        #system("copy $ScriptPath\\$output \\\\10.1.5.11\\qa-results\\boot_data\\$hostname");
}

sub ComparePs(){
        my $native=shift;
        my $dr=shift;

        open (IN,"<$native") or die "cannot open $native: $!\n";
        open (DR,"<$dr") or die "cannot open $dr: $!\n";

        #$temp= "$ScriptPath\\" . "compare" . "\.NOTFOUND" . $output_tag . ".txt";

        $hostname=~s/\n//g;
        $sumfile="Services_Summary_" . $hostname. "\.txt";
        $fullpath="$OutputPath\\$hostname\\$sumfile";
        print "Summary= $sumfile\n";

        if ( -e $fullpath) {
                open(OUT,">>$fullpath") or die "cannot open existing summary: $!\n";
        }

        else {
                open(OUT,">$fullpath") or die "cannot create summary: $!\n";}

        while ($line=<DR>) {
                 if ( $line =~ /run/i) {
                        #print "line222=$line\n";
                        @temp=split(' ',$line);
                        $DR{$temp[0]} =4;
                }
        }

        print OUT "\n********************************************************************\n";
        print OUT "\n------ Services stopped when booting in ";
        print OUT "$output_tag";
        print OUT "----------------\n";
        print  "\n------ Services stopped when booting in ---------------\n";

        while ($line=<IN>) {
                #print "line=$line\n";
                if ( $line =~ /run/i) {
                        #print "line222=$line\n";
                        @temp1=split(' ',$line);
                        if (! $DR{$temp1[0]} ) {
                                print OUT "$temp1[0], ";
                                print "$temp1[0] stopped when booting in DR\n" ;
                        }
                }
        }

        print "\n------- Services stopped when booting in Native ---------------\n";
        print OUT "\n------- Services stopped when booting in Native ---------------------------\n";
        @temp1='';

        while ($line=<OUT>) {
                 #print "line=$line\n";
                if ( $line =~ /run/i) {
                        #print "line222=$line\n";
                        @temp1=split(' ',$line);
                                if (! $Native{$temp1[0]} ) {
                                        print OUT "$temp1[0], ";
                                        print "$temp1[0] stopped when booting in NATIVE\n";
                                }
                }
        }
        close OUT;

}

sub Compare(){
        my $val;
        my $ShouldNotBeInDR;
        my $Warning=0;
        open(OUT,">>$fullpath") or die "cannot open existing summary: $!\n";

        foreach $val (keys %STAT) {
                if  (($STAT{$val} =~ /native/) && (exists($Slist{$val})) ) {
                        #print "$val : $Slist{$val} native\n";
                        print OUT "Warning: $val.exe=> should be running in DR!!!\n";
                        print "Warning: $val.exe=> should be running in DR!!!\n";
                        $Warning=1;
                }
                if (($STAT{$val} =~ /sc/i) && (!exists($Slist{$val})) ) {
                        #print "$val : $Slist{$val} native\n";
                        print OUT "Warning: $val.exe=> should NOT be running in DR!!!\n";
                        print "Warning: $val.exe=> should NOT be running in DR!!!\n";
                        $Warning=1;
                }

        }
        #print the list that I am not sure it should be in DR and it is
        #print "The following should not be in DR (it is ok if BITS) ?...\n";
        #print "\t=>$ShouldNotBeInDR\n";

        if (!$Warning) {
                print OUT "\n\tAll services that should be in DR are running in DR\n";
                print "\n\tAll services that should be in DR are running in DR\n";
        }
        print "Done -----------------------------------------------\n";
        print "fullpath=$fullpath\nhostname=$hostname\n";
        system("copy /Y $fullpath \"$OutputPath\\$hostname\" " );

}


sub CreateSlist() {
        my @arr1;
        my $line;
        my $val;



        open CONT,"<$ScriptPath\\drcontrol_full.txt" or die "cannot open drcontrol_full.txt :$!\n";
        while ($line=<CONT>) {
                if ($line =~ /Config Group/i) {
                        $line =~ s/\n//g;
                        $line =~ s/\s+//g;
                        @arr1=split(':',$line);

                        #print "service=$arr1[1]\n";
                        if ($arr1[1]=~ /-/) {
                                @tmpexe=split('-',$arr1[1]); #dllhost.exe-Processid02D4B3F1FD
                                $exe=$tmpexe[0];
                                $Slist{lc $exe}=1;
                        }
                        else {
                                $Slist{lc $arr1[1]}= 1;
                        }
                }

        }

        #foreach $val (keys %Slist) {
        #       print "$val=>>$Slist{$val}\n";
        #}

}

sub ParseDrview(){ # set to 0 for native and 1 for DR
        my @arr1;
        my $Native=1;
        my $process;
        my $line;
        my $val;

        my @ServiceList;
        my @ServiceList2;
        system("drview -listall  > $ScriptPath\\drview_all.txt");
        open VIEW,"<$ScriptPath\\drview_all.txt" or die "cannot open drview_all.txt :$!\n";
        while ($line=<VIEW>) {
                if ($line =~ /Process System/i) { next};
                my $i=0;
                if ($line =~ /PID/) {
                        $line=~ s/\s+/ /g;
                        $line=~ s/,//g;
                        #print " line111 = $line \n";
                        @ServiceList=split(' ',$line);
                        $EXE=$ServiceList[3];
                        $DRNative=$ServiceList[5];
                        $STAT{lc $EXE}=$DRNative;

                }
        }

        #foreach $val (keys %STAT) {
        #       print "|$val=>>$STAT{$val}\n";
        #}

}
