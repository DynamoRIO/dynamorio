/* **********************************************************
 * Copyright (c) 2016-2023 Google, Inc.  All rights reserved.
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

#include "analyzer.h"
#include "analyzer_multi.h"
#include "common/options.h"
#include "common/utils.h"
#include "common/directory_iterator.h"
#include "tracer/raw2trace_directory.h"
#include "tracer/raw2trace.h"
#include "reader/file_reader.h"
#ifdef HAS_ZLIB
#    include "common/gzip_istream.h"
#    include "reader/compressed_file_reader.h"
#endif
#ifdef HAS_ZIP
#    include "common/zipfile_istream.h"
#endif
#include "reader/ipc_reader.h"
#include "simulator/cache_simulator_create.h"
#include "simulator/tlb_simulator_create.h"
#include "tools/basic_counts_create.h"
#include "tools/func_view_create.h"
#include "tools/histogram_create.h"
#include "tools/invariant_checker.h"
#include "tools/invariant_checker_create.h"
#include "tools/opcode_mix_create.h"
#include "tools/syscall_mix_create.h"
#include "tools/reuse_distance_create.h"
#include "tools/reuse_time_create.h"
#include "tools/view_create.h"

namespace dynamorio {
namespace drmemtrace {

using ::dynamorio::droption::droption_parser_t;
using ::dynamorio::droption::DROPTION_SCOPE_ALL;

analyzer_multi_t::analyzer_multi_t()
{
    worker_count_ = op_jobs.get_value();
    skip_instrs_ = op_skip_instrs.get_value();
    interval_microseconds_ = op_interval_microseconds.get_value();
    // Initial measurements show it's sometimes faster to keep the parallel model
    // of using single-file readers but use them sequentially, as opposed to
    // the every-file interleaving reader, but the user can specify -jobs 1, so
    // we still keep the serial vs parallel split for 0.
    if (worker_count_ == 0)
        parallel_ = false;
    if (!op_indir.get_value().empty() || !op_infile.get_value().empty())
        op_offline.set_value(true); // Some tools check this on post-proc runs.
    // XXX: add a "required" flag to droption to avoid needing this here
    if (op_indir.get_value().empty() && op_infile.get_value().empty() &&
        op_ipc_name.get_value().empty()) {
        error_string_ =
            "Usage error: -ipc_name or -indir or -infile is required\nUsage:\n" +
            droption_parser_t::usage_short(DROPTION_SCOPE_ALL);
        success_ = false;
        return;
    }
    if (!op_indir.get_value().empty()) {
        std::string tracedir =
            raw2trace_directory_t::tracedir_from_rawdir(op_indir.get_value());
        // We support the trace dir being empty if we haven't post-processed
        // the raw files yet.
        bool needs_processing = false;
        if (!directory_iterator_t::is_directory(tracedir))
            needs_processing = true;
        else {
            directory_iterator_t end;
            directory_iterator_t iter(tracedir);
            if (!iter) {
                needs_processing = true;
            } else {
                int count = 0;
                for (; iter != end; ++iter) {
                    if ((*iter) == "." || (*iter) == ".." ||
                        starts_with(*iter, DRMEMTRACE_SERIAL_SCHEDULE_FILENAME) ||
                        *iter == DRMEMTRACE_CPU_SCHEDULE_FILENAME)
                        continue;
                    ++count;
                    // XXX: It would be nice to call file_reader_t::is_complete()
                    // but we don't have support for that for compressed files.
                    // Thus it's up to the user to delete incomplete processed files.
                }
                if (count == 0)
                    needs_processing = true;
            }
        }
        if (needs_processing) {
            raw2trace_directory_t dir(op_verbose.get_value());
            std::string dir_err =
                dir.initialize(op_indir.get_value(), "", op_trace_compress.get_value());
            if (!dir_err.empty()) {
                success_ = false;
                error_string_ = "Directory setup failed: " + dir_err;
                return;
            }
            raw2trace_t raw2trace(
                dir.modfile_bytes_, dir.in_files_, dir.out_files_, dir.out_archives_,
                dir.encoding_file_, dir.serial_schedule_file_, dir.cpu_schedule_file_,
                nullptr, op_verbose.get_value(), op_jobs.get_value(),
                op_alt_module_dir.get_value(), op_chunk_instr_count.get_value(),
                dir.in_kfiles_map_, dir.kcoredir_, dir.kallsymsdir_);
            std::string error = raw2trace.do_conversion();
            if (!error.empty()) {
                success_ = false;
                error_string_ = "raw2trace failed: " + error;
            }
        }
    }
    // Create the tools after post-processing so we have the schedule files for
    // test_mode.
    if (!create_analysis_tools()) {
        success_ = false;
        error_string_ = "Failed to create analysis tool: " + error_string_;
        return;
    }
    if (!op_indir.get_value().empty()) {
        std::string tracedir =
            raw2trace_directory_t::tracedir_from_rawdir(op_indir.get_value());
        if (!init_scheduler(tracedir, op_only_thread.get_value(), op_verbose.get_value()))
            success_ = false;
    } else if (op_infile.get_value().empty()) {
        // XXX i#3323: Add parallel analysis support for online tools.
        parallel_ = false;
        auto reader = std::unique_ptr<reader_t>(
            new ipc_reader_t(op_ipc_name.get_value().c_str(), op_verbose.get_value()));
        auto end = std::unique_ptr<reader_t>(new ipc_reader_t());
        if (!init_scheduler(std::move(reader), std::move(end), op_verbose.get_value())) {
            success_ = false;
        }
    } else {
        // Legacy file.
        if (!init_scheduler(op_infile.get_value(), INVALID_THREAD_ID /*all threads*/,
                            op_verbose.get_value()))
            success_ = false;
    }
    if (!init_analysis_tools()) {
        success_ = false;
        return;
    }
    // We can't call serial_trace_iter_->init() here as it blocks for ipc_reader_t.
}

