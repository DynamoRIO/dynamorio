/* **********************************************************
 * Copyright (c) 2024 Google, LLC  All rights reserved.
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
 * * Neither the name of Google, LLC nor the names of its contributors may be
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

#include <iostream>
#include <vector>
#include "v2p_reader_unit_test.h"
#include "reader/v2p_reader.h"

namespace dynamorio {
namespace drmemtrace {

static void
check_v2p_map(const std::unordered_map<uint64_t, uint64_t> &v2p_map)
{
    // Change the number of entries if v2p_map_example.textproto is updated.
    // Must be equal to the number of "address_mapping {...}" blocks in the textproto.
    constexpr uint64_t num_entries = 3;
    if (v2p_map.size() != num_entries) {
        std::cerr << "v2p_map incorrect number of entries. Expected " << num_entries
                  << " got " << v2p_map.size() << "\n";
        exit(1);
    }

    // Virtual and physical addresses must be aligned with v2p_map_example.textproto.
    const std::vector<uint64_t> virtual_addresses = { 0x123, 0x456, 0x789 };
    const std::vector<uint64_t> physical_addresses = { 0x3, 0x4, 0x5 };
    for (int i = 0; i < virtual_addresses.size(); ++i) {
        auto key_val = v2p_map.find(virtual_addresses[i]);
        if (key_val != v2p_map.end()) {
            if (key_val->second != physical_addresses[i]) {
                std::cerr << "v2p_map incorrect physical address. Expected "
                          << physical_addresses[i] << " got " << key_val->second << "\n";
                exit(1);
            }
        } else {
            std::cerr << "v2p_map incorrect virtual address. Expected "
                      << virtual_addresses[i] << " not found\n";
            exit(1);
        }
    }
}

void
unit_test_v2p_reader(const char *testdir)
{
    std::unordered_map<uint64_t, uint64_t> v2p_map;
    std::string file_path = std::string(testdir) + "/v2p_map_example.textproto";

    v2p_reader_t v2p_reader;
    if (!v2p_reader.gen_v2p_map(file_path, v2p_map)) {
        std::cerr << "drcachesim v2p_reader_test failed (generate v2p map error)\n";
        exit(1);
    }

    check_v2p_map(v2p_map);
}

} // namespace drmemtrace
} // namespace dynamorio
