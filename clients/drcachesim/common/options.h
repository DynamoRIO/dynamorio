/* **********************************************************
 * Copyright (c) 2015-2018 Google, Inc.  All rights reserved.
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

#define REPLACE_POLICY_NON_SPECIFIED            ""
#define REPLACE_POLICY_LRU                      "LRU"
#define REPLACE_POLICY_LFU                      "LFU"
#define REPLACE_POLICY_FIFO                     "FIFO"
#define PREFETCH_POLICY_NEXTLINE                "nextline"
#define PREFETCH_POLICY_NONE                    "none"
#define CPU_CACHE                               "cache"
#define TLB                                     "TLB"
#define HISTOGRAM                               "histogram"
#define REUSE_DIST                              "reuse_distance"
#define REUSE_TIME                              "reuse_time"
#define BASIC_COUNTS                            "basic_counts"
#define OPCODE_MIX                              "opcode_mix"
#define CACHE_TYPE_INSTRUCTION                  "instruction"
#define CACHE_TYPE_DATA                         "data"
#define CACHE_TYPE_UNIFIED                      "unified"
#define CACHE_PARENT_MEMORY                     "memory"

#include <string>
#include "droption.h"

extern droption_t<bool> op_offline;
extern droption_t<std::string> op_ipc_name;
extern droption_t<std::string> op_outdir;
extern droption_t<std::string> op_infile;
extern droption_t<std::string> op_indir;
extern droption_t<std::string> op_module_file;
extern droption_t<unsigned int> op_num_cores;
extern droption_t<unsigned int> op_line_size;
extern droption_t<bytesize_t> op_L1I_size;
extern droption_t<bytesize_t> op_L1D_size;
extern droption_t<unsigned int> op_L1I_assoc;
extern droption_t<unsigned int> op_L1D_assoc;
extern droption_t<bytesize_t> op_LL_size;
extern droption_t<unsigned int> op_LL_assoc;
extern droption_t<std::string> op_LL_miss_file;
extern droption_t<bytesize_t> op_L0I_size;
extern droption_t<bool> op_L0_filter;
extern droption_t<bytesize_t> op_L0D_size;
extern droption_t<bool> op_use_physical;
extern droption_t<unsigned int> op_virt2phys_freq;
extern droption_t<bool> op_cpu_scheduling;
extern droption_t<bytesize_t> op_max_trace_size;
extern droption_t<bytesize_t> op_trace_after_instrs;
extern droption_t<bytesize_t> op_exit_after_tracing;
extern droption_t<bool> op_online_instr_types;
extern droption_t<std::string> op_replace_policy;
extern droption_t<std::string> op_data_prefetcher;
extern droption_t<bytesize_t> op_page_size;
extern droption_t<unsigned int> op_TLB_L1I_entries;
extern droption_t<unsigned int> op_TLB_L1D_entries;
extern droption_t<unsigned int> op_TLB_L1I_assoc;
extern droption_t<unsigned int> op_TLB_L1D_assoc;
extern droption_t<unsigned int> op_TLB_L2_entries;
extern droption_t<unsigned int> op_TLB_L2_assoc;
extern droption_t<std::string> op_TLB_replace_policy;
extern droption_t<std::string> op_simulator_type;
extern droption_t<unsigned int> op_verbose;
#ifdef DEBUG
extern droption_t<bool> op_test_mode;
#endif
extern droption_t<std::string> op_dr_root;
extern droption_t<bool> op_dr_debug;
extern droption_t<std::string> op_dr_ops;
extern droption_t<std::string> op_tracer;
extern droption_t<std::string> op_tracer_ops;
extern droption_t<bytesize_t> op_skip_refs;
extern droption_t<bytesize_t> op_warmup_refs;
extern droption_t<double> op_warmup_fraction;
extern droption_t<bytesize_t> op_sim_refs;
extern droption_t<unsigned int> op_report_top;
extern droption_t<unsigned int> op_reuse_distance_threshold;
extern droption_t<bool> op_reuse_distance_histogram;
extern droption_t<unsigned int> op_reuse_skip_dist;
extern droption_t<bool> op_reuse_verify_skip;
extern droption_t<bool> op_enable_func_trace;
#endif /* _OPTIONS_H_ */
