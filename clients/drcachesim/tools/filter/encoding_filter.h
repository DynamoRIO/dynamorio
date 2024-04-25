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

#include "record_filter.h"
#include "trace_entry.h"
#include "utils.h"

#include <cstring>
#include <vector>

/* We are not exporting the defines in core/ir/isa_regdeps/encoding_common.h, so we
 * redefine DR_ISA_REGDEPS alignment requirement here.
 */
#define REGDEPS_ALIGN_BYTES 4

#define REGDEPS_MAX_ENCODING_LENGTH 16

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
        return nullptr;
    }

    bool
    parallel_shard_filter(trace_entry_t &entry, void *shard_data,
                          std::vector<trace_entry_t> &last_encoding) override
    {
        /* TODO i#6662: modify trace_entry_t header entry to regdeps ISA, instead of the
         * real ISA of the incoming trace.
         */

        /* We have encoding to convert.
         * Normally the sequence of trace_entry_t(s) looks like:
         * [encoding,]+ instr_with_PC, [read | write]*
         * (+ = one or more, * = zero or more)
         * If we enter here, trace_entry_t is instr_with_PC.
         */
        if (is_any_instr_type(static_cast<trace_type_t>(entry.type)) &&
            !last_encoding.empty()) {
            /* Gather real ISA encoding bytes looping through all previously saved
             * encoding bytes in last_encoding.
             */
            const app_pc pc = reinterpret_cast<app_pc>(entry.addr);
            byte encoding[MAX_ENCODING_LENGTH];
            memset(encoding, 0, sizeof(encoding));
            uint encoding_offset = 0;
            for (auto &trace_encoding : last_encoding) {
                memcpy(encoding + encoding_offset, trace_encoding.encoding,
                       trace_encoding.size);
                encoding_offset += trace_encoding.size;
            }

            /* Genenerate the real ISA instr_t by decoding the encoding bytes.
             */
            instr_t instr;
            instr_init(dcontext_.dcontext, &instr);
            app_pc next_pc = decode_from_copy(dcontext_.dcontext, encoding, pc, &instr);
            if (next_pc == NULL || !instr_valid(&instr)) {
                instr_free(dcontext_.dcontext, &instr);
                error_string_ =
                    "Failed to decode instruction " + to_hex_string(entry.addr);
                return false;
            }

            /* Convert the real ISA instr_t into a regdeps ISA instr_t.
             */
            instr_t instr_regdeps;
            instr_init(dcontext_.dcontext, &instr_regdeps);
            instr_convert_to_isa_regdeps(dcontext_.dcontext, &instr, &instr_regdeps);

            /* Obtain regdeps ISA instr_t encoding bytes.
             */
            byte ALIGN_VAR(REGDEPS_ALIGN_BYTES)
                encoding_regdeps[REGDEPS_MAX_ENCODING_LENGTH];
            app_pc next_pc_regdeps =
                instr_encode(dcontext_.dcontext, &instr_regdeps, encoding_regdeps);

            /* Compute number of trace_entry_t to contain regdeps ISA encoding.
             * Each trace_entry_t record can contain 8 byte encoding.
             */
            uint trace_entry_encoding_size = (uint)sizeof(entry.addr); /* == 8 */
            uint regdeps_encoding_size = next_pc_regdeps - encoding_regdeps;
            uint num_regdeps_encoding_entries =
                ALIGN_FORWARD(regdeps_encoding_size, trace_entry_encoding_size) /
                trace_entry_encoding_size;
            last_encoding.resize(num_regdeps_encoding_entries);

            /* Copy regdeps ISA encoding, splitting it among the last_encoding
             * trace_entry_t records.
             */
            uint regdeps_encoding_offset = 0;
            for (trace_entry_t &encoding_entry : last_encoding) {
                encoding_entry.type = TRACE_TYPE_ENCODING;
                uint size = regdeps_encoding_size < trace_entry_encoding_size
                    ? regdeps_encoding_size
                    : trace_entry_encoding_size;
                encoding_entry.size = (unsigned short)size;
                memset(encoding_entry.encoding, 0, trace_entry_encoding_size);
                memcpy(encoding_entry.encoding,
                       encoding_regdeps + regdeps_encoding_offset, encoding_entry.size);
                regdeps_encoding_size -= trace_entry_encoding_size;
                regdeps_encoding_offset += encoding_entry.size;
            }
        }
        return true;
    }

    bool
    parallel_shard_exit(void *shard_data) override
    {
        return true;
    }

private:
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
};

} // namespace drmemtrace
} // namespace dynamorio
#endif /* _ENCODING_FILTER_H_ */
