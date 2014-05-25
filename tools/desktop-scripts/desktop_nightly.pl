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

# desktop_nightly.pl
#
# NOTE: ASSUMES drview, drcontrol, svccntrl and perl are in path
# NOTE: ASSUMES c:\desktop-nightly\boot_data_scripts and
#               c:\desktop-nightly\boot_data exist  #TODO do a mkdir
# NOTE: ASSUMES this script is in c:\desktop-nightly\boot_data_scripts #FIXME
#
# To run:
#   1. Setup $NIGHTLY_DIR and $NIGHTLY_SUMMARY to your liking
#   2. run the script: perl desktop_nightly.pl
#      or
#      run desktop_nightly.bat
#          [NOTE: put it in Programs->Startup to run at reboot/login]
#

use strict;
use Getopt::Long;
use Cwd;

my $PROG = "desktop_nightly.pl"; # TODO: use cwd? basename etc.

# TODO: if cpu utilization hits <= 2%
#   write all sums to log file

my $ROOT      = qq(C:\\desktop-nightly);
my $SCRIPTDIR = qq($ROOT\\boot_data_scripts);
my $OUTDIR    = qq($ROOT\\boot_data);

# get the boot time before doing anything else
my ($boot_time, @ignore) = grep s/uptime in seconds: //i,
                                `cscript -nologo $SCRIPTDIR\\uptime.vbs`;

my %opt;
&GetOptions(\%opt,
    "email",  # send email when done
    "help",
    "usage",
    "debug",
);
#TODO: print usage, verify options?

#my $DETERMINA = qq("$ENV{'PROGRAMFILES'}\\Determina\\Determina Agent");

my $DRVER = &get_build();
# list $determina lib dir 30xxx builds
#my @builds = `dir /b $DETERMINA\\lib\\302??r`;
# get the last element from that list
#chomp(my $DRVER  = @builds[-1]);

print "$PROG: running $DRVER\n";
print "$PROG: measuring memory until cpu utilization is <= 2%\n";

# regular nightly dir
#my $NIGHTLY_DIR = qq(\\\\10.1.5.85\\g_shares\\desktop-nightly\\results\\boot_data);

# run it three times to see noise
my $NIGHTLY_DIR = qq(\\\\10.1.5.85\\g_shares\\desktop-nightly\\results\\boot_data\\nightly3);

#FIXME this works?
#mkdir $NIGHTLY_DIR unless (-d $NIGHTLY_DIR)

my $NIGHTLY_SUMMARY = qq(qatest_xp_nightly_summary.txt);

#for boot time alone
#my $MEASURE_MEM_CMD = qq($SCRIPTDIR\\measure_mem.pl --times 1 --name "$DRVER-def-opt" --nightly $NIGHTLY_DIR --summary $NIGHTLY_SUMMARY --b $boot_time);

my $MEASURE_MEM_CMD = qq($SCRIPTDIR\\measure_mem.pl --tillidle --name "$DRVER-def-opt" --nightly $NIGHTLY_DIR --summary $NIGHTLY_SUMMARY --b $boot_time);

print "$PROG: $MEASURE_MEM_CMD\n";
system("perl $MEASURE_MEM_CMD");

if (defined $opt{'email'}) {
    my $EMAIL_CMD = qq($SCRIPTDIR\\email_mem_summary.pl --summary "$NIGHTLY_DIR\\$NIGHTLY_SUMMARY");

    print "$PROG: $EMAIL_CMD\n";
    system("perl $EMAIL_CMD");
}


#--------------------------------------------------------------------------
sub get_build() {
    my @dump = `drcontrol -dump 2> $OUTDIR\\drcontrol_err.txt`;

    # drcontrol -dump prints
    #   error message, if core is not installed
    #   data for SC config, if disabled from DMC
    # if we have any app instrumented, then we care about under DR
    my ($ignore_sc, $build, @ignore_rest) =
        grep s/DYNAMORIO_AUTOINJECT.*Agent.lib.//i, @dump;

    chomp($build);

    $build = "native" unless ($build);
    $build =~ s/\s*([0-9]+[a-z]).dynamorio.dll/$1/i;

    return $build;
}
