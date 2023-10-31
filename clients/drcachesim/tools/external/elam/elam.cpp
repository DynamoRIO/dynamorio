/* **********************************************************
 * Copyright (c) 2023 Google, Inc.  All rights reserved.
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

/* External analysis tool example. */

#include "dr_api.h"
#include "elam.h"
#include <iostream>
#include "analysis_tool.h"

const std::string elam_t::TOOL_NAME = "Elam's tool";

analysis_tool_t *
elam_tool_create(unsigned int verbose)
{
    return new elam_t(verbose);
}

elam_t::elam_t(unsigned int verbose)
{
    line_size = 64; // bytes
    line_size_bits_ = dynamorio::drmemtrace::compute_log2((int)line_size);
}

std::string
elam_t::initialize()
{
    return std::string("");
}

elam_t::~elam_t()
{
}

bool
elam_t::parallel_shard_supported()
{
    return false;
    
}

void *
elam_t::parallel_worker_init(int worker_index)
{
    return NULL;
}

std::string
elam_t::parallel_worker_exit(void *worker_data)
{
    return std::string("");
}

void *
elam_t::parallel_shard_init(int shard_index, void *worker_data)
{
    return NULL;
}

bool
elam_t::parallel_shard_exit(void *shard_data)
{
    return true;
}

static inline addr_t
back_align(addr_t addr, addr_t align)
{
    return addr & ~(align - 1);
}


void update_map_from_access(std::unordered_map<addr_t, uint64_t> * m, addr_t start_addr, uint64_t size, uint64_t line_size, uint64_t line_size_bits_) {
        for (dynamorio::drmemtrace::addr_t addr = back_align(start_addr, line_size);
            addr < start_addr + size && addr < addr + line_size /* overflow */;
            addr += line_size) {
            ++(*m)[addr >> line_size_bits_];
        }
}

bool
elam_t::parallel_shard_memref(void *shard_data, const memref_t &memref)
{
    return false;
}

std::string
elam_t::parallel_shard_error(void *shard_data)
{
    return std::string("");
}

bool
elam_t::process_memref(const memref_t &memref) // TODO make this work with parallel / multithreaded applications if we actually care about delineating
{
    if (memref.data.type == dynamorio::drmemtrace::TRACE_TYPE_READ) {
            update_map_from_access(&ios[last_timestamp][IoType::Load], memref.instr.addr, memref.instr.size, line_size, line_size_bits_);
    } else if (memref.data.type == dynamorio::drmemtrace::TRACE_TYPE_WRITE) {
            update_map_from_access(&ios[last_timestamp][IoType::Store], memref.instr.addr, memref.instr.size, line_size, line_size_bits_);
    } else if (memref.marker.type == dynamorio::drmemtrace::TRACE_TYPE_MARKER && memref.marker.marker_type == dynamorio::drmemtrace::TRACE_MARKER_TYPE_TIMESTAMP) {
        uint64_t timestamp = memref.marker.marker_value;
        last_timestamp = timestamp;
    }
    return true;
}

bool
elam_t::print_results()
{
    std::fprintf(stderr, "timestamp,address,count,type\n");
    for (const auto & a : ios) {
        unsigned int timestamp = a.first;
        for (const auto & ioType : a.second) {
            for (const auto & io : ioType.second) {
                addr_t addr = io.first;
                unsigned int n_occurences = io.second;
                switch (ioType.first) {
                    case IoType::Load: 
                        std::fprintf(stderr, "%u,%llu,%u,Load\n", timestamp, addr, n_occurences);
                     case IoType::Store:
                        std::fprintf(stderr, "%u,%llu,%u,Store\n", timestamp, addr, n_occurences);
                        // TODO the rest of ops (ifetch, prefetch, etc?)
                }
            }
        }

    }
    return true;
}
