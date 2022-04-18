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

# Loads symbols for libdynamorio.so by looking in /proc/self/maps.
# This clears the symbol table, so run it early/first.
# TODO i#2100: Integrate with a revived libdynamorio.so-gdb.py.
# Usage:
# 1) Place this line into your ~/.gdbinit:
#    source <path-to>/gdb-drsymload.py
# 2) Execute the command in gdb:
#    (gdb) drsymload

try:
    import gdb
except ImportError as e:
    raise ImportError("This script must be run in GDB: ", str(e))
import re
import subprocess
from pathlib import Path
import os.path

class DRSymLoadCommand (gdb.Command):
    """Loads symbols for libdynamorio.so"""
    def __init__ (self):
        super (DRSymLoadCommand, self).__init__ ("drsymload",
                                                gdb.COMMAND_DATA,
                                                gdb.COMPLETE_FILENAME)
    def invoke(self, unused_arg, unused_from_tty):
        pid = int(gdb.selected_inferior().pid)
        exefile = "/proc/%d/exe" % pid
        drfile = str(Path(exefile).resolve())
        base = 0
        map_name = "/proc/%d/maps" % pid
        with open(map_name,'r') as map:
            for line in map:
                line = line.rstrip()
                if not line: continue
                match = re.match(r'^([^-]*)-.*libdynamorio.so', line)
                if match:
                    base = int(match.group(1), 16)
                    print("libdynamorio base is 0x%x" % base)
                    break
        p = subprocess.Popen(["objdump", "-h", drfile], stdout=subprocess.PIPE)
        stdout, _ = p.communicate()
        for line in iter(stdout.splitlines()):
            match = re.match(r'.*\.text *\w+ *\w+ *\w+ *(\w+)', str(line))
            if match:
                offs = match.group(1)
                cmd = "add-symbol-file %s 0x%x+0x%s" % (drfile, base, offs)
                # First clear the symbols (sometimes gdb has DR syms at the wrong
                # load address).
                gdb.execute("symbol-file")
                print("Running %s" % cmd)
                gdb.execute(cmd)
DRSymLoadCommand()
