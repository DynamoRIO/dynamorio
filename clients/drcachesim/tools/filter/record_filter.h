/* **********************************************************
 * Copyright (c) 2022-2024 Google, Inc.  All rights reserved.
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

#ifndef _RECORD_FILTER_H_
#define _RECORD_FILTER_H_ 1

#include <stdint.h>

#include <condition_variable>
#include <memory>
#include <mutex>
#include <ostream>
#include <string>
#include <unordered_map>
#include <vector>

#include "analysis_tool.h"
#include "archive_ostream.h"
#include "memref.h"
#include "memtrace_stream.h"
#include "raw2trace_shared.h"
#include "schedule_file.h"
#include "trace_entry.h"

namespace dynamorio {
namespace drmemtrace {

/**
 * Analysis tool that filters the #trace_entry_t records of an offline
 * trace. Streams through each shard independenty and parallelly, and
 * writes the filtered version to the output directory with the same
 * base name. Serial mode is not yet supported.
 */
class record_filter_t : public record_analysis_tool_t {
public:
    /**
     * Interface for the record_filter to share data with its filters.
     */
    struct record_filter_info_t {
        /**
         * Stores the encoding of an instructions, which may be split among more than one
         * #trace_entry_t, hence the vector.
         */
        std::vector<trace_entry_t> *last_encoding;

        /**
         * Gives filters access to dcontext_t.
         * Note that dcontext_t is not entirely thread-safe. AArch32 encoding and
         * decoding is problematic as the global encode_state_t and decode_state_t are
         * used for GLOBAL_DCONTEXT. Furthermore, modifying the ISA mode can lead to data
         * races.
         */
        /* xref i#6690 i#1595: multi-dcontext_t solution.
         */
        void *dcontext;
    };

    /**
     * The base class for a single filter.
     */
    class record_filter_func_t {
    public:
        record_filter_func_t()
        {
        }
        virtual ~record_filter_func_t()
        {
        }
        /**
         * Invoked for each shard prior to calling parallel_shard_filter() on
         * any entry. The returned pointer is passed to all invocations of
         * parallel_shard_filter() and parallel_shard_exit().
         * This routine can be used to initialize state for each shard.
         * \p partial_trace_filter denotes whether the trace will be filtered
         * only partially, e.g. due to stop_timestamp.
         */
        virtual void *
        parallel_shard_init(memtrace_stream_t *shard_stream,
                            bool partial_trace_filter) = 0;
        /**
         * Invoked for each #trace_entry_t in the shard. It returns
         * whether or not this \p entry should be included in the result
         * trace. \p shard_data is same as what was returned by parallel_shard_init().
         * The given \p entry is included in the result trace iff all provided
         * #dynamorio::drmemtrace::record_filter_t::record_filter_func_t return true.
         * The \p entry parameter can also be modified by the record_filter_func_t.
         * The passed \p entry is not guaranteed to be the original one from
         * the trace if other filter tools are present, and may include changes
         * made by other tools.
         * An error is indicated by setting error_string_ to a non-empty value.
         * \p record_filter_info is the interface used by record_filter to
         * share data with its filters.
         */
        virtual bool
        parallel_shard_filter(trace_entry_t &entry, void *shard_data,
                              record_filter_info_t &record_filter_info) = 0;
        /**
         * Invoked when all #trace_entry_t in a shard have been processed
         * by parallel_shard_filter(). \p shard_data is same as what was
         * returned by parallel_shard_init().
         */
        virtual bool
        parallel_shard_exit(void *shard_data) = 0;

        /**
         * Returns the error string. If no error occurred, it will be empty.
         */
        std::string
        get_error_string()
        {
            return error_string_;
        }

        /**
         * If a filter modifies the file type of a trace, its changes should be made here,
         * so they are visible to the record_filter even if the #trace_entry_t containing
         * the file type marker is not modified directly by the filter.
         */
        virtual uint64_t
        update_filetype(uint64_t filetype)
        {
            return filetype;
        }

    protected:
        std::string error_string_;
    };

    // stop_timestamp sets a point beyond which no filtering will occur.
    record_filter_t(const std::string &output_dir,
                    std::vector<std::unique_ptr<record_filter_func_t>> filters,
                    uint64_t stop_timestamp, unsigned int verbose);
    ~record_filter_t() override;
    std::string
    initialize_stream(memtrace_stream_t *serial_stream) override;
    bool
    process_memref(const trace_entry_t &entry) override;
    bool
    print_results() override;
    bool
    parallel_shard_supported() override;
    std::string
    initialize_shard_type(shard_type_t shard_type) override;
    void *
    parallel_shard_init_stream(int shard_index, void *worker_data,
                               memtrace_stream_t *shard_stream) override;
    bool
    parallel_shard_exit(void *shard_data) override;
    bool
    parallel_shard_memref(void *shard_data, const trace_entry_t &entry) override;
    std::string
    parallel_shard_error(void *shard_data) override;

    // Automatically called from print_results().
    // Calls open_serial_schedule_file() and open_cpu_schedule_file() and then
    // writes out the file contents.
    std::string
    write_schedule_files();

protected:
    struct dcontext_cleanup_last_t {
    public:
        ~dcontext_cleanup_last_t()
        {
            if (dcontext != nullptr)
                dr_standalone_exit();
        }
        void *dcontext = nullptr;
    };

    // For core-sharded we need to remember encodings for an input that were
    // seen on a different core, as there is no reader_t remembering them for us.
    // XXX i#6635: Is this something the scheduler should help us with?
    struct per_input_t {
        // There should be no contention on the lock as each input is on
        // just one core at a time.
        std::mutex lock;
        std::unordered_map<addr_t, std::vector<trace_entry_t>> pc2encoding;
    };

