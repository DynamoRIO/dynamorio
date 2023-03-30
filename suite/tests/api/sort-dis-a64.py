#!/usr/bin/env python3

# ***********************************************************
# Copyright (c) 2022 Arm Limited    All rights reserved.
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

import sys
import argparse

def main(dis_a64, rewrite=False):
    tests = []
    header = []
    header_done = False
    with open(dis_a64) as a64_file:
        current_test = []
        for line in (line.strip() for line in a64_file):
            if not header_done:
                header.append(line)
                if line == "# Tests:":
                    header_done = True
                continue

            if line.strip() != "":
                current_test.append(line)
            elif current_test:
                tests.append(current_test)
                current_test = []

    if current_test:
        tests.append(current_test)

    def sort_fn(test):
        """
        Produce a unique key for a block of tests that can be used to sort the tests.
        """
        # Sort the tests alphabetically by mnemonic.
        # Some instructions have multiple variants with the same mnemonic so we add the
        # encoding of the first instruction in the test to the end of the key to make it
        # unique. This ensures that we have a stable order when adding tests to a file.
        valid_line = next(line for line in test if line and not line.strip().startswith("#"))
        parts = valid_line.split(":")
        encoding = parts[0].strip()
        mnemonic = parts[1].strip().split()[0]
        return mnemonic+encoding

    tests.sort(key=sort_fn)

    if rewrite:
        out_file = open(dis_a64, "w")
    else:
        out_file = sys.stdout

    print("\n".join(header), file=out_file)
    for test in tests:
        print("\n".join(test), file=out_file)
        print(file=out_file)
    if rewrite:
        out_file.close()

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--dis-a64",  help="the location of the disassembly tests", required=True)
    parser.add_argument(
        "--rewrite", help="overwrite the file", action="store_true", default=False)
    args = parser.parse_args(sys.argv[1:])
    sys.exit(main(args.dis_a64, args.rewrite))
