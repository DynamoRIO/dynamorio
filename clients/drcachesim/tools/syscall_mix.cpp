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

#include "syscall_mix.h"

#include <stdint.h>

#include <algorithm>
#include <cassert>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "analysis_tool.h"
#include "dr_api.h"
#include "memref.h"
#include "trace_entry.h"
#include "utils.h"

namespace dynamorio {
namespace drmemtrace {

const std::string syscall_mix_t::TOOL_NAME = "Syscall mix tool";

analysis_tool_t *
syscall_mix_tool_create(unsigned int verbose = 0)
{
    return new syscall_mix_t(verbose);
}

syscall_mix_t::syscall_mix_t(unsigned int verbose)
    : knob_verbose_(verbose)
{
}

syscall_mix_t::~syscall_mix_t()
{
    for (auto &iter : shard_map_) {
        delete iter.second;
    }
}

bool
syscall_mix_t::parallel_shard_supported()
{
    return true;
}

void *
syscall_mix_t::parallel_shard_init(int shard_index, void *worker_data)
{
    auto shard = new shard_data_t();
    std::lock_guard<std::mutex> guard(shard_map_mutex_);
    shard_map_[shard_index] = shard;
    return reinterpret_cast<void *>(shard);
}

void *
syscall_mix_t::parallel_worker_init(int worker_index)
{
    return nullptr;
}

std::string
syscall_mix_t::parallel_worker_exit(void *worker_data)
{
    return "";
}

bool
syscall_mix_t::parallel_shard_exit(void *shard_data)
{
    // Nothing (we read the shard data in print_results).
    return true;
}

bool
syscall_mix_t::parallel_shard_memref(void *shard_data, const memref_t &memref)
{
    shard_data_t *shard = reinterpret_cast<shard_data_t *>(shard_data);
    if (memref.marker.type != TRACE_TYPE_MARKER)
        return true;
    switch (memref.marker.marker_type) {
    case TRACE_MARKER_TYPE_FILETYPE:
        if (!TESTANY(OFFLINE_FILE_TYPE_SYSCALL_NUMBERS, memref.marker.marker_value)) {
            shard->error = "Trace does not have TRACE_MARKER_TYPE_SYSCALL markers.";
            return false;
        }
        break;
    case TRACE_MARKER_TYPE_SYSCALL: {
        int syscall_num = static_cast<int>(memref.marker.marker_value);
#ifdef X64
        assert(static_cast<uintptr_t>(syscall_num) == memref.marker.marker_value);
#endif
        ++shard->syscall_counts[syscall_num];
        break;
    }
    default: break;
    }
    return true;
}

std::string
syscall_mix_t::parallel_shard_error(void *shard_data)
{
    shard_data_t *shard = reinterpret_cast<shard_data_t *>(shard_data);
    return shard->error;
}

bool
syscall_mix_t::process_memref(const memref_t &memref)
{
    if (!parallel_shard_memref(reinterpret_cast<void *>(&serial_shard_), memref)) {
        error_string_ = serial_shard_.error;
        return false;
    }
    return true;
}

static bool
cmp_second_val(const std::pair<int, int64_t> &l, const std::pair<int, int64_t> &r)
{
    return l.second > r.second;
}

bool
syscall_mix_t::print_results()
{
    shard_data_t total;
    if (shard_map_.empty()) {
        total = serial_shard_;
    } else {
        for (const auto &shard : shard_map_) {
            for (const auto &keyvals : shard.second->syscall_counts) {
                total.syscall_counts[keyvals.first] += keyvals.second;
            }
        }
    }
    std::cerr << TOOL_NAME << " results:\n";
    std::cerr << std::setw(15) << "count"
              << " : " << std::setw(9) << "syscall_num\n";
    std::vector<std::pair<int, int64_t>> sorted(total.syscall_counts.begin(),
                                                total.syscall_counts.end());
    std::sort(sorted.begin(), sorted.end(), cmp_second_val);
    for (const auto &keyvals : sorted) {
        // XXX: It would be nicer to print the system call name string instead of
        // its number.
        std::cerr << std::setw(15) << keyvals.second << " : " << std::setw(9)
                  << keyvals.first << "\n";
    }
    return true;
}

} // namespace drmemtrace
} // namespace dynamorio
