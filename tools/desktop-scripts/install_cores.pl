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

# install_cores.pl
#
# Installs all cores since blowfish (Adapted from QA's scripts)
#

use strict;
use Cwd 'chdir';

system("net use n: \\\\10.1.5.15\\nightly \"\" /USER:guest") unless (-e "n:");
my $NIGHTLY_DIR = qq(n:);
my $dir;


system("net stop tomcatsc");
foreach $dir (`cmd /c dir /on /b $NIGHTLY_DIR\\302??`) {
    chomp($dir);
    next if ($dir < "30259");  # optionally skip ones already installed
    &InstallNewCore($dir);
    #print "installing $dir\n";
}
system("net start tomcatsc");

#--------------------------------------------------
sub InstallNewCore() {
    my $build_dir = shift;

    # lib path
    my $lib_r_path = qq($NIGHTLY_DIR\\$build_dir\\ver_2_5\\lib\\release);

    # install new core to controller
    my $controller_path = qq("C:\\Program Files\\Determina\\Determina Management Console");

    chdir("C:\\Program Files\\Determina\\Determina Management Console\\conf\\tool") or
       die "Couldn't change to conf dir: $!\n";

    my $ver_r  = $build_dir . "r";
    my $desc_r = $build_dir . "_release_build";

    print "Installing $ver_r\n";
    system("admintool -installCore braksha $ver_r $ver_r $desc_r $lib_r_path\\dynamorio.dll");

    # make this new release core the default for all policies
    #system("\"C:\\Program Files\\Determina\\Determina Management Console\\Support Tools\\MySQL\\bin\\mysql\" -D emv-1 -u root --password=braksha -e \"UPDATE systemconfig SET DefaultCoreVersion='$ver_r'\"");

    # nightly core upgrade" use the new release build
    #system("\"C:\\Program Files\\Determina\\Determina Management Console\\Support Tools\\MySQL\\bin\\mysql\" -D emv-1 -u root --password=braksha -e \"UPDATE policies SET CoreVersion='$ver_r' WHERE Name='qatest configuration'\"");
}
