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
#include <cstdlib>
#include <fstream>
#include <string>
#include <sstream>
#include <unordered_map>
#include <vector>

#include "utils.h"

namespace dynamorio {
namespace drmemtrace {

std::string
v2p_reader_t::set_value_or_fail(std::string key_str, uint64_t new_value, uint64_t &value)
{
    if (value != 0) {
        if (value != new_value) {
            std::stringstream error_ss;
            error_ss << "ERROR: " << key_str << " mismatch. Current value " << value
                     << " is different than new value " << new_value << ".";
            return error_ss.str();
        }
    } else {
        value = new_value;
    }
    return "";
}

std::string
v2p_reader_t::get_value_from_line(std::string line, uint64_t &value)
{
    std::vector<std::string> key_val_pair = split_by(line, ":");
    if (key_val_pair.size() != 2) {
        return "ERROR: value not found.";
    }
    // base = 0 allows to handle both decimal and hex numbers.
    value = std::stoull(key_val_pair[1], nullptr, /*base = */ 0);
    return "";
}

std::string
v2p_reader_t::create_v2p_info_from_file(std::istream &v2p_file, v2p_info_t &v2p_info)
{
    const std::string page_size_key = "page_size";
    const std::string page_count_key = "page_count";
    const std::string bytes_mapped_key = "bytes_mapped";
    const std::string virtual_address_key = "virtual_address";
    const std::string physical_address_key = "physical_address";
    // Assumes virtual_address 0 is not in the v2p file.
    addr_t virtual_address = 0;
    uint64_t value = 0;
    std::string error_str;
    std::stringstream error_ss;
    std::string line;
    while (std::getline(v2p_file, line)) {
        // Ignore comments in v2p.textproto file.
        if (starts_with(line, "#"))
            continue;

        std::size_t found = line.find(virtual_address_key);
        if (found != std::string::npos) {
            error_str = get_value_from_line(line, value);
            if (!error_str.empty())
                return error_str;
            virtual_address = static_cast<addr_t>(value);
            continue;
        }

        found = line.find(physical_address_key);
        if (found != std::string::npos) {
            error_str = get_value_from_line(line, value);
            if (!error_str.empty())
                return error_str;
            addr_t physical_address = static_cast<addr_t>(value);
            if (virtual_address == 0) {
                error_ss << "ERROR: no corresponding " << virtual_address_key << " for "
                         << physical_address_key << " " << physical_address << ".";
                return error_ss.str();
            }
            if (v2p_info.v2p_map.count(virtual_address) > 0) {
                error_ss << "ERROR: " << virtual_address_key << " " << virtual_address
                         << " is already present in v2p_map.";
                return error_ss.str();
            }
            v2p_info.v2p_map[virtual_address] = physical_address;
        }
        virtual_address = 0;

        found = line.find(page_size_key);
        if (found != std::string::npos) {
            error_str = get_value_from_line(line, value);
            if (!error_str.empty())
                return error_str;
            error_str = set_value_or_fail(page_size_key, value, v2p_info.page_size);
            if (!error_str.empty())
                return error_str;
            continue;
        }

        found = line.find(page_count_key);
        if (found != std::string::npos) {
            error_str = get_value_from_line(line, value);
            if (!error_str.empty())
                return error_str;
            error_str = set_value_or_fail(page_count_key, value, v2p_info.page_count);
            if (!error_str.empty())
                return error_str;
            continue;
        }

        found = line.find(bytes_mapped_key);
        if (found != std::string::npos) {
            error_str = get_value_from_line(line, value);
            if (!error_str.empty())
                return error_str;
            error_str = set_value_or_fail(bytes_mapped_key, value, v2p_info.bytes_mapped);
            if (!error_str.empty())
                return error_str;
            continue;
        }
    }

    return "";
}

} // namespace drmemtrace
} // namespace dynamorio
