/* **********************************************************
 * Copyright (c) 2015-2025 Google, Inc.  All rights reserved.
 * **********************************************************/

/*
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * * Redistributions of source code must retain the above copyright notice,
 *   this list of conditions and the following disclaimer.
 *
 * * Redistributions in binary form must reproduce the above copyright notice,
 *   this list of conditions and the following disclaimer in the documentation
 *   and/or other materials provided with the distribution.
 *
 * * Neither the name of Google, Inc. nor the names of its contributors may be
 *   used to endorse or promote products derived from this software without
 *   specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL VMWARE, INC. OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

/* shared options for both the frontend and the client */

#include "options.h"

#include <cstdint>
#include <limits>
#include <string>

#include "dr_api.h" // For IF_X86_ELSE.
#include "droption.h"

namespace dynamorio {
namespace drmemtrace {

using ::dynamorio::droption::bytesize_t;
using ::dynamorio::droption::DROPTION_FLAG_ACCUMULATE;
using ::dynamorio::droption::DROPTION_FLAG_INTERNAL;
using ::dynamorio::droption::DROPTION_FLAG_SWEEP;
using ::dynamorio::droption::DROPTION_SCOPE_ALL;
using ::dynamorio::droption::DROPTION_SCOPE_CLIENT;
using ::dynamorio::droption::DROPTION_SCOPE_FRONTEND;
using ::dynamorio::droption::droption_t;

droption_t<bool> op_offline(
    DROPTION_SCOPE_ALL, "offline", false, "Store trace files for offline analysis",
    "By default, traces are processed online, sent over a pipe to a simulator.  "
    "If this option is enabled, trace data is instead written to files in -outdir "
    "for later offline analysis.  No simulator is executed.");

droption_t<std::string> op_ipc_name(
    DROPTION_SCOPE_ALL, "ipc_name", "drcachesimpipe", "Name of named pipe",
    "For online tracing and simulation (the default, unless -offline is requested), "
    "specifies the name of the named pipe used to communicate between the target "
    "application processes and the caching device simulator.  On Linux this can include "
    "an absolute path (if it doesn't, a default temp directory "
    "will be used).  A unique name must be chosen "
    "for each instance of the simulator being run at any one time.  On Windows, the name "
    "is limited to 247 characters.");

droption_t<std::string> op_outdir(
    DROPTION_SCOPE_ALL, "outdir", ".", "Target directory for offline trace files",
    "For the offline analysis mode (when -offline is requested), specifies the path "
    "to a directory where per-thread trace files will be written.  The contents of this "
    "directory are internal to the tool.  Do not alter, add, or delete files here.");

droption_t<std::string> op_subdir_prefix(
    DROPTION_SCOPE_ALL, "subdir_prefix", "drmemtrace",
    "Prefix for output subdir for offline traces",
    "For the offline analysis mode (when -offline is requested), specifies the prefix "
    "for the name of the sub-directory where per-thread trace files will be written. "
    "The sub-directory is created inside -outdir and has the form "
    "'prefix.app-name.pid.id.dir'.");

droption_t<std::string> op_indir(
    DROPTION_SCOPE_ALL, "indir", "", "Input directory of offline trace files",
    "After a trace file is produced via -offline into -outdir, it can be passed to the "
    "simulator via this flag pointing at the subdirectory created in -outdir. "
    "The -offline tracing produces raw data files which are converted into final "
    "trace files on the first execution with -indir.  The raw files can also be manually "
    "converted using the drraw2trace tool.  Legacy single trace files with all threads "
    "interleaved into one are not supported with this option: use -infile instead.  "
    "The contents of this directory are internal to the tool.  Do not alter, add, or "
    "delete files here.");

droption_t<std::string> op_multi_indir(
    DROPTION_SCOPE_ALL, "multi_indir", "", "Colon-separated list of input directories",
    "Identical to -indir except this takes a colon-separated list of directories "
    "for offline analysis in core-sharded mode.  Multiple inputs are not supported in "
    "any other mode; they are not supported currently with thread or shard limits; they "
    "are not supported for interval analysis, for replaying as-traced, or for "
    "core-sharded-on-disk inputs. Skipping is applied to every input thread. Auxiliary "
    "files such as the traced function list (see -funclist_file), the v2p file (see "
    "-v2p_file), schedule files for the invariant checker, and the module file (for "
    "legacy traces without encodings) are only auto-located in the first directory "
    "listed.  Additional support may be added in the future. See -indir for further "
    "information on each input directory.");

droption_t<std::string> op_infile(
    DROPTION_SCOPE_ALL, "infile", "", "Single offline trace input file",
    "Directs the framework to use a single trace file.  This could be a legacy "
    "all-software-threads-interleaved-into-one trace file, a core-sharded single "
    "hardware thread file mixing multiple software threads, or a single software "
    "thread selected from a directory (though in that case it is better to use "
    "-only_thread, -only_threads, or -only_shards).  This method of input does "
    "not support any features that require auxiliary metadata files. "
    "Passing '-' will read from stdin as plain or gzip-compressed data.");

droption_t<int> op_jobs(
    DROPTION_SCOPE_ALL, "jobs", -1, "Number of parallel jobs",
    "By default, both post-processing of offline raw trace files and analysis of trace "
    "files is parallelized.  This option controls the number of concurrent jobs.  0 "
    "disables concurrency and uses a single thread to perform all operations.  A "
    "negative value sets the job count to the number of hardware threads, "
    "with a cap of 16.  This is ignored for -core_sharded where -cores sets the "
    "parallelism.");

droption_t<std::string> op_module_file(
    DROPTION_SCOPE_ALL, "module_file", "", "Path to modules.log for opcode_mix tool",
    "The opcode_mix tool needs the modules.log file (generated by the offline "
    "post-processing step in the raw/ subdirectory) in addition to the trace file. "
    "If the file is named modules.log and is in the same directory as the trace file, "
    "or a raw/ subdirectory below the trace file, this parameter can be omitted.");

droption_t<std::string> op_alt_module_dir(
    DROPTION_SCOPE_FRONTEND, "alt_module_dir", "", "Alternate module search directory",
    "Specifies a directory containing libraries referenced in -module_file for "
    "analysis tools, or in the raw modules file for post-prcoessing of offline "
    "raw trace files.  This directory takes precedence over the recorded path.");

droption_t<bytesize_t> op_chunk_instr_count(
    DROPTION_SCOPE_FRONTEND, "chunk_instr_count", bytesize_t(10 * 1000 * 1000U),
    // We do not support tiny chunks.  We do not support disabling chunks with a 0
    // value, to simplify testing: although we're still having to support generating
    // non-zip files for !HAS_ZLIB/!HAS_ZIP!
    bytesize_t(1000),
#ifdef X64
    bytesize_t(1ULL << 63),
#else
    // We store this value in a marker which can only hold a pointer-sized value and
    // thus is limited to 4G.
    // XXX i#5634: This happens to timestamps too: what we should do is use multiple
    // markers (need up to 3) to support 64-bit values in 32-bit builds.
    bytesize_t(UINT_MAX),
#endif
    "Chunk instruction count",
    "Specifies the size in instructions of the chunks into which a trace output file "
    "is split inside a zipfile.  This is the granularity of a fast seek. "
    "This only applies when generating .zip-format traces; when built without "
    "support for writing .zip files, this option is ignored. "
    "For 32-bit this cannot exceed 4G.");

droption_t<bool> op_instr_encodings(
    DROPTION_SCOPE_CLIENT, "instr_encodings", false,
    "Whether to include encodings for online tools",
    "By default instruction encodings are not sent to online tools, to reduce "
    "overhead.  (Offline tools have them added by default.)");

droption_t<std::string> op_funclist_file(
    DROPTION_SCOPE_ALL, "funclist_file", "",
    "Path to function map file for func_view tool",
    "The func_view tool needs the mapping from function name to identifier that was "
    "recorded during offline tracing.  This data is stored in its own separate "
    "file in the raw/ subdirectory. If the file is named funclist.log and is in the same "
    "directory as the trace file, or a raw/ subdirectory below the trace file, this "
    "parameter can be omitted.");

droption_t<unsigned int> op_num_cores(
    DROPTION_SCOPE_FRONTEND, "cores", 4, "Number of cores",
    "Specifies the number of cores to simulate.  For -core_sharded, each core executes "
    "in parallel in its own worker thread and -jobs is ignored.");

droption_t<unsigned int> op_line_size(
    DROPTION_SCOPE_FRONTEND, "line_size", 64, "Cache line size",
    "Specifies the cache line size, which is assumed to be identical for L1 and L2 "
    "caches.  Must be at least 4 and a power of 2.");

droption_t<bytesize_t>
    op_L1I_size(DROPTION_SCOPE_FRONTEND, "L1I_size", 32 * 1024U,
                "Instruction cache total size",
                "Specifies the total size of each L1 instruction cache. "
                "L1I_size/L1I_assoc must be a power of 2 and a multiple of line_size.");

droption_t<bytesize_t>
    op_L1D_size(DROPTION_SCOPE_FRONTEND, "L1D_size", bytesize_t(32 * 1024),
                "Data cache total size",
                "Specifies the total size of each L1 data cache. "
                "L1D_size/L1D_assoc must be a power of 2 and a multiple of line_size.");

droption_t<unsigned int>
    op_L1I_assoc(DROPTION_SCOPE_FRONTEND, "L1I_assoc", 8,
                 "Instruction cache associativity",
                 "Specifies the associativity of each L1 instruction cache. "
                 "L1I_size/L1I_assoc must be a power of 2 and a multiple of line_size.");

droption_t<unsigned int>
    op_L1D_assoc(DROPTION_SCOPE_FRONTEND, "L1D_assoc", 8, "Data cache associativity",
                 "Specifies the associativity of each L1 data cache. "
                 "L1D_size/L1D_assoc must be a power of 2 and a multiple of line_size.");

droption_t<bytesize_t> op_LL_size(DROPTION_SCOPE_FRONTEND, "LL_size", 8 * 1024 * 1024,
                                  "Last-level cache total size",
                                  "Specifies the total size of the unified last-level "
                                  "(L2) cache. "
                                  "LL_size/LL_assoc must be a power of 2 "
                                  "and a multiple of line_size.");

droption_t<unsigned int>
    op_LL_assoc(DROPTION_SCOPE_FRONTEND, "LL_assoc", 16, "Last-level cache associativity",
                "Specifies the associativity of the unified last-level (L2) cache. "
                "LL_size/LL_assoc must be a power of 2 and a multiple of line_size.");

droption_t<std::string> op_LL_miss_file(
    DROPTION_SCOPE_FRONTEND, "LL_miss_file", "",
    "Path for dumping LLC misses or prefetching hints",
    "If non-empty, when running the cache simulator, requests that "
    "every last-level cache miss be written to a file at the specified path. Each miss "
    "is written in text format as a <process id, program counter, address> tuple. If "
    "this tool is linked with zlib, the file is written in gzip-compressed format. If "
    "non-empty, when running the cache miss analyzer, requests that prefetching hints "
    "based on the miss analysis be written to the specified file. Each hint is written "
    "in text format as a <program counter, stride, locality level> tuple.");

droption_t<bool> op_L0_filter_deprecated(
    DROPTION_SCOPE_CLIENT, "L0_filter", false,
    "Filter out first-level instruction and data cache hits during tracing",
    "DEPRECATED: Use the -L0I_filter and -L0D_filter options instead.");

droption_t<bool> op_L0I_filter(
    DROPTION_SCOPE_CLIENT, "L0I_filter", false,
    "Filter out first-level instruction cache hits during tracing",
    "Filters out instruction hits in a 'zero-level' cache during tracing itself, "
    "shrinking the final trace to only contain instructions that miss in this initial "
    "cache.  This cache is direct-mapped with size equal to L0I_size.  It uses virtual "
    "addresses regardless of -use_physical. The dynamic (pre-filtered) per-thread "
    "instruction count is tracked and supplied via a "
    "#dynamorio::drmemtrace::TRACE_MARKER_TYPE_INSTRUCTION_COUNT marker at thread "
    "buffer boundaries and at thread exit.");

droption_t<bool> op_L0D_filter(
    DROPTION_SCOPE_CLIENT, "L0D_filter", false,
    "Filter out first-level data cache hits during tracing",
    "Filters out data hits in a 'zero-level' cache during tracing itself, shrinking the "
    "final trace to only contain data accesses that miss in this initial cache.  This "
    "cache is direct-mapped with size equal to L0D_size.  It uses virtual addresses "
    "regardless of -use_physical. ");

droption_t<bytesize_t> op_L0I_size(
    DROPTION_SCOPE_CLIENT, "L0I_size", 32 * 1024U,
    "If -L0I_filter, filter out instruction hits during tracing",
    "Specifies the size of the 'zero-level' instruction cache for L0I_filter.  "
    "Must be a power of 2 and a multiple of line_size, unless it is set to 0, "
    "which disables instruction fetch entries from appearing in the trace.");

droption_t<bytesize_t> op_L0D_size(
    DROPTION_SCOPE_CLIENT, "L0D_size", 32 * 1024U,
    "If -L0D_filter, filter out data hits during tracing",
    "Specifies the size of the 'zero-level' data cache for L0D_filter.  "
    "Must be a power of 2 and a multiple of line_size, unless it is set to 0, "
    "which disables data entries from appearing in the trace.");

droption_t<bool> op_instr_only_trace(
    DROPTION_SCOPE_CLIENT, "instr_only_trace", false,
    "Include only instruction fetch entries in trace",
    "If -instr_only_trace, only instruction fetch entries are included in the "
    "trace and data entries are omitted.");

droption_t<bool> op_coherence(
    DROPTION_SCOPE_FRONTEND, "coherence", false, "Model coherence for private caches",
    "Writes to cache lines will invalidate other private caches that hold that line.");

droption_t<bool> op_use_physical(
    DROPTION_SCOPE_ALL, "use_physical", false, "Use physical addresses if possible",
    "If available, metadata with virtual-to-physical-address translation information "
    "is added to the trace.  This is not possible from user mode on all platforms.  "
    "The regular trace entries remain virtual, with a pair of markers of "
    "types #dynamorio::drmemtrace::TRACE_MARKER_TYPE_PHYSICAL_ADDRESS and "
    "#dynamorio::drmemtrace::TRACE_MARKER_TYPE_VIRTUAL_ADDRESS "
    "inserted at some prior point for each new or changed page mapping to show the "
    "corresponding physical addresses.  If translation fails, a "
    "#dynamorio::drmemtrace::TRACE_MARKER_TYPE_PHYSICAL_ADDRESS_NOT_AVAILABLE is "
    "inserted. This option may incur significant overhead "
    "both for the physical translation and as it requires disabling optimizations."
    "For -offline, this option must be passed to both the tracer (to insert the "
    "markers) and the simulator (to use the markers).");

droption_t<unsigned int> op_virt2phys_freq(
    DROPTION_SCOPE_CLIENT, "virt2phys_freq", 0, "Frequency of physical mapping refresh",
    "This option only applies if -use_physical is enabled.  The virtual to physical "
    "mapping is cached for performance reasons, yet the underlying mapping can change "
    "without notice.  This option controls the frequency with which the cached value is "
    "ignored in order to re-access the actual mapping and ensure accurate results.  "
    "The units are the number of memory accesses per forced access.  A value of 0 "
    "uses the cached values for the entire application execution.");

droption_t<std::string> op_v2p_file(
    DROPTION_SCOPE_FRONTEND, "v2p_file", "", "Path to v2p.textproto for simulator tools",
    "The " TLB " simulator can use v2p.textproto to translate virtual addresses to "
    "physical ones during offline analysis. If the file is named v2p.textproto and is in "
    "the same directory as the trace file, or a raw/ subdirectory below the trace file, "
    "this parameter can be omitted. This option overwrites both -page_size and the page "
    "size marker in the trace (if present) with the page size in v2p.textproto. The "
    "option -use_physical (in offline mode) must be set to use the v2p.textproto "
    "mapping. Note that -use_physical does not need to be set during tracing.");

droption_t<bool> op_cpu_scheduling(
    DROPTION_SCOPE_CLIENT, "cpu_scheduling", false,
    "Map threads to cores matching recorded cpu execution",
    "By default for online analysis, the simulator schedules threads to simulated cores "
    "in a static "
    "round-robin fashion.  This option causes the scheduler to instead use the recorded "
    "cpu that each thread executed on (at a granularity of the trace buffer size) "
    "for scheduling, mapping traced cpu's to cores and running each segment of each "
    "thread on the core that owns the recorded cpu for that segment. "
    "This option is not supported with -core_serial; use "
    "-cpu_schedule_file with -core_serial instead.  For offline analysis, the "
    "recommendation is to not recreate the as-traced schedule (as it is not accurate due "
    "to overhead) and instead use a dynamic schedule via -core_serial.  If only "
    "core-sharded-preferring tools are enabled (e.g., " CPU_CACHE ", " TLB
    ", " SCHEDULE_STATS
    "), -core_serial is automatically turned on for offline analysis.");

droption_t<bytesize_t> op_max_trace_size(
    DROPTION_SCOPE_CLIENT, "max_trace_size", 0,
    "Cap on the raw trace size for each thread",
    "If non-zero, this sets a maximum size on the amount of raw trace data gathered "
    "for each thread.  This is not an exact limit: it may be exceeded by the size "
    "of one internal buffer.  Once reached, instrumentation continues for that thread, "
    "but no further data is recorded.");

droption_t<bytesize_t> op_max_global_trace_refs(
    DROPTION_SCOPE_CLIENT, "max_global_trace_refs", 0,
    "Cap on the total references of any type traced",
    "If non-zero, this sets a maximum size on the amount of trace entry references "
    "(of any type: instructions, loads, stores, markers, etc.) recorded. "
    "Once reached, instrumented execution continues, but no further data is recorded. "
    "This is similar to -exit_after_tracing but without terminating the process."
    "The reference count is approximate.");

droption_t<bool> op_align_endpoints(
    // XXX i#2039,i#5686: Remove this altogether once more time passes and we
    // are no longer worried about any robustness issues with drbbdup where we might
    // want to disable this to see where a new problem is coming from.
    DROPTION_SCOPE_CLIENT, "align_endpoints", true,
    "Nop tracing when partially attached or detached",
    "When using attach/detach to trace a burst, the attach and detach processes are "
    "staggered, with the set of threads producing trace data incrementally growing or "
    "shrinking.  This results in uneven thread activity at the start and end of the "
    "burst.  If this option is enabled, tracing is nop-ed until fully attached to "
    "all threads and is nop-ed as soon as detach starts, eliminating the unevenness. "
    "This also allows omitting threads that did nothing during the burst.");

droption_t<bool> op_memdump_on_window(
    // TODO i#7508: Add Windows support. raw2trace fails with "Non-module instructions
    // found with no encoding information.". This occurs on Windows when capturing memory
    // dumps at the start of a new tracing window.
    DROPTION_SCOPE_CLIENT, "memdump_on_window", false,
    "Capture a memory dump when a tracing window opens",
    "Capture a memory dump upon the initiation of tracing triggered by "
    "-trace_after_instrs, -trace_instr_intervals_file, or -retrace_every_instrs. "
    "If -retrace_every_instrs is also enabled, a memory dump will be captured for "
    "each individual tracing window. This is only supported on X64 Linux.");

droption_t<bytesize_t> op_trace_after_instrs(
    DROPTION_SCOPE_CLIENT, "trace_after_instrs", 0,
    "Do not start tracing until N instructions",
    "If non-zero, this causes tracing to be suppressed until this many dynamic "
    "instruction executions are observed from the start of the application. "
    "At that point, regular tracing is put into place. "
    "The threshold should be considered approximate, especially for larger values. "
    "Use -trace_for_instrs, -max_trace_size, or -max_global_trace_refs to set a limit "
    "on the subsequent trace length.  Use -retrace_every_instrs to trace repeatedly.");

droption_t<bytesize_t> op_trace_for_instrs(
    DROPTION_SCOPE_CLIENT, "trace_for_instrs", 0,
    "After tracing N instructions, stop tracing, but continue executing.",
    "If non-zero, this stops recording a trace after the specified number of "
    "instructions are traced.  Unlike -exit_after_tracing, which kills the "
    "application (and counts data as well as instructions), the application "
    "continues executing.  This can be combined with -retrace_every_instrs. "
    "The actual trace period may vary slightly from this number due to optimizations "
    "that reduce the overhead of instruction counting.");

droption_t<bytesize_t> op_retrace_every_instrs(
    DROPTION_SCOPE_CLIENT, "retrace_every_instrs", 0,
    "Trace for -trace_for_instrs, execute this many, and repeat.",
    "This option augments -trace_for_instrs.  After tracing concludes, this option "
    "causes non-traced instructions to be counted and after the number specified by "
    "this option, tracing will start up again for the -trace_for_instrs duration.  This "
    "process repeats itself.  This can be combined with -trace_after_instrs for an "
    "initial period of non-tracing.  Each tracing window is delimited by "
    "TRACE_MARKER_TYPE_WINDOW_ID markers.  For -offline traces, each window is placed "
    "into its own separate set of output files, unless -no_split_windows is set.");

droption_t<std::string> op_trace_instr_intervals_file(
    DROPTION_SCOPE_CLIENT, "trace_instr_intervals_file", "",
    "File containing instruction intervals to trace.",
    "File containing instruction intervals to trace in csv format.  "
    "Intervals are specified as a <start, duration> pair per line. Example in: "
    "clients/drcachesim/tests/instr_intervals_example.csv");

droption_t<bool> op_split_windows(
    DROPTION_SCOPE_CLIENT, "split_windows", true,
    "Whether -retrace_every_instrs should write separate files",
    "By default, offline traces in separate windows from -retrace_every_instrs are "
    "written to a different set of files for each window.  If this option is disabled, "
    "all windows are concatenated into a single trace, separated by "
    "TRACE_MARKER_TYPE_WINDOW_ID markers.");

droption_t<bytesize_t> op_exit_after_tracing(
    DROPTION_SCOPE_CLIENT, "exit_after_tracing", 0,
    "Exit the process after tracing N references",
    "If non-zero, after tracing the specified number of references, the process is "
    "exited with an exit code of 0.  The reference count is approximate. "
    "Use -max_global_trace_refs instead to avoid terminating the process.");

droption_t<std::string> op_raw_compress(
    DROPTION_SCOPE_CLIENT, "raw_compress",
#if defined(HAS_LZ4) && !defined(DRMEMTRACE_STATIC)
    // lz4 performs best but has no allocator parameterization so cannot be static.
    "lz4",
#elif defined(HAS_SNAPPY) && !defined(DRMEMTRACE_STATIC)
    // snappy_nocrc also has no allocator parameterization so cannot be static.
    "snappy_nocrc",
#else
    // All other choices are slowdowns for an SSD so we turn them off by default.
    "none",
#endif
    "Raw compression: \"snappy\",\"snappy_nocrc\",\"gzip\",\"zlib\",\"lz4\",\"none\"",
    "Specifies the compression type to use for raw offline files: \"snappy\", "
    "\"snappy_nocrc\" (snappy without checksums, which is much faster), \"gzip\", "
    "\"zlib\", \"lz4\", or \"none\".  Whether this reduces overhead depends on the "
    "storage type: "
    "for an SSD, zlib and gzip typically add overhead and would only be used if space is "
    "at a premium; snappy_nocrc and lz4 are nearly always performance wins.");

droption_t<std::string> op_trace_compress(
    DROPTION_SCOPE_FRONTEND, "compress", DEFAULT_TRACE_COMPRESSION_TYPE,
    "Trace compression: \"zip\",\"gzip\",\"zlib\",\"lz4\",\"none\"",
    "Specifies the compression type to use for trace files: \"zip\", "
    "\"gzip\", \"zlib\", \"lz4\", or \"none\". "
    "In most cases where fast skipping by instruction count is not needed "
    "lz4 compression generally improves performance and is recommended. "
    "When it comes to storage types, the impact on overhead varies: "
    "for SSDs, zip and gzip often increase overhead and should only be chosen "
    "if space is limited.");

droption_t<bool> op_online_instr_types(
    DROPTION_SCOPE_CLIENT, "online_instr_types", false,
    "Whether online traces should distinguish instr types",
    "By default, offline traces include some information on the types of instructions, "
    "branches in particular.  For online traces, this comes at a performance cost, so "
    "it is turned off by default.");

droption_t<std::string> op_replace_policy(
    DROPTION_SCOPE_FRONTEND, "replace_policy", REPLACE_POLICY_LRU,
    "Cache replacement policy (LRU, LFU, FIFO)",
    "Specifies the replacement policy for "
    "caches. Supported policies: LRU (Least Recently Used), LFU (Least Frequently Used), "
    "FIFO (First-In-First-Out).");

droption_t<std::string> op_data_prefetcher(
    DROPTION_SCOPE_FRONTEND, "data_prefetcher", PREFETCH_POLICY_NEXTLINE,
    "Hardware data prefetcher policy (nextline, none)",
    "Specifies the hardware data "
    "prefetcher policy.  The currently supported policies are 'nextline' (fetch the "
    "subsequent cache line) and 'none' (disables hardware prefetching).  The prefetcher "
    "is located between the L1D and LL caches.");

droption_t<bytesize_t> op_page_size(DROPTION_SCOPE_FRONTEND, "page_size",
                                    bytesize_t(4 * 1024), "Virtual/physical page size",
                                    "Specifies the virtual/physical page size.");

droption_t<unsigned int> op_TLB_L1I_entries(
    DROPTION_SCOPE_FRONTEND, "TLB_L1I_entries", 32,
    "Number of entries in instruction TLB",
    "Specifies the number of entries in each L1 instruction TLB.  Must be a power of 2.");

droption_t<unsigned int> op_TLB_L1D_entries(
    DROPTION_SCOPE_FRONTEND, "TLB_L1D_entries", 32, "Number of entries in data TLB",
    "Specifies the number of entries in each L1 data TLB.  Must be a power of 2.");

droption_t<unsigned int> op_TLB_L1I_assoc(
    DROPTION_SCOPE_FRONTEND, "TLB_L1I_assoc", 32, "Instruction TLB associativity",
    "Specifies the associativity of each L1 instruction TLB.  Must be a power of 2.");

droption_t<unsigned int> op_TLB_L1D_assoc(
    DROPTION_SCOPE_FRONTEND, "TLB_L1D_assoc", 32, "Data TLB associativity",
    "Specifies the associativity of each L1 data TLB.  Must be a power of 2.");

droption_t<unsigned int> op_TLB_L2_entries(
    DROPTION_SCOPE_FRONTEND, "TLB_L2_entries", 1024, "Number of entries in L2 TLB",
    "Specifies the number of entries in each unified L2 TLB.  Must be a power of 2.");

droption_t<unsigned int> op_TLB_L2_assoc(
    DROPTION_SCOPE_FRONTEND, "TLB_L2_assoc", 4, "L2 TLB associativity",
    "Specifies the associativity of each unified L2 TLB.  Must be a power of 2.");

droption_t<std::string>
    op_TLB_replace_policy(DROPTION_SCOPE_FRONTEND, "TLB_replace_policy",
                          REPLACE_POLICY_LFU, "TLB replacement policy",
                          "Specifies the replacement policy for TLBs. "
                          "Supported policies: " REPLACE_POLICY_LFU
                          " (Least Frequently Used), " REPLACE_POLICY_BIT_PLRU
                          " (Pseudo Least Recently Used) " REPLACE_POLICY_LRU
                          " (Least Recently Used) " REPLACE_POLICY_FIFO
                          " (First-In-First-Out)");

droption_t<std::string>
    op_tool(DROPTION_SCOPE_FRONTEND,
            std::vector<std::string>({ "tool", "simulator_type" }), CPU_CACHE,
            "Specifies which trace analysis tool(s) to run.  Multiple tools "
            "can be specified, separated by a colon (\":\").",
            "Predefined types: " CPU_CACHE ", " MISS_ANALYZER ", " TLB ", " REUSE_DIST
            ", " REUSE_TIME ", " HISTOGRAM ", " BASIC_COUNTS ", " INVARIANT_CHECKER
            ", " SCHEDULE_STATS ", or " RECORD_FILTER ". The " RECORD_FILTER
            " tool cannot be combined with the others "
            "as it operates on raw disk records. "
            "To invoke an external tool: specify its name as identified by a "
            "name.drcachesim config file in the DR tools directory.");

droption_t<unsigned int> op_verbose(DROPTION_SCOPE_ALL, "verbose", 0, 0, 64,
                                    "Verbosity level",
                                    "Verbosity level for notifications.");

droption_t<bool>
    op_show_func_trace(DROPTION_SCOPE_FRONTEND, "show_func_trace", true,
                       "Show every traced call in the func_trace tool",
                       "In the func_trace tool, this controls whether every traced call "
                       "is shown or instead only aggregate statistics are shown.");
droption_t<bool> op_test_mode(DROPTION_SCOPE_ALL, "test_mode", false, "Run sanity tests",
                              "Run extra analyses for sanity checks on the trace.");
droption_t<std::string> op_test_mode_name(
    DROPTION_SCOPE_ALL, "test_mode_name", "", "Run custom sanity tests",
    "Run extra analyses for specific sanity checks by name on the trace.");
droption_t<bool> op_disable_optimizations(
    DROPTION_SCOPE_ALL, "disable_optimizations", false,
    "Disable offline trace optimizations for testing",
    "Disables various optimizations where information is omitted from offline trace "
    "recording when it can be reconstructed during post-processing.  This is meant for "
    "testing purposes.");

droption_t<std::string> op_dr_root(DROPTION_SCOPE_FRONTEND, "dr", "",
                                   "Path to DynamoRIO root directory",
                                   "Specifies the path of the DynamoRIO root directory.");

droption_t<bool> op_dr_debug(
    DROPTION_SCOPE_FRONTEND, "dr_debug", false, "Use DynamoRIO debug build",
    "Requests use of the debug build of DynamoRIO rather than the release build.");

droption_t<std::string> op_dr_ops(DROPTION_SCOPE_FRONTEND, "dr_ops", "",
                                  "Options to pass to DynamoRIO",
                                  "Specifies the options to pass to DynamoRIO.");

droption_t<std::string> op_tracer(DROPTION_SCOPE_FRONTEND, "tracer", "",
                                  "Path to the tracer",
                                  "The full path to the tracer library.");

droption_t<std::string> op_tracer_alt(DROPTION_SCOPE_FRONTEND, "tracer_alt", "",
                                      "Path to the alternate-bitwidth tracer",
                                      "The full path to the tracer library for the other "
                                      "bitwidth, for use on child processes with a "
                                      "different bitwidth from their parent.  If empty, "
                                      "such child processes will die with fatal errors.");

droption_t<std::string> op_tracer_ops(
    DROPTION_SCOPE_FRONTEND, "tracer_ops",
    DROPTION_FLAG_SWEEP | DROPTION_FLAG_ACCUMULATE | DROPTION_FLAG_INTERNAL, "",
    "(For internal use: sweeps up tracer options)",
    "This is an internal option that sweeps up other options to pass to the tracer.");

droption_t<bytesize_t> op_interval_microseconds(
    DROPTION_SCOPE_FRONTEND, "interval_microseconds", 0,
    "Enable periodic heartbeats for intervals of given microseconds in the trace.",
    "Desired length of each trace interval, defined in microseconds of trace time. "
    "Trace intervals are measured using the TRACE_MARKER_TYPE_TIMESTAMP marker values. "
    "If set, analysis tools receive a callback at the end of each interval, and one "
    "at the end of trace analysis to print the whole-trace interval results.");

droption_t<bytesize_t> op_interval_instr_count(
    DROPTION_SCOPE_FRONTEND, "interval_instr_count", 0,
    "Enable periodic heartbeats for intervals of given per-shard instr count. ",
    "Desired length of each trace interval, defined in instr count of each shard. "
    "With -parallel, this does not support whole trace intervals, only per-shard "
    "intervals. If set, analysis tools receive a callback at the end of each interval, "
    "and separate callbacks per shard at the end of trace analysis to print each "
    "shard's interval results.");

droption_t<int>
    op_only_thread(DROPTION_SCOPE_FRONTEND, "only_thread", 0,
                   "Only analyze this thread (0 means all)",
                   "Limits analyis to the single "
                   "thread with the given identifier.  0 enables all threads.  "
                   "Applies only to -indir, not to -infile.  "
                   "Cannot be combined with -only_threads or -only_shards.");

droption_t<std::string>
    op_only_threads(DROPTION_SCOPE_FRONTEND, "only_threads", "",
                    "Only analyze these comma-separated threads",
                    "Limits analyis to the list of comma-separated thread ids.  "
                    "Applies only to -indir, not to -infile.  "
                    "Cannot be combined with -only_thread or -only_shards.");
droption_t<std::string>
    op_only_shards(DROPTION_SCOPE_FRONTEND, "only_shards", "",
                   "Only analyze these comma-separated shard ordinals",
                   "Limits analyis to the list of comma-separated shard ordinals. "
                   "A shard is typically an input thread but might be a core for "
                   "core-sharded-on-disk traces.  The ordinal is 0-based and indexes "
                   "into the sorted order of input filenames.  "
                   "Applies only to -indir, not to -infile.  "
                   "Cannot be combined with -only_thread or -only_threads.");

droption_t<bytesize_t> op_skip_instrs(
    DROPTION_SCOPE_FRONTEND, "skip_instrs", 0, "Number of instructions to skip",
    "Specifies the number of instructions to skip in the beginning of the trace "
    "analysis.  For serial iteration, this number is "
    "computed just once across the interleaving sequence of all threads; for parallel "
    "iteration, each thread skips this many instructions (see -skip_to_timestamp for "
    "an alternative which does align all threads).  When built with zipfile "
    "support, this skipping is optimized and large instruction counts can be quickly "
    "skipped; this is not the case for -skip_records or -skip_refs. "
    "This will skip over top-level "
    "metadata records (such as #dynamorio::drmemtrace::TRACE_MARKER_TYPE_VERSION, "
    "#dynamorio::drmemtrace::TRACE_MARKER_TYPE_FILETYPE, "
    "#dynamorio::drmemtrace::TRACE_MARKER_TYPE_PAGE_SIZE, and "
    "#dynamorio::drmemtrace::TRACE_MARKER_TYPE_CACHE_LINE_SIZE) and so those "
    "records will not appear to analysis tools; however, their contents can be obtained "
    "from #dynamorio::drmemtrace::memtrace_stream_t API accessors.  "
    "Synthetic records, such as dynamically injected system call or context switch "
    "sequences, are not counted at all.");

droption_t<bytesize_t> op_skip_records(
    DROPTION_SCOPE_FRONTEND, "skip_records", 0, "Number of records to skip",
    "Specifies the number of records to skip in the beginning of the trace "
    "analysis.  For serial iteration, this number is "
    "computed just once across the interleaving sequence of all threads; for parallel "
    "iteration, each worker skips this many records (see -skip_to_timestamp for "
    "an alternative which aligns all threads).  This skipping is not as fast as "
    "-skip_instrs.  This will skip over top-level "
    "metadata records (such as #dynamorio::drmemtrace::TRACE_MARKER_TYPE_VERSION, "
    "#dynamorio::drmemtrace::TRACE_MARKER_TYPE_FILETYPE, "
    "#dynamorio::drmemtrace::TRACE_MARKER_TYPE_PAGE_SIZE, and "
    "#dynamorio::drmemtrace::TRACE_MARKER_TYPE_CACHE_LINE_SIZE) and so those "
    "records will not appear to analysis tools; however, their contents can be obtained "
    "from #dynamorio::drmemtrace::memtrace_stream_t API accessors.  "
    "Synthetic records, such as dynamically injected system call or context switch "
    "sequences, are not counted at all.");

droption_t<bytesize_t> op_skip_refs(
    DROPTION_SCOPE_FRONTEND, "skip_refs", 0,
    "Number of non-markers to skip in certain tools",
    "This option is honored by certain tools such as the cache and TLB simulators.  "
    "It causes them to ignore the specified count of non-marker (i.e., actual address "
    "reference ('ref') records) at the "
    "start of the trace and only start processing after that count is reached.  Since "
    "the framework is still iterating over those records and it is the tool who is "
    "ignoring them, this skipping may be slow for large skip values; consider "
    "-skip_instrs for a faster method of skipping inside the framework itself.  "
    "To skip markers also, use -skip_records.");

droption_t<uint64_t> op_skip_to_timestamp(
    DROPTION_SCOPE_FRONTEND, "skip_to_timestamp", 0, "Timestamp to start at",
    "Specifies a timestamp to start at, skipping over prior records in the trace. "
    "This is cross-cutting across all threads.  If the target timestamp is not "
    "present as a timestamp marker, interpolation is used to approximate the "
    "target location in each thread.  Only one of this and -skip_instrs can be "
    "specified.  Requires -cpu_schedule_file to also be specified as a schedule file "
    "is required to translate the timestamp into per-thread instruction ordinals."
    "When built with zipfile support, this skipping is optimized and large "
    "instruction counts can be quickly skipped.  This will skip over top-level "
    "metadata records (such as #dynamorio::drmemtrace::TRACE_MARKER_TYPE_VERSION, "
    "#dynamorio::drmemtrace::TRACE_MARKER_TYPE_FILETYPE, "
    "#dynamorio::drmemtrace::TRACE_MARKER_TYPE_PAGE_SIZE, and "
    "#dynamorio::drmemtrace::TRACE_MARKER_TYPE_CACHE_LINE_SIZE) and so those "
    "records will not appear to analysis tools; however, their contants can be obtained "
    "from #dynamorio::drmemtrace::memtrace_stream_t API accessors.");

droption_t<bytesize_t> op_L0_filter_until_instrs(
    DROPTION_SCOPE_CLIENT, "L0_filter_until_instrs", 0,
    "Number of instructions for warmup trace",
    "Specifies the number of instructions to run in warmup mode. This instruction count "
    "is per-thread. In warmup mode, we "
    "filter accesses through the -L0{D,I}_filter caches. If neither -L0D_filter nor "
    "-L0I_filter are specified then both are assumed to be true. The size of these can "
    "be specified using -L0{D,I}_size. The filter instructions come after the "
    "-trace_after_instrs count and before the full trace. This is intended to be "
    "used together with other trace options (e.g., -trace_for_instrs, "
    "-exit_after_tracing, -max_trace_size etc.) but with the difference that a filter "
    "trace is also collected. The filter trace and full trace are stored in a single "
    "file separated by a TRACE_MARKER_TYPE_FILTER_ENDPOINT marker. When used with "
    "windows (i.e., -retrace_every_instrs), each window contains a filter trace and a "
    "full trace. Therefore TRACE_MARKER_TYPE_WINDOW_ID markers indicate start of "
    "filtered records.");

droption_t<bytesize_t> op_warmup_refs(
    DROPTION_SCOPE_FRONTEND, "warmup_refs", 0, "Number of non-markers to warm caches up",
    "This option is honored by certain tools such as the cache and TLB simulators. "
    "It causes them to not start analysis/simulation until this many non-marker records "
    "(i.e., actual memory reference ('ref') records) are seen.  "
    "If -skip_refs is specified, the warmup records start after "
    "the skipped ones end. This flag is incompatible with warmup_fraction.");

droption_t<double> op_warmup_fraction(
    DROPTION_SCOPE_FRONTEND, "warmup_fraction", 0.0, 0.0, 1.0,
    "Fraction of last level cache blocks to be loaded as warm up",
    "Specifies the fraction of last level cache blocks to be loaded such that the "
    "cache is considered to be warmed up before simulation. The warmup fraction "
    "is computed after the skipped references and before simulated references. "
    "This flag is incompatible with warmup_refs.");

droption_t<bytesize_t> op_sim_refs(
    DROPTION_SCOPE_FRONTEND, "sim_refs", bytesize_t(1ULL << 63),
    "Number of non-markers records to analyze",
    "This option is honored by certain tools such as the cache and TLB simulators.  "
    "It causes them to only analyze this many non-marker (i.e., actual memory reference "
    "('ref') records) and then exit. "
    "If -skip_refs is specified, the analyzed records start after the skipped ones end; "
    "similarly, if -warmup_refs or -warmup_fraction is specified, the warmup records "
    "come prior to the -sim_refs records.  Use -exit_after_records for a similar feature "
    "that works on all tools (but does not work with -warmup_*, -sim_refs, or "
    "-skip_refs).");

droption_t<bytesize_t> op_exit_after_records(
    DROPTION_SCOPE_FRONTEND, "exit_after_records", bytesize_t(1ULL << 63),
    "Limits analyzers to this many records",
    "Causes trace analyzers to only analyze this many records and then exit.  If "
    "instructions are skipped (-skip_instrs), that happens "
    "first before record counting starts here.  This is similar to -sim_refs, though "
    "it is implemented in the framework and so applies to all tools.  This option is not "
    "compatible with -sim_refs, -skip_refs, -warmup_refs, or -warmup_fraction. "
    "For traces with multiple shards, each shard separately stops when it reaches this "
    "count within the shard.");

droption_t<std::string>
    op_view_syntax(DROPTION_SCOPE_FRONTEND, "view_syntax", "att/arm/dr/riscv",
                   "Syntax to use for disassembly.",
                   "Specifies the syntax to use when viewing disassembled offline traces."
                   // TODO i#4382: Add aarch64 syntax support.
                   " The option can be set to one of \"att\" (AT&T style), \"intel\""
                   " (Intel style), \"dr\" (DynamoRIO's native style with all implicit"
                   " operands listed), \"arm\" (32-bit ARM style), and \"riscv\" (RISC-V "
                   "style). An invalid specification falls back to the default, which is "
                   "\"att\" for x86, \"arm\" for ARM (32-bit), \"dr\" for AArch64, "
                   "and \"riscv\" for RISC-V.");

droption_t<std::string>
    op_config_file(DROPTION_SCOPE_FRONTEND, "config_file", "",
                   "Cache hierarchy configuration file",
                   "The full path to the cache hierarchy configuration file.");

droption_t<bool> op_add_noise_generator(
    DROPTION_SCOPE_FRONTEND, "add_noise_generator", false, "Add noise generation.",
    "Adds synthetic trace records produced by a noise generator as another input "
    "workload to the scheduler. These synthetic records are interleaved by the scheduler "
    "with the target trace(s) records. Currently it only adds a single-process, "
    "single thread noise generator, but that may change in the future.");

// XXX: if we separate histogram + reuse_distance we should move this with them.
droption_t<unsigned int>
    op_report_top(DROPTION_SCOPE_FRONTEND, "report_top", 10,
                  "Number of top results to be reported",
                  "Specifies the number of top results to be reported.");

// XXX: if we separate histogram + reuse_distance we should move these with them.
droption_t<unsigned int> op_reuse_distance_threshold(
    DROPTION_SCOPE_FRONTEND, "reuse_distance_threshold", 100,
    "The reuse distance threshold for reporting the distant repeated references.",
    "Specifies the reuse distance threshold for reporting the distant repeated "
    "references. "
    "A reference is a distant repeated reference if the distance to the previous "
    "reference"
    " on the same cache line exceeds the threshold.");
droption_t<bool> op_reuse_distance_histogram(
    DROPTION_SCOPE_FRONTEND, "reuse_distance_histogram", false,
    "Print the entire reuse distance histogram.",
    "By default only the mean, median, and standard deviation of the reuse distances "
    "are reported.  This option prints out the full histogram of reuse distances.");
droption_t<unsigned int> op_reuse_skip_dist(
    DROPTION_SCOPE_FRONTEND, "reuse_skip_dist", 500,
    "For performance tuning: distance between skip nodes.",
    "Specifies the distance between nodes in the skip list.  For optimal performance, "
    "set this to a value close to the estimated average reuse distance of the dataset.");
droption_t<unsigned int> op_reuse_distance_limit(
    DROPTION_SCOPE_FRONTEND, "reuse_distance_limit", 0,
    "If nonzero, restricts distance tracking to the specified maximum distance.",
    "Specifies the maximum length of the access history list used for distance "
    "calculation.  Setting this limit can significantly improve performance "
    "and reduce memory consumption for very long traces.");
droption_t<bool> op_reuse_verify_skip(
    DROPTION_SCOPE_FRONTEND, "reuse_verify_skip", false,
    "Use full list walks to verify the skip list results.",
    "Verifies every skip list-calculated reuse distance with a full list walk. "
    "This incurs significant additional overhead.  This option is only available "
    "in debug builds.");
droption_t<double> op_reuse_histogram_bin_multiplier(
    DROPTION_SCOPE_FRONTEND, "reuse_histogram_bin_multiplier", 1.00,
    "When reporting histograms, grow bins geometrically by this multiplier.",
    "The first histogram bin has a size of 1, meaning it contains the count for "
    "one distance.  Each subsequent bin size is increased by this multiplier. "
    "For multipliers >1.0, this results in geometric growth of bin sizes, with "
    "multiple distance values being reported for each bin. For large traces, "
    "a value of 1.05 works well to limit the output to a reasonable number of "
    "bins.  Note that this option only affects the printing of histograms via "
    "the -reuse_distance_histogram option; the raw histogram data is always "
    "collected at full precision.");

#define OP_RECORD_FUNC_ITEM_SEP "&"
// XXX i#3048: replace function return address with function callstack
droption_t<std::string> op_record_function(
    DROPTION_SCOPE_CLIENT, "record_function", DROPTION_FLAG_ACCUMULATE,
    OP_RECORD_FUNC_ITEM_SEP, "",
    "Record invocations trace for the specified function(s).",
    "Record invocations trace for the specified function(s) in the option"
    " value. Default value is empty. The value should fit this format:"
    " function_name|func_args_num"
    " (e.g., -record_function \"memset|3\") with an optional suffix \"|noret\""
    " (e.g., -record_function \"free|1|noret\"). The trace would contain"
    " information for each function invocation's return address, function argument"
    " value(s), and (unless \"|noret\" is specified) function return value."
    " (If multiple requested functions map to the same address and differ in whether"
    " \"noret\" was specified, the attribute from the first one requested will be used."
    " If they differ in the number of args, the minimum value will be used.)"
    " We only record pointer-sized arguments and"
    " return values. The trace identifies which function is involved"
    " via a numeric ID entry prior to each set of value entries."
    " The mapping from numeric ID to library-qualified symbolic name is recorded"
    " during tracing in a file \"funclist.log\" whose format is described by the "
    " drmemtrace_get_funclist_path() function's documentation."
    " If the target function is in the dynamic symbol table, then the function_name"
    " should be a mangled name (e.g. \"_Znwm\" for \"operator new\", \"_ZdlPv\" for"
    " \"operator delete\"). Otherwise, the function_name should be a demangled name."
    " Recording multiple functions can be achieved by using the separator"
    " \"" OP_RECORD_FUNC_ITEM_SEP
    "\" (e.g., -record_function \"memset|3" OP_RECORD_FUNC_ITEM_SEP "memcpy|3\"), or"
    " specifying multiple -record_function options (e.g., -record_function"
    " \"memset|3\" -record_function \"memcpy|3\")."
    " Note that the provided function name should be unique, and not collide with"
    " existing heap functions (see -record_heap_value) if -record_heap"
    " option is enabled.");
droption_t<bool> op_record_heap(
    DROPTION_SCOPE_CLIENT, "record_heap", false,
    "Enable recording a trace for the defined heap functions.",
    "It is a convenience option to enable recording a trace for the defined heap"
    " functions in -record_heap_value. Specifying this option is equivalent to"
    " -record_function [heap_functions], where [heap_functions] is"
    " the value in -record_heap_value.");
droption_t<std::string> op_record_heap_value(
    // There are other, more obscure variants, like pvalloc, valloc, memalign,
    // posix_memalign, independent_calloc, plus malloc_zone_* on Mac and
    // *_impl versions on Windows.  We ignore these for now.
    DROPTION_SCOPE_CLIENT, "record_heap_value", DROPTION_FLAG_ACCUMULATE,
    OP_RECORD_FUNC_ITEM_SEP,
    "malloc|1" OP_RECORD_FUNC_ITEM_SEP "free|1|noret" OP_RECORD_FUNC_ITEM_SEP
    "realloc|2" OP_RECORD_FUNC_ITEM_SEP "calloc|2" OP_RECORD_FUNC_ITEM_SEP
    "tc_malloc|1" OP_RECORD_FUNC_ITEM_SEP "tc_free|1|noret" OP_RECORD_FUNC_ITEM_SEP
    "tc_realloc|2" OP_RECORD_FUNC_ITEM_SEP "tc_calloc|2" OP_RECORD_FUNC_ITEM_SEP
    "__libc_malloc|1" OP_RECORD_FUNC_ITEM_SEP
    "__libc_free|1|noret" OP_RECORD_FUNC_ITEM_SEP
    "__libc_realloc|2" OP_RECORD_FUNC_ITEM_SEP "__libc_calloc|2"
#ifdef UNIX
    // i#3048: We only have Itanium ABI manglings for now so we disable for MSVC.
    // XXX: This is getting quite long.  I would change the option to point at
    // a file, except that does not work well with some third-party uses.
    // Another option would be to support wildcards and give up on extra args like
    // alignment and nothrow: so we'd do "_Zn*|1&_Zd*|1|noret".
    OP_RECORD_FUNC_ITEM_SEP "_Znwm|1" OP_RECORD_FUNC_ITEM_SEP
    "_ZnwmRKSt9nothrow_t|2" OP_RECORD_FUNC_ITEM_SEP
    "_ZnwmSt11align_val_t|2" OP_RECORD_FUNC_ITEM_SEP
    "_ZnwmSt11align_val_tRKSt9nothrow_t|3" OP_RECORD_FUNC_ITEM_SEP
    "_ZnwmPv|2" OP_RECORD_FUNC_ITEM_SEP "_Znam|1" OP_RECORD_FUNC_ITEM_SEP
    "_ZnamRKSt9nothrow_t|2" OP_RECORD_FUNC_ITEM_SEP
    "_ZnamSt11align_val_t|2" OP_RECORD_FUNC_ITEM_SEP
    "_ZnamSt11align_val_tRKSt9nothrow_t|3" OP_RECORD_FUNC_ITEM_SEP
    "_ZnamPv|2" OP_RECORD_FUNC_ITEM_SEP "_ZdlPv|1|noret" OP_RECORD_FUNC_ITEM_SEP
    "_ZdlPvRKSt9nothrow_t|2|noret" OP_RECORD_FUNC_ITEM_SEP
    "_ZdlPvSt11align_val_t|2|noret" OP_RECORD_FUNC_ITEM_SEP
    "_ZdlPvSt11align_val_tRKSt9nothrow_t|3|noret" OP_RECORD_FUNC_ITEM_SEP
    "_ZdlPvm|2|noret" OP_RECORD_FUNC_ITEM_SEP
    "_ZdlPvmSt11align_val_t|3|noret" OP_RECORD_FUNC_ITEM_SEP
    "_ZdlPvS_|2|noret" OP_RECORD_FUNC_ITEM_SEP "_ZdaPv|1|noret" OP_RECORD_FUNC_ITEM_SEP
    "_ZdaPvRKSt9nothrow_t|2|noret" OP_RECORD_FUNC_ITEM_SEP
    "_ZdaPvSt11align_val_t|2|noret" OP_RECORD_FUNC_ITEM_SEP
    "_ZdaPvSt11align_val_tRKSt9nothrow_t|3|noret" OP_RECORD_FUNC_ITEM_SEP
    "_ZdaPvm|2|noret" OP_RECORD_FUNC_ITEM_SEP
    "_ZdaPvmSt11align_val_t|3|noret" OP_RECORD_FUNC_ITEM_SEP "_ZdaPvS_|2|noret"
#endif
    ,
    "Functions recorded by -record_heap",
    "Functions recorded by -record_heap. The option value should fit the same"
    " format required by -record_function. These functions will not"
    " be traced unless -record_heap is specified.");
droption_t<bool> op_record_dynsym_only(
    DROPTION_SCOPE_CLIENT, "record_dynsym_only", false,
    "Only look in .dynsym for -record_function and -record_heap.",
    "Symbol lookup can be expensive for large applications and libraries.  This option "
    " causes the symbol lookup for -record_function and -record_heap to look in the "
    " dynamic symbol table *only*.");
droption_t<bool> op_record_replace_retaddr(
    DROPTION_SCOPE_CLIENT, "record_replace_retaddr", false,
    "Wrap by replacing retaddr for -record_function and -record_heap.",
    "Function wrapping can be expensive for large concurrent applications.  This option "
    "causes the post-function control point to be located using return address "
    "replacement, which has lower overhead, but runs the risk of breaking an "
    "application that examines or changes its own return addresses in the recorded "
    "functions.");
droption_t<std::string> op_record_syscall(
    DROPTION_SCOPE_CLIENT, "record_syscall", DROPTION_FLAG_ACCUMULATE,
    OP_RECORD_FUNC_ITEM_SEP, "", "Record parameters for the specified syscall number(s).",
    "Record the parameters and success of the specified system call number(s)."
    " The option value should fit this format:"
    " sycsall_number|parameter_number"
    " E.g., -record_syscall \"2|2\" will record SYS_open's 2 parameters and whether"
    " successful (1 for success or 0 for failure, in a function return value record)"
    " for x86 Linux.  SYS_futex is recorded by default on Linux and this option's value"
    " adds to futex rather than replacing it (setting futex to 0 parameters disables)."
    " The trace identifies which syscall owns each set of parameter and return value"
    " records via a numeric ID equal to the syscall number + TRACE_FUNC_ID_SYSCALL_BASE."
    " Recording multiple syscalls can be achieved by using the separator"
    " \"" OP_RECORD_FUNC_ITEM_SEP
    "\" (e.g., -record_syscall \"202|6" OP_RECORD_FUNC_ITEM_SEP "3|1\"), or"
    " specifying multiple -record_syscall options."
    " It is up to the user to ensure the values are correct; a too-large parameter"
    " count may cause tracing to fail with an error mid-run.");
droption_t<unsigned int> op_miss_count_threshold(
    DROPTION_SCOPE_FRONTEND, "miss_count_threshold", 50000,
    "For cache miss analysis: minimum LLC miss count for a load to be eligible for "
    "analysis.",
    "Specifies the minimum number of LLC misses of a load for it to be eligible for "
    "analysis in search of patterns in the miss address stream.");
droption_t<double> op_miss_frac_threshold(
    DROPTION_SCOPE_FRONTEND, "miss_frac_threshold", 0.005,
    "For cache miss analysis: minimum LLC miss fraction for a load to be eligible for "
    "analysis.",
    "Specifies the minimum fraction of LLC misses of a load (from all misses) for it to "
    "be eligible for analysis in search of patterns in the miss address stream.");
droption_t<double> op_confidence_threshold(
    DROPTION_SCOPE_FRONTEND, "confidence_threshold", 0.75,
    "For cache miss analysis: minimum confidence threshold of a pattern to be printed "
    "out.",
    "Specifies the minimum confidence to include a discovered pattern in the output "
    "results. Confidence in a discovered pattern for a load instruction is calculated "
    "as the fraction of the load's misses with the discovered pattern over all the "
    "load's misses.");
droption_t<bool> op_enable_drstatecmp(
    DROPTION_SCOPE_CLIENT, "enable_drstatecmp", false, "Enable the drstatecmp library.",
    "When true, this option enables the drstatecmp library that performs state "
    "comparisons to detect instrumentation-induced bugs due to state clobbering.");

#ifdef BUILD_PT_TRACER
droption_t<bool> op_enable_kernel_tracing(
    DROPTION_SCOPE_ALL, "enable_kernel_tracing", false, "Enable Kernel Intel PT tracing.",
    "By default, offline tracing only records a userspace trace. If this option is "
    "enabled, offline tracing will record each syscall's Kernel PT and write every "
    "syscall's PT and metadata to files in -outdir/kernel.raw/ for later offline "
    "analysis. And this feature is available only on Intel CPUs that support Intel@ "
    "Processor Trace.");
droption_t<bool> op_skip_kcore_dump(
    DROPTION_SCOPE_ALL, "skip_kcore_dump", false,
    "Skip creation of the kcore dump during kernel tracing.",
    "By default, when -enable_kernel_tracing is set, offline tracing will dump kcore "
    "and kallsyms to the raw trace directory, which requires the user to run the target "
    "application with superuser permissions. However, if this option is enabled, we "
    "skip the dump, and since collecting kernel trace data using Intel-PT does not "
    "necessarily need superuser permissions, the target application can be run "
    "as normal. This may be useful if it is not feasible to run the application "
    "with superuser permissions and the user wants to use a different kcore "
    "dump, from a prior trace or created separately.");
droption_t<int> op_kernel_trace_buffer_size_shift(
    DROPTION_SCOPE_ALL, "kernel_trace_buffer_size_shift", 8,
    "Size of the buffer used to collect kernel trace data.",
    "When -enable_kernel_tracing is set, this is used to compute the size of the "
    "buffer used to collect kernel trace data. The size is computed as "
    "(1 << kernel_trace_buffer_size_shift) * page_size. Too large buffers can cause "
    "OOMs on apps with many threads, whereas too small buffers can cause decoding "
    "issues in raw2trace due to dropped trace data.");
#endif

// Core-oriented analysis.
droption_t<bool> op_core_sharded(
    DROPTION_SCOPE_ALL, "core_sharded", false, "Analyze per-core in parallel.",
    "By default, the sharding mode is determined by the preferred shard type of the"
    "tools selected (unless overridden, the default preferred type is thread-sharded). "
    "This option enables core-sharded, overriding tool defaults.  Core-sharded "
    "anlysis schedules the input software threads onto virtual cores "
    "and analyzes each core in parallel.  Thus, each shard consists of pieces from "
    "many software threads.  How the scheduling is performed is controlled by a set "
    "of options with the prefix \"sched_\" along with -cores.  If only "
    "core-sharded-preferring tools are enabled (e.g., " CPU_CACHE ", " TLB
    ", " SCHEDULE_STATS ") and they all support parallel operation, -core_sharded is "
    "automatically turned on for offline analysis.");

droption_t<bool> op_core_serial(
    DROPTION_SCOPE_ALL, "core_serial", false, "Analyze per-core in serial.",
    "In this mode, scheduling is performed just like for -core_sharded. "
    "However, the resulting schedule is acted upon by a single analysis thread"
    "which walks the N cores in lockstep in round robin fashion. "
    "How the scheduling is performed is controlled by a set "
    "of options with the prefix \"sched_\" along with -cores.  If only "
    "core-sharded-preferring tools are enabled (e.g., " CPU_CACHE ", " TLB
    ", " SCHEDULE_STATS ") and not all of them support parallel operation, "
    "-core_serial is automatically turned on for offline analysis.");

droption_t<int64_t>
    // We pick 10 million to match 2 instructions per nanosecond with a 5ms quantum.
    op_sched_quantum(DROPTION_SCOPE_ALL, "sched_quantum", 10 * 1000 * 1000,
                     "Scheduling quantum",
                     "Applies to -core_sharded and -core_serial.  Scheduling quantum in "
                     "instructions, unless -sched_time is set in which case this value "
                     "is the quantum in simulated microseconds (equal to wall-clock "
                     "microseconds multiplied by -sched_time_per_us).");

droption_t<bool>
    op_sched_time(DROPTION_SCOPE_ALL, "sched_time", false,
                  "Whether to use time for the scheduling quantum",
                  "Applies to -core_sharded and -core_serial.  Whether to use wall-clock "
                  "time (multiplied by -sched_time_per_us) for measuring idle time and "
                  "for the scheduling quantum (see -sched_quantum).");

droption_t<bool> op_sched_order_time(DROPTION_SCOPE_ALL, "sched_order_time", true,
                                     "Whether to honor recorded timestamps for ordering",
                                     "Applies to -core_sharded and -core_serial. "
                                     "Whether to honor recorded timestamps for ordering");

droption_t<uint64_t> op_sched_syscall_switch_us(
    DROPTION_SCOPE_ALL, "sched_syscall_switch_us", 30000000,
    "Minimum latency to consider a non-blocking syscall as incurring a context switch.",
    "Minimum latency in timestamp units (us) to consider a non-blocking syscall as "
    "incurring a context switch (see -sched_blocking_switch_us for maybe-blocking "
    "syscalls).  Applies to -core_sharded and -core_serial. ");

droption_t<uint64_t> op_sched_blocking_switch_us(
    DROPTION_SCOPE_ALL, "sched_blocking_switch_us", 500,
    "Minimum latency to consider a maybe-blocking syscall as incurring a context switch.",
    "Minimum latency in timestamp units (us) to consider any syscall that is marked as "
    "maybe-blocking to incur a context switch. Applies to -core_sharded and "
    "-core_serial. ");

droption_t<double> op_sched_block_scale(
    DROPTION_SCOPE_ALL, "sched_block_scale", 0.1, "Input block time scale factor",
    "A system call considered to block (see -sched_blocking_switch_us) will "
    "block in the trace scheduler for an amount of simulator time equal to its "
    "as-traced latency in trace-time microseconds multiplied by this parameter "
    "and by -sched_time_per_us in simulated microseconds, subject to a "
    "maximum of --sched_block_max_us. A higher value here results in blocking "
    "syscalls keeping inputs unscheduled for longer. There is indirect "
    "overhead inflating the as-traced times, so a value below 1 is typical.");

// We have a max to avoid outlier latencies from scaling up to extreme times.  There is
// some inflation in the as-traced latencies and some can be inflated more than others.
// We assume a cap is representative as the outliers
// likely were not part of key dependence chains.  Without a cap the other threads all
// finish and the simulation waits for tens of minutes further for a couple of outliers.
// The cap remains a flag and not a constant as different length traces and different
// speed simulators need different idle time ranges, so we need to be able to tune this
// to achieve desired cpu usage targets.  The default value was selected to avoid unduly
// long idle times with local analyzers; it may need to be increased with more
// heavyweight analyzers/simulators.
// TODO i#6959: Once we have -exit_if_all_unscheduled raise this.
droption_t<uint64_t> op_sched_block_max_us(DROPTION_SCOPE_ALL, "sched_block_max_us", 2500,
                                           "Maximum blocked input time, in microseconds",
                                           "The maximum blocked time, after scaling with "
                                           "-sched_block_scale.");
#ifdef HAS_ZIP
droption_t<std::string> op_record_file(DROPTION_SCOPE_FRONTEND, "record_file", "",
                                       "Path for storing record of schedule",
                                       "Applies to -core_sharded and -core_serial. "
                                       "Path for storing record of schedule.");

droption_t<std::string> op_replay_file(DROPTION_SCOPE_FRONTEND, "replay_file", "",
                                       "Path with stored schedule for replay",
                                       "Applies to -core_sharded and -core_serial. "
                                       "Path with stored schedule for replay.");
droption_t<std::string>
    op_cpu_schedule_file(DROPTION_SCOPE_FRONTEND, "cpu_schedule_file", "",
                         "Path to as-traced schedule for replay or skip-to-timestamp",
                         "Applies to -core_sharded and -core_serial. "
                         "Path with stored as-traced schedule for replay.  If specified "
                         "with a non-zero -skip_to_timestamp, there is no replay "
                         "and instead the file is used for the skip request.");
#endif
droption_t<std::string> op_sched_switch_file(
    DROPTION_SCOPE_FRONTEND, "sched_switch_file", "",
    "Path to file holding kernel context switch sequences",
    "Applies to -core_sharded and -core_serial.  Path to file holding context switch "
    "sequences.  The file can contain multiple sequences each with regular trace headers "
    "and the sequence proper bracketed by TRACE_MARKER_TYPE_CONTEXT_SWITCH_START and "
    "TRACE_MARKER_TYPE_CONTEXT_SWITCH_END markers.");

droption_t<std::string> op_sched_syscall_file(
    DROPTION_SCOPE_FRONTEND, "sched_syscall_file", "",
    "Path to file holding kernel system call sequences",
    "Path to file holding system call sequences.  The file can contain multiple "
    "sequences each with regular trace headers and the sequence proper bracketed by "
    "TRACE_MARKER_TYPE_SYSCALL_TRACE_START and TRACE_MARKER_TYPE_SYSCALL_TRACE_END "
    "markers.");

droption_t<bool> op_sched_randomize(
    DROPTION_SCOPE_FRONTEND, "sched_randomize", false,
    "Pick next inputs randomly on context switches",
    "Applies to -core_sharded and -core_serial.  Disables the normal methods of "
    "choosing the next input based on priority, timestamps (if -sched_order_time is "
    "set), and FIFO order and instead selects the next input randomly. "
    "This is intended for experimental use in sensitivity studies.");

droption_t<bool> op_sched_disable_direct_switches(
    DROPTION_SCOPE_FRONTEND, "sched_disable_direct_switches", false,
    "Ignore direct thread switch requests",
    "Applies to -core_sharded and -core_serial.  Disables switching to the recorded "
    "targets of TRACE_MARKER_TYPE_DIRECT_THREAD_SWITCH system call metadata markers "
    "and causes the associated system call to be treated like any other call with a "
    "switch being determined by latency and the next input in the queue.  The "
    "TRACE_MARKER_TYPE_DIRECT_THREAD_SWITCH markers are not removed from the trace.");

droption_t<bool> op_sched_infinite_timeouts(
    DROPTION_SCOPE_FRONTEND, "sched_infinite_timeouts", false,
    "Whether unscheduled-indefinitely means never scheduled",
    "Applies to -core_sharded and -core_serial.  Determines whether an "
    "unscheduled-indefinitely input really is never scheduled (set to true), or instead "
    "is treated as blocked for the maximum time (scaled by the regular block scale) "
    "(set to false).");

droption_t<double> op_sched_time_units_per_us(
    DROPTION_SCOPE_ALL, "sched_time_units_per_us", 1000.,
    "Time units per simulated microsecond",
    "Time units per simulated microsecond.  The units are either the instruction count "
    "plus the idle count (the default) or if -sched_time is selected wall-clock "
    "microseconds.  This option value scales all of the -sched_*_us values as it "
    "converts time units into the simulated microseconds measured by those options.");

droption_t<uint64_t> op_sched_migration_threshold_us(
    DROPTION_SCOPE_ALL, "sched_migration_threshold_us", 500,
    "Time in simulated microseconds before an input can be migrated across cores",
    "The minimum time in simulated microseconds that must have elapsed since an input "
    "last ran on a core before it can be migrated to another core.");

droption_t<uint64_t> op_sched_rebalance_period_us(
    DROPTION_SCOPE_ALL, "sched_rebalance_period_us", 50000,
    "Period in microseconds at which core run queues are load-balanced",
    "The period in simulated microseconds at which per-core run queues are re-balanced "
    "to redistribute load.");

droption_t<double> op_sched_exit_if_fraction_inputs_left(
    DROPTION_SCOPE_FRONTEND, "sched_exit_if_fraction_inputs_left", 0.1,
    "Exit if non-EOF inputs left are <= this fraction of the total",
    "Applies to -core_sharded and -core_serial.  When an input reaches EOF, if the "
    "number of non-EOF inputs left as a fraction of the original inputs is equal to or "
    "less than this value then the scheduler exits (sets all outputs to EOF) rather than "
    "finishing off the final inputs.  This helps avoid long sequences of idles during "
    "staggered endings with fewer inputs left than cores and only a small fraction of "
    "the total instructions left in those inputs.  Since the remaining instruction "
    "count is not considered (as it is not available), use discretion when raising "
    "this value on uneven inputs.");

droption_t<int> op_sched_max_cores(
    DROPTION_SCOPE_ALL, "sched_max_cores", 0,
    "Limit scheduling to this many peak live cores",
    "If non-zero, only this many live cores can be scheduled at any one time.  "
    "Other cores will remain idle.");

// Schedule_stats options.
droption_t<uint64_t>
    op_schedule_stats_print_every(DROPTION_SCOPE_ALL, "schedule_stats_print_every",
                                  500000, "A letter is printed every N instrs",
                                  "A letter is printed every N instrs or N waits");

droption_t<std::string> op_syscall_template_file(
    DROPTION_SCOPE_FRONTEND, "syscall_template_file", "",
    "Path to the file that contains system call trace templates.",
    "Path to the file that contains system call trace templates. "
    "If set, system call traces will be injected from the file "
    "into the resulting trace. This is still experimental so the template file "
    "format may change without backward compatibility.");

// Record filter options.
droption_t<uint64_t> op_filter_stop_timestamp(
    DROPTION_SCOPE_FRONTEND, "filter_stop_timestamp", 0, 0,
    // Wrap max in parens to work around Visual Studio compiler issues with the
    // max macro (even despite NOMINMAX defined above).
    (std::numeric_limits<uint64_t>::max)(),
    "Timestamp (in us) in the trace when to stop filtering.",
    "Record filtering will be disabled (everything will be output) "
    "when the tool sees a TRACE_MARKER_TYPE_TIMESTAMP marker with "
    "timestamp greater than the specified value.");

droption_t<int> op_filter_cache_size(
    DROPTION_SCOPE_FRONTEND, "filter_cache_size", 0,
    "Enable data cache filter with given size (in bytes).",
    "Enable data cache filter with given size (in bytes), with 64 byte "
    "line size and a direct mapped LRU cache.");

droption_t<std::string>
    op_filter_trace_types(DROPTION_SCOPE_FRONTEND, "filter_trace_types", "",
                          "Comma-separated integers for trace types to remove.",
                          "Comma-separated integers for trace types to remove. "
                          "See trace_type_t for the list of trace entry types.");

droption_t<std::string>
    op_filter_marker_types(DROPTION_SCOPE_FRONTEND, "filter_marker_types", "",
                           "Comma-separated integers for marker types to remove.",
                           "Comma-separated integers for marker types to remove. "
                           "See trace_marker_type_t for the list of marker types.");

/* XXX i#6369: we should partition our options by tool. This one should belong to the
 * record_filter partition. For now we add the filter_ prefix to options that should be
 * used in conjunction with record_filter.
 */
droption_t<bool> op_encodings2regdeps(
    DROPTION_SCOPE_FRONTEND, "filter_encodings2regdeps", false,
    "Enable converting the encoding of instructions to synthetic ISA DR_ISA_REGDEPS.",
    "This option is for -tool " RECORD_FILTER ". When present, it converts "
    "the encoding of instructions from a real ISA to the DR_ISA_REGDEPS synthetic ISA.");

/* XXX i#6369: we should partition our options by tool. This one should belong to the
 * record_filter partition. For now we add the filter_ prefix to options that should be
 * used in conjunction with record_filter.
 */
droption_t<std::string>
    op_filter_func_ids(DROPTION_SCOPE_FRONTEND, "filter_keep_func_ids", "",
                       "Comma-separated integers of function IDs to keep.",
                       "This option is for -tool " RECORD_FILTER ". It preserves "
                       "TRACE_MARKER_TYPE_FUNC_[ID | ARG | RETVAL | RETADDR] markers "
                       "for the listed function IDs and removes those belonging to "
                       "unlisted function IDs.");

droption_t<std::string> op_modify_marker_value(
    DROPTION_SCOPE_FRONTEND, "filter_modify_marker_value", "",
    "Comma-separated pairs of integers representing <TRACE_MARKER_TYPE_, new_value>.",
    "This option is for -tool " RECORD_FILTER ". It modifies the value of all listed "
    "TRACE_MARKER_TYPE_ markers in the trace with their corresponding new_value. "
    "The list must have an even size. Example: -filter_modify_marker_value 3,24,18,2048 "
    "sets all TRACE_MARKER_TYPE_CPU_ID == 3 in the trace to core 24 and "
    "TRACE_MARKER_TYPE_PAGE_SIZE == 18 to 2k.");

droption_t<uint64_t> op_trim_before_timestamp(
    DROPTION_SCOPE_ALL, "trim_before_timestamp", 0, 0,
    (std::numeric_limits<uint64_t>::max)(),
    "Trim records until this timestamp (in us) in the trace.",
    "Removes all records (after headers) before the first TRACE_MARKER_TYPE_TIMESTAMP "
    "marker in the trace with timestamp greater than or equal to the specified value.");

droption_t<uint64_t> op_trim_after_timestamp(
    DROPTION_SCOPE_ALL, "trim_after_timestamp", 0, 0,
    (std::numeric_limits<uint64_t>::max)(),
    "Trim records after this timestamp (in us) in the trace.",
    "Removes all records from the first TRACE_MARKER_TYPE_TIMESTAMP marker with "
    "timestamp larger than the specified value.");

droption_t<uint64_t> op_trim_before_instr(
    DROPTION_SCOPE_ALL, "trim_before_instr", 0, 0, (std::numeric_limits<uint64_t>::max)(),
    "Trim records approximately until this instruction ordinal in the trace.",
    "Removes all records (after headers) before the first TRACE_MARKER_TYPE_TIMESTAMP "
    "marker in the trace that comes after the specified instruction ordinal.");

droption_t<uint64_t> op_trim_after_instr(
    DROPTION_SCOPE_ALL, "trim_after_instr", 0, 0, (std::numeric_limits<uint64_t>::max)(),
    "Trim records approximately after this instruction ordinal in the trace.",
    "Removes all records from the first TRACE_MARKER_TYPE_TIMESTAMP marker in the trace "
    "that comes after the specified instruction ordinal.");

droption_t<bool> op_abort_on_invariant_error(
    DROPTION_SCOPE_ALL, "abort_on_invariant_error", true,
    "Abort invariant checker when a trace invariant error is found.",
    "When set to true, the trace invariant checker analysis tool aborts when a trace "
    "invariant error is found. Otherwise it prints the error and continues. Also, the "
    "total invariant error count is printed at the end; a non-zero error count does not "
    "affect the exit code of the analyzer.");

droption_t<bool> op_pt2ir_best_effort(
    DROPTION_SCOPE_ALL, "pt2ir_best_effort", false,
    "Whether errors encountered during PT trace conversion in pt2ir are ignored.",
    "By default, errors in decoding the kernel syscall PT trace in pt2ir are fatal to "
    "raw2trace. With -pt2ir_best_effort, those errors do not cause failures and their "
    "counts are reported by raw2trace at the end. This may result in a trace where not "
    "all syscalls have a trace, and the ones that do may have some PC discontinuities "
    "due to non-fatal decoding errors (these discontinuities will still be reported by "
    "the invariant checker). When using this option, it is prudent to check raw2trace "
    "stats to confirm that the error counts are within expected bounds (total syscall "
    "traces converted, syscall traces that were dropped from final trace because "
    "conversion failed, syscall traces found to be empty, and non-fatal decode errors "
    "seen in converted syscall traces).");

droption_t<int> op_scale_timers(
    DROPTION_SCOPE_ALL, "scale_timers", 1, 1, (std::numeric_limits<int>::max)(),
    "If >1, inflate application timer periods by this value",
    "If >1, application timer initial durations and periodic durations are "
    "inflated by this scale.  This can help preserve relative timing between timer-based "
    "application work and other application work in the presence of "
    "significant slowdowns from tracing.  Currently only supported on Linux.");
droption_t<int> op_scale_timeouts(
    DROPTION_SCOPE_ALL, "scale_timeouts", 1, 1, (std::numeric_limits<int>::max)(),
    "If >1, inflate syscall timeouts by this value",
    "If >1, time arguments to certain system calls (currently Linux-only "
    "sleeps) are multiplied by the specified value.  This can help preserve relative "
    "timing among application threads in the presence of significant slowdowns from "
    "tracing.");

} // namespace drmemtrace
} // namespace dynamorio
