#!/usr/bin/perl

# **********************************************************
# Copyright (c) 2011-2013 Google, Inc.  All rights reserved.
# Copyright (c) 2008-2009 VMware, Inc.  All rights reserved.
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

# mksystable.pl
#
# expecting headers like this, currently from either Nebbett or Metasploit:
#
# NTSYSAPI
# NTSTATUS
# NTAPI
# ZwQuerySystemInformation(
#     IN SYSTEM_INFORMATION_CLASS SystemInformationClass,
#     OUT PVOID SystemInformation,
#     IN ULONG SystemInformationLength,
#     OUT PULONG ReturnLength OPTIONAL
#     );
#
# with i#96 also supports ntgdi annotated headers.

use Getopt::Long;

$all_params = 1; # we now want all, not just memory

# list of non-pointer types that start with P
%ptypes = ('PROCESSINFOCLASS' => 1,
           'POWER_ACTION' => 1,
           'POWER_INFORMATION_LEVEL' => 1,
           'PKNORMAL_ROUTINE' => 1,
           'PIO_APC_ROUTINE' => 1,
           'PTIMER_APC_ROUTINE' => 1,
           'PTHREAD_START_ROUTINE' => 1,
           'PALETTEENTRY' => 1,
           'PATHOBJ' => 1,
           'POINT' => 1,
           'POINTL' => 1,
           'POINTFIX' => 1,
           'POINTQF' => 1,
           'POLYTEXTW' => 1,
           'POLYPATBLT' => 1,
           'PERBANDINFO' => 1,
    );

