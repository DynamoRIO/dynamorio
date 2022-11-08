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

#include "analysis_tool.h"
#include <memory>
#include <vector>

namespace dynamorio {
namespace drmemtrace {

/**
 * Analysis tool that filters the trace_entry_t records of an offline
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
        virtual bool
        filter(const trace_entry_t &entry) = 0;
    };

    record_filter_t(const std::string &output_dir,
                    const std::vector<record_filter_func_t *> &filters,
                    unsigned int verbose);
    ~record_filter_t() override;
    bool
    process_memref(const trace_entry_t &entry) override;
    bool
    print_results() override;
    bool
    parallel_shard_supported() override;
    void *
    parallel_shard_init_ex(int shard_index, void *worker_data,
                           const std::string &shard_suffix) override;
    bool
    parallel_shard_exit(void *shard_data) override;
    bool
    parallel_shard_memref(void *shard_data, const trace_entry_t &entry) override;
    std::string
    parallel_shard_error(void *shard_data) override;

private:
    struct per_shard_t {
        std::string output_path;
        std::unique_ptr<std::ostream> writer;
        std::string error;
    };

    std::string output_dir_;
    std::vector<record_filter_func_t *> filters_;
    unsigned int verbosity_;
};

} // namespace drmemtrace
} // namespace dynamorio
#endif /* _RECORD_FILTER_H_ */
