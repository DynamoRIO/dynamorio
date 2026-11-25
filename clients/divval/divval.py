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
"""Post-processing for the output of the divval DynamoRIO client.

Only uses the Python standard library.

  divval.py summary   PREFIX
  divval.py latency   PREFIX [-m MODEL]... [--trace FILE] [--csv OUT]
  divval.py trace2csv PREFIX.trace.bin [-o OUT.csv]

PREFIX is the common prefix of the output files, e.g. ./divval.myapp.1234
(as printed by the client at exit).

A latency MODEL is either the name of a preset (see --list-models) or a
Python expression evaluated per divide.  Available variables:

  op            'udiv' or 'sdiv'
  signed        True for sdiv
  width         32 or 64
  n_bits        significant bits of |dividend| (0..64; 0 means dividend == 0)
  d_bits        significant bits of |divisor|  (0 means divide by zero)
  q_bits        significant bits of |quotient| (0 means quotient == 0)
  n_neg, d_neg  operand is negative (sdiv only)
  d_pow2        |divisor| is a power of 2 (including 1)

With --trace (requires the client's -trace output) these are also available:

  n, d, q, r    dividend, divisor, quotient, remainder as Python ints
                (signed for sdiv), with AArch64 semantics: x / 0 == 0 and
                INT_MIN / -1 == INT_MIN.
  site          static site index (see PREFIX.sites.csv)

plus the functions ceil, floor, log2, min, max, abs.
"""

import argparse
import collections
import csv
import math
import struct
import sys

TRACE_MAGIC = b'DIVVALTR'
TRACE_VERSION = 1
HEADER = struct.Struct('<8sII')
RECORD = struct.Struct('<QQQ')

# Illustrative latency schemes, in cycles.  These do not describe any specific
# core: they are starting points for writing your own expressions.
PRESETS = collections.OrderedDict([
    ('fixed', '12 if width == 32 else 20'),
    # Early termination at 1, 2 or 4 quotient bits per cycle, plus fixed
    # overhead for normalization and result fix-up.
    ('radix2', '3 + max(q_bits, 1)'),
    ('radix4', '3 + ceil(max(q_bits, 1) / 2)'),
    ('radix16', '3 + ceil(max(q_bits, 1) / 4)'),
    # Iteration count estimated from leading-zero counts of the operands
    # (as hardware does before it knows the quotient), radix 4.
    ('lzc_radix4', '3 + ceil(max(n_bits - d_bits + 1, 1) / 2)'),
    # Radix 4 with a fast path for divisor == 0 / power of 2 / dividend < divisor.
    ('radix4_fastpath',
     '2 if (d_bits == 0 or d_pow2 or q_bits == 0) '
     'else 3 + ceil(q_bits / 2)'),
])

MODEL_FUNCS = {
    'ceil': math.ceil,
    'floor': math.floor,
    'log2': math.log2,
    'min': min,
    'max': max,
    'abs': abs,
}


def _int(row, key):
    return int(row[key], 0)


def load_sites(prefix):
    """Returns {site index: row dict} from PREFIX.sites.csv."""
    sites = {}
    with open(prefix + '.sites.csv', newline='') as f:
        for row in csv.DictReader(f):
            for key in ('site', 'pc', 'offset', 'width', 'count', 'div_by_zero',
                        'divisor_pow2', 'quotient_zero', 'repeat_divisor',
                        'repeat_operands'):
                row[key] = _int(row, key)
            row['avg_quotient_bits'] = float(row['avg_quotient_bits'])
            sites[row['site']] = row
    return sites


def load_hist(prefix):
    """Returns the rows of PREFIX.hist.csv as (features dict, count) pairs."""
    rows = []
    with open(prefix + '.hist.csv', newline='') as f:
        for row in csv.DictReader(f):
            feat = {
                'op': row['op'],
                'signed': row['op'] == 'sdiv',
                'width': _int(row, 'width'),
                'n_neg': bool(_int(row, 'dividend_neg')),
                'd_neg': bool(_int(row, 'divisor_neg')),
                'd_pow2': bool(_int(row, 'divisor_pow2')),
                'n_bits': _int(row, 'dividend_bits'),
                'd_bits': _int(row, 'divisor_bits'),
                'q_bits': _int(row, 'quotient_bits'),
            }
            rows.append((feat, _int(row, 'count')))
    return rows


def _to_signed(x):
    return x - (1 << 64) if x >= (1 << 63) else x


