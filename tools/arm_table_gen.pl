#!/usr/bin/perl

# **********************************************************
# Copyright (c) 2014 Google, Inc.  All rights reserved.
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

while ($#ARGV >= 0) {
    if ($ARGV[0] eq '-nopred') {
        $pred = 0;
    } elsif ($ARGV[0] eq '-v') {
        $verbose++;
    } elsif ($ARGV[0] eq '-simd') {
        $simd = 1;
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
    if (/^Encoding A/ || /^Encoding ..\/A/) {
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
            if (!$pred) {
                if ($last =~ /^31 30 29/ && /^1 1 1 1/) {
                    #print "found $name $_\n";
                }
            } elsif (/^cond/) {
                # We encode the "x x x P U {D,R} W S" specifiers either into
                # our opcodes or we have multiple entries with encoding chains.
                my $enc = $_;
                if (/^cond\s+((\(?[01PUWSRD]\)? ){8})(.*)/) {
                    my $opc = $1;
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
    my $negative = 0;

    # Handle "x x x P U {D,R} W S" by expanding the chars
    my @bits = split(' ', $opc);
    my $hexopc = 0;
    for (my $i = 0; $i <= $#bits; $i++) {
        if ($bits[$i] eq 'S') {
            $bits[$i] = 1;
            $other_opc = $opc;
            $other_opc =~ s/S/0/;
            generate_entry($name, $asm, $enc, $other_opc, $rest, $PUW);
            $name .= "s";
            $eflags = "fWNZCV";
        } elsif ($bits[$i] eq 'P') {
            $PUW = 1;
            $other_opc = $opc;
            $other_opc =~ s/P/0/;
            generate_entry($name, $asm, $enc, $other_opc, $rest, $PUW);
            $opc =~ s/P/1/;
            $bits[$i] = 1;
        } elsif ($bits[$i] eq 'U') {
            $PUW = 1;
            $other_opc = $opc;
            $other_opc =~ s/U/0/;
            generate_entry($name, $asm, $enc, $other_opc, $rest, $PUW);
            $opc =~ s/U/1/;
            $bits[$i] = 1;
            $negative = 1;
        } elsif ($bits[$i] eq 'W') {
            $PUW = 1;
            $other_opc = $opc;
            $other_opc =~ s/W/0/;
            generate_entry($name, $asm, $enc, $other_opc, $rest, $PUW);
            $bits[$i] = 1;
            $opc =~ s/W/1/;
        } elsif ($bits[$i] eq 'D') {
            $other_opc = $opc;
            $other_opc =~ s/D/0/;
            generate_entry($name, $asm, $enc, $other_opc, $rest, $PUW);
            $bits[$i] = 1;
            $opc =~ s/D/1/;
        } elsif ($bits[$i] eq 'R') {
            $other_opc = $opc;
            $other_opc =~ s/R/0/;
            generate_entry($name, $asm, $enc, $other_opc, $rest, $PUW);
            $bits[$i] = 1;
            $opc =~ s/R/1/;
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

    my @opnds = split(' ', $rest);
    my $opcnt = 0;
    for (my $i = 0; $i <= $#opnds; $i++) {
        if ($opnds[$i] ne '0' && $opnds[$i] ne '1' &&
            (!$simd || ($opnds[$i] ne 'sz' && $opnds[$i] ne 'N' &&
                        $opnds[$i] ne 'M'))) {
            print "$opnds[$i], ";
            $opcnt++;
        }
    }
    for (my $i = $opcnt; $i < 5; $i++) {
        print "xx, ";
    }
    print ($nopred ? "no" : "pred");
    print ", $eflags, END_LIST},";
    if ($PUW) {
        $PUW_str = $bits[3] . $bits[4] . $bits[6];
        print "/*PUW=$PUW_str*/";
    }
    print "/* ($asm) */ /* <$enc> */\n";
}
