#!/usr/bin/env python

# **********************************************************
# Copyright (c) 2022 Arm Limited    All rights reserved.
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

# Script to check the order of operands in opnd_defs.txt and codec.c
# Usage: python aarch64_check_codec_order.py <path to core/ir/aarch64/> <path to build dir>


import sys
import subprocess
import os
import re
import difflib


def filter_lines(path, regex, ignore_until=''):
    with open(path) as f:
        patterns = []
        ignore = True
        for l in f.readlines():
            ignore = ignore and l.find(ignore_until) == -1
            if ignore:
                continue
            m = re.match(regex, l)
            if not m:
                continue
            patterns.append(m.group(1))
        return patterns


def check(l1, l2):
    if len(l1) != len(l2):
        raise Exception(
            "Lists of different length.\n"
            "In source 1, but not 2:\n"
            "{} \n"
            "In source 2, but not 1:\n"
            "{} \n".format(
                ", ".join(set(l1) - set(l2)),
                ", ".join(set(l2) - set(l1))))

    mismatches = [(i, a, b)
                  for (i, (a, b)) in enumerate(zip(l1, l2)) if a != b]

    if mismatches:
        for (i, a, b) in mismatches:
            print('Lines {} differ: \n  >  {}\n  <  {}'.format(i, a, b))
        sys.exit(1)

def normalise_plus(string):
    return string.replace("+", "x")

def main():
    src_dir = sys.argv[1]
    bld_dir = sys.argv[2]

    # Check if operand patterns in opnd_defs.txt are ordered by pattern.
    patterns = filter_lines(
        os.path.join(src_dir, 'opnd_defs.txt'),
        re.compile(r'^([x\-\?\+]+)  [a-z0-9A-Z_]+.+#.+'))
    print('Checking if operand patterns in opnd_defs.txt are ordered by pattern')
    check(patterns, sorted(patterns, key=normalise_plus))
    print('  OK!')

    # Check if operand order in opnd_defs.txt and codec.c matches.
    op_names_txt = filter_lines(
        os.path.join( src_dir, 'opnd_defs.txt'),
        re.compile(r'^[x\-\?\+]+  ([a-z0-9A-Z_]+).+#.+'))
    op_names_c = filter_lines(os.path.join(src_dir, 'codec.c'), re.compile(
        r'^decode_opnd_([^\(]+).+'), ignore_until='each type of operand')
    print('Checking if operand order in opnd_defs.txt matches codec.c')
    check(op_names_txt, op_names_c)
    print('  OK!')

    # The Arm AArch64's architecture versions supported by the DynamoRIO codec.
    # Currently, v8.0 is fully supported, while v8.1, v8.2 and SVE are partially
    # supported.
    isa_versions = ['v80', 'v81', 'v82', 'sve']

    codecsort_py = os.path.join(src_dir, "codecsort.py")

    print("Checking if instructions are in the correct order and format in each codec_<version>.txt file.")
    for isa_version in isa_versions:
        codec_file = os.path.join(src_dir, 'codec_' + isa_version + '.txt')
        reordered_codec = subprocess.check_output([codecsort_py, codec_file])
        with open(codec_file) as f:

            if f.read().strip() != reordered_codec.decode().strip():
                print("{} instructions are out of order, run:\n{} --rewrite {}".format(
                    codec_file, codecsort_py, codec_file))
                sys.exit(1)
            print(" " + codec_file + " OK!")

    print("Check there are no duplicate opcode enums across ALL codec_<version>.txt files.")

    # Create one codec_all.txt file which concatanates all codec_<version>.txt files.
    codec_all_file = os.path.join(bld_dir, 'codec_all.txt')
    with open(codec_all_file, "w") as codec_all_txt:
        codec_all_txt.write("\n# Instruction definitions:\n\n")
        for isa_version in isa_versions:
            codec_file = os.path.join(src_dir, 'codec_' + isa_version + '.txt')
            with open(codec_file, "r") as lines:
                for line in (l.strip() for l in lines if l.strip()):
                    if line.strip().startswith("#"):
                        continue
                    codec_all_txt.write(line + "\n")

    # Call codecsort.py to check for opcode enum duplicates in codec_all.txt
    # using the --global option to limit checking for duplicates only.
    opc_dup_enum_status = subprocess.check_output([codecsort_py, '--global', codec_all_file])
    print(opc_dup_enum_status)

if __name__ == '__main__':
    main()
