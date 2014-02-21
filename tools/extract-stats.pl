#!/usr/bin/perl -w

# **********************************************************
# Copyright (c) 2004 VMware, Inc.  All rights reserved.
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

### extract-stats.pl
###
### Summarize data gathered from DR logs under the various loglevels


sub init_tag($$) {

    my($tag, $rate) = @_;

    $marker_hash{$tag} = $marker_count++;
    $rate_hash{$tag} = $rate;
}


sub init_syscall_tags() {

    init_tag("System calls, pre", 1);
    init_tag("System calls, post", 1);

}


sub init_security_tags() {

    init_tag("Return after call total validations", 1);

}


sub init_cache_exits_tags() {

    init_tag("Fcache exits, total", 1);
    init_tag("Fcache exits, from traces", 1);
    init_tag("Fcache exits, from BBs", 1);
    init_tag("Fcache exits, total indirect branches", 1);
    init_tag("Fcache exits, total direct branches", 1);
    init_tag("Fcache exits, non-trace indirect branches", 1);
    init_tag("Fcache exits, ind target not in cache", 1);
    init_tag("Fcache exits, ind target extending a trace, BAD", 1);
    init_tag("Fcache exits, ind target in cache but not table", 1);
    init_tag("Fcache exits, from BB, ind target ...", 1);
    init_tag("Fcache exits, BB->BB, ind target ...", 1);
    init_tag("Fcache exits, BB->BB, ind back target ...", 1);
    init_tag("Fcache exits, BB->trace, ind target ...", 1);
    init_tag("Fcache exits, from trace, ind target ...", 1);
    init_tag("Fcache exits, trace->trace, ind target ...", 1);
    init_tag("Fcache exits, trace->BB, ind target ...", 1);
    init_tag("Fcache exits, indirect calls", 1);
    init_tag("Fcache exits, indirect jmps", 1);
    init_tag("Fcache exits, returns", 1);
    init_tag("Fcache exits, returns from traces", 1);
    init_tag("Fcache exits, dir target not in cache", 1);
    init_tag("Fcache exits, link not allowed", 1);
    init_tag("Fcache exits, target trace head", 1);
    init_tag("Fcache exits, extending a trace", 1);
    init_tag("Fcache exits, no link shared <-> private", 1);
    init_tag("Fragments generated, bb and trace", 1);
    init_tag("Trace fragments generated", 1);
    init_tag("Trace wannabes prevented from being traces", 1);
    init_tag("Future fragments generated", 1);
    init_tag("Shared fragments generated", 1);
    init_tag("Private fragments generated", 1);
    init_tag("Shared future fragments generated", 1);
    init_tag("Unique fragments generated", 1);
    init_tag("Fragments deleted for any reason", 1);
    init_tag("Fragments deleted due to capacity conflicts", 1);
    init_tag("Fragments deleted and replaced with traces", 1);
    init_tag("Fragments regenerated in cache", 1);
    init_tag("Trace fragments extended", 1);
    init_tag("Trace fragments extended, ibl exits updated", 1);
    init_tag("Patched fragments", 1);
    init_tag("Patched relocation slots", 1);
    init_tag("Branches linked, direct", 1);
    init_tag("Branches linked, indirect", 1);

}


