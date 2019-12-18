/* **********************************************************
 * Copyright (c) 2017-2019 Google, Inc.  All rights reserved.
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

#ifndef _REUSE_TIME_H_
#define _REUSE_TIME_H_ 1

#include <mutex>
#include <unordered_map>
#include <string>

#include "analysis_tool.h"

class reuse_time_t : public analysis_tool_t {
public:
    reuse_time_t(unsigned int line_size, unsigned int verbose);
    ~reuse_time_t() override;
    bool
    process_memref(const memref_t &memref) override;
    bool
    print_results() override;
    bool
    parallel_shard_supported() override;
    void *
    parallel_shard_init(int shard_index, void *worker_data) override;
    bool
    parallel_shard_exit(void *shard_data) override;
    bool
    parallel_shard_memref(void *shard_data, const memref_t &memref) override;
    std::string
    parallel_shard_error(void *shard_data) override;

protected:
    // Just like for reuse_distance_t, we assume that the shard unit is the unit over
    // which we should measure time.  By default this is a traced thread.
    struct shard_data_t {
        std::unordered_map<addr_t, int_least64_t> time_map;
        int_least64_t time_stamp = 0;
        int_least64_t total_instructions = 0;
        std::unordered_map<int_least64_t, int_least64_t> reuse_time_histogram;
        memref_tid_t tid;
        std::string error;
    };

    void
    print_shard_results(const shard_data_t *shard);

    const unsigned int knob_verbose;
    const unsigned int knob_line_size;
    const unsigned int line_size_bits;

    static const std::string TOOL_NAME;

    // In parallel operation the keys are "shard indices": just ints.
    std::unordered_map<memref_tid_t, shard_data_t *> shard_map;
    // This mutex is only needed in parallel_shard_init.  In all other accesses to
    // shard_map (process_memref, print_results) we are single-threaded.
    std::mutex shard_map_mutex;
};

#endif /* _REUSE_TIME_H_ */
