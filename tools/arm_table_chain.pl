#!/usr/bin/perl

# **********************************************************
# Copyright (c) 2014-2020 Google, Inc.  All rights reserved.
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
# * Neither the name of Google, Inc. nor the names of its contributors may be
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

# Pass this the file with the op_instr[] encoding chain starting points
# array along with headers containing the table shortcuts and the files
# containing the decoding table files to chain.
# If -o OP_<opc> is passed it will only consider that opcode; else it will
# process all opcodes.
# It will construct each encoding chain and in-place edit the table files.
# It has assumptions on the precise format of the decoding table
# and of the op_instr* starting point array.
#
# To run on one A32 opcode:
#
#   tools/arm_table_chain.pl -v -o <opc> core/ir/arm/table_{private,encode,a32}*.[ch]
#
# To run on all A32 opcodes:
#
#   tools/arm_table_chain.pl core/ir/arm/table_{private,encode,a32}*.[ch]
#
# To run on one T32 opcode:
#
#   tools/arm_table_chain.pl -v -o <opc> core/ir/arm/table_{private,encode,t32}*.[ch]
#
# To run on all T32 opcodes for outside of IT blocks:
#
#   tools/arm_table_chain.pl core/ir/arm/table_{private,encode,t32}*.[ch]
#
# To run on all T32 opcodes for inside of IT blocks:
#
#   tools/arm_table_chain.pl -it core/ir/arm/table_{private,encode,t32}*.[ch]
#
# Assuming the .n format are always first and thus always the entries,
# we can run arm_table_chain.pl twice for inside and outside of IT blocks
# with T32 tables without affecting the in-table chaining.


my $verbose = 0;

