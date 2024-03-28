/* **********************************************************
 * Copyright (c) 2016-2024 Google, Inc.  All rights reserved.
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
#    ifdef HAS_ZIP
#        include "zipfile_istream.h"
#        include "zipfile_ostream.h"
#    endif
#endif
#ifdef HAS_ZIP
#    include "common/zipfile_istream.h"
#endif
#include "reader/ipc_reader.h"
#include "simulator/cache_simulator_create.h"
#include "simulator/tlb_simulator_create.h"
#include "tools/basic_counts_create.h"
#include "tools/filter/record_filter_create.h"
#include "tools/func_view_create.h"
#include "tools/histogram_create.h"
#include "tools/invariant_checker.h"
#include "tools/invariant_checker_create.h"
#include "tools/opcode_mix_create.h"
#include "tools/schedule_stats_create.h"
#include "tools/syscall_mix_create.h"
#include "tools/reuse_distance_create.h"
#include "tools/reuse_time_create.h"
#include "tools/view_create.h"
#include "tools/loader/external_config_file.h"
#include "tools/loader/external_tool_creator.h"
#include "tools/filter/record_filter_create.h"

namespace dynamorio {
namespace drmemtrace {

using ::dynamorio::droption::droption_parser_t;
using ::dynamorio::droption::DROPTION_SCOPE_ALL;

/****************************************************************
 * Specializations for analyzer_multi_tmpl_t<memref_t, reader_t>,
 * aka analyzer_multi_t.
 */

template <>
std::unique_ptr<reader_t>
analyzer_multi_t::create_ipc_reader(const char *name, int verbose)
{
    return std::unique_ptr<reader_t>(new ipc_reader_t(name, verbose));
}

template <>
std::unique_ptr<reader_t>
analyzer_multi_t::create_ipc_reader_end()
{
    return std::unique_ptr<reader_t>(new ipc_reader_t());
}

template <>
analysis_tool_t *
analyzer_multi_t::create_external_tool(const std::string &tool_name)
{
    analysis_tool_t *tool = nullptr;

    std::string tools_dir(op_dr_root.get_value());
    tools_dir += std::string(DIRSEP) + "tools" + std::string(DIRSEP);
    directory_iterator_t end;
    directory_iterator_t iter(tools_dir);

    if (!iter) {
        return nullptr;
    }

    for (; iter != end; ++iter) {
        if ((*iter).find(".drcachesim") != std::string::npos) {
            std::string abs_path(tools_dir);
            abs_path.append(*iter);
            external_tool_config_file_t config(op_dr_root.get_value(), abs_path);
            if (config.valid_ && config.tool_name_ == tool_name) {
                external_tool_creator_t creator(config.creator_path_);
                error_string_ = creator.error();
                if (creator.error().empty()) {
                    DR_ASSERT(creator.get_tool_name() == tool_name.c_str());
                    tool = creator.create_tool();
                    loaders_.push_back(std::move(creator));
                    break;
                }
            }
        }
    }
    return tool;
}

template <>
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
                this->error_string_ = "Failed to list directory: " + iter.error_string();
                return nullptr;
            }
            for (; iter != end; ++iter) {
                const std::string fname = *iter;
                const std::string fpath = tracedir + DIRSEP + fname;
                if (starts_with(fname, DRMEMTRACE_SERIAL_SCHEDULE_FILENAME)) {
                    if (ends_with(fname, ".gz")) {
#ifdef HAS_ZLIB
                        this->serial_schedule_file_ =
                            std::unique_ptr<std::istream>(new gzip_istream_t(fpath));
#endif
                    } else {
                        this->serial_schedule_file_ = std::unique_ptr<std::istream>(
                            new std::ifstream(fpath, std::ifstream::binary));
                    }
                    if (this->serial_schedule_file_ && !*serial_schedule_file_) {
                        this->error_string_ =
                            "Failed to open serial schedule file " + fpath;
                        return nullptr;
                    }
                } else if (fname == DRMEMTRACE_CPU_SCHEDULE_FILENAME) {
#ifdef HAS_ZIP
                    this->cpu_schedule_file_ =
                        std::unique_ptr<std::istream>(new zipfile_istream_t(fpath));
#endif
                }
            }
        }
    }
    return new invariant_checker_t(op_offline.get_value(), op_verbose.get_value(),
                                   op_test_mode_name.get_value(),
                                   serial_schedule_file_.get(), cpu_schedule_file_.get(),
                                   op_abort_on_invariant_error.get_value());
}

