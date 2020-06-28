#!/usr/bin/env python

# **********************************************************
# Copyright (c) 2018 Arm Limited    All rights reserved.
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

# Script to check the order of operands in codec.txt and codec.c
#   Usage: python aarch64_codec_codec_order.py <path to core/ir/aarch64/>


import sys
import os
import re


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
    assert len(l1) == len(l2)
    mismatches = [(i, a, b)
                  for (i, (a, b)) in enumerate(zip(l1, l2)) if a != b]

    if mismatches:
        for (i, a, b) in mismatches:
            print('Lines {} differ: \n  >  {}\n  <  {}'.format(i, a, b))
        sys.exit(1)


def main():
    src_dir = sys.argv[1]

    # Check if operand patterns in codec.txt are ordered by pattern.
    patterns = filter_lines(
        os.path.join(src_dir, 'codec.txt'),
        re.compile(r'^([x\-\?]+)  [a-z0-9A-Z_]+.+#.+'))
    print('Checking if operand patterns in codec.txt are ordered by pattern')
    check(patterns, sorted(patterns))
    print('  OK!')

    # Check if operand order in codec.txt and codec.c matches.
    op_names_txt = filter_lines(
        os.path.join( src_dir, 'codec.txt'),
        re.compile(r'^[x\-\?]+  ([a-z0-9A-Z_]+).+#.+'))
    op_names_c = filter_lines(os.path.join(src_dir, 'codec.c'), re.compile(
        r'^decode_opnd_([^\(]+).+'), ignore_until='each type of operand')
    print('Checking if operand order in codec.txt matches codec.c')
    check(op_names_txt, op_names_c)
    print('  OK!')


if __name__ == '__main__':
    main()
