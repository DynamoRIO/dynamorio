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

class CodecLine:
    """Container to keep line info together"""
    def __init__(self, pattern, nzcv, enum, opcode, opndtypes):
        self.enum = enum
        self.pattern = pattern
        self.nzcv = nzcv
        self.opcode = opcode
        self.opndtypes = opndtypes

def read_instrs():
    """Read the instr lines from the codec.txt file"""
    seen_delimeter = False
    instrs = []

    with open(CODEC_FILE, "r") as lines:
        for line in (l.strip() for l in lines if l.strip()):
            if not seen_delimeter:
                if line == DELIMITER:
                    seen_delimeter = True
                continue
            if line.strip().startswith("#"):
                continue
            line = line.split(None, 4)
            if not line[2].isnumeric():
                # missing an enum entry, put a none in
                line = [line[0], line[1], None, line[2], " ".join(line[3:])]
            instrs.append(CodecLine(*line))

    return instrs

def handle_enums(instrs):
    """Make sure that every instr has an enum and that there are no clashes"""
    # There are 5 values populated by default into the enum so
    # we need to make sure that our first index is after those
    max_enum = 5
    enums = {}
    for i in (i for i in instrs if i.enum):
        if i.opcode in enums:
            assert enums[i.opcode] == i.enum, \
                "Multiple enums for the same opcode {}: {} {}".format(
                    i.opcode, i.enum, enums[i.opcode])
        else:
            enums[i.opcode] = i.enum

    enums = {i.opcode: i.enum for i in instrs if i.enum}
    if enums:
        max_enum = max(int(i.enum) for i in instrs if i.enum)

    for i in (i for i in instrs if not i.enum):
        try:
            i.enum = enums[i.opcode]
        except KeyError:
            max_enum += 1
            i.enum = max_enum
            enums[i.opcode] = max_enum


def main():
    """Reorder the instr section of codec.txt, making sure to be a stable function"""
    instrs = read_instrs()
    instrs.sort(key=lambda line: line.opcode)

    handle_enums(instrs)

    # Scan for some max lengths for formatting
    instr_length = max(len(i.opcode) for i in instrs)
    pre_colon = max(
        len(i.opndtypes.split(":")[0].strip())
        for i in instrs
        if ":" in i.opndtypes
        and len(i.opndtypes.split(":")[0].strip()) < 14)

    contains_z =  re.compile(r'z[0-9]+')

    v8_instrs = [i for i in instrs if not contains_z.match(i.opndtypes)]
    sve_instrs = [i for i in instrs if contains_z.match(i.opndtypes)]

    new_lines = {}

    for name, instr_list in (["v8", v8_instrs], ["sve", sve_instrs]):
        new_lines[name] = [
            "{pattern}  {nzcv:<3} {enum:<4} {opcode_pad}{opcode}  {opand_pad}{opand}".format(
                enum=i.enum,
                pattern=i.pattern,
                nzcv=i.nzcv,
                opcode_pad=(instr_length - len(i.opcode)) * " ",
                opcode=i.opcode,
                opand_pad=(pre_colon - len(i.opndtypes.split(":")[0].strip())) * " ",
                opand="{} : {}".format(
                    i.opndtypes.split(":")[0].strip(),
                    i.opndtypes.split(":")[1].strip()) if ":" in i.opndtypes
                else i.opndtypes
                ).strip() for i in instr_list]

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
