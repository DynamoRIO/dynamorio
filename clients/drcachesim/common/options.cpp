/* **********************************************************
 * Copyright (c) 2015-2023 Google, Inc.  All rights reserved.
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
    "to a directory where per-thread trace files will be written.");

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
    "interleaved into one are not supported with this option: use -infile instead.");

droption_t<std::string> op_infile(
    DROPTION_SCOPE_ALL, "infile", "", "Offline legacy file for input to the simulator",
    "Directs the simulator to use a single all-threads-interleaved-into-one trace file. "
    "This is a legacy file format that is no longer produced.");

droption_t<int> op_jobs(
    DROPTION_SCOPE_ALL, "jobs", -1, "Number of parallel jobs",
    "By default, both post-processing of offline raw trace files and analysis of trace "
    "files is parallelized.  This option controls the number of concurrent jobs.  0 "
    "disables concurrency and uses a single thread to perform all operations.  A "
    "negative value sets the job count to the number of hardware threads, "
    "with a cap of 16.");

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

droption_t<unsigned int> op_num_cores(DROPTION_SCOPE_FRONTEND, "cores", 4,
                                      "Number of cores",
                                      "Specifies the number of cores to simulate.");

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
    "is written in text format as a <program counter, address> pair. If this tool is "
    "linked with zlib, the file is written in gzip-compressed format. If non-empty, when "
    "running the cache miss analyzer, requests that prefetching hints based on the miss "
    "analysis be written to the specified file. Each hint is written in text format as a "
    "<program counter, stride, locality level> tuple.");

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

droption_t<bool> op_cpu_scheduling(
    DROPTION_SCOPE_CLIENT, "cpu_scheduling", false,
    "Map threads to cores matching recorded cpu execution",
    "By default, the simulator schedules threads to simulated cores in a static "
    "round-robin fashion.  This option causes the scheduler to instead use the recorded "
    "cpu that each thread executed on (at a granularity of the trace buffer size) "
    "for scheduling, mapping traced cpu's to cores and running each segment of each "
    "thread "
    "on the core that owns the recorded cpu for that segment.");

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
                          "Supported policies: LFU (Least Frequently Used).");

droption_t<std::string>
    op_simulator_type(DROPTION_SCOPE_FRONTEND, "simulator_type", CPU_CACHE,
                      "Simulator type (" CPU_CACHE ", " MISS_ANALYZER ", " TLB
                      ", " REUSE_DIST ", " REUSE_TIME ", " HISTOGRAM ", " VIEW
                      ", " FUNC_VIEW ", " BASIC_COUNTS ", or " INVARIANT_CHECKER ").",
                      "Specifies the type of the simulator. "
                      "Supported types: " CPU_CACHE ", " MISS_ANALYZER ", " TLB
                      ", " REUSE_DIST ", " REUSE_TIME ", " HISTOGRAM ", " BASIC_COUNTS
                      ", or " INVARIANT_CHECKER ".");

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
    "If set, analysis tools receive a callback at the end of each interval.");

droption_t<int>
    op_only_thread(DROPTION_SCOPE_FRONTEND, "only_thread", 0,
                   "Only analyze this thread (0 means all)",
                   "Limits analyis to the single "
                   "thread with the given identifier.  0 enables all threads.");

droption_t<bytesize_t> op_skip_instrs(
    DROPTION_SCOPE_FRONTEND, "skip_instrs", 0, "Number of instructions to skip",
    "Specifies the number of instructions to skip in the beginning of the trace "
    "analysis.  For serial iteration, this number is "
    "computed just once across the interleaving sequence of all threads; for parallel "
    "iteration, each thread skips this many insructions.  When built with zipfile "
    "support, this skipping is optimized and large instruction counts can be quickly "
    "skipped; this is not the case for -skip_refs.");

droption_t<bytesize_t>
    op_skip_refs(DROPTION_SCOPE_FRONTEND, "skip_refs", 0,
                 "Number of memory references to skip",
                 "Specifies the number of references to skip in the beginning of the "
                 "application execution. These memory references are dropped instead "
                 "of being simulated.  This skipping may be slow for large skip values; "
                 "consider -skip_instrs for a faster method of skipping.");

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
    DROPTION_SCOPE_FRONTEND, "warmup_refs", 0,
    "Number of memory references to warm caches up",
    "Specifies the number of memory references to warm up caches before simulation. "
    "The warmup references come after the skipped references and before the "
    "simulated references. This flag is incompatible with warmup_fraction.");

droption_t<double> op_warmup_fraction(
    DROPTION_SCOPE_FRONTEND, "warmup_fraction", 0.0, 0.0, 1.0,
    "Fraction of last level cache blocks to be loaded as warm up",
    "Specifies the fraction of last level cache blocks to be loaded such that the "
    "cache is considered to be warmed up before simulation. The warmup fraction "
    "is computed after the skipped references and before simulated references. "
    "This flag is incompatible with warmup_refs.");

droption_t<bytesize_t>
    op_sim_refs(DROPTION_SCOPE_FRONTEND, "sim_refs", bytesize_t(1ULL << 63),
                "Number of memory references to simulate",
                "Specifies the number of memory references to simulate. "
                "The simulated references come after the skipped and warmup references, "
                "and the references following the simulated ones are dropped.");

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
    DROPTION_SCOPE_CLIENT, "record_heap_value", DROPTION_FLAG_ACCUMULATE,
    OP_RECORD_FUNC_ITEM_SEP,
    "malloc|1" OP_RECORD_FUNC_ITEM_SEP "free|1|noret" OP_RECORD_FUNC_ITEM_SEP
    "tc_malloc|1" OP_RECORD_FUNC_ITEM_SEP "tc_free|1|noret" OP_RECORD_FUNC_ITEM_SEP
    "__libc_malloc|1" OP_RECORD_FUNC_ITEM_SEP
    "__libc_free|1|noret" OP_RECORD_FUNC_ITEM_SEP "calloc|2"
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
#endif

} // namespace drmemtrace
} // namespace dynamorio