def iter_trace(path, sites):
    """Yields (seq, site, op, width, dividend, divisor) from a -trace file.

    Values are Python ints, signed for sdiv.
    """
    if path.endswith('.csv'):
        with open(path, newline='') as f:
            for row in csv.DictReader(f):
                yield (_int(row, 'seq'), _int(row, 'site'), row['op'],
                       _int(row, 'width'), _int(row, 'dividend'),
                       _int(row, 'divisor'))
        return
    with open(path, 'rb') as f:
        magic, version, rec_size = HEADER.unpack(f.read(HEADER.size))
        if magic != TRACE_MAGIC or version != TRACE_VERSION or \
                rec_size != RECORD.size:
            raise ValueError('%s: not a divval v%d binary trace' %
                             (path, TRACE_VERSION))
        seq = 0
        chunk_size = RECORD.size * 65536
        while True:
            chunk = f.read(chunk_size)
            if not chunk:
                break
            usable = len(chunk) - len(chunk) % RECORD.size
            for n, d, site in RECORD.iter_unpack(chunk[:usable]):
                info = sites[site]
                if info['op'] == 'sdiv':
                    n, d = _to_signed(n), _to_signed(d)
                yield seq, site, info['op'], info['width'], n, d
                seq += 1


def arch_divide(op, width, n, d):
    """Returns (quotient, remainder) with AArch64 UDIV/SDIV semantics."""
    if d == 0:
        return 0, n
    q = abs(n) // abs(d)
    if (n < 0) != (d < 0):
        q = -q
    if op == 'sdiv' and q == 1 << (width - 1):
        q = -q  # INT_MIN / -1 wraps to INT_MIN.
    return q, n - q * d


def features(op, width, n, d):
    """Same features as a PREFIX.hist.csv row, computed from exact values."""
    q, _ = arch_divide(op, width, n, d)
    q_mag = abs(n) // abs(d) if d != 0 else 0
    return {
        'op': op,
        'signed': op == 'sdiv',
        'width': width,
        'n_neg': n < 0,
        'd_neg': d < 0,
        'd_pow2': d != 0 and (abs(d) & (abs(d) - 1)) == 0,
        'n_bits': abs(n).bit_length(),
        'd_bits': abs(d).bit_length(),
        'q_bits': q_mag.bit_length(),
        'n': n,
        'd': d,
        'q': q,
        'r': n - q * d,
    }


def _pct(part, total):
    return 100.0 * part / total if total else 0.0


def _percentile(dist, total, frac):
    target = frac * total
    cum = 0
    for value in sorted(dist):
        cum += dist[value]
        if cum >= target:
            return value
    return None


