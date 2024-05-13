/* **********************************************************
 * Copyright (c) 2015-2024 Google, Inc.  All rights reserved.
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

/* shared options for the frontend, the client, and the documentation */

#ifndef _OPTIONS_H_
#define _OPTIONS_H_ 1

// Tool names (for -tool option).
#define CPU_CACHE_LEGACY "cache"
#define CPU_CACHE "cache_simulator"
#define CPU_CACHE_ALT "drcachesim"
#define MISS_ANALYZER "miss_analyzer"
#define TLB_LEGACY "TLB"
#define TLB "TLB_simulator"
#define HISTOGRAM "histogram"
#define REUSE_DIST "reuse_distance"
#define REUSE_TIME "reuse_time"
#define BASIC_COUNTS "basic_counts"
#define OPCODE_MIX "opcode_mix"
#define SYSCALL_MIX "syscall_mix"
#define VIEW "view"
#define FUNC_VIEW "func_view"
#define INVARIANT_CHECKER "invariant_checker"
#define SCHEDULE_STATS "schedule_stats"
#define RECORD_FILTER "record_filter"

// Constants used by specific tools.
#define REPLACE_POLICY_NON_SPECIFIED ""
#define REPLACE_POLICY_LRU "LRU"
#define REPLACE_POLICY_LFU "LFU"
#define REPLACE_POLICY_FIFO "FIFO"
#define PREFETCH_POLICY_NEXTLINE "nextline"
#define PREFETCH_POLICY_NONE "none"
#define CACHE_TYPE_INSTRUCTION "instruction"
#define CACHE_TYPE_DATA "data"
#define CACHE_TYPE_UNIFIED "unified"
#define CACHE_PARENT_MEMORY "memory"

// The expected pattern for a single_op_value is:
//     function_name|function_id|arguments_num
// where function_name can contain spaces (for instance, C++ namespace prefix)
#define PATTERN_SEPARATOR "|"

#ifdef HAS_ZIP
#    define DEFAULT_TRACE_COMPRESSION_TYPE "zip"
#elif defined(HAS_LZ4)
#    define DEFAULT_TRACE_COMPRESSION_TYPE "lz4"
#elif defined(HAS_ZLIB)
#    define DEFAULT_TRACE_COMPRESSION_TYPE "gzip"
#else
#    define DEFAULT_TRACE_COMPRESSION_TYPE "none"
#endif

#include <string>

#include "droption.h"

