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

#include "record_view.h"

#include <cstdio>
#include <inttypes.h>
#include <iostream>
#include <stdint.h>
#include <string>

#include "memtrace_stream.h"
#include "trace_entry.h"

namespace dynamorio {
namespace drmemtrace {

typedef unsigned int uint;

record_analysis_tool_t *
record_view_tool_create(uint64_t skip_refs, uint64_t sim_refs)
{
    return new dynamorio::drmemtrace::record_view_t(skip_refs, sim_refs);
}

record_view_t::record_view_t(uint64_t skip_refs, uint64_t sim_refs)
    : skip_refs_(skip_refs)
    , sim_refs_(sim_refs)
{
}

record_view_t::~record_view_t()
{
}

bool
record_view_t::should_skip(void)
{
    if (skip_refs_ > 0) {
        --skip_refs_;
        return true;
    }
    if (sim_refs_ > 0) {
        --sim_refs_;
        return false;
    }
    return true;
}

bool
record_view_t::parallel_shard_supported()
{
    return false;
}

std::string
record_view_t::initialize_shard_type(shard_type_t shard_type)
{
    return "";
}

void *
record_view_t::parallel_shard_init_stream(int shard_index, void *worker_data,
                                          memtrace_stream_t *shard_stream)
{
    return shard_stream;
}

bool
record_view_t::parallel_shard_exit(void *shard_data)
{
    return true;
}

std::string
record_view_t::parallel_shard_error(void *shard_data)
{
    return "";
}

bool
record_view_t::parallel_shard_memref(void *shard_data, const trace_entry_t &entry)
{
    if (should_skip())
        return true;

    trace_type_t trace_type = (trace_type_t)entry.type;
    if (trace_type == TRACE_TYPE_INVALID) {
        std::cerr << "ERROR: trace_entry_t invalid.\n";
        return false;
    }

    std::string trace_type_name = std::string(trace_type_names[trace_type]);

    /* Large if-else for all TRACE_TYPE_.  Prints one line per trace_entry_t.
     * In some cases we use some helper functions (e.g., type_is_instr()), which group
     * similar TRACE_TYPE_ together, otherwise we compare trace_type against one or two
     * specific TRACE_TYPE_ directly.
     */
    if (trace_type == TRACE_TYPE_HEADER) {
        trace_version_t trace_version = (trace_version_t)entry.addr;
        std::string trace_version_name = std::string(trace_version_names[trace_version]);
        std::cerr << "<" << trace_type_name << ", trace_version: " << trace_version
                  << " == " << trace_version_name << ">\n";
    } else if (trace_type == TRACE_TYPE_FOOTER) {
        std::cerr << "<" << trace_type_name << ">\n";
    } else if ((trace_type == TRACE_TYPE_THREAD) ||
               (trace_type == TRACE_TYPE_THREAD_EXIT)) {
        uint tid = (uint)entry.addr;
        std::cerr << "<" << trace_type_name << ", tid: " << tid << ">\n";
    } else if (trace_type == TRACE_TYPE_PID) {
        uint pid = (uint)entry.addr;
        std::cerr << "<" << trace_type_name << ", pid: " << pid << ">\n";
    } else if (trace_type == TRACE_TYPE_ENCODING) {
        unsigned short num_encoding_bytes = entry.size;
        std::cerr << "<" << trace_type_name
                  << ", num_encoding_bytes: " << num_encoding_bytes
                  << ", encoding_bytes: 0x" << std::hex;
        /* Print encoding byte by byte (little-endian).
         */
        for (int i = num_encoding_bytes - 1; i >= 0; --i) {
            uint encoding_byte = static_cast<uint>(entry.encoding[i]);
            std::cerr << encoding_byte;
        }
        std::cerr << std::dec << ">\n";
    } else if (trace_type == TRACE_TYPE_INSTR_BUNDLE) {
        unsigned short num_instructions_in_bundle = entry.size;
        std::cerr << "<" << trace_type_name
                  << ", num_instructions_in_bundle: " << num_instructions_in_bundle
                  << ", instrs_length:";
        /* Print length of each instr in the bundle.
         */
        for (int i = 0; i < num_instructions_in_bundle; ++i) {
            unsigned char instr_length = entry.length[i];
            std::cerr << " " << instr_length;
        }
        std::cerr << ">\n";
    } else if (type_is_instr(trace_type)) {
        unsigned short instr_length = entry.size;
        addr_t pc = entry.addr;
        std::cerr << "<" << trace_type_name << ", length: " << instr_length << ", pc: 0x"
                  << std::hex << pc << std::dec << ">\n";
    } else if (type_has_address(trace_type)) { // Includes no-fetch, prefetch, and flush.
        unsigned short memref_size = entry.size;
        addr_t memref_addr = entry.addr;
        std::cerr << "<" << trace_type_name << ", memref_size: " << memref_size
                  << ", memref_addr: 0x" << std::hex << memref_addr << std::dec << ">\n";
        /* trace_entry_t is a marker. Print marker type and value accordingly.
         */
    } else if (trace_type == TRACE_TYPE_MARKER) {
        trace_marker_type_t trace_marker_type = (trace_marker_type_t)entry.size;
        uintptr_t trace_marker_value = static_cast<uintptr_t>(entry.addr);
        std::cerr << trace_marker_type_value_as_string(trace_marker_type,
                                                       trace_marker_value);
    } else {
        std::cerr << "ERROR: unrecognized trace_entry_t: " << trace_type << ".\n";
        return false;
    }
    return true;
}

bool
record_view_t::process_memref(const trace_entry_t &entry)
{
    return parallel_shard_memref(NULL, entry);
}

bool
record_view_t::print_results()
{
    return true;
}

} // namespace drmemtrace
} // namespace dynamorio