sub init_cache_memory_tags() {

    init_tag("Fcache trace space claimed \\(bytes\\)", 0);
    init_tag("Fcache trace space used \\(bytes\\)", 0);
    init_tag("Fcache trace peak used \\(bytes\\)", 0);
    init_tag("Fcache trace headers \\(bytes\\)", 0);
    init_tag("Fcache trace fragment bodies \\(bytes\\)", 0);
    init_tag("Fcache trace direct exit stubs \\(bytes\\)", 0);
    init_tag("Fcache trace indirect exit stubs \\(bytes\\)", 0);
    init_tag("Fcache trace fragment prefixes \\(bytes\\)", 0);
    init_tag("Fcache trace align space \\(bytes\\)", 0);
    init_tag("Fcache bb capacity \\(bytes\\)", 0);
    init_tag("Fcache bb space claimed \\(bytes\\)", 0);
    init_tag("Fcache bb space used \\(bytes\\)", 0);
    init_tag("Fcache bb peak used \\(bytes\\)", 0);
    init_tag("Fcache bb headers \\(bytes\\)", 0);
    init_tag("Fcache bb fragment bodies \\(bytes\\)", 0);
    init_tag("Fcache bb indirect exit stubs \\(bytes\\)", 0);
    init_tag("Fcache bb align space \\(bytes\\)", 0);
    init_tag("Fcache shared capacity \\(bytes\\)", 0);
    init_tag("Fcache shared space claimed \\(bytes\\)", 0);
    init_tag("Fcache shared space used \\(bytes\\)", 0);
    init_tag("Fcache shared peak used \\(bytes\\)", 0);
    init_tag("Fcache shared headers \\(bytes\\)", 0);
    init_tag("Fcache shared fragment bodies \\(bytes\\)", 0);
    init_tag("Fcache shared direct exit stubs \\(bytes\\)", 0);
    init_tag("Fcache shared indirect exit stubs \\(bytes\\)", 0);
    init_tag("Fcache shared align space \\(bytes\\)", 0);
    init_tag("Fcache combined claimed \\(bytes\\)", 0);
    init_tag("Fcache combined capacity \\(bytes\\)", 0);
    init_tag("Peak fcache combined capacity \\(bytes\\)", 0);

}


sub init_memory_tags() {

    init_tag("Heap headers \\(bytes\\)", 0);
    init_tag("Total memory from OS", 0);
    init_tag("Peak total memory from OS", 0);
    init_tag("Stack capacity \\(bytes\\)", 0);
    init_tag("Peak stack capacity \\(bytes\\)", 0);
    init_tag("Mmap capacity \\(bytes\\)", 0);
    init_tag("Peak mmap capacity \\(bytes\\)", 0);
    init_tag("Heap capacity \\(bytes\\)", 0);
    init_tag("Peak heap capacity \\(bytes\\)", 0);
    init_tag("Total memory from OS", 0);
    init_tag("Peak total memory from OS", 0);
    init_tag("Our library space \\(bytes\\)", 0);
    init_tag("Application reserved-only capacity \\(bytes\\)", 0);
    init_tag("Peak application reserved-only capacity \\(bytes\\)", 0);
    init_tag("Application committed capacity \\(bytes\\)", 0);
    init_tag("Peak application committed capacity \\(bytes\\)", 0);
    init_tag("Application stack capacity \\(bytes\\)", 0);
    init_tag("Peak application stack capacity \\(bytes\\)", 0);
    init_tag("Application heap capacity \\(bytes\\)", 0);
    init_tag("Peak application heap capacity \\(bytes\\)", 0);
    init_tag("Application image capacity \\(bytes\\)", 0);
    init_tag("Peak application image capacity \\(bytes\\)", 0);
    init_tag("Application mmap capacity \\(bytes\\)", 0);
    init_tag("Peak application mmap capacity \\(bytes\\)", 0);
    init_tag("Application executable capacity \\(bytes\\)", 0);
    init_tag("Peak application executable capacity \\(bytes\\)", 0);
    init_tag("Application read-only capacity \\(bytes\\)", 0);
    init_tag("Peak application read-only capacity \\(bytes\\)", 0);
    init_tag("Application writable capacity \\(bytes\\)", 0);
    init_tag("Peak application writable capacity \\(bytes\\)", 0);
    init_tag("Total \\(app + us\\) virtual size \\(bytes\\)", 0);
    init_tag("Peak total \\(app + us\\) virtual size \\(bytes\\)", 0);
    init_tag("Application virtual size \\(bytes\\)", 0);
    init_tag("Peak application virtual size \\(bytes\\)", 0);
    init_tag("Our additional virtual size \\(bytes\\)", 0);
    init_tag("Peak our additional virtual size \\(bytes\\)", 0);
    init_tag("Our commited capacity \\(bytes\\)", 0);
    init_tag("Our peak commited capacity \\(bytes\\)", 0);
    init_tag("Our reserved capacity \\(bytes\\)", 0);
    init_tag("Our peak reserved capacity \\(bytes\\)", 0);
    init_tag("App unallocatable free space", 0);
    init_tag("Peak app unallocatable free space", 0);
    init_tag("Our unallocatable free space", 0);
    init_tag("Our peak unallocatable free space", 0);
    init_tag("Total unallocatable free space", 0);
    init_tag("Peak total unallocatable free space", 0);
    init_tag("Number of unaligned allocations \\(TEB's etc.\\)", 0);
    init_tag("Peak unaligned allocations", 0);
}


