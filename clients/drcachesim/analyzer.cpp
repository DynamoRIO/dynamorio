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

#include <inttypes.h>
#include <iostream>
#include <thread>
#include "analysis_tool.h"
#include "analyzer.h"
#include "reader/file_reader.h"
#ifdef HAS_ZLIB
#    include "reader/compressed_file_reader.h"
#endif
#ifdef HAS_ZIP
#    include "reader/zipfile_file_reader.h"
#endif
#ifdef HAS_SNAPPY
#    include "reader/snappy_file_reader.h"
#endif
#include "common/utils.h"
#include "memtrace_stream.h"

#ifdef HAS_ZLIB
// Even if the file is uncompressed, zlib's gzip interface is faster than
// file_reader_t's fstream in our measurements, so we always use it when
// available.
typedef compressed_file_reader_t default_file_reader_t;
typedef compressed_record_file_reader_t default_record_file_reader_t;
#else
typedef file_reader_t<std::ifstream *> default_file_reader_t;
typedef dynamorio::drmemtrace::record_file_reader_t<std::ifstream>
    default_record_file_reader_t;
#endif

/****************************************************************
 * Specializations for analyzer_tmpl_t<reader_t>, aka analyzer_t.
 */

template <>
bool
analyzer_t::serial_mode_supported()
{
    return true;
}

template <>
bool
analyzer_t::record_has_tid(memref_t record, memref_tid_t &tid)
{
    // All memref_t records have tids (after PR #5739 changed the reader).
    tid = record.marker.tid;
    return true;
}

template <>
bool
analyzer_t::record_is_thread_final(memref_t record)
{
    return record.exit.type == TRACE_TYPE_THREAD_EXIT;
}

/******************************************************************************
 * Specializations for analyzer_tmpl_t<record_reader_t>, aka record_analyzer_t.
 */

template <>
bool
record_analyzer_t::serial_mode_supported()
{
    // TODO i#5727,i#5843: Once we move serial interleaving from file_reader_t into
    // the scheduler we can support serial mode for record files as we won't need
    // to implement interleaving inside record_file_reader_t.
    return false;
}

template <>
bool
record_analyzer_t::record_has_tid(trace_entry_t record, memref_tid_t &tid)
{
    if (record.type != TRACE_TYPE_THREAD)
        return false;
    tid = static_cast<memref_tid_t>(record.addr);
    return true;
}

template <>
bool
record_analyzer_t::record_is_thread_final(trace_entry_t record)
{
    return record.type == TRACE_TYPE_FOOTER;
}

/********************************************************************
 * Other analyzer_tmpl_t routines that do not need to be specialized.
 */

template <typename RecordType, typename ReaderType>
analyzer_tmpl_t<RecordType, ReaderType>::analyzer_tmpl_t()
    : success_(true)
    , num_tools_(0)
    , tools_(NULL)
    , parallel_(true)
    , worker_count_(0)
{
    /* Nothing else: child class needs to initialize. */
}

template <typename RecordType, typename ReaderType>
bool
analyzer_tmpl_t<RecordType, ReaderType>::init_scheduler(const std::string &trace_path,
                                                        memref_tid_t only_thread,
                                                        int verbosity)
{
    verbosity_ = verbosity;
    if (trace_path.empty()) {
        ERRMSG("Trace file name is empty\n");
        return false;
    }
    std::vector<typename sched_type_t::range_t> regions;
    if (skip_instrs_ > 0) {
        // TODO i#5843: For serial mode with multiple inputs this is not doing the
        // right thing: this is skipping in every input stream, while the documented
        // behavior is supposed to be an output stream skip.  Once we have that
        // capability in the scheduler we should switch to that.
        regions.emplace_back(skip_instrs_ + 1, 0);
    }
    typename sched_type_t::input_workload_t workload(trace_path, regions);
    if (only_thread != INVALID_THREAD_ID) {
        workload.only_threads.insert(only_thread);
    }
    return init_scheduler_common(workload);
}

