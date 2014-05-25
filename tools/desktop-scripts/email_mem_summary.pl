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

# email_mem_summary.pl
#
# NOTE:  ASSUMES Net::smtp module exists
# FIXME: Header string specifes hard coded path


use strict;
use Getopt::Long;
use Net::SMTP;

my $PROG = "email_mem_summary.pl";

my %opt;
&GetOptions(\%opt,
    "summary=s",
    "help",
    "usage",
    "debug",
);

my $summary_file = qq($opt{'summary'});

my $ServerName = "DHOST002-63.DEX002.intermedia.net" ;

open (FD,$summary_file) or die "Cannot open";
my @summary = <FD>;
close FD;

my $msg;
my $header =
    "\n\n".
    "Desktop memory usage summary: \n".
    "    for lionfish default configuration (only core sevices under DR)\n".
    "    on qatest (xp)\n".
    "\n".
    "Run data and summary file at:\n".
    "\\\\10.1.5.85\\g_shares\\desktop-nightly\\results\\boot_data \n".
    "\n\n".
    $summary[0];  # header

my $line;
# reverse sorted order (latest first) of summary
foreach $line (@summary) {
    $msg = $line . $msg;
}

# Now prepend the header information
$msg = $header . $msg;

#print $msg;

# Connect to the server
my $smtp = Net::SMTP->new($ServerName);
die "Couldn't connect to server" unless $smtp;

my $MailFrom = "eng-nightly-results\@determina.com";
my $MailTo = "eng-nightly-results\@determina.com";
#my $MailTo = "srini\@determina.com";

print "$PROG: sending email summary\n";
$smtp->mail( $MailFrom );
$smtp->to( $MailTo );
$smtp->data();

# Send the message
$smtp->datasend("Subject: Memory summary: lionfish desktop configuration\n");
$smtp->datasend("$msg\n\n");
# Send the termination string
$smtp->dataend();
$smtp->quit() ;