sub process_snapshot($$) {

    my($verbose, $line) = @_;

    if ($verbose) {
        print STDERR "Digest $line...\n";
    }

    # Bypass the "** statistics @<fragment count>" line.
    if ($line =~ /^(All|Thread) statistics @[0-9]/) {
        if ($verbose) {
            print STDERR "Bypass $line\n";
        }
        return;
    }

    my $found = 0;
    my $key;
    my $value;

    $lines[ $line_count++ ] = $line;
    while (($key, $value) = each %marker_hash) {
        if ($found == 1) {
            next;
        }
        if ($verbose) {
            print STDERR "Check for \"$key\" in line $.: $line\n";
        }

        # XXX Match thread stats also...
        #if ($line =~ /^[ ]*${key}[ ]+:[ ]+(\d+)/) {
        if ($line =~ /^[ ]*${key}[ ]+(\(thread\))?:[ ]+(\d+)/) {
        #if ($line =~ /^[ ]*${key}/ && /:[ ]+(\d+)/) {

            # Adjust for processing thread-private log files.
            my $counter = defined($2) ? $2 : $1;
            if ($verbose) {
                print STDERR "Count for $key is $counter\n";
            }

            # XXX Tried to put the substr call in the parens but
            # it wouldn't work.
            #if ($line eq "(") {
            #    if ($verbose) {
            #        print STDERR "THREAD log\n";
            #    }
            #}
            # Now update the table
            $counter_array[$value][$snapshot_opens] = $counter;
            $valid{$key} = 1;
            if ($verbose) {
                print STDERR "Update counter for $key => $value, " .
                    "$snapshot_opens with $counter\n";
            }
            # We don't 'last' out of the loop because we'd have
            # to reset the hash iter using 'keys' on the next
            # key match. And keys could be a sort underneath, which
            # is costlier than continuing the loop, which is
            # linear.
            $found = 1;
        }
    }
}

sub prepare_stats($$$) {

    my $i;
    my($limit, $value, $rate) = @_;

    # Clean up undefined values.
    for ($i = 0; $i < $limit - 1; $i++) {
        if (!defined($counter_array[$value][$i])) {
            $counter_array[$value][$i] = 0;
        }
    }

    # We don't track non-rate stats incrementally.
    if ($rate) {
        $avg_rate = $counter_array[$value][$limit - 1] /
            int($snapshot_times[$limit - 1] / 1000000);
        for ($i = $limit - 1; $i > 0; $i--) {
            $counter_array[$value][$i] -= $counter_array[$value][$i-1];
            $rate_array[$value][$i] =
                ($counter_array[$value][$i] - $counter_array[$value][$i-1]) /
                $snapshot_times[$i];
        }
    }
}

sub numerically { $a <=> $b; }

