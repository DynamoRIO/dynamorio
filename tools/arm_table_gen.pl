#!/usr/bin/perl

# **********************************************************
# Copyright (c) 2014-2015 Google, Inc.  All rights reserved.
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

# Feed this the text from the ARM manual for the A32 instructions.

my $verbose = 0;
my $line = 0;
my $pred = 1; # Process predicated instrs, or non-pred?
my $simd = 0; # Assume SIMD?
my $t32 = 0; # Look for T32 instrs

while ($#ARGV >= 0) {
    if ($ARGV[0] eq '-nopred') {
        $pred = 0;
    } elsif ($ARGV[0] eq '-v') {
        $verbose++;
    } elsif ($ARGV[0] eq '-simd') {
        $simd = 1;
    } elsif ($ARGV[0] eq '-t32') {
        $t32 = 1;
    } else {
        die "Unknown argument $ARGV[0]\n";
    }
    shift;
}

while (<>) {
    $line++;
    chomp;
    chomp if (/\r$/); # DOS
    print "xxx $line $_\n" if ($verbose > 1);
  startover:
    if ((!$t32 && (/^Encoding A/ || /^Encoding ..\/A/)) ||
        ($t32  && (/^Encoding T/))) {
        my $name;
        my $asm;
        while (<>) {
            $line++;
            chomp;
            chomp if (/\r$/); # DOS
            if (/^ARMv/) {
                $flags .= "|v8" if (/^ARMv8/);
                last;
            } elsif (/^[A-Z].*<.*<.*>$/) {
                # Sometimes the encoding is after the name in the .text version
                goto at_name;
            }
            goto startover if (/^Encoding /); # some descriptions have Encoding A...
        }
        while (<>) {
            $line++;
            chomp;
            chomp if (/\r$/); # DOS
            next if (/^ARMv/);
            next if ($_ !~ /^[A-Z][A-Z]/ && $_ !~ /^[A-Z]</);
            last;
        }
        last if eof();
      at_name:
        if (/^(\w+)/) {
            $name = $1;
            $asm = $_;
        } else {
            print "unexpected asm on line $line: $_\n";
        }
        print "found $name: $asm\n" if ($verbose);
        my $last = "";
        while (<>) {
            $line++;
            chomp;
            chomp if (/\r$/); # DOS
            my $prefix = '';
            if ($t32) {
                if ($last =~ /^15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 0 15/) {
                    $prefix = "1 1 1 .";
                }
            } elsif (!$pred) {
                if ($last =~ /^31 30 29/ && /^1 1 1 1/) {
                    $prefix = "1 1 1 1";
                }
            } elsif (/^cond/) {
                $prefix = "cond";
            }
            if ($prefix ne '') {
                # We encode the "x x x P U {D,R} W S" specifiers either into
                # our opcodes or we have multiple entries with encoding chains.
                my $enc = $_;
                if (/^$prefix\s+((\(?[01PUWSRDQi]\)? ){8})(.*)/) {
                    my $opc = $1;
                    my $rest = $3;
                    print "matched $name $enc\n" if ($verbose);
                    # Ignore parens: go w/ value inside.
                    $opc =~ s/\(//g;
                    $opc =~ s/\)//g;
                    generate_entry(lc($name), $asm, $enc, $opc, $rest, 0);
                } elsif (/^$prefix\s+((\(?[01PUWSRDQi]\)? ){6})(.*)/) {
                    my $opc = $1 . "0 0";
                    my $rest = $3;
                    print "matched $name $enc\n" if ($verbose);
                    # Ignore parens: go w/ value inside.
                    $opc =~ s/\(//g;
                    $opc =~ s/\)//g;
                    generate_entry(lc($name), $asm, $enc, $opc, $rest, 0);
                } elsif (/^$prefix\s+((\(?[01PUWSRDQi]\)? ){4})(.*)/) {
                    my $opc = $1 . "0 0 0 0";
                    my $rest = $3;
                    print "matched $name $enc\n" if ($verbose);
                    # Ignore parens: go w/ value inside.
                    $opc =~ s/\(//g;
                    $opc =~ s/\)//g;
                    generate_entry(lc($name), $asm, $enc, $opc, $rest, 0);
                } elsif (/^$prefix\s+((\(?[01PUWSRDQi]\)? ){2})(.*)/) {
                    my $opc = $1 . "0 0 0 0 0 0";
                    my $rest = $3;
                    print "matched $name $enc\n" if ($verbose);
                    # Ignore parens: go w/ value inside.
                    $opc =~ s/\(//g;
                    $opc =~ s/\)//g;
                    generate_entry(lc($name), $asm, $enc, $opc, $rest, 0);
                } else {
                    print "no match for $name: $_\n";
                }
                last;
            }
            goto startover if (/^Encoding /);
            $last = $_;
        }
    }
}