template <typename RecordType, typename ReaderType>
bool
analyzer_tmpl_t<RecordType, ReaderType>::init_scheduler(
    std::unique_ptr<ReaderType> reader, std::unique_ptr<ReaderType> reader_end,
    int verbosity)
{
    verbosity_ = verbosity;
    if (!reader || !reader_end) {
        ERRMSG("Readers are empty\n");
        return false;
    }
    std::vector<typename sched_type_t::input_reader_t> readers;
    // With no modifiers or only_threads the tid doesn't matter.
    readers.emplace_back(std::move(reader), std::move(reader_end), /*tid=*/1);
    std::vector<typename sched_type_t::range_t> regions;
    if (skip_instrs_ > 0)
        regions.emplace_back(skip_instrs_ + 1, 0);
    typename sched_type_t::input_workload_t workload(std::move(readers), regions);
    return init_scheduler_common(workload);
}

template <typename RecordType, typename ReaderType>
bool
analyzer_tmpl_t<RecordType, ReaderType>::init_scheduler_common(
    typename sched_type_t::input_workload_t &workload)
{
    for (int i = 0; i < num_tools_; ++i) {
        if (parallel_ && !tools_[i]->parallel_shard_supported()) {
            parallel_ = false;
            break;
        }
    }
    std::vector<typename sched_type_t::input_workload_t> sched_inputs(1);
    sched_inputs[0] = std::move(workload);
    typename sched_type_t::scheduler_options_t sched_ops;
    int output_count;
    if (parallel_) {
        sched_ops = sched_type_t::make_scheduler_parallel_options(verbosity_);
        if (worker_count_ <= 0)
            worker_count_ = std::thread::hardware_concurrency();
    } else {
        sched_ops = sched_type_t::make_scheduler_serial_options(verbosity_);
        worker_count_ = 1;
    }
    output_count = worker_count_;
    if (scheduler_.init(sched_inputs, output_count, sched_ops) !=
        sched_type_t::STATUS_SUCCESS) {
        ERRMSG("Failed to initialize scheduler: %s\n",
               scheduler_.get_error_string().c_str());
        return false;
    }

    for (int i = 0; i < worker_count_; ++i) {
        worker_data_.push_back(analyzer_worker_data_t(i, scheduler_.get_stream(i)));
    }

    return true;
}

template <typename RecordType, typename ReaderType>
analyzer_tmpl_t<RecordType, ReaderType>::analyzer_tmpl_t(
    const std::string &trace_path, analysis_tool_tmpl_t<RecordType> **tools,
    int num_tools, int worker_count, uint64_t skip_instrs, uint64_t quantum_microseconds,
    int verbosity)
    : success_(true)
    , num_tools_(num_tools)
    , tools_(tools)
    , parallel_(true)
    , worker_count_(worker_count)
    , skip_instrs_(skip_instrs)
    , quantum_microseconds_(quantum_microseconds)
    , verbosity_(verbosity)
{
    // The scheduler will call reader_t::init() for each input file.  We assume
    // that won't block (analyzer_multi_t separates out IPC readers).
    if (!init_scheduler(trace_path, verbosity)) {
        success_ = false;
        error_string_ = "Failed to create scheduler";
        return;
    }
    for (int i = 0; i < num_tools; ++i) {
        if (tools_[i] == NULL || !*tools_[i]) {
            success_ = false;
            error_string_ = "Tool is not successfully initialized";
            if (tools_[i] != NULL)
                error_string_ += ": " + tools_[i]->get_error_string();
            return;
        }
    }
}

// Work around clang-format bug: no newline after return type for single-char operator.
// clang-format off
template <typename RecordType, typename ReaderType>
// clang-format on
analyzer_tmpl_t<RecordType, ReaderType>::~analyzer_tmpl_t()
{
    // Empty.
}

template <typename RecordType, typename ReaderType>
// Work around clang-format bug: no newline after return type for single-char operator.
// clang-format off
bool
analyzer_tmpl_t<RecordType,ReaderType>::operator!()
// clang-format on
{
    return !success_;
}

template <typename RecordType, typename ReaderType>
std::string
analyzer_tmpl_t<RecordType, ReaderType>::get_error_string()
{
    return error_string_;
}