sub compute_stats($$$$) {

    my $i;
    my($limit, $key, $value, $rate) = @_;
    my $start_index = $limit > 2 ? 1 : 0;

    # Sort the counter array and store the max and median. Don't count
    # the first counter as it's often artificially high.
    @sorted_counters = sort numerically
        @{ $counter_array[$value] } [$start_index..$limit - 1];
    $max_val = $sorted_counters[$limit - ($limit > 2 ? 2 : 1)];
    $median_val = $sorted_counters[$median_index];

    # XXX Figure out how to sort this and use an array of structs
    $prev_time = $max_rate = $max_time = $max_rate_time = 0;

    @rate_array = ();

    for ($i = $start_index; $i < $limit; $i++) {
        $curr_time = $snapshot_times[$i];
        $count = $counter_array[$value][$i];

        # Find the largest value.
        if ($count == $max_val) {
            $max_time = $curr_time;
        }

        # Find the largest rate.
        if ($rate) {
            # If the delta is too small, don't compute a rate.
            if ($curr_time - $prev_time == 0) {
                next;
            }
            $current_rate = int(1000000 * $count / ($curr_time - $prev_time));
            if ($current_rate > $max_rate) {
                $max_rate = $current_rate;
                $max_rate_time = $curr_time;
            }
            $rate_array[$i] = $current_rate;
            $prev_time = $curr_time;
        }
    }

    # Sort the rate array and store the max and median. Don't count
    # the first counter as it's not computed.
    for ($i = $start_index; $i < $limit; $i++) {
        if (!defined($rate_array[$i])) {
            $rate_array[$i] = 0;
        }
    }

    if ($rate) {
        @sorted_rates = sort numerically
            @rate_array [$start_index..$limit - 1];
        $max_rate = $sorted_rates[$limit - 2];
        $median_rate = $sorted_rates[$median_index];
    }
}

sub print_stats($$$$$) {

    my $i;
    my $currtime;
    my $prevtime = 0;
    my($rate, $value, $details, $limit, $raw) = @_;

    if (!$raw && $rate) {
        printf "\tmax rate = $max_rate [snapshot = " .
            int($max_rate_time / 1000000) . "], median rate = $median_rate" .
            ", avg rate = " . int($avg_rate) . "\n"
    }
    elsif ($raw) {
        print "$key raw data\n";
    }

    if ($details || $raw) {
        if ($rate) {
            printf "%15s %15s %15s\n", "Snapshot (us)", "Value", "Rate (/s)";
            printf "%15s %15s %15s\n", "==========", "==========",
            "==========";
        }
        else {
            printf "%15s %15s\n", "Snapshot (us)", "Value";
            printf "%15s %15s\n", "==========", "==========";
        }
        for ($i = 0; $i < $limit; $i++) {
            $currtime = $snapshot_times[$i];
            if ($raw && !defined($counter_array[$value][$i])) {
                $counter_array[$value][$i] = 0;
            }
            printf "%15d %15d", "$currtime", "$counter_array[$value][$i]";
            if ($rate) {
                printf " %15.2f", $counter_array[$value][$i] /
                    (($currtime - $prevtime)/1000000);
            }
            printf "\n";
            $prevtime = $currtime;
        }
    }
    print "\n";
}


################## main() ################

$usage = "Usage: $0 -v -d -cache_exits -cache_memory -memory <stats file>\n";

$verbose = 0;
$warning = 0;
$details = 0;
%marker_hash = ();
%valid = ();
%rate_hash = (); # tracks if a tag is a rate or not

@counter_array = ();
@rate_array = ();

$catch_cache_exits = 1;
$catch_syscalls = 1;
$catch_security = 1;
$catch_cache_memory = 0;
$catch_memory = 0;

while ($#ARGV >= 0) {
    if ($ARGV[0] eq "-v") {
        $verbose = 1;
    }
    elsif ($ARGV[0] eq "-w") {
        $warning = 1;
    }
    elsif ($ARGV[0] eq "-d") {
        $details = 1;
    }
    elsif ($ARGV[0] eq "-catch_syscalls") {
        $catch_syscalls = 1;
    }
    elsif ($ARGV[0] eq "-cache_exits") {
        $catch_cache_exits = 1;
    }
    elsif ($ARGV[0] eq "-cache_memory") {
        $catch_cache_memory = 1;
    }
    elsif ($ARGV[0] eq "-memory") {
        init_memory_tags();
    }
    else {
        $logfile = $ARGV[0];
    }
    shift;
}
if (!defined($logfile)) {
    print $usage;
    exit;
}