analyzer_multi_t::~analyzer_multi_t()
{
    destroy_analysis_tools();
}

bool
analyzer_multi_t::create_analysis_tools()
{
    /* TODO i#2006: add multiple tool support. */
    /* TODO i#2006: create a single top-level tool for multi-component
     * tools.
     */
    tools_ = new analysis_tool_t *[max_num_tools_];
    tools_[0] = create_analysis_tool_from_options();
    if (tools_[0] == NULL)
        return false;
    if (!*tools_[0]) {
        std::string tool_error = tools_[0]->get_error_string();
        if (tool_error.empty())
            tool_error = "no error message provided.";
        error_string_ = "Tool failed to initialize: " + tool_error;
        delete tools_[0];
        tools_[0] = NULL;
        return false;
    }
    num_tools_ = 1;
    if (op_test_mode.get_value()) {
        tools_[1] = create_invariant_checker();
        if (tools_[1] == NULL)
            return false;
        if (!*tools_[1]) {
            error_string_ = tools_[1]->get_error_string();
            delete tools_[1];
            tools_[1] = NULL;
            return false;
        }
        num_tools_ = 2;
    }
    return true;
}

bool
analyzer_multi_t::init_analysis_tools()
{
    // initialize_stream() is now called from analyzer_t::run().
    return true;
}

void
analyzer_multi_t::destroy_analysis_tools()
{
    if (!success_)
        return;
    for (int i = 0; i < num_tools_; i++)
        delete tools_[i];
    delete[] tools_;
}

analysis_tool_t *
analyzer_multi_t::create_analysis_tool_from_options()
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
        knobs.use_physical = op_use_physical.get_value();
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
        knobs.distance_limit = op_reuse_distance_limit.get_value();
        knobs.verify_skip = op_reuse_verify_skip.get_value();
        knobs.histogram_bin_multiplier = op_reuse_histogram_bin_multiplier.get_value();
        if (knobs.histogram_bin_multiplier < 1.0) {
            ERRMSG("Usage error: reuse_histogram_bin_multiplier must be >= 1.0\n");
            return nullptr;
        }
        knobs.verbose = op_verbose.get_value();
        return reuse_distance_tool_create(knobs);
    } else if (op_simulator_type.get_value() == REUSE_TIME) {
        return reuse_time_tool_create(op_line_size.get_value(), op_verbose.get_value());
    } else if (op_simulator_type.get_value() == BASIC_COUNTS) {
        return basic_counts_tool_create(op_verbose.get_value());
    } else if (op_simulator_type.get_value() == OPCODE_MIX) {
        std::string module_file_path = get_module_file_path();
        if (module_file_path.empty() && op_indir.get_value().empty() &&
            op_infile.get_value().empty() && !op_instr_encodings.get_value()) {
            ERRMSG("Usage error: the opcode_mix tool requires offline traces, or "
                   "-instr_encodings for online traces.\n");
            return nullptr;
        }
        return opcode_mix_tool_create(module_file_path, op_verbose.get_value(),
                                      op_alt_module_dir.get_value());
    } else if (op_simulator_type.get_value() == SYSCALL_MIX) {
        return syscall_mix_tool_create(op_verbose.get_value());
    } else if (op_simulator_type.get_value() == VIEW) {
        std::string module_file_path = get_module_file_path();
        // The module file is optional so we don't check for emptiness.
        return view_tool_create(module_file_path, op_skip_refs.get_value(),
                                op_sim_refs.get_value(), op_view_syntax.get_value(),
                                op_verbose.get_value(), op_alt_module_dir.get_value());
    } else if (op_simulator_type.get_value() == FUNC_VIEW) {
        std::string funclist_file_path = get_aux_file_path(
            op_funclist_file.get_value(), DRMEMTRACE_FUNCTION_LIST_FILENAME);
        if (funclist_file_path.empty()) {
            ERRMSG("Usage error: the func_view tool requires offline traces.\n");
            return nullptr;
        }
        return func_view_tool_create(funclist_file_path, op_show_func_trace.get_value(),
                                     op_verbose.get_value());
    } else if (op_simulator_type.get_value() == INVARIANT_CHECKER) {
        return create_invariant_checker();
    } else {
        ERRMSG("Usage error: unsupported analyzer type. "
               "Please choose " CPU_CACHE ", " MISS_ANALYZER ", " TLB ", " HISTOGRAM
               ", " REUSE_DIST ", " BASIC_COUNTS ", " OPCODE_MIX ", " SYSCALL_MIX
               ", " VIEW ", or " FUNC_VIEW ".\n");
        return nullptr;
    }
}