template <typename RecordType, typename ReaderType>
void
analyzer_tmpl_t<RecordType, ReaderType>::process_serial(analyzer_worker_data_t &worker)
{
    std::vector<void *> user_worker_data(num_tools_);

    for (int i = 0; i < num_tools_; ++i) {
        worker.error = tools_[i]->initialize_stream(worker.stream);
        if (!worker.error.empty())
            return;
    }
    while (true) {
        RecordType record;
        if (quantum_microseconds_ != 0) {
            int next_quantum_index = (worker.stream->get_last_timestamp() -
                                      worker.stream->get_first_timestamp()) /
                quantum_microseconds_;
            if (next_quantum_index != worker.cur_quantum_index) {
                assert(next_quantum_index > worker.cur_quantum_index);
                if (!process_quantum(worker.cur_quantum_index, &worker))
                    return;
                worker.cur_quantum_index = next_quantum_index;
            }
        }
        typename sched_type_t::stream_status_t status =
            worker.stream->next_record(record);
        if (status != sched_type_t::STATUS_OK) {
            if (status != sched_type_t::STATUS_EOF) {
                if (status == sched_type_t::STATUS_REGION_INVALID) {
                    worker.error =
                        "Too-far -skip_instrs for: " + worker.stream->get_stream_name();
                } else {
                    worker.error =
                        "Failed to read from trace: " + worker.stream->get_stream_name();
                }
            }
            if (quantum_microseconds_ != 0 &&
                !process_quantum(worker.cur_quantum_index, &worker))
                return;
            return;
        }
        for (int i = 0; i < num_tools_; ++i) {
            if (!tools_[i]->process_memref(record)) {
                worker.error = tools_[i]->get_error_string();
                VPRINT(this, 1, "Worker %d hit memref error %s on trace shard %s\n",
                       worker.index, worker.error.c_str(),
                       worker.stream->get_stream_name().c_str());
                return;
            }
        }
    }
}

template <typename RecordType, typename ReaderType>
void
analyzer_tmpl_t<RecordType, ReaderType>::process_tasks(analyzer_worker_data_t *worker)
{
    std::vector<void *> user_worker_data(num_tools_);

    for (int i = 0; i < num_tools_; ++i)
        user_worker_data[i] = tools_[i]->parallel_worker_init(worker->index);
    RecordType record;
    for (typename sched_type_t::stream_status_t status =
             worker->stream->next_record(record);
         status != sched_type_t::STATUS_EOF;
         status = worker->stream->next_record(record)) {
        if (status != sched_type_t::STATUS_OK) {
            if (status == sched_type_t::STATUS_REGION_INVALID) {
                worker->error =
                    "Too-far -skip_instrs for: " + worker->stream->get_stream_name();
            } else {
                worker->error =
                    "Failed to read from trace: " + worker->stream->get_stream_name();
            }
            return;
        }
        int shard_index = worker->stream->get_input_stream_ordinal();
        if (shard_data_.find(shard_index) == shard_data_.end()) {
            VPRINT(this, 1, "Worker %d starting on trace shard %d stream is %p\n",
                   worker->index, shard_index, worker->stream);
            shard_data_[shard_index].resize(num_tools_);
            for (int i = 0; i < num_tools_; ++i) {
                shard_data_[shard_index][i] = tools_[i]->parallel_shard_init_stream(
                    shard_index, user_worker_data[i], worker->stream);
            }
            worker->cur_quantum_index = 0;
        }
        if (quantum_microseconds_ != 0) {
            int next_quantum_index = (worker->stream->get_last_timestamp() -
                                      worker->stream->get_first_timestamp()) /
                quantum_microseconds_;
            if (next_quantum_index != worker->cur_quantum_index) {
                assert(next_quantum_index > worker->cur_quantum_index);
                if (!process_shard_quantum(shard_index, worker->cur_quantum_index,
                                           worker))
                    return;
                worker->cur_quantum_index = next_quantum_index;
            }
        }
        for (int i = 0; i < num_tools_; ++i) {
            if (!tools_[i]->parallel_shard_memref(shard_data_[shard_index][i], record)) {
                worker->error =
                    tools_[i]->parallel_shard_error(shard_data_[shard_index][i]);
                VPRINT(this, 1, "Worker %d hit shard memref error %s on trace shard %s\n",
                       worker->index, worker->error.c_str(),
                       worker->stream->get_stream_name().c_str());
                return;
            }
        }
        if (record_is_thread_final(record)) {
            VPRINT(this, 1, "Worker %d finished trace shard %s\n", worker->index,
                   worker->stream->get_stream_name().c_str());
            if (quantum_microseconds_ != 0 &&
                !process_shard_quantum(shard_index, worker->cur_quantum_index, worker))
                return;
            for (int i = 0; i < num_tools_; ++i) {
                if (!tools_[i]->parallel_shard_exit(shard_data_[shard_index][i])) {
                    worker->error =
                        tools_[i]->parallel_shard_error(shard_data_[shard_index][i]);
                    VPRINT(this, 1,
                           "Worker %d hit shard exit error %s on trace shard %s\n",
                           worker->index, worker->error.c_str(),
                           worker->stream->get_stream_name().c_str());
                    return;
                }
            }
        }
    }
    for (int i = 0; i < num_tools_; ++i) {
        const std::string error = tools_[i]->parallel_worker_exit(user_worker_data[i]);
        if (!error.empty()) {
            worker->error = error;
            VPRINT(this, 1, "Worker %d hit worker exit error %s\n", worker->index,
                   error.c_str());
            return;
        }
    }
}