%typemap_inline =
    ('ULONG'                    => 'DRSYS_TYPE_UNSIGNED_INT',
     'LONG'                     => 'DRSYS_TYPE_SIGNED_INT',
     'USHORT'                   => 'DRSYS_TYPE_UNSIGNED_INT',
     'UCHAR'                    => 'DRSYS_TYPE_UNSIGNED_INT',
     'DWORD'                    => 'DRSYS_TYPE_UNSIGNED_INT',
     'LCID'                     => 'DRSYS_TYPE_UNSIGNED_INT',
     'LANGID'                   => 'DRSYS_TYPE_UNSIGNED_INT',
     'ACCESS_MASK'              => 'DRSYS_TYPE_UNSIGNED_INT',
     'SECURITY_INFORMATION'     => 'DRSYS_TYPE_UNSIGNED_INT',
     'EXECUTION_STATE'          => 'DRSYS_TYPE_UNSIGNED_INT',
     'DEBUG_CONTROL_CODE'       => 'DRSYS_TYPE_UNSIGNED_INT',
     'APPHELPCACHECONTROL'      => 'DRSYS_TYPE_UNSIGNED_INT',
     'UINT'                     => 'DRSYS_TYPE_UNSIGNED_INT',
     'FLONG'                    => 'DRSYS_TYPE_UNSIGNED_INT',
     'ULONG_PTR'                => 'DRSYS_TYPE_UNSIGNED_INT',
     'COLORREF'                 => 'DRSYS_TYPE_UNSIGNED_INT',
     'SIZE_T'                   => 'DRSYS_TYPE_UNSIGNED_INT',
     'WCHAR'                    => 'DRSYS_TYPE_UNSIGNED_INT',
     'LFTYPE'                   => 'DRSYS_TYPE_UNSIGNED_INT',
     'ROP4'                     => 'DRSYS_TYPE_UNSIGNED_INT',
     'MIX'                      => 'DRSYS_TYPE_UNSIGNED_INT',
     'BYTE'                     => 'DRSYS_TYPE_UNSIGNED_INT',
     'WPARAM'                   => 'DRSYS_TYPE_UNSIGNED_INT',
     'DWORD_PTR'                => 'DRSYS_TYPE_UNSIGNED_INT',
     'UINT_PTR'                 => 'DRSYS_TYPE_UNSIGNED_INT',
     'unsigned'                 => 'DRSYS_TYPE_UNSIGNED_INT',
     'POWER_INFORMATION_LEVEL'  => 'DRSYS_TYPE_SIGNED_INT',
     'POWER_ACTION'             => 'DRSYS_TYPE_SIGNED_INT',
     'SYSTEM_POWER_STATE'       => 'DRSYS_TYPE_SIGNED_INT',
     'DEVICE_POWER_STATE'       => 'DRSYS_TYPE_SIGNED_INT',
     'LATENCY_TIME'             => 'DRSYS_TYPE_SIGNED_INT',
     'AUDIT_EVENT_TYPE'         => 'DRSYS_TYPE_SIGNED_INT',
     'SHUTDOWN_ACTION'          => 'DRSYS_TYPE_SIGNED_INT',
     'THREADINFOCLASS'          => 'DRSYS_TYPE_SIGNED_INT',
     'PROCESSINFOCLASS'         => 'DRSYS_TYPE_SIGNED_INT',
     'JOBOBJECTINFOCLASS'       => 'DRSYS_TYPE_SIGNED_INT',
     'DEBUGOBJECTINFOCLASS'     => 'DRSYS_TYPE_SIGNED_INT',
     'VDMSERVICECLASS'          => 'DRSYS_TYPE_SIGNED_INT',
     'TOKEN_TYPE'               => 'DRSYS_TYPE_SIGNED_INT',
     'WAIT_TYPE'                => 'DRSYS_TYPE_SIGNED_INT',
     'KPROFILE_SOURCE'          => 'DRSYS_TYPE_SIGNED_INT',
     'EVENT_TYPE'               => 'DRSYS_TYPE_SIGNED_INT',
     'TIMER_TYPE'               => 'DRSYS_TYPE_SIGNED_INT',
     'SECTION_INHERIT'          => 'DRSYS_TYPE_SIGNED_INT',
     'HARDERROR_RESPONSE_OPTION'=> 'DRSYS_TYPE_SIGNED_INT',
     'ARCTYPE'                  => 'DRSYS_TYPE_SIGNED_INT',
     'INT'                      => 'DRSYS_TYPE_SIGNED_INT',
     'WORD'                     => 'DRSYS_TYPE_SIGNED_INT',
     'DXGI_FORMAT'              => 'DRSYS_TYPE_SIGNED_INT',
     'HLSURF_INFORMATION_CLASS' => 'DRSYS_TYPE_SIGNED_INT',
     'LPARAM'                   => 'DRSYS_TYPE_SIGNED_INT',
     'GETCPD'                   => 'DRSYS_TYPE_SIGNED_INT',
     'USERTHREADINFOCLASS'      => 'DRSYS_TYPE_SIGNED_INT',
     'WORKERFACTORYINFOCLASS'   => 'DRSYS_TYPE_SIGNED_INT',
     'int'                      => 'DRSYS_TYPE_SIGNED_INT',
     'BOOL'                     => 'DRSYS_TYPE_BOOL',
     'BOOLEAN'                  => 'DRSYS_TYPE_BOOL',
     'HANDLE'                   => 'DRSYS_TYPE_HANDLE',
     'HDC'                      => 'DRSYS_TYPE_HANDLE',
     'HBITMAP'                  => 'DRSYS_TYPE_HANDLE',
     'HPALETTE'                 => 'DRSYS_TYPE_HANDLE',
     'HWND'                     => 'DRSYS_TYPE_HANDLE',
     'HCOLORSPACE'              => 'DRSYS_TYPE_HANDLE',
     'HBRUSH'                   => 'DRSYS_TYPE_HANDLE',
     'HRGN'                     => 'DRSYS_TYPE_HANDLE',
     'HPEN'                     => 'DRSYS_TYPE_HANDLE',
     'HFONT'                    => 'DRSYS_TYPE_HANDLE',
     'HSURF',                   => 'DRSYS_TYPE_HANDLE',
     'HDEV',                    => 'DRSYS_TYPE_HANDLE',
     'DHSURF'                   => 'DRSYS_TYPE_HANDLE',
     'HUMPD'                    => 'DRSYS_TYPE_HANDLE',
     'HLSURF'                   => 'DRSYS_TYPE_HANDLE',
     'HMENU'                    => 'DRSYS_TYPE_HANDLE',
     'HKL'                      => 'DRSYS_TYPE_HANDLE',
     'HDESK'                    => 'DRSYS_TYPE_HANDLE',
     'HWINSTA'                  => 'DRSYS_TYPE_HANDLE',
     'HACCEL'                   => 'DRSYS_TYPE_HANDLE',
     'HINSTANCE'                => 'DRSYS_TYPE_HANDLE',
     'HDWP'                     => 'DRSYS_TYPE_HANDLE',
     'HCURSOR'                  => 'DRSYS_TYPE_HANDLE',
     'HICON'                    => 'DRSYS_TYPE_HANDLE',
     'HRAWINPUT'                => 'DRSYS_TYPE_HANDLE',
     'HMODULE'                  => 'DRSYS_TYPE_HANDLE',
     'HRSRC'                    => 'DRSYS_TYPE_HANDLE',
     'HHOOK'                    => 'DRSYS_TYPE_HANDLE',
     'HWINEVENTHOOK'            => 'DRSYS_TYPE_HANDLE',
     'HMONITOR'                 => 'DRSYS_TYPE_HANDLE',
     'ATOM'                     => 'DRSYS_TYPE_ATOM',
     'NTSTATUS'                 => 'DRSYS_TYPE_NTSTATUS',
     'PVOID'                    => 'DRSYS_TYPE_POINTER',
     'PKNORMAL_ROUTINE'         => 'DRSYS_TYPE_FUNCTION',
     'PIO_APC_ROUTINE'          => 'DRSYS_TYPE_FUNCTION',
     'PTIMER_APC_ROUTINE'       => 'DRSYS_TYPE_FUNCTION',
     'PTHREAD_START_ROUTINE'    => 'DRSYS_TYPE_FUNCTION',
     'TIMERPROC'                => 'DRSYS_TYPE_FUNCTION',
     'HOOKPROC'                 => 'DRSYS_TYPE_FUNCTION',
     'WINEVENTPROC'             => 'DRSYS_TYPE_FUNCTION',
     # no targeted type
     'LDT_ENTRY'                => 'DRSYS_TYPE_STRUCT',
     'BLENDFUNCTION'            => 'DRSYS_TYPE_STRUCT',
     'PALETTEENTRY'             => 'DRSYS_TYPE_STRUCT',
     'SIZEL'                    => 'DRSYS_TYPE_STRUCT',
     'POINT'                    => 'DRSYS_TYPE_STRUCT',
     'PIO_APC_ROUTINE'          => 'DRSYS_TYPE_POINTER',
     'PTIMER_APC_ROUTINE'       => 'DRSYS_TYPE_POINTER',
     'PKNORMAL_ROUTINE'         => 'DRSYS_TYPE_POINTER',
    );
