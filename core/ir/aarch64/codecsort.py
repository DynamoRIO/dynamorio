#!/usr/bin/env python3

# **********************************************************
# Copyright (c) 2021 Arm Limited    All rights reserved.
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

"""
Script to order instructions in codec.txt
Usage: python codecsort.py [--rewrite]
"""

import re
import sys
import os.path


DELIMITER = "# Instructions:"
CODEC_FILE = os.path.join(os.path.dirname(__file__), "codec.txt")

def read_instrs():
    """Read the instr lines from the codec.txt file"""
    seen_delimeter = False
    instrs = []

    with open(CODEC_FILE, "r") as lines:
        for line in (l.strip() for l in lines):
            if not seen_delimeter:
                if line == DELIMITER:
                    seen_delimeter = True
                continue
            if not line.strip():
                continue
            if line.strip().startswith("#"):
                continue
            instrs.append(line.split(None, 3))
    return instrs


def main():
    """Reorder the instr section of codec.txt, making sure to be a stable function"""
    instrs = read_instrs()
    instrs.sort(key=lambda line: line[2])

    # Scan for some max lengths for formatting
    instr_length = max(len(i[2]) for i in instrs)
    pre_colon = max(
        len(opndtypes.split(":")[0].strip())
        for _, _, _, opndtypes in instrs
        if ":" in opndtypes
        and len(opndtypes.split(":")[0].strip()) < 14)

    contains_z =  re.compile(r'z[0-9]+')

    v8_instrs = [i for i in instrs if not contains_z.match(i[3])]
    sve_instrs = [i for i in instrs if contains_z.match(i[3])]

    new_lines = {}

    for name, instr_list in (["v8", v8_instrs], ["sve", sve_instrs]):
        new_lines[name] = [
            "{pattern}  {nzcv}{nzcv_pad} {opcode_pad}{opcode}  {opand_pad}{opand}".format(
                pattern=pattern,
                nzcv=nzcv_flag,
                nzcv_pad=(3 - len(nzcv_flag)) * " ",
                opcode_pad=(instr_length - len(opcode)) * " ",
                opcode=opcode,
                opand_pad=(pre_colon - len(opndtypes.split(":")[0].strip())) * " ",
                opand="{} : {}".format(
                    opndtypes.split(":")[0].strip(),
                    opndtypes.split(":")[1].strip()) if ":" in opndtypes
                else opndtypes
                ).strip() for pattern, nzcv_flag, opcode, opndtypes in instr_list]

    header = []
    with open(CODEC_FILE, "r") as lines:
        for line in lines:
            header.append(line.strip("\n"))
            if line.strip() == DELIMITER:
                header.append("")
                break

    def output(dest):
        dest("\n".join(header))
        dest("\n".join(new_lines["v8"]))
        dest("\n# SVE instructions")
        dest("\n".join(new_lines["sve"]))

    if len(sys.argv) > 1 and sys.argv[1] == "--rewrite":
        with open(CODEC_FILE, "w") as codec_file:
            output(lambda l: codec_file.write(l+"\n"))
    else:
        output(lambda l: sys.stdout.write(l + "\n"))

if __name__ == "__main__":
    main()
