# divval: operand values of AArch64 integer divides

`divval` is a DynamoRIO client that records the operands of every dynamic
AArch64 scalar integer divide (`UDIV`/`SDIV`, both the 32-bit `W` and the 64-bit
`X` forms). You can use it to study the statistical properties of divide
operands in a workload, see how different data-dependent latency schemes would
shape the latency distribution, and pick a divider model for a simulator.

## Outputs

All files share the prefix `<outdir>/divval.<app>.<pid>`. The client prints
this prefix at exit.

| File | Always? | Size | Use |
|---|---|---|---|
| `.sites.csv` | yes | one row per static divide | where the divides are and how hot each one is; value locality |
| `.hist.csv` | yes | bounded, typically a few hundred rows | latency models that depend on operand/quotient magnitudes |
| `.trace.bin` / `.trace.csv` | with `-trace` | 24 B (binary) per dynamic divide | models that need exact values or the order of divides |

The aggregate files cover most latency studies and stay small no matter how
long the run is. Real dividers mostly vary their latency with how many
quotient bits they have to produce (early termination), which comes down to
the leading-zero counts of the operands, plus a few special cases (division
by zero, divisor is a power of 2, dividend smaller than divisor). The
histogram keeps exactly these features. When a scheme needs more than that,
for example a divide-result cache or a reciprocal cache (which depends on
which values repeat), or a simulator that replays the actual values, turn on
`-trace` to get the full sequence.

### `.sites.csv`

| column | meaning |
|---|---|
| `site` | site index (matches `site` in the trace) |
| `pc`, `module`, `offset` | location of the instruction (`offset` is relative to the module base) |
| `op`, `width` | `udiv`/`sdiv`, 32/64 |
| `count` | dynamic executions |
| `div_by_zero` | executions with divisor == 0 (the result is 0 on AArch64: no trap) |
| `divisor_pow2` | executions where \|divisor\| is a power of 2 (including 1) |
| `quotient_zero` | executions where \|dividend\| < \|divisor\| (or divisor == 0) |
| `repeat_divisor` | executions with the same divisor as the previous execution of this site |
| `repeat_operands` | executions with the same dividend *and* divisor as the previous one |
| `avg_quotient_bits` | mean significant bits of \|quotient\| |
| `disasm` | disassembly |

### `.hist.csv`

This is a joint histogram in long ("tidy") format: one row per distinct
combination of the dimensions below, with its `count`.

| column | meaning |
|---|---|
| `op`, `width` | `udiv`/`sdiv`, 32/64 |
| `dividend_neg`, `divisor_neg` | operand is negative (always 0 for `udiv`) |
| `divisor_pow2` | \|divisor\| is a power of 2 |
| `dividend_bits`, `divisor_bits`, `quotient_bits` | significant bits of the magnitude: `64 - clz(\|x\|)`, so 0 means the value is 0 |
| `count` | number of dynamic divides with these features |

The values follow the semantics of each instruction. For 32-bit forms, only
the low half of the source registers is used. For `sdiv`, the operands are
sign-extended and the magnitudes are taken as absolute values. `INT_MIN / -1`
has a quotient with `width` bits (the architectural result wraps to `INT_MIN`).

### `.trace.bin` / `.trace.csv` (`-trace`)

The trace holds every dynamic divide in execution order.

* **Binary** (default, compact). A 16-byte header (`"DIVVALTR"`, `u32 version=1`,
  `u32 record_size=24`) followed by little-endian records
  `{u64 dividend, u64 divisor, u64 site}`. The values are normalized: they are
  zero-extended for `udiv` and sign-extended for `sdiv` (read them as `int64`).
  The signedness and width of each record come from `.sites.csv`.
* **CSV** (`-trace_format csv`). Rows of `seq,site,op,width,dividend,divisor`,
  with decimal values that are signed for `sdiv`.

Use `-trace_max N` to cap the number of trace records. The aggregate files
still cover the whole run.

## Building

The client builds only for AArch64. On other architectures it is skipped.

**In the DynamoRIO tree.** `clients/CMakeLists.txt` picks up this directory
automatically, so building DynamoRIO for AArch64 produces
`clients/lib64/<config>/libdivval.so`.

