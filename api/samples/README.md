This directory contains
[sample DynamoRIO clients](http://dynamorio.org/API_samples.html),
which can be directly
used for analysis or extended with additional functionality. Details about
each client can be found in the source code comments at the top of each file.

The summaries below provide a high-level overview of each sample to help users identify relevant starting points; implementation details remain documented in the source code comments.


## Sample Client Overview

### [MF_moduledb.c](MF_moduledb.c)

**What it does**
Applies a module-based policy to security violations using a configuration file and supports runtime policy reloads.

**Why this sample exists**
Shows how to integrate security-event callbacks with external configuration and nudges.

**DynamoRIO concepts demonstrated**
- Security violation events
- Client options parsing
- Process nudges
- Thread-safe global data

**Typical use cases**
- Prototyping allow/deny policies for control-flow violations
- Testing policy reload behavior without restarting a process
- Building module-aware security rules

**How to extend this sample**
- Support richer policy formats
- Add per-module decision logging
- Track per-thread violation statistics

### [bbbuf.c](bbbuf.c)

**What it does**
Records basic block start addresses into a per-thread circular buffer using fast inline instrumentation.

**Why this sample exists**
Demonstrates low-overhead per-thread logging with the `drx_buf` extension.

**DynamoRIO concepts demonstrated**
- TLS fields
- Basic block instrumentation
- `drx_buf` fast circular buffers

**Typical use cases**
- Hot path profiling based on basic block history
- Lightweight execution tracing for debugging

**How to extend this sample**
- Periodically flush buffers to files
- Add timestamps or thread IDs to entries
- Track counts instead of raw addresses

### [bbcount.c](bbcount.c)

**What it does**
Counts dynamic executions of basic blocks using an inline global counter.

**Why this sample exists**
Illustrates efficient counter updates and flag-liveness checks.

**DynamoRIO concepts demonstrated**
- Basic block instrumentation
- Inline counter updates with `drx`
- Flag liveness queries via `drreg`

**Typical use cases**
- Measuring overall execution volume
- Comparing workloads by basic block activity

**How to extend this sample**
- Maintain per-thread counters
- Emit counts per module
- Track block-specific hotness

### [bbsize.c](bbsize.c)

**What it does**
Computes statistics on basic block sizes at runtime.

**Why this sample exists**
Shows how to collect aggregate statistics while preserving floating-point state.

**DynamoRIO concepts demonstrated**
- Analysis callbacks
- Floating-point state preservation
- Shared statistics with synchronization

**Typical use cases**
- Characterizing code layout behavior
- Comparing compiler output across binaries

**How to extend this sample**
- Track a histogram of block sizes
- Emit per-module size statistics
- Log statistics to a file

### [cbr.c](cbr.c)

**What it does**
Instruments conditional branches and removes instrumentation as branch outcomes are observed.

**Why this sample exists**
Demonstrates dynamic re-instrumentation to reduce overhead after collecting control-flow edges.

**DynamoRIO concepts demonstrated**
- Fragment flushing and re-instrumentation
- Conditional branch analysis
- Custom metadata tracking

**Typical use cases**
- Building dynamic control-flow graphs
- Reducing overhead after edge discovery

**How to extend this sample**
- Record edge frequencies
- Persist discovered edges to a file
- Track indirect branches as well

### [cbrtrace.c](cbrtrace.c)

**What it does**
Logs conditional branch addresses, targets, and taken direction to per-thread files.

**Why this sample exists**
Shows how to instrument conditional branches and write per-thread logs.

**DynamoRIO concepts demonstrated**
- Conditional branch instrumentation
- Thread-local storage
- Per-thread file output

**Typical use cases**
- Debugging branch behavior
- Building branch outcome traces

**How to extend this sample**
- Aggregate counts instead of raw logs
- Add timestamps or thread IDs
- Filter by module or address range

### [countcalls.c](countcalls.c)

**What it does**
Counts direct calls, indirect calls, and returns during execution.

**Why this sample exists**
Illustrates per-thread data structures and efficient inline counting.

**DynamoRIO concepts demonstrated**
- TLS-based per-thread data
- Instruction classification
- Inline counters

**Typical use cases**
- Profiling call/return behavior
- Comparing indirect call frequency across inputs

**How to extend this sample**
- Emit per-thread summaries
- Track call targets for indirect calls
- Count tail calls separately

### [div.c](div.c)

**What it does**
Counts dynamic division instructions whose divisor is a power of two.

**Why this sample exists**
Demonstrates clean calls that read operand values at runtime.

**DynamoRIO concepts demonstrated**
- Clean calls
- Operand value extraction
- Thread-safe counters

**Typical use cases**
- Identifying optimization opportunities
- Finding expensive divisions in hot code

**How to extend this sample**
- Track per-instruction locations
- Separate counts by opcode variant
- Record divisor distributions

### [empty.c](empty.c)

**What it does**
Registers a minimal client that performs no instrumentation.

**Why this sample exists**
Provides the smallest possible client skeleton for onboarding.

**DynamoRIO concepts demonstrated**
- Client initialization
- Exit event handling

**Typical use cases**
- Starting point for new clients
- Verifying build and load pipelines

**How to extend this sample**
- Add a basic block callback
- Add logging on thread start/exit

### [hot_bbcount.c](hot_bbcount.c)

**What it does**
Counts executions of hot basic blocks by switching instrumentation once a threshold is reached.

**Why this sample exists**
Shows how to use `drbbdup` to provide distinct instrumentation for cold vs. hot code.

**DynamoRIO concepts demonstrated**
- Basic block duplication (`drbbdup`)
- Clean calls vs. inline updates
- Runtime case selection

**Typical use cases**
- Hot path profiling with reduced overhead
- Tiered instrumentation strategies

**How to extend this sample**
- Use multiple hotness tiers
- Log hot block addresses to a file
- Add per-thread hot counters

### [inc2add.c](inc2add.c)

**What it does**
Rewrites increment and decrement instructions into equivalent add/sub operations at runtime.

**Why this sample exists**
Demonstrates safe instruction replacement driven by runtime conditions.

**DynamoRIO concepts demonstrated**
- App2app transformations
- Instruction replacement
- Liveness checks

**Typical use cases**
- Exploring micro-architecture-specific optimizations
- Testing transformation pipelines

**How to extend this sample**
- Add more rewrite rules
- Gate transformations by module or function
- Collect before/after opcode statistics

### [inline.c](inline.c)

**What it does**
Builds custom traces that inline callees into traces.

**Why this sample exists**
Illustrates advanced trace control and custom trace construction.

**DynamoRIO concepts demonstrated**
- Custom trace control
- Trace head tracking
- Fragment lifecycle events

**Typical use cases**
- Dynamic inlining experiments
- Trace-level performance research

**How to extend this sample**
- Inline only selected callees
- Add heuristics for size thresholds
- Record inline success rates

### [instrace_simple.c](instrace_simple.c)

**What it does**
Records a per-thread instruction trace to a text file.

**Why this sample exists**
Provides a simple, architecture-independent instruction tracing baseline.

**DynamoRIO concepts demonstrated**
- Per-thread buffers
- Clean calls
- Raw TLS access

**Typical use cases**
- Building simple instruction traces
- Validating instrumentation correctness

**How to extend this sample**
- Switch to binary output
- Add module-relative addressing
- Include thread IDs in the log

### [instrace_x86.c](instrace_x86.c)

**What it does**
Captures a dynamic instruction trace with a per-thread buffer and writes it to files, optimized for x86.

**Why this sample exists**
Shows how to reduce overhead with a local code cache and a lean clean-call path.

**DynamoRIO concepts demonstrated**
- Local code cache generation
- Lean clean calls
- Per-thread buffering

**Typical use cases**
- High-performance instruction tracing
- Producing binary traces for offline analysis

**How to extend this sample**
- Add module-relative address mapping
- Include timestamps or thread IDs
- Make buffer sizing configurable

### [instrcalls.c](instrcalls.c)

**What it does**
Logs direct calls, indirect calls, and returns with target information to per-thread files.

**Why this sample exists**
Demonstrates detailed call/return tracing with optional symbolization.

**DynamoRIO concepts demonstrated**
- Call and return instrumentation
- Per-thread logging
- Optional symbol lookup

**Typical use cases**
- Building call flow traces
- Investigating indirect call behavior

**How to extend this sample**
- Aggregate counts by function
- Filter by module or address range
- Emit binary logs for scalability

### [memtrace_simple.c](memtrace_simple.c)

**What it does**
Records memory reference information to a text file using per-thread buffers.

**Why this sample exists**
Provides a straightforward, architecture-independent memory tracing example.

**DynamoRIO concepts demonstrated**
- Memory operand analysis
- `drutil` helpers for address/size
- Clean calls and TLS buffers

**Typical use cases**
- Studying memory access patterns
- Validating memory reference instrumentation

**How to extend this sample**
- Switch to binary output
- Add instruction-level aggregation
- Filter references by size or type

### [memtrace_x86.c](memtrace_x86.c)

**What it does**
Captures memory references with an optimized x86 path and writes per-thread logs.

**Why this sample exists**
Shows how to combine address expansion with a local code cache and lean clean calls for performance.

**DynamoRIO concepts demonstrated**
- `drutil` string and scatter/gather expansion
- Local code cache generation
- Per-thread buffers

**Typical use cases**
- High-performance memory tracing
- Generating binary traces for offline analysis

**How to extend this sample**
- Include per-thread timestamps
- Add module-relative addressing
- Track only reads or only writes

### [memval_simple.c](memval_simple.c)

**What it does**
Records memory write addresses along with the written values into per-thread logs.

**Why this sample exists**
Demonstrates post-write instrumentation and multi-buffer logging with `drx_buf`.

**DynamoRIO concepts demonstrated**
- Post-instruction instrumentation
- `drx_buf` trace and circular buffers
- Memory operand address calculation

**Typical use cases**
- Tracking data writes for debugging
- Building lightweight memory value traces

**How to extend this sample**
- Add read value logging
- Switch to binary logging for speed
- Track write origins per module

### [modxfer.c](modxfer.c)

**What it does**
Counts dynamic instruction executions and indirect control transfers between modules.

**Why this sample exists**
Shows module tracking, per-module accounting, and cross-module transfer analysis.

**DynamoRIO concepts demonstrated**
- Module load/unload events
- Indirect branch instrumentation
- Aggregated logging

**Typical use cases**
- Studying inter-module call patterns
- Identifying frequent cross-module transfers

**How to extend this sample**
- Include direct cross-module branches
- Emit per-thread transfer matrices
- Track transfers for selected modules only

### [modxfer_app2lib.c](modxfer_app2lib.c)

**What it does**
Separates instruction counts for the main application versus libraries and records transfers between them.

**Why this sample exists**
Demonstrates module-specific instrumentation and optimized clean calls.

**DynamoRIO concepts demonstrated**
- Module range checks
- Clean call optimization
- Per-basic-block analysis data

**Typical use cases**
- Measuring time spent in app vs. libraries
- Detecting app-to-lib transition frequency

**How to extend this sample**
- Track per-library counts
- Include returns as separate transfer types
- Log transition addresses

### [opcodes.c](opcodes.c)

**What it does**
Counts dynamic instruction executions by opcode and reports the top counts.

**Why this sample exists**
Shows how to build opcode-level execution profiles with low overhead.

**DynamoRIO concepts demonstrated**
- Basic block instrumentation
- Inline counter updates with `drx`
- ISA mode awareness

**Typical use cases**
- Opcode mix profiling
- Comparing dynamic behavior across workloads

**How to extend this sample**
- Write full opcode histograms to a file
- Add per-thread or per-module breakdowns
- Track opcode counts per basic block

### [prefetch.c](prefetch.c)

**What it does**
Removes prefetch instructions on Intel CPUs to avoid illegal instruction faults.

**Why this sample exists**
Demonstrates app2app transformations for compatibility fixes.

**DynamoRIO concepts demonstrated**
- App2app instruction removal
- CPU vendor detection
- Thread-safe counting

**Typical use cases**
- Running AMD-optimized binaries on Intel CPUs
- Testing instruction stream sanitization

**How to extend this sample**
- Replace prefetch with no-ops instead of removal
- Track per-module removal counts
- Add support for other incompatible opcodes

### [signal.c](signal.c)

**What it does**
Monitors application signals and optionally alters delivery behavior.

**Why this sample exists**
Shows how to register signal events and manipulate signal handling.

**DynamoRIO concepts demonstrated**
- Signal event callbacks
- Signal suppression and redirection
- Atomic counters

**Typical use cases**
- Observing crash signals in a controlled way
- Prototyping signal-based fault handling

**How to extend this sample**
- Log signal details per thread
- Filter signals by module address
- Implement custom recovery logic

### [ssljack.c](ssljack.c)

**What it does**
Wraps SSL/TLS read and write functions and logs decrypted data to per-context files.

**Why this sample exists**
Demonstrates function wrapping with `drwrap` on dynamically loaded modules.

**DynamoRIO concepts demonstrated**
- Module load instrumentation
- Function wrapping (`drwrap`)
- Per-context logging

**Typical use cases**
- Inspecting decrypted SSL/TLS traffic
- Debugging application-level crypto usage

**How to extend this sample**
- Add filters by process or module
- Log metadata such as sizes and timestamps
- Support additional TLS libraries

### [statecmp.c](statecmp.c)

**What it does**
Uses state comparison checks to detect instrumentation-induced state mismatches.

**Why this sample exists**
Shows how to enable and react to `drstatecmp` mismatch detection.

**DynamoRIO concepts demonstrated**
- `drstatecmp` integration
- Instrumentation correctness checking
- Custom error callbacks

**Typical use cases**
- Validating new instrumentation passes
- Debugging subtle state clobbers

**How to extend this sample**
- Log mismatches to a file
- Gate checks by module or thread
- Add more mismatch scenarios

### [stats.c](stats.c)

**What it does**
Exports instruction, floating-point, and syscall statistics via shared memory on Windows.

**Why this sample exists**
Demonstrates cross-process statistics sharing and a stable shared memory layout for a viewer.

**DynamoRIO concepts demonstrated**
- Shared memory usage
- Inline counters with `drx`
- Windows-specific API interactions

**Typical use cases**
- Live statistics dashboards
- Comparing runs without heavy logging

**How to extend this sample**
- Add more counters
- Export per-thread statistics
- Provide a custom viewer format

### [strace.c](strace.c)

**What it does**
Prints the name and result of every system call using `drsyscall`.

**Why this sample exists**
Provides a minimal example of system call tracing with structured syscall metadata.

**DynamoRIO concepts demonstrated**
- Syscall event callbacks
- `drsyscall` integration
- Result decoding

**Typical use cases**
- Debugging unexpected syscalls
- Building basic syscall traces

**How to extend this sample**
- Log syscall arguments
- Filter by syscall type
- Write to a structured log format

### [syscall.c](syscall.c)

**What it does**
Monitors system calls and modifies a target write syscall behavior while counting total syscalls.

**Why this sample exists**
Shows cross-platform syscall interception and thread-context-local data management.

**DynamoRIO concepts demonstrated**
- Syscall interception
- Thread-context-local storage
- Platform-specific syscall handling

**Typical use cases**
- Auditing syscall activity
- Prototyping syscall rewriting

**How to extend this sample**
- Track per-thread syscall counts
- Add syscall argument logging
- Support more syscall rewrites

### [tracedump.c](tracedump.c)

**What it does**
Disassembles and prints trace dump files created by DynamoRIO’s trace dump facility.

**Why this sample exists**
Demonstrates the standalone API for offline inspection of cached code.

**DynamoRIO concepts demonstrated**
- Standalone disassembly
- Trace dump file parsing
- Code cache layout interpretation

**Typical use cases**
- Inspecting trace dumps offline
- Debugging code cache behavior

**How to extend this sample**
- Emit machine-readable output
- Filter for specific trace IDs
- Support batch processing of multiple files

### [utils.c](utils.c)

Note: This file provides shared utilities used by multiple samples rather than a standalone client.


**What it does**
Provides logging and file utility helpers used by multiple samples.

**Why this sample exists**
Centralizes common log-file creation and stream handling routines.

**DynamoRIO concepts demonstrated**
- Client path discovery
- Log file creation helpers

**Typical use cases**
- Reusing logging utilities across sample clients
- Standardizing output file naming

**How to extend this sample**
- Add structured logging formats
- Support configurable output directories
- Add rotation or size limits

### [wrap.c](wrap.c)

**What it does**
Wraps allocation routines to track maximum allocation size and occasionally simulate allocation failure.

**Why this sample exists**
Demonstrates function wrapping and post-call return value modification with `drwrap`.

**DynamoRIO concepts demonstrated**
- Module load callbacks
- Function wrapping (`drwrap`)
- Return value modification

**Typical use cases**
- Testing error handling paths
- Tracking allocation patterns

**How to extend this sample**
- Log allocation sizes per thread
- Add wrappers for additional allocators
- Make failure injection configurable
