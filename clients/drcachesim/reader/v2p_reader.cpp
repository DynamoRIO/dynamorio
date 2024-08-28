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

#include "v2p_reader.h"

#include <cstdint>
#include <sstream>
#include <stdint.h>

#include <cstdlib>
#include <iostream>
#include <fstream>
#include <string>
#include <unordered_map>
#include <vector>

#include "utils.h"

namespace dynamorio {
namespace drmemtrace {

std::string
v2p_reader_t::gen_v2p_map(std::string path_to_file,
                          std::unordered_map<uint64_t, uint64_t> &v2p_map)
{
    std::stringstream error_ss;
    std::ifstream file(path_to_file);
    if (!file.is_open()) {
        error_ss << "ERROR: Failed to open " << path_to_file << ".";
        return error_ss.str();
    }

    const std::string separator = ":";
    const std::string virtual_address_key = "virtual_address";
    const std::string physical_address_key = "physical_address";
    // Assumes virtual_address 0 is not in the v2p file.
    uint64_t virtual_address = 0;
    std::string line;
    while (std::getline(file, line)) {
        std::size_t found = line.find(virtual_address_key);
        if (found != std::string::npos) {
            std::vector<std::string> key_val_pair = split_by(line, ":");
            if (key_val_pair.size() != 2) {
                error_ss << "ERROR: virtual_address key or value mismatch.";
                return error_ss.str();
            }
            virtual_address = std::stoull(key_val_pair[1], nullptr, 0);
            continue;
        }
        found = line.find(physical_address_key);
        if (found != std::string::npos) {
            std::vector<std::string> key_val_pair = split_by(line, ":");
            if (key_val_pair.size() != 2) {
                error_ss << "ERROR: physical_address key or value mismatch.";
                return error_ss.str();
            }
            if (virtual_address == 0) {
                error_ss << "ERROR: no corresponding virtual_address for this "
                            "physical_address "
                         << key_val_pair[1] << ".";
                return error_ss.str();
            }
            uint64_t physical_address = std::stoull(key_val_pair[1], nullptr, 0);
            v2p_map[virtual_address] = physical_address;
        }
        virtual_address = 0;
    }

    return "";
}

} // namespace drmemtrace
} // namespace dynamorio
