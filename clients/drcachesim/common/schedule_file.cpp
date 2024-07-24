/* **********************************************************
 * Copyright (c) 2016-2024 Google, Inc.  All rights reserved.
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

/* Produces schedule files that record the instruction counts at thread interleaving
 * points.
 */

#include <algorithm>
#include <cstdint>
#include <fstream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

#include "archive_ostream.h"
#include "memref.h"
#include "schedule_file.h"
#include "trace_entry.h"

namespace dynamorio {
namespace drmemtrace {

std::string
schedule_file_t::per_shard_t::record_cpu_id(memref_tid_t tid, uint64_t cpuid,
                                            uint64_t last_timestamp, uint64_t instr_count)
{
    schedule_entry_t new_entry(tid, last_timestamp, cpuid, instr_count);
    // Avoid identical entries, which are common with the end of the
    // previous buffer's timestamp followed by the start of the next.
    if (sched_.empty() || sched_.back() != new_entry) {
        sched_.emplace_back(tid, last_timestamp, cpuid, instr_count);
    }
    if (cpu2sched_[cpuid].empty() || cpu2sched_[cpuid].back() != new_entry) {
        cpu2sched_[cpuid].emplace_back(tid, last_timestamp, cpuid, instr_count);
    }
    return "";
}

const std::vector<schedule_entry_t> &
schedule_file_t::get_full_serial_records()
{
    if (!aggregated_)
        aggregate_schedule_data();
    return serial_;
}

const std::vector<schedule_entry_t> &
schedule_file_t::get_serial_records()
{
    if (!aggregated_)
        aggregate_schedule_data();
    return serial_redux_;
}

const std::unordered_map<uint64_t, std::vector<schedule_entry_t>> &
schedule_file_t::get_full_cpu_records()
{
    if (!aggregated_)
        aggregate_schedule_data();
    return cpu2sched_;
}

const std::unordered_map<uint64_t, std::vector<schedule_entry_t>> &
schedule_file_t::get_cpu_records()
{
    if (!aggregated_)
        aggregate_schedule_data();
    return cpu2sched_redux_;
}

std::string
schedule_file_t::merge_shard_data(const schedule_file_t::per_shard_t &shard)
{
    serial_.insert(serial_.end(), shard.sched_.begin(), shard.sched_.end());
    for (auto &keyval : shard.cpu2sched_) {
        auto &vec = cpu2sched_[keyval.first];
        vec.insert(vec.end(), keyval.second.begin(), keyval.second.end());
    }
    return "";
}

void
schedule_file_t::aggregate_schedule_data()
{
    if (aggregated_)
        return;
    auto schedule_entry_comparator = [](const schedule_entry_t &l,
                                        const schedule_entry_t &r) {
        if (l.timestamp != r.timestamp)
            return l.timestamp < r.timestamp;
        if (l.cpu != r.cpu)
            return l.cpu < r.cpu;
        // We really need to sort by either (timestamp, cpu_id,
        // start_instruction) or (timestamp, thread_id, start_instruction): a
        // single thread cannot be on two CPUs at the same timestamp; also a
        // single CPU cannot have two threads at the same timestamp. We still
        // sort by (timestamp, cpu_id, thread_id, start_instruction) to prevent
        // inadvertent issues with test data.
        if (l.thread != r.thread)
            return l.thread < r.thread;
        // We need to consider the start_instruction since it is possible to
        // have two entries with the same timestamp, cpu_id, and thread_id.
        return l.start_instruction < r.start_instruction;
    };

    std::sort(serial_.begin(), serial_.end(), schedule_entry_comparator);
    // Collapse same-thread entries.
    for (const auto &entry : serial_) {
        if (serial_redux_.empty() || entry.thread != serial_redux_.back().thread)
            serial_redux_.push_back(entry);
    }
    for (auto &keyval : cpu2sched_) {
        std::sort(keyval.second.begin(), keyval.second.end(), schedule_entry_comparator);
        // Collapse same-thread entries.
        std::vector<schedule_entry_t> redux;
        for (const auto &entry : keyval.second) {
            if (redux.empty() || entry.thread != redux.back().thread)
                redux.push_back(entry);
        }
        cpu2sched_redux_[keyval.first] = redux;
    }
    aggregated_ = true;
}

std::string
schedule_file_t::write_serial_file(std::ostream *out)
{
    if (out == nullptr)
        return "Output stream is null";
    if (!aggregated_)
        aggregate_schedule_data();
    if (!out->write(reinterpret_cast<const char *>(serial_redux_.data()),
                    serial_redux_.size() * sizeof(serial_redux_[0])))
        return "Failed to write to serial schedule file";
    return "";
}

std::string
schedule_file_t::write_cpu_file(archive_ostream_t *out)
{
    if (out == nullptr)
        return "Output archive stream is null";
    if (!aggregated_)
        aggregate_schedule_data();
    for (auto &keyval : cpu2sched_redux_) {
        std::ostringstream stream;
        stream << keyval.first;
        std::string err = out->open_new_component(stream.str());
        if (!err.empty())
            return err;
        if (!out->write(reinterpret_cast<const char *>(keyval.second.data()),
                        keyval.second.size() * sizeof(keyval.second[0])))
            return "Failed to write to cpu schedule file";
    }
    return "";
}

std::string
schedule_file_t::read_serial_file(std::istream *in)
{
    serial_.clear();
    serial_redux_.clear();
    schedule_entry_t next(0, 0, 0, 0);
    while (in->read(reinterpret_cast<char *>(&next), sizeof(next))) {
        serial_.push_back(next);
    }
    return "";
}

std::string
schedule_file_t::read_cpu_file(std::istream *in)
{
    cpu2sched_.clear();
    cpu2sched_redux_.clear();
    // The zipfile reader will form a continuous stream from all elements in the
    // archive.  We figure out which cpu each one is from on the fly.
    schedule_entry_t next(0, 0, 0, 0);
    while (in->read(reinterpret_cast<char *>(&next), sizeof(next))) {
        cpu2sched_[next.cpu].push_back(next);
    }
    return "";
}

} // namespace drmemtrace
} // namespace dynamorio
