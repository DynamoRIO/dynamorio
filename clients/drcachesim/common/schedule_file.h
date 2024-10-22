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

/* schedule_file_t: support for creating, sorting, writing, and reading
 * schedule files of schedule_entry_t records.
 */

#ifndef _SCHEDULE_FILE_H_
#define _SCHEDULE_FILE_H_ 1

#include <cstdint>
#include <fstream>
#include <string>
#include <unordered_map>
#include <vector>

#include "archive_ostream.h"
#include "memref.h"
#include "trace_entry.h"

namespace dynamorio {
namespace drmemtrace {

// Usage: add an instance of schedule_file_t::per_shard_t to each shard.
// When a TRACE_MARKER_TYPE_CPU_ID record is encountered, call
// record_cpu_id on that instance.
// At aggregation time, use an instance of schedule_file_t andloop over the
// shards calling merge_shard_data().  Now that instance can be written
// out to a file.
// Alternatively, use the read_* routines to read into an instane.

class schedule_file_t {
public:
    class per_shard_t {
    public:
        std::string
        record_cpu_id(memref_tid_t tid, uint64_t cpuid, uint64_t last_timestamp,
                      uint64_t instr_count);

    private:
        friend class schedule_file_t;
        std::vector<schedule_entry_t> sched_;
        std::unordered_map<uint64_t, std::vector<schedule_entry_t>> cpu2sched_;
    };

    std::string
    merge_shard_data(const per_shard_t &shard);

    std::string
    write_serial_file(std::ostream *out);

    std::string
    write_cpu_file(archive_ostream_t *out);

    std::string
    read_serial_file(std::istream *in);

    std::string
    read_cpu_file(std::istream *in);

    // Returns the uncollapsed record sequence.
    const std::vector<schedule_entry_t> &
    get_full_serial_records();
    // Returns the record sequence with consecutive-same-thread entries collapsed.
    const std::vector<schedule_entry_t> &
    get_serial_records();
    const std::unordered_map<uint64_t, std::vector<schedule_entry_t>> &
    get_full_cpu_records();
    const std::unordered_map<uint64_t, std::vector<schedule_entry_t>> &
    get_cpu_records();

private:
    void
    aggregate_schedule_data();

    // Some uses cases want both a version with all entries and one with collapsed
    // consecutive-same-thread entries.  We assume the extra size is not significant.
    std::vector<schedule_entry_t> serial_;
    std::vector<schedule_entry_t> serial_redux_;
    std::unordered_map<uint64_t, std::vector<schedule_entry_t>> cpu2sched_;
    std::unordered_map<uint64_t, std::vector<schedule_entry_t>> cpu2sched_redux_;
    bool aggregated_ = false;
};

} // namespace drmemtrace
} // namespace dynamorio

#endif /* _SCHEDULE_FILE_H_ */