%typemap_complex =
    ('PCSTR'                    => 'DRSYS_TYPE_CSTRING',
     'PCWSTR'                   => 'DRSYS_TYPE_CWSTRING',
     'wchar_t'                  => 'DRSYS_TYPE_CWSTRING',
     'UNICODE_STRING'           => 'DRSYS_TYPE_UNICODE_STRING',
     'LARGE_STRING'             => 'DRSYS_TYPE_LARGE_STRING',
     'OBJECT_ATTRIBUTES'        => 'DRSYS_TYPE_OBJECT_ATTRIBUTES',
     'SECURITY_DESCRIPTOR'      => 'DRSYS_TYPE_SECURITY_DESCRIPTOR',
     'SECURITY_QUALITY_OF_SERVICE' => 'DRSYS_TYPE_SECURITY_QOS',
     'PORT_MESSAGE'             => 'DRSYS_TYPE_PORT_MESSAGE',
     'CONTEXT'                  => 'DRSYS_TYPE_CONTEXT',
     'EXCEPTION_RECORD'         => 'DRSYS_TYPE_EXCEPTION_RECORD',
     'DEVMODEW'                 => 'DRSYS_TYPE_DEVMODEW',
     'WNDCLASSEXW'              => 'DRSYS_TYPE_WNDCLASSEXW',
     'CLSMENUNAME'              => 'DRSYS_TYPE_CLSMENUNAME',
     'MENUITEMINFOW'            => 'DRSYS_TYPE_MENUITEMINFOW',
     'ALPC_PORT_ATTRIBUTES'     => 'DRSYS_TYPE_ALPC_PORT_ATTRIBUTES',
     'ALPC_SECURITY_ATTRIBUTES' => 'DRSYS_TYPE_ALPC_SECURITY_ATTRIBUTES',
     'LOGFONTW'                 => 'DRSYS_TYPE_LOGFONTW',
     'NONCLIENTMETRICSW'        => 'DRSYS_TYPE_NONCLIENTMETRICSW',
     'ICONMETRICSW'             => 'DRSYS_TYPE_ICONMETRICSW',
     'SERIALKEYSW'              => 'DRSYS_TYPE_SERIALKEYSW',
     'SOCKADDR'                 => 'DRSYS_TYPE_SOCKADDR',
     'MSGHDR'                   => 'DRSYS_TYPE_MSGHDR',
     'MSGBUF'                   => 'DRSYS_TYPE_MSGBUF',
     'FILE_IO_COMPLETION_INFORMATION' => 'DRSYS_TYPE_FILE_IO_COMPLETION_INFO'
    );
