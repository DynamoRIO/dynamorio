/* **********************************************************
 * Copyright (c) 2018-2023 Google, LLC  All rights reserved.
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

/* config_reader: reads, parses, and configures a cache hierarchy from a config
 * file.
 */

#ifndef _CONFIG_READER_H_
#define _CONFIG_READER_H_ 1

#include <stdint.h>

#include <fstream>
#include <map>
#include <string>
#include <vector>

#include "options.h"
#include "cache.h"
#include "cache_simulator_create.h"

namespace dynamorio {
namespace drmemtrace {

// Cache configuration settings.
struct cache_params_t {
    cache_params_t()
        : type(CACHE_TYPE_UNIFIED)
        , core(-1)
        , size(0)
        , assoc(0)
        , inclusive(false)
        , parent(CACHE_PARENT_MEMORY)
        , replace_policy(REPLACE_POLICY_LRU)
        , prefetcher(PREFETCH_POLICY_NONE)
        , miss_file("")
    {
    }
    // Cache's name. Each cache must have a unique name.
    std::string name;
    // Cache type: CACHE_TYPE_INSTRUCTION, CACHE_TYPE_DATA,
    // or CACHE_TYPE_UNIFIED (default).
    std::string type;
    // CPU core this cache is associated with.
    // Must be specified for L1 caches only.
    int core;
    // Cache size in bytes.
    uint64_t size;
    // Cache associativity. Must be a power of 2.
    unsigned int assoc;
    // Is the cache inclusive of its children.
    bool inclusive;
    // Name of the cache's parent. LLC's parent is main memory
    // (CACHE_PARENT_MEMORY).
    std::string parent;
    // Names of the cache's children. L1 caches don't have children.
    std::vector<std::string> children;
    // Cache replacement policy as described by the runtime option
    // op_replace_policy (see ../common/options.cpp).
    std::string replace_policy;
    // Type of prefetcher as described by the runtime option
    // op_data_prefetcher (see ../common/options.cpp).
    std::string prefetcher;
    // Name of the file to use to dump cache misses info.
    std::string miss_file;
};

class config_reader_t {
public:
    config_reader_t();
    bool
    configure(std::istream *config_file, cache_simulator_knobs_t &knobs,
              std::map<std::string, cache_params_t> &caches);

private:
    std::istream *fin_;

    bool
    configure_cache(cache_params_t &cache);
    bool
    check_cache_config(int num_cores, std::map<std::string, cache_params_t> &caches_map);
    bool
    convert_string_to_size(const std::string &s, uint64_t &size);
    bool
    is_true(std::string bool_val)
    {
        if (bool_val == "true" || bool_val == "True" || bool_val == "TRUE") {
            return true;
        }
        return false;
    }
};

} // namespace drmemtrace
} // namespace dynamorio

#endif /* _CONFIG_READER_H_ */
