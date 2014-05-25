#!/usr/bin/perl

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

# creates a unit test summary report from runregression log and runlog.*

if ( ($#ARGV+1) != 3) {
    print ("Usage: utreport <winrunreg.log> <winrunlog dir> <details-file-name>\n");
    exit;
}

$winrunlogdir=$ARGV[1];
open(OUT,">$ARGV[2]") or die ("Cannot open output file :$!");

sub parserunlog;

#
# windows details
#
print OUT "\n====================== Regression Suite Details ======================\n\n";

open(IN,"<$ARGV[0]") or die ("Cannot open input file :$!");

while(($val=<IN>) && $val !~ /Summary of results:/) {
    #nothing
}

$val=<IN>;
while (1) {

    if($val =~ /^==========/) {
        last;
    }

    @tmp=split(' ', $val);
    @tmp2=split(':', $tmp[1]);
    $nmbr=$tmp2[0];

    if ($val =~ /^Build/) {
        $next=<IN>;
        $next_next=<IN>;
        if ($next =~ /<no errors>/ && $next_next =~ /no warnings/) {
            print OUT "\nBuild ", $nmbr, " successful.\n";
        } else {
            print OUT "\nBuild ", $nmbr, " failure!\n";
        }
        $val=<IN>;
        if ($val =~ /SHARETESTS/) {
            print OUT $val;
        } else {
            # next iter will handle
            next;
        }
    }
    elsif ($val =~ /Run/) {

        parserunlog($nmbr, "long");

        # read in the summary lines
        $val=<IN>;
        if ($val =~ /SPEC:/) {
            $val=<IN>;
        }
        if ($val !~ /TESTS:/) {
            # next iter will handle
            next;
        }
    }
    elsif ($val =~ /^(SHARE|TOOLS)/) {
        print OUT $val;
    }
    elsif (length($val)>1) {
        print OUT "Warning! unexpected result line>", $val;
    }
    if (!($val=<IN>)) {
        last;
    }
}
close(OUT);

# summaries go to stdout

#
# windows summary
#
print "\n====================== Regression Suite Core Errors ======================\n\n";

# get summary of DR errors
system("grep -E 'Unrecoverable|Internal DynamoRIO Error|CURIOSITY' $ARGV[2] | sort | uniq -c | awk '{s+=\$1} END {printf \"Total DR errors: %d\\n\\n\", s}'");
system("grep -E -o 'Unrecoverable.*|Internal DynamoRIO Error.*|CURIOSITY.*' $ARGV[2] | sort | uniq -c | sort -nr");

print "\n====================== Regression Suite Test Failures ======================\n\n";

# get summary of DR errors
system("grep -E '^------------- ' $ARGV[2] | sed 's/---*//g' | sort | uniq -c | awk '{s+=\$1} END {printf \"Total failing tests: %d\\n\\n\", s}'");
system("grep -E '^------------- ' $ARGV[2] | sed 's/---*//g' | sort | uniq -c | sort -nr");

print "\n====================== Regression Suite Failures ======================\n\n";

print "Total failing runs: ";
system("grep -c \" \\*\\*\\* .* failed:\" $ARGV[0]");
print "\n";

# first, get summary of failures
# could move this grep to caller but nice to all be from this report script
system("grep -A 5000 'Summary of results' $ARGV[0] | grep -E 'Run |[1-9][0-9]* failed|s in build|FAILED' | grep -B 1 -E '[1-9][0-9]* failed|s in build|FAILED'");

print "\n====================== Regression Suite Summary ======================\n\n";

open(IN,"<$ARGV[0]") or die ("Cannot open input file :$!");

while(($val=<IN>) && $val !~ /Summary of results:/) {
    #nothing
}

# print entire summary as is, regression now prints which tests failed
while(($val=<IN>) && $val !~ /=====/) {
    print $val;
}

close(IN);

exit;

sub parserunlog
{
    $nmbr = shift;
    $type = shift;
    $specpass = 0;
    $specfail = 0;
    $runlogdir = $winrunlogdir;

    if ($type !~ /short/ && $type !~ /long/) {
        print OUT "parameter error!";
        exit;
    }

    if($type =~ "short") {
        $runlogdir = shift;
    }

    my $file;
    my $rlval;
    $file = sprintf("%s/runlog.%03d", $runlogdir, $nmbr);

    open(RLIN,"<$file") or die ("Cannot open input file \"$file\" : $!");

    if($type =~ "long") {
        print OUT "\n======================= Run ", $nmbr, " =========================\n";
    }

    #first scan for spec, if present
    while ($rlval=<RLIN>) {
        if ($rlval =~ /Benchmark/ || $rlval =~ /Using build/) {
            last;
        }
    }

    if ($rlval =~ /Benchmark/) {
        if($type =~ "short") {
            print OUT "    ";
        }
        print OUT "SPEC: ";
        do {
            $rlval=<RLIN>;
            @tmps=split(' ', $rlval);
            if ($tmps[2] =~ "ok") {
                $specpass++;
            }
            elsif ($tmps[2] =~ "FAIL") {
                $specfail++;
                print OUT $tmps[0], " ";
            }
            else {
                $rlval="done";
            }
        } while ($rlval !~ "done");

        if ($specfail > 0) {
            print OUT "failed, ";
        }
        print OUT $specpass, " tests passed.\n";

        while ($rlval=<RLIN>) {
            if ($rlval =~ /Using build/) {
                last;
            }
        }
    }

    # now the unit tests, if present
    if ($rlval =~ /Using build/) {
        if ($type =~ "long") {
            print OUT "\nunit tests:\n\n";
        }
        else {
            print OUT "    TESTS: ";
        }

        $utpass=0;
        $utfail=0;
        my $pass;

        while ($val=<RLIN>) {

            if ($val =~ /^----- /) {
                $next = <RLIN>;
                @tmp=split(' ', $val);
                if ($next =~ /^PASS/) {
                    $pass[$utpass]=$tmp[1];
                    $utpass++;
                }
                elsif ($next =~ /^SKIP/) {
                    # do nothing
                }
                else {
                    if ($type =~ "long") {
                        print OUT "------------- ", $tmp[1], " -----------------\n";
                        $badln=1;
                        do {
                            print OUT $next;
                            $next = <RLIN>;
                            $badln++;
                        } while ($next !~ /^FAIL/ && $next !~ /^----- / && $badln < 20);
                        if ($badln == 20) {
                            print OUT "   --<diff truncated>--\n";
                        }
                        print OUT "\n";
                        $utfail++;
                    }
                    else { #short
                        print OUT $tmp[1], " ";
                    }
                }
            }
        }

        if ($type =~ "long") {
            print OUT $utfail, " unit tests ";
        }

        if ($type =~ "long" || $utfail > 0) {
            print OUT  "failed. ";
        }

        if($type =~ "long") {
            print OUT "\n--------------------------------------\npassed tests:  ";

            for ($j=0; $j<$utpass; $j++) {
                print OUT $pass[$j], ", ";
            }
            print OUT "\n";
        }

        print OUT $utpass, " tests passed.\n";
    }

}

