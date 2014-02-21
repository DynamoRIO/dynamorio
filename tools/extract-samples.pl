#!/usr/bin/perl -w

# **********************************************************
# Copyright (c) 2011 Google, Inc.  All rights reserved.
# Copyright (c) 2004-2005 VMware, Inc.  All rights reserved.
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

### extract-samples.pl
###
### Merge output from addressquery and extract-profile to report
### DR hotspots.

$verbose = 0;
$num_routines = 10;

$usage = "Usage: $0 [-h] -symbol-file <file> -addr-file <file>\n" .
    "   -symbol-file   A file produced by address_query.pl containing\n" .
    "                  address to symbol mappings.\n" .
    "   -addr-file     An address file produced by extract-profile script.\n" .
    "   -num-routines <num>\n" .
    "                  The max # of routines to dump sample info for. The\n" .
    "                  default is $num_routines\n.";

while ($#ARGV >= 0) {
    if ($ARGV[0] eq "-v") {
        $verbose = 1;
    } elsif ($ARGV[0] eq "-h") {
        print $usage;
        exit;
    } elsif ($ARGV[0] eq "-num-routines") {
        shift;
        $num_routines = $ARGV[0];
    } elsif ($ARGV[0] eq "-symbol-file") {
        shift;
        $SYMBOL_FILE = $ARGV[0];
        unless (-f "$SYMBOL_FILE") {
            die "No file $SYMBOL_FILE\n";
        }
    } elsif ($ARGV[0] eq "-addr-file") {
        shift;
        $ADDRESS_FILE = $ARGV[0];
        unless (-f "$ADDRESS_FILE") {
            die "No file $ADDRESS_FILE\n";
        }
    }
    else {
        print $usage;
        exit;
    }
    shift;
}

if (!defined($SYMBOL_FILE) || !defined($ADDRESS_FILE)) {
    print $usage;
    exit;
}

%addr_to_symbol = ();
%symbol_hits = ();
%addr_to_srcline = ();
%srcline_hits = ();
$find_symbol = 0;

