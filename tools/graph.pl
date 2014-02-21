#!/usr/bin/perl

# **********************************************************
# Copyright (c) 2002 VMware, Inc.  All rights reserved.
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

# Derek's Graph Builder
# Need to have: perl-GD and perl-GIFgraph

# For linux, on balrog (RH7.2), I had to do:
# rpm -Uvh gd-2.0.1-4.i386.rpm gd-devel-2.0.1-4.i386.rpm
# rpm -ivh perl-5.6.1-14.i686.rpm
# rpm -Uvh perl-GD-1.33-3.i686.rpm
# rpm -ivh --oldpackage db3-3.1.17-7.i386.rpm
# rpm -ivh perl-modules-5.6.1-14.i686.rpm
# rpm -Uvh perl-GD-TextUtil-0.80-2.i386.rpm
# rpm -Uvh perl-GD-Graph-1.33-3.i686.rpm
# rpm -ivh perl-GIFgraph-1.20-4.i386.rpm

# Installing GIFgraph For win32:
# Got FreeType library from www.freetype.org, built & installed it
# Got gd library from http://www.boutell.com/gd/, built and installed it
# GIFgraph: gifs are no longer supported b/c of unisys patent on lzw!
# -> use GD::Graph
# Got GD-1.33, GDGraph-1.33, and GDTextUtil-0.80 from:
#   ftp://cpan.valueclick.com/pub/CPAN/modules/by-module/
# Installing perl packages: "perl Makefile.PL; make; make install"

use GD::Graph::bars;
use GD;

# Datafile example:
#  type=clustered
#  size=600x400
#  title=Dynamo Speedup
#  x_label=Benchmark
#  y_label=Speedup
#  legend="-O0" "-O3"
#  y_max_value=1.4
#  y_tick_number=14
#  y_number_format=%3.1f
#
#  apsi    0.994019        0.994626
#  art     1.23069         1.08014
#  bzip2   0.928           0.825924
#  crafty  0.210147        0.305814
#  eon     0.310073        0.34035
#  gap     0.268882        0.283897
#  gcc     0.147219        0.14813
#  gzip    0.643855        0.648045
#  mcf     1.1145          1.12565
#  parser  0.425933        0.48503
#  perlbmk 0.193682        0.287931
#  swim    0.997532        0.974938
#  twolf   0.847986        0.805139
#  vortex  0.207531        0.261248
#  vpr     0.877838        0.926712

$usage = "Usage: $0 <datafile>\n";
die $usage if (@ARGV == 0);
$infile = shift;

# default parameters
$multiple_runs = 0;
$sep_slim = 0;
$overwrite = 0; # clustered
$sizex = 600; $sizey = 500;
$title = "Default Title";
$x_label = "X LABEL";
$y_label = "Y LABEL";
$y_max_value = 10;
$y_tick_number = 1;
$y_number_format = "%3.0f";
$legend_placement = 'RL';
$transparent = 1;
# lg_cols default is undefined

