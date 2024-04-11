/* **********************************************************
 * Copyright (c) 2024 Google, Inc.  All rights reserved.
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

#ifndef _RECORD_VIEW_H_
#define _RECORD_VIEW_H_ 1

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
#include "trace_entry.h"

namespace dynamorio {
namespace drmemtrace {

/**
 * Analysis tool that prints #trace_entry_t records of an offline trace in human readable
 * form.
 */
class record_view_t : public record_analysis_tool_t {
public:
    record_view_t();

    ~record_view_t() override;

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
};

} // namespace drmemtrace
} // namespace dynamorio

#endif /* _RECORD_VIEW_H_ */