template <>
analysis_tool_t *
analyzer_multi_t::create_analysis_tool_from_options(const std::string &simulator_type)
{
    if (simulator_type == CPU_CACHE) {
        const std::string &config_file = op_config_file.get_value();
        if (!config_file.empty()) {
            return cache_simulator_create(config_file);
        } else {
            cache_simulator_knobs_t *knobs = get_cache_simulator_knobs();
            return cache_simulator_create(*knobs);
        }
    } else if (simulator_type == MISS_ANALYZER) {
        cache_simulator_knobs_t *knobs = get_cache_simulator_knobs();
        return cache_miss_analyzer_create(*knobs, op_miss_count_threshold.get_value(),
                                          op_miss_frac_threshold.get_value(),
                                          op_confidence_threshold.get_value());
    } else if (simulator_type == TLB) {
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
    } else if (simulator_type == HISTOGRAM) {
        return histogram_tool_create(op_line_size.get_value(), op_report_top.get_value(),
                                     op_verbose.get_value());
    } else if (simulator_type == REUSE_DIST) {
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
    } else if (simulator_type == REUSE_TIME) {
        return reuse_time_tool_create(op_line_size.get_value(), op_verbose.get_value());
    } else if (simulator_type == BASIC_COUNTS) {
        return basic_counts_tool_create(op_verbose.get_value());
    } else if (simulator_type == OPCODE_MIX) {
        std::string module_file_path = get_module_file_path();
        if (module_file_path.empty() && op_indir.get_value().empty() &&
            op_infile.get_value().empty() && !op_instr_encodings.get_value()) {
            ERRMSG("Usage error: the opcode_mix tool requires offline traces, or "
                   "-instr_encodings for online traces.\n");
            return nullptr;
        }
        return opcode_mix_tool_create(module_file_path, op_verbose.get_value(),
                                      op_alt_module_dir.get_value());
    } else if (simulator_type == SYSCALL_MIX) {
        return syscall_mix_tool_create(op_verbose.get_value());
    } else if (simulator_type == VIEW) {
        std::string module_file_path = get_module_file_path();
        // The module file is optional so we don't check for emptiness.
        return view_tool_create(module_file_path, op_skip_refs.get_value(),
                                op_sim_refs.get_value(), op_view_syntax.get_value(),
                                op_verbose.get_value(), op_alt_module_dir.get_value());
    } else if (simulator_type == FUNC_VIEW) {
        std::string funclist_file_path = get_aux_file_path(
            op_funclist_file.get_value(), DRMEMTRACE_FUNCTION_LIST_FILENAME);
        if (funclist_file_path.empty()) {
            ERRMSG("Usage error: the func_view tool requires offline traces.\n");
            return nullptr;
        }
        return func_view_tool_create(funclist_file_path, op_show_func_trace.get_value(),
                                     op_verbose.get_value());
    } else if (simulator_type == INVARIANT_CHECKER) {
        return create_invariant_checker();
    } else if (simulator_type == SCHEDULE_STATS) {
        return schedule_stats_tool_create(op_schedule_stats_print_every.get_value(),
                                          op_verbose.get_value());
    } else {
        auto tool = create_external_tool(simulator_type);
        if (tool == nullptr) {
            ERRMSG("Usage error: unsupported analyzer type \"%s\". "
                   "Please choose " CPU_CACHE ", " MISS_ANALYZER ", " TLB ", " HISTOGRAM
                   ", " REUSE_DIST ", " BASIC_COUNTS ", " OPCODE_MIX ", " SYSCALL_MIX
                   ", " VIEW ", " FUNC_VIEW ", or some external analyzer.\n",
                   simulator_type.c_str());
        }
        return tool;
    }
}

/******************************************************************************
 * Specializations for analyzer_multi_tmpl_t<trace_entry_t, record_reader_t>, aka
 * record_analyzer_multi_t.
 */

