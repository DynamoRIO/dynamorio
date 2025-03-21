#!/usr/bin/perl

# **********************************************************
# Copyright (c) 2014 Google, Inc.  All rights reserved.
# **********************************************************

# Dr. Memory: the memory debugger
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public
# License as published by the Free Software Foundation;
# version 2.1 of the License, and no later version.
#
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
# Library General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with this library; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

# mksystable_macos.pl
#
# Operates on syscalls.master from xnu sources (bsd/kern/syscalls.master)

use Getopt::Long;

%typemap =
    ('int'                      => 'DRSYS_TYPE_SIGNED_INT',
     'long'                     => 'DRSYS_TYPE_SIGNED_INT',
     'user_ssize_t'             => 'DRSYS_TYPE_SIGNED_INT',
     'off_t'                    => 'DRSYS_TYPE_SIGNED_INT',
     'int32_t'                  => 'DRSYS_TYPE_SIGNED_INT',
     'int64_t'                  => 'DRSYS_TYPE_SIGNED_INT',
     'socklen_t'                => 'DRSYS_TYPE_SIGNED_INT',
     'pid_t'                    => 'DRSYS_TYPE_SIGNED_INT',
     'uid_t'                    => 'DRSYS_TYPE_SIGNED_INT',
     'gid_t'                    => 'DRSYS_TYPE_SIGNED_INT',
     'au_asid_t'                => 'DRSYS_TYPE_SIGNED_INT',
     'au_id_t'                  => 'DRSYS_TYPE_SIGNED_INT',
     'key_t'                    => 'DRSYS_TYPE_SIGNED_INT',
     'short'                    => 'DRSYS_TYPE_SIGNED_INT',
     'sem_t'                    => 'DRSYS_TYPE_SIGNED_INT',
     'mach_port_name_t'         => 'DRSYS_TYPE_UNSIGNED_INT',
     'uint32_t'                 => 'DRSYS_TYPE_UNSIGNED_INT',
     'uint64_t'                 => 'DRSYS_TYPE_UNSIGNED_INT',
     'u_int'                    => 'DRSYS_TYPE_UNSIGNED_INT',
     'u_long'                   => 'DRSYS_TYPE_UNSIGNED_INT',
     'u_int32_t'                => 'DRSYS_TYPE_UNSIGNED_INT',
     'unsigned int'             => 'DRSYS_TYPE_UNSIGNED_INT',
     'id_t'                     => 'DRSYS_TYPE_UNSIGNED_INT',
     'size_t'                   => 'DRSYS_TYPE_UNSIGNED_INT',
     'sigset_t'                 => 'DRSYS_TYPE_UNSIGNED_INT',
     'mode_t'                   => 'DRSYS_TYPE_UNSIGNED_INT',
     'user_size_t'              => 'DRSYS_TYPE_SIGNED_INT',
     'user_addr_t'              => 'DRSYS_TYPE_POINTER',
     'void *'                   => 'DRSYS_TYPE_POINTER',
     'unsigned char *'          => 'DRSYS_TYPE_POINTER',
     'void'                     => 'DRSYS_TYPE_VOID',
     'caddr_t'                  => 'DRSYS_TYPE_CSTRING',
     'char *'                   => 'DRSYS_TYPE_CSTRING',
     'char **'                  => 'DRSYS_TYPE_CSTRARRAY',
     'idtype_t'                 => 'DRSYS_TYPE_STRUCT',
     'fhandle_t'                => 'DRSYS_TYPE_STRUCT',
     'siginfo_t'                => 'DRSYS_TYPE_STRUCT',
     # This is a union of int and pointer types: we go w/ simplification
     'semun_t'                  => 'DRSYS_TYPE_POINTER',
    );

# Although the user_*_t types are declared as 64-bit, for 32-bit
# mode they are in fact 32-bit (xref i#1448).
%sizemap =
    ('user_ssize_t'             => 'ssize_t',
     'user_size_t'              => 'size_t',
     'user_addr_t'              => 'void*',
     'user_long_t'              => 'long',
     'user_ulong_t'             => 'ulong_t'
     # Avoid conflicts with varying _STRUCT_SIGALTSTACK
     'struct sigaltstack'       => 'stack_t'
    );

