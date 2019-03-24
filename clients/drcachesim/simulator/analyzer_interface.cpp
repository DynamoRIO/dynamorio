/* **********************************************************
 * Copyright (c) 2017-2018 Google, Inc.  All rights reserved.
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

#include "../analysis_tool_interface.h"
#include "../analysis_tool.h"
#include "../common/options.h"
#include "../common/utils.h"
#include "cache_simulator_create.h"
#include "tlb_simulator_create.h"
/* XXX i#2006: we include these here for now but it's undecided whether they
 * should be separated and this should only include
 * cache-simulation-based tools.
 */
#include "../tools/histogram_create.h"
#include "../tools/reuse_distance_create.h"
#include "../tools/reuse_time_create.h"
#include "../tools/basic_counts_create.h"
#include "../tools/opcode_mix_create.h"
#include "../tools/view_create.h"
#include "../tracer/raw2trace.h"
#include <fstream>

/* Get the path to the modules.log file by examining
 * 1. the module_file option
 * 2. the trace directory
 * If a trace file is provided instead of a trace directory, it searches in the
 * directory which contains the trace file.
 */
static std::string
get_module_file_path()
{
    std::string module_file_path;
    if (!op_module_file.get_value().empty())
        module_file_path = op_module_file.get_value();
    else {
        std::string trace_dir;
        if (!op_indir.get_value().empty())
            trace_dir = op_indir.get_value();
        else {
            if (op_infile.get_value().empty()) {
                ERRMSG("Usage error: the opcode mix tool requires offline traces.\n");
                return "";
            }
            size_t sep_index = op_infile.get_value().find_last_of(DIRSEP ALT_DIRSEP);
            if (sep_index != std::string::npos)
                trace_dir = std::string(op_infile.get_value(), 0, sep_index);
        }
        module_file_path =
            trace_dir + std::string(DIRSEP) + DRMEMTRACE_MODULE_LIST_FILENAME;
        if (!std::ifstream(module_file_path.c_str()).good()) {
            trace_dir += std::string(DIRSEP) + OUTFILE_SUBDIR;
            module_file_path =
                trace_dir + std::string(DIRSEP) + DRMEMTRACE_MODULE_LIST_FILENAME;
        }
    }
    return module_file_path;
}

/* Get the cache simulator knobs used by the cache simulator
 * and the cache miss analyzer.
 */
static cache_simulator_knobs_t *
get_cache_simulator_knobs()
{
    cache_simulator_knobs_t *knobs = new cache_simulator_knobs_t;
    knobs->num_cores = op_num_cores.get_value();
    knobs->line_size = op_line_size.get_value();
    knobs->L1I_size = op_L1I_size.get_value();
    knobs->L1D_size = op_L1D_size.get_value();
    knobs->L1I_assoc = op_L1I_assoc.get_value();
    knobs->L1D_assoc = op_L1D_assoc.get_value();
    knobs->LL_size = op_LL_size.get_value();
    knobs->LL_assoc = op_LL_assoc.get_value();
    knobs->LL_miss_file = op_LL_miss_file.get_value();
    knobs->replace_policy = op_replace_policy.get_value();
    knobs->data_prefetcher = op_data_prefetcher.get_value();
    knobs->skip_refs = op_skip_refs.get_value();
    knobs->warmup_refs = op_warmup_refs.get_value();
    knobs->warmup_fraction = op_warmup_fraction.get_value();
    knobs->sim_refs = op_sim_refs.get_value();
    knobs->verbose = op_verbose.get_value();
    knobs->cpu_scheduling = op_cpu_scheduling.get_value();
    return knobs;
}

analysis_tool_t *
drmemtrace_analysis_tool_create()
{
    if (op_simulator_type.get_value() == CPU_CACHE) {
        const std::string &config_file = op_config_file.get_value();
        if (!config_file.empty()) {
            return cache_simulator_create(config_file);
        } else {
            cache_simulator_knobs_t *knobs = get_cache_simulator_knobs();
            return cache_simulator_create(*knobs);
        }
    } else if (op_simulator_type.get_value() == MISS_ANALYZER) {
        cache_simulator_knobs_t *knobs = get_cache_simulator_knobs();
        return cache_miss_analyzer_create(*knobs, op_miss_count_threshold.get_value(),
                                          op_miss_frac_threshold.get_value(),
                                          op_confidence_threshold.get_value());
    } else if (op_simulator_type.get_value() == TLB) {
        tlb_simulator_knobs_t knobs;
        knobs.num_cores = op_num_cores.get_value();
        knobs.page_size = op_page_size.get_value();
        knobs.TLB_L1I_entries = op_TLB_L1I_entries.get_value();
        knobs.TLB_L1D_entries = op_TLB_L1D_entries.get_value();
        knobs.TLB_L1I_assoc = op_TLB_L1I_assoc.get_value();
        knobs.TLB_L1D_assoc = op_TLB_L1D_assoc.get_value();
        knobs.TLB_L2_entries = op_TLB_L2_entries.get_value();
        knobs.TLB_L2_assoc = op_TLB_L2_assoc.get_value();
        knobs.TLB_replace_policy = op_TLB_replace_policy.get_value();
        knobs.skip_refs = op_skip_refs.get_value();
        knobs.warmup_refs = op_warmup_refs.get_value();
        knobs.warmup_fraction = op_warmup_fraction.get_value();
        knobs.sim_refs = op_sim_refs.get_value();
        knobs.verbose = op_verbose.get_value();
        knobs.cpu_scheduling = op_cpu_scheduling.get_value();
        return tlb_simulator_create(knobs);
    } else if (op_simulator_type.get_value() == HISTOGRAM) {
        return histogram_tool_create(op_line_size.get_value(), op_report_top.get_value(),
                                     op_verbose.get_value());
    } else if (op_simulator_type.get_value() == REUSE_DIST) {
        reuse_distance_knobs_t knobs;
        knobs.line_size = op_line_size.get_value();
        knobs.report_histogram = op_reuse_distance_histogram.get_value();
        knobs.distance_threshold = op_reuse_distance_threshold.get_value();
        knobs.report_top = op_report_top.get_value();
        knobs.skip_list_distance = op_reuse_skip_dist.get_value();
        knobs.verify_skip = op_reuse_verify_skip.get_value();
        knobs.verbose = op_verbose.get_value();
        return reuse_distance_tool_create(knobs);
    } else if (op_simulator_type.get_value() == REUSE_TIME) {
        return reuse_time_tool_create(op_line_size.get_value(), op_verbose.get_value());
    } else if (op_simulator_type.get_value() == BASIC_COUNTS) {
        return basic_counts_tool_create(op_verbose.get_value());
    } else if (op_simulator_type.get_value() == OPCODE_MIX) {
        std::string module_file_path = get_module_file_path();
        if (module_file_path.empty())
            return nullptr;
        return opcode_mix_tool_create(module_file_path, op_verbose.get_value());
    } else if (op_simulator_type.get_value() == VIEW) {
        std::string module_file_path = get_module_file_path();
        if (module_file_path.empty())
            return nullptr;
        return view_tool_create(module_file_path, op_skip_refs.get_value(),
                                op_sim_refs.get_value(), op_view_syntax.get_value(),
                                op_verbose.get_value());
    } else {
        ERRMSG("Usage error: unsupported analyzer type. "
               "Please choose " CPU_CACHE ", " MISS_ANALYZER ", " TLB ", " HISTOGRAM
               ", " REUSE_DIST ", " BASIC_COUNTS ", " OPCODE_MIX " or " VIEW ".\n");
        return nullptr;
    }
}
