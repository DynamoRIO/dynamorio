#!/usr/bin/env python3

# ***********************************************************
# Copyright (c) 2021-2023 Arm Limited    All rights reserved.
# ***********************************************************

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
Script to order instructions in codec_<version>.txt
Usage: python codecsort.py [--rewrite|--global] codec_<version>.txt
"""

import re
import sys
import os.path


DELIMITER = "# Instruction definitions:"

class CodecLine:
    """Container to keep line info together"""
    def __init__(self, pattern, nzcv, enum, feat, opcode, opndtypes):
        self.enum = enum
        self.feat = feat
        self.pattern = pattern
        self.nzcv = nzcv
        self.opcode = opcode
        self.opndtypes = opndtypes

def read_instrs(codec_file):
    """Read the instr lines from the codec_<version>.txt file"""
    seen_delimeter = False
    instrs = []

    with open(codec_file, "r") as lines:
        for line in (l.strip() for l in lines if l.strip()):
            try:
                if not seen_delimeter:
                    if line == DELIMITER:
                        seen_delimeter = True
                    continue
                if line.strip().startswith("#"):
                    continue
                line = line.split(None, 5)
                if not line[2].isnumeric():
                    # missing an enum entry, put a none in
                    line = [line[0], line[1], None, line[2], line[3], " ".join(line[4:])]
                instrs.append(CodecLine(*line))
            except:
                print("Error parsing line: {}".format(line), file=sys.stderr)
                raise

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

    reversed_enums = {}
    for opcode, enum in enums.items():
        reversed_enums.setdefault(enum, set()).add(opcode)
    for enum, opcodes in reversed_enums.items():
        assert len(opcodes) == 1, \
            "Multiple opcodes for the same enum {}: {}".format(
                enum, ','.join(opcodes))

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
    """Reorder the given codec_<version>.txt """

    if len(sys.argv) < 2:
        print('Usage: codecsort.py [--rewrite|--global] <codec definitions files>')
        sys.exit(1)

    is_rewrite = False
    if len(sys.argv) >= 3 and sys.argv[1] == "--rewrite":
        is_rewrite = True
        codec_files = sys.argv[2:]
    else:
        codec_files = sys.argv[1:]

    instr_orig = {codec_file: read_instrs(codec_file) for codec_file in codec_files}
    file_instrs = {codec_file: sorted(instrs, key=lambda line: line.opcode) for codec_file, instrs in instr_orig.items()}

    handle_enums([instr for finstrs in file_instrs.values() for instr in finstrs])

    for codec_file, instrs in file_instrs.items():
        new_lines = []

        if instrs:
            # Scan for some max lengths for formatting
            instr_length = max(len(i.opcode) for i in instrs)
            pre_colon = max(
                len(i.opndtypes.split(":")[0].strip())
                for i in instrs
                if ":" in i.opndtypes
                and len(i.opndtypes.split(":")[0].strip()) < 14)

            for instr in instrs:
                new_lines.append(
                    "{pattern}  {nzcv:<3} {enum:<4} {feat:<4} {opcode_pad}{opcode}  {opand_pad}{opand}".format(
                        enum=instr.enum,
                        feat=instr.feat,
                        pattern=instr.pattern,
                        nzcv=instr.nzcv,
                        opcode_pad=(instr_length - len(instr.opcode)) * " ",
                        opcode=instr.opcode,
                        opand_pad=(pre_colon - len(instr.opndtypes.split(":")[0].strip())) * " ",
                        opand="{} : {}".format(
                            instr.opndtypes.split(":")[0].strip(),
                            instr.opndtypes.split(":")[1].strip()) if ":" in instr.opndtypes
                        else instr.opndtypes
                        ).strip())

        header = []
        with open(codec_file, "r") as lines:
            for line in lines:
                header.append(line.strip("\n"))
                if line.strip() == DELIMITER:
                    header.append("")
                    break

        def output(dest):
            dest("\n".join(header))
            dest("\n".join(new_lines))

        if is_rewrite:
            with open(codec_file, "w") as codec_txt:
                output(lambda l: codec_txt.write(l+"\n"))
        else:
            output(lambda l: sys.stdout.write(l + "\n"))

            if instr_orig[codec_file] != file_instrs[codec_file]:
                sys.exit(1)

if __name__ == "__main__":
    main()