**Standalone**, against a DynamoRIO build or release package:

```sh
cmake -DDynamoRIO_DIR=<DR>/cmake <path-to>/clients/divval
make
```

**Cross-compiling from x86-64:**

```sh
cmake -DCMAKE_TOOLCHAIN_FILE=<dr-src>/make/toolchain-aarch64.cmake \
      -DDynamoRIO_DIR=<dr-aarch64-build>/cmake <path-to>/clients/divval
```

## Running

```sh
drrun -c libdivval.so [options] -- <app> <args>
```

| option | default | meaning |
|---|---|---|
| `-outdir DIR` | app cwd | where to write the outputs |
| `-trace` | off | also write the full sequence |
| `-trace_format binary\|csv` | `binary` | format of the trace |
| `-trace_max N` | 0 (unlimited) | maximum number of trace records |
| `-verbose N` | 0 | at 2 or higher, prints each site when it is first instrumented |

## Analysis: `divval.py`

`divval.py` uses only the Python standard library.

```sh
# Operand statistics per op/width and the hottest sites.
divval.py summary ./divval.myapp.1234

# Latency distribution (mean, p50/p90/p99, max) under the preset schemes.
divval.py latency ./divval.myapp.1234
divval.py --list-models

# Your own scheme: any Python expression over the histogram features.
divval.py latency ./divval.myapp.1234 \
    -m '2 + ceil(max(q_bits, 1) / 2)' \
    -m '1 if d_bits == 0 else 4 + ceil(max(n_bits - d_bits + 1, 1) / 3)' \
    --csv latency.csv -v

# Schemes that need exact values evaluate over the trace (adds n, d, q, r, site).
divval.py latency ./divval.myapp.1234 --trace ./divval.myapp.1234.trace.bin \
    -m '1 if d in (1, 2, 4, 8, 10) else 3 + ceil(max(q_bits, 1) / 2)'

# Convert a binary trace to CSV.
divval.py trace2csv ./divval.myapp.1234.trace.bin -o trace.csv
```

The presets are illustrative starting points. They are not models of any
particular core. `--csv` writes the distributions as
`model,expr,group,latency,count,fraction`, which you can use directly as an
empirical latency distribution in a simulator, overall or per op/width.

## Design notes

* **Instrumentation.** Before each divide, inline code stores
  `{Xn, Xm, site}` into a per-thread buffer, using two drreg scratch registers
  and no flags. When the buffer is full (8192 records), a clean call folds the
  records into the aggregates. So the cost is a handful of instructions per
  divide, plus one clean call per 8192 divides.
* **Operand registers.** The operand registers are kept out of the scratch
  set. The client handles the zero register, DR's stolen register (`x28`),
  and stale drreg lazy restores through `drreg_get_app_value()`.
* **SVE.** SVE `UDIV`/`SDIV` share the opcodes but take a governing
  predicate and Z registers. The client skips them.
* **Threads.** The client is written for single-threaded workloads, as
  requested, but it is safe with multiple threads. Each thread has its own
  buffer, and the aggregates are protected by a lock. In the trace, records
  from different threads are interleaved in 8192-record chunks, and there is
  no thread id.
* **Site identity.** A site is identified by `(pc, op, width)`. If generated
  code reuses an address for a different divide with the same op and width,
  the two share a site.

## Testing

`test/divtest.c` executes divides with known operands. These include corner
cases (W forms with garbage in the upper half of the register, `x28`, `xzr`
and `wzr`, the same register as both operands, `INT_MIN / -1`, division by
zero) and 200k random divides, enough to wrap the buffer many times.
`test/check.py` compares the trace with the sequence that divtest reports, and
checks that the histogram and per-site counts agree with the trace:

```sh
aarch64-linux-gnu-gcc -O2 -o divtest test/divtest.c
drrun -c libdivval.so -trace -- ./divtest expected.csv
python3 test/check.py ./divval.divtest.<pid> expected.csv ./divval.divtest.<pid>.trace.bin
```

On an x86-64 host you can run the AArch64 build under QEMU user mode:
`qemu-aarch64 -L /usr/aarch64-linux-gnu <dr>/bin64/drrun -xarch_root /usr/aarch64-linux-gnu -c ...`.