template <>
std::unique_ptr<record_reader_t>
record_analyzer_multi_t::create_ipc_reader(const char *name, int verbose)
{
    error_string_ = "Online analysis is not supported for record_filter";
    ERRMSG("%s\n", error_string_.c_str());
    return std::unique_ptr<record_reader_t>();
}

template <>
std::unique_ptr<record_reader_t>
record_analyzer_multi_t::create_ipc_reader_end()
{
    error_string_ = "Online analysis is not supported for record_filter";
    ERRMSG("%s\n", error_string_.c_str());
    return std::unique_ptr<record_reader_t>();
}

template <>
record_analysis_tool_t *
record_analyzer_multi_t::create_external_tool(const std::string &tool_name)
{
    error_string_ = "External tools are not supported for record analysis";
    ERRMSG("%s\n", error_string_.c_str());
    return nullptr;
}

template <>
record_analysis_tool_t *
record_analyzer_multi_t::create_invariant_checker()
{
    error_string_ = "Invariant checker is not supported for record analysis";
    ERRMSG("%s\n", error_string_.c_str());
    return nullptr;
}

template <>
record_analysis_tool_t *
record_analyzer_multi_t::create_analysis_tool_from_options(
    const std::string &simulator_type)
{
    if (simulator_type == RECORD_FILTER) {
        return record_filter_tool_create(
            op_outdir.get_value(), op_filter_stop_timestamp.get_value(),
            op_filter_cache_size.get_value(), op_filter_trace_types.get_value(),
            op_filter_marker_types.get_value(), op_trim_before_timestamp.get_value(),
            op_trim_after_timestamp.get_value(), op_verbose.get_value());
    }
    ERRMSG("Usage error: unsupported record analyzer type \"%s\".  Only " RECORD_FILTER
           " is supported.\n",
           simulator_type.c_str());
    return nullptr;
}

/********************************************************************
 * Other analyzer_multi_tmpl_t routines that do not need to be specialized.
 */

template <typename RecordType, typename ReaderType>
analyzer_multi_tmpl_t<RecordType, ReaderType>::analyzer_multi_tmpl_t()
{
    this->worker_count_ = op_jobs.get_value();
    this->skip_instrs_ = op_skip_instrs.get_value();
    this->interval_microseconds_ = op_interval_microseconds.get_value();
    this->interval_instr_count_ = op_interval_instr_count.get_value();
    // Initial measurements show it's sometimes faster to keep the parallel model
    // of using single-file readers but use them sequentially, as opposed to
    // the every-file interleaving reader, but the user can specify -jobs 1, so
    // we still keep the serial vs parallel split for 0.
    if (this->worker_count_ == 0)
        this->parallel_ = false;
    if (!op_indir.get_value().empty() || !op_infile.get_value().empty())
        op_offline.set_value(true); // Some tools check this on post-proc runs.
    // XXX: add a "required" flag to droption to avoid needing this here
    if (op_indir.get_value().empty() && op_infile.get_value().empty() &&
        op_ipc_name.get_value().empty()) {
        this->error_string_ =
            "Usage error: -ipc_name or -indir or -infile is required\nUsage:\n" +
            droption_parser_t::usage_short(DROPTION_SCOPE_ALL);
        this->success_ = false;
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
                dir.initialize(op_indir.get_value(), "", op_trace_compress.get_value(),
                               op_syscall_template_file.get_value());
            if (!dir_err.empty()) {
                this->success_ = false;
                this->error_string_ = "Directory setup failed: " + dir_err;
                return;
            }
            raw2trace_t raw2trace(
                dir.modfile_bytes_, dir.in_files_, dir.out_files_, dir.out_archives_,
                dir.encoding_file_, dir.serial_schedule_file_, dir.cpu_schedule_file_,
                nullptr, op_verbose.get_value(), op_jobs.get_value(),
                op_alt_module_dir.get_value(), op_chunk_instr_count.get_value(),
                dir.in_kfiles_map_, dir.kcoredir_, dir.kallsymsdir_,
                std::move(dir.syscall_template_file_reader_));
            std::string error = raw2trace.do_conversion();
            if (!error.empty()) {
                this->success_ = false;
                this->error_string_ = "raw2trace failed: " + error;
            }
        }
    }
    // Create the tools after post-processing so we have the schedule files for
    // test_mode.
    if (!create_analysis_tools()) {
        this->success_ = false;
        this->error_string_ = "Failed to create analysis tool:" + this->error_string_;
        return;
    }

    typename sched_type_t::scheduler_options_t sched_ops;
    if (op_core_sharded.get_value() || op_core_serial.get_value()) {
        if (op_core_serial.get_value()) {
            this->parallel_ = false;
        }
        sched_ops = init_dynamic_schedule();
    }

    if (!op_indir.get_value().empty()) {
        std::string tracedir =
            raw2trace_directory_t::tracedir_from_rawdir(op_indir.get_value());
        if (!this->init_scheduler(tracedir, op_only_thread.get_value(),
                                  op_verbose.get_value(), std::move(sched_ops)))
            this->success_ = false;
    } else if (op_infile.get_value().empty()) {
        // XXX i#3323: Add parallel analysis support for online tools.
        this->parallel_ = false;
        auto reader =
            create_ipc_reader(op_ipc_name.get_value().c_str(), op_verbose.get_value());
        if (!reader) {
            this->error_string_ = "Failed to create IPC reader: " + this->error_string_;
            this->success_ = false;
            return;
        }
        auto end = create_ipc_reader_end();
        // We do not want the scheduler's init() to block.
        sched_ops.read_inputs_in_init = false;
        if (!this->init_scheduler(std::move(reader), std::move(end),
                                  op_verbose.get_value(), std::move(sched_ops))) {
            this->success_ = false;
        }
    } else {
        // Legacy file.
        if (!this->init_scheduler(op_infile.get_value(),
                                  INVALID_THREAD_ID /*all threads*/,
                                  op_verbose.get_value(), std::move(sched_ops)))
            this->success_ = false;
    }
    if (!init_analysis_tools()) {
        this->success_ = false;
        return;
    }
    // We can't call serial_trace_iter_->init() here as it blocks for ipc_reader_t.
}

