#!/usr/bin/env python3
# **********************************************************
# Copyright (c) 2026 Google, Inc.    All rights reserved.
# **********************************************************
#
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
# ARE DISCLAIMED. IN NO EVENT SHALL GOOGLE, INC. OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
# SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
# CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
# OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
# DAMAGE.
"""Checks divval output for test/divtest.c.

  check.py PREFIX EXPECTED.csv TRACE_FILE

Verifies that:
  * the -trace records of the divides in the divtest module match, in order,
    the sequence that divtest says it executed;
  * PREFIX.hist.csv agrees with the histogram recomputed from the trace;
  * the per-site counts in PREFIX.sites.csv add up to the histogram total.
"""

import collections
import csv
import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), '..'))
import divval  # pylint: disable=wrong-import-position


def main():
    if len(sys.argv) != 4:
        sys.exit(__doc__)
    prefix, expected_path, trace_path = sys.argv[1:]
    sites = divval.load_sites(prefix)
    test_sites = {s for s, row in sites.items() if row['module'].startswith('divtest')}
    if not test_sites:
        sys.exit('FAIL: no divide sites found in the divtest module')

    with open(expected_path, newline='') as f:
        expected = [(r['op'], int(r['width']), int(r['dividend']), int(r['divisor']))
                    for r in csv.DictReader(f)]
    all_recs = list(divval.iter_trace(trace_path, sites))
    actual = [(op, w, n, d) for _, s, op, w, n, d in all_recs if s in test_sites]
    errors = 0
    if len(actual) != len(expected):
        print('FAIL: %d divides traced in divtest, expected %d' %
              (len(actual), len(expected)))
        errors += 1
    for i, (a, e) in enumerate(zip(actual, expected)):
        if a != e:
            print('FAIL: record %d: got %s, expected %s' % (i, a, e))
            errors += 1
            if errors > 10:
                break

    # The histogram covers all divides (including libc's), as does the trace.
    keys = ('op', 'width', 'n_neg', 'd_neg', 'd_pow2', 'n_bits', 'd_bits', 'q_bits')
    from_trace = collections.Counter()
    for _, _, op, w, n, d in all_recs:
        feat = divval.features(op, w, n, d)
        from_trace[tuple(feat[k] for k in keys)] += 1
    from_hist = collections.Counter()
    for feat, count in divval.load_hist(prefix):
        from_hist[tuple(feat[k] for k in keys)] += count
    if from_trace != from_hist:
        print('FAIL: histogram does not match the trace (%d vs %d entries)' %
              (len(from_hist), len(from_trace)))
        for k in sorted(set(from_trace) | set(from_hist))[:10]:
            if from_trace[k] != from_hist[k]:
                print('  %s: trace %d hist %d' % (k, from_trace[k], from_hist[k]))
        errors += 1
    site_total = sum(row['count'] for row in sites.values())
    if site_total != sum(from_hist.values()):
        print('FAIL: site counts sum to %d, histogram to %d' %
              (site_total, sum(from_hist.values())))
        errors += 1

    if errors:
        sys.exit(1)
    print('PASS: %d divtest divides match (%d divides total, %d sites)' %
          (len(actual), len(all_recs), len(sites)))


if __name__ == '__main__':
    main()