open(SYMBOL_FILE, "< $SYMBOL_FILE") or die "Error opening $SYMBOL_FILE\n";
while (<SYMBOL_FILE>) {

    chop($_);

    if ($verbose) {
        print STDERR "Process $.: <$_>\n";
    }

    # We expect lines of the form to follow:
    # 70007a70:
    # utils.c(916)
    # (700079b0)   dynamorio!read_lock_fragment_lookup+0xc0   |  (70007aa0)   dynamorio!write_lock
    if (/^[0x]??([\d|a-f]+):/) {

        $address = $1;
        if ($verbose) {
            print STDERR "Match for addr $address\n";
        }
        $find_symbol = 1;

        if ( !($_ = <SYMBOL_FILE>) ) {
            print STDERR "ERROR -- unexpectedly out of data!\n";
            exit;
        }
        chop($_);
        # The src line is something like:
        # utils.c(549)+0x2
        if (/^\(/) {
            # don't always have src line: already on symbol line
        } else {
            if ($verbose) {
                print STDERR "Src line is $_\n";
            }
            if (/^(\w+)\.(\w+)\((\d+)\)/) {
                $srcline = "$1.$2:$3";
                $addr_to_srcline{$address} = $srcline;
                $srcline_hits{$srcline} = 0;
                if ($verbose) {
                    print STDERR "Addr $address -> $addr_to_srcline{$address}\n";
                }
            }

            if ( !($_ = <SYMBOL_FILE>) ) {
                print STDERR "ERROR -- out of data, expecting src info!\n";
                exit;
            }
            chop($_);
        }
        if ($verbose) {
            print STDERR "Symbol line is $_\n";
        }

        # The symbol line is something like:
        # (70006bb0)   dynamorio!mutex_trylock+0x1c   |  (70006be0)   dynamorio!mutex_unlock
        if (/^\(([\d|a-f]+)\)[ ]+dynamorio!(\w+)/) {
            # Add the symbol name to the map
            $addr_to_symbol{$address} = $2;
            $symbol_hits{$2} = 0;
            if ($verbose) {
                print STDERR "Addr $address -> $addr_to_symbol{$address}\n";
            }
            $find_symbol = 0;
        } else {
            print STDERR "ERROR -- out of data at line $., expecting symbol info!: $_\n";
            # exit;
        }
    }
}

close(SYMBOL_FILE);

# The addr->symbol mappings are set. Reconcile these with the addr->hits
# mappings.

open(ADDRESS_FILE, "< $ADDRESS_FILE") or die "Error opening $ADDRESS_FILE\n";
$total_hits = 0;
while (<ADDRESS_FILE>) {

    chop($_);

    if ($verbose) {
        print STDERR "Process $.: <$_>\n";
    }
    if (/^[0x]??([\d|a-f]+)[ ]+(\d+)+[ ]+(\d|'.')+/) {
        $address = $1;
        $hits = $2;
        $total_hits += $hits;
        if ($verbose) {
            print STDERR "Match for addr $address -- $hits\n";
        }

        # Find the symbol name for the address.
        if (defined($addr_to_symbol{$address})) {
            $symbol = $addr_to_symbol{$address};
            $symbol_hits{$symbol} += $hits;
            if ($verbose) {
                print STDERR "Update $symbol ($address) to " .
                    " $symbol_hits{$symbol} hits\n";
            }
        } else {
            if ($verbose) {
                print STDERR "Addr $address not present in symbol file\n";
            }
        }

        # Find the src line for the address.
        if (defined($addr_to_srcline{$address})) {
            $srcline = $addr_to_srcline{$address};
            $srcline_hits{$srcline} += $hits;
            if ($verbose) {
                print STDERR "Update $srcline ($address) to " .
                    " $srcline_hits{$srcline} hits\n";
            }
        } else {
            if ($verbose) {
                print STDERR "Addr $address not present in symbol file\n";
            }
        }
    }
}
close(ADDRESS_FILE);

@array = ();
while (($key, $value) = each %symbol_hits) {
    $array[$#array + 1] = "$key $value";
    if ($verbose) {
        printf STDERR "Symbol array[$#array] -- %s\n", $array[$#array];
    }
}

sub by_hits {
    ($trash, $a_hits) = split / /, $a;
    ($trash, $b_hits) = split / /, $b;
    $b_hits <=> $a_hits;
}

@sorted_symbols = sort by_hits @array;
if ($#sorted_symbols > 0) {
    printf "\n\tHottest symbols\n\n";
    printf "%30s  %15s  %8s\n", "Symbol name", "Hits", "%-age";
    printf "%30s  %15s  %8s\n", "===========", "====", "=====";
    for ($i = 0; $i <= $#sorted_symbols; $i++) {
        if ($i > $num_routines) {
            last;
        }
        ($symbol, $hits) = split / /, $sorted_symbols[$i];
        printf "%30s  %15d  %8.2f\n", $symbol, $hits, 100 * $hits / $total_hits;
    }
}

@array = ();
while (($key, $value) = each %srcline_hits) {
    $array[$#array + 1] = "$key $value";
    if ($verbose) {
        printf STDERR "Srcline array[$#array] -- %s\n", $array[$#array];
    }
}

@sorted_srclines = sort by_hits @array;
if ($#sorted_srclines > 0) {
    printf "\n\tHottest src lines\n\n";
    printf "%30s  %15s  %8s\n", "Src line", "Hits", "%-age";
    printf "%30s  %15s  %8s\n", "=========", "====", "=====";
    for ($i = 0; $i <= $#sorted_srclines; $i++) {
        if ($i > $num_routines) {
            last;
        }
        ($srcline, $hits) = split / /, $sorted_srclines[$i];
        printf "%30s  %15d  %8.2f\n", $srcline, $hits, 100 * $hits / $total_hits;
    }
}