$n = 0;
$d = 0;
$data_sets = 0;
open(IN, "< $infile") || die "Error: Couldn't open $infile for input\n";
while (<IN>) {
    next if /^\s*$/; # skip blank lines
    next if /^\#/; # skip comments
    if ($_ =~ /\S*=/) {
        # parameter line
        if ($_ =~ /multiple_runs/) {
            $multiple_runs = 1;
        } elsif ($_ =~ /sep_slim/) {
            $sep_slim = 1;
        } elsif ($_ =~ /type=(\w+)/) {
            $type = $1;
            if ($type eq "clustered") {
                $overwrite = 0;
            } else {
                $overwrite = 2; # stacked
            }
        } elsif ($_ =~ /size=(\d+)x(\d+)/) {
            $sizex = $1;
            $sizey = $2;
        } elsif ($_ =~ /title=(.+)/) {
            $title = $1;
        } elsif ($_ =~ /x_label=(.+)/) {
            $x_label = $1;
        } elsif ($_ =~ /y_label=(.+)/) {
            $y_label = $1;
        } elsif ($_ =~ /y_max_value=([\d\.]+)/) {
            $y_max_value = $1;
        } elsif ($_ =~ /y_tick_number=([\d\.]+)/) {
            $y_tick_number = $1;
        } elsif ($_ =~ /y_number_format=\"?(.*)\"?/) {
            $y_number_format = $1;
        } elsif ($_ =~ /legend_placement=(.+)/) {
            $legend_placement = $1;
        } elsif ($_ =~ /transparent=(.+)/) {
            $transparent = $1;
        } elsif ($_ =~ /lg_cols=([\d]+)/) {
            $lg_cols = $1;
        } elsif ($_ =~ /legend=/) {
            $_ =~ s/legend=//;
            chop;
            @legend = split("\" ", $_);
            # now remove quotes
            foreach $l (@legend) {
                $l =~ s/\"//g;
            }
            if ($sep_slim) {
                # add blank legend for separator dataset
                @legend = ("", @legend);
            }
        } else {
            $_ =~ /(.*)=/;
            die "Unknown parameter: $1\n";
        }
    } else {
        # data line
        # first get label
        if ($multiple_runs) {
            $_ =~ s/(\w+)\s+(\d+)\s+//;
            $bm[$n] = "$1 $2";
            $this_bm = $1;
        } else {
            $_ =~ s/(\w+)\s+//;
            $bm[$n] = $1;
            $this_bm = $1;
        }
        if ($sep_slim) {
            # add empty data set for separator
            $val[0][$n] = "";
            $d++;
        } else {
            # add entire empty entry for separator
            if ($n > 0 && $this_bm ne $last_bm) {
                # separator
                $temp = $bm[$n];
                $bm[$n] = "";
                for ($i=0; $i<$data_sets; $i++) {
                    $val[$i][$n] = "";
                }
                $n++;
                $bm[$n] = $temp;
            }
        }
        while (1) {
            if ($_ =~ /^\s*$/) {
                last;
            }
            $_ =~ s/([\d\.]+)\s*//;
            $data = $1;
            if ($data ne "") {
                $val[$d][$n] = $data;
                $d++;
            } else {
                last;
            }
        }
        if ($n == 0) {
            $data_sets = $d;
        }
        $d = 0;
        $last_bm = $this_bm;
        $n++;
    }
}

@data = ([@bm]);
for ($d=0; $d<$data_sets; $d++) {
    push(@data, @val[$d]);
}
$my_graph = new GD::Graph::bars($sizex, $sizey);
if (defined(@legend)) {
    $my_graph->set_legend(@legend);
}
$my_graph->set_title_font(gdLargeFont);
$my_graph->set_x_axis_font(gdMediumBoldFont);
$my_graph->set_y_axis_font(gdMediumBoldFont);
$my_graph->set_x_label_font(gdMediumBoldFont);
$my_graph->set_y_label_font(gdMediumBoldFont);
$my_graph->set_legend_font(gdMediumBoldFont);
if ($sep_slim) {
    $my_graph->set(dclrs =>
           [ qw(pink lred lgreen lblue lyellow lpurple cyan lorange) ]);
}
if (defined($lg_cols)) {
    $my_graph->set(lg_cols           => $lg_cols);
}
$my_graph->set(
               overwrite         => $overwrite,
               title             => $title,
               x_label           => $x_label,
               y_label           => $y_label,
               long_ticks        => 1,
               axis_space        => 5,
               y_max_value       => $y_max_value,
               y_tick_number     => $y_tick_number,
               y_label_skip      => 1,
               y_number_format   => $y_number_format,
               x_ticks           => 0,
               x_labels_vertical => 1,
               x_label_position  => 0.5,
               legend_placement  => $legend_placement,
               transparent       => $transparent,
               );

## can't do gifs anymore!
## $my_graph->plot_to_gif( "$infile.gif", \@data );

open(IMG, "> $infile.png") or die $!;
binmode IMG;
print IMG $my_graph->plot(\@data)->png;
close(IMG);
