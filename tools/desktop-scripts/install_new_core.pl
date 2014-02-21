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

#
# adapted from QA's scripts
#
#FIXME remove dependency on last_build_num.txt

system("net use n: \\\\10.1.5.15\\nightly \"\" /USER:guest") unless (-e "n:");

my $NIGHTLY_DIR = qq(n:);

my $latest_build;
my $last_build;


&GetLatestBuildNum();
&InstallNewCore();
&UpdateLastBuildNum();



sub GetLatestBuildNum() {

# retrieve the latest build number on nightly
    my @builds = `cmd /c dir /on /b $NIGHTLY_DIR\\302??`;
    $latest_build = @builds[-1];
    chomp($latest_build);

# retrieve last build number on controller

    open(IN,"C:\\work\\last_build_num.txt") or die "Cannot find the file last_build_num.txt\n";
    $last_build = <IN>;
    chomp($last_build);
    close IN;

# do we have a new build?

    if ($latest_build <= $last_build) {  # no new build; do nothing
       print "Build $latest_build is older or equal to the latest core available in the controller - Installcore will abort now\n";
       exit;
    }

}


sub InstallNewCore() {

# copy the latest lib, skip debug and profile for now
    #$lib_d_path = "\\\\10.1.5.15\\nightly\\$latest_build\\ver_2_5\\lib\\debug";
    #$lib_p_path = "\\\\10.1.5.15\\nightly\\$latest_build\\ver_2_5\\lib\\profile";

    $lib_r_path = "\\\\10.1.5.15\\nightly\\$latest_build\\ver_2_5\\lib\\release";

# install new core to controller
     my $controller_path = "\"C:\\Program Files\\Determina\\Determina Management Console\"";

    system("net stop tomcatsc");
    chdir("C:\\Program Files\\Determina\\Determina Management Console\\conf\\tool");

# skip debug and profile for now
    #my $ver_d = $latest_build . "d";
    #my $desc_d = $latest_build . "_debug_build";
    #system("admintool -installCore braksha $ver_d $ver_d $desc_d $lib_d_path\\dynamorio.dll");

    #my $ver_p = $latest_build . "p";
    #my $desc_p = $latest_build . "_profile_build";
    #system("admintool -installCore braksha $ver_p $ver_p $desc_p $lib_p_path\\dynamorio.dll");

    my $ver_r = $latest_build . "r";
    my $desc_r = $latest_build . "_release_build";
    system("admintool -installCore braksha $ver_r $ver_r $desc_r $lib_r_path\\dynamorio.dll");

    # make this new release core the default for all policies
    # system("\"C:\\Program Files\\Determina\\Determina Management Console\\Support Tools\\MySQL\\bin\\mysql\" -D emv-1 -u root --password=braksha -e \"UPDATE systemconfig SET DefaultCoreVersion='$ver_r'\"");

    # nightly core upgrade" use the new release build
    system("\"C:\\Program Files\\Determina\\Determina Management Console\\Support Tools\\MySQL\\bin\\mysql\" -D emv-1 -u root --password=braksha -e \"UPDATE policies SET CoreVersion='$ver_r' WHERE Name='qatest configuration'\"");
    system("net start tomcatsc");
}

sub UpdateLastBuildNum() {
    open OUT,">C:\\work\\last_build_num.txt";
    print OUT "$latest_build";
    close OUT;
}
