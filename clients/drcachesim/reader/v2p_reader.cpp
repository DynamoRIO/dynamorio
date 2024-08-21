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
#include <stdint.h>

#include <cstdlib>
#include <iostream>
#include <fstream>
#include <string>
#include <unordered_map>

#include "utils.h"

namespace dynamorio {
namespace drmemtrace {

v2p_reader_t::v2p_reader_t()
{
    /* Empty. */
}

bool
v2p_reader_t::gen_v2p_map(std::string path_to_file,
                          std::unordered_map<uint64_t, uint64_t> &v2p_map)
{
    std::ifstream file(path_to_file);
    if (!file.is_open()) {
        ERRMSG("ERROR: Failed to open %s\n", path_to_file.c_str());
        return false;
    }

    constexpr char virtual_address_key[] = "virtual_address";
    constexpr char physical_address_key[] = "physical_address";
    // Assumes virtual_address 0 is reserved and not used in the application.
    uint64_t virtual_address = 0;
    std::string line;
    while (std::getline(file, line)) {
        std::size_t found = line.rfind(virtual_address_key);
        if (found != std::string::npos) {
            std::stringstream ss(line);
            std::string elem;
            if (!std::getline(ss, elem, ':')) {
                ERRMSG("ERROR: virtual_address key not found. Found %s instead.\n",
                       elem.c_str());
                return false;
            }
            if (!std::getline(ss, elem, ':')) {
                ERRMSG("ERROR: virtual_address value not found. Found %s instead.\n",
                       elem.c_str());
                return false;
            }
            virtual_address = std::stoull(elem, nullptr, 0);
            continue;
        }
        found = line.rfind(physical_address_key);
        if (found != std::string::npos) {
            if (virtual_address == 0) {
                ERRMSG("ERROR: no corresponding virtual_address for this "
                       "physical_address.\n");
                return false;
            }
            std::stringstream ss(line);
            std::string elem;
            if (!std::getline(ss, elem, ':')) {
                ERRMSG("ERROR: physical_address key not found. Found %s instead.\n",
                       elem.c_str());
                return false;
            }
            if (!std::getline(ss, elem, ':')) {
                ERRMSG("ERROR: physical_address value not found. Found %s instead.\n",
                       elem.c_str());
                return false;
            }
            uint64_t physical_address = std::stoull(elem, nullptr, 0);
            v2p_map[virtual_address] = physical_address;
        }
        virtual_address = 0;
    }

    return true;
}

} // namespace drmemtrace
} // namespace dynamorio
