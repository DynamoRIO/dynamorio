/* **********************************************************
 * Copyright (c) 2021-2024 Google, LLC  All rights reserved.
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
 * ARE DISCLAIMED. IN NO EVENT SHALL GOOGLE, LLC OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

/* Unit tests for the schedule_file_t library. */

#include <cassert>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <vector>

#include "memref.h"
#include "schedule_file.h"
#include "trace_entry.h"

namespace dynamorio {
namespace drmemtrace {

bool
check_read_and_collapse()
{
    std::cerr << "Testing reading and collapsing\n";
    // Synthesize a serial schedule file.
    // For now we leave the cpu-schedule testing to the end-to-end tests
    // of users of the library like raw2trace and invariant_checker.
    static constexpr uint64_t TIMESTAMP_BASE = 100;
    static constexpr int CPU_BASE = 6;
    static constexpr memref_tid_t TID_A = 42;
    static constexpr memref_tid_t TID_B = 43;
    static constexpr memref_tid_t TID_C = 44;
    std::vector<schedule_entry_t> records;
    records.emplace_back(TID_A, TIMESTAMP_BASE, CPU_BASE, 0);
    // Include same-timestamp records to stress handling that.
    records.emplace_back(TID_C, TIMESTAMP_BASE, CPU_BASE + 1, 0);
    records.emplace_back(TID_B, TIMESTAMP_BASE, CPU_BASE + 2, 0);
    records.emplace_back(TID_A, TIMESTAMP_BASE + 1, CPU_BASE + 1, 2);
    records.emplace_back(TID_B, TIMESTAMP_BASE + 2, CPU_BASE, 1);
    // Include records with the same thread ID, timestamp, and CPU, but
    // different start_instruction.
    records.emplace_back(TID_C, TIMESTAMP_BASE + 3, CPU_BASE + 2, 3);
    records.emplace_back(TID_C, TIMESTAMP_BASE + 3, CPU_BASE + 2, 4);

    std::ostringstream ostream;
    for (const auto &record : records) {
        std::string as_string(reinterpret_cast<const char *>(&record), sizeof(record));
        ostream << as_string;
    }
    std::istringstream istream(ostream.str());

    schedule_file_t sched;
    std::string res = sched.read_serial_file(&istream);
    assert(res.empty());

    const std::vector<schedule_entry_t> &serial = sched.get_full_serial_records();
    const std::vector<schedule_entry_t> &serial_redux = sched.get_serial_records();
    assert(serial.size() == records.size());
    for (size_t i = 0; i < serial.size(); ++i) {
        assert(memcmp(&serial[i], &records[i], sizeof(serial[i])) == 0);
    }
    // We have one identical-thread record that will collapse.
    assert(serial_redux.size() == records.size() - 1);
    return true;
}

bool
check_aggregate()
{
    std::cerr << "Testing aggregation\n";
    static constexpr uint64_t TIMESTAMP_BASE = 100;
    static constexpr int CPU_X = 6;
    static constexpr int CPU_Y = 7;
    static constexpr memref_tid_t TID_A = 42;
    static constexpr memref_tid_t TID_B = 43;

    schedule_file_t::per_shard_t shardA;
    shardA.record_cpu_id(TID_A, CPU_X, TIMESTAMP_BASE + 0, 0);
    shardA.record_cpu_id(TID_A, CPU_X, TIMESTAMP_BASE + 20, 4);
    shardA.record_cpu_id(TID_A, CPU_X, TIMESTAMP_BASE + 40, 8);
    schedule_file_t::per_shard_t shardB;
    shardB.record_cpu_id(TID_B, CPU_Y, TIMESTAMP_BASE + 10, 0);
    shardB.record_cpu_id(TID_B, CPU_Y, TIMESTAMP_BASE + 30, 4);
    shardB.record_cpu_id(TID_B, CPU_Y, TIMESTAMP_BASE + 50, 8);

    schedule_file_t merged;
    merged.merge_shard_data(shardA);
    merged.merge_shard_data(shardB);

    const std::vector<schedule_entry_t> &serial = merged.get_full_serial_records();
    assert(serial.size() == 6);
    uint64_t last_timestamp = 0;
    for (size_t i = 0; i < serial.size(); ++i) {
        assert(serial[i].timestamp >= last_timestamp);
        last_timestamp = serial[i].timestamp;
        if (i % 2 == 0)
            assert(serial[i].thread == TID_A);
        else
            assert(serial[i].thread == TID_B);
    }
    return true;
}

int
test_main(int argc, const char *argv[])
{
    if (check_read_and_collapse() && check_aggregate()) {
        std::cerr << "schedule_file_t tests passed\n";
        return 0;
    }
    std::cerr << "schedule_file_t tests FAILED\n";
    exit(1);
}

} // namespace drmemtrace
} // namespace dynamorio
