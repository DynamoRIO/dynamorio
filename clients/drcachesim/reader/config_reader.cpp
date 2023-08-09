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

#include "config_reader.h"

#include <stdint.h>

#include <cstdlib>
#include <iostream>
#include <iterator>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "options.h"
#include "cache_simulator_create.h"
#include "utils.h"

namespace dynamorio {
namespace drmemtrace {

config_reader_t::config_reader_t()
{
    /* Empty. */
}

bool
config_reader_t::configure(std::istream *config_file, cache_simulator_knobs_t &knobs,
                           std::map<std::string, cache_params_t> &caches)
{
    fin_ = config_file;

    // Walk through the configuration file.
    while (!fin_->eof()) {
        std::string param;

        if (!(*fin_ >> std::ws >> param)) {
            ERRMSG("Unable to read from the configuration file\n");
            return false;
        }

        if (param == "//") {
            // A comment.
            if (!getline(*fin_, param)) {
                ERRMSG("Comment expected but not found\n");
                return false;
            }
        } else if (param == "num_cores") {
            // Number of cache cores.
            if (!(*fin_ >> knobs.num_cores)) {
                ERRMSG("Error reading num_cores from the configuration file\n");
                return false;
            }
            if (knobs.num_cores == 0) {
                ERRMSG("Number of cores must be >0\n");
                return false;
            }
        }
        // XXX i#3047: Add support for page_size, which is needed to
        // configure TLBs.
        else if (param == "line_size") {
            // Cache line size in bytes.
            if (!(*fin_ >> knobs.line_size)) {
                ERRMSG("Error reading line_size from the configuration file\n");
                return false;
            }
            if (knobs.line_size == 0) {
                ERRMSG("Line size must be >0\n");
                return false;
            }
        } else if (param == "skip_refs") {
            // Number of references to skip.
            if (!(*fin_ >> knobs.skip_refs)) {
                ERRMSG("Error reading skip_refs from the configuration file\n");
                return false;
            }
        } else if (param == "warmup_refs") {
            // Number of references to use for caches warmup.
            if (!(*fin_ >> knobs.warmup_refs)) {
                ERRMSG("Error reading warmup_refs from "
                       "the configuration file\n");
                return false;
            }
        } else if (param == "warmup_fraction") {
            // Fraction of cache lines that must be filled to end the warmup.
            if (!(*fin_ >> knobs.warmup_fraction)) {
                ERRMSG("Error reading warmup_fraction from "
                       "the configuration file\n");
                return false;
            }
            if (knobs.warmup_fraction < 0.0 || knobs.warmup_fraction > 1.0) {
                ERRMSG("Warmup fraction should be in [0.0, 1.0]\n");
                return false;
            }
        } else if (param == "sim_refs") {
            // Number of references to simulate.
            if (!(*fin_ >> knobs.sim_refs)) {
                ERRMSG("Error reading sim_refs from the configuration file\n");
                return false;
            }
        } else if (param == "cpu_scheduling") {
            // Whether to simulate CPU scheduling or not.
            std::string bool_val;
            if (!(*fin_ >> bool_val)) {
                ERRMSG("Error reading cpu_scheduling from "
                       "the configuration file\n");
                return false;
            }
            if (is_true(bool_val)) {
                knobs.cpu_scheduling = true;
            } else {
                knobs.cpu_scheduling = false;
            }
        } else if (param == "verbose") {
            // Verbose level.
            if (!(*fin_ >> knobs.verbose)) {
                ERRMSG("Error reading verbose from the configuration file\n");
                return false;
            }
        } else if (param == "coherence") {
            // Whether to simulate coherence
            std::string bool_val;
            if (!(*fin_ >> bool_val)) {
                ERRMSG("Error reading coherence from the configuration file\n");
                return false;
            }
            if (is_true(bool_val)) {
                knobs.model_coherence = true;
            } else {
                knobs.model_coherence = false;
            }
        } else if (param == "use_physical") {
            // Whether to use physical addresses
            std::string bool_val;
            if (!(*fin_ >> bool_val)) {
                ERRMSG("Error reading use_physical from the configuration file\n");
                return false;
            }
            if (is_true(bool_val)) {
                knobs.use_physical = true;
            } else {
                knobs.use_physical = false;
            }
        } else {
            // A cache unit.
            cache_params_t cache;
            cache.name = param;
            if (!configure_cache(cache)) {
                return false;
            }
            caches[cache.name] = cache;
        }

        if (!(*fin_ >> std::ws)) {
            ERRMSG("Unable to read from the configuration file\n");
            return false;
        }
    }

    // Check cache configuration.
    return check_cache_config(knobs.num_cores, caches);
}

bool
config_reader_t::configure_cache(cache_params_t &cache)
{
    // String used to construct meaningful error messages.
    std::string error_msg;

    char c;
    if (!(*fin_ >> std::ws >> c)) {
        ERRMSG("Unable to read from the configuration file\n");
        return false;
    }
    if (c != '{') {
        ERRMSG("Expected '{' before cache params\n");
        return false;
    }

    while (!fin_->eof()) {
        std::string param;
        if (!(*fin_ >> std::ws >> param)) {
            ERRMSG("Unable to read from the configuration file\n");
            return false;
        }

        if (param == "}") {
            return true;
        } else if (param == "//") {
            // A comment.
            if (!getline(*fin_, param)) {
                ERRMSG("Comment expected but not found\n");
                return false;
            }
        } else if (param == "type") {
            // Cache type: CACHE_TYPE_INSTRUCTION, CACHE_TYPE_DATA,
            // or CACHE_TYPE_UNIFIED.
            if (!(*fin_ >> cache.type)) {
                ERRMSG("Error reading cache type from "
                       "the configuration file\n");
                return false;
            }
            if (cache.type != CACHE_TYPE_INSTRUCTION && cache.type != CACHE_TYPE_DATA &&
                cache.type != CACHE_TYPE_UNIFIED) {
                ERRMSG("Unknown cache type: %s\n", cache.type.c_str());
                return false;
            }
        } else if (param == "core") {
            // CPU core this cache is associated with.
            if (!(*fin_ >> cache.core)) {
                ERRMSG("Error reading cache core from "
                       "the configuration file\n");
                return false;
            }
        } else if (param == "size") {
            // Cache size in bytes.
            std::string size_str;
            if (!(*fin_ >> size_str)) {
                ERRMSG("Error reading cache size from "
                       "the configuration file\n");
                return false;
            }
            if (!convert_string_to_size(size_str, cache.size)) {
                ERRMSG("Unusable cache size %s\n", size_str.c_str());
                return false;
            }
            if (cache.size <= 0) {
                ERRMSG("Cache size (%llu) must be >0\n", (unsigned long long)cache.size);
                return false;
            }
        } else if (param == "assoc") {
            // Cache associativity_.
            if (!(*fin_ >> cache.assoc)) {
                ERRMSG("Error reading cache assoc from "
                       "the configuration file\n");
                return false;
            }
            if (cache.assoc <= 0) {
                ERRMSG("Cache associativity (%u) must be >0\n", cache.assoc);
                return false;
            }
        } else if (param == "inclusive") {
            // Is the cache inclusive of its children.
            std::string bool_val;
            if (!(*fin_ >> bool_val)) {
                ERRMSG("Error reading cache inclusivity from "
                       "the configuration file\n");
                return false;
            }
            if (is_true(bool_val)) {
                cache.inclusive = true;
            } else {
                cache.inclusive = false;
            }
        } else if (param == "parent") {
            // Name of the cache's parent. LLC's parent is main memory
            // (CACHE_PARENT_MEMORY).
            if (!(*fin_ >> cache.parent)) {
                ERRMSG("Error reading cache parent from "
                       "the configuration file\n");
                return false;
            }
        } else if (param == "replace_policy") {
            // Cache replacement policy: REPLACE_POLICY_LRU (default),
            // REPLACE_POLICY_LFU or REPLACE_POLICY_FIFO.
            if (!(*fin_ >> cache.replace_policy)) {
                ERRMSG("Error reading cache replace_policy from "
                       "the configuration file\n");
                return false;
            }
            if (cache.replace_policy != REPLACE_POLICY_NON_SPECIFIED &&
                cache.replace_policy != REPLACE_POLICY_LRU &&
                cache.replace_policy != REPLACE_POLICY_LFU &&
                cache.replace_policy != REPLACE_POLICY_FIFO) {
                ERRMSG("Unknown replacement policy: %s\n", cache.replace_policy.c_str());
                return false;
            }
        } else if (param == "prefetcher") {
            // Type of prefetcher: PREFETCH_POLICY_NEXTLINE
            // or PREFETCH_POLICY_NONE.
            if (!(*fin_ >> cache.prefetcher)) {
                ERRMSG("Error reading cache prefetcher from "
                       "the configuration file\n");
                return false;
            }
            if (cache.prefetcher != PREFETCH_POLICY_NEXTLINE &&
                cache.prefetcher != PREFETCH_POLICY_NONE) {
                ERRMSG("Unknown prefetcher type: %s\n", cache.prefetcher.c_str());
                return false;
            }
        } else if (param == "miss_file") {
            // Name of the file to use to dump cache misses info.
            if (!(*fin_ >> cache.miss_file)) {
                ERRMSG("Error reading cache miss_file from "
                       "the configuration file\n");
                return false;
            }
        } else {
            ERRMSG("Unknown cache configuration setting '%s'\n", param.c_str());
            return false;
        }

        if (!(*fin_ >> std::ws)) {
            ERRMSG("Unable to read from the configuration file\n");
            return false;
        }
    }

    ERRMSG("Expected '}' at the end of cache params\n");
    return false;
}

bool
config_reader_t::check_cache_config(int num_cores,
                                    std::map<std::string, cache_params_t> &caches_map)
{
    std::vector<int> core_inst_caches(num_cores, 0);
    std::vector<int> core_data_caches(num_cores, 0);

    for (auto &cache_map : caches_map) {
        std::string cache_name = cache_map.first;
        auto &cache = cache_map.second;

        // Associate a cache to a core.
        if (cache.core >= 0) {
            if (cache.core >= num_cores) {
                ERRMSG("Cache %s belongs to core %d which does not exist\n",
                       cache_name.c_str(), cache.core);
                return false;
            }
            if (cache.type == CACHE_TYPE_INSTRUCTION ||
                cache.type == CACHE_TYPE_UNIFIED) {
                core_inst_caches[cache.core]++;
            }
            if (cache.type == CACHE_TYPE_DATA || cache.type == CACHE_TYPE_UNIFIED) {
                core_data_caches[cache.core]++;
            }
        }

        // Associate a cache with its parent and children caches.
        if (cache.parent != CACHE_PARENT_MEMORY) {
            auto parent_it = caches_map.find(cache.parent);
            if (parent_it != caches_map.end()) {
                auto &parent = parent_it->second;
                // Check that the cache types are compatible.
                if (parent.type != CACHE_TYPE_UNIFIED && cache.type != parent.type) {
                    ERRMSG("Cache %s and its parent have incompatible types\n",
                           cache_name.c_str());
                    return false;
                }

                // Add the cache to its parents children.
                parent.children.push_back(cache_name);
            } else {
                ERRMSG("Cache %s has a listed parent %s that does not exist\n",
                       cache_name.c_str(), cache.parent.c_str());
                return false;
            }

            // Check for cycles between the cache and its parent.
            std::string parent = cache.parent;
            while (parent != CACHE_PARENT_MEMORY) {
                if (parent == cache_name) {
                    ERRMSG("Cache %s & its parent %s have a cyclic reference\n",
                           cache_name.c_str(), cache.parent.c_str());
                    return false;
                }
                parent = caches_map.find(parent)->second.parent;
            }
        }
    }

    // Check that each core has exactly one instruction and one data cache or
    // exactly one unified cache.
    for (int i = 0; i < num_cores; i++) {
        if (core_inst_caches[i] != 1) {
            ERRMSG("Core %d has %d instruction caches. Must have exactly 1.\n", i,
                   core_inst_caches[i]);
            return false;
        }
        if (core_data_caches[i] != 1) {
            ERRMSG("Core %d has %d data caches. Must have exactly 1.\n", i,
                   core_data_caches[i]);
            return false;
        }
    }

    return true;
}

// XXX: This function is a duplicate of
//      droption_t<bytesize_t>::convert_from_string
//      Consider sharing the function using a single copy.
bool
config_reader_t::convert_string_to_size(const std::string &s, uint64_t &size)
{
    char suffix = *s.rbegin(); // s.back() only in C++11
    int scale;
    switch (suffix) {
    case 'K':
    case 'k': scale = 1024; break;
    case 'M':
    case 'm': scale = 1024 * 1024; break;
    case 'G':
    case 'g': scale = 1024 * 1024 * 1024; break;
    default: scale = 1;
    }

    std::string toparse = s;
    if (scale > 1)
        toparse = s.substr(0, s.size() - 1); // s.pop_back() only in C++11

    // While the overall size is likely too large to be represented
    // by a 32-bit integer, the prefix number is usually not.
    int input = atoi(toparse.c_str());
    if (input >= 0)
        size = (uint64_t)(input * scale);
    else {
        size = 0;
        return false;
    }
    return true;
}

} // namespace drmemtrace
} // namespace dynamorio
