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
#include <fstream>
#include <string>
#include <unordered_map>
#include <vector>

#include "utils.h"

namespace dynamorio {
namespace drmemtrace {

std::string
v2p_reader_t::init_v2p_info_from_file(std::string path_to_file, v2p_info_t &v2p_info)
{
    std::stringstream error_ss;
    if (path_to_file.empty()) {
        error_ss << "ERROR: Path to v2p.textproto is empty.";
        return error_ss.str();
    }

    std::ifstream file(path_to_file);
    if (!file.is_open()) {
        error_ss << "ERROR: Failed to open " << path_to_file << ".";
        return error_ss.str();
    }

    const std::string separator = ":";
    const std::string page_size_key = "page_size";
    const std::string page_count_key = "page_count";
    const std::string bytes_mapped_key = "bytes_mapped";
    const std::string virtual_address_key = "virtual_address";
    const std::string physical_address_key = "physical_address";
    // Assumes virtual_address 0 is not in the v2p file.
    addr_t virtual_address = 0;
    std::string line;
    while (std::getline(file, line)) {
        std::size_t found = line.find(virtual_address_key);
        if (found != std::string::npos) {
            std::vector<std::string> key_val_pair = split_by(line, separator);
            if (key_val_pair.size() != 2) {
                error_ss << "ERROR: " << virtual_address_key
                         << " key or value not found.";
                return error_ss.str();
            }
            virtual_address =
                static_cast<addr_t>(std::stoull(key_val_pair[1], nullptr, 0));
            continue;
        }

        found = line.find(physical_address_key);
        if (found != std::string::npos) {
            std::vector<std::string> key_val_pair = split_by(line, separator);
            if (key_val_pair.size() != 2) {
                error_ss << "ERROR: " << physical_address_key
                         << " key or value not found.";
                return error_ss.str();
            }
            if (virtual_address == 0) {
                error_ss << "ERROR: no corresponding " << virtual_address_key
                         << " for this " << physical_address_key << " " << key_val_pair[1]
                         << ".";
                return error_ss.str();
            }
            addr_t physical_address =
                static_cast<addr_t>(std::stoull(key_val_pair[1], nullptr, 0));
            v2p_info.v2p_map[virtual_address] = physical_address;
        }
        virtual_address = 0;

        found = line.find(page_size_key);
        if (found != std::string::npos) {
            std::vector<std::string> key_val_pair = split_by(line, separator);
            if (key_val_pair.size() != 2) {
                error_ss << "ERROR: " << page_size_key << " key or value not found.";
                return error_ss.str();
            }
            v2p_info.page_size =
                static_cast<size_t>(std::stoull(key_val_pair[1], nullptr, 0));
            continue;
        }

        found = line.find(page_count_key);
        if (found != std::string::npos) {
            std::vector<std::string> key_val_pair = split_by(line, separator);
            if (key_val_pair.size() != 2) {
                error_ss << "ERROR: " << page_count_key << " key or value not found.";
                return error_ss.str();
            }
            v2p_info.page_count = std::stoull(key_val_pair[1], nullptr, 0);
            continue;
        }

        found = line.find(bytes_mapped_key);
        if (found != std::string::npos) {
            std::vector<std::string> key_val_pair = split_by(line, separator);
            if (key_val_pair.size() != 2) {
                error_ss << "ERROR: " << bytes_mapped_key << " key or value not found.";
                return error_ss.str();
            }
            v2p_info.bytes_mapped = std::stoull(key_val_pair[1], nullptr, 0);
            continue;
        }
    }

    return "";
}

} // namespace drmemtrace
} // namespace dynamorio
