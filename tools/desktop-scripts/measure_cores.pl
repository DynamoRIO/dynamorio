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

# measure_cores.pl
#
# Catchup on memory measurements for all the core since blowfish
# optionall skip ones already done
#
# NOTE: setup the new core for qatest configuration, need to change this to
#       use for a different config

# FIXME: clean up hardcoded dir srini
#

use strict;

my $PROG = "measure_30xxx_core.pl";

my $dir;
my $core_dir = qq("C:\\Program Files\\Determina\\Determina Management Console\\data\\core");

# for each core available,
#   install it and reboot qatest
#   qatest will run desktop-nightly.bat which will send out an email
foreach $dir (`cmd /c dir /on /b $core_dir\\30???r`) {
    chomp($dir);
    # optionally skip the ones that were done
    #next if ($dir < "30259r");
    system("net stop tomcatsc");
    print "$PROG: Installing using core: $dir\n";
    &InstallNewCore($dir);
    system("net start tomcatsc");
    print "$PROG: Sleeping 180s\n";
    sleep 180;
    print "$PROG: Rebooting qatest to run desktop-nightly\n";

    # run this 3 times
    for (my $i = 0; $i < 3; $i++) {
        # give it about 30 min to complete and start the next cycle
        system("c:\\perl\\bin\\perl c:\\srini\\boot_data_scripts\\reboot_qatest.pl 1400");
    }
}

#--------------------------------------------------
sub InstallNewCore() {
    my $ver_r  = shift;

    # nightly core upgrade" use the new release build
    system("\"C:\\Program Files\\Determina\\Determina Management Console\\Support Tools\\MySQL\\bin\\mysql\" -D emv-1 -u root --password=braksha -e \"UPDATE policies SET CoreVersion='$ver_r' WHERE Name='qatest configuration'\"");
}
