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
#include "config_reader_helpers.h"
#include <sstream>

namespace dynamorio {
namespace drmemtrace {

// Extract parameter value from string
template <typename T>
bool
parse_param_value_or_fail(const std::string &pname, const config_param_node_t &p, T *dst)
{
    if (p.type == config_param_node_t::MAP) {
        ERRMSG(
            "Array for '%s' not supported, '%s' value expected at line %d column %d.\n",
            pname.c_str(), get_type_name<T>(), p.val_line, p.val_column);
        return false;
    }
    if (!parse_value(p.value, dst)) {
        ERRMSG(
            "Incorrect value '%s' for '%s', '%s' value expected at line %d column %d.\n",
            p.value.c_str(), pname.c_str(), get_type_name<T>(), p.val_line, p.val_column);
        return false;
    }
    return true;
}

// XXX: This function is a duplicate of
//      droption_t<bytesize_t>::convert_from_string
//      Consider sharing the function using a single copy.
static bool
convert_string_to_size(const std::string &s, uint64_t &size)
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

static bool
configure_cache(const config_t &params, cache_params_t *cache)
{
    for (const auto &p : params) {
        if (p.first == "type") {
            // Cache type: CACHE_TYPE_INSTRUCTION, CACHE_TYPE_DATA,
            // or CACHE_TYPE_UNIFIED.
            if (!parse_param_value_or_fail(p.first, p.second, &cache->type)) {
                return false;
            }
            if (cache->type != CACHE_TYPE_INSTRUCTION && cache->type != CACHE_TYPE_DATA &&
                cache->type != CACHE_TYPE_UNIFIED) {
                ERRMSG("Unknown cache type %s at line %d column %d\n",
                       cache->type.c_str(), p.second.val_line, p.second.val_column);
                return false;
            }
        } else if (p.first == "core") {
            // CPU core this cache is associated with.
            if (!parse_param_value_or_fail(p.first, p.second, &cache->core)) {
                return false;
            }
        } else if (p.first == "size") {
            // Cache size with units.
            std::string cache_size_str;
            if (!parse_param_value_or_fail(p.first, p.second, &cache_size_str)) {
                return false;
            }
            // Cache size in bytes.
            if (!convert_string_to_size(cache_size_str, cache->size)) {
                ERRMSG("Unusable cache size %s at line %d column %d\n",
                       cache_size_str.c_str(), p.second.val_line, p.second.val_column);
                return false;
            }
            if (cache->size <= 0) {
                ERRMSG("Cache size (%llu) must be positive at line %d column %d\n",
                       (unsigned long long)cache->size, p.second.val_line,
                       p.second.val_column);
                return false;
            }
        } else if (p.first == "assoc") {
            // Cache associativity_.
            if (!parse_param_value_or_fail(p.first, p.second, &cache->assoc)) {
                return false;
            }
            if (cache->assoc <= 0) {
                ERRMSG("Cache associativity (%u) must be positive at line %d column %d\n",
                       cache->assoc, p.second.val_line, p.second.val_column);
                return false;
            }
        } else if (p.first == "inclusive") {
            // Is the cache inclusive of its children.
            if (!parse_param_value_or_fail(p.first, p.second, &cache->inclusive)) {
                return false;
            }
        } else if (p.first == "exclusive") {
            // Is the cache exclusive of its children.
            if (!parse_param_value_or_fail(p.first, p.second, &cache->exclusive)) {
                return false;
            }
        } else if (p.first == "parent") {
            // Name of the cache's parent. LLC's parent is main memory
            // (CACHE_PARENT_MEMORY).
            if (!parse_param_value_or_fail(p.first, p.second, &cache->parent)) {
                return false;
            }
        } else if (p.first == "replace_policy") {
            // Cache replacement policy: REPLACE_POLICY_LRU (default),
            // REPLACE_POLICY_LFU or REPLACE_POLICY_FIFO.
            if (!parse_param_value_or_fail(p.first, p.second, &cache->replace_policy)) {
                return false;
            }
            if (cache->replace_policy != REPLACE_POLICY_NON_SPECIFIED &&
                cache->replace_policy != REPLACE_POLICY_LRU &&
                cache->replace_policy != REPLACE_POLICY_LFU &&
                cache->replace_policy != REPLACE_POLICY_FIFO &&
                cache->replace_policy != REPLACE_POLICY_RRIP) {
                ERRMSG("Unknown replacement policy %s at line %d column %d\n",
                       cache->replace_policy.c_str(), p.second.val_line,
                       p.second.val_column);
                return false;
            }
        } else if (p.first == "prefetcher") {
            // Type of prefetcher: PREFETCH_POLICY_NEXTLINE
            // or PREFETCH_POLICY_NONE.
            if (!parse_param_value_or_fail(p.first, p.second, &cache->prefetcher)) {
                return false;
            }
            if (cache->prefetcher != PREFETCH_POLICY_NEXTLINE &&
                cache->prefetcher != PREFETCH_POLICY_NONE) {
                ERRMSG("Unknown prefetcher type %s at line %d column %d\n",
                       cache->prefetcher.c_str(), p.second.val_line, p.second.val_column);
                return false;
            }
        } else if (p.first == "miss_file") {
            // Name of the file to use to dump cache misses info.
            if (!parse_param_value_or_fail(p.first, p.second, &cache->miss_file)) {
                return false;
            }
        } else {
            ERRMSG("Unknown cache configuration setting '%s' at line %d column %d\n",
                   p.first.c_str(), p.second.name_line, p.second.name_column);
            return false;
        }
    }
    if (cache->inclusive && cache->exclusive) {
        const auto &p_exclusive = params.find("exclusive");
        const auto &p_inclusive = params.find("inclusive");
        if (p_exclusive != params.end() && p_inclusive != params.end()) {
            ERRMSG(
                "Cache cannot be both inclusive AND exclusive. See line %d column %d and "
                "line %d column %d\n",
                p_inclusive->second.val_line, p_inclusive->second.val_column,
                p_exclusive->second.val_line, p_exclusive->second.val_column);
        } else {
            // Cannot detect position.
            ERRMSG("Cache cannot be both inclusive AND exclusive\n");
        }
        return false;
    }
    return true;
}

static bool
check_cache_config(int num_cores, std::map<std::string, cache_params_t> &caches_map)
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

config_reader_t::config_reader_t()
{
    /* Empty. */
}

bool
config_reader_t::configure(std::istream *config_file, cache_simulator_knobs_t &knobs,
                           std::map<std::string, cache_params_t> &caches)
{
    config_t params;
    if (!read_param_map(config_file, &params)) {
        return false;
    }

    for (const auto &p : params) {
        if (p.first == "num_cores") {
            // Number of cache cores.
            if (!parse_param_value_or_fail(p.first, p.second, &knobs.num_cores)) {
                return false;
            }
            if (knobs.num_cores == 0) {
                ERRMSG("Number of cores must be positive at line %d column %d\n",
                       p.second.val_line, p.second.val_column);
                return false;
            }
        }
        // XXX i#3047: Add support for page_size, which is needed to
        // configure TLBs.
        else if (p.first == "line_size") {
            // Cache line size in bytes.
            if (!parse_param_value_or_fail(p.first, p.second, &knobs.line_size)) {
                return false;
            }
            if (knobs.line_size == 0) {
                ERRMSG("Cache line size must be positive at line %d column %d\n",
                       p.second.val_line, p.second.val_column);
                return false;
            }
        } else if (p.first == "skip_refs") {
            // Number of references to skip.
            if (!parse_param_value_or_fail(p.first, p.second, &knobs.skip_refs)) {
                return false;
            }
        } else if (p.first == "warmup_refs") {
            // Number of references to use for caches warmup.
            if (!parse_param_value_or_fail(p.first, p.second, &knobs.warmup_refs)) {
                return false;
            }
        } else if (p.first == "warmup_fraction") {
            // Fraction of cache lines that must be filled to end the warmup.
            if (!parse_param_value_or_fail(p.first, p.second, &knobs.warmup_fraction)) {
                return false;
            }
            if (knobs.warmup_fraction < 0.0 || knobs.warmup_fraction > 1.0) {
                ERRMSG("Warmup fraction should be in [0.0, 1.0] at line %d column %d\n",
                       p.second.val_line, p.second.val_column);
                return false;
            }
        } else if (p.first == "sim_refs") {
            // Number of references to simulate.
            if (!parse_param_value_or_fail(p.first, p.second, &knobs.sim_refs)) {
                return false;
            }
        } else if (p.first == "cpu_scheduling") {
            // Whether to simulate CPU scheduling or not.
            if (!parse_param_value_or_fail(p.first, p.second, &knobs.cpu_scheduling)) {
                return false;
            }
        } else if (p.first == "verbose") {
            // Verbose level.
            if (!parse_param_value_or_fail(p.first, p.second, &knobs.verbose)) {
                return false;
            }
        } else if (p.first == "coherence" || p.first == "coherent") {
            // Whether to simulate coherence
            if (!parse_param_value_or_fail(p.first, p.second, &knobs.model_coherence)) {
                return false;
            }
        } else if (p.first == "use_physical") {
            // Whether to use physical addresses
            if (!parse_param_value_or_fail(p.first, p.second, &knobs.use_physical)) {
                return false;
            }
        } else if (p.second.type == config_param_node_t::MAP) {
            // A cache unit.
            cache_params_t cache;
            cache.name = p.first;
            if (!configure_cache(p.second.children, &cache)) {
                return false;
            }
            caches[cache.name] = std::move(cache);
        } else {
            ERRMSG("Unknown parameter %s at line %d column %d\n", p.first.c_str(),
                   p.second.name_line, p.second.name_column);
            return false;
        }
    }

    // Check cache configuration.
    return check_cache_config(knobs.num_cores, caches);
}

} // namespace drmemtrace
} // namespace dynamorio
