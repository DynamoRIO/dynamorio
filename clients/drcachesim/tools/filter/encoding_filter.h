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

#ifndef _ENCODING_FILTER_H_
#define _ENCODING_FILTER_H_ 1

#include "utils.h"
#include "dr_api.h" // Must be before trace_entry.h from analysis_tool.h.
#include "record_filter.h"
#include "trace_entry.h"
#include "utils.h"

#include <cstring>
#include <vector>

namespace dynamorio {
namespace drmemtrace {

class encoding_filter_t : public record_filter_t::record_filter_func_t {
public:
    encoding_filter_t()
    {
    }

    void *
    parallel_shard_init(memtrace_stream_t *shard_stream,
                        bool partial_trace_filter) override
    {
        dcontext_.dcontext = dr_standalone_init();
        per_shard_t *per_shard = new per_shard_t;
        per_shard->prev_instr_length = 0;
        per_shard->prev_pc = 0;
        return per_shard;
    }

    bool
    parallel_shard_filter(trace_entry_t &entry, void *shard_data,
                          std::vector<trace_entry_t> &last_encoding) override
    {
        per_shard_t *per_shard = reinterpret_cast<per_shard_t *>(shard_data);

        /* We have encoding to convert.
         * Normally the sequence of trace_entry_t(s) looks like:
         * [encoding,]+ instr_with_PC, [read | write]*
         * (+ = one or more, * = zero or more)
         * If we enter here, trace_entry_t is instr_with_PC.
         */
        if (is_any_instr_type(static_cast<trace_type_t>(entry.type)) &&
            !last_encoding.empty()) {
            const app_pc pc = reinterpret_cast<app_pc>(entry.addr);
            byte encoding[MAX_ENCODING_LENGTH];
            memset(encoding, 0, sizeof(encoding));
            uint encoding_size_acc = 0;
            for (auto &trace_encoding : last_encoding) {
                memcpy(encoding + encoding_size_acc, trace_encoding.encoding,
                       trace_encoding.size);
                encoding_size_acc += trace_encoding.size;
            }

            instr_t instr;
            instr_init(dcontext_.dcontext, &instr);
            app_pc next_pc = decode_from_copy(dcontext_.dcontext, encoding, pc, &instr);
            if (next_pc == NULL || !instr_valid(&instr)) {
                instr_free(dcontext_.dcontext, &instr);
                return false;
            }

            instr_t instr_regdeps;
            instr_init(dcontext_.dcontext, &instr_regdeps);
            instr_convert_to_isa_regdeps(dcontext_.dcontext, &instr, &instr_regdeps);

            byte ALIGN_VAR(4) encoding_regdeps[16];
            app_pc next_pc_regdeps =
                instr_encode(dcontext_.dcontext, &instr_regdeps, encoding_regdeps);

            uint encoding_regdeps_length = next_pc_regdeps - encoding_regdeps;

            entry.size = encoding_regdeps_length;
            if (per_shard->prev_pc == 0)
                per_shard->prev_pc = entry.addr;
            old_to_new_pc[entry.addr] = per_shard->prev_pc + per_shard->prev_instr_length;
            old_to_new_length[entry.addr] = encoding_regdeps_length;
            entry.addr = per_shard->prev_pc + per_shard->prev_instr_length;
            per_shard->prev_instr_length = encoding_regdeps_length;
            per_shard->prev_pc = entry.addr;

            uint num_encoding_entries_regdeps =
                ALIGN_FORWARD(encoding_regdeps_length, 8) / 8;
            last_encoding.resize(num_encoding_entries_regdeps);
            for (trace_entry_t &te : last_encoding) {
                te.type = TRACE_TYPE_ENCODING;
                te.size = encoding_regdeps_length < 8 ? encoding_regdeps_length : 8;
                memset(te.encoding, 0, 8);
                memcpy(te.encoding, encoding_regdeps, te.size);
                encoding_regdeps_length -= 8;
            }
        } else if (is_any_instr_type(static_cast<trace_type_t>(entry.type)) &&
                   last_encoding.empty()) {
            addr_t old_pc = entry.addr;
            auto found = old_to_new_pc.find(old_pc);
            if (found != old_to_new_pc.end()) {
                entry.addr = found->second;
                entry.size = old_to_new_length[old_pc];
            }
        }
        return true;
    }

    bool
    parallel_shard_exit(void *shard_data) override
    {
        per_shard_t *per_shard = reinterpret_cast<per_shard_t *>(shard_data);
        delete per_shard;
        return true;
    }

private:
    struct per_shard_t {
        uint prev_instr_length;
        addr_t prev_pc;
    };

    struct dcontext_cleanup_last_t {
    public:
        ~dcontext_cleanup_last_t()
        {
            if (dcontext != nullptr)
                dr_standalone_exit();
        }
        void *dcontext = nullptr;
    };

    dcontext_cleanup_last_t dcontext_;

    std::unordered_map<addr_t, addr_t> old_to_new_pc;
    std::unordered_map<addr_t, unsigned short> old_to_new_length;
};

} // namespace drmemtrace
} // namespace dynamorio
#endif /* _ENCODING_FILTER_H_ */