die "Usage: $0 [-o OP_<opcode>] [-it] <table-files>\n" if ($#ARGV < 1);
my $single_op = '';
while ($ARGV[0] eq '-v') {
    shift;
    $verbose++;
}
if ($ARGV[0] eq '-o') {
    shift;
    $single_op = shift;
}
if ($ARGV[0] eq '-it') {
    $it_block = 1;
    shift;
}
my @allfiles = @ARGV;
my $table = "";
my $shape = "";
my $major = 0;
my $minor = 0;
my $instance = 0;
my $t32 = 0;
my @infiles;

# Remove table_t32_16.c or table_t32_16_it.c
foreach $infile (@allfiles) {
    if ($it_block) {
        push(@infiles, $infile) if ($infile !~/t32_16\.c/);
    } else {
        push(@infiles, $infile) if ($infile !~/t32_16_it\.c/);
    }
}

# Must process headers first
@infiles = sort({return -1 if($a =~ /\.h$/);return 1 if($b =~ /\.h$/); return 0;}
                @infiles);

foreach $infile (@infiles) {
    print "Processing $infile\n" if ($verbose > 0);
    $t32 = 1 if ($infile =~ /_t32_/);

    open(INFILE, "< $infile") || die "Couldn't open $file\n";
    while (<INFILE>) {
        print "xxx $_\n" if ($verbose > 2);
        if (/^#define (\w+)\s+\(ptr_int_t\)&(\w+)/) {
            $shorthand{$2} = $1;
            print "shorthand for $2 = $1\n" if ($verbose > 1);
        }
        if (/^const instr_info_t (\w+)([^=]+)=/) {
            $table = $1;
            $shape = $2;
            $major = -1;
            $minor = 0;
        }
        if (/{\s*\/\*\s*(\d+)\s*\*\/\s*$/) {
            $major++;
            $minor = 0;
        }
        if (/^\s*{(OP_\w+)[ ,]/ && ($single_op eq '' || $single_op eq $1) &&
            $1 ne 'OP_CONTD') {
            my $opc = $1;
            $instance{$opc} = 0 if (!defined($instance{$opc}));

            # Ignore duplicate encodings
            my $is_new = 1;
            my $instance_next = $instance{$opc} + 1;
            my $encoding = extract_encoding($_);
            my $hex = extract_hex($_);
            for (my $i = 0; $i < @{$entry{$opc}}; $i++) {
                if ($encoding eq $entry{$opc}[$i]{'encoding'}) {
                    $is_new = 0;
                    $dup{$opc}{$encoding} = 1;
                    # We must place the entry with the fewest required bits into the
                    # encoding chain, to allow for entries to set distinguishing
                    # immed and other varying bits on duplicate entries (e.g., b vs f
                    # for the SIMD D bit) for easier decode table reading.
                    if (hex($hex) < hex($hex{$opc}{$encoding})) {
                        $hex{$opc}{$encoding} = $hex;
                        # Replace the stored instance with this one
                        $is_new = 1;
                        $instance_next = $instance{$opc} + 1;
                        $instance{$opc} = $i;
                    }
                    print "Duplicate $hex $encoding => $hex{$opc}{$encoding}\n"
                        if ($verbose > 1);
                    last;
                }
            }
            goto dup_line if (!$is_new);

            $entry{$opc}[$instance{$opc}]{'line'} = $_;
            $entry{$opc}[$instance{$opc}]{'encoding'} = $encoding;
            $entry{$opc}[$instance{$opc}]{'hex'} = $hex;
            $hex{$opc}{$encoding} = $hex;
            die "Error: no shorthand for $table\n" if ($shorthand{$table} eq '');
            if ($shape =~ /\d/) {
                $entry{$opc}[$instance{$opc}]{'addr_short'} =
                    sprintf "$shorthand{$table}\[$major][0x%02x]", $minor;
                $entry{$opc}[$instance{$opc}]{'addr_long'} =
                    sprintf "$table\[$major][0x%02x]", $minor;
            } else {
                $entry{$opc}[$instance{$opc}]{'addr_short'} =
                    sprintf "$shorthand{$table}\[0x%02x]", $minor;
                $entry{$opc}[$instance{$opc}]{'addr_long'} =
                    sprintf "$table\[0x%02x]", $minor;
            }
            # Order for sorting:
            # + Prefer no-shift over shift
            # + Prefer shift via immed over shift via reg
            # + Prefer P=1 and U=1
            # + Prefer 8-byte over 16-byte and over 4-byte
            my $priority = $instance{$opc};
            $priority-=10   if (/sh2, i/);
            $priority-=10   if (/sh2, R/);
            $priority-=10   if (/xop_shift/);
            $priority+=10   if (/, [in]/);
            $priority+=10   if (/, M.\d/);
            $priority-=10   if (/, M.S/);
            $priority+=50   if (/PUW=1../);
            $priority+=100  if (/PUW=1.0/);
            $priority+=10   if (/PUW=.1./);
            $priority+=10   if (/[VW][ABC]q,/);
            $priority-=50   if (/[VW][ABC]d,/);
            # exop must be final member of chain
            $priority-=1000 if (/exop\[\w+\]},/);
            # + Prefer 16-bit Thumb code over 32-bit Thumb code
            $priority+=500  if (/0x([\da-f]){4},/);
            $priority+=10   if (/\bSPw\b|\bPCw\b/);
            $priority+=10   if (/\bMSPP8w\b|\bMPCP8w\b/);
            $priority+=10   if (/\bMSPDBl\b|\bMSPl\b/);
            # + We have to take PC special-case first, b/c in some cases
            #   it has a larger immed which conflicts w/ other bits in non-PC
            #   (e.g., T32 OP_ldr with MPCN12w opnd).  I don't think this
            #   generalization of that one known case as any negatives.
            $priority+=200 if ($table =~ /PC$/);

            $entry{$opc}[$instance{$opc}]{'priority'} = $priority;
            if ($verbose > 0) {
                my $tmp = $entry{$opc}[$instance{$opc}]{'addr_long'};
                print "$priority $tmp $_";
            }
            $instance{$opc} = $instance_next;
        }
      dup_line:
        $minor++ if (/^\s*{[A-Z]/);
    }
    close(INFILE);
}

foreach my $opc (keys %entry) {
    @{$entry{$opc}} = sort({$b->{'priority'} <=> $a->{'priority'}} @{$entry{$opc}});
    if ($verbose > 1) {
        print "Sorted:\n";
        for (my $i = 0; $i < @{$entry{$opc}}; $i++) {
            my $tmp = $entry{$opc}[$i]{'addr_long'};
            my $pri = $entry{$opc}[$i]{'priority'};
            print "$pri $tmp\n";
        }
    }
}

# Now edit the files in place
$^I = '.bak';
foreach $infile (@infiles) {
    @ARGV = ($infile);
    while (<>) {
        my $encoding = extract_encoding($_);
        my $hex = extract_hex($_);
        my $handled = 0;
        if (/^\s+\/\* (OP_\w+)[ ,]/ && ($single_op eq '' || $single_op eq $1)) {
            my $opc = $1;
            if (defined($entry{$opc}[0]{'addr_long'})) {
                my $start = $entry{$opc}[0]{'addr_long'};
                if ($t32) {
                    if ($it_block) {
                        # Assuming the .n format are always first and thus always
                        # the entries, we can implement here for IT block and
                        # not inside the in-table chaining.
                        if ($opc !~ /OP_cps/ && $opc !~ /OP_setend$/) {
                            # OP_cps* and OP_setend are not permitted in IT block.
                            # We exclude OP_cps* and OP_setend to avoid creating
                            # separate table for T32.32 IT block instructions.
                            s/{(&.*,\s*&.*,\s*)&.*}/{\1&$start}/;
                        }
                    } else {
                        s/{(&.*,\s*)&.*(,\s*&.*)}/{\1&$start\2}/;
                    }
                } else {
                    s/{&.*,(\s*&.*\s*&.*)}/{&$start,\1}/;
                }
            }
        }
        if (/^\s*{(OP_\w+)[ ,]/ && ($single_op eq '' || $single_op eq $1) &&
            $1 ne 'OP_CONTD') {
            my $opc = $1;
            if (defined($dup{$opc}{$encoding})) {
                # Keep the one with the fewest bits, to allow for optional bits
                # (e.g., immeds or reg bits) to be set to make the tables easier to
                # read.
                # For duplicate hex values, take 1st one.
                if ($hex eq $hex{$opc}{$encoding} &&
                    !defined($printed_dup{$opc}{$encoding})) {
                    $printed_dup{$opc}{$encoding} = 1;
                    if (@{$entry{$opc}} == 1) {
                        s/, *[\w\[\]_]+},/, END_LIST},/ unless /exop\[\w+\]},/;
                        $handled = 1;
                    }
                } else {
                    s/, *[\w\[\]_]+},/, DUP_ENTRY},/ unless /exop\[\w+\]},/;
                    $handled = 1;
                }
            }
            if (!$handled) {
                for (my $i = 0; $i < @{$entry{$opc}}; $i++) {
                    if ($_ eq $entry{$opc}[$i]{'line'}) {
                        if ($i == @{$entry{$opc}} - 1) {
                            s/, *[\w\[\]_]+},/, END_LIST},/ unless /exop\[\w+\]},/;
                        } else {
                            if (/exop\[\w+\]},/) {
                                print STDERR
                                    "ERROR: exop must be final element in chain: $_\n";
                            } else {
                                my $chain = $entry{$opc}[$i+1]{'addr_short'};
                                s/, *[\w\[\]_]+},/, $chain},/;
                            }
                        }
                        last;
                    }
                }
            }
        }
        print;
    }
}

sub extract_encoding($)
{
    my ($line) = @_;

    # Remove up through opcode name (to remove encoding hexes) and
    # final field and comments
    $line =~ s/^[^"]+"/"/;
    $line =~ s/,[^,]+}.*$//;
    $line =~ s/\s*$//;
    return $line;
}

sub extract_hex($)
{
    my ($line) = @_;
    if ($line =~ /{OP_\w+,\s*0x([0-9a-fA-F]+),/) {
        return $1;
    } else {
        return "not found";
    }
}
