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

/* This filter changes the encoding of trace_entry_t and generates discrepancies between
 * encoding size and instruction length. So, we need to tell reader_t, which here comes in
 * the form of memref_counter_t used in record_filter, to ignore such discrepancies. We do
 * so by adding OFFLINE_FILE_TYPE_ARCH_REGDEPS to the file type of the filtered trace.
 * Note that simulators that deal with these filtered traces will also have to handle the
 * fact that encoding_size != instruction_length.
 */
class encodings2regdeps_filter_t : public record_filter_t::record_filter_func_t {
public:
    encodings2regdeps_filter_t()
    {
    }

    void *
    parallel_shard_init(memtrace_stream_t *shard_stream,
                        bool partial_trace_filter) override
    {
        return nullptr;
    }

    bool
    parallel_shard_filter(
        trace_entry_t &entry, void *shard_data,
        record_filter_t::record_filter_info_t &record_filter_info) override
    {
        std::vector<trace_entry_t> *last_encoding = record_filter_info.last_encoding;
        void *dcontext = record_filter_info.dcontext;

        /* Modify file_type to regdeps ISA, removing the real ISA of the input trace.
         */
        trace_type_t entry_type = static_cast<trace_type_t>(entry.type);
        if (entry_type == TRACE_TYPE_MARKER) {
            trace_marker_type_t marker_type =
                static_cast<trace_marker_type_t>(entry.size);
            if (marker_type == TRACE_MARKER_TYPE_FILETYPE) {
                uint64_t marker_value = static_cast<uint64_t>(entry.addr);
                marker_value = update_filetype(marker_value);
                entry.addr = static_cast<addr_t>(marker_value);
            }
        }

        /* We have encoding to convert.
         * Normally the sequence of trace_entry_t(s) looks like:
         * [TRACE_TYPE_ENCODING,]+ [TRACE_TYPE_MARKER.TRACE_MARKER_TYPE_BRANCH_TARGET,]
         * TRACE_TYPE_INSTR_, [TRACE_TYPE_READ | TRACE_TYPE_WRITE]*
         * ([] = zero or one, + = one or more, * = zero or more)
         * If we enter here, trace_entry_t is some TRACE_TYPE_INSTR_ for which
         * last_encoding already contains its encoding.
         */
        if (is_any_instr_type(static_cast<trace_type_t>(entry.type)) &&
            !last_encoding->empty()) {
            /* Gather real ISA encoding bytes looping through all previously saved
             * encoding bytes in last_encoding.
             */
            const app_pc pc = reinterpret_cast<app_pc>(entry.addr);
            byte encoding[MAX_ENCODING_LENGTH];
            memset(encoding, 0, sizeof(encoding));
            uint encoding_offset = 0;
            for (auto &trace_encoding : *last_encoding) {
                memcpy(encoding + encoding_offset, trace_encoding.encoding,
                       trace_encoding.size);
                encoding_offset += trace_encoding.size;
            }

            /* Genenerate the real ISA instr_t by decoding the encoding bytes.
             */
            instr_t instr;
            instr_init(dcontext, &instr);
            app_pc next_pc = decode_from_copy(dcontext, encoding, pc, &instr);
            if (next_pc == NULL || !instr_valid(&instr)) {
                instr_free(dcontext, &instr);
                error_string_ =
                    "Failed to decode instruction " + to_hex_string(entry.addr);
                return false;
            }

            /* Convert the real ISA instr_t into a regdeps ISA instr_t.
             */
            instr_t instr_regdeps;
            instr_init(dcontext, &instr_regdeps);
            instr_convert_to_isa_regdeps(dcontext, &instr, &instr_regdeps);
            instr_free(dcontext, &instr);

            /* Obtain regdeps ISA instr_t encoding bytes.
             */
            byte ALIGN_VAR(REGDEPS_ALIGN_BYTES)
                encoding_regdeps[REGDEPS_MAX_ENCODING_LENGTH];
            memset(encoding_regdeps, 0, sizeof(encoding_regdeps));
            app_pc next_pc_regdeps =
                instr_encode(dcontext, &instr_regdeps, encoding_regdeps);
            instr_free(dcontext, &instr_regdeps);
            if (next_pc_regdeps == NULL) {
                error_string_ =
                    "Failed to encode regdeps instruction " + to_hex_string(entry.addr);
                return false;
            }

            /* Compute number of trace_entry_t to contain regdeps ISA encoding.
             * Each trace_entry_t record can contain pointer-sized byte encoding
             * (i.e., 4 bytes for 32 bits architectures and 8 bytes for 64 bits).
             */
            uint trace_entry_encoding_size = static_cast<uint>(sizeof(entry.addr));
            uint regdeps_encoding_size =
                static_cast<uint>(next_pc_regdeps - encoding_regdeps);
            uint num_regdeps_encoding_entries =
                ALIGN_FORWARD(regdeps_encoding_size, trace_entry_encoding_size) /
                trace_entry_encoding_size;
            last_encoding->resize(num_regdeps_encoding_entries);

            /* Copy regdeps ISA encoding, splitting it among the last_encoding
             * trace_entry_t records.
             */
            uint regdeps_encoding_offset = 0;
            for (trace_entry_t &encoding_entry : *last_encoding) {
                encoding_entry.type = TRACE_TYPE_ENCODING;
                uint size = std::min(regdeps_encoding_size, trace_entry_encoding_size);
                encoding_entry.size = static_cast<unsigned short>(size);
                memset(encoding_entry.encoding, 0, trace_entry_encoding_size);
                memcpy(encoding_entry.encoding,
                       encoding_regdeps + regdeps_encoding_offset, encoding_entry.size);
                regdeps_encoding_size -= encoding_entry.size;
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

    uint64_t
    update_filetype(uint64_t filetype) override
    {
        filetype &= ~OFFLINE_FILE_TYPE_ARCH_ALL;
        filetype |= OFFLINE_FILE_TYPE_ARCH_REGDEPS;
        return filetype;
    }
};

} // namespace drmemtrace
} // namespace dynamorio
#endif /* _ENCODING_FILTER_H_ */
