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

# measure_options.pl
#
# NOTE: ASSUMES drview, drcontrol, svccntrl and perl are in path
# NOTE: ASSUMES c:\desktop-nightly\boot_data_scripts and
#               c:\desktop-nightly\boot_data exist  #TODO do a mkdir
# NOTE: ASSUMES this script is in c:\desktop-nightly\boot_data_scripts #FIXME
#
# NOTE: USES: BOOTNUM_FILE, uptime.vbs
#
# NOTE: Set up BOOTNUM_FILE (see below) to 0 (when done it will set it up to
#       0 and should be ready to use, but if runs were interrupted you'd
#       need to do this manually)
#
# NOTE: Appends options specified in @dropts to current options provided by
#       nodemgr
#
# To run:
#   1. Just write 0 to BOOTNUM_FILE
#   2. Setup @dropts array with option to be run
#   3. Setup $RESULTS_DIR and $SUMMARY_FILE to your liking
#   4. run the script: perl measure_options.pl
#      or
#      call it from a batch file
#      [NOTE: put batch file in Programs->Startup to run at reboot/login]
#

use strict;
use Getopt::Long;

my $PROG = "measure_options.pl"; # TODO: use cwd? basename etc.

# calculate boot time and log it
#
# disable/enable node manager
#
# run drtop every boot every 30s
#   sum all cols from -showmem o/p
# if cpu utilization hits <= 2%
#   write all sums to log file
#
# for each boot do
#   set up registry keys for each app with
#     appropriate options for this run
#   do the run
#
# different runs
#   reset from 1 to nth thread
#   set -hotp_nudge_is_reset
#   -C
#   -E
#   -F
#   shared bbs
#   shared traces
#   private traces with diff cache sizes
#   different set of services loaded
#     default set
#     medium set
#     all set
#
# move all logfiles to \\fileserver\g_shares\desktop_nightly\results
#
# enable node manager
#   some other script installs latest core

my $ROOT      = qq(C:\\desktop-nightly);
my $SCRIPTDIR = qq($ROOT\\boot_data_scripts);
my $OUTDIR    = qq($ROOT\\boot_data);

# get the boot time before doing anything else
my ($boot_time, @ignore) = grep s/uptime in seconds: //i,
                                `cscript -nologo $SCRIPTDIR\\uptime.vbs`;

my %opt;
&GetOptions(\%opt,
    "debug",
    "default",
    "help",
    "usage",
);
#TODO: print usage, verify options

my $DRVER = &get_build();

system("net use m: \\\\10.1.5.85\\g_shares\\desktop-benchmarks \"\" /USER:guest") unless (-e "m:");

my $date = &datestamp();
my $RESULTS_DIR = qq(m:\\results\\boot_data\\qatest_$DRVER).qq(_mem_$date);
mkdir $RESULTS_DIR unless -e $RESULTS_DIR;

my $SUMMARY_FILE = qq(qatest_$DRVER).qq(_mem_$date.txt);