sub generate_entry($,$,$,$,$,$)
{
    my ($name, $asm, $enc, $opc, $rest, $PUW) = @_;
    my $eflags = "x";
    my $other_opc;
    my $other_enc;
    my $other_rest;
    my $negative = 0;
    my $hexopc = 0;

    # Ensure we've got all the bits, and fill in opcode for lower bits
    my %bitlen = (
        '(0)' => 1, '(1)' => 1, '(S)' => 1,
        'J1' => 1, 'J2' => 1,
        'Rn' => '4', 'Rd' => 4, 'Rt' => 4, 'Rt2' => 4, 'Rs' => 4, 'Rm' => 4, 'Ra' => 4,
        'RdHi' => 4, 'RdLo' => 4,
        'CRd' => 4, 'CRn' => 4, 'CRm' => 4,
        'Vn' => '4', 'Vd' => 4, 'Vt' => 4, 'Vm' => 4, 'Va' => 4,
        'imm2' => 2, 'imm3' => 3, 'imm4' => 4, 'imm5' => 5, 'imm6' => 6, 'imm8' => 8,
        'imm10' => 10, 'imm11' => 11, 'imm12' => 12, 'imm24' => 24,
        'imm10H' => 10, 'imm10L' => 10, 'imm4H' => 4, 'imm4L' => 4,
        'sat_imm4' => 4, 'sat_imm5' => 5,
        'type' => 2, # shift type
        'type_vld' => 4, # OP_vld1
        'cond' => 4,
        'option' => 4,
        'msb' => 5, 'lsb' => 5,
        'coproc' => 4, 'opc1' => 4, 'opc2' => 3, # OP_cdp
        'opc1_mcr' => 3, # OP_mcr
        'opc1_vmov' => 2, 'opc2_vmov' => 2, # OP_vmov
        'opt' => 2, # OP_dcps
        'register_list_t32' => 13, # for T32
        'register_list' => 16, # for A32
        'register_list_priv' => 15, # for A32 priv ldm
        'mask' => 2, # OP_msr
        'mask_priv' => 4, # OP_msr priv
        'tb' => 1, # OP_pkh
        'widthm1' => 5, # OP_sbfx
        'sh' => 1, # OP_ssat
        'rotate' => 2, # OP_sxtab
        'imod' => 2, 'mode' => 5, # OP_cps
        'M1' => 4, # OP_mrs
        'reg' => 4, # OP_vmrs
        'opcode' => 4, # OP_subs pc
        'sz_crc32' => 2, # OP_crc32
        'sz' => 1, # OP_vabs
        # SIMD
        'size' => 2, 'size=8' => 2, 'size=16' => 2, 'size=32' => 2, 'size=64' => 2,
        'size=s8' => 2, 'size=s16' => 2, 'size=s32' => 2, 'size=s64' => 2,
        'size=u8' => 2, 'size=u16' => 2, 'size=u32' => 2, 'size=u64' => 2,
        'size=i8' => 2, 'size=i16' => 2, 'size=i32' => 2, 'size=i64' => 2,
        'sz=0' => 1, 'sz=1' => 1, 'cmode' => 4,
        'op' => 1, # OP_vacge
        'op_2b' => 2, # OP_vbif, OP_vcvt, OP_vqmov, OP_vrev
        'op_3b' => 3, # OP_vrint
        'sf' => 1, 'sx' => 1, 'RM' => 2, # OP_vcvt
        'align' => 2, 'index_align' => 4,
        'cc' => 2, # OP_vsel
        'len' => 2, # OP_vtbl
        );
    my @encbits = split(' ', $enc);
    my $totlen = 0;
    for (my $i = 0; $i <= $#encbits; $i++) {
        my $token = $encbits[$i];
        $token =~ s/register_list/register_list_t32/ if ($t32);
        $token =~ s/register_list/register_list_priv/
            if ($name eq 'ldm' && $asm =~ /amode/);
        $token =~ s/opc1/opc1_mcr/ if ($name eq 'mcr' || $name eq 'mcr2' ||
                                       $name eq 'mrc' || $name eq 'mrc2');
        $token =~ s/mask/mask_priv/ if ($name eq 'msr' && $enc =~ / R /);
        $token =~ s/\bsz\b/sz_crc32/ if ($name eq 'crc32');
        $token =~ s/\bop\b/op_2b/
            if (($name eq 'v' && $enc =~ /D op V/) ||
                ($name =~ /^vcvt/ && $enc =~ /1 op Q/) ||
                ($name =~ /^vqmov/) ||
                ($name =~ /^vrev/));
        $token =~ s/\bop\b/op_3b/ if ($name =~ /^vrint/ && $enc =~ /1 op Q/);
        $token =~ s/\btype\b/type_vld/ if ($name =~ /^vld/ || $name =~ /^vst/);
        $token =~ s/\b(opc\d)\b/\1_vmov/ if ($name =~ /^vmov/);
        my $len = 0;
        if (length($token) == 1) {
            $len = 1;
        } elsif (defined($bitlen{$token})) {
            $len = $bitlen{$token};
            my $unmod = $encbits[$i];
            if ($unmod eq 'type') {
                $rest =~ s/\btype\b/sh2/;
            } elsif ($unmod !~ /^R/ && $unmod !~ /^CR/ && $unmod !~ /^\(/) {
                my $pos = 32 - $totlen - $len;
                my $repl = $len . "_" . $pos;
                $rest =~ s/\b$unmod\b/imm$repl/;
            }
        } else {
            die "Unknown length for $name: \"$token\" ($enc)\n";
        }
        $totlen += $len;
        if ($token eq '1' || $token eq '(1)') {
            $hexopc |= 1 << (32 - $totlen);
        }
    }
    die "Missing chars (have $totlen) for $name $asm:  $enc\n" unless ($totlen == 32);

    # Handle "x x x P U {D,R} W S" by expanding the chars
    my @bits = split(' ', $opc);
    for (my $i = 0; $i <= $#bits; $i++) {
        if ($bits[$i] eq 'S') {
            $other_opc = $opc;
            $other_opc =~ s/S/0/;
            generate_entry($name, $asm, $enc, $other_opc, $rest, $PUW);
            $name .= "s";
            $bits[$i] = '1';
            $eflags = "fWNZCV";
        } elsif ($bits[$i] eq 'P') {
            $PUW = 1;
            $other_opc = $opc;
            $other_opc =~ s/P/0/;
            generate_entry($name, $asm, $enc, $other_opc, $rest, $PUW);
            $opc =~ s/P/1/;
            $bits[$i] = '1';
        } elsif ($bits[$i] eq 'U') {
            $PUW = 1;
            $other_opc = $opc;
            $other_opc =~ s/U/0/;
            generate_entry($name, $asm, $enc, $other_opc, $rest, $PUW);
            $opc =~ s/U/1/;
            $bits[$i] = '1';
            $negative = 1;
        } elsif ($bits[$i] eq 'W') {
            $PUW = 1;
            $other_opc = $opc;
            $other_opc =~ s/W/0/;
            generate_entry($name, $asm, $enc, $other_opc, $rest, $PUW);
            $bits[$i] = '1';
            $opc =~ s/W/1/;
        } elsif ($bits[$i] eq 'D' || $bits[$i] eq 'R' || $bits[$i] eq 'i') {
            $other_opc = $opc;
            $other_opc =~ s/$bits[$i]/0/;
            generate_entry($name, $asm, $enc, $other_opc, $rest, $PUW);
            $opc =~ s/$bits[$i]/1/;
            $bits[$i] = '1';
        } elsif ($bits[$i] eq 'Q') {
            $other_opc = $opc;
            $other_opc =~ s/Q/0/;
            generate_entry($name, $asm, $enc, $other_opc, $rest, $PUW);
            $bits[$i] = '1';
            $opc =~ s/Q/1/;
            $rest =~ s/ V/ VQ/g;
        }
        if ($bits[$i] eq '1' || $bits[$i] eq '0') {
            $hexopc |= $bits[$i] << (27 - $i);
        } else {
            die "invalid code $bits[$i]\n";
        }
    }

    # Floating-point precision bit: bit 8 == "sz"
    if ($simd && $enc =~ / sz /) {
        $other_name = $name . ".f32";
        $other_enc = $enc;
        $other_enc =~ s/ sz / sz=0 /;
        generate_entry($other_name, $asm, $other_enc, $opc, $rest, $PUW);
        $name .= ".f64";
        $enc =~ s/ sz / sz=1 /;
        $hexopc |= 0x100;
    }
    # For SIMD, Q bit is down low
    if ($simd && $rest =~ / Q /) {
        $other_rest = $rest;
        $other_rest =~ s/Q //;
        $other_rest =~ s/Vn/VAq/;
        $other_rest =~ s/Vd/VBq/;
        $other_rest =~ s/Vm/VCq/;
        generate_entry($name, $asm, $enc, $opc, $other_rest, $PUW);
        $rest =~ s/Q //;
        $rest =~ s/Vn/VAdq/;
        $rest =~ s/Vd/VBdq/;
        $rest =~ s/Vm/VCdq/;
        $hexopc |= 0x40;
    }

    # Data type: "<dt>" or "<size>"
    if ($simd && $enc =~ / size /) {
        # We bail on the precise hex encoding: we just try to pre-generate
        # entries that can be manually tweaked
        my @subtypes;
        if ($asm =~ /.<dt>/) {
            if ($enc =~ / U /) {
                @subtypes = ('s8', 's16', 's32', 'u8', 'u16', 'u32');
            } else {
                @subtypes = ('i8', 'i16', 'i32', 'i64');
            }
        } else {
            @subtypes = ('8', '16', '32', '64');
        }
        $rest =~ s/size\s*//;
        foreach my $sub (@subtypes) {
            $other_name = $name . "." . $sub;
            $other_enc = $enc;
            $other_enc =~ s/ size / size=$sub /;
            generate_entry($other_name, $asm, $other_enc, $opc, $rest, $PUW);
        }
    }

    if ($t32) {
        my @topbits = split(' ', $enc);
        for (my $i = 0; $i < 4; $i++) {
            if ($topbits[$i] eq '1' || $topbits[$i] eq '0') {
                $hexopc |= $topbits[$i] << (31 - $i);
            }
        }
    } elsif (!$pred) {
        $hexopc |= 0xf0000000;
    }
    $opname = $name;
    $opname =~ s/\./_/g;
    $opname .= ",";
    $name .= "\",";
    printf "    {OP_%-8s 0x%08x, \"%-8s ", $opname, $hexopc, $name;

    # Clean up extra spaces, parens, digits
    $enc =~ s/\s\s+/ /g;
    $rest =~ s/\s\s+/ /g;
    $rest =~ s/\(//g;
    $rest =~ s/\)//g;
    $rest =~ s/\s\d+\s/ /g;

    # Put Rd or Rt first, as dst
    $rest =~ s/(.*) (R[dt])/\2 \1/;
    # Put shift last, in disasm order
    $rest =~ s/imm5 type (.*)/\1 type imm5/;
    $rest =~ s/Rs type (.*)/\1 type Rs/;
    # Rn is (usually) before Rm
    $rest =~ s/Rm (.*) Rn/Rn \1 Rm/;

    # Names of types
    $rest =~ s/imm(\d+)/i\1/g;
    $rest =~ s/type/sh2/g;

    $rest =~ s/Rm/-Rm/ if ($negative);

    # Get the 2nd empty dest in there for SIMD with Q.
    # XXX: do the same for the others!
    $rest =~ s/(VA\w+) (VB\w+)/\2 xx \1/;

    my @opnds = split(' ', $rest);
    my $opcnt = 0;
    for (my $i = 0; $i <= $#opnds; $i++) {
        if ($opnds[$i] ne '0' && $opnds[$i] ne '1' &&
            (!$simd || ($opnds[$i] ne 'sz' && $opnds[$i] ne 'N' &&
                        $opnds[$i] ne 'M' && $opnds[$i] ne 'F'))) {
            if ($opcnt == 0 && $opnds[$i] !~ /Rd$/) {
                print "xx, xx, ";
                $opcnt += 2;
            }
            # Convert to the new types
            my $toprint = $opnds[$i];
            $toprint =~ s/Rn/RAw/;
            # XXX: convert these based on bit positions up above -- but keep dst
            # vs src info too
            if ($t32) {
                $toprint =~ s/Rd/RCw/;
                $toprint =~ s/Rt/RCw/;
                die "No Rs in T32!\n" if ($toprint =~ /Rs/);
            } else {
                $toprint =~ s/Rd/RBw/;
                $toprint =~ s/Rt/RBw/;
                $toprint =~ s/Rs/RCw/;
            }
            $toprint =~ s/Rm/RDw/;
            print "$toprint, ";
            $opcnt++;
            if ($opcnt == 1) {
                print "xx, ";
                $opcnt++;
            }
        }
    }
    for (my $i = $opcnt; $i < 5; $i++) {
        print "xx, ";
    }
    print (($pred && !$t32) ? "pred" : "no");
    print ", $eflags, END_LIST},";
    if ($PUW) {
        $PUW_str = $bits[3] . $bits[4] . $bits[6];
        print "/*PUW=$PUW_str*/";
    }
    print "/* ($asm) */ /* <$enc> */\n";
}
