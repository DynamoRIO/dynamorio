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

# Pass this a set of decoding table files from which to extract
# INSTR_CREATE_ macro signature information.
# It will construct the different sets of macros needed for each
# instruction form.
# It has assumptions on the precise format of the decoding tables and
# on the decoding table file names (specifically, that *t32* is Thumb).
#
# To run on one opcode:
#
#   tools/arm_macros_gen.pl -v -o OP_add core/ir/arm/table_{a32,t32}*.[ch]
#
# To run on everything:
#
#   tools/arm_macros_gen.pl core/ir/arm/table_{a32,t32}*.[ch]
#
# XXX i#1563: should we swap src and dst for stores and for OP_cdp and
# other instructions to more closely match the assembly order?
#
# XXX i#1563: currently we have "Rd" for the dest of a load but asm
# has it as "Rt".

my $verbose = 0;

die "Usage: $0 [-o OP_<opcode>] <table-files>\n" if ($#ARGV < 1);
my $single_op = '';
if ($ARGV[0] eq '-v') {
    shift;
    $verbose++;
}
if ($ARGV[0] eq '-o') {
    shift;
    $single_op = shift;
}
my @infiles = @ARGV;

# Must process headers first
@infiles = sort({return -1 if($a =~ /\.h$/);return 1 if($b =~ /\.h$/); return 0;}
                @infiles);

foreach $infile (@infiles) {
    print "Processing $infile\n" if ($verbose > 0);
    my $extra_num = 0;
    my $thumb = ($infile =~ /t32/) ? 1 : 0;
    open(INFILE, "< $infile") || die "Couldn't open $file\n";
    while (<INFILE>) {
        print "xxx $_\n" if ($verbose > 2);
        if (/^\s*{(OP_\w+)[ ,]/ && ($single_op eq '' || $single_op eq $1) &&
            $1 ne 'OP_CONTD') {
            my $opc = $1;
            $instance{$opc} = 0 if (!defined($instance{$opc}));
            my $encoding = $_;
            my $flags = '';
            # Get the 5 operands plus the flags and exop chain
            if ($encoding =~ /exop\[(\w+)\]},/) {
                $entry{$opc}[$instance{$opc}]{'exop'} = hex($1);
            }
            $encoding =~ s|/\*dup\*/||;
            $encoding =~ s/^[^"]+"\S+",\s*//;
            $encoding =~ s/,[^,]+,[^,]+}.*$//;
            $encoding =~ s/\s//g;
            if ($encoding =~ /,([^,]+)$/) {
                $flags = $1;
                if ($flags =~ /(xop_\w+)/) {
                    $entry{$opc}[$instance{$opc}]{'xop'} = $1;
                }
                $encoding =~ s/,[^,]+$//;
            } else {
                die "Cannot find flags: $_";
            }

            my $opstr = collapse_types($encoding, $infile);

            my @opnds = split(',', $opstr);
            my @dsts = ();
            my @srcs = ();
            push(@dsts, $opnds[0]) unless ($opnds[0] eq 'xx');
            if ($flags =~ /srcX4/) {
                push(@srcs, $opnds[1]) unless ($opnds[1] eq 'xx');
            } else {
                push(@dsts, $opnds[1]) unless ($opnds[1] eq 'xx');
            }
            if ($flags =~ /dstX3/) {
                push(@dsts, $opnds[2]) unless ($opnds[2] eq 'xx');
            } else {
                push(@srcs, $opnds[2]) unless ($opnds[2] eq 'xx');
            }
            push(@srcs, $opnds[3]) unless ($opnds[3] eq 'xx');
            push(@srcs, $opnds[4]) unless ($opnds[4] eq 'xx');

            $entry{$opc}[$instance{$opc}]{'line'} = $_;
            $entry{$opc}[$instance{$opc}]{'encoding'} = $encoding;
            $entry{$opc}[$instance{$opc}]{'opstr'} = $opstr;

            @{$entry{$opc}[$instance{$opc}]{'srcs'}} = @srcs;
            @{$entry{$opc}[$instance{$opc}]{'dsts'}} = @dsts;
            $entry{$opc}[$instance{$opc}]{'file'} = $infile;

            if ($verbose > 0) {
                print "$opstr <== $_";
            }
            $instance{$opc}++;
        } elsif (/^\s*{OP_CONTD/) {
            my $opnds = $_;
            die "Two extra-op lines not supported: $_" unless ($opnds =~ /END_LIST/);
            if ($opnds =~ /",(\s*\w+,\s*\w+,\s*\w+,\s*\w+,\s*\w+,)/) {
                my $encoding = collapse_types($1, $infile);
                my @opnds = split(',', $encoding);
                my @dsts = ();
                my @srcs = ();
                push(@dsts, $opnds[0]) unless ($opnds[0] eq 'xx');
                push(@dsts, $opnds[1]) unless ($opnds[1] eq 'xx');
                push(@srcs, $opnds[2]) unless ($opnds[2] eq 'xx');
                push(@srcs, $opnds[3]) unless ($opnds[3] eq 'xx');
                push(@srcs, $opnds[4]) unless ($opnds[4] eq 'xx');
                # Separate extra-args for Thumb and ARM
                $extra{$thumb}[$extra_num]{'line'} = $_;
                @{$extra{$thumb}[$extra_num]{'srcs'}} = @srcs;
                @{$extra{$thumb}[$extra_num]{'dsts'}} = @dsts;
            } else {
                die "Failed to parse extra-op line $_";
            }
            $extra_num++;
        }
    }
    close(INFILE);
}

foreach my $opc (keys %entry) {
    my %sigs = ();
    my $check_list = 0;
    my $shift_variant = '';
    my $shift_arg_str = '';
    my $shift_call_str = '';
    my $arg_str = '';

    for (my $i = 0; $i < @{$entry{$opc}}; $i++) {
        # Get extra args
        my $infile = $entry{$opc}[$i]{'file'};
        my $thumb = ($infile =~ /t32/) ? 1 : 0;
        if (defined($entry{$opc}[$i]{'xop'})) {
            my $xop = $entry{$opc}[$i]{'xop'};
            for (my $j = 0; $j < @{$extra{$thumb}}; $j++) {
                if ($extra{$thumb}[$j]{'line'} =~ /$xop\W/) {
                    push(@{$entry{$opc}[$i]{'srcs'}}, @{$extra{$thumb}[$j]{'srcs'}});
                    push(@{$entry{$opc}[$i]{'dsts'}}, @{$extra{$thumb}[$j]{'dsts'}});
                }
            }
        } elsif (defined($entry{$opc}[$i]{'exop'})) {
            my $exop = $entry{$opc}[$i]{'exop'};
            push(@{$entry{$opc}[$i]{'srcs'}}, @{$extra{$thumb}[$exop]{'srcs'}});
            push(@{$entry{$opc}[$i]{'dsts'}}, @{$extra{$thumb}[$exop]{'dsts'}});
        }
    }


    # Prefer the ARM register names, as the Thumb ones are often completely
    # different, and it would be a real pain to add another set of
    # fragile, hacky mappings to "semantic names".
    # We want to map from each Thumb encoding to a "similar" ARM encoding.
    # We can't do global reg slots b/c they do vary for wb vs non-wb so we
    # store the ARM reg for each slot per operand count.
    # First, store the ARM reg values:
    my %arm_src = ();
    my %arm_dst = ();
    for (my $i = 0; $i < @{$entry{$opc}}; $i++) {
        if ($entry{$opc}[$i]{'file'} !~ /t32/) {
            my $count = @{$entry{$opc}[$i]{'srcs'}};
            for (my $j = 0; $j < $count; $j++) {
                if ($entry{$opc}[$i]{'srcs'}[$j] =~ /\bR/) {
                    if (defined($arm_src{$count}{$j})) {
                        die "Src regs for $opc in slot x$count $j do not match: ".
                            $arm_src{$count}{$j} ." vs ".
                            $entry{$opc}[$i]{'srcs'}[$j] ."\n"
                            unless ($arm_src{$count}{$j} eq $entry{$opc}[$i]{'srcs'}[$j]);
                    }
                    $arm_src{$count}{$j} = $entry{$opc}[$i]{'srcs'}[$j];
                }
            }
            $count = @{$entry{$opc}[$i]{'dsts'}};
            for (my $j = 0; $j < $count; $j++) {
                if ($entry{$opc}[$i]{'dsts'}[$j] =~ /\bR/) {
                    if (defined($arm_dst{$j})) {
                        die "Dst regs for $opc in slot $j do not match: ".
                            $arm_dst{$count}{$j} ." vs ".
                            $entry{$opc}[$i]{'dsts'}[$j] ."\n"
                            unless ($arm_dst{$count}{$j} eq $entry{$opc}[$i]{'dsts'}[$j]);
                    }
                    $arm_dst{$count}{$j} = $entry{$opc}[$i]{'dsts'}[$j];
                }
            }
        }
    }
    # Now, substitute the ARM reg values into the corresponding Thumb slots:
    for (my $i = 0; $i < @{$entry{$opc}}; $i++) {
        if ($entry{$opc}[$i]{'file'} =~ /t32/) {
            my $count = @{$entry{$opc}[$i]{'srcs'}};
            for (my $j = 0; $j < $count; $j++) {
                if (($entry{$opc}[$i]{'srcs'}[$j] =~ /\bR/ ||
                     $entry{$opc}[$i]{'srcs'}[$j] =~ /\bSPw/) &&
                    defined($arm_src{$count}{$j})) {
                    print "Setting src $j from ".$entry{$opc}[$i]{'srcs'}[$j].
                        " to ". $arm_src{$count}{$j} ."\n" if ($verbose > 1);
                    $entry{$opc}[$i]{'srcs'}[$j] = $arm_src{$count}{$j};
                }
            }
            $count = @{$entry{$opc}[$i]{'dsts'}};
            for (my $j = 0; $j < $count; $j++) {
                if (($entry{$opc}[$i]{'dsts'}[$j] =~ /\bR/ ||
                     $entry{$opc}[$i]{'dsts'}[$j] =~ /\bSPw/) &&
                    defined($arm_dst{$count}{$j})) {
                    print "Setting dst $j from ".$entry{$opc}[$i]{'dsts'}[$j].
                        " to ". $arm_dst{$count}{$j} ."\n" if ($verbose > 1);
                    $entry{$opc}[$i]{'dsts'}[$j] = $arm_dst{$count}{$j};
                }
            }
        }
    }

    for (my $i = 0; $i < @{$entry{$opc}}; $i++) {
        # Create a single-string signature line
        my $sig = join " ",@{$entry{$opc}[$i]{'dsts'}};
        $sig .= "; ";
        $sig .= join " ",@{$entry{$opc}[$i]{'srcs'}};

        # Combine first and consec into single var-arg list
        $sig =~ s/WBd LCd/LCd/;
        $sig =~ s/VBq LCq/LCq/;

        # All types of lists look the same
        $sig =~ s/LX\w+/LX/;
        $sig =~ s/LC\w+/LCx/;
        # We collapse SIMD reg sizes
        $sig =~ s/([VW][A-Z]+)[a-z]+/\1x/g;
        # We use V for everything like AArch64 does (XXX i#1563: could be nice
        # to list the S or D name for asm but we are collapsing sizes...)
        $sig =~ s/W/V/g;
        # Special case vmrs
        $sig =~ s/\bCPSR/RBd/ if ($opc eq 'OP_vmrs');
        # Collapse spsr and cpsr
        $sig =~ s/\bCPSR/statreg/;
        $sig =~ s/\bSPSR/statreg/;

        if (!defined($sigs{$sig})) {
            $sigs{$sig} = $sig;
            $check_list = 1 if ($sig =~ /\bL[XC]/);
            $shift_variant = $sig if (($sig =~ /\ssh2 i/ || $sig =~ /\ssh2 R/) &&
                                      # only arith srcs, not shifts in mem opnds
                                      $sig !~ /M/);
        }

        print "Entry #$i: $sig\n" if ($verbose > 0);
    }

    if ($check_list) {
        # Avoid a separate instance for singleton vs list of regs
        my @ksig = keys %sigs;
        foreach my $sig (@ksig) {
            # We have to replace in this direction b/c there can be multiple single reg
            # opnds and we only want the one that lines up w/ the list
            if ($sig =~ /\bLX/) {
                my $sig_singleB = $sig;
                my $sig_singleA = $sig;
                $sig_singleB =~ s/\bLX\w*/VBx/; # V[AB]x only singleton type so far
                $sig_singleA =~ s/\bLX\w*/VAx/; # V[AB]x only singleton type so far
                if (defined($sigs{$sig_singleB})) {
                    delete $sigs{$sig_singleB};
                } elsif (defined($sigs{$sig_singleA})) {
                    delete $sigs{$sig_singleA};
                }
            }
        }
    }

    my $immed_name = 0;
    my @ksig = keys %sigs;
    if ($#ksig == 1) {
        if (($ksig[0] =~ /\bi/ && $ksig[1] !~ /\bi/) ||
            ($ksig[1] =~ /\bi/ && $ksig[0] !~ /\bi/)) {
            # See if have same # args and thus candidate for "Rm_or_immed"
            my @sp0 = $ksig[0] =~ / /g;
            my @sp1 = $ksig[1] =~ / /g;
            if ($#sp0 == $#sp1 &&
                # Rule out FPSCR vs immed (OP_vmrs).
                !($ksig[0] =~ /; FPSCR/ && $ksig[1] =~ /; i/) &&
                !($ksig[0] =~ /; i/ && $ksig[1] =~ /; FPSCR/)) {
                # Distinguished by immed but let's make the macros more versatile
                # by combining into a variable-type "Rm_or_immed" arg rather than
                # naming $name_imm.
                my $newsig;
                $newsig = ($ksig[0] =~ /\bi/) ? $ksig[1] : $ksig[0];
                $newsig =~ s/\s*$/_or_imm/;
                delete $sigs{$ksig[0]};
                delete $sigs{$ksig[1]};
                $sigs{$newsig} = $newsig;
            } else {
                $immed_name = 1;
            }
        }
    }

    if ($shift_variant ne '') {
        # Add a simple version w/o the shifted reg
        my $replaced = 0;
        foreach my $sig (keys %sigs) {
            # When we add a simple reg-reg DR_SHIFT_NONE, and a reg-imm or reg-reg
            # exists, combine them into a variable-type "Rm_or_immed" arg
            # rather than forcing the reg-imm to be $name_imm.
            if ($sig =~ /^R\w+;\s*\bi$/ ||
                $sig =~ /^;\s*R\w+\s*\bi$/ ||
                $sig =~ /^;\s*R\w+\s*R\w+$/ ||
                $sig =~ /^R\w+;\s*R\w+$/ ||
                $sig =~ /^R\w+;\s*R\w+\s*\bi$/ ||
                $sig =~ /^R\w+;\s*R\w+\s*\bR\w+$/) {
                my $newsig = $sig;
                my $with_reg = $shift_variant;
                $with_reg =~ s/\s*sh2\s*\w+$//;
                my $check = $with_reg;
                $check =~ s/R\w+\s*$//;
                if ($newsig =~ /\bi/) {
                    $newsig =~ s/\bi\s*$//;
                } else {
                    $newsig =~ s/\bR\w+\s*$//;
                }
                die "Unknown shift variant $opc |$shift_variant|: |$check| vs |$newsig|\n"
                    unless ($check eq $newsig);
                $newsig = $with_reg . "_or_imm_specialshift";
                delete $sigs{$sig};
                $sigs{$newsig} = $newsig;
                $replaced = 1;
            }
        }
        if (!$replaced) {
            my $newsig = $shift_variant;
            $sigs{$newsig} = $newsig;
            $newsig =~ s/\s*sh2\s*\w+$/_specialshift/;
            $sigs{$newsig} = $newsig;
        }
    }

    foreach my $sig (sort keys %sigs) {
        print "Sig: $sig\n" if ($verbose > 0);
        my $name = $opc;
        $name =~ s/^OP_//;

        # Friendly names for things we don't care about for macro names
        $sig =~ s/\bi\w*/imm/g;
        $sig =~ s/\bj\w*/pc/g;
        $sig =~ s/\bM/mem/g;
        $sig =~ s/\bsh\d/shift/g;
        $sig =~ s/\bSPw/sp/g;
        $sig =~ s/\bCR\w+/cpreg/g;

        # Ordinals for dup names
        $sig =~ s/imm\b(.*)imm\b(.*)imm\b/imm\1imm2\2imm3/;
        $sig =~ s/imm\b(.*)imm\b/imm\1imm2/;
        $sig =~ s/cpreg\b(.*)cpreg\b(.*)cpreg\b/cpreg\1cpreg2\2cpreg3/;
        $sig =~ s/cpreg\b(.*)cpreg\b/cpreg\1cpreg2/;

        # Total arg counts
        my $sig_src = $sig;
        $sig_src =~ s/^.*;//;
        my @count = $sig_src =~ /\w+/g;
        my $num_tot_srcs = scalar @count;
        my $sig_dst = $sig;
        $sig_dst =~ s/;.*$//;
        @count = $sig_dst =~ /\w+/g;
        my $num_tot_dsts = scalar @count;

        # String for explicit args
        my $esig = $sig;

        # Look for writeback => name and implicit args
        if ($sig =~ /\bmem/ &&
            ($sig =~ /RAw.*;.*RAw/ || $sig =~ /sp;.*sp/) &&
            $num_tot_dsts > 0) {
            # For us, writeback or post-indexed look the same.  We have
            # two variants: immed disp or index (possibly shifted) reg.
            if ($sig =~ /;.*RD/) {
                $name .= "_wbreg" unless (scalar(keys %sigs) == 1);
            } elsif ($sig =~ /;.*\bi/) {
                $name .= "_wbimm" unless (scalar(keys %sigs) == 1);
            } else {
                $name .= "_wb" unless (scalar(keys %sigs) == 1);
            }
            if ($sig =~ /sp;.*sp/) {
                $sig =~ s/sp/opnd_create_reg(opnd_get_base(mem))/g;
                $esig =~ s/\s*sp//g;
            } else {
                $sig =~ s/RAw/opnd_create_reg(opnd_get_base(mem))/g;
                $esig =~ s/\s*RAw//g;
            }
            $sig =~ s/RDw/opnd_create_reg_ex(opnd_get_reg(Rm) 0 DR_OPND_SHIFTED)/;
        } elsif ($sig =~ /\bshift R/) {
            $name .= "_shreg";
            $sig =~ s/RDw/opnd_create_reg_ex(opnd_get_reg(Rm) 0 DR_OPND_SHIFTED)/;
        } elsif ($sig =~ /\bshift imm/) {
            $name .= "_shimm";
            $sig =~ s/RDw/opnd_create_reg_ex(opnd_get_reg(Rm) 0 DR_OPND_SHIFTED)/;
        } elsif ($immed_name && $sig =~ /\bimm/) {
            $name .= "_imm";
        } elsif ($sig =~ /\bSPSR/) {
            $name .= "_spsr";
            $sig =~ s/SPSR/opnd_create_reg(DR_REG_SPSR)/g;
            $esig =~ s/\s*SPSR//g;
        } elsif ($sig =~ /\bSPSR/) {
            $name .= "_spsr";
            $sig =~ s/SPSR/opnd_create_reg(DR_REG_SPSR)/g;
            $esig =~ s/\s*SPSR//g;
        } elsif ($sig =~ /RAw RBw; RAw RBw/) {
            # OP_smlal{b,t}{b,t}
            $esig =~ s/; RAw RBw/;/g;
        } elsif ($sig =~ /VC\d._/) {
            # Things like OP_vmull, OP_vmls
            $name .= "_scalar";
        } elsif ($opc eq 'OP_msr' && $sig =~ /\bimm2$/) {
            # Distinguished by immed
            $name .= "_imm";
        } elsif (($opc eq 'OP_msr_priv' || $opc eq 'OP_mrs_priv') &&
                 $sig =~ /\bstatreg\b/) {
            # Distinguished by immed
            $name .= "_spsr";
        } elsif ($opc =~ /OP_cpsi/ && $sig =~ /\bimm.*\bimm/) {
            $name .= "_noflags";
        } elsif ($opc =~ /OP_vmov_32$/) {
            if ($sig =~ /V.*;/) {
                $name .= "_g2s";
            } else {
                $name .= "_s2g";
            }
        } elsif ($opc eq 'OP_vmov') {
            # Using C++ function overloads would help here.
            # XXX: could we create a vararg version?  But how know #dsts?
            # User passes it in and we add instr_create_Ndst_varsrc()?
            if ($sig =~ /^V\w+;\s*R\w+$/) {
                $name .= "_g2s";
            } elsif ($sig =~ /^R\w+;\s*V\w+$/) {
                $name .= "_s2g";
            } elsif ($sig =~ /^V\w+;\s*R\w+\s*R/) {
                $name .= "_gg2s";
            } elsif ($sig =~ /^R\w+\s*R\w+;\s*V\w+$/) {
                $name .= "_s2gg";
            } elsif ($sig =~ /^V\w+\s*V\w+;\s*R\w+\s*R/) {
                $name .= "_gg2ss";
            } else {
                $name .= "_ss2gg";
            }
        } elsif (($opc =~ /^OP_stc/ || $opc =~ /^OP_ldc/) &&
                 $sig eq 'cpreg; mem imm imm2') {
            $name .= "_option";
        } elsif ($esig =~ /RBw; .* RBw$/) {
            # Implicit arg for OP_bfc and OP_bfi
            $esig =~ s/RBw$//;
        } elsif ($esig =~ /mem.*; mem/) {
            # Implicit arg for OP_swp*
            $esig =~ s/^mem//;
        }

        if ($sig =~ /\bshift\b/) {
            $sig =~ s/\bshift/opnd_add_flags(shift DR_OPND_IS_SHIFT)/;
        }

        # Hardcoded implicit args:
        # SPw, LSL, and ASR, and the k8, k16, k32 consts are explicit in the asm.
        # LRw for OP_bl is not.
        if ($sig =~ /\bLRw/) {
            $sig =~ s/LRw/opnd_create_reg(DR_REG_LR)/;
            $esig =~ s/\s*LRw//g;
        }
        if ($sig =~ /\bFPSCR/) {
            $sig =~ s/FPSCR/opnd_create_reg(DR_REG_FPSCR)/;
            $esig =~ s/\s*FPSCR//g;
        }

        # Special docs
        if ($opc eq 'OP_msr' && $sig !~ /imm2/) {
            $sig =~ s/imm/imm_msr/;
            $esig =~ s/imm/imm_msr/;
        }

        $sig = rename_regs($sig);
        $esig = rename_regs($esig);

        # List of total (including implicit) args
        my $call_str = $sig;
        $call_str =~ s/;//g;
        $call_str =~ s/\s*$//;
        $call_str =~ s/^\s*//;
        $call_str =~ s/\b(\w+)/, \1/g;
        $call_str =~ s/\b([\w\(\)]+)\s+/\1/g;
        $call_str =~ s/\b([A-Za-z0-9_]+)\b/(\1)/g;
        $call_str =~ s/\((opnd_\w+)\)/\1/g;
        $call_str =~ s/\((\d+)\)/\1/g;
        $call_str =~ s/\((DR_[A-Z_]+)\)/\1/g;
        $call_str =~ s/\(, /(/g;
        $call_str =~ s/\(\((\w+)\)\)/(\1)/g;

        # List of explicit args
        $arg_str = $esig;
        $arg_str =~ s/;//g;
        $arg_str =~ s/\s*$//g;
        $arg_str =~ s/^\s*//g;
        $arg_str =~ s/\b(\w+)/, \1/g;
        $arg_str =~ s/\b(\w+)\s+/\1/g;

        my $opc_str = $opc;
        my $func_sfx = '';

        # List => vararg with list at end of course
        if ($sig =~ /\bL/) {
            #  Figure out at which index in src/dst list the vararg should be inserted
            my $idx = 0;
            my @ops = split(' ', $sig);
            for (my $i = 0; $i <= $#ops; $i++) {
                last if ($ops[$i] =~ /^L/);
                $idx++;
                $idx = 0 if ($ops[$i] =~ /;$/);
            }
            $arg_str =~ s/, L\w+//;
            $call_str =~ s/, \(L\w+\)//;
            $arg_str .= ", list_len, ...";
            $call_str .= ", __VA_ARGS__";
            if ($sig =~ /;.*\bL/) {
                $func_sfx = '_varsrc';
                $opc_str .= ", $num_tot_dsts, ".($num_tot_srcs-1);
            } else {
                $func_sfx = '_vardst';
                $opc_str .= ", ".($num_tot_dsts-1).", $num_tot_srcs";
            }
            $opc_str .= ", list_len, $idx";
            $num_tot_dsts = 'N';
            $num_tot_srcs = 'M';
        }
        print "  Macro sigs: ($esig) | $sig\n" if ($verbose > 0);
        die "XXXX Duplicate macro $name for |$sig|\n" if (defined($dupcheck{$name}));
        $dupcheck{$name} = 1;

        if ($sig =~ /_specialshift/ && $sig !~ /_imm_specialshift/) {
            $sig =~ s/_specialshift//;
            $arg_str =~ s/_specialshift//;
            $call_str =~ s/_specialshift/, DR_SHIFT_NONE, 0/;
        }
        if ($sig =~ /_or_imm_specialshift/) {
            $sig =~ s/_specialshift//;
            $arg_str =~ s/_specialshift//;
            $call_str =~ s/_specialshift//;
            $arg_str =~ /\s+(\S+)$/;
            $last_arg = $1;
            my $mac = sprintf "#define INSTR_CREATE_%s(dc%s) \\\n".
                "  (opnd_is_reg(%s) ? \\\n".
                "   INSTR_CREATE_%s_shimm((dc)%s, \\\n".
                "     OPND_CREATE_INT8(DR_SHIFT_NONE), OPND_CREATE_INT8(0)) : \\\n".
                "   instr_create_%sdst_%ssrc%s((dc), %s%s))\n",
                $name, $arg_str, $last_arg, $name, $call_str, $num_tot_dsts,
                $num_tot_srcs, $func_sfx, $opc_str, $call_str;
            push(@{$macro{$arg_str}}, ($mac));
        } else {
            my $mac = sprintf "#define INSTR_CREATE_%s(dc%s) \\\n".
                "  instr_create_%sdst_%ssrc%s((dc), %s%s)\n",
                $name, $arg_str, $num_tot_dsts, $num_tot_srcs, $func_sfx,
                $opc_str, $call_str;
            push(@{$macro{$arg_str}}, ($mac));
        }
    }
}

foreach my $args (keys %macro) {
    $order{$args} = order_sig($args);
}

my @order = sort({$order{$a} <=> $order{$b}} keys %macro);

my %mapping = ('reg' => 'register',
               'Rd' => 'destination register',
               'Rd2' => 'second destination register',
               'Rn' => 'source register',
               'Rn_or_imm' => 'source register, or integer constant,',
               'Rt' => 'source register',
               'Rt2' => 'second source register',
               'Rm' => 'second source register',
               'Rm_or_imm' => 'second source register, or integer constant,',
               'Ra' => 'third source register',
               'Rs' => 'third source register',
               'Vd' => 'destination SIMD register',
               'Vd2' => 'second destination register',
               'Vn' => 'source SIMD register',
               'Vt' => 'source SIMD register',
               'Vt2' => 'second source SIMD register',
               'Vm' => 'second source SIMD register',
               'Vm2' => 'third source SIMD register',
               'Vm_or_imm' => 'source SIMD register, or integer constant,',
               'Vn_or_imm' => 'source SIMD register, or integer constant,',
               'Vs' => 'third source SIMD register',
               'imm' => 'integer constant',
               'imm_msr' => 'integer constant (typically from OPND_CREATE_INT_MSR*)',
               'imm2' => 'second integer constant',
               'imm3' => 'third integer constant',
               'pc' => 'program counter constant',
               'mem' => 'memory',
               'shift' => 'dr_shift_type_t integer constant',
               'cpreg' => 'coprocessor register',
               'cpreg2' => 'second coprocessor register',
               'cpreg3' => 'third coprocessor register',
               'statreg' => 'status register (usually DR_REG_CPSR)',
               'sp' => 'stack pointer');

foreach my $args (@order) {
    print '
/** @name Signature: ';
    my $sigline = "($args)";
    $sigline =~ s/\(, /(/;
    print $sigline;
    print ' */
/** @{ */ /* start doxygen group (via DISTRIBUTE_GROUP_DOC=YES). */
/**
 * This INSTR_CREATE_xxx macro creates an instr_t with opcode OP_xxx and
 * the given explicit operands, automatically supplying any implicit operands.
 * The operands should be listed with destinations first, followed by sources.
 * The ordering within these two groups should follow the conventional
 * assembly ordering.
 * \param dc The void * dcontext used to allocate memory for the instr_t.
';
    $sigline =~ s/\(//;
    $sigline =~ s/\)//;
    $sigline =~ s/\s*//g;
    my @params = split(',', $sigline);
    my %count;
    my $saw_n = 0;
    foreach my $param (@params) {
        $saw_n = 1 if ($param =~ /[VR]n/);
        my $tomap = $param;
        # Singleton src => no "second"
        $tomap =~ s/m/n/ if ($param =~ /[VR]m/ && !$saw_n);
        if ($tomap eq 'list_len') {
            print " * \\param $param The number of registers in the register list.\n";
        } elsif ($tomap eq '...') {
            print " * \\param $param The register list as separate opnd_t arguments.\n";
            print " * The registers in the list must be in increasing order.\n";
        } else {
            die "XXXX No mapping for $param\n" if (!defined($mapping{$tomap}));
            my $full = $mapping{$tomap};
            $count{$param}++;
            die "XXXX Duplicate name $param\n" unless ($count{$param} == 1);
            print " * \\param $param The $full opnd_t operand.\n";
        }
    }
    print " */\n";
    foreach my $mac (sort @{$macro{$args}}) {
        print $mac;
    }
    print '/** @} */ /* end doxygen group */
';
}

sub collapse_types($, $)
{
    my ($str, $infile) = @_;
    $str =~ s/\s//g;
    # Collapse opnds that will look identical to the macro and for which
    # we don't need to distinguish later
    $str =~ s/\bM\w+/M/g;
    $str =~ s/\bn\w+/i/g; # Negative immed == positive
    $str =~ s/\bi\w+/i/g; # Type of immed doesn't matter
    $str =~ s/\bj\w+/j/g; # Type of jump immed doesn't matter
    $str =~ s/\bro2\w*/i/; # Rotate-by
    $str =~ s/\bk\w+/i/; # Hardcoded const => immed (all are explicit in asm)
    $str =~ s/\b(R\w)N(\w)/\1\2/g; # User must negate registers
    $str =~ s/\bLSL/shift/g;
    $str =~ s/\bASR/shift/g;
    $str =~ s/\bL[\dPL]+/L/;
    # Thumb-only rules
    if ($infile =~ /t32/) {
        # This is only there when encoding
        $str =~ s/RA_EQ_D\w+/xx/;
        # We're not adding special macros for implicit regs or for
        # destructive src==dst forms.  We assume there's a general form
        # (sometimes only w/ our special DR_SHIFT_NONE implicit) for each.
        $str =~ s/,RV/,RA/; # destructive src => diff name (no special macro for it)
        $str =~ s/^SPw/RBw/;
        $str =~ s/\bSPw/RAw/ unless ($str =~ /\bM/); # don't do this for srs
        $str =~ s/\bPC/RA/;
        # We map Thumb names to the corresponding ARM names
        $str =~ s/sh2_4/sh2/;
        $str =~ s/sh1_21/sh1/;
        $str =~ s/^RCw/RBw/; # T32.W often have RCw as dst
        $str =~ s/\bRU/RD/g;
        $str =~ s/\bRV/RB/g;
        $str =~ s/\bRW/RA/g;
        $str =~ s/\bRX/RD/g;
        $str =~ s/\bRY/RA/g;
        $str =~ s/^RZ/RB/g;
        $str =~ s/\bRZ/RA/g;
    }
    return $str;
}

sub order_sig($)
{
    my ($a) = @_;
    my $ord = 0;
    $ord += 100000000 if ($a =~ /V/);
    $ord += 10000000 if ($a =~ /cpreg/);
    $ord += 1000000 if ($a =~ /\.\.\./);
    $ord += 100000 if ($a =~ /mem/);
    $ord += 10000 if ($a =~ /shift/);
    $ord += 1000 if ($a =~ /imm/);
    @commas = $a =~ /,/g;
    $ord += 100*@commas;
    for (my $i = 0; $i < length($a); $i++) {
        $ord += ord(substr($a, $i, 1))/(($i+1));
    }
    return $ord;
}

# Should be called after removing implicit regs (writeback regs)
sub rename_regs($)
{
    my ($str) = @_;
    if ($str !~ /_or_imm/) {
        # Remove subreg modifiers (can't remove earlier)
        $str =~ s/\b([RV]\w[^E2])\w+/\1/;
    }
    # Try to match assembly names
    # XXX i#1563: currently loads use Rd -- using Rt would add complexity
    # We also have several other breaks from asm.
    if ($str =~ /\b[RV]A.*\b[RV]B.*;/) {
        $str =~ s/\b([RV])A\w/\1d/g; # g is for things like OP_smlalbb
        $str =~ s/\b([RV])B\w/\1d2/g;
        $str =~ s/\b([RV])C\w/\1m/;
        $str =~ s/\bRD\w/Rn/;
    } elsif ($str =~ /\bVC.*\bVC2.*/) { # OP_vmov
        # I'm breaking w/ asm to make things clearer and easier for the
        # docs mapping for what is a dst and which is 1st vs 2nd src:
        if ($str =~ /\bVC.*;/) {
            $str =~ s/\bRA\w/Rt2/;
            $str =~ s/\bRB\w/Rt/;
            $str =~ s/\bVC[^0-9]/Vd/; # asm has Vm, Vm2
            $str =~ s/\bVC2\w/Vd2/;
        } else {
            $str =~ s/\bRA\w/Rd2/; # asm has Rt, Rt2
            $str =~ s/\bRB\w/Rd/;
            $str =~ s/\bVC[^0-9]/Vt/; # asm has Vm, Vm2
            $str =~ s/\bVC2\w/Vt2/;
        }
    } elsif ($str =~ /\bVC.*;.*\bRB.*\bRA/) { # OP_vmov
        # I'm breaking w/ asm to make things clearer and easier for the
        # docs mapping for what is a dst:
        $str =~ s/\bRA\w/Rt2/;
        $str =~ s/\bRB\w/Rt/;
        $str =~ s/\bVC\w/Vd/;
    } elsif ($str =~ /\b[RV]A.*;/) {
        if ($str =~ /\bVA.*;/) {
            $str =~ s/\b([RV])B\w/\1t/;
        } else {
            $str =~ s/\b([RV])B\w/\1a/;
        }
        $str =~ s/\b([RV])A\w/\1d/;
        $str =~ s/\b([RV])C\w/\1m/;
        $str =~ s/\bRD\w/Rn/;
    } elsif ($str =~ /;.*\b[RV]DE.*\b[RV]D2.*/) { # OP_stlexd
        $str =~ s/\b([RV])A\w/\1n/;
        $str =~ s/\b([RV])B\w/\1d/;
        $str =~ s/\bRC\w/Rs/;
        $str =~ s/\b([RV])DE\w/\1t/;
        $str =~ s/\b([RV])D2\w/\1t2/;
    } elsif ($str =~ /\b[RV]BE.*\b[RV]B2.*;/) {
        $str =~ s/\b([RV])A\w/\1n/;
        $str =~ s/\b([RV])BE\w/\1d/;
        $str =~ s/\b([RV])B2\w/\1d2/;
        $str =~ s/\bRC\w/Rs/;
        $str =~ s/\bVC\w/Vm/;
        $str =~ s/\bRD\w/Rm/;
    } elsif ($str =~ /\bRB\w; RD\w RC\w+$/) { # OP_lsl
        $str =~ s/\bRB\w/Rd/;
        $str =~ s/\bRC\w/Rm/;
        $str =~ s/\bRD\w/Rn/;
    } elsif ($str =~ /\b[RV]B.*;/) {
        $str =~ s/\b([RV])A\w/\1n/;
        $str =~ s/\b([RV])B\w/\1d/g;
        $str =~ s/\bRC\w/Rs/;
        $str =~ s/\bVC\w/Vm/;
        $str =~ s/\bRD\w/Rm/;
    } elsif ($str =~ /;.*\b[RV]BE.*\b[RV]B2.*/) {
        $str =~ s/\b([RV])A\w/\1n/;
        $str =~ s/\b([RV])BE\w/\1t/;
        $str =~ s/\b([RV])B2\w/\1t2/;
        $str =~ s/\bRC\w/Rs/;
        $str =~ s/\bVC\w/Vm/;
        $str =~ s/\bRD\w/Rm/;
    } elsif ($str =~ /;.*\b[RV]B.*/) {
        $str =~ s/\b([RV])A\w/\1n/;
        $str =~ s/\b([RV])B\w/\1t/;
        $str =~ s/\bRC\w/Rs/;
        $str =~ s/\bVC\w/Vm/;
        $str =~ s/\bRD\w/Rm/;
    } elsif ($str !~ /\b[RV].*;/ && $str =~ /;.*\b[RV]\w/) {
        # Single regs for post-indexing or small # opnds
        $str =~ s/\b([RV])A\w/\1n/;
        $str =~ s/\b([RV])B\w/\1d/;
        $str =~ s/\bRC\w/Rs/;
        $str =~ s/\bVC\w/Vm/;
        $str =~ s/\bRD\w/Rm/;
    } elsif ($str =~ /\bRC\w; RA\w RD\w$/) {
        $str =~ s/\bRC\w/Rd/;
        $str =~ s/\bRA\w/Rn/;
        $str =~ s/\bRD\w/Rm/;
    } elsif ($str =~ /\b[RV]\w/) {
        die "XXXX No reg mapping for $str\n";
    }
    return $str;
}
