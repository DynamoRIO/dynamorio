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

#include "v2p_reader_unit_test.h"

#include <cstdint>
#include <fstream>
#include <iostream>
#include <vector>

#include "reader/v2p_reader.h"

namespace dynamorio {
namespace drmemtrace {

static void
check_v2p_info(const v2p_info_t &v2p_info)
{
    // Change the number of entries if v2p_map_example.textproto is updated.
    // Must be equal to the number of "address_mapping {...}" blocks in the textproto.
    constexpr size_t NUM_ENTRIES = 3;
    if (v2p_info.v2p_map.size() != NUM_ENTRIES) {
        std::cerr << "v2p_map incorrect number of entries. Expected " << NUM_ENTRIES
                  << " got " << v2p_info.v2p_map.size() << ".\n";
        exit(1);
    }

    // Virtual and physical addresses must be aligned with v2p_example.textproto.
    const std::vector<addr_t> virtual_addresses = { 0x123, 0x456, 0x789 };
    const std::vector<addr_t> physical_addresses = { 0x3, 0x4, 0x5 };
    for (int i = 0; i < virtual_addresses.size(); ++i) {
        auto key_val = v2p_info.v2p_map.find(virtual_addresses[i]);
        if (key_val != v2p_info.v2p_map.end()) {
            if (key_val->second != physical_addresses[i]) {
                std::cerr << "v2p_map incorrect physical address. Expected "
                          << physical_addresses[i] << " got " << key_val->second << ".\n";
                exit(1);
            }
        } else {
            std::cerr << "v2p_map incorrect virtual address. Expected "
                      << virtual_addresses[i] << " not found.\n";
            exit(1);
        }
    }

    // Check page_size.
    constexpr uint64_t PAGE_SIZE = 0x200000;
    if (v2p_info.page_size != PAGE_SIZE) {
        std::cerr << "Incorrect page size. Expected " << PAGE_SIZE << " got "
                  << v2p_info.page_size << ".\n";
        exit(1);
    }

    // Check page_count.
    constexpr uint64_t PAGE_COUNT = 0x1;
    if (v2p_info.page_count != PAGE_COUNT) {
        std::cerr << "Incorrect page count. Expected " << PAGE_COUNT << " got "
                  << v2p_info.page_count << ".\n";
        exit(1);
    }

    // Check number of bytes_mapped.
    constexpr uint64_t BYTES_MAPPED = 0x18;
    if (v2p_info.bytes_mapped != BYTES_MAPPED) {
        std::cerr << "Incorrect number of bytes mapped. Expected " << BYTES_MAPPED
                  << " got " << v2p_info.bytes_mapped << ".\n";
        exit(1);
    }
}

void
unit_test_v2p_reader(const std::string &testdir)
{
    std::string v2p_file_path = testdir + "/v2p_example.textproto";
    v2p_info_t v2p_info;
    v2p_reader_t v2p_reader;
    std::ifstream fin;
    fin.open(v2p_file_path);
    if (!fin.is_open()) {
        std::cerr << "Failed to open the v2p file '" << v2p_file_path << "'\n";
        exit(1);
    }
    std::string error_str = v2p_reader.create_v2p_info_from_file(fin, v2p_info);
    fin.close();
    if (!error_str.empty()) {
        std::cerr << "v2p_reader failed with: " << error_str << "\n";
        exit(1);
    }

    check_v2p_info(v2p_info);
}

} // namespace drmemtrace
} // namespace dynamorio
