/* **********************************************************
 * Copyright (c) 2022 Google, Inc.  All rights reserved.
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

#include <memory>
#include <mutex>
#include <ostream>
#include <string>
#include <unordered_map>
#include <vector>

#include "analysis_tool.h"
#include "memref.h"
#include "memtrace_stream.h"
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
         * trace. \p shard_data is same as what was returned by
         * parallel_shard_init(). The given \p entry is included in the result
         * trace iff all provided #record_filter_func_t return true. The
         * \p entry parameter can also be modified by the record_filter_func_t.
         * The passed \p entry is not guaranteed to be the original one from
         * the trace if other filter tools are present, and may include changes
         * made by other tools.
         */
        virtual bool
        parallel_shard_filter(trace_entry_t &entry, void *shard_data) = 0;
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

    protected:
        std::string error_string_;
    };

    record_filter_t(const std::string &output_dir,
                    std::vector<std::unique_ptr<record_filter_func_t>> filters,
                    uint64_t stop_timestamp, unsigned int verbose);
    ~record_filter_t() override;
    bool
    process_memref(const trace_entry_t &entry) override;
    bool
    print_results() override;
    bool
    parallel_shard_supported() override;
    void *
    parallel_shard_init_stream(int shard_index, void *worker_data,
                               memtrace_stream_t *shard_stream) override;
    bool
    parallel_shard_exit(void *shard_data) override;
    bool
    parallel_shard_memref(void *shard_data, const trace_entry_t &entry) override;
    std::string
    parallel_shard_error(void *shard_data) override;

protected:
    struct per_shard_t {
        std::string output_path;
        std::unique_ptr<std::ostream> writer;
        std::string error;
        std::vector<void *> filter_shard_data;
        std::vector<trace_entry_t> last_delayed_unit_header;
        std::unordered_map<uint64_t, std::vector<trace_entry_t>> delayed_encodings;
        std::vector<trace_entry_t> last_encoding;
        uint64_t input_entry_count;
        uint64_t output_entry_count;
        memtrace_stream_t *shard_stream;
        bool enabled;
    };
    // In parallel operation the keys are "shard indices": just ints.
    std::unordered_map<memref_tid_t, per_shard_t *> shard_map_;
    // This mutex is only needed in parallel_shard_init. In all other accesses
    // to shard_map (print_results) we are single-threaded.
    std::mutex shard_map_mutex_;

private:
    virtual bool
    write_trace_entry(per_shard_t *shard, const trace_entry_t &entry);

    virtual std::unique_ptr<std::ostream>
    get_writer(per_shard_t *per_shard, memtrace_stream_t *shard_stream);

    bool
    write_trace_entries(per_shard_t *shard, const std::vector<trace_entry_t> &entries);

    std::string output_dir_;
    std::vector<std::unique_ptr<record_filter_func_t>> filters_;
    uint64_t stop_timestamp_;
    unsigned int verbosity_;
    const char *output_prefix_ = "[record_filter]";
};

} // namespace drmemtrace
} // namespace dynamorio

#endif /* _RECORD_FILTER_H_ */