open(PROFILE, "< $logfile") || die "Error opening $logfile\n";

if ($catch_security) {
    init_security_tags();
}
if ($catch_syscalls) {
    init_syscall_tags();
}
if ($catch_cache_exits) {
    init_cache_exits_tags();
}
if ($catch_cache_memory) {
    init_cache_memory_tags();
}
if ($catch_memory) {
    init_memory_tags();
}

if ($verbose) {
    print STDERR "# stats to match: $marker_count...\n";
}

print "Stats from logfile $logfile\n(All rates are in per s)\n";

$marker_count = 0;

$snapshot_opens = 0;
$time_snapshot_start = 0;
$fragment_snapshot_start = 0;

# Buffer the lines pertaining to memory data
@lines = ();
$line_count = 0;

while (<PROFILE>) {

    chop($_);

    if ($verbose) {
        print STDERR "Check line $. >>$_<<\n";
    }

    if (/^Begin stats @([0-9]+):([0-9]+):([0-9]+)$/) {
        if ($time_snapshot_start || $fragment_snapshot_start) {
            if ($warning) {
                print STDERR "WARNING: Nested time stats detected, " .
                    "line $.!\n";
            }
            $snapshot_times[$snapshot_opens++] = $currtime;
        }
        $currtime = ($1 * 60 + $2) * 1000000 + $3;
        $time_snapshot_start = 1;
        $fragment_snapshot_start = 0;
        @lines = ();
        $line_count = 0;
        if ($verbose) {
            print STDERR "Begin time-based snapshot for $currtime " .
                "\@line $.\n";
        }
    }
    # An alternative to pull out only the fragment count is
    # /^All statistics @([0-9]+)+ \([0-9]+:[0-9]+\.[0-9]+\)/
    elsif (/^All statistics @([0-9]+) \(([0-9]+):([0-9]+)\.([0-9]+)\)/) {
        if ($1 == 0 && $2 == 0 && $3 == 0 && $4 == 0) {
            if ($verbose) {
                print STDERR "Skip 0-time 'All stats...'\n";
            }
            next;
        }
        elsif ($time_snapshot_start) { # time-based snapshot begin-marker
            if ($verbose) {
                print STDERR "Skip 'All stats...', gathering time stats\n";
            }
            next;
        }
        else { # Open a snapshot
            if ($fragment_snapshot_start) {
                if ($warning) {
                    print STDERR "WARNING: Nested fragment stats detected, " .
                        "line $.!\n";
                }
                $snapshot_times[$snapshot_opens++] = $currtime;
            }
            $currtime = ($2 * 60 + $3) * 1000000 + $4;
            $fragment_snapshot_start = 1;
            $time_snapshot_start = 0;
            @lines = ();
            $line_count = 0;
            if ($verbose) {
                print STDERR "Begin fragment-based snapshot $currtime " .
                    "\@line $.\n";
            }
        }
    }
    elsif (/^Thread statistics @[0-9]+ global, ([0-9]+) thread fragments \(([0-9]+):([0-9]+)\.([0-9]+)\)/) { # Begin thread-private stats
        if ($1 == 0 && $2 == 0 && $3 == 0 && $4 == 0) {
            if ($verbose) {
                print STDERR "Skip 0-time \"Thread statistics...\"\n";
            }
            next;
        }
        elsif ($time_snapshot_start) { # time-based snapshot begin-marker
            if ($verbose) {
                print STDERR "Skip \"Thread statistics...\", " .
                    "gathering time stats\n";
            }
            next;
        }
        else { # Open a snapshot
            if ($fragment_snapshot_start) {
                if ($warning) {
                    print STDERR "WARNING: Nested fragment stats detected, " .
                        "line $.!\n";
                }
                $snapshot_times[$snapshot_opens++] = $currtime;
            }
            $currtime = ($2 * 60 + $3) * 1000000 + $4;
            $fragment_snapshot_start = 1;
            $time_snapshot_start = 0;
            @lines = ();
            $line_count = 0;
            if ($verbose) {
                print STDERR "Begin fragment-based snapshot $currtime " .
                    "\@line $.\n";
            }
        }
    }
    elsif (/^End stats/) { # We see a time-based snapshot end-marker
        if ($fragment_snapshot_start) {
            print STDERR "ERROR: fragment-based stats open, " .
                "time-based trying to close, line $.!\n";
            die;
        }
        elsif (!$time_snapshot_start) {
            print STDERR "ERROR: no time-based stats open, " .
                "time-based trying to close, line $.!\n";
            die;
        }
        else { # All is well, close this snapshot
            $snapshot_times[$snapshot_opens++] = $currtime;
            $time_snapshot_start = 0;
            if ($verbose) {
                print STDERR "End time-based snapshot for $currtime\n";
            }
        }
    }
    # XXX Sometimes "/^[ ]+Peak our additional virtual size/" works??
    elsif (/^(Process|Thread) heap breakdown:$/) {
        #if (!$fragment_snapshot_start && !$time_snapshot_start) {
        #    print STDERR "ERROR: no stats window open, " .
        #        "fragment-based trying to close, line $.!\n";
        #    die;
        #}
        if ($fragment_snapshot_start) { # All is well, close this snapshot
            $snapshot_times[$snapshot_opens++] = $currtime;
            $fragment_snapshot_start = 0;
            if ($verbose) {
                print STDERR "End fragment-based snapshot for $currtime\n";
            }
        }
    }

    elsif (/^(Process|Thread) heap breakdown:$/) {
        #if (!$fragment_snapshot_start && !$time_snapshot_start) {
        #    print STDERR "ERROR: no stats window open, " .
        #        "fragment-based trying to close, line $.!\n";
        #    die;
        #}
        if ($fragment_snapshot_start) { # All is well, close this snapshot
            $snapshot_times[$snapshot_opens++] = $currtime;
            $fragment_snapshot_start = 0;
            if ($verbose) {
                print STDERR "End fragment-based snapshot for $currtime\n";
            }
        }
    }

    # We are reading a stats snapshot
    elsif ($fragment_snapshot_start || $time_snapshot_start) {
        if ($verbose) {
            print STDERR "Process line $_\n";
        }
        process_snapshot($verbose, $_);
    }
}

$median_index = $snapshot_opens / 2;

# The times are in microseconds. Let's group by milliseconds.
printf "# snapshots $snapshot_opens, median snapshot time " .
    int($snapshot_times[$median_index]/1000000) . "s\n\n";

if ($snapshot_opens == 0) {
    print "No snapshots collected!\n";
    print STDERR "No snapshots collected!\n";
    exit;
}

foreach $key (sort keys %marker_hash) {

    if (!defined($valid{$key})) {
        if ($verbose) {
            print STDERR "Bypass $key, no stats gathered...\n";
        }
        next;
    }

    # Is this a rate-oriented stat? What's the index into the
    # counter table?
    $rate = $rate_hash{$key};
    $value = $marker_hash{$key};

    # print_stats($rate, $value, $details, $snapshot_opens, 1);

    prepare_stats($snapshot_opens, $value, $rate);
    compute_stats($snapshot_opens, $key, $value, $rate);

    if ($verbose) {
        print STDERR "Print summary for $key...\n";
    }

    print "$key: median = $median_val\n" .
        "max = $max_val [snapshot time = " . int($max_time / 1000000) .
        "s]\n";

    print_stats($rate, $value, $details, $snapshot_opens, 0);
}
