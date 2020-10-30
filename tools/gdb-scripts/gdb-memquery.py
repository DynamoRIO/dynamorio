#!/usr/bin/env python

# **********************************************************
# Copyright (c) 2020 Google, Inc.    All rights reserved.
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

# Searches /proc/self/maps for the line containing a given address.
# Usage:
# 1) Place this line into your ~/.gdbinit:
#    source <path-to>/gdb-memquery.py
# 2) Execute the command in gdb:
#    (gdb) memquery $rsp
#    7ffffffde000-7ffffffff000 rw-p 00000000 00:00 0                          [stack]

try:
    import gdb
except ImportError as e:
    raise ImportError("This script must be run in GDB: ", str(e))
import re
from subprocess import check_output

class MemQueryCommand (gdb.Command):
    """Prints memory address properties"""
    def __init__ (self):
        super (MemQueryCommand, self).__init__ ("memquery",
                                                gdb.COMMAND_DATA,
                                                gdb.COMPLETE_FILENAME)
    def invoke(self, arg, unused_from_tty):
        addr = gdb.parse_and_eval(arg)
        pid = int(gdb.selected_inferior().pid)
        map_name = "/proc/%d/maps" % pid
        with open(map_name,'r') as map:
            for line in map:
                line = line.rstrip()
                if not line: continue
                match = re.match(r'^(\w+)-(\w+)', line)
                if match:
                    start = int(match.group(1), 16)
                    end = int(match.group(2), 16)
                    if addr >= start and addr < end:
                        print(line)
MemQueryCommand()