    struct per_shard_t {
        std::string output_path;
        // One and only one of these writers can be valid.
        std::unique_ptr<std::ostream> file_writer;
        std::unique_ptr<archive_ostream_t> archive_writer;
        // This points to one of the writers.
        std::ostream *writer = nullptr;
        std::string error;
        std::vector<void *> filter_shard_data;
        std::unordered_map<uint64_t, std::vector<trace_entry_t>> delayed_encodings;
        std::vector<trace_entry_t> last_encoding;
        uint64_t input_entry_count;
        uint64_t output_entry_count;
        memtrace_stream_t *shard_stream;
        bool enabled;
        // For re-chunking archive files.
        uint64_t chunk_ordinal = 0;
        uint64_t chunk_size = 0;
        uint64_t cur_chunk_instrs = 0;
        uint64_t cur_refs = 0;
        uint64_t input_count_at_ordinal = 0;
        memref_counter_t memref_counter;
        addr_t last_timestamp = 0;
        addr_t last_cpu_id = 0;
        std::unordered_set<addr_t> cur_chunk_pcs;
        bool prev_was_output = false;
        addr_t filetype = 0;
        bool now_empty = false;
        // For thread-sharded.
        memref_tid_t tid = 0;
        int64_t prev_workload_id = -1;
        // For core-sharded.
        int64_t prev_input_id = -1;
        trace_entry_t last_written_record;
        // Cached value updated on context switches.
        per_input_t *per_input = nullptr;
        record_filter_info_t record_filter_info;
        schedule_file_t::per_shard_t sched_info;
    };

    virtual std::string
    open_new_chunk(per_shard_t *shard);

    std::string
    emit_marker(per_shard_t *shard, unsigned short marker_type, uint64_t marker_value);

    virtual std::string
    remove_output_file(per_shard_t *per_shard);

    std::string
    process_markers(per_shard_t *per_shard, trace_entry_t &entry, bool &output);

    std::string
    process_chunk_encodings(per_shard_t *per_shard, trace_entry_t &entry, bool output);

    std::string
    process_delayed_encodings(per_shard_t *per_shard, trace_entry_t &entry, bool output);

    // Computes the output path without the extension output_ext_ which is added
    // separately after determining the input path extension.
    virtual std::string
    get_output_basename(memtrace_stream_t *shard_stream);

    dcontext_cleanup_last_t dcontext_;

    std::unordered_map<int, per_shard_t *> shard_map_;
    // This mutex is only needed in parallel_shard_init. In all other accesses
    // to shard_map (print_results) we are single-threaded.
    std::mutex shard_map_mutex_;
    shard_type_t shard_type_ = SHARD_BY_THREAD;

    // For core-sharded we don't have a 1:1 input:output file mapping.
    // Thus, some shards may not have an input stream at init time, and
    // need to figure out their file extension and header info from other shards.
    std::mutex input_info_mutex_;
    std::condition_variable input_info_cond_var_;
    // The above locks guard these fields:
    std::string output_ext_;
    uint64_t version_ = 0;
    uint64_t filetype_ = 0;
    std::unique_ptr<std::ostream> serial_schedule_file_;
    std::ostream *serial_schedule_ostream_ = nullptr;
    std::unique_ptr<archive_ostream_t> cpu_schedule_file_;
    archive_ostream_t *cpu_schedule_ostream_ = nullptr;

private:
    virtual bool
    write_trace_entry(per_shard_t *shard, const trace_entry_t &entry);

    // Sets one of file_writer or archive_writer, along with writer, in per_shard.
    // Returns "" or an error string.
    virtual std::string
    get_writer(per_shard_t *per_shard, memtrace_stream_t *shard_stream);

    // Sets output_path plus cross-shard output_ext_, version_, filetype_.
    virtual std::string
    initialize_shard_output(per_shard_t *per_shard, memtrace_stream_t *shard_stream);

    // Sets serial_schedule_ostream_, optionally using serial_schedule_file_.
    virtual std::string
    open_serial_schedule_file();

    // Sets cpu_schedule_ostream_, optionally using cpu_schedule_file_.
    virtual std::string
    open_cpu_schedule_file();

    bool
    write_trace_entries(per_shard_t *shard, const std::vector<trace_entry_t> &entries);

    inline uint64_t
    add_to_filetype(uint64_t filetype)
    {
        if (stop_timestamp_ != 0)
            filetype |= OFFLINE_FILE_TYPE_BIMODAL_FILTERED_WARMUP;
        if (shard_type_ == SHARD_BY_CORE)
            filetype |= OFFLINE_FILE_TYPE_CORE_SHARDED;
        /* If filters modify the file type, add their changes here.
         */
        for (auto &filter : filters_) {
            filetype = filter->update_filetype(filetype);
        }
        return filetype;
    }

    std::string output_dir_;
    std::vector<std::unique_ptr<record_filter_func_t>> filters_;
    uint64_t stop_timestamp_;
    unsigned int verbosity_;
    const char *output_prefix_ = "[record_filter]";
    // For core-sharded, but used for thread-sharded to simplify the code.
    std::mutex input2info_mutex_;
    // We use a pointer so we can safely cache it in per_shard_t to avoid
    // input2info_mutex_ on every access.
    // XXX: We could use a read-write lock but C++11 doesn't have a ready-made one.
    // If we had the input count we could use an array and atomic reads.
    std::unordered_map<int64_t, std::unique_ptr<per_input_t>> input2info_;
};

} // namespace drmemtrace
} // namespace dynamorio

#endif /* _RECORD_FILTER_H_ */