namespace dynamorio {
namespace drmemtrace {

extern dynamorio::droption::droption_t<bool> op_offline;
extern dynamorio::droption::droption_t<std::string> op_ipc_name;
extern dynamorio::droption::droption_t<std::string> op_outdir;
extern dynamorio::droption::droption_t<std::string> op_subdir_prefix;
extern dynamorio::droption::droption_t<std::string> op_infile;
extern dynamorio::droption::droption_t<std::string> op_indir;
extern dynamorio::droption::droption_t<std::string> op_module_file;
extern dynamorio::droption::droption_t<std::string> op_alt_module_dir;
extern dynamorio::droption::droption_t<dynamorio::droption::bytesize_t>
    op_chunk_instr_count;
extern dynamorio::droption::droption_t<bool> op_instr_encodings;
extern dynamorio::droption::droption_t<std::string> op_funclist_file;
extern dynamorio::droption::droption_t<unsigned int> op_num_cores;
extern dynamorio::droption::droption_t<unsigned int> op_line_size;
extern dynamorio::droption::droption_t<dynamorio::droption::bytesize_t> op_L1I_size;
extern dynamorio::droption::droption_t<dynamorio::droption::bytesize_t> op_L1D_size;
extern dynamorio::droption::droption_t<unsigned int> op_L1I_assoc;
extern dynamorio::droption::droption_t<unsigned int> op_L1D_assoc;
extern dynamorio::droption::droption_t<dynamorio::droption::bytesize_t> op_LL_size;
extern dynamorio::droption::droption_t<unsigned int> op_LL_assoc;
extern dynamorio::droption::droption_t<std::string> op_LL_miss_file;
extern dynamorio::droption::droption_t<dynamorio::droption::bytesize_t> op_L0I_size;
extern dynamorio::droption::droption_t<bool> op_L0_filter_deprecated;
extern dynamorio::droption::droption_t<bool> op_L0I_filter;
extern dynamorio::droption::droption_t<bool> op_L0D_filter;
extern dynamorio::droption::droption_t<dynamorio::droption::bytesize_t> op_L0D_size;
extern dynamorio::droption::droption_t<dynamorio::droption::bytesize_t>
    op_L0_filter_until_instrs;
extern dynamorio::droption::droption_t<bool> op_instr_only_trace;
extern dynamorio::droption::droption_t<bool> op_coherence;
extern dynamorio::droption::droption_t<bool> op_use_physical;
extern dynamorio::droption::droption_t<unsigned int> op_virt2phys_freq;
extern dynamorio::droption::droption_t<bool> op_cpu_scheduling;
extern dynamorio::droption::droption_t<dynamorio::droption::bytesize_t> op_max_trace_size;
extern dynamorio::droption::droption_t<dynamorio::droption::bytesize_t>
    op_max_global_trace_refs;
extern dynamorio::droption::droption_t<bool> op_align_endpoints;
extern dynamorio::droption::droption_t<dynamorio::droption::bytesize_t>
    op_trace_after_instrs;
extern dynamorio::droption::droption_t<dynamorio::droption::bytesize_t>
    op_trace_for_instrs;
extern dynamorio::droption::droption_t<dynamorio::droption::bytesize_t>
    op_retrace_every_instrs;
extern dynamorio::droption::droption_t<bool> op_split_windows;
extern dynamorio::droption::droption_t<dynamorio::droption::bytesize_t>
    op_exit_after_tracing;
extern dynamorio::droption::droption_t<std::string> op_raw_compress;
extern dynamorio::droption::droption_t<std::string> op_trace_compress;
extern dynamorio::droption::droption_t<bool> op_online_instr_types;
extern dynamorio::droption::droption_t<std::string> op_replace_policy;
extern dynamorio::droption::droption_t<std::string> op_data_prefetcher;
extern dynamorio::droption::droption_t<dynamorio::droption::bytesize_t> op_page_size;
extern dynamorio::droption::droption_t<unsigned int> op_TLB_L1I_entries;
extern dynamorio::droption::droption_t<unsigned int> op_TLB_L1D_entries;
extern dynamorio::droption::droption_t<unsigned int> op_TLB_L1I_assoc;
extern dynamorio::droption::droption_t<unsigned int> op_TLB_L1D_assoc;
extern dynamorio::droption::droption_t<unsigned int> op_TLB_L2_entries;
extern dynamorio::droption::droption_t<unsigned int> op_TLB_L2_assoc;
extern dynamorio::droption::droption_t<std::string> op_TLB_replace_policy;
extern dynamorio::droption::droption_t<std::string> op_tool;
extern dynamorio::droption::droption_t<unsigned int> op_verbose;
extern dynamorio::droption::droption_t<bool> op_show_func_trace;
extern dynamorio::droption::droption_t<int> op_jobs;
extern dynamorio::droption::droption_t<bool> op_test_mode;
extern dynamorio::droption::droption_t<std::string> op_test_mode_name;
extern dynamorio::droption::droption_t<bool> op_disable_optimizations;
extern dynamorio::droption::droption_t<std::string> op_dr_root;
extern dynamorio::droption::droption_t<bool> op_dr_debug;
extern dynamorio::droption::droption_t<std::string> op_dr_ops;
extern dynamorio::droption::droption_t<std::string> op_tracer;
extern dynamorio::droption::droption_t<std::string> op_tracer_alt;
extern dynamorio::droption::droption_t<std::string> op_tracer_ops;
extern dynamorio::droption::droption_t<dynamorio::droption::bytesize_t>
    op_interval_microseconds;
extern dynamorio::droption::droption_t<dynamorio::droption::bytesize_t>
    op_interval_instr_count;
extern dynamorio::droption::droption_t<int> op_only_thread;
extern dynamorio::droption::droption_t<dynamorio::droption::bytesize_t> op_skip_instrs;
extern dynamorio::droption::droption_t<dynamorio::droption::bytesize_t> op_skip_refs;
extern dynamorio::droption::droption_t<dynamorio::droption::bytesize_t> op_warmup_refs;
extern dynamorio::droption::droption_t<double> op_warmup_fraction;
extern dynamorio::droption::droption_t<dynamorio::droption::bytesize_t> op_sim_refs;
extern dynamorio::droption::droption_t<std::string> op_config_file;
extern dynamorio::droption::droption_t<unsigned int> op_report_top;
extern dynamorio::droption::droption_t<unsigned int> op_reuse_distance_threshold;
extern dynamorio::droption::droption_t<bool> op_reuse_distance_histogram;
extern dynamorio::droption::droption_t<unsigned int> op_reuse_skip_dist;
extern dynamorio::droption::droption_t<unsigned int> op_reuse_distance_limit;
extern dynamorio::droption::droption_t<bool> op_reuse_verify_skip;
extern dynamorio::droption::droption_t<double> op_reuse_histogram_bin_multiplier;
extern dynamorio::droption::droption_t<std::string> op_view_syntax;
extern dynamorio::droption::droption_t<std::string> op_record_function;
extern dynamorio::droption::droption_t<bool> op_record_heap;
extern dynamorio::droption::droption_t<std::string> op_record_heap_value;
extern dynamorio::droption::droption_t<bool> op_record_dynsym_only;
extern dynamorio::droption::droption_t<bool> op_record_replace_retaddr;
extern dynamorio::droption::droption_t<std::string> op_record_syscall;
extern dynamorio::droption::droption_t<unsigned int> op_miss_count_threshold;
extern dynamorio::droption::droption_t<double> op_miss_frac_threshold;
extern dynamorio::droption::droption_t<double> op_confidence_threshold;
extern dynamorio::droption::droption_t<bool> op_enable_drstatecmp;
#ifdef BUILD_PT_TRACER
extern dynamorio::droption::droption_t<bool> op_enable_kernel_tracing;
#endif
extern dynamorio::droption::droption_t<bool> op_core_sharded;
extern dynamorio::droption::droption_t<bool> op_core_serial;
extern dynamorio::droption::droption_t<int64_t> op_sched_quantum;
extern dynamorio::droption::droption_t<bool> op_sched_time;
extern dynamorio::droption::droption_t<bool> op_sched_order_time;
extern dynamorio::droption::droption_t<uint64_t> op_sched_syscall_switch_us;
extern dynamorio::droption::droption_t<uint64_t> op_sched_blocking_switch_us;
extern dynamorio::droption::droption_t<double> op_sched_block_scale;
extern dynamorio::droption::droption_t<uint64_t> op_sched_block_max_us;
#ifdef HAS_ZIP
extern dynamorio::droption::droption_t<std::string> op_record_file;
extern dynamorio::droption::droption_t<std::string> op_replay_file;
extern dynamorio::droption::droption_t<std::string> op_cpu_schedule_file;
#endif
extern dynamorio::droption::droption_t<std::string> op_sched_switch_file;
extern dynamorio::droption::droption_t<bool> op_sched_randomize;
extern dynamorio::droption::droption_t<bool> op_sched_disable_direct_switches;
extern dynamorio::droption::droption_t<uint64_t> op_schedule_stats_print_every;
extern dynamorio::droption::droption_t<std::string> op_syscall_template_file;
extern dynamorio::droption::droption_t<uint64_t> op_filter_stop_timestamp;
extern dynamorio::droption::droption_t<int> op_filter_cache_size;
extern dynamorio::droption::droption_t<std::string> op_filter_trace_types;
extern dynamorio::droption::droption_t<std::string> op_filter_marker_types;
extern dynamorio::droption::droption_t<bool> op_encodings2regdeps;
extern dynamorio::droption::droption_t<uint64_t> op_trim_before_timestamp;
extern dynamorio::droption::droption_t<uint64_t> op_trim_after_timestamp;
extern dynamorio::droption::droption_t<bool> op_abort_on_invariant_error;

} // namespace drmemtrace
} // namespace dynamorio

#endif /* _OPTIONS_H_ */
