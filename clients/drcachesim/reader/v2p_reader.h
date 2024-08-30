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

/* v2p_reader: reads and parses a virtual-to-physical address mapping in textproto format.
 * Creates virtual-to-physical address map in memory.
 * The section of the textproto file that we are interested in parsing is a sequence of
 * blocks that follow this format:
 * address_mapping {
    virtual_address: 0x123
    physical_address: 0x3
 * }
 * In gen_v2p_map() we rely on the fact that virtual_address and physical_address are one
 * after the other on two different lines.
 */

#ifndef _V2P_READER_H_
#define _V2P_READER_H_ 1

#include "trace_entry.h"

#include <cstdint>
#include <string>
#include <unordered_map>

namespace dynamorio {
namespace drmemtrace {

struct v2p_info_t {
    uint64_t page_count;
    uint64_t bytes_mapped;
    size_t page_size;
    std::unordered_map<addr_t, addr_t> v2p_map;
};

class v2p_reader_t {
public:
    v2p_reader_t() = default;

    std::string
    gen_v2p_map(std::string path_to_file, v2p_info_t &v2p_info);
};

} // namespace drmemtrace
} // namespace dynamorio

#endif /* _V2P_READER_H_ */