template <typename RecordType, typename ReaderType>
bool
analyzer_tmpl_t<RecordType, ReaderType>::run()
{
    // XXX i#3286: Add a %-completed progress message by looking at the file sizes.
    if (!parallel_) {
        process_serial(worker_data_[0]);
        if (!worker_data_[0].error.empty()) {
            error_string_ = worker_data_[0].error;
            return false;
        }
        return true;
    }
    if (worker_count_ <= 0) {
        error_string_ = "Invalid worker count: must be > 0";
        return false;
    }
    for (int i = 0; i < num_tools_; ++i) {
        error_string_ = tools_[i]->initialize_stream(nullptr);
        if (!error_string_.empty())
            return false;
    }
    std::vector<std::thread> threads;
    VPRINT(this, 1, "Creating %d worker threads\n", worker_count_);
    threads.reserve(worker_count_);
    for (int i = 0; i < worker_count_; ++i) {
        threads.emplace_back(
            std::thread(&analyzer_tmpl_t::process_tasks, this, &worker_data_[i]));
    }
    for (std::thread &thread : threads)
        thread.join();
    for (auto &worker : worker_data_) {
        if (!worker.error.empty()) {
            error_string_ = worker.error;
            return false;
        }
    }
    return true;
}

template <typename RecordType, typename ReaderType>
bool
analyzer_tmpl_t<RecordType, ReaderType>::print_stats()
{
    for (int i = 0; i < num_tools_; ++i) {
        // Each tool should reset i/o state, but we reset the format here just in case.
        std::cerr << std::dec;
        if (!tools_[i]->print_results()) {
            error_string_ = tools_[i]->get_error_string();
            return false;
        }
        if (i + 1 < num_tools_) {
            // Separate tool output.
            std::cerr << "\n=========================================================="
                         "=================\n";
        }
    }
    return true;
}

template <typename RecordType, typename ReaderType>
bool
analyzer_tmpl_t<RecordType, ReaderType>::process_quantum(int quantum_id,
                                                         analyzer_worker_data_t *worker)
{
    for (int i = 0; i < num_tools_; ++i) {
        if (!tools_[i]->notify_quantum_end(quantum_id)) {
            worker->error = tools_[i]->get_error_string();
            VPRINT(this, 1, "Worker %d hit process_quantum error %s on trace shard %s\n",
                   worker->index, worker->error.c_str(),
                   worker->stream->get_stream_name().c_str());
            return false;
        }
    }
    return true;
}

template <typename RecordType, typename ReaderType>
bool
analyzer_tmpl_t<RecordType, ReaderType>::process_shard_quantum(
    int shard_id, int quantum_id, analyzer_worker_data_t *worker)
{
    for (int i = 0; i < num_tools_; ++i) {
        if (!tools_[i]->parallel_shard_quantum_end(shard_data_[shard_id][i],
                                                   quantum_id)) {
            worker->error = tools_[i]->get_error_string();
            VPRINT(this, 1,
                   "Worker %d hit process_shard_quantum error %s on trace shard %s\n",
                   worker->index, worker->error.c_str(),
                   worker->stream->get_stream_name().c_str());
            return false;
        }
    }
    return true;
}

template class analyzer_tmpl_t<memref_t, reader_t>;
template class analyzer_tmpl_t<trace_entry_t, dynamorio::drmemtrace::record_reader_t>;