analysis_tool_t *
analyzer_multi_t::create_invariant_checker()
{
    if (op_offline.get_value()) {
        // TODO i#5538: Locate and open the schedule files and pass to the
        // reader(s) for seeking. For now we only read them for this test.
        // TODO i#5843: Share this code with scheduler_t or pass in for all
        // tools from here for fast skipping in serial and per-cpu modes.
        std::string tracedir =
            raw2trace_directory_t::tracedir_from_rawdir(op_indir.get_value());
        if (directory_iterator_t::is_directory(tracedir)) {
            directory_iterator_t end;
            directory_iterator_t iter(tracedir);
            if (!iter) {
                error_string_ = "Failed to list directory: " + iter.error_string();
                return nullptr;
            }
            for (; iter != end; ++iter) {
                const std::string fname = *iter;
                const std::string fpath = tracedir + DIRSEP + fname;
                if (starts_with(fname, DRMEMTRACE_SERIAL_SCHEDULE_FILENAME)) {
                    if (ends_with(fname, ".gz")) {
#ifdef HAS_ZLIB
                        serial_schedule_file_ =
                            std::unique_ptr<std::istream>(new gzip_istream_t(fpath));
#endif
                    } else {
                        serial_schedule_file_ = std::unique_ptr<std::istream>(
                            new std::ifstream(fpath, std::ifstream::binary));
                    }
                    if (serial_schedule_file_ && !*serial_schedule_file_) {
                        error_string_ = "Failed to open serial schedule file " + fpath;
                        return nullptr;
                    }
                } else if (fname == DRMEMTRACE_CPU_SCHEDULE_FILENAME) {
#ifdef HAS_ZIP
                    cpu_schedule_file_ =
                        std::unique_ptr<std::istream>(new zipfile_istream_t(fpath));
#endif
                }
            }
        }
    }
    return new invariant_checker_t(op_offline.get_value(), op_verbose.get_value(),
                                   op_test_mode_name.get_value(),
                                   serial_schedule_file_.get(), cpu_schedule_file_.get());
}

/* Get the path to an auxiliary file by examining
 * 1. The corresponding command line option
 * 2. The trace directory
 * If a trace file is provided instead of a trace directory, it searches in the
 * directory which contains the trace file.
 */
std::string
analyzer_multi_t::get_aux_file_path(std::string option_val, std::string default_filename)
{
    std::string file_path;
    if (!option_val.empty())
        file_path = option_val;
    else {
        std::string trace_dir;
        if (!op_indir.get_value().empty())
            trace_dir = op_indir.get_value();
        else {
            if (op_infile.get_value().empty()) {
                return "";
            }
            size_t sep_index = op_infile.get_value().find_last_of(DIRSEP ALT_DIRSEP);
            if (sep_index != std::string::npos)
                trace_dir = std::string(op_infile.get_value(), 0, sep_index);
        }
        if (raw2trace_directory_t::is_window_subdir(trace_dir)) {
            // If we're operating on a specific window, point at the parent for the
            // modfile.
            trace_dir += std::string(DIRSEP) + "..";
        }
        file_path = trace_dir + std::string(DIRSEP) + default_filename;
        /* Support the aux file in either raw/ or trace/. */
        if (!std::ifstream(file_path.c_str()).good()) {
            file_path = trace_dir + std::string(DIRSEP) + TRACE_SUBDIR +
                std::string(DIRSEP) + default_filename;
        }
        if (!std::ifstream(file_path.c_str()).good()) {
            file_path = trace_dir + std::string(DIRSEP) + OUTFILE_SUBDIR +
                std::string(DIRSEP) + default_filename;
        }
    }
    if (!std::ifstream(file_path.c_str()).good())
        return "";
    return file_path;
}

std::string
analyzer_multi_t::get_module_file_path()
{
    return get_aux_file_path(op_module_file.get_value(), DRMEMTRACE_MODULE_LIST_FILENAME);
}

/* Get the cache simulator knobs used by the cache simulator
 * and the cache miss analyzer.
 */
cache_simulator_knobs_t *
analyzer_multi_t::get_cache_simulator_knobs()
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
    knobs->model_coherence = op_coherence.get_value();
    knobs->replace_policy = op_replace_policy.get_value();
    knobs->data_prefetcher = op_data_prefetcher.get_value();
    knobs->skip_refs = op_skip_refs.get_value();
    knobs->warmup_refs = op_warmup_refs.get_value();
    knobs->warmup_fraction = op_warmup_fraction.get_value();
    knobs->sim_refs = op_sim_refs.get_value();
    knobs->verbose = op_verbose.get_value();
    knobs->cpu_scheduling = op_cpu_scheduling.get_value();
    knobs->use_physical = op_use_physical.get_value();
    return knobs;
}

} // namespace drmemtrace
} // namespace dynamorio