# List of options for each run.
# Just enable/put the options you want run here.
my @dropts = (
     "",   # dont change this, let this be boot 0

#    "",                      # default
#    "",                      # default
#    "",                      # default
#    "-no_enable_reset",
#    "-no_enable_reset",
#    "-no_enable_reset",
#    "-hotp_nudge_is_reset",
#    "-hotp_nudge_is_reset",
#    "-hotp_nudge_is_reset",
#    "-C -hotp_nudge_is_reset",
#    "-C -hotp_nudge_is_reset",
#    "-C -hotp_nudge_is_reset",
#    "-E -hotp_nudge_is_reset",
#    "-E -hotp_nudge_is_reset",
#    "-E -hotp_nudge_is_reset",
#    "-F -hotp_nudge_is_reset",
#    "-F -hotp_nudge_is_reset",
#    "-F -hotp_nudge_is_reset",
#    "-C -E -F -hotp_nudge_is_reset",
#    "-C -E -F -hotp_nudge_is_reset",
#    "-C -E -F -hotp_nudge_is_reset",
    # -reset_at_nth_thread is added to this list in a loop (see below)

#    # see memory impact of two new features I added:
#    "-no_remove_shared_trace_heads",
#    "-remove_trace_components",
#
#    # shared ibt tables experimental
#    "-shared_ibt_tables",
#
#    # shared bbs, no traces
#    "-shared_bbs_only",
#    "-shared_bbs_only",
#    "-shared_bbs_only",
#
#    # shared bbs, private traces
#    "-no_shared_traces",
#    "-no_shared_traces -cache_trace_regen 20",
#    "-no_shared_traces -cache_trace_regen 30",
#    "-no_shared_traces -cache_trace_regen 40",
#    "-no_shared_traces -no_finite_trace_cache",
#
#    # private bbs, no traces
#    "-disable_traces -no_shared_bbs",
#    "-disable_traces -no_shared_bbs -cache_bb_regen 20",
#    "-disable_traces -no_shared_bbs -cache_bb_regen 30",
#    "-disable_traces -no_shared_bbs -cache_bb_regen 40",
#    "-disable_traces -no_shared_bbs -no_finite_bb_cache",
#
#    # private bbs, shared traces
#    "-no_shared_bbs",
#    "-no_shared_bbs -cache_bb_regen 20",
#    "-no_shared_bbs -cache_bb_regen 30",
#    "-no_shared_bbs -cache_bb_regen 40",
#    "-no_shared_bbs -no_finite_bb_cache",
#
#    # all private
#    # bb regen 10 (default), vary trace regen 10,20,30,40,infinite
#    "-no_shared_traces -no_shared_bbs",
#    "-no_shared_traces -no_shared_bbs -cache_trace_regen 20",
#    "-no_shared_traces -no_shared_bbs -cache_trace_regen 30",
#    "-no_shared_traces -no_shared_bbs -cache_trace_regen 40",
#    "-no_shared_traces -no_shared_bbs -no_finite_trace_cache",
#    # bb regen 20, vary trace regen 10,20,30,40,infinite
#    "-no_shared_traces -no_shared_bbs -cache_bb_regen 20",
#    "-no_shared_traces -no_shared_bbs -cache_bb_regen 20 -cache_trace_regen 20",
#    "-no_shared_traces -no_shared_bbs -cache_bb_regen 20 -cache_trace_regen 30",
#    "-no_shared_traces -no_shared_bbs -cache_bb_regen 20 -cache_trace_regen 40",
#    "-no_shared_traces -no_shared_bbs -cache_bb_regen 20 -no_finite_trace_cache",
#    # bb regen 30, vary trace regen 10,20,30,40,infinite
#    "-no_shared_traces -no_shared_bbs -cache_bb_regen 30",
#    "-no_shared_traces -no_shared_bbs -cache_bb_regen 30 -cache_trace_regen 20",
#    "-no_shared_traces -no_shared_bbs -cache_bb_regen 30 -cache_trace_regen 30",
#    "-no_shared_traces -no_shared_bbs -cache_bb_regen 30 -cache_trace_regen 40l",
#    "-no_shared_traces -no_shared_bbs -cache_bb_regen 30 -no_finite_trace_cache",
#    # bb regen 40, vary trace regen 10,20,30,40,infinite
#    "-no_shared_traces -no_shared_bbs -cache_bb_regen 40",
#    "-no_shared_traces -no_shared_bbs -cache_bb_regen 40 -cache_trace_regen 20",
#    "-no_shared_traces -no_shared_bbs -cache_bb_regen 40 -cache_trace_regen 30",
#    "-no_shared_traces -no_shared_bbs -cache_bb_regen 40 -cache_trace_regen 40",
#    "-no_shared_traces -no_shared_bbs -cache_bb_regen 40 -no_finite_trace_cache",
#    # bb infinite, vary trace regen 10,20,30,40,infinite
#    "-no_shared_traces -no_shared_bbs -no_finite_bb_cache",
#    "-no_shared_traces -no_shared_bbs -no_finite_bb_cache -cache_trace_regen 20",
#    "-no_shared_traces -no_shared_bbs -no_finite_bb_cache -cache_trace_regen 30",
#    "-no_shared_traces -no_shared_bbs -no_finite_bb_cache -cache_trace_regen 40",
#    "-no_shared_traces -no_shared_bbs -no_finite_bb_cache -no_finite_trace_cache",

);

# enable to run reset_at_nth_thread 1..20
#for (my $thr = 1; $thr < 21; $thr++) {
#    push @dropts, "reset_at_nth_thread $thr";
#}


# run with just the default set of options
@dropts = ( "" ) if defined $opt{'default'};

# file to save registry settings
my $SAVE_CFG = qq($SCRIPTDIR\\qatest_determina_keys.cfg);

# file that holds the boot number
my $BOOTNUM_FILE = qq($SCRIPTDIR\\bootnum);
my $bootnum = &get_bootnum();
print "$PROG: Boot number: $bootnum\n";

# at each boot, pick a set of options to run with

&disable_node_manager();

# writes 0 to BOOTNUM_FILE
&cleanup_and_exit() if ($bootnum >= scalar @dropts);

print "$PROG: running options: $dropts[$bootnum+1]\n";

# save the registry settings the first time
system("drcontrol -save $SAVE_CFG") if ($bootnum == 0);
system("drcontrol -dump > $RESULTS_DIR\\drcontrol-dump.$bootnum");

my $MEASURE_MEM_CMD = qq($SCRIPTDIR\\measure_mem.pl -tillidle --name "$DRVER-$bootnum" --nightly $RESULTS_DIR --summary $SUMMARY_FILE --b $boot_time);

