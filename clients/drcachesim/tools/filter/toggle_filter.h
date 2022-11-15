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

#ifndef _TOGGLE_FILTER_H_
#define _TOGGLE_FILTER_H_ 1

#include "record_filter.h"

namespace dynamorio {
namespace drmemtrace {

/**
 * Filter that starts off either enabled (outputting all entries) or
 * disabled (skipping all entries), and toggles its behavior when the
 * given instruction count is reached. This is useful for shortening
 * or splitting a trace.
 */
class toggle_filter_t : public record_filter_t::record_filter_func_t {
public:
    toggle_filter_t(uint64 instr_count_toggle, bool enable_write)
        : instr_count_toggle_(instr_count_toggle)
        , enable_write_(enable_write)
    {
    }
    void *
    parallel_shard_init() override
    {
        auto per_shard = new per_shard_t;
        per_shard->instr_count = 0;
        return per_shard;
    }
    bool
    parallel_shard_filter(const trace_entry_t &entry, void *shard_data,
                          bool *stop) override
    {
        per_shard_t *per_shard = reinterpret_cast<per_shard_t *>(shard_data);
        if (type_is_instr((trace_type_t)entry.type)) {
            ++per_shard->instr_count;
            if (per_shard->instr_count == instr_count_toggle_) {
                enable_write_ = !enable_write_;
                if (!enable_write_) {
                    *stop = true;
                }
            }
        }
        return enable_write_;
    }
    bool
    parallel_shard_exit(void *shard_data) override
    {
        delete reinterpret_cast<per_shard_t *>(shard_data);
        return true;
    }

private:
    struct per_shard_t {
        uint64 instr_count;
    };
    uint64 instr_count_toggle_;
    bool enable_write_;
};

} // namespace drmemtrace
} // namespace dynamorio
#endif /* _TOGGLE_FILTER_H_ */