my $verbose = 0;
if (!GetOptions("v=i" => \$verbose)) {
    die "usage error\n";
}

# We expect lines like this:
#   132	AUE_MKFIFO	ALL	{ int mkfifo(user_addr_t path, int mode); }
# Each syscall is a single line, and they're already sorted.
while (<STDIN>) {
    print "Line: $_" if ($verbose >= 3);
    next unless (/^(\d+)\s+AUE_\w+\s+\w+\s+({[^}]+})/);
    print "Processing: $_" if ($verbose >= 1);
    my $num = $1;
    my $sig = $2;
    die "FATAL: malformed \"$sig\""
        unless ($sig =~ /{\s*(\w+)\s+(\w+)\s*\(([^\)]*)\)[^,]*;/);
    my $ret = $1;
    my $name = $2;
    next if ($name eq 'nosys' || $name eq 'enosys');
    my $args = $3;
    my $rtype = "UNKNOWN";
    my $flags = "";
    if ($ret eq 'int' || $ret eq 'user_ssize_t') {
        $rtype = 'RLONG';
    } elsif ($ret eq 'uint32_t' || $ret eq 'mach_port_name_t') {
        $rtype = "DRSYS_TYPE_UNSIGNED_INT";
    } elsif ($ret eq 'void') {
        $rtype = "DRSYS_TYPE_VOID";
    } elsif ($ret eq 'user_addr_t') {
        $rtype = "DRSYS_TYPE_POINTER";
    } elsif ($ret eq 'uint64_t') {
        $rtype = "DRSYS_TYPE_UNSIGNED_INT";
        $flags .= "|SYSINFO_RET_64BIT";
    } elsif ($ret eq 'off_t') {
        $rtype = "RLONG/*64-bit*/";
        $flags .= "|SYSINFO_RET_64BIT";
    } else {
        die "FATAL: unknown return type \"$ret\"\n";
    }
    print "    {{SYS_$name /*$num*/}, \"$name\", OK$flags, ";
    my @arglist = split(',', $args);
    my $argnum = $#arglist + 1;
    if ($argnum == 1 && $arglist[0] eq 'void') {
        $argnum = 0;
    }
    print "$rtype, $argnum,";
    if ($argnum == 0) {
        print " },\n";
        next;
    }
    print "\n     {\n";
    for (my $i = 0; $i < $argnum; $i++) {
        my $a = $arglist[$i];
        $a =~ s/^\s*//;
        $a =~ s/^const\s*//;
        # Remove arg name
        $a =~ s/(\*|\s)\w+\s*$/\1/;
        # Remove trailing whitespace
        $a =~ s/\s*$//;
        print "got arg: \"$a\"\n" if ($verbose >= 2);
        my $argtype = "";
        my $memarg = 0;
        if (defined($typemap{$a})) {
            $argtype = $typemap{$a};
            $memarg = 1 if ($argtype eq 'DRSYS_TYPE_POINTER' ||
                            $argtype eq 'DRSYS_TYPE_CSTRING' ||
                            $argtype eq 'DRSYS_TYPE_CSTRARRAY');
        } elsif ($a =~ /\*$/) {
            $memarg = 1;
            $a =~ s/\s*\*$//;
            if (defined($typemap{$a})) {
                $argtype = $typemap{$a};
            } elsif ($a =~ /^struct\s+/) {
                $argtype = 'DRSYS_TYPE_STRUCT';
            }
        }
        die "FATAL: unknown arg type \"$a\"\n" unless ($argtype ne '');
        print "         {$i, ";
        my $size = defined($sizemap{$a}) ? $sizemap{$a} : $a;
        if ($memarg) {
            # We don't know whether IN or OUT.
            # Safer to mark as OUT and have false neg rather than false pos
            # until we update each entry.
            printf "sizeof($size), W|HT, $argtype},\n";
        } else {
            printf "sizeof($size), SYSARG_INLINED, $argtype},\n";
        }
    }
    printf "     }\n    },\n";
}