template <typename RecordType, typename ReaderType>
analyzer_multi_tmpl_t<RecordType, ReaderType>::~analyzer_multi_tmpl_t()
{

#ifdef HAS_ZIP
    if (!op_record_file.get_value().empty()) {
        if (this->scheduler_.write_recorded_schedule() != sched_type_t::STATUS_SUCCESS) {
            ERRMSG("Failed to write schedule to %s", op_record_file.get_value().c_str());
        }
    }
#endif
    destroy_analysis_tools();
}

template <typename RecordType, typename ReaderType>
typename scheduler_tmpl_t<RecordType, ReaderType>::scheduler_options_t
analyzer_multi_tmpl_t<RecordType, ReaderType>::init_dynamic_schedule()
{
    this->shard_type_ = SHARD_BY_CORE;
    this->worker_count_ = op_num_cores.get_value();
    typename sched_type_t::scheduler_options_t sched_ops(
        sched_type_t::MAP_TO_ANY_OUTPUT,
        op_sched_order_time.get_value() ? sched_type_t::DEPENDENCY_TIMESTAMPS
                                        : sched_type_t::DEPENDENCY_IGNORE,
        sched_type_t::SCHEDULER_DEFAULTS, op_verbose.get_value());
    sched_ops.quantum_duration = op_sched_quantum.get_value();
    if (op_sched_time.get_value())
        sched_ops.quantum_unit = sched_type_t::QUANTUM_TIME;
    sched_ops.syscall_switch_threshold = op_sched_syscall_switch_us.get_value();
    sched_ops.blocking_switch_threshold = op_sched_blocking_switch_us.get_value();
    sched_ops.block_time_scale = op_sched_block_scale.get_value();
    sched_ops.block_time_max = op_sched_block_max_us.get_value();
    sched_ops.randomize_next_input = op_sched_randomize.get_value();
#ifdef HAS_ZIP
    if (!op_record_file.get_value().empty()) {
        record_schedule_zip_.reset(new zipfile_ostream_t(op_record_file.get_value()));
        sched_ops.schedule_record_ostream = record_schedule_zip_.get();
    } else if (!op_replay_file.get_value().empty()) {
        replay_schedule_zip_.reset(new zipfile_istream_t(op_replay_file.get_value()));
        sched_ops.schedule_replay_istream = replay_schedule_zip_.get();
        sched_ops.mapping = sched_type_t::MAP_AS_PREVIOUSLY;
        sched_ops.deps = sched_type_t::DEPENDENCY_TIMESTAMPS;
    } else if (!op_cpu_schedule_file.get_value().empty()) {
        cpu_schedule_zip_.reset(new zipfile_istream_t(op_cpu_schedule_file.get_value()));
        sched_ops.mapping = sched_type_t::MAP_TO_RECORDED_OUTPUT;
        sched_ops.deps = sched_type_t::DEPENDENCY_TIMESTAMPS;
        sched_ops.replay_as_traced_istream = cpu_schedule_zip_.get();
    }
#endif
    sched_ops.kernel_switch_trace_path = op_sched_switch_file.get_value();
    return sched_ops;
}