def cmd_summary(args):
    sites = load_sites(args.prefix)
    hist = load_hist(args.prefix)
    total = sum(c for _, c in hist)
    print('Total divides: %d at %d static sites' % (total, len(sites)))
    if total == 0:
        return
    groups = collections.defaultdict(list)
    for feat, count in hist:
        groups[(feat['op'], feat['width'])].append((feat, count))
    for key in sorted(groups):
        rows = groups[key]
        n = sum(c for _, c in rows)
        print('\n%s%d: %d (%.1f%% of divides)' % (key[0], key[1], n,
                                                 _pct(n, total)))
        by_zero = sum(c for f, c in rows if f['d_bits'] == 0)
        pow2 = sum(c for f, c in rows if f['d_pow2'])
        q_zero = sum(c for f, c in rows if f['q_bits'] == 0 and f['d_bits'])
        neg = sum(c for f, c in rows if f['n_neg'] or f['d_neg'])
        mean_q = sum(f['q_bits'] * c for f, c in rows) / n
        print('  divisor == 0: %.2f%%  |divisor| power of 2: %.2f%%  '
              '|dividend| < |divisor|: %.2f%%  negative operand: %.2f%%' %
              (_pct(by_zero, n), _pct(pow2, n), _pct(q_zero, n), _pct(neg, n)))
        print('  mean significant bits: dividend %.2f  divisor %.2f  '
              'quotient %.2f' %
              (sum(f['n_bits'] * c for f, c in rows) / n,
               sum(f['d_bits'] * c for f, c in rows) / n, mean_q))
        buckets = collections.Counter()
        for f, c in rows:
            b = f['q_bits']
            buckets[0 if b == 0 else (b - 1) // 8 + 1] += c
        print('  quotient bits: ' + '  '.join(
            '%s: %.1f%%' % ('0' if b == 0 else '%d-%d' % (8 * b - 7, 8 * b),
                            _pct(buckets[b], n))
            for b in sorted(buckets)))
    print('\nTop sites:')
    print('  %10s %6s  %-6s %8s %8s  %s' %
          ('count', 'pct', 'op', 'avg_qb', 'rep_div', 'location / disasm'))
    for s in sorted(sites.values(), key=lambda s: -s['count'])[:args.top]:
        print('  %10d %5.1f%%  %-6s %8.2f %7.1f%%  %s+0x%x  %s' %
              (s['count'], _pct(s['count'], total),
               '%s%d' % (s['op'], s['width']), s['avg_quotient_bits'],
               _pct(s['repeat_divisor'], s['count']), s['module'],
               s['offset'], s['disasm']))


def _compile_model(model):
    expr = PRESETS.get(model, model)
    try:
        return expr, compile(expr, '<model>', 'eval')
    except SyntaxError as e:
        sys.exit('Invalid model expression %r: %s' % (expr, e))


def cmd_latency(args):
    models = args.model or list(PRESETS)
    compiled = [(m,) + _compile_model(m) for m in models]
    dists = [collections.Counter() for _ in compiled]
    by_group = [collections.defaultdict(collections.Counter) for _ in compiled]
    env = {'__builtins__': {}}
    env.update(MODEL_FUNCS)

    def feed(feat, count):
        for i, (_, _, code) in enumerate(compiled):
            lat = eval(code, env, feat)
            dists[i][lat] += count
            by_group[i]['%s%d' % (feat['op'], feat['width'])][lat] += count

    if args.trace:
        sites = load_sites(args.prefix)
        for _, site, op, width, n, d in iter_trace(args.trace, sites):
            feat = features(op, width, n, d)
            feat['site'] = site
            feed(feat, 1)
    else:
        for feat, count in load_hist(args.prefix):
            feed(feat, count)

    out = None
    if args.csv:
        out = csv.writer(open(args.csv, 'w', newline=''))
        out.writerow(['model', 'expr', 'group', 'latency', 'count', 'fraction'])
    for (name, expr, _), dist, groups in zip(compiled, dists, by_group):
        total = sum(dist.values())
        print('model %s: %s' % (name, expr) if name != expr else
              'model: %s' % expr)
        if total == 0:
            print('  no divides')
            continue
        for label, d in [('all', dist)] + sorted(groups.items()):
            n = sum(d.values())
            mean = sum(k * v for k, v in d.items()) / n
            print('  %-7s n=%-10d mean=%7.2f  p50=%-4s p90=%-4s p99=%-4s max=%s'
                  % (label, n, mean, _percentile(d, n, 0.5),
                     _percentile(d, n, 0.9), _percentile(d, n, 0.99), max(d)))
            if out:
                for lat in sorted(d):
                    out.writerow([name, expr, label, lat, d[lat], d[lat] / n])
        if args.verbose:
            cum = 0
            print('    %8s %12s %7s %7s' % ('latency', 'count', 'pct', 'cum'))
            for lat in sorted(dist):
                cum += dist[lat]
                print('    %8s %12d %6.2f%% %6.2f%%' %
                      (lat, dist[lat], _pct(dist[lat], total), _pct(cum, total)))


def cmd_trace2csv(args):
    path = args.trace
    prefix = args.prefix
    if prefix is None and path.endswith('.trace.bin'):
        prefix = path[:-len('.trace.bin')]
    if prefix is None:
        sys.exit('Cannot derive the prefix from %s: use --prefix' % path)
    sites = load_sites(prefix)
    out = open(args.output, 'w', newline='') if args.output else sys.stdout
    w = csv.writer(out, lineterminator='\n')
    w.writerow(['seq', 'site', 'op', 'width', 'dividend', 'divisor'])
    for rec in iter_trace(path, sites):
        w.writerow(rec)


def main():
    parser = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--list-models', action='store_true',
                        help='List the preset latency models and exit.')
    sub = parser.add_subparsers(dest='cmd')

    p = sub.add_parser('summary', help='Operand statistics.')
    p.add_argument('prefix')
    p.add_argument('--top', type=int, default=10, help='Number of sites to show.')
    p.set_defaults(func=cmd_summary)

    p = sub.add_parser('latency', help='Latency distribution under models.')
    p.add_argument('prefix')
    p.add_argument('-m', '--model', action='append',
                   help='Preset name or expression (repeatable). '
                   'Default: all presets.')
    p.add_argument('--trace', help='Evaluate over this -trace file instead of '
                   'the histogram (exposes n, d, q, r, site).')
    p.add_argument('--csv', help='Write the distributions to this CSV file.')
    p.add_argument('-v', '--verbose', action='store_true',
                   help='Print the full distribution.')
    p.set_defaults(func=cmd_latency)

    p = sub.add_parser('trace2csv', help='Convert a binary -trace file to CSV.')
    p.add_argument('trace')
    p.add_argument('--prefix', help='Output prefix (default: from the path).')
    p.add_argument('-o', '--output', help='Output file (default: stdout).')
    p.set_defaults(func=cmd_trace2csv)

    args = parser.parse_args()
    if args.list_models:
        for name, expr in PRESETS.items():
            print('%-16s %s' % (name, expr))
        return
    if not args.cmd:
        parser.error('missing command')
    args.func(args)


if __name__ == '__main__':
    main()