print "$PROG: $MEASURE_MEM_CMD\n";
system("perl $MEASURE_MEM_CMD");

&do_hotp_nudge() if ($dropts[$bootnum] =~ m/hotp_nudge_is_reset/i);

# now set up registry keys for next measurement
# restore the registry settings for our agent
system("drcontrol -detachall");
system("drcontrol -load $SAVE_CFG");
&resetreg($dropts[$bootnum+1]);

print "$PROG: rebooting..\n";

reboot($bootnum);



#-----------------------------------------------------------------------------

sub resetreg {
    my $append_dropt = shift;

    #TODO: use %PROGRAM FILES% ?
    my $DETERMINA = qq("$ENV{'PROGRAMFILES'}\\Determina\\Determina Agent");
    my $DRHOME = $DETERMINA;
    #my $DRHOME = qq("C:\\Program Files\\Determina\\Determina Agent");
    my $LIBDIR = "lib";
    my $LOGDIR = "logs";

    my $prev_dropt = "";
    my $dropt = "";

    my @ignore;

    # disable node manager if needed
    &disable_node_manager() unless
        scalar grep m/scnodemgr.*disabled/i, `svccntrl scnodemgr -show`;

    my $drdll = qq($DRHOME\\$LIBDIR\\$DRVER\\dynamorio.dll);

    # may as well set global key though we're setting every app key below
    system("drcontrol -drlib $drdll");
    system("drcontrol -logdir $DRHOME\\$LOGDIR");

    ($prev_dropt, @ignore) =
            grep s/\s*DYNAMORIO_OPTIONS=\s*//, `drcontrol -dump`;
    chomp($prev_dropt);
    print "$PROG: global dropt: $prev_dropt\n";

    $dropt = qq("$prev_dropt $append_dropt");
    system("drcontrol -options $dropt");

    # drcontrol -load not implemented, and no way to remove a string value,
    # so our only options are to reproduce the slist parsing and get the
    # right rununder values, or set the values we want for each app, which
    # is a pain to edit manually later but oh well:
    my @apps = grep s/\s*Config\s*Group\s*: \s*//, `drcontrol -dump`;
    my $app;
    foreach $app (@apps) {
        system("drcontrol -app $app -drlib $drdll");
        system("drcontrol -app $app -logdir $DRHOME\\$LOGDIR");
        ($prev_dropt, @ignore) =
            grep s/\s*DYNAMORIO_OPTIONS=\s*//, `drcontrol -appdump $app`;
        next unless $prev_dropt;
        chomp($prev_dropt);
        $dropt = qq("$prev_dropt $append_dropt");
        system("drcontrol -app $app -options $dropt");
    }

    ### if ignore RUNUNDER this will re-make reg keys from scc file:
    ###    # now re-build reg setup
    ###    drcontrol -reset;
    ###    for i in `grep -i exe /c/Program\ Files/Determina/SecureCore/config/default.scc`; do
    ###        # pull exe name out of [HKEY_LOCAL_MACHINE\<...>\inetinfo.exe]
    ###        j=${i##*\\};
    ###        drcontrol -add ${j/]/};
    ###    done;
    system("drcontrol -preinject ON");
}

sub do_hotp_nudge {
    # should be idle now, so nudge
    system("drcontrol -hot_patch_nudge_all");
    # sleep for 10 more minutes to see what happens to memory
    sleep 600;
}

sub cleanup_and_exit {
    # clean up after all runs
    system("drcontrol -load $SAVE_CFG");
    system("echo 0 > $BOOTNUM_FILE");

    &enable_node_manager();

    exit 0;
}

sub disable_node_manager {
    #TODO: manual is fine or do we need to disable
    system("svccntrl scnodemgr -disabled");
    system("net stop scnodemgr");
}

sub enable_node_manager {
    system("svccntrl scnodemgr -auto");
    system("net start scnodemgr");
}

sub get_bootnum {
    open FD, "<$BOOTNUM_FILE" or die "Cannot open $BOOTNUM_FILE: $!";
    chomp(my $num = <FD>);
    close FD;
    return $num;
}

sub reboot {
    my $bootnum = shift;
    $bootnum++;
    system("echo $bootnum > $BOOTNUM_FILE");
    system("shutdown -r -t 0 -f");
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

sub datestamp() {
    # ignore wday - day of the week, $yday - day of the year, and
    # $is_dst - is daylight savings time
    my ($sec,  $min,  $hour,
        $mday, $mon,  $year,
        $wday, $yday, $is_dst) = localtime(time);

    # last 2 digits of year
    $year = ($year + 1900) % 100;
    my $stamp = sprintf("%02d%02d%02d",
                            $year,$mon+1,$mday);
    return $stamp;
}