template <typename RecordType, typename ReaderType>
bool
analyzer_multi_tmpl_t<RecordType, ReaderType>::create_analysis_tools()
{
    this->tools_ = new analysis_tool_tmpl_t<RecordType> *[this->max_num_tools_];
    if (!op_simulator_type.get_value().empty()) {
        std::stringstream stream(op_simulator_type.get_value());
        std::string type;
        while (std::getline(stream, type, ':')) {
            if (this->num_tools_ >= this->max_num_tools_ - 1) {
                this->error_string_ = "Only " + std::to_string(this->max_num_tools_ - 1) +
                    " simulators are allowed simultaneously";
                return false;
            }
            auto tool = create_analysis_tool_from_options(type);
            if (tool == NULL)
                continue;
            if (!*tool) {
                std::string tool_error = tool->get_error_string();
                if (tool_error.empty())
                    tool_error = "no error message provided.";
                this->error_string_ = "Tool failed to initialize: " + tool_error;
                delete tool;
                return false;
            }
            this->tools_[this->num_tools_++] = tool;
        }
    }

    if (op_test_mode.get_value()) {
        // This will return nullptr for record_ instantiation; we just don't support
        // -test_mode for record_.
        this->tools_[this->num_tools_] = create_invariant_checker();
        if (this->tools_[this->num_tools_] == NULL)
            return false;
        if (!*this->tools_[this->num_tools_]) {
            this->error_string_ = this->tools_[this->num_tools_]->get_error_string();
            delete this->tools_[this->num_tools_];
            this->tools_[this->num_tools_] = NULL;
            return false;
        }
        this->num_tools_++;
    }

    return (this->num_tools_ != 0);
}

template <typename RecordType, typename ReaderType>
bool
analyzer_multi_tmpl_t<RecordType, ReaderType>::init_analysis_tools()
{
    // initialize_stream() is now called from analyzer_t::run().
    return true;
}

template <typename RecordType, typename ReaderType>
void
analyzer_multi_tmpl_t<RecordType, ReaderType>::destroy_analysis_tools()
{
    if (!this->success_)
        return;
    for (int i = 0; i < this->num_tools_; i++)
        delete this->tools_[i];
    delete[] this->tools_;
}

/* Get the path to an auxiliary file by examining
 * 1. The corresponding command line option
 * 2. The trace directory
 * If a trace file is provided instead of a trace directory, it searches in the
 * directory which contains the trace file.
 */
template <typename RecordType, typename ReaderType>
std::string
analyzer_multi_tmpl_t<RecordType, ReaderType>::get_aux_file_path(
    std::string option_val, std::string default_filename)
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

template <typename RecordType, typename ReaderType>
std::string
analyzer_multi_tmpl_t<RecordType, ReaderType>::get_module_file_path()
{
    return get_aux_file_path(op_module_file.get_value(), DRMEMTRACE_MODULE_LIST_FILENAME);
}

/* Get the cache simulator knobs used by the cache simulator
 * and the cache miss analyzer.
 */
template <typename RecordType, typename ReaderType>
cache_simulator_knobs_t *
analyzer_multi_tmpl_t<RecordType, ReaderType>::get_cache_simulator_knobs()
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

template class analyzer_multi_tmpl_t<memref_t, reader_t>;
template class analyzer_multi_tmpl_t<trace_entry_t,
                                     dynamorio::drmemtrace::record_reader_t>;

} // namespace drmemtrace
} // namespace dynamorio
