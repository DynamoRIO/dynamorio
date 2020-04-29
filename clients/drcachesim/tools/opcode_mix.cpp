/* **********************************************************
 * Copyright (c) 2017-2020 Google, Inc.  All rights reserved.
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

/* This trace analyzer requires access to the modules.log file and the
 * libraries and binary from the traced execution in order to obtain further
 * information about each instruction than was stored in the trace.
 * It does not support online use, only offline.
 */

#include "dr_api.h"
#include "opcode_mix.h"
#include <algorithm>
#include <iomanip>
#include <iostream>
#include <vector>
#include <string.h>

const std::string opcode_mix_t::TOOL_NAME = "Opcode mix tool";

analysis_tool_t *
opcode_mix_tool_create(const std::string &module_file_path, unsigned int verbose)
{
    return new opcode_mix_t(module_file_path, verbose);
}

opcode_mix_t::opcode_mix_t(const std::string &module_file_path, unsigned int verbose)
    : module_file_path_(module_file_path)
    , knob_verbose_(verbose)
{
}

std::string
opcode_mix_t::initialize()
{
    serial_shard_.worker = &serial_worker_;
    if (module_file_path_.empty())
        return "Module file path is missing";
    dcontext_.dcontext = dr_standalone_init();
    std::string error = directory_.initialize_module_file(module_file_path_);
    if (!error.empty())
        return "Failed to initialize directory: " + error;
    module_mapper_ = module_mapper_t::create(directory_.modfile_bytes_, nullptr, nullptr,
                                             nullptr, nullptr, knob_verbose_);
    module_mapper_->get_loaded_modules();
    error = module_mapper_->get_last_error();
    if (!error.empty())
        return "Failed to load binaries: " + error;
    return "";
}

opcode_mix_t::~opcode_mix_t()
{
    for (auto &iter : shard_map_) {
        delete iter.second;
    }
}

bool
opcode_mix_t::parallel_shard_supported()
{
    return true;
}

void *
opcode_mix_t::parallel_worker_init(int worker_index)
{
    auto worker = new worker_data_t;
    return reinterpret_cast<void *>(worker);
}

std::string
opcode_mix_t::parallel_worker_exit(void *worker_data)
{
    worker_data_t *worker = reinterpret_cast<worker_data_t *>(worker_data);
    delete worker;
    return "";
}

void *
opcode_mix_t::parallel_shard_init(int shard_index, void *worker_data)
{
    worker_data_t *worker = reinterpret_cast<worker_data_t *>(worker_data);
    auto shard = new shard_data_t(worker);
    std::lock_guard<std::mutex> guard(shard_map_mutex_);
    shard_map_[shard_index] = shard;
    return reinterpret_cast<void *>(shard);
}

bool
opcode_mix_t::parallel_shard_exit(void *shard_data)
{
    // Nothing (we read the shard data in print_results).
    return true;
}

bool
opcode_mix_t::parallel_shard_memref(void *shard_data, const memref_t &memref)
{
    if (!type_is_instr(memref.instr.type) &&
        memref.data.type != TRACE_TYPE_INSTR_NO_FETCH) {
        return true;
    }
    shard_data_t *shard = reinterpret_cast<shard_data_t *>(shard_data);
    ++shard->instr_count;

    app_pc mapped_pc;
    const app_pc trace_pc = reinterpret_cast<app_pc>(memref.instr.addr);
    if (trace_pc >= shard->last_trace_module_start &&
        static_cast<size_t>(trace_pc - shard->last_trace_module_start) <
            shard->last_trace_module_size) {
        mapped_pc =
            shard->last_mapped_module_start + (trace_pc - shard->last_trace_module_start);
    } else {
        std::lock_guard<std::mutex> guard(mapper_mutex_);
        mapped_pc = module_mapper_->find_mapped_trace_bounds(
            trace_pc, &shard->last_mapped_module_start, &shard->last_trace_module_size);
        if (!module_mapper_->get_last_error().empty()) {
            shard->last_trace_module_start = nullptr;
            shard->last_trace_module_size = 0;
            shard->error = "Failed to find mapped address for " +
                to_hex_string(memref.instr.addr) + ": " +
                module_mapper_->get_last_error();
            return false;
        }
        shard->last_trace_module_start =
            trace_pc - (mapped_pc - shard->last_mapped_module_start);
    }
    int opcode;
    auto cached_opcode = shard->worker->opcode_cache.find(mapped_pc);
    if (cached_opcode != shard->worker->opcode_cache.end()) {
        opcode = cached_opcode->second;
    } else {
        instr_t instr;
        instr_init(dcontext_.dcontext, &instr);
        app_pc next_pc = decode(dcontext_.dcontext, mapped_pc, &instr);
        if (next_pc == NULL || !instr_valid(&instr)) {
            error_string_ =
                "Failed to decode instruction " + to_hex_string(memref.instr.addr);
            return false;
        }
        opcode = instr_get_opcode(&instr);
        shard->worker->opcode_cache[mapped_pc] = opcode;
        instr_free(dcontext_.dcontext, &instr);
    }
    ++shard->opcode_counts[opcode];
    return true;
}

std::string
opcode_mix_t::parallel_shard_error(void *shard_data)
{
    shard_data_t *shard = reinterpret_cast<shard_data_t *>(shard_data);
    return shard->error;
}

bool
opcode_mix_t::process_memref(const memref_t &memref)
{
    if (!parallel_shard_memref(reinterpret_cast<void *>(&serial_shard_), memref)) {
        error_string_ = serial_shard_.error;
        return false;
    }
    return true;
}

static bool
cmp_val(const std::pair<int, int_least64_t> &l, const std::pair<int, int_least64_t> &r)
{
    return (l.second > r.second);
}

bool
opcode_mix_t::print_results()
{
    shard_data_t total(0);
    if (shard_map_.empty()) {
        total = serial_shard_;
    } else {
        for (const auto &shard : shard_map_) {
            total.instr_count += shard.second->instr_count;
            for (const auto &keyvals : shard.second->opcode_counts) {
                total.opcode_counts[keyvals.first] += keyvals.second;
            }
        }
    }
    std::cerr << TOOL_NAME << " results:\n";
    std::cerr << std::setw(15) << total.instr_count << " : total executed instructions\n";
    std::vector<std::pair<int, int_least64_t>> sorted(total.opcode_counts.begin(),
                                                      total.opcode_counts.end());
    std::sort(sorted.begin(), sorted.end(), cmp_val);
    for (const auto &keyvals : sorted) {
        std::cerr << std::setw(15) << keyvals.second << " : " << std::setw(9)
                  << decode_opcode_name(keyvals.first) << "\n";
    }
    return true;
}