%typemap_struct =
    (
     'LARGE_INTEGER'            => 'DRSYS_TYPE_LARGE_INTEGER',
     'ULARGE_INTEGER'           => 'DRSYS_TYPE_ULARGE_INTEGER',
     'IO_STATUS_BLOCK'          => 'DRSYS_TYPE_IO_STATUS_BLOCK',
    );

$verbose = 0;
if (!GetOptions("v" => \$verbose)) {
    die "usage error\n";
}

while (<STDIN>) {
    # Nebbett has ^NTSYSAPI, Metasploit has .*NTSYSAPI, ntuser.h has NTAPI
    next unless (/(NTSYSAPI\s*$)|(^__kernel_entry W32KAPI)|(NTAPI\s*$)|(APIENTRY)/);
    $is_w32k = /W32KAPI/;
    while (<STDIN>) {
        next if (/^NTSTATUS/ || /^NTAPI/ || /^ULONG/ || /^BOOLEAN/);
        last;
    }
    next if (/^APIENTRY\s*$/); # sometimes on next line
    next if (/^Rtl/);
    die "Function parsing error $_" unless (/^((Zw)|(Nt))(\w+)\s*\(/);
    $name = "Nt" . $4;
    my $noargs = 0;
    if (/\(VOID\);/) {
        $noargs = 1;
    }
    # not a real system call: just reads KUSER_SHARED_DATA
    next if ($name eq "NtGetTickCount");
    $entry{$name} = "    {{0,0},\"$name\", OK, RNTST, ";
    my $argnum = 0;
    my $toprint = "";
    my $nameline = $_;
    while (<STDIN>) {
        last if ($noargs || /^\s*\);/ || $nameline =~ /\(\s*\);/);
        s|//.*$||; # remove comments
        s|/\*[^\*]+\*/||; # remove comments
        s|\s*const(\s*)|\1|i; # remove const
        s|\s*OPTIONAL(\s*)|\1|i; # remove OPTIONAL
        if (/^\s*(VOID)\s*(,|\);|)\s*$/) {
            $inout = "";
            $arg_type[$argnum] = $1;
        } else {
            if ($is_w32k) {
                # __-style param annotations in ntgdi.h
                # hack for missing var name (rather than relaxing pattern)
                s/SURFOBJ \*\s*$/SURFOBJ *varname/;
                die "Param parsing error $_" unless (/^\s*(__[_a-z]+(\(.+\))?(\s*__typefix\(.*\))?)\s*((struct\s+)?[_\w]+)\s*(\**\s*\w+)\s*(,|\);|)\s*$/);
                print "\tann: $1, type: $3, var: $5\n" if ($verbose > 1);
                $ann = $1;
                $arg_type[$argnum] = $4;
                $arg_var[$argnum] = $6;
                $arg_bufsz[$argnum] = '';
                $arg_comment[$argnum] = '';
                # annotation components are split by underscores.
                # we also have parens that go with range(x,y) or
                # with [eb]count -- but for the latter there can be
                # other modifiers prior to the parens.
                # examples:
                #   __out_ecount_part_opt(cwc, *pcCh)
                #   __deref_out_range(0, MAX_PATH * 3) ULONG* pcwc,
                $ann =~ s/^__//;
                # deal w/ parens first, then we can use split on rest
                # we ignore range()
                $ann =~ s/range\([^\)]+\)//;
                # typefix seems to be separate by a space
                if ($ann =~ s/\s*__typefix\(([^\)]+)\)//) {
                    $arg_type[$argnum] = $1; # replace type
                }
                # we assume only one other set of parens: [eb]count
                if ($ann =~ s/\((.+)\)//) {
                    $arg_bufsz[$argnum] = $1;
                }
                foreach $a (split('_', $ann)) {
                    if ($a eq 'in' || $a eq 'out' || $a eq 'inout') {
                        $arg_inout[$argnum] = uc($a);
                    } elsif ($a eq 'opt' || # we don't care
                             $a eq 'part') { # we assume we'll see (size,length)
                        # ignore annotation
                    } elsif ($a =~ /^bcount/) {
                        $arg_ecount[$argnum] = 0;
                    } elsif ($a =~ /^ecount/) {
                        $arg_ecount[$argnum] = 1;
                    } elsif ($a =~ /^xcount/) {
                        # xcount requires additional handling done manually
                        # so far only seen in NtGdiExtTextOutW
                        $arg_ecount[$argnum] = 1;
                        $arg_comment[$argnum] .= '/*FIXME size can be larger*/';
                    } elsif ($a =~ /^post/) {
                        # the buffer size is unknown at pre time: supposed to
                        # call twice, first w/ NULL buffer to get required size.
                        $arg_comment[$argnum] .= '/*FIXME pre size from prior syscall ret*/';
                    } elsif ($a =~ /^deref/) {
                        # XXX: this one I don't quite get: it's used on things
                        # that look like regular OUT vars to me.
                        # is it just to say it can't be NULL (vs _deref_opt)?
                        # but what about all the OUT vars w/ no _dref?
                    } else {
                        die "Unknown annotation: $a\n";
                    }
                }
                # handle "bcount(var * sizeof(T)) __typefix(T) PVOID"
                if ($arg_bufsz[$argnum] =~ /^(\w+)\s*\*\s*sizeof\(([^\)]+)\)/) {
                    my $newsz = $1;
                    my $type = $2;
                    if ($arg_type[$argnum] =~ /^${type}\s*\*$/ ||
                        $arg_type[$argnum] =~ /^P${type}$/) {
                        $arg_bufsz[$argnum] = $newsz;
                        $arg_ecount[$argnum] = 1;
                    }
                }
            } else {
                # all-caps, separate-words param annotations
                die "Param parsing error $_" unless
                    (/^\s*((IN\s+OUT)|(IN)|(OUT))?\s*((struct\s+)?[_\w]+)\s*(\*?\s*\w+)\s*(OPTIONAL)?\s*(,|\);|)\s*$/);
                print "\t$1-$2-$3-$4-$5-$6-$7\n" if ($verbose);
                # if no IN/OUT annotation assume IN
                $arg_inout[$argnum] = $1;
                $arg_type[$argnum] = $5;
                $arg_var[$argnum] = $7;
                $optional = $8;
            }
            while ($arg_var[$argnum] =~ s/\*//) {
                $arg_type[$argnum] .= '*';
                $arg_var[$argnum] =~ s/^\s*//;
            }
            s/\r?\n$//;
            print "\t$argnum: $_ => $arg_inout[$argnum]:$arg_type[$argnum]:$arg_var[$argnum]\n"
                if ($verbose);

            if (!$is_w32k) {
                # convert Nebbett types to Metasploit's updated types
                $arg_type[$argnum] =~ s/PORT_SECTION_READ/REMOTE_PORT_VIEW/;
                $arg_type[$argnum] =~ s/PORT_SECTION_WRITE/PORT_VIEW/;

                # convert enum to ULONG
                $arg_type[$argnum] =~ s/^HARDERROR_RESPONSE$/ULONG/;
                $arg_type[$argnum] =~ s/SAFEBOOT_MODE/ULONG/;
                $arg_type[$argnum] =~ s/OPEN_SUB_KEY_INFORMATION/ULONG/;

                # "CMENUINFO" is "CONST MENUINFO"
                $arg_type[$argnum] =~ s/PCMENUINFO/PMENUINFO/;
                $arg_type[$argnum] =~ s/PCRAWINPUTDEVICE/PRAWINPUTDEVICE/;
                $arg_type[$argnum] =~ s/PCSCROLLINFO/PSCROLLINFO/;
                $arg_type[$argnum] =~ s/PCRECT/PRECT/;

                # eliminate special ROS types
                $arg_type[$argnum] =~ s/^PROSMENU/PMENU/;
            }

            # convert VOID* to PVOID
            $arg_type[$argnum] =~ s/^VOID\*/PVOID/;

            $arg_type[$argnum] =~ s/^LP/P/ unless ($arg_type[$argnum] eq 'LPARAM');

            if ($arg_inout[$argnum] eq '') {
                $arg_inout[$argnum] = ($arg_type[$argnum] eq 'PVOID') ? 'OUT' : 'IN';
            }
        }
        $arg_name_to_num{$arg_var[$argnum]} = $argnum;
        $argnum++;
        last if (/\);/);
    }

    # now print out the entry
    for ($i = 0; $i < $argnum; $i++) {
        my $inout = $arg_inout[$i];
        my $type = $arg_type[$i];
        my $param_in_memory =
            ($type =~ /^P/ || $type =~ /\*/) &&
            # IN PVOID is usually inlined
            ($type ne 'PVOID' || $inout =~ /OUT/ ||
             $arg_var[$i] =~ /Buffer$/ ||
             $arg_var[$i] =~ /Information$/) &&
            # list of inlined types that start with P
            $type !~ /_INFORMATION_CLASS/ &&
            !defined($ptypes{$type});
        if ($name eq 'NtVdmControl' && $arg_var[$i] eq 'ServiceData') {
            # FIXME: ServiceData arg to NtVdmControl in Metasploit is IN OUT
            # but we don't know size so we ignore its OUT and hope we
            # never see it
        } elsif ($all_params || $param_in_memory) {
            my $rtype = $type;
            if ($type eq 'PWSTR' || $type eq 'PCWSTR') {
                $rtype = 'wchar_t';
            } elsif ($type eq 'PSTR') {
                $rtype = 'char';
            } else {
                if ($rtype =~ /\*/) {
                    $rtype =~ s/\s*\*$//;
                } elsif ($rtype =~ /^P/ && $param_in_memory) {
                    $rtype =~ s/^P//;
                }
            }
            if ($name =~ /Atom$/) {
                $rtype =~ s/USHORT/ATOM/;
            }
            print "type=$type, rtype=$rtype\n" if ($verbose);
            my $inout_string = $inout =~ /OUT/ ? ($inout =~ /IN/ ? "R|W" : "W") : "R";
            my $cap;
            my $wrote = '';
            $toprint .= "         {";
            if ($arg_bufsz[$i] ne '') {
                if ($arg_bufsz[$i] =~ /([^,]+),\s+(.+)/) {
                    $cap = $1;
                    $wrote = $2;
                    die "Cannot have 2 sizes for IN param\n" unless ($inout =~ /OUT/);
                } else {
                    $cap = $arg_bufsz[$i];
                    $wrote = ''; # same as cap
                }
                if ($cap eq 'return') {
                    $cap = 0;
                    $arg_comment[$i] .= "/*FIXME size from retval so earlier call*/";
                }
                if ($cap =~ /^\*/) {
                    $cap =~ s/^\*//;
                    if ($inout_string eq 'W') {
                        $inout_string = 'WI';
                    } else {
                        $inout_string = 'R|SYSARG_LENGTH_INOUT';
                    }
                }
                if (defined($arg_name_to_num{$cap})) {
                    $toprint .= sprintf("%d, -%d,", $i, $arg_name_to_num{$cap});
                } else {
                    $toprint .= sprintf("%d, %s,", $i, $cap);
                }
            } elsif ($param_in_memory && !$is_w32k &&
                     ($type eq 'PVOID' || $type eq 'PWSTR')) {
                # when don't have length annotations: assume next arg holds length
                # XXX: pretty risky assumption: verify manually
                $toprint .= sprintf("%d, -%d, ", $i, $i+1);
            } else {
                my $size_type = $rtype;
                if ($type eq 'PVOID') {
                    $size_type = 'PVOID';
                }
                $toprint .= sprintf("%d, sizeof(%s), ", $i, $size_type);
            }
            my $stype;
            if ($type ne 'VOID') {
                if ($rtype =~ /_INFORMATION_CLASS$/) {
                    $stype = 'DRSYS_TYPE_SIGNED_INT'; # enum
                } elsif (defined($typemap_inline{$rtype})) {
                    $stype = $typemap_inline{$rtype};
                } elsif (defined($typemap_struct{$rtype})) {
                    $stype = $typemap_struct{$rtype};
                } elsif (defined($typemap_complex{$rtype})) {
                    $stype = $typemap_complex{$rtype};
                    # shrink diff
                    $stype =~ s/DRSYS_TYPE/SYSARG_TYPE/;
                } else {
                    if ($param_in_memory) {
                        $stype = 'DRSYS_TYPE_STRUCT';
                    } else {
                        die "unknown type: \"$rtype\"\n";
                    }
                }
                if (!$param_in_memory) {
                    $stype =~ s/DRSYS_TYPE_POINTER/DRSYS_TYPE_UNKNOWN/;
                }
            }
            if ($param_in_memory) {
                $toprint .= sprintf("%s", $inout_string);
                if (defined($typemap_complex{$rtype})) {
                    $toprint .= '|CT, ';
                } else {
                    $toprint .= '|HT, ';
                }
            } elsif ($type ne 'VOID') {
                # no R or W for inlined: implied R
                $toprint .= 'SYSARG_INLINED, ';
            }
            $toprint .= $stype;
            if ($arg_bufsz[$i] ne '' && $arg_ecount[$i]) {
                $toprint .= sprintf("|SYSARG_SIZE_IN_ELEMENTS, sizeof(%s)", $rtype);
            }
            $toprint .= $arg_comment[$i];
            $toprint .= "},\n";

            if ($wrote ne '') {
                # XXX: share code w/ above
                my $wrote_inout = 'W';
                if ($wrote =~ /^\*/) {
                    $wrote =~ s/^\*//;
                    $wrote_inout = 'WI';
                }
                if (defined($arg_name_to_num{$wrote})) {
                    $toprint .= sprintf("{%d, -%d, ", $i, $arg_name_to_num{$wrote});
                } else {
                    $wrote = 'RET' if ($wrote eq 'return');
                    $toprint .= sprintf("{%d, %s, ", $i, $wrote);
                }
                $toprint .= $wrote_inout;
                if ($arg_bufsz[$i] ne '' && $arg_ecount[$i]) {
                    $toprint .= sprintf("|SYSARG_SIZE_IN_ELEMENTS, sizeof(%s)", $rtype);
                } else {
                    $toprint .= ",";
                }
                $toprint .= "\n},";
            }
        } elsif ($inout eq 'IN' && $type eq 'BOOLEAN') {
            $toprint .= sprintf("{%d,0,IB,}, ", $i);
        } else {
            die "OUT arg w/o P or * type" if ($inout =~ /OUT/);
        }
    }
    $entry{$name} .= sprintf("%d,\n     {\n%s     }\n    },\n", $argnum, $toprint);
    print $entry{$name} if ($verbose);
}

foreach $n (sort(keys %entry)) {
    print $entry{$n};
}